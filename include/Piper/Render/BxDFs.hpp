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
#include <Piper/Render/Scattering.hpp>

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

    Rational<Spectrum> evaluate(const Direction& wo, const Direction& wi, TransportMode transportMode) const noexcept override {
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

    Float mEta;
    TrowbridgeReitzDistribution<Setting> mDistribution;

public:
    explicit DielectricBxDF(const Float eta, const TrowbridgeReitzDistribution<Setting>& distribution)
        : mEta(eta), mDistribution(distribution) {}

    [[nodiscard]] BxDFPart part() const noexcept override {
        const auto flags = mEta == 1.0f ? BxDFPart::Transmission : (BxDFPart::Reflection | BxDFPart::Transmission);
        return flags | (mDistribution.effectivelySmooth() ? BxDFPart::Specular : BxDFPart::Glossy);
    }

    Rational<Spectrum> evaluate(const Direction& wo, const Direction& wi, const TransportMode transportMode) const noexcept override {
        if(mEta == 1.0f || mDistribution.effectivelySmooth())
            return Rational<Spectrum>::zero();
        const auto cosThetaO = cosTheta(wo);
        const auto cosThetaI = cosTheta(wi);
        const auto reflect = cosThetaO * cosThetaI > 0.0f;
        const auto etaP = reflect ? 1.0f : (cosThetaO > 0.0f ? mEta : rcp(mEta));
        const auto halfVector = wi.raw() * etaP + wo.raw();

        if(cosThetaI == 0.0f || cosThetaO == 0.0f || glm::length2(halfVector) == 0.0f)
            return Rational<Spectrum>::zero();

        const auto wm = faceForward(Direction::fromRaw(glm::normalize(halfVector)), Direction::positiveZ());

        if(dot(wm, wi) * cosThetaI < 0.0f || dot(wm, wo) * cosThetaO < 0.0f)
            return Rational<Spectrum>::zero();

        const auto f = fresnelDielectric(dot(wo, wm), mEta);

        if(reflect) {
            return Rational<Spectrum>::fromScalar(mDistribution.evalD(wm) * mDistribution.evalG(wo, wi) * f /
                                                  std::fabs(4.0f * cosThetaI * cosThetaO));
        }

        const auto denominator = sqr(dot(wi, wm) + dot(wo, wm) / etaP) * cosThetaI * cosThetaO;
        auto ft = mDistribution.evalD(wm) * (1.0f - f) * mDistribution.evalG(wo, wi) * std::fabs(dot(wi, wm) * dot(wo, wm) / denominator);
        if(transportMode == TransportMode::Radiance)
            ft /= sqr(etaP);

        return Rational<Spectrum>::fromScalar(ft);
    }

    BSDFSample sample(SampleProvider& sampler, const Direction& wo, const TransportMode transportMode,
                      const BxDFDirection sampleDirection) const noexcept override {
        if(mEta == 1.0f || mDistribution.effectivelySmooth()) {
            auto reflection = fresnelDielectric(cosTheta(wo), mEta);
            auto transmission = 1.0f - reflection;
            if(!match(sampleDirection, BxDFDirection::Reflection))
                reflection = 0.0f;
            if(!match(sampleDirection, BxDFDirection::Transmission))
                transmission = 0.0f;
            if(transmission == 0.0f && reflection == 0.0f)
                return BSDFSample::invalid();

            if(sampler.sample() < reflection / (reflection + transmission)) {
                const auto wi = Direction::fromRaw({ -wo.x(), -wo.y(), wo.z() });
                const auto fr = reflection / absCosTheta(wi);

                return { wi, importanceSampled<PdfType::BSDF>(Rational<Spectrum>::fromScalar(fr)),
                         InversePdfValue::fromRaw((reflection + transmission) / reflection), BxDFPart::SpecularReflection };
            }

            const auto wm = glm::vec3{ 0, 0, wo.z() > 0.0f ? 1.0f : -1.0f };
            const auto wiRaw = glm::refract(-wo.raw(), wm, wo.z() < 0.0f ? mEta : rcp(mEta));

            if(glm::length2(wiRaw) == 0.0f)
                return BSDFSample::invalid();
            const auto wi = Direction::fromRaw(normalize(wiRaw));

            auto ft = transmission / absCosTheta(wi);

            const auto etaP = wo.z() > 0.0f ? mEta : rcp(mEta);
            if(transportMode == TransportMode::Radiance)
                ft /= sqr(etaP);

            return { wi, importanceSampled<PdfType::BSDF>(Rational<Spectrum>::fromScalar(ft)),
                     InversePdfValue::fromRaw((reflection + transmission) / transmission), BxDFPart::SpecularTransmission, etaP };
        }

        const auto wm = mDistribution.sampleWm(wo, sampler.sampleVec2());
        auto reflection = fresnelDielectric(dot(wo, wm), mEta);
        auto transmission = 1.0f - reflection;
        if(!match(sampleDirection, BxDFDirection::Reflection))
            reflection = 0.0f;
        if(!match(sampleDirection, BxDFDirection::Transmission))
            transmission = 0.0f;
        if(reflection == 0.0f && transmission == 0.0f)
            return BSDFSample::invalid();

        if(sampler.sample() < reflection / (reflection + transmission)) {
            const auto wi = Direction::fromRaw(glm::reflect(-wo.raw(), wm.raw()));
            if(!sameHemisphere(wo, wi))
                return BSDFSample::invalid();

            const auto pdf =
                mDistribution.pdf(wo, wm) / std::fmax(epsilon, 4.0f * absDot(wo, wm)) * reflection / (reflection + transmission);
            const auto fr = mDistribution.evalD(wm) * mDistribution.evalG(wo, wi) * reflection / (4.0f * cosTheta(wi) * cosTheta(wo));

            return { wi, importanceSampled<PdfType::BSDF>(Rational<Spectrum>::fromScalar(fr)), InversePdfValue::fromPdf(pdf),
                     BxDFPart::GlossyReflection };
        }

        const auto normal = dot(wo, wm) > 0.0f ? wm.raw() : -wm.raw();
        const auto wiRaw = glm::refract(-wo.raw(), normal, dot(wo, wm) < 0.0f ? mEta : rcp(mEta));

        if(glm::length2(wiRaw) == 0.0f)
            return BSDFSample::invalid();

        const auto wi = Direction::fromRaw(normalize(wiRaw));

        if(sameHemisphere(wo, wi) || wi.z() == 0.0f)
            return BSDFSample::invalid();

        const auto etaP = dot(wo, wm) > 0.0f ? mEta : rcp(mEta);

        const auto denominator = sqr(dot(wi, wm) + dot(wo, wm) / etaP);
        if(denominator < 1e-8f)
            return BSDFSample::invalid();
        const auto derv = absDot(wi, wm) / denominator;
        const auto pdf = mDistribution.pdf(wo, wm) * derv * transmission / (reflection + transmission);

        auto ft = transmission * mDistribution.evalD(wm) * mDistribution.evalG(wo, wi) *
            std::fabs(dot(wi, wm) * dot(wo, wm) / (cosTheta(wi) * cosTheta(wo) * denominator));

        if(transportMode == TransportMode::Radiance)
            ft /= sqr(etaP);

        return { wi, importanceSampled<PdfType::BSDF>(Rational<Spectrum>::fromScalar(ft)), InversePdfValue::fromPdf(pdf),
                 BxDFPart::GlossyTransmission, etaP };
    }

    [[nodiscard]] InversePdfValue inversePdf(const Direction& wo, const Direction& wi, const TransportMode transportMode,
                                             const BxDFDirection sampleDirection) const noexcept override {
        if(mEta == 1.0f || mDistribution.effectivelySmooth())
            return InversePdfValue::invalid();

        const auto cosThetaO = cosTheta(wo), cosThetaI = cosTheta(wi);
        const bool reflect = cosThetaO * cosThetaI > 0.0f;

        const auto etaP = reflect ? 1.0f : (cosThetaO > 0.0f ? mEta : rcp(mEta));
        const auto halfVector = wi.raw() * etaP + wo.raw();

        if(cosThetaI == 0.0f || cosThetaO == 0.0f || glm::length2(halfVector) == 0.0f)
            return InversePdfValue::invalid();

        const auto wm = faceForward(Direction::fromRaw(glm::normalize(halfVector)), Direction::positiveZ());

        if(dot(wm, wi) * cosThetaI < 0.0f || dot(wm, wo) * cosThetaO < 0.0f)
            return InversePdfValue::invalid();

        auto reflection = fresnelDielectric(dot(wo, wm), mEta);
        auto transmission = 1.0f - reflection;
        if(!match(sampleDirection, BxDFDirection::Reflection))
            reflection = 0.0f;
        if(!match(sampleDirection, BxDFDirection::Transmission))
            transmission = 0.0f;
        if(reflection == 0.0f && transmission == 0.0f)
            return InversePdfValue::invalid();

        Float pdf;
        if(reflect) {
            pdf = mDistribution.pdf(wo, wm) / std::fmax(epsilon, 4.0f * absDot(wo, wm)) * reflection / (reflection + transmission);
        } else {
            const auto denominator = sqr(dot(wi, wm) + dot(wo, wm) / etaP);
            const auto derv = absDot(wi, wm) / denominator;
            pdf = mDistribution.pdf(wo, wm) * derv * transmission / (reflection + transmission);
        }

        return InversePdfValue::fromPdf(pdf);
    }
};

