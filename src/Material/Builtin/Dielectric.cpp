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

    Float eta = 1.5f;
    Float uRoughness, vRoughness;
    bool remapRoughness = true;

public:
    explicit Dielectric(const Ref<ConfigNode>& node) {
        if(const auto ptr = node->tryGet("Eta"sv))
            eta = (*ptr)->as<Float>();
        if(const auto ptr = node->tryGet("RoughnessU"sv))
            uRoughness = (*ptr)->as<Float>();
        else
            uRoughness = node->get("Roughness"sv)->as<Float>();
        if(const auto ptr = node->tryGet("RoughnessV"sv))
            vRoughness = (*ptr)->as<Float>();
        else
            vRoughness = node->get("Roughness"sv)->as<Float>();
        if(const auto ptr = node->tryGet("RemapRoughness"sv))
            remapRoughness = (*ptr)->as<bool>();
    }

    BSDF<Setting> evaluate(const Wavelength& sampledWavelength, const SurfaceHit& intersection) const noexcept override {
        Float urough = uRoughness / 1000, vrough = vRoughness / 1000;
        if(remapRoughness) {
            urough = TrowbridgeReitzDistribution<Setting>::RoughnessToAlpha(urough);
            vrough = TrowbridgeReitzDistribution<Setting>::RoughnessToAlpha(vrough);
        }
        return BSDF<Setting>{ ShadingFrame{ intersection.shadingNormal.asDirection(), intersection.dpdu },
                              DielectricBxDF<Setting>{ eta, TrowbridgeReitzDistribution<Setting>(urough, vrough) } };
    }

    [[nodiscard]] RGBSpectrum estimateAlbedo(const SurfaceHit&) const noexcept override {
        return identity<RGBSpectrum>();
    }
};

PIPER_REGISTER_VARIANT(Dielectric, Material);

PIPER_NAMESPACE_END
