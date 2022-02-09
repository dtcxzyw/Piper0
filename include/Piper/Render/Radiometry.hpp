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
    Float mValue;

    constexpr explicit InversePdf(const Float x) noexcept : mValue{ x } {}

public:
    [[nodiscard]] Float raw() const noexcept {
        return mValue;
    }

    static constexpr InversePdf fromRaw(const Float x) noexcept {
        return InversePdf{ x };
    }

    static constexpr InversePdf invalid() noexcept {
        return InversePdf{ 0.0f };
    }

    [[nodiscard]] bool valid() const noexcept {
        return mValue != 0.0f;
    }
};

class SolidAngle final {
    Float mValue;

    constexpr explicit SolidAngle(const Float x) noexcept : mValue{ x } {}

public:
    [[nodiscard]] Float raw() const noexcept {
        return mValue;
    }
    static constexpr SolidAngle fromRaw(const Float x) noexcept {
        return SolidAngle{ x };
    }
    static constexpr SolidAngle fullSphere() noexcept {
        return SolidAngle{ fourPi };
    }
    static constexpr SolidAngle semiSphere() noexcept {
        return SolidAngle{ twoPi };
    }
};

class Area final {
    Float mValue;

    constexpr explicit Area(const Float x) noexcept : mValue{ x } {}

public:
    [[nodiscard]] constexpr Float raw() const noexcept {
        return mValue;
    }
    static constexpr Area fromRaw(const Float x) noexcept {
        return Area{ x };
    }
    constexpr Area operator*(const Float rhs) const noexcept {
        return Area{ mValue * rhs };
    }
    constexpr Float operator/(const Area rhs) const noexcept {
        return mValue / rhs.raw();
    }
};

constexpr Area operator*(const Distance lhs, const Distance rhs) noexcept {
    return Area::fromRaw(lhs.raw() * rhs.raw());
}

class Time final {
    Float mValue;

    constexpr explicit Time(const Float x) noexcept : mValue{ x } {}

public:
    [[nodiscard]] Float raw() const noexcept {
        return mValue;
    }
    static constexpr Time fromRaw(const Float x) noexcept {
        return Time{ x };
    }
};

// W/(m^2)
template <SpectrumLike Spectrum, PdfType... Pdfs>
class Irradiance final {
    Spectrum mSpectrum;

    constexpr explicit Irradiance(const Spectrum& x) noexcept : mSpectrum{ x } {}

public:
    [[nodiscard]] const Spectrum& raw() const noexcept {
        return mSpectrum;
    }

    static Irradiance fromRaw(const Spectrum& x) noexcept {
        return Irradiance{ x };
    }
};

// W/(sr*m^2)
template <SpectrumLike Spectrum, PdfType... Pdfs>
class Radiance final {
    Spectrum mSpectrum;

    constexpr explicit Radiance(const Spectrum& x) noexcept : mSpectrum{ x } {}

public:
    [[nodiscard]] const Spectrum& raw() const noexcept {
        return mSpectrum;
    }

    static Radiance fromRaw(const Spectrum& x) noexcept {
        return Radiance{ x };
    }

    static Radiance zero() noexcept {
        return Radiance{ Piper::zero<Spectrum>() };
    }

    template <PdfType... NewPdfs>
    [[nodiscard]] constexpr auto importanceSampled() const noexcept {
        return Radiance<Spectrum, Pdfs..., NewPdfs...>::fromRaw(mSpectrum);
    }
};

// W/sr
template <SpectrumLike Spectrum>
class Intensity final {
    Spectrum mSpectrum;

    constexpr explicit Intensity(const Spectrum& x) noexcept : mSpectrum{ x } {}

public:
    [[nodiscard]] const Spectrum& raw() const noexcept {
        return mSpectrum;
    }

    static Intensity fromRaw(const Spectrum& x) noexcept {
        return Intensity{ x };
    }

    [[nodiscard]] Radiance<Spectrum> toRadiance(const DistanceSquare distanceSquare) const noexcept {
        return Radiance<Spectrum>::fromRaw(mSpectrum * rcp(distanceSquare.raw()));
    }
};

// W
template <SpectrumLike Spectrum>
class Power final {
    Spectrum mSpectrum;

    constexpr explicit Power(const Spectrum& x) noexcept : mSpectrum{ x } {}

public:
    [[nodiscard]] const Spectrum& raw() const noexcept {
        return mSpectrum;
    }
    static Power fromRaw(const Spectrum& x) noexcept {
        return Power{ x };
    }

    Intensity<Spectrum> operator/(const SolidAngle rhs) const noexcept {
        return Intensity<Spectrum>::fromRaw(mSpectrum * rcp(rhs.raw()));
    }

    Power operator+(const Power& rhs) const noexcept {
        return Power{ mSpectrum + rhs.mSpectrum };
    }

    [[nodiscard]] Float scalar() const noexcept {
        return luminance(mSpectrum);
    }
};

template <SpectrumLike Spectrum>
Power<Spectrum> operator*(const Irradiance<Spectrum>& lhs, const Area rhs) noexcept {
    return Power{ lhs.raw() * rhs.raw() };
}

template <SpectrumLike Spectrum>
auto operator*(const Intensity<Spectrum>& lhs, const SolidAngle rhs) noexcept {
    return Power<Spectrum>::fromRaw(lhs.raw() * rhs.raw());
}

// J
template <SpectrumLike Spectrum>
class Energy final {
    Spectrum mSpectrum;

    constexpr explicit Energy(const Spectrum& x) noexcept : mSpectrum{ x } {}

public:
    friend Energy operator*(const Power<Spectrum>& lhs, const Time rhs) noexcept {
        return Energy{ lhs.raw() * rhs.raw() };
    }
};

PIPER_NAMESPACE_END
