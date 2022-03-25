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
#include <Piper/Render/Spectrum.hpp>
#include <Piper/Render/Transform.hpp>

PIPER_NAMESPACE_BEGIN

inline Float fresnelDielectric(Float cosThetaI, Float eta) noexcept {
    cosThetaI = std::clamp(cosThetaI, -1.0f, 1.0f);
    if(cosThetaI < 0.0f) {
        eta = rcp(eta);
        cosThetaI = -cosThetaI;
    }
    const auto sin2ThetaI = 1.0f - sqr(cosThetaI);
    const auto sin2ThetaT = sin2ThetaI / sqr(eta);

    // Total internal reflection
    if(sin2ThetaT >= 1.0f)
        return 1.0f;
    const auto cosThetaT = safeSqrt(1.0f - sin2ThetaT);

    const auto parallel = (eta * cosThetaI - cosThetaT) / (eta * cosThetaI + cosThetaT);
    const auto perpendicular = (cosThetaI - eta * cosThetaT) / (cosThetaI + eta * cosThetaT);
    return (sqr(parallel) + sqr(perpendicular)) * 0.5f;
}

template <typename Setting>
class TrowbridgeReitzDistribution {
    PIPER_IMPORT_SETTINGS();
    PIPER_IMPORT_SHADING();

    Float mAlphaX, mAlphaY;

public:
    TrowbridgeReitzDistribution(const Float alphaX, const Float alphaY) : mAlphaX(alphaX), mAlphaY(alphaY) {}

    [[nodiscard]] Float evalD(const Direction& wm) const noexcept {
        if(isZero(cosTheta(wm)))
            return 0.0f;
        const auto tan2T = tan2Theta(wm);
        const auto cos4T = sqr(cos2Theta(wm));
        const auto e = tan2T * (sqr(cosPhi(wm) / mAlphaX) + sqr(sinPhi(wm) / mAlphaY));  // NOLINT(clang-diagnostic-shadow)
        return invPi / (mAlphaX * mAlphaY * cos4T * sqr(1 + e));
    }

    [[nodiscard]] bool effectivelySmooth() const noexcept {
        return std::fmax(mAlphaX, mAlphaY) < 1e-3f;
    }

    [[nodiscard]] Float lambda(const Direction& w) const noexcept {
        if(isZero(cosTheta(w)))
            return 0.0f;
        const auto tan2T = tan2Theta(w);
        const auto alpha2 = sqr(cosPhi(w) * mAlphaX) + sqr(sinPhi(w) * mAlphaY);
        return (std::sqrt(std::fma(alpha2, tan2T, 1.0f)) - 1) * 0.5f;
    }

    [[nodiscard]] Float evalG1(const Direction& w) const noexcept {
        return rcp(1 + lambda(w));
    }

    [[nodiscard]] Float evalG(const Direction& wo, const Direction& wi) const noexcept {
        return rcp(1 + lambda(wo) + lambda(wi));
    }

    [[nodiscard]] Float evalD(const Direction& w, const Direction& wm) const {
        if(isZero(cosTheta(w)))
            return 0.0f;
        return evalG1(w) / absCosTheta(w) * evalD(wm) * absDot(w, wm);
    }

    [[nodiscard]] Float pdf(const Direction& w, const Direction& wm) const noexcept {
        return evalD(w, wm);
    }

    [[nodiscard]] Direction sampleWm(const Direction& w, const glm::vec2 u) const noexcept {
        auto wh = Direction::fromRaw(glm::normalize(glm::vec3{ mAlphaX * w.x(), mAlphaY * w.y(), w.z() }));
        if(wh.z() < 0.0f)
            wh = -wh;

        const auto tangent = (wh.z() < (1.0f - epsilon)) ? Direction::fromRaw(glm::normalize(cross(glm::vec3{ 0, 0, 1 }, wh.raw()))) :
                                                           Direction::fromRaw({ 1, 0, 0 });
        const auto biTangent = cross(wh, tangent);
        auto p = sampleUniformDisk(u);
        const auto h = std::sqrt(1.0f - sqr(p.x));
        p.y = glm::mix(h, p.y, (1.0f + wh.z()) * 0.5f);
        const auto pz = safeSqrt(1.0f - glm::length2(p));
        const auto nh = p.x * tangent.raw() + p.y * biTangent.raw() + pz * wh.raw();
        return Direction::fromRaw(glm::normalize(glm::vec3{ mAlphaX * nh.x, mAlphaY * nh.y, std::fmax(1e-6f, nh.z) }));
    }

    static Float roughnessToAlpha(const Float roughness) noexcept {
        return std::sqrt(roughness);
    }

    //???
    void regularize() {
        if(mAlphaX < 0.3f)
            mAlphaX = std::clamp(2 * mAlphaX, 0.1f, 0.3f);
        if(mAlphaY < 0.3f)
            mAlphaY = std::clamp(2 * mAlphaY, 0.1f, 0.3f);
    }
};

PIPER_NAMESPACE_END
