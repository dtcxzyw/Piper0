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

static SurfaceHit modifyNormal(const SurfaceHit& intersection, const Direction<FrameOfReference::Shading>& normal) {
    const ShadingFrame originShadingFrame{ intersection.shadingNormal.asDirection(), intersection.dpdu };
    const auto newNormal = originShadingFrame(normal);
    const auto dpdv = cross(newNormal, intersection.dpdu);
    const auto dpdu = cross(dpdv, newNormal);

    SurfaceHit newIntersection = intersection;
    newIntersection.shadingNormal = Normal<FrameOfReference::World>::fromRaw(newNormal.raw());
    newIntersection.dpdu = dpdu;
    return newIntersection;
}

template <typename Setting>
class NormalMap final : public Material<Setting> {
    PIPER_IMPORT_SETTINGS();
    PIPER_IMPORT_SHADING();

    Ref<Material<Setting>> mMaterial;
    Ref<NormalizedTexture2D> mNormalMap;

    [[nodiscard]] SurfaceHit preprocess(const SurfaceHit& intersection) const noexcept {
        const auto normal = mNormalMap->evaluate(intersection.texCoord);
        return modifyNormal(intersection, normal);
    }

public:
    explicit NormalMap(const Ref<ConfigNode>& node)
        : mMaterial{ this->template make<Material>(node->get("Material"sv)->as<Ref<ConfigNode>>()) }, mNormalMap{
              getStaticFactory().make<NormalizedTexture2D>(node->get("NormalMap"sv)->as<Ref<ConfigNode>>())
          } {}

    BSDF<Setting> evaluate(const Wavelength& sampledWavelength, const SurfaceHit& intersection) const noexcept override {
        const auto modifiedIntersection = preprocess(intersection);
        return mMaterial->evaluate(sampledWavelength, modifiedIntersection);
    }

    [[nodiscard]] RGBSpectrum estimateAlbedo(const SurfaceHit& intersection) const noexcept override {
        const auto modifiedIntersection = preprocess(intersection);
        return mMaterial->estimateAlbedo(modifiedIntersection);
    }
};

PIPER_REGISTER_VARIANT(NormalMap, Material);

template <typename Setting>
class BumpMap final : public Material<Setting> {
    PIPER_IMPORT_SETTINGS();
    PIPER_IMPORT_SHADING();

    Ref<Material<Setting>> mMaterial;
    Ref<ScalarTexture2D> mBumpMap;

    [[nodiscard]] SurfaceHit preprocess(const SurfaceHit& intersection) const noexcept {
        constexpr auto dx = 1e-4f;
        const auto [u, v] = intersection.texCoord;
        const auto base = mBumpMap->evaluate({ u, v });
        const auto dhdu = (mBumpMap->evaluate({ u + dx, v }) - base) / dx;
        const auto dhdv = (mBumpMap->evaluate({ u, v + dx }) - base) / dx;

        const auto normal = Direction::fromRaw(glm::normalize(glm::vec3{ -dhdu, -dhdv, 1.0f }));
        return modifyNormal(intersection, normal);
    }

public:
    explicit BumpMap(const Ref<ConfigNode>& node)
        : mMaterial{ this->template make<Material>(node->get("Material"sv)->as<Ref<ConfigNode>>()) },  //
          mBumpMap{ getScalarTexture2D(node, "BumpMap"sv, ""sv, 0.0f) } {}

    BSDF<Setting> evaluate(const Wavelength& sampledWavelength, const SurfaceHit& intersection) const noexcept override {
        const auto modifiedIntersection = preprocess(intersection);
        return mMaterial->evaluate(sampledWavelength, modifiedIntersection);
    }

    [[nodiscard]] RGBSpectrum estimateAlbedo(const SurfaceHit& intersection) const noexcept override {
        const auto modifiedIntersection = preprocess(intersection);
        return mMaterial->estimateAlbedo(modifiedIntersection);
    }
};

PIPER_REGISTER_VARIANT(BumpMap, Material);

PIPER_NAMESPACE_END
