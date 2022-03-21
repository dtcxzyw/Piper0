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

        uint32_t depth = 0;

        while(true) {

            if(intersection.index() == 0) {
                // TODO: sample infinite lights
                for(auto light : lightSampler.infiniteLights()) {
                }
                break;
            }

            const auto& info = std::get<SurfaceHit>(intersection);

            const auto& material = info.surface.as<Setting>();
            const auto bsdf = material.evaluate(sampledWavelength, info);

            const auto [selectedLight, weight] = lightSampler.sample(sampler);
            const auto sampledLight = selectedLight.as<Setting>().sampleLi({ ray.t, sampledWavelength }, info.hit, sampler);

            // for(auto& x : acceleration.occlusions()) {}

            const auto wo = -ray.direction;
            const auto wi = sampledLight.dir;
            if(sampledLight.valid() && !acceleration.occluded(Ray{ info.hit, sampledLight.dir, ray.t }, sampledLight.distance))
                result +=
                    beta * bsdf.evaluate(wo, wi) * (sampledLight.rad * (sampledLight.inversePdf * weight * absDot(info.shadingNormal, wi)));

            if(depth++ == mMaxDepth)
                return;

            const auto sampledBSDF = bsdf.sample(sampler, wo);
            if(!sampledBSDF.valid())
                return;

            beta = beta * sampledBSDF.f * (sampledBSDF.inversePdf * absDot(info.shadingNormal, sampledBSDF.wi));
            ray.origin = info.hit;
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
