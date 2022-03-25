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
#include <Piper/Render/Transform.hpp>

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

inline bool Refract(glm::vec3 wi, glm::vec3 N, Float Eta, Float* etap, glm::vec3* wt) {
    Float cosT_i = dot(N, wi);
    Float eta(Eta);
    glm::vec3 n(N);
    if(cosT_i < 0) {
        eta = 1 / eta;
        cosT_i = -cosT_i;
        n = -N;
    }
    Float sin2T_i = std::max<Float>(0.0f, 1 - sqr(cosT_i));
    Float sin2T_t = sin2T_i / sqr(eta);
    if(sin2T_t >= 1)
        return false;
    Float cosT_t = sqrt(std::max<Float>(0.0f, 1 - sin2T_t));
    *wt = glm::normalize(-wi / eta + (cosT_i / eta - cosT_t) * n);
    if(etap)
        *etap = eta;
    return true;
}

inline Float FrDielectric(Float CosTheta_i, Float Eta) {
    Float cosT_i = clamp(CosTheta_i, -1, 1);
    Float eta(Eta);
    if(cosT_i < 0) {
        eta = 1 / eta;
        cosT_i = -cosT_i;
    }
    Float sin2T_i = 1 - sqr(cosT_i);
    Float sin2T_t = sin2T_i / sqr(eta);
    if(sin2T_t >= 1)
        return 1.0f;
    Float cosT_t = sqrt(std::max<Float>(0.0f, 1 - sin2T_t));
    Float r_parl = (eta * cosT_i - cosT_t) / (eta * cosT_i + cosT_t);
    Float r_perp = (cosT_i - eta * cosT_t) / (cosT_i + eta * cosT_t);
    return (sqr(r_parl) + sqr(r_perp)) / 2;
}

// Trowbridge-Reitz Distribution
template <typename Setting>
class TrowbridgeReitzDistribution {
    PIPER_IMPORT_SETTINGS();
    PIPER_IMPORT_SHADING();

    Float alpha_x, alpha_y;

public:
    TrowbridgeReitzDistribution() = default;

    TrowbridgeReitzDistribution(Float alpha_x, Float alpha_y) : alpha_x(alpha_x), alpha_y(alpha_y) {}

    Float D(const Direction& wm) const {
        if(cosTheta(wm) == 0.0f)
            return 0;
        Float tan2T = tan2Theta(wm);
        Float cos4T = sqr(cos2Theta(wm));
        if(cos4T < pow<4, Float>(epsilon))
            return 0;
        Float e = tan2T * (sqr(cosPhi(wm) / alpha_x) + sqr(sinPhi(wm) / alpha_y));
        return 1 / (pi * alpha_x * alpha_y * cos4T * sqr(1 + e));
    }

    bool EffectivelySmooth() const {
        return std::max<Float>(alpha_x, alpha_y) < 1e-3f;
    }

    Float Lambda(const Direction& w) const {
        if(cosTheta(w) == 0.0f)
            return 0;
        Float tan2T = tan2Theta(w);
        Float alpha2 = sqr(cosPhi(w) * alpha_x) + sqr(sinPhi(w) * alpha_y);
        return (std::sqrt(1 + alpha2 * tan2T) - 1) / 2;
    }

    Float G1(const Direction& w) const {
        return 1 / (1 + Lambda(w));
    }

    Float G(const Direction& wo, const Direction& wi) const {
        return 1 / (1 + Lambda(wo) + Lambda(wi));
    }

    Float D(const Direction& w, const Direction& wm) const {
        if(cosTheta(w) == 0.0f)
            return 0;
        return G1(w) / absCosTheta(w) * D(wm) * absDot(w, wm);
    }

    Float PDF(const Direction& w, const Direction& wm) const {
        return D(w, wm);
    }

    Direction Sample_wm(const Direction& w, glm::vec2 u) const {
        Direction wh = Direction::fromRaw(glm::normalize(glm::vec3(alpha_x * w.x(), alpha_y * w.y(), w.z())));
        if(wh.z() < 0.0f)
            wh = -wh;
        Direction T1 = (wh.z() < 0.99999f) ? Direction::fromRaw(glm::normalize(cross(glm::vec3(0, 0, 1), wh.raw()))) :
                                             Direction::fromRaw(glm::vec3(1, 0, 0));
        Direction T2 = cross(wh, T1);
        glm::vec2 p = sampleUniformDisk(u);
        Float h = std::sqrt(1 - sqr(p.x));
        p.y = lerp((1 + wh.z()) / 2, h, p.y);
        Float pz = std::sqrt(std::max<Float>(0.0f, 1 - sqr(p.x) - sqr(p.y)));
        glm::vec3 nh = p.x * T1.raw() + p.y * T2.raw() + pz * wh.raw();
        return Direction::fromRaw(glm::normalize(glm::vec3(alpha_x * nh.x, alpha_y * nh.y, std::max<Float>(1e-6f, nh.z))));
    }

