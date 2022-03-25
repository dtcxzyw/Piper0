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

    std::variant<Float, Ref<Texture2D<Setting>>> mEta = 1.5f;
    Float mRoughnessU, mRoughnessV;
    bool mRemapRoughness = true;

    Float evaluateEta(const Float eta, const Wavelength&, const SurfaceHit&) const {
        return eta;
    }

    Float evaluateEta(const Ref<Texture2D<Setting>>& eta, const Wavelength& sampledWavelength, const SurfaceHit& intersection) const {
        const auto val = eta->evaluate(intersection.texCoord, sampledWavelength);
        if constexpr(std::is_same_v<Spectrum, MonoSpectrum>)
            return val;
        else
            return val.raw()[0];
    }

public:
    explicit Dielectric(const Ref<ConfigNode>& node) {
        if(const auto ptr = node->tryGet("Eta"sv)) {
            if((*ptr)->convertibleTo<Float>())
                mEta = (*ptr)->as<Float>();
            else
                mEta = this->template make<Texture2D>((*ptr)->as<Ref<ConfigNode>>());
        }

        if(const auto ptr = node->tryGet("RoughnessU"sv))
            mRoughnessU = (*ptr)->as<Float>();
        else
            mRoughnessU = node->get("Roughness"sv)->as<Float>();
        if(const auto ptr = node->tryGet("RoughnessV"sv))
            mRoughnessV = (*ptr)->as<Float>();
        else
            mRoughnessV = node->get("Roughness"sv)->as<Float>();
        if(const auto ptr = node->tryGet("RemapRoughness"sv))
            mRemapRoughness = (*ptr)->as<bool>();
    }

    BSDF<Setting> evaluate(const Wavelength& sampledWavelength, const SurfaceHit& intersection) const noexcept override {
        auto roughnessU = mRoughnessU, roughnessV = mRoughnessV;
        if(mRemapRoughness) {
            roughnessU = TrowbridgeReitzDistribution<Setting>::roughnessToAlpha(roughnessU);
            roughnessV = TrowbridgeReitzDistribution<Setting>::roughnessToAlpha(roughnessV);
        }

        // TODO: four-way reflection
        const auto eta = std::visit([&](const auto& x) { return evaluateEta(x, sampledWavelength, intersection); }, mEta);

        return BSDF<Setting>{ ShadingFrame{ intersection.shadingNormal.asDirection(), intersection.dpdu },
                              DielectricBxDF<Setting>{ eta, TrowbridgeReitzDistribution<Setting>(roughnessU, roughnessV) } };
    }

    [[nodiscard]] RGBSpectrum estimateAlbedo(const SurfaceHit&) const noexcept override {
        return identity<RGBSpectrum>();
    }
};

PIPER_REGISTER_VARIANT(Dielectric, Material);

PIPER_NAMESPACE_END
