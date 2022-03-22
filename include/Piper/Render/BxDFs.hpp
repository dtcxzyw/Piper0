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
#include <Piper/Core/Report.hpp>
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

// VecMath
inline Float Clamp(Float val, Float low, Float high) {
    if(val < low)
        return low;
    else if(val > high)
        return high;
    else
        return val;
}

inline Float CosTheta(glm::vec3 w) {
    return w.z;
}

inline Float Cos2Theta(glm::vec3 w) {
    return sqr(w.z);
}

inline Float AbsCosTheta(glm::vec3 w) {
    return std::abs(w.z);
}

inline Float Sin2Theta(glm::vec3 w) {
    return std::max<Float>(0, 1 - Cos2Theta(w));
}

inline Float SinTheta(glm::vec3 w) {
    return std::sqrt(Sin2Theta(w));
}

inline Float TanTheta(glm::vec3 w) {
    return SinTheta(w) / CosTheta(w);
}

inline Float Tan2Theta(glm::vec3 w) {
    return Sin2Theta(w) / Cos2Theta(w);
}

inline Float CosPhi(glm::vec3 w) {
    Float sinTheta = SinTheta(w);
    return (sinTheta == 0) ? 1 : Clamp(w.x / sinTheta, -1, 1);
}

inline Float SinPhi(glm::vec3 w) {
    Float sinTheta = SinTheta(w);
    return (sinTheta == 0) ? 0 : Clamp(w.y / sinTheta, -1, 1);
}

inline glm::vec3 Normalize(glm::vec3 v) {
    return v / std::sqrt(sqr(v.x) + sqr(v.y) + sqr(v.z));
}

inline Float Lerp(Float x, Float a, Float b) {
    return (1 - x) * a + x * b;
}

inline glm::vec3 FaceForward(glm::vec3 v, glm::vec3 n2) {
    return (dot(v, n2) < 0.0f) ? -v : v;
}

inline Float FrDielectric(Float CosTheta_i, Float Eta) {
    Float cosTheta_i = Clamp(CosTheta_i, -1, 1);
    Float eta(Eta);
    if(cosTheta_i < 0) {
        eta = 1 / eta;
        cosTheta_i = -cosTheta_i;
    }
    Float sin2Theta_i = 1 - sqr(cosTheta_i);
    Float sin2Theta_t = sin2Theta_i / sqr(eta);
    if(sin2Theta_t >= 1)
        return 1.0f;
    Float cosTheta_t = sqrt(std::max<Float>(0.0f, 1 - sin2Theta_t));
    Float r_parl = (eta * cosTheta_i - cosTheta_t) / (eta * cosTheta_i + cosTheta_t);
    Float r_perp = (cosTheta_i - eta * cosTheta_t) / (cosTheta_i + eta * cosTheta_t);
    return (sqr(r_parl) + sqr(r_perp)) / 2;
}

inline bool Refract(glm::vec3 wi, glm::vec3 N, Float Eta, Float* etap, glm::vec3* wt) {
    Float cosTheta_i = dot(N, wi);
    Float eta(Eta);
    glm::vec3 n(N);
    if(cosTheta_i < 0) {
        eta = 1 / eta;
        cosTheta_i = -cosTheta_i;
        n = -N;
    }
    Float sin2Theta_i = std::max<Float>(0.0f, 1 - sqr(cosTheta_i));
    Float sin2Theta_t = sin2Theta_i / sqr(eta);
    if(sin2Theta_t >= 1)
        return false;
    Float cosTheta_t = sqrt(std::max<Float>(0.0f, 1 - sin2Theta_t));
    *wt = -wi / eta + (cosTheta_i / eta - cosTheta_t) * n;
    if(etap)
        *etap = eta;
    return true;
}

inline glm::vec3 Reflect(const glm::vec3 wo, glm::vec3 n) {
    return -wo + 2 * dot(wo, n) * n;
}

// TrowbridgeReitzDistribution
template <typename Setting>
class TrowbridgeReitzDistribution {
    PIPER_IMPORT_SETTINGS();
    PIPER_IMPORT_SHADING();

    Float alpha_x, alpha_y;

public:
    TrowbridgeReitzDistribution() = default;

    TrowbridgeReitzDistribution(Float alpha_x, Float alpha_y) : alpha_x(alpha_x), alpha_y(alpha_y) {}