    static Float RoughnessToAlpha(Float roughness) {
        // const auto x = std::log(fmax(roughness, epsilon));
        // return (((0.000640711f * x + 0.0171201f) * x + 0.1734f) * x + 0.819955f) * x + 1.62142f;
        return std::sqrt(roughness);
    }

    void Regularize() {
        if(alpha_x < 0.3f)
            alpha_x = clamp(2 * alpha_x, 0.1f, 0.3f);
        if(alpha_y < 0.3f)
            alpha_y = clamp(2 * alpha_y, 0.1f, 0.3f);
    }
};

// DielectricBxDF
template <typename Setting>
class DielectricBxDF final : public BxDF<Setting> {
    PIPER_IMPORT_SETTINGS();
    PIPER_IMPORT_SHADING();

    Float eta;
    TrowbridgeReitzDistribution<Setting> mfDistrib;

public:
    explicit DielectricBxDF(Float Eta, TrowbridgeReitzDistribution<Setting> MfDistrib) : eta(Eta), mfDistrib(MfDistrib) {}

    [[nodiscard]] BxDFPart part() const noexcept override {
        auto flags = (eta == 1.0f) ? BxDFPart::Transmission : (BxDFPart::Reflection | BxDFPart::Transmission);
        return flags | (mfDistrib.EffectivelySmooth() ? BxDFPart::Specular : BxDFPart::Glossy);
    }

    Rational<Spectrum> evaluate(const Direction& wo, const Direction& wi, TransportMode transportMode) const noexcept override {
        if(eta == 1 || mfDistrib.EffectivelySmooth())
            return Rational<Spectrum>::zero();
        Float cosT_o = cosTheta(wo);
        Float cosT_i = cosTheta(wi);
        bool reflect = cosT_i * cosT_o > 0;
        Float etap = 1.0f;
        if(!reflect)
            etap = cosT_o > 0 ? eta : (1 / eta);
        Direction wm = Direction::fromRaw(glm::normalize(wi.raw() * etap + wo.raw()));
        if(cosT_i == 0 || cosT_o == 0 || sqr(wm.x()) + sqr(wm.y()) + sqr(wm.z()) == 0)
            return Rational<Spectrum>::zero();
        wm = Direction::fromRaw(glm::faceforward(wm.raw(), glm::vec3(0, 0, 1), -wm.raw()));

        if(dot(wm, wi) * cosT_i < 0 || dot(wm, wo) * cosT_o < 0)
            return Rational<Spectrum>::zero();

        Float F = FrDielectric(dot(wo, wm), eta);

        if(reflect) {
            return mfDistrib.D(wm) * mfDistrib.G(wo, wi) * F / std::abs(4 * cosT_i * cosT_o) * Rational<Spectrum>::identity();
        } else {
            Float denom = sqr(dot(wi, wm) + dot(wo, wm) / etap) * cosT_i * cosT_o;
            Float ft = mfDistrib.D(wm) * (1 - F) * mfDistrib.G(wo, wi) * std::abs(dot(wi, wm) * dot(wo, wm) / denom);
            if(transportMode == TransportMode::Radiance)
                ft /= sqr(etap);
            return ft * Rational<Spectrum>::identity();
        }
    }

