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

#include <Piper/Render/BxDFs.hpp>
#include <Piper/Render/Material.hpp>
#include <Piper/Render/Texture.hpp>

// Please refer to https://www.khronos.org/registry/glTF/specs/2.0/glTF-2.0.html#appendix-b-brdf-implementation

PIPER_NAMESPACE_BEGIN

template <typename Setting>
class StandardBRDF final : public Material<Setting> {
    PIPER_IMPORT_SETTINGS();
    PIPER_IMPORT_SHADING();

    Ref<SpectrumTexture2D<Setting>> mBaseColor;
    Ref<ScalarTexture2D> mRoughness;
    Ref<ScalarTexture2D> mMetallic;
    Float mEta = 1.5f;

public:
    explicit StandardBRDF(const Ref<ConfigNode>& node)
        : mBaseColor{ this->template make<SpectrumTexture2D>(node->get("BaseColor"sv)->as<Ref<ConfigNode>>()) },
          mRoughness{ getScalarTexture2D(node, "Roughness"sv, ""sv, 0.0f) },  //
          mMetallic{ getScalarTexture2D(node, "Metallic"sv, ""sv, 0.0f) } {

        if(const auto ptr = node->tryGet("Eta"sv))
            mEta = (*ptr)->as<Float>();
    }

    BSDF<Setting> evaluate(const Wavelength& sampledWavelength, const SurfaceHit& intersection) const noexcept override {
        const auto baseColor = mBaseColor->evaluate(intersection.texCoord, sampledWavelength);
        const auto roughness = mRoughness->evaluate(intersection.texCoord);
        const auto metallic = mMetallic->evaluate(intersection.texCoord);
        const auto distribution = TrowbridgeReitzDistribution<Setting>(roughness, roughness);

        auto dielectric = SchlickMixedBxDF<Setting, LambertianBxDF<Setting>, DielectricBxDF<Setting>>{
            LambertianBxDF<Setting>{ Rational<Spectrum>::fromRaw(baseColor) }, DielectricBxDF<Setting>{ mEta, distribution }, mEta
        };

        const auto eta = spectrumCast<ConductorBxDF<Setting>::EtaType>(baseColor, sampledWavelength);
        auto metal = ConductorBxDF<Setting>{ { eta, zero<ConductorBxDF<Setting>::EtaType>() }, distribution };
        return BSDF<Setting>{ ShadingFrame{ intersection.shadingNormal.asDirection(), intersection.dpdu },
                              mixBxDF<Setting>(std::move(dielectric), std::move(metal), metallic), false };
    }

    [[nodiscard]] RGBSpectrum estimateAlbedo(const SurfaceHit&) const noexcept override {
        return identity<RGBSpectrum>();  // FIXME
    }
};

PIPER_REGISTER_VARIANT(StandardBRDF, Material);

PIPER_NAMESPACE_END
