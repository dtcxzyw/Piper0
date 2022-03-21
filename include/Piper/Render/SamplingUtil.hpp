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
#include <Piper/Render/Math.hpp>
#include <Piper/Render/Radiometry.hpp>
#include <Piper/Render/Transform.hpp>
#include <complex>

PIPER_NAMESPACE_BEGIN

inline glm::vec2 sampleUniformDisk(const glm::vec2 u) noexcept {
    const auto angle = u.x * twoPi;
    const auto rad = std::sqrt(u.y);
    return rad * glm::vec2{ std::cos(angle), std::sin(angle) };
}

inline glm::vec2 sampleConcentricDisk(glm::vec2 u) noexcept {
    u = 2.0f * u - 1.0f;
    const auto absU = glm::abs(u);
    if(std::fmin(absU.x, absU.y) < epsilon)
        return glm::vec2{ 0.0, 0.0 };
    const auto radius = (absU.x > absU.y ? u.x : u.y);
    const auto theta = (absU.x > absU.y ? quarterPi * (u.y / u.x) : halfPi - quarterPi * (u.x / u.y));
    return glm::vec2{ std::cos(theta), std::sin(theta) } * radius;
}

inline Direction<FrameOfReference::Shading> sampleCosineHemisphere(const glm::vec2 u) noexcept {
    const auto coord = sampleConcentricDisk(u);
    const auto z = safeSqrt(1.0f - glm::dot(coord, coord));
    return Direction<FrameOfReference::Shading>::fromRaw({ coord, z });
}

inline uint32_t select(const Float* cdf, const Float* pdf, const uint32_t size, Float& u) noexcept {
    uint32_t l = 0, r = size;  //[l,r)
    while(l < r) {
        if(const auto mid = (l + r) >> 1; u >= cdf[mid]) {
            l = mid + 1;
        } else
            r = mid;
    }
    u = (u - cdf[r]) / pdf[r];
    return r;
}

inline Float calcGeometrySamplePdf(const Distance distance, const Direction<FrameOfReference::World>& wi,
                                   const Direction<FrameOfReference::World>& n, const Area area) noexcept {
    return sqr(distance) / (area * std::fabs(dot(n, wi)));
}

inline auto cosineHemispherePdf(const Float cosTheta) noexcept {
    return cosTheta > epsilon ? InversePdf<PdfType::BSDF>::fromRaw(rcp(cosTheta) * pi) : InversePdf<PdfType::BSDF>::invalid();
}

template <PdfType T>
constexpr auto uniformSpherePdf() noexcept {
    return InversePdf<T>::fromRaw(fourPi);
}

template <FrameOfReference F>
auto sampleUniformSphere(const glm::vec2 u) noexcept {
    const auto z = 1.0f - 2.0f * u.x;
    const auto r = safeSqrt(1.0f - sqr(z));
    const auto phi = twoPi * u.y;
    return Direction<F>::fromRaw(glm::vec3{ r * std::cos(phi), r * std::sin(phi), z });
}

PIPER_NAMESPACE_END
