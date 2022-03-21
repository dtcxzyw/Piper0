/*
    SPDX-License-Identifier: GPL-3.0-or-later

    This file is part of Piper0, a physically based renderer.
    Copyright (C) 2022 Yingwei Zheng

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#include <Piper/Core/Stats.hpp>
#include <Piper/Render/Acceleration.hpp>
#include <Piper/Render/Integrator.hpp>
#include <Piper/Render/LightSampler.hpp>
#include <Piper/Render/Material.hpp>
#include <Piper/Render/Radiometry.hpp>

PIPER_NAMESPACE_BEGIN

template <typename Setting>
class PathIntegrator final : public Integrator<Setting> {
    PIPER_IMPORT_SETTINGS();

    uint32_t mMaxDepth;

    Radiance<Spectrum> estimateDirect(const LightSampler& lightSampler, SampleProvider& sampler, const ShadingContext<Setting>& ctx,
                                      const SurfaceHit& info, const Acceleration& acceleration,
                                      const Direction<FrameOfReference::World>& wo, const BSDF<Setting>& bsdf) const noexcept {
        auto hit = info.hit;
        if(match(bsdf.part(), BxDFPart::Reflection) && !match(bsdf.part(), BxDFPart::Transmission))
            hit = info.offsetOrigin(true);
        else if(!match(bsdf.part(), BxDFPart::Reflection) && match(bsdf.part(), BxDFPart::Transmission))
            hit = info.offsetOrigin(false);

        const auto [selectedLight, weight] = lightSampler.sample(sampler);
        const auto sampledLight = selectedLight.as<Setting>().sampleLi(ctx, hit, sampler);
        if(!sampledLight.valid())
            return Radiance<Spectrum>::zero();

        // for(auto& x : acceleration.occlusions()) {}

        const auto wi = sampledLight.dir;

        if(acceleration.occluded(Ray{ hit, wi, ctx.t }, sampledLight.distance))
            return Radiance<Spectrum>::zero();

        const auto f = bsdf.evaluate(wo, wi) * absDot(info.shadingNormal, wi);
        const auto inverseLightPdf = weight * sampledLight.inversePdf;

        // only sample BSDF
        if(match(selectedLight.as<Setting>().attributes(), LightAttributes::Delta))
            return sampledLight.rad * f * inverseLightPdf;
        // MIS
        const auto bsdfPdf = bsdf.pdf(wo, wi);
        const auto mixedWeight = powerHeuristic(inverseLightPdf, bsdfPdf);
        return sampledLight.rad * f * (mixedWeight * inverseLightPdf);
    }

public:
    explicit PathIntegrator(const Ref<ConfigNode>& node) : mMaxDepth{ node->get("MaxDepth"sv)->as<uint32_t>() } {}
    void preprocess() const noexcept override {}
    void estimate(const Ray& rayInit, const Intersection& intersectionInit, const Acceleration& acceleration,
                  const LightSampler& lightSampler, SampleProvider& sampler, Float* output) const noexcept override {
        Ray ray = rayInit;
        Intersection intersection = intersectionInit;

        Radiance<Spectrum> result = Radiance<Spectrum>::zero();
        Rational<Spectrum> beta = Rational<Spectrum>::identity();

        Wavelength sampledWavelength = *reinterpret_cast<Wavelength*>(output);  // TODO
        ShadingContext<Setting> ctx{ ray.t, sampledWavelength };

        uint32_t depth = 0;
        Float etaScale = 1;

        while(true) {
            if(intersection.index() == 0) {
                for(auto light : lightSampler.infiniteLights())
                    result += beta * light.as<Setting>().evalLe(ctx, ray);
                break;
            }

            const auto& info = std::get<SurfaceHit>(intersection);
            // TODO: area light

            const auto& material = info.surface.as<Setting>();
            const auto bsdf = material.evaluate(sampledWavelength, info);

            const auto wo = -ray.direction;
            // compute direct illumination using MIS
            if(hasNonSpecular(bsdf.part()))
                result += beta * estimateDirect(lightSampler, sampler, ctx, info, acceleration, wo, bsdf);

            if(depth++ == mMaxDepth)
                break;

            // Spawn new ray
            const auto sampledBSDF = bsdf.sample(sampler, wo);
            if(!sampledBSDF.valid())
                break;

            if(match(sampledBSDF.part, BxDFPart::Transmission)) {
                // TODO: maintain etaScale
            }

            beta = beta * sampledBSDF.f * (sampledBSDF.inversePdf * absDot(info.shadingNormal, sampledBSDF.wi));

            // Russian roulette
            const auto rrBeta = maxComponentValue(beta.raw()) * etaScale;
            if(rrBeta < 0.95f && depth > 1) {
                const auto q = std::fmax(0.0f, 1.0f - rrBeta);
                if(sampler.sample() < q)
                    break;
                beta /= 1.0f - q;
            }

            ray.origin = info.offsetOrigin(match(sampledBSDF.part, BxDFPart::Reflection));
            ray.direction = sampledBSDF.wi;
            intersection = acceleration.trace(ray);
        }

        Histogram<StatsType::TraceDepth>::count(depth);

        if constexpr(spectrumType<Spectrum>() == SpectrumType::Mono)
            *output = luminance(result.raw(), sampledWavelength);
        else
            *reinterpret_cast<RGBSpectrum*>(output) = toRGB(result.raw(), sampledWavelength);
    }
};

PIPER_REGISTER_VARIANT(PathIntegrator, Integrator);

PIPER_NAMESPACE_END