// ConductorBxDF
template <typename Setting>
class ConductorBxDF final : public BxDF<Setting> {
    PIPER_IMPORT_SETTINGS();
    PIPER_IMPORT_SHADING();

    Float mEta, mK;
    TrowbridgeReitzDistribution<Setting> mDistribution;

public:
    explicit ConductorBxDF(Float eta, Float k, TrowbridgeReitzDistribution<Setting> distribution)
        : mEta(eta), mK(k), mDistribution(distribution) {}

    [[nodiscard]] BxDFPart part() const noexcept override {
        return mDistribution.effectivelySmooth() ? BxDFPart::SpecularReflection : BxDFPart::GlossyReflection;
    }

    Rational<Spectrum> makeBSDF(const Float f) const {
        if constexpr(std::is_same_v<Spectrum, SampledSpectrum>) {
            // TODO: four-way reflection
            auto res = glm::zero<SampledSpectrum::VecType>();
            res[0] = f;  // TODO: trace pdf
            return Rational<Spectrum>::fromRaw(SampledSpectrum::fromRaw(res));
        } else
            return Rational<Spectrum>::fromScalar(f);
    }

    Rational<Spectrum> evaluate(const Direction& wo, const Direction& wi, TransportMode transportMode) const noexcept override {
        if(!sameHemisphere(wo, wi) || mDistribution.effectivelySmooth())
            return Rational<Spectrum>::zero();
        Float cosThetaO = absCosTheta(wo);
        Float cosThetaI = absCosTheta(wi);
        if(cosThetaI == 0 || cosThetaO == 0)
            return Rational<Spectrum>::zero();
        Direction wm = Direction::fromRaw(normalize(wi.raw() + wo.raw()));
        if(dot(wm, wm) == 0)
            return Rational<Spectrum>::zero();
        Float ft =
            fresnelComplex(absDot(wo, wm), mEta, mK) * mDistribution.evalD(wm) * mDistribution.evalG(wo, wi) / (4 * cosThetaI * cosThetaO);
        return makeBSDF(ft);
    }

