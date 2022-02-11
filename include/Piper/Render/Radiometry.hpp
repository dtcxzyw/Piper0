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

enum class PdfType { None = 0, BSDF = 1 << 0, Light = 1 << 1, LightSampler = 1 << 2, Texture = 1 << 3, All = (1 << 4) - 1 };
PIPER_BIT_ENUM(PdfType)

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

template <PdfType A, PdfType B>
requires((A & B) == PdfType::None) constexpr auto operator*(const InversePdf<A>& lhs, const InversePdf<B>& rhs) noexcept {
    return InversePdf<A | B>::fromRaw(lhs.raw() * rhs.raw());
}

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

// Dimensionless
template <SpectrumLike Spectrum, PdfType Pdf = PdfType::None>
class Rational final {
    PIPER_GUARD_BASE(Rational, Spectrum)
    PIPER_GUARD_BASE_OP(Rational)
};

template <SpectrumLike Spectrum, PdfType A, PdfType B>
requires((A & B) == PdfType::None) constexpr auto operator*(const Rational<Spectrum, A>& lhs, const Rational<Spectrum, B>& rhs) noexcept {
    return Rational<Spectrum, A | B>::fromRaw(lhs.raw() * rhs.raw());
}
template <typename Spectrum, PdfType A, PdfType B>
requires(match(A, B)) constexpr auto operator*(const Rational<Spectrum, A>& lhs, const InversePdf<B> rhs) noexcept {
    return Rational<Spectrum, A ^ B>::fromRaw(lhs.raw() * rhs.raw());
}
template <PdfType T, typename Spectrum>
constexpr auto importanceSampled(const Rational<Spectrum>& x) noexcept {
    return Rational<Spectrum, T>::fromRaw(x.raw());
}

#define PIPER_GUARD_IMPORTANCE_OP(TYPE)                                                                                                    \
    template <typename Spectrum, PdfType A, PdfType B>                                                                                     \
    requires((A & B) == PdfType::None) constexpr auto operator*(const TYPE<Spectrum, A>& lhs, const Rational<Spectrum, B>& rhs) noexcept { \
        return TYPE<Spectrum, A | B>::fromRaw(lhs.raw() * rhs.raw());                                                                      \
    }                                                                                                                                      \
    template <typename Spectrum, PdfType A, PdfType B>                                                                                     \
    requires((A & B) == PdfType::None) constexpr auto operator*(const Rational<Spectrum, A>& lhs, const TYPE<Spectrum, B>& rhs) noexcept { \
        return TYPE<Spectrum, A | B>::fromRaw(lhs.raw() * rhs.raw());                                                                      \
    }                                                                                                                                      \
    template <typename Spectrum, PdfType A, PdfType B>                                                                                     \
    requires(match(A, B)) constexpr auto operator*(const TYPE<Spectrum, A>& lhs, const InversePdf<B> rhs) noexcept {                       \
        return TYPE<Spectrum, A ^ B>::fromRaw(lhs.raw() * rhs.raw());                                                                      \
    }                                                                                                                                      \
    template <PdfType T, typename Spectrum>                                                                                                \
    constexpr auto importanceSampled(const TYPE<Spectrum>& x) noexcept {                                                                   \
        return TYPE<Spectrum, T>::fromRaw(x.raw());                                                                                        \
    }

// W/(m^2)
template <SpectrumLike Spectrum, PdfType Pdf = PdfType::None>
class Irradiance final {
    PIPER_GUARD_BASE(Irradiance, Spectrum)
    PIPER_GUARD_BASE_OP(Irradiance)
};

PIPER_GUARD_IMPORTANCE_OP(Irradiance)

// W/(sr*m^2)
template <SpectrumLike Spectrum, PdfType Pdf = PdfType::None>
class Radiance final {
    PIPER_GUARD_BASE(Radiance, Spectrum)
    PIPER_GUARD_BASE_OP(Radiance)

    static Radiance zero() noexcept {
        return Radiance{ Piper::zero<Spectrum>() };
    }

    template <PdfType NewPdf>
    requires((Pdf & NewPdf) == PdfType::None) [[nodiscard]] constexpr auto importanceSampled() const noexcept {
        return Radiance<Spectrum, Pdf | NewPdf>::fromRaw(mValue);
    }
};
PIPER_GUARD_IMPORTANCE_OP(Radiance)

PIPER_GUARD_MULTIPLY_COMMUTATIVE(template <SpectrumLike Spectrum>, Radiance<Spectrum>, SolidAngle, Irradiance<Spectrum>)

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

template <SpectrumLike Spectrum>
using Flux = Power<Spectrum>;

// J
template <SpectrumLike Spectrum>
class Energy final {
    PIPER_GUARD_BASE(Energy, Spectrum)
    PIPER_GUARD_BASE_OP(Energy)
};

PIPER_GUARD_MULTIPLY_COMMUTATIVE(template <SpectrumLike Spectrum>, Power<Spectrum>, Time, Energy<Spectrum>)

PIPER_NAMESPACE_END
