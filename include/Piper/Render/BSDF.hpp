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

#pragma once
#include <Piper/Render/RenderGlobalSetting.hpp>
#include <Piper/Render/Sampler.hpp>
#include <Piper/Render/SamplingUtil.hpp>

PIPER_NAMESPACE_BEGIN

enum class BxDFPart {
    Reflection = 1 << 0,
    Transmission = 1 << 1,
    Diffuse = 1 << 2,
    Specular = 1 << 3,
    Glossy = 1 << 4,
    All = (1 << 5) - 1
};

PIPER_BIT_ENUM(BxDFPart)

enum class TransportMode : uint8_t { Radiance, Importance };

#define PIPER_IMPORT_SHADING()                                               \
    using Direction = Piper::Direction<FrameOfReference::Shading>;           \
    using BSDFSample = BSDFSampleResult<Setting, FrameOfReference::Shading>; \
    using InversePdfValue = InversePdf<PdfType::BSDF>

template <typename Setting, FrameOfReference F>
struct BSDFSampleResult final {
    PIPER_IMPORT_SETTINGS();
    PIPER_IMPORT_SHADING();

    Piper::Direction<F> wi;
    Rational<Spectrum, PdfType::BSDF> f;
    InversePdfValue inversePdf;
    BxDFPart part;

    [[nodiscard]] bool valid() const noexcept {
        return inversePdf.valid();
    }
};

class BxDFBase : public RenderVariantBase {
public:
    virtual BxDFPart part() const noexcept = 0;
};

template <typename Setting>
class BxDF : public TypedRenderVariantBase<Setting, BxDFBase> {
public:
    PIPER_IMPORT_SETTINGS();
    PIPER_IMPORT_SHADING();

    virtual Rational<Spectrum> evaluate(const Direction& wo, const Direction& wi) const noexcept = 0;

    virtual BSDFSample sample(SampleProvider& sampler, const Direction& wo) const noexcept {
        auto wi = sampleCosineHemisphere(sampler.sampleVec2());
        if(wo.z() < 0.0f)
            wi.flipZ();
        return { wi, importanceSampled<PdfType::BSDF>(this->evaluate(wo, wi)), this->inversePdf(wo, wi), this->part() };
    }

    [[nodiscard]] virtual InversePdfValue inversePdf(const Direction& wo, const Direction& wi) const noexcept {
        return wo.z() * wi.z() > 0.0f ? InversePdfValue::fromRaw(rcp(std::fabs(wi.z())) * pi) : InversePdfValue::invalid();
    }
};

class ShadingFrame final {
    glm::mat3 mMatTBN{};

public:
    ShadingFrame(const Direction<FrameOfReference::World> shadingNormal, const Direction<FrameOfReference::World> dpdu) noexcept {
        const auto normal = shadingNormal.raw();
        auto tangent = dpdu.raw();
        const auto biTangent = glm::cross(normal, tangent);
        tangent = glm::cross(biTangent, normal);
        mMatTBN = { tangent, biTangent, normal };
    }

    Direction<FrameOfReference::Shading> operator()(const Direction<FrameOfReference::World>& x) const noexcept {
        return Direction<FrameOfReference::Shading>::fromRaw(x.raw() * mMatTBN);
    }

    Direction<FrameOfReference::World> operator()(const Direction<FrameOfReference::Shading>& x) const noexcept {
        return Direction<FrameOfReference::World>::fromRaw(mMatTBN * x.raw());
    }

    [[nodiscard]] Direction<FrameOfReference::World> shadingNormal() const noexcept {
        return Direction<FrameOfReference::World>::fromRaw(mMatTBN[2]);
    }
};

template <typename Setting>
class BSDF final {
    PIPER_IMPORT_SETTINGS();
    PIPER_IMPORT_SHADING();

    static constexpr uint32_t maxBxDFSize = 256;

    ShadingFrame mFrame;
    std::byte mBxDFStorage[maxBxDFSize]{};

    [[nodiscard]] const BxDF<Setting>* cast() const noexcept {
        return reinterpret_cast<const BxDF<Setting>*>(mBxDFStorage);
    }

public:
    template <typename T>
    requires std::is_base_of_v<BxDF<Setting>, std::decay_t<T>> BSDF(const ShadingFrame& shadingFrame, const T& bxdf)
        : mFrame{ shadingFrame } {
        static_assert(sizeof(T) <= maxBxDFSize);
        memcpy(mBxDFStorage, &bxdf, sizeof(T));
    }

    [[nodiscard]] Piper::Direction<FrameOfReference::World> shadingNormal() const noexcept {
        return mFrame.shadingNormal();
    }

    BSDFSampleResult<Setting, FrameOfReference::World> sample(SampleProvider& sampler,
                                                              const Piper::Direction<FrameOfReference::World>& wo) const noexcept {
        const auto res = cast()->sample(sampler, mFrame(wo));
        return BSDFSampleResult<Setting, FrameOfReference::World>{ mFrame(res.wi), res.f, res.inversePdf, res.part };
    }

    Rational<Spectrum> evaluate(const Piper::Direction<FrameOfReference::World>& wo,
                                const Piper::Direction<FrameOfReference::World>& wi) const noexcept {
        return cast()->evaluate(mFrame(wo), mFrame(wi));
    }

    [[nodiscard]] InversePdfValue pdf(const Piper::Direction<FrameOfReference::World>& wo,
                                      const Piper::Direction<FrameOfReference::World>& wi) const noexcept {
        return cast()->pdf(mFrame(wo), mFrame(wi));
    }
};

PIPER_NAMESPACE_END