    BSDFSample sample(SampleProvider& sampler, const Direction& wo, const TransportMode transportMode,
                      const BxDFDirection sampleDirection) const noexcept override {
        if(!match(sampleDirection, BxDFDirection::Reflection))
            return BSDFSample::invalid();
        if(mDistribution.effectivelySmooth()) {
            Direction wi = Direction::fromRaw(glm::vec3(-wo.x(), -wo.y(), wo.z()));
            Float ft = fresnelComplex(absCosTheta(wi), mEta, mK) / absCosTheta(wi);
            return { wi, importanceSampled<PdfType::BSDF>(ft * Rational<Spectrum>::identity()), InversePdfValue::fromRaw(1),
                     BxDFPart::SpecularReflection };
        }
        if(wo.z() == 0)
            return BSDFSample::invalid();
        Direction wm = mDistribution.sampleWm(wo, sampler.sampleVec2());
        Direction wi = Direction::fromRaw(glm::reflect(-wo.raw(), wm.raw()));
        if(!sameHemisphere(wo, wi))
            return BSDFSample::invalid();
        Float pdf = mDistribution.pdf(wo, wm) / (4 * absDot(wo, wm));
        Float cosThetaO = absCosTheta(wo);
        Float cosThetaI = absCosTheta(wi);
        if(cosThetaI == 0 || cosThetaO == 0)
            return BSDFSample::invalid();
        Float ft =
            fresnelComplex(absDot(wo, wm), mEta, mK) * mDistribution.evalD(wm) * mDistribution.evalG(wo, wi) / (4 * cosThetaI * cosThetaO);
        return { wi, importanceSampled<PdfType::BSDF>(ft * Rational<Spectrum>::identity()), InversePdfValue::fromRaw(rcp(pdf)),
                 BxDFPart::GlossyReflection };
    }

    [[nodiscard]] InversePdfValue inversePdf(const Direction& wo, const Direction& wi, const TransportMode transportMode,
                                             const BxDFDirection sampleDirection) const noexcept override {
        if(!match(sampleDirection, BxDFDirection::Reflection) || !sameHemisphere(wo, wi) || mDistribution.effectivelySmooth())
            return InversePdfValue::invalid();
        Direction wh = Direction::fromRaw(normalize(wi.raw() + wo.raw()));
        if(dot(wh, wh) == 0)
            return InversePdfValue::invalid();
        wh = faceForward(wh, Direction::fromRaw(glm::vec3(0, 0, 1)));
        Float pdf = mDistribution.pdf(wo, wh) / (4 * absDot(wo, wh));
        return InversePdfValue::fromPdf(pdf);
    }
};

PIPER_NAMESPACE_END