    BSDFSample sample(SampleProvider& sampler, const Direction& wo, const TransportMode transportMode,
                      const BxDFDirection sampleDirection) const noexcept override {
        if(eta == 1 || mfDistrib.EffectivelySmooth()) {
            Float R = FrDielectric(cosTheta(wo), eta);
            Float T = 1 - R;
            Float pr = R, pt = T;
            if(!match(sampleDirection, BxDFDirection::Reflection))
                pr = 0;
            if(!match(sampleDirection, BxDFDirection::Transmission))
                pt = 0;
            if(pr == 0 && pt == 0)
                return BSDFSample::invalid();

            if(sampler.sample() < pr / (pr + pt)) {
                Direction wi = Direction::fromRaw(glm::vec3(-wo.x(), -wo.y(), wo.z()));
                Float ft = R / absCosTheta(wi);

                return { wi, importanceSampled<PdfType::BSDF>(ft * Rational<Spectrum>::identity()),
                         InversePdfValue::fromRaw((pr + pt) / pr), BxDFPart::SpecularReflection };
            } else {
                glm::vec3 wm = glm::vec3(0, 0, sgn(wo.z()));
                Float etap = wo.z() < 0.0 ? eta : rcp(eta);
                glm::vec3 wiraw = glm::refract(-wo.raw(), wm, etap);

                if(wiraw.x == 0 && wiraw.y == 0 && wiraw.z == 0)
                    return BSDFSample::invalid();
                Direction wi = Direction::fromRaw(normalize(wiraw));

                Float ft = T / absCosTheta(wi);
                if(transportMode == TransportMode::Radiance)
                    ft /= sqr(etap);

                return { wi, importanceSampled<PdfType::BSDF>(ft * Rational<Spectrum>::identity()),
                         InversePdfValue::fromRaw((pr + pt) / pt), BxDFPart::SpecularTransmission };
            }
        } else {
            Direction wm = mfDistrib.Sample_wm(wo, sampler.sampleVec2());
            Float R = FrDielectric(dot(wo, wm), eta);
            Float T = 1 - R;
            Float pr = R, pt = T;
            if(!match(sampleDirection, BxDFDirection::Reflection))
                pr = 0;
            if(!match(sampleDirection, BxDFDirection::Transmission))
                pt = 0;
            if(pr == 0 && pt == 0)
                return BSDFSample::invalid();

            Float pdf;
            if(sampler.sample() < pr / (pr + pt)) {
                Direction wi = Direction::fromRaw(glm::reflect(-wo.raw(), wm.raw()));
                if(!sameHemisphere(wo, wi))
                    return BSDFSample::invalid();

                pdf = mfDistrib.PDF(wo, wm) / std::max<Float>(epsilon, 4 * absDot(wo, wm)) * pr / (pr + pt);
                if(pdf == 0.0f)
                    return BSDFSample::invalid();
                Float ft = mfDistrib.D(wm) * mfDistrib.G(wo, wi) * R / (4 * cosTheta(wi) * cosTheta(wo));

                return { wi, importanceSampled<PdfType::BSDF>(ft * Rational<Spectrum>::identity()), InversePdfValue::fromRaw(1 / pdf),
                         BxDFPart::GlossyReflection };
            } else {
                glm::vec3 wn = sgn(dot(wo, wm)) > 0.0 ? wm.raw() : -wm.raw();
                Float etap = sgn(dot(wo, wm)) < 0.0 ? eta : rcp(eta);
                glm::vec3 wiraw = glm::refract(-wo.raw(), wn, etap);

                if(wiraw.x == 0 && wiraw.y == 0 && wiraw.z == 0)
                    return BSDFSample::invalid();
                Direction wi = Direction::fromRaw(normalize(wiraw));

                if(sameHemisphere(wo, wi) || wi.z() == 0)
                    return BSDFSample::invalid();

                Float denom = sqr(dot(wi, wm) + dot(wo, wm) / etap);
                if(denom < 1e-8f)
                    return BSDFSample::invalid();
                Float dwm_dwi = absDot(wi, wm) / denom;
                pdf = mfDistrib.PDF(wo, wm) * dwm_dwi * pt / (pr + pt);
                if(pdf == 0.0f)
                    return BSDFSample::invalid();

                Float ft =
                    T * mfDistrib.D(wm) * mfDistrib.G(wo, wi) * std::abs(dot(wi, wm) * dot(wo, wm) / (cosTheta(wi) * cosTheta(wo) * denom));

                if(transportMode == TransportMode::Radiance)
                    ft /= sqr(etap);

                return { wi, importanceSampled<PdfType::BSDF>(ft * Rational<Spectrum>::identity()), InversePdfValue::fromRaw(1 / pdf),
                         BxDFPart::GlossyTransmission };
            }
        }
    }

    [[nodiscard]] InversePdfValue inversePdf(const Direction& wo, const Direction& wi, const TransportMode transportMode,
                                             const BxDFDirection sampleDirection) const noexcept override {
        if(eta == 1 || mfDistrib.EffectivelySmooth())
            return InversePdfValue::invalid();
        Float cosT_o = cosTheta(wo), cosT_i = cosTheta(wi);
        bool reflect = cosT_i * cosT_o > 0;
        Float etap = 1;
        if(!reflect)
            etap = cosT_o > 0 ? eta : (1 / eta);
        Direction wm = Direction::fromRaw(glm::normalize(wi.raw() * etap + wo.raw()));
        if(cosT_i == 0 || cosT_o == 0 || sqr(wm.x()) + sqr(wm.y()) + sqr(wm.z()) == 0)
            return InversePdfValue::invalid();
        wm = Direction::fromRaw(glm::faceforward(wm.raw(), glm::vec3(0, 0, 1), -wm.raw()));

        if(dot(wm, wi) * cosT_i < 0 || dot(wm, wo) * cosT_o < 0)
            return InversePdfValue::invalid();

        Float R = FrDielectric(dot(wo, wm), eta);
        Float T = 1 - R;

        Float pr = R, pt = T;
        if(!match(sampleDirection, BxDFDirection::Reflection))
            pr = 0;
        if(!match(sampleDirection, BxDFDirection::Transmission))
            pt = 0;
        if(pr == 0 && pt == 0)
            return InversePdfValue::invalid();

        Float pdf;
        if(reflect) {
            pdf = mfDistrib.PDF(wo, wm) / std::max<Float>(epsilon, 4 * absDot(wo, wm)) * pr / (pr + pt);
        } else {
            Float denom = sqr(dot(wi, wm) + dot(wo, wm) / etap);
            Float dwm_dwi = absDot(wi, wm) / denom;
            pdf = mfDistrib.PDF(wo, wm) * dwm_dwi * pt / (pr + pt);
        }

        if(pdf == 0.0f)
            return InversePdfValue::invalid();

        return InversePdfValue::fromRaw(1 / pdf);
    }
};

PIPER_NAMESPACE_END