    Float D(const Direction& wm) const {
        Float tan2Theta = Tan2Theta(wm.raw());
        if(std::isinf(tan2Theta))
            return 0;
        Float cos4Theta = sqr(Cos2Theta(wm.raw()));
        if(cos4Theta < 1e-16f)
            return 0;
        Float e = tan2Theta * (sqr(CosPhi(wm.raw()) / alpha_x) + sqr(SinPhi(wm.raw()) / alpha_y));
        return 1 / (pi * alpha_x * alpha_y * cos4Theta * sqr(1 + e));
    }

    bool EffectivelySmooth() const {
        return std::max<Float>(alpha_x, alpha_y) < epsilon;
    }

    Float Lambda(const Direction& w) const {
        Float tan2Theta = Tan2Theta(w.raw());
        if(std::isinf(tan2Theta))
            return 0;
        Float alpha2 = sqr(CosPhi(w.raw()) / alpha_x) + sqr(SinPhi(w.raw()) / alpha_y);
        return (std::sqrt(1 + alpha2 * tan2Theta) - 1) / 2;
    }

    Float G1(const Direction& w) const {
        return 1 / (1 + Lambda(w));
    }

    Float G(const Direction& wo, const Direction& wi) const {
        return 1 / (1 + Lambda(wo) + Lambda(wi));
    }

    Float D(const Direction& w, const Direction& wm) const {
        return G1(w) / AbsCosTheta(w.raw()) * D(wm) * absDot(w, wm);
    }

    Float PDF(const Direction& w, const Direction& wm) const {
        return D(w, wm);
    }

    Direction Sample_wm(const Direction& w, glm::vec2 u) const {
        Direction wh = Direction::fromRaw(Normalize(glm::vec3(alpha_x * w.x(), alpha_y * w.y(), w.z())));
        if(wh.z() < 0.0f)
            wh.flipZ();
        Direction T1 = (wh.z() < 0.99999f) ? Direction::fromRaw(Normalize(cross(glm::vec3(1, 0, 0), wh.raw()))) :
                                             Direction::fromRaw(glm::vec3(1, 0, 0));
        Direction T2 = cross(wh, T1);
        glm::vec2 p = sampleUniformDisk(u);
        Float h = std::sqrt(1 - sqr(p.x));
        p.y = Lerp((1 + wh.z()) / 2, h, p.y);
        Float pz = std::sqrt(std::max<Float>(0.0f, 1 - sqr(p.x) - sqr(p.y)));
        glm::vec3 nh = p.x * T1.raw() + p.y * T2.raw() + pz * wh.raw();
        return Direction::fromRaw(Normalize(glm::vec3(alpha_x * nh.x, alpha_y * nh.y, std::max<Float>(epsilon, nh.z))));
    }

    static Float RoughnessToAlpha(Float roughness) {
        return std::sqrt(roughness);
    }

