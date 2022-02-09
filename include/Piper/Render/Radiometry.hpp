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

// Type guards for unit checking/importance sampling

#pragma once
#include <Piper/Render/Spectrum.hpp>
#include <Piper/Render/Transform.hpp>

PIPER_NAMESPACE_BEGIN

enum class PdfType { BSDF = 1 << 0, Light = 1 << 1, LightSampler = 1 << 2 };

template <PdfType T>
class InversePdf final {
    PIPER_GUARD_BASE(InversePdf, Float)

    static constexpr InversePdf invalid() noexcept {
        return InversePdf{ 0.0f };
    }

    [[nodiscard]] bool valid() const noexcept {
        return mValue != 0.0f;
    }
};

class SolidAngle final {
    PIPER_GUARD_BASE(SolidAngle, Float)

    static constexpr SolidAngle fullSphere() noexcept {
        return SolidAngle{ fourPi };
    }
    static constexpr SolidAngle semiSphere() noexcept {
        return SolidAngle{ twoPi };
    }
};

class Area final {
    PIPER_GUARD_BASE(Area, Float)
    PIPER_GUARD_BASE_OP(Area)

    constexpr Float operator/(const Area rhs) const noexcept {
        return mValue / rhs.raw();
    }
};

PIPER_GUARD_MULTIPLY(Distance, Distance, Area)

class Time final {
    PIPER_GUARD_BASE(Time, Float)
    PIPER_GUARD_BASE_OP(Time)
};

// W/(m^2)
template <SpectrumLike Spectrum, PdfType... Pdfs>
class Irradiance final {
    PIPER_GUARD_BASE(Irradiance, Spectrum)
    PIPER_GUARD_BASE_OP(Irradiance)
};

// W/(sr*m^2)
template <SpectrumLike Spectrum, PdfType... Pdfs>
class Radiance final {
    PIPER_GUARD_BASE(Radiance, Spectrum)
    PIPER_GUARD_BASE_OP(Radiance)

    static Radiance zero() noexcept {
        return Radiance{ Piper::zero<Spectrum>() };
    }

    template <PdfType... NewPdfs>
    [[nodiscard]] constexpr auto importanceSampled() const noexcept {
        return Radiance<Spectrum, Pdfs..., NewPdfs...>::fromRaw(mValue);
    }
};

// W/sr
template <SpectrumLike Spectrum>
class Intensity final {
    PIPER_GUARD_BASE(Intensity, Spectrum)
    PIPER_GUARD_BASE_OP(Intensity)

    [[nodiscard]] Radiance<Spectrum> toRadiance(const DistanceSquare distanceSquare) const noexcept {
        return Radiance<Spectrum>::fromRaw(mValue * rcp(distanceSquare.raw()));
    }
};

PIPER_GUARD_MULTIPLY_COMMUTATIVE(template <SpectrumLike Spectrum>, Radiance<Spectrum>, DistanceSquare, Intensity<Spectrum>)

// W
template <SpectrumLike Spectrum>
class Power final {
    PIPER_GUARD_BASE(Power, Spectrum)
    PIPER_GUARD_BASE_OP(Power)

    Intensity<Spectrum> operator/(const SolidAngle rhs) const noexcept {
        return Intensity<Spectrum>::fromRaw(mValue * rcp(rhs.raw()));
    }

    [[nodiscard]] Float scalar() const noexcept {
        return luminance(mValue);
    }
};

PIPER_GUARD_MULTIPLY_COMMUTATIVE(template <SpectrumLike Spectrum>, Intensity<Spectrum>, SolidAngle, Power<Spectrum>)
PIPER_GUARD_MULTIPLY_COMMUTATIVE(template <SpectrumLike Spectrum>, Irradiance<Spectrum>, Area, Power<Spectrum>)

// J
template <SpectrumLike Spectrum>
class Energy final {
    PIPER_GUARD_BASE(Energy, Spectrum)
    PIPER_GUARD_BASE_OP(Energy)
};

PIPER_GUARD_MULTIPLY_COMMUTATIVE(template <SpectrumLike Spectrum>, Power<Spectrum>, Time, Energy<Spectrum>)

PIPER_NAMESPACE_END
