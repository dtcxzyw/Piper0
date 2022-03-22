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
#include <Piper/Render/BSDF.hpp>

PIPER_NAMESPACE_BEGIN

// DiffuseBxDF
template <typename Setting>
class LambertianBxDF final : public BxDF<Setting> {
    PIPER_IMPORT_SETTINGS();
    PIPER_IMPORT_SHADING();

    Rational<Spectrum> mReflectance;

public:
    explicit LambertianBxDF(const Rational<Spectrum>& reflectance) : mReflectance{ reflectance } {}

    [[nodiscard]] BxDFPart part() const noexcept override {
        return BxDFPart::DiffuseReflection;
    }

    Rational<Spectrum> evaluate(const Direction& wo, const Direction& wi, TransportMode) const noexcept override {
        if(!sameHemisphere(wo, wi))
            return Rational<Spectrum>::zero();
        return mReflectance * invPi;
    }

    BSDFSample sample(SampleProvider& sampler, const Direction& wo, const TransportMode transportMode,
                      const BxDFDirection sampleDirection) const noexcept override {
        if(!match(sampleDirection, BxDFDirection::Reflection))
            return BSDFSample::invalid();
        auto wi = sampleCosineHemisphere(sampler.sampleVec2());
        if(wo.z() < 0.0f)
            wi.flipZ();
        return { wi, importanceSampled<PdfType::BSDF>(evaluate(wo, wi, transportMode)), cosineHemispherePdf(absCosTheta(wi)),
                 BxDFPart::DiffuseReflection };
    }

    [[nodiscard]] InversePdfValue inversePdf(const Direction& wo, const Direction& wi, const TransportMode transportMode,
                                             const BxDFDirection sampleDirection) const noexcept override {
        if(!match(sampleDirection, BxDFDirection::Reflection) || !sameHemisphere(wo, wi))
            return InversePdfValue::invalid();
        return cosineHemispherePdf(absCosTheta(wi));
    }
};

// DielectricBxDF
template <typename Setting>
class DielectricBxDF final : public BxDF<Setting> {
    PIPER_IMPORT_SETTINGS();
    PIPER_IMPORT_SHADING();

    Rational<Spectrum> mReflectance;

public:
    explicit DielectricBxDF(const Rational<Spectrum>& reflectance) : mReflectance{ reflectance } {}

    [[nodiscard]] BxDFPart part() const noexcept override {
        return BxDFPart::Glossy | BxDFPart::Reflection | BxDFPart::Transmission;  // TODO: FIXME
    }

    Rational<Spectrum> evaluate(const Direction& wo, const Direction& wi, TransportMode) const noexcept override {
        if(!sameHemisphere(wo, wi))
            return Rational<Spectrum>::zero();
        return mReflectance * invPi;
    }

    BSDFSample sample(SampleProvider& sampler, const Direction& wo, const TransportMode transportMode,
                      const BxDFDirection sampleDirection) const noexcept override {
        if(!match(sampleDirection, BxDFDirection::Reflection))
            return BSDFSample::invalid();
        auto wi = sampleCosineHemisphere(sampler.sampleVec2());
        if(wo.z() < 0.0f)
            wi.flipZ();
        return { wi, importanceSampled<PdfType::BSDF>(evaluate(wo, wi, transportMode)), cosineHemispherePdf(absCosTheta(wi)),
                 BxDFPart::DiffuseReflection };
    }

    [[nodiscard]] InversePdfValue inversePdf(const Direction& wo, const Direction& wi, const TransportMode transportMode,
                                             const BxDFDirection sampleDirection) const noexcept override {
        if(!match(sampleDirection, BxDFDirection::Reflection) || !sameHemisphere(wo, wi))
            return InversePdfValue::invalid();
        return cosineHemispherePdf(absCosTheta(wi));
    }
};

PIPER_NAMESPACE_END
