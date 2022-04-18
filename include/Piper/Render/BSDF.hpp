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

enum class BxDFDirection { Reflection = 1 << 0, Transmission = 1 << 1, All = (1 << 2) - 1 };

PIPER_BIT_ENUM(BxDFDirection)

enum class BxDFPart {
    None = 0,
    Reflection = 1 << 0,
    Transmission = 1 << 1,
    Diffuse = 1 << 2,
    Specular = 1 << 3,
    Glossy = 1 << 4,

    DiffuseReflection = Diffuse | Reflection,
    DiffuseTransmission = Diffuse | Transmission,
    SpecularReflection = Specular | Reflection,
    SpecularTransmission = Specular | Transmission,
    GlossyReflection = Glossy | Reflection,
    GlossyTransmission = Glossy | Transmission,

    All = (1 << 5) - 1
};

PIPER_BIT_ENUM(BxDFPart)

constexpr bool hasNonSpecular(const BxDFPart part) noexcept {
    return static_cast<bool>(part & (BxDFPart::Diffuse | BxDFPart::Glossy));
}

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
    Float eta = 1.0f;

    [[nodiscard]] bool valid() const noexcept {
        return inversePdf.valid();
    }

    [[nodiscard]] constexpr static BSDFSampleResult invalid() noexcept {
        return { Piper::Direction<F>::undefined(), Rational<Spectrum, PdfType::BSDF>::undefined(), InversePdf<PdfType::BSDF>::invalid(),
                 BxDFPart::None };
    }
};

class BxDFBase {
public:
    BxDFBase() noexcept = default;
    BxDFBase(const BxDFBase&) noexcept = delete;
    BxDFBase& operator=(const BxDFBase&) = delete;
    BxDFBase(BxDFBase&&) noexcept = default;
    BxDFBase& operator=(BxDFBase&&) = delete;

    [[nodiscard]] virtual BxDFPart part() const noexcept = 0;
    virtual ~BxDFBase() noexcept = default;  // NOLINT(clang-diagnostic-deprecated-copy-with-dtor)
};

template <typename Setting>
class BxDF : public BxDFBase {
public:
    PIPER_IMPORT_SETTINGS();
    PIPER_IMPORT_SHADING();

    [[nodiscard]] virtual Rational<Spectrum> evaluate(const Direction& wo, const Direction& wi,
                                                      TransportMode transportMode) const noexcept = 0;

    virtual BSDFSample sample(SampleProvider& sampler, const Direction& wo, TransportMode transportMode = TransportMode::Radiance,
                              BxDFDirection sampleDirection = BxDFDirection::All) const noexcept = 0;

    [[nodiscard]] virtual InversePdfValue inversePdf(const Direction& wo, const Direction& wi, TransportMode transportMode,
                                                     BxDFDirection sampleDirection = BxDFDirection::All) const noexcept = 0;
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
};

template <typename Setting>
class BSDF final {
    PIPER_IMPORT_SETTINGS();
    PIPER_IMPORT_SHADING();

    static constexpr uint32_t maxBxDFSize = 256;

    ShadingFrame mFrame;
    std::byte mBxDFStorage[maxBxDFSize]{};
    bool mOwns;
    bool mKeepOneWavelength;

public:
    template <typename T>
    requires std::is_base_of_v<BxDF<Setting>, std::decay_t<T>> BSDF(const ShadingFrame& shadingFrame, T bxdf,
                                                                    const bool keepOneWavelength = false)
        : mFrame{ shadingFrame }, mOwns{ true }, mKeepOneWavelength{ keepOneWavelength } {
        static_assert(sizeof(T) <= maxBxDFSize);
        new(reinterpret_cast<T*>(mBxDFStorage)) T{ std::move(bxdf) };
    }
    BSDF(const BSDF&) = delete;
    BSDF& operator=(const BSDF&) = delete;
    BSDF(BSDF&& rhs)
    noexcept : mFrame{ rhs.mFrame }, mOwns{ std::exchange(rhs.mOwns, false) }, mKeepOneWavelength{ rhs.mKeepOneWavelength } {
        if(mOwns)
            memcpy(mBxDFStorage, rhs.mBxDFStorage, maxBxDFSize);
    }
    BSDF& operator=(BSDF&&) = delete;

    ~BSDF() {
        if(mOwns)
            std::destroy_at(reinterpret_cast<BxDF<Setting>*>(mBxDFStorage));
    }

    [[nodiscard]] const BxDF<Setting>* cast() const noexcept {
        return reinterpret_cast<const BxDF<Setting>*>(mBxDFStorage);
    }

    [[nodiscard]] BxDFPart part() const noexcept {
        return cast()->part();
    }

    [[nodiscard]] bool keepOneWavelength() const noexcept {
        return mKeepOneWavelength;
    }

    BSDFSampleResult<Setting, FrameOfReference::World> sample(SampleProvider& sampler, const Piper::Direction<FrameOfReference::World>& wo,
                                                              TransportMode transportMode = TransportMode::Radiance,
                                                              BxDFDirection sampleDirection = BxDFDirection::All) const noexcept {
        const auto res = cast()->sample(sampler, mFrame(wo), transportMode, sampleDirection);
        return BSDFSampleResult<Setting, FrameOfReference::World>{ mFrame(res.wi), res.f, res.inversePdf, res.part };
    }

    [[nodiscard]] Rational<Spectrum> evaluate(const Piper::Direction<FrameOfReference::World>& wo,
                                              const Piper::Direction<FrameOfReference::World>& wi,
                                              TransportMode transportMode = TransportMode::Radiance) const noexcept {
        return cast()->evaluate(mFrame(wo), mFrame(wi), transportMode);
    }

    [[nodiscard]] InversePdfValue pdf(const Piper::Direction<FrameOfReference::World>& wo,
                                      const Piper::Direction<FrameOfReference::World>& wi,
                                      TransportMode transportMode = TransportMode::Radiance,
                                      BxDFDirection sampleDirection = BxDFDirection::All) const noexcept {
        return cast()->inversePdf(mFrame(wo), mFrame(wi), transportMode, sampleDirection);
    }
};

PIPER_NAMESPACE_END
