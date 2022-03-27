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

#include <Piper/Render/Light.hpp>
#include <Piper/Render/SamplingUtil.hpp>
#include <Piper/Render/Texture.hpp>

PIPER_NAMESPACE_BEGIN

template <typename Setting>
class PointLight final : public Light<Setting> {
    PIPER_IMPORT_SETTINGS();

    Ref<SphericalTexture<Setting>> mIntensity;
    ResolvedTransform mTransform{};

public:
    explicit PointLight(const Ref<ConfigNode>& node)
        : mIntensity{ this->template make<SphericalTexture>(node->get("Intensity"sv)->as<Ref<ConfigNode>>()) } {}
    [[nodiscard]] LightAttributes attributes() const noexcept override {
        return LightAttributes::Delta;
    }

    void updateTransform(const KeyFrames& keyFrames, const TimeInterval timeInterval) override {
        mTransform = resolveTransform(keyFrames, timeInterval);
    }

    LightLiSample<Spectrum> sampleLi(const ShadingContext<Setting>& ctx, const Point<FrameOfReference::World>& pos,
                                     SampleProvider& sampler) const noexcept override {
        const auto transform = mTransform(ctx.t);
        const auto lightSource = Point<FrameOfReference::World>::fromRaw(transform.translation);
        const auto [dir, dist2] = direction(pos, lightSource);
        const auto intensity = Intensity<Spectrum>::fromRaw(mIntensity->evaluate(transform.rotateOnly(dir), ctx.sampledWavelength));
        const auto radiance = importanceSampled<PdfType::Light | PdfType::LightSampler>(intensity.toRadiance(dist2));
        return LightLiSample<Spectrum>{ dir, radiance, InversePdf<PdfType::Light>::identity(), sqrt(dist2) };
    }
    InversePdf<PdfType::Light> inversePdfLi(const ShadingContext<Setting>& ctx,
                                            const Direction<FrameOfReference::World>& wi) const noexcept override {
        return InversePdf<PdfType::Light>::invalid();
    }
    LightLeSample<Spectrum> sampleLe(const ShadingContext<Setting>& ctx, SampleProvider& sampler) const noexcept override {
        const auto transform = mTransform(ctx.t);
        const auto dir = sampleUniformSphere<FrameOfReference::World>(sampler.sampleVec2());
        const auto lightSource = Point<FrameOfReference::World>::fromRaw(transform.translation);
        const Ray ray{ lightSource, dir, ctx.t };
        const auto intensity = Intensity<Spectrum>::fromRaw(mIntensity->evaluate(transform.rotateOnly(-dir), ctx.sampledWavelength));
        return LightLeSample<Spectrum>{ ray, intensity, InversePdf<PdfType::LightPos>::identity(), uniformSpherePdf<PdfType::LightDir>() };
    }
    std::pair<InversePdf<PdfType::LightPos>, InversePdf<PdfType::LightDir>> pdfLe(const ShadingContext<Setting>& ctx,
                                                                                  const Ray& ray) const noexcept override {
        return { InversePdf<PdfType::LightPos>::invalid(), uniformSpherePdf<PdfType::LightDir>() };
    }

    [[nodiscard]] Power<MonoSpectrum> power() const noexcept override {
        return Intensity<MonoSpectrum>::fromRaw(mIntensity->mean()) * SolidAngle::fullSphere();
    }
};

PIPER_REGISTER_VARIANT(PointLight, Light);

PIPER_NAMESPACE_END
