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
class Dielectric final : public Material<Setting> {
    PIPER_IMPORT_SETTINGS();
    PIPER_IMPORT_SHADING();

    Ref<ScalarTexture2D> mEta;
    Ref<ScalarTexture2D> mRoughnessU, mRoughnessV;
    bool mRemapRoughness = true;

    auto evaluateEta(const Ref<ScalarTexture2D>& eta, const TextureEvaluateInfo& info, const Wavelength& sampledWavelength) const noexcept {
        if constexpr(isSpectral) {
            return eta->evaluateOneWavelength(info, sampledWavelength.firstComponent());
        } else {
            return std::pair{ false, eta->evaluate(info) };
        }
    }

public:
    explicit Dielectric(const Ref<ConfigNode>& node) {
        if(const auto ptr = node->tryGet("Material"sv)) {
            const ResolveConfiguration configuration;
            const auto path = resolvePath((*ptr)->as<std::string_view>());
            const auto subNode = parseJSONConfigNode(path, configuration);

            mEta = getStaticFactory().make<ScalarTexture2D>(subNode->get("Eta"sv)->as<Ref<ConfigNode>>());
        } else
            mEta = getScalarTexture2D(node, "Eta"sv, ""sv, 1.5f);

        mRoughnessU = getScalarTexture2D(node, "RoughnessU"sv, "Roughness"sv, 0.0f);
        mRoughnessV = getScalarTexture2D(node, "RoughnessV"sv, "Roughness"sv, 0.0f);

        if(const auto ptr = node->tryGet("RemapRoughness"sv))
            mRemapRoughness = (*ptr)->as<bool>();
    }

    BSDF<Setting> evaluate(const Wavelength& sampledWavelength, const SurfaceHit& intersection) const noexcept override {
        const auto textureEvaluateInfo = intersection.makeTextureEvaluateInfo();
        auto roughnessU = mRoughnessU->evaluate(textureEvaluateInfo), roughnessV = mRoughnessV->evaluate(textureEvaluateInfo);
        if(mRemapRoughness) {
            roughnessU = TrowbridgeReitzDistribution<Setting>::roughnessToAlpha(roughnessU);
            roughnessV = TrowbridgeReitzDistribution<Setting>::roughnessToAlpha(roughnessV);
        }

        const auto [keepOneWavelength, eta] = evaluateEta(mEta, textureEvaluateInfo, sampledWavelength);

        return BSDF<Setting>{ ShadingFrame{ intersection.shadingNormal.asDirection(), intersection.dpdu },
                              DielectricBxDF<Setting>{ eta, TrowbridgeReitzDistribution<Setting>(roughnessU, roughnessV) },
                              keepOneWavelength };
    }

    [[nodiscard]] RGBSpectrum estimateAlbedo(const SurfaceHit&) const noexcept override {
        return identity<RGBSpectrum>();  // FIXME
    }
};

PIPER_REGISTER_VARIANT(Dielectric, Material);

PIPER_NAMESPACE_END
