/*
    SPDX-License-Identifier: GPL-3.0-or-later

    This file is part of Piper0, a physically based renderer.
    Copyright (C) 2022 Xu Yankai a.k.a. Froster

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
#include <Piper/Render/Texture.hpp>

PIPER_NAMESPACE_BEGIN

template <typename Setting>
class SpotLight final : public Light<Setting> {
    PIPER_IMPORT_SETTINGS();
    Ref<SphericalTexture<Setting>> mIntensity;
    Float mCosTotalWidth, mCosFalloffStart;
    ResolvedTransform mTransform{};

    [[nodiscard]] std::optional<Float> getDelta(const Direction<FrameOfReference::Object>& dir) const {
        Float delta = 1;
        const Float cosTheta = dot(dir, Direction<FrameOfReference::Object>::fromRaw({ 0, 0, 1 }));
        if(cosTheta < mCosTotalWidth) {
            return std::nullopt;
        }
        if(cosTheta <= mCosFalloffStart) {
            delta = (cosTheta - mCosTotalWidth) / (mCosFalloffStart - mCosTotalWidth);
            delta = pow<4>(delta);
        }
        return delta;
    }

public:
    explicit SpotLight(const Ref<ConfigNode>& node)
        : mIntensity{ this->template make<SphericalTexture>(node->get("Intensity"sv)->as<Ref<ConfigNode>>()) },
          mCosTotalWidth{ std::cos(node->get("TotalWidth"sv)->as<Float>()) }, mCosFalloffStart{ std::cos(
                                                                                  node->get("FalloffStart"sv)->as<Float>()) } {}
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

        const auto objDirection = transform.rotateOnly(-dir);

        auto delta = getDelta(objDirection);
        if(!delta.has_value())
            return LightLiSample<Spectrum>::invalid();

        Intensity<Spectrum> intensity = Intensity<Spectrum>::fromRaw(
            mIntensity->evaluate({ mIntensity->dir2TexCoord(objDirection), ctx.t, 0U }, ctx.sampledWavelength) * delta.value());
        const auto radiance = importanceSampled<PdfType::Light | PdfType::LightSampler>(intensity.toRadiance(dist2));
        return LightLiSample<Spectrum>{ dir, radiance, InversePdf<PdfType::Light>::identity(), sqrt(dist2) };
    }
    InversePdf<PdfType::Light> inversePdfLi(const ShadingContext<Setting>& ctx,
                                            const Direction<FrameOfReference::World>& wi) const noexcept override {
        return InversePdf<PdfType::Light>::invalid();
    }
    LightLeSample<Spectrum> sampleLe(const ShadingContext<Setting>& ctx, SampleProvider& sampler) const noexcept override {
        const auto transform = mTransform(ctx.t);

        const auto rand = sampler.sampleVec2();
        // minZ = mCosTotalWidth * 1, maxZ = 1
        const auto z = (1.0f - rand.x) * mCosTotalWidth + rand.x;
        const auto r = safeSqrt(1.0f - sqr(z));
        const auto phi = twoPi * rand.y;
        const auto objDir = Direction<FrameOfReference::Object>::fromRaw(glm::vec3{ r * std::cos(phi), r * std::sin(phi), z });
        const auto dir = transform.rotateOnly(objDir);

        const auto lightSource = Point<FrameOfReference::World>::fromRaw(transform.translation);
        const Ray ray{ lightSource, dir, ctx.t };
        const auto intensity = Intensity<Spectrum>::fromRaw(
            mIntensity->evaluate({ mIntensity->dir2TexCoord(-objDir), ctx.t, 0U }, ctx.sampledWavelength) * getDelta(objDir).value());
        return LightLeSample<Spectrum>{ ray, intensity, InversePdf<PdfType::LightPos>::identity(),
                                        InversePdf<PdfType::LightDir>::fromRaw(twoPi * (1 - mCosTotalWidth)) };
    }
    std::pair<InversePdf<PdfType::LightPos>, InversePdf<PdfType::LightDir>> pdfLe(const ShadingContext<Setting>& ctx,
                                                                                  const Ray& ray) const noexcept override {
        return { InversePdf<PdfType::LightPos>::invalid(), InversePdf<PdfType::LightDir>::fromRaw(twoPi * (1 - mCosTotalWidth)) };
    }

    [[nodiscard]] Power<MonoSpectrum> power() const noexcept override {
        return Intensity<MonoSpectrum>::fromRaw(mIntensity->mean()) *
            SolidAngle::fromRaw(twoPi * (1 - 0.5f * (mCosFalloffStart + mCosTotalWidth)));
    }
};

PIPER_REGISTER_VARIANT(SpotLight, Light);

PIPER_NAMESPACE_END
