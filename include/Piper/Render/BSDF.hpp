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
#include <Piper/Render/SampleUtil.hpp>
#include <Piper/Render/Sampler.hpp>

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

#define PIPER_IMPORT_SHADING()                           \
    using Direction = Normal<FrameOfReference::Shading>; \
    using BSDFSample = BSDFSampleResult<Setting>;        \
    using InversePdfValue = InversePdf<PdfType::BSDF>

template <typename Setting>
struct BSDFSampleResult final {
    PIPER_IMPORT_SETTINGS();
    PIPER_IMPORT_SHADING();

    Direction wi;
    Rational<Spectrum, PdfType::BSDF> f;
    InversePdfValue inversePdf;
    BxDFPart part;
};

class BSDFBase : public RenderVariantBase {
public:
    virtual BxDFPart part() const noexcept = 0;
};

template <typename Setting>
class BSDF : public TypedRenderVariantBase<Setting, BSDFBase> {
public:
    PIPER_IMPORT_SETTINGS();
    PIPER_IMPORT_SHADING();

    virtual Rational<Spectrum> evaluate(const Direction& wo, const Direction& wi) const noexcept = 0;

    virtual BSDFSample sample(SampleProvider& sampler, const Direction& wo) const noexcept {
        const auto wi = sampleCosineHemisphere(sampler.sampleVec2());
        return { wi, importanceSampled<PdfType::BSDF>(this->evaluate(wo, wi)), this->inversePdf(wo, wi), this->part() };
    }

    [[nodiscard]] virtual InversePdfValue inversePdf(const Direction&, const Direction& wi) const noexcept {
        return wi.z() > 0.0f ? InversePdfValue::fromRaw(rcp(std::fabs(wi.z())) * pi) : InversePdfValue::invalid();
    }
};

template <typename Setting>
class BSDFArray final {
    PIPER_IMPORT_SETTINGS();
    PIPER_IMPORT_SHADING();

    static constexpr uint32_t maxBSDFCount = 8;
    static constexpr uint32_t maxBSDFSize = 64;

    std::byte mBSDFStorage[maxBSDFCount * maxBSDFSize]{};
    uint32_t mBSDFCount = 0;

    [[nodiscard]] const BSDF<Setting>* locate(const uint32_t idx) const noexcept {
        return reinterpret_cast<const BSDF<Setting>*>(mBSDFStorage + idx * maxBSDFSize);
    }

public:
    template <typename T>
    requires std::is_base_of_v<BSDF<Setting>, std::decay_t<T>>
    void emplace(T&& storage) {
        static_assert(sizeof(T) <= maxBSDFSize);
        *reinterpret_cast<T*>(mBSDFStorage + mBSDFCount * maxBSDFSize) = std::forward<T>(storage);
        ++mBSDFCount;
    }

    BSDFSample sample(SampleProvider& sampler, const Direction& wo) const noexcept {
        return locate(0)->sample(sampler, wo);
    }
    Rational<Spectrum> evaluate(const Direction& wo, const Direction& wi) const noexcept {
        return locate(0)->evaluate(wo, wi);
    }

    [[nodiscard]] InversePdfValue pdf(const Direction& wo, const Direction& wi) const noexcept {
        return locate(0)->pdf(wo, wi);
    }
};

PIPER_NAMESPACE_END
