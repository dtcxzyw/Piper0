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
class BSDFWrapper final : public BxDF<Setting> {
    PIPER_IMPORT_SETTINGS();
    PIPER_IMPORT_SHADING();

    std::pmr::memory_resource* mAllocator;
    BSDF<Setting>* mBSDF;

public:
    explicit BSDFWrapper(BSDF<Setting> bsdf)
        : mAllocator{ context().scopedAllocator }, mBSDF{ static_cast<BSDF<Setting>*>(
                                                       mAllocator->allocate(sizeof(bsdf), alignof(BSDF<Setting>))) } {
        new(mBSDF) BSDF<Setting>{ std::move(bsdf) };
    }

    BSDFWrapper(const BSDFWrapper&) = delete;
    BSDFWrapper(BSDFWrapper&& rhs) noexcept
        : mAllocator{ std::exchange(rhs.mAllocator, nullptr) }, mBSDF{ std::exchange(rhs.mBSDF, nullptr) } {}
    BSDFWrapper& operator=(const BSDFWrapper&) = delete;
    BSDFWrapper& operator=(BSDFWrapper&&) = delete;

    ~BSDFWrapper() override {
        if(mBSDF)
            mAllocator->deallocate(mBSDF, sizeof(BSDF<Setting>), alignof(BSDF<Setting>));
    }

    [[nodiscard]] BxDFPart part() const noexcept override {
        return mBSDF->cast()->part();
    }

    [[nodiscard]] Rational<Spectrum> evaluate(const Direction& wo, const Direction& wi,
                                              TransportMode transportMode) const noexcept override {
        return mBSDF->cast()->evaluate(wo, wi, transportMode);
    }

    BSDFSample sample(SampleProvider& sampler, const Direction& wo, TransportMode transportMode = TransportMode::Radiance,
                      BxDFDirection sampleDirection = BxDFDirection::All) const noexcept override {
        return mBSDF->cast()->sample(sampler, wo, transportMode, sampleDirection);
    }

    [[nodiscard]] InversePdfValue inversePdf(const Direction& wo, const Direction& wi, TransportMode transportMode,
                                             BxDFDirection sampleDirection = BxDFDirection::All) const noexcept override {
        return mBSDF->cast()->inversePdf(wo, wi, transportMode, sampleDirection);
    }
};

template <typename Setting>
class MixedMaterial final : public Material<Setting> {
    PIPER_IMPORT_SETTINGS();
    PIPER_IMPORT_SHADING();

    Ref<Material<Setting>> mMaterialA, mMaterialB;
    Ref<ScalarTexture2D> mWeight;

public:
    explicit MixedMaterial(const Ref<ConfigNode>& node)
        : mMaterialA{ this->template make<Material>(node->get("MaterialA"sv)->as<Ref<ConfigNode>>()) },
          mMaterialB{ this->template make<Material>(node->get("MaterialB"sv)->as<Ref<ConfigNode>>()) }, mWeight{
              getScalarTexture2D(node, "Weight"sv, ""sv, 1.0f)
          } {}

    BSDF<Setting> evaluate(const Wavelength& sampledWavelength, const SurfaceHit& intersection) const noexcept override {
        auto bsdf1 = mMaterialA->evaluate(sampledWavelength, intersection);
        auto bsdf2 = mMaterialB->evaluate(sampledWavelength, intersection);
        const auto textureEvaluateInfo = intersection.makeTextureEvaluateInfo();
        const auto weight = mWeight->evaluate(textureEvaluateInfo);

        // NOTICE: modified ShadingFrame (e.g., Normal/Bump Map) will be ignored
        const auto keepOneWavelength = bsdf1.keepOneWavelength() || bsdf2.keepOneWavelength();
        return BSDF<Setting>{ ShadingFrame{ intersection.shadingNormal.asDirection(), intersection.dpdu },
                              mixBxDF<Setting>(BSDFWrapper<Setting>{ std::move(bsdf1) }, BSDFWrapper<Setting>{ std::move(bsdf2) }, weight),
                              keepOneWavelength };
    }

    [[nodiscard]] RGBSpectrum estimateAlbedo(const SurfaceHit& intersection) const noexcept override {
        const auto textureEvaluateInfo = intersection.makeTextureEvaluateInfo();
        const auto weight = mWeight->evaluate(textureEvaluateInfo);
        return mix(mMaterialA->estimateAlbedo(intersection), mMaterialB->estimateAlbedo(intersection), weight);
    }
};

PIPER_REGISTER_VARIANT(MixedMaterial, Material);

PIPER_NAMESPACE_END