    void Regularize() {
        if(alpha_x < 0.3f)
            alpha_x = Clamp(2 * alpha_x, 0.1f, 0.3f);
        if(alpha_y < 0.3f)
            alpha_y = Clamp(2 * alpha_y, 0.1f, 0.3f);
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
    explicit DielectricBxDF(Float Eta, Float uRoughness, Float vRoughness, bool remapRoughness) {
        eta = Eta;
        // Float urough = texEval(uRoughness, ctx), vrough = texEval(vRoughness, ctx);
        Float urough = uRoughness, vrough = vRoughness;
        if(remapRoughness) {
            urough = TrowbridgeReitzDistribution<Setting>::RoughnessToAlpha(urough);
            vrough = TrowbridgeReitzDistribution<Setting>::RoughnessToAlpha(vrough);
        }
        mfDistrib = TrowbridgeReitzDistribution<Setting>(urough, vrough);
    }

    [[nodiscard]] BxDFPart part() const noexcept override {
        return BxDFPart::Glossy | BxDFPart::Reflection | BxDFPart::Transmission;  // TODO: FIXME
    }

    Rational<Spectrum> evaluate(const Direction& wo, const Direction& wi, TransportMode transportMode) const noexcept override {
        if(eta == 1 || mfDistrib.EffectivelySmooth())
            return Rational<Spectrum>::zero();
        Float cosTheta_o = CosTheta(wo.raw());
        Float cosTheta_i = CosTheta(wi.raw());
        bool reflect = cosTheta_i * cosTheta_o > 0;
        Float etap = 1.0f;
        if(!reflect)
            etap = cosTheta_o > 0 ? eta : (1 / eta);
        Direction wm = Direction::fromRaw(wi.raw() * etap + wo.raw());
        if(cosTheta_i == 0 || cosTheta_o == 0 || sqr(wm.x()) + sqr(wm.y()) + sqr(wm.z()) == 0)
            return Rational<Spectrum>::zero();
        wm = Direction::fromRaw(FaceForward(Normalize(wm.raw()), glm::vec3(0, 0, 1)));

        if(dot(wm, wi) * cosTheta_i < 0 || dot(wm, wo) * cosTheta_o < 0)
            return Rational<Spectrum>::zero();

        Float F = FrDielectric(dot(wo, wm), eta);

        if(reflect) {
            return mfDistrib.D(wm) * mfDistrib.G(wo, wi) * F / std::abs(4 * cosTheta_i * cosTheta_o) * Rational<Spectrum>::identity();
        } else {
            Float denom = sqr(dot(wi, wm) + dot(wo, wm) / etap) * cosTheta_i * cosTheta_o;
            Float ft = mfDistrib.D(wm) * (1 - F) * mfDistrib.G(wo, wi) * std::abs(dot(wi, wm) * dot(wo, wm) / denom);
            if(transportMode == TransportMode::Radiance)
                ft /= sqr(etap);
            return ft * Rational<Spectrum>::identity();
        }
    }

    BSDFSample sample(SampleProvider& sampler, const Direction& wo, const TransportMode transportMode,
                      const BxDFDirection sampleDirection) const noexcept override {
        if(eta == 1 || mfDistrib.EffectivelySmooth()) {
            Float R = FrDielectric(CosTheta(wo.raw()), eta);
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
                Float ft = R / AbsCosTheta(wi.raw());
                return { wi, importanceSampled<PdfType::BSDF>(ft * Rational<Spectrum>::identity()),
                         InversePdfValue::fromRaw((pr + pt) / pr), BxDFPart::SpecularReflection };
            } else {
                Float etap;
                glm::vec3 wiraw;
                bool valid = Refract(wo.raw(), glm::vec3(0, 0, 1), eta, &etap, &wiraw);
                Direction wi = Direction::fromRaw(wiraw);
                if(!valid)
                    return BSDFSample::invalid();

                Float ft = T / AbsCosTheta(wi.raw());
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
                Direction wi = Direction::fromRaw(Reflect(wo.raw(), wm.raw()));
                if(!sameHemisphere(wo, wi))
                    return BSDFSample::invalid();

                pdf = mfDistrib.PDF(wo, wm) / std::max<Float>(epsilon, 4 * absDot(wo, wm)) * pr / (pr + pt);
                Float ft = mfDistrib.D(wm) * mfDistrib.G(wo, wi) * R / (4 * CosTheta(wi.raw()) * CosTheta(wo.raw()));
                return { wi, importanceSampled<PdfType::BSDF>(ft * Rational<Spectrum>::identity()), InversePdfValue::fromRaw(1 / pdf),
                         BxDFPart::GlossyReflection };
            } else {
                Float etap;
                glm::vec3 wiraw;
                bool tir = !Refract(wo.raw(), wm.raw(), eta, &etap, &wiraw);
                Direction wi = Direction::fromRaw(wiraw);
                if(sameHemisphere(wo, wi) || wi.z() == 0 || tir)
                    return BSDFSample::invalid();

                Float denom = sqr(dot(wi, wm) + dot(wo, wm) / etap);
                Float dwm_dwi = absDot(wi, wm) / denom;
                pdf = mfDistrib.PDF(wo, wm) * dwm_dwi * pt / (pr + pt);

                Float ft = T * mfDistrib.D(wm) * mfDistrib.G(wo, wi) *
                    std::abs(dot(wi, wm) * dot(wo, wm) / (CosTheta(wi.raw()) * CosTheta(wo.raw()) * denom));
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
        Float cosTheta_o = CosTheta(wo.raw()), cosTheta_i = CosTheta(wi.raw());
        bool reflect = cosTheta_i * cosTheta_o > 0;
        Float etap = 1;
        if(!reflect)
            etap = cosTheta_o > 0 ? eta : (1 / eta);
        Direction wm = Direction::fromRaw(wi.raw() * etap + wo.raw());
        if(cosTheta_i == 0 || cosTheta_o == 0 || sqr(wm.x()) + sqr(wm.y()) + sqr(wm.z()) == 0)
            return InversePdfValue::invalid();
        wm = Direction::fromRaw(FaceForward(Normalize(wm.raw()), glm::vec3(0, 0, 1)));

        if(dot(wm, wi) * cosTheta_i < 0 || dot(wm, wo) * cosTheta_o < 0)
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

        return InversePdfValue::fromRaw(1 / pdf);
    }
};

PIPER_NAMESPACE_END
