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
class DirectionalLight final : public Light<Setting> {
    PIPER_IMPORT_SETTINGS();
    Ref<SphericalTexture<Setting>> mIntensity;
    Float mSceneRadius{};
    Distance mSceneDiameter = Distance::undefined();
    DistanceSquare mSceneDiameterSquare = DistanceSquare::undefined();
    Direction<FrameOfReference::World> mDirection;

public:
    explicit DirectionalLight(const Ref<ConfigNode>& node)
        : mIntensity{ this->template make<SphericalTexture>(node->get("Intensity"sv)->as<Ref<ConfigNode>>()) }, mDirection{
              Direction<FrameOfReference::World>::fromRaw(glm::normalize(parseVec3(node->get("Direction"sv))))
          } {}
    [[nodiscard]] LightAttributes attributes() const noexcept override {
        return LightAttributes::Delta;
    }

    void updateTransform(const KeyFrames&, const TimeInterval) override {}

    virtual void preprocess(const Float& sceneRadius) override {
        mSceneRadius = sceneRadius;
        mSceneDiameter = Distance::fromRaw(sceneRadius * 2);
        mSceneDiameterSquare = DistanceSquare::fromRaw(sqr(sceneRadius * 2));
    }

    LightLiSample<Spectrum> sampleLi(const ShadingContext<Setting>& ctx, const Point<FrameOfReference::World>& pos,
                                     SampleProvider& sampler) const noexcept override {
        Intensity<Spectrum> intensity = Intensity<Spectrum>::fromRaw(
            mIntensity->evaluate({ mIntensity->dir2TexCoord(Direction<FrameOfReference::Object>::fromRaw(mDirection.raw())), ctx.t, 0U },
                                 ctx.sampledWavelength));
        const auto radiance = importanceSampled<PdfType::Light | PdfType::LightSampler>(intensity.toRadiance(mSceneDiameterSquare));
        return LightLiSample<Spectrum>{ -mDirection, radiance, InversePdf<PdfType::Light>::identity(), mSceneDiameter };
    }

    InversePdf<PdfType::Light> inversePdfLi(const ShadingContext<Setting>& ctx,
                                            const Direction<FrameOfReference::World>& wi) const noexcept override {
        return InversePdf<PdfType::Light>::invalid();
    }
    LightLeSample<Spectrum> sampleLe(const ShadingContext<Setting>& ctx, SampleProvider& sampler) const noexcept override {
        return LightLeSample<Spectrum>::invalid();
    }
    std::pair<InversePdf<PdfType::LightPos>, InversePdf<PdfType::LightDir>> pdfLe(const ShadingContext<Setting>& ctx,
                                                                                  const Ray& ray) const noexcept override {
        return { InversePdf<PdfType::LightPos>::invalid(), InversePdf<PdfType::LightDir>::invalid() };
    }

    [[nodiscard]] Power<MonoSpectrum> power() const noexcept override {
        return Intensity<MonoSpectrum>::fromRaw(mIntensity->mean()) * SolidAngle::fromRaw(pi * mSceneRadius * mSceneRadius);
    }
};

PIPER_REGISTER_VARIANT(DirectionalLight, Light);

PIPER_NAMESPACE_END
