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

PIPER_NAMESPACE_BEGIN

template <typename Setting>
class Conductor final : public Material<Setting> {
    PIPER_IMPORT_SETTINGS();
    PIPER_IMPORT_SHADING();

    Ref<SpectrumTexture2D<Setting>> mEta, mK;
    Ref<ScalarTexture2D> mRoughnessU, mRoughnessV;
    bool mRemapRoughness = true;

    auto evaluateEta(const Ref<SpectrumTexture2D<Setting>>& eta, const TexCoord texCoord,
                     const Wavelength& sampledWavelength) const noexcept {
        if constexpr(isSpectral) {
            return eta->evaluateOneWavelength(texCoord, sampledWavelength.firstComponent());
        } else {
            return std::pair<bool, Spectrum>{ false, eta->evaluate(texCoord, sampledWavelength) };
        }
    }

public:
    explicit Conductor(const Ref<ConfigNode>& node) {
        if(const auto ptr = node->tryGet("Material"sv)) {
            const ResolveConfiguration configuration;
            const auto path = resolvePath((*ptr)->as<std::string_view>());
            const auto subNode = parseJSONConfigNode(path, configuration);

            mEta = this->template make<SpectrumTexture2D>(subNode->get("Eta"sv)->as<Ref<ConfigNode>>());
            mK = this->template make<SpectrumTexture2D>(subNode->get("K"sv)->as<Ref<ConfigNode>>());
        } else {
            mEta = this->template make<SpectrumTexture2D>(node->get("Eta"sv)->as<Ref<ConfigNode>>());
            mK = this->template make<SpectrumTexture2D>(node->get("K"sv)->as<Ref<ConfigNode>>());
        }

        mRoughnessU = getScalarTexture2D(node, "RoughnessU"sv, "Roughness"sv, 0.0f);
        mRoughnessV = getScalarTexture2D(node, "RoughnessV"sv, "Roughness"sv, 0.0f);

        if(const auto ptr = node->tryGet("RemapRoughness"sv))
            mRemapRoughness = (*ptr)->as<bool>();
    }

    BSDF<Setting> evaluate(const Wavelength& sampledWavelength, const SurfaceHit& intersection) const noexcept override {
        auto roughnessU = mRoughnessU->evaluate(intersection.texCoord), roughnessV = mRoughnessV->evaluate(intersection.texCoord);
        if(mRemapRoughness) {
            roughnessU = TrowbridgeReitzDistribution<Setting>::roughnessToAlpha(roughnessU);
            roughnessV = TrowbridgeReitzDistribution<Setting>::roughnessToAlpha(roughnessV);
        }

        const auto [keepOneWavelengthEta, eta] = evaluateEta(mEta, intersection.texCoord, sampledWavelength);
        const auto [keepOneWavelengthK, k] = evaluateEta(mK, intersection.texCoord, sampledWavelength);

        return BSDF<Setting>{ ShadingFrame{ intersection.shadingNormal.asDirection(), intersection.dpdu },
                              ConductorBxDF<Setting>{ { eta, k }, TrowbridgeReitzDistribution<Setting>(roughnessU, roughnessV) },
                              keepOneWavelengthEta || keepOneWavelengthK };
    }

    [[nodiscard]] RGBSpectrum estimateAlbedo(const SurfaceHit&) const noexcept override {
        return identity<RGBSpectrum>();  // FIXME
    }
};

PIPER_REGISTER_VARIANT(Conductor, Material);

PIPER_NAMESPACE_END
