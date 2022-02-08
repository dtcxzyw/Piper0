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

PIPER_NAMESPACE_BEGIN

enum class MISType { BSDF, Light };

template <MISType T>
class Pdf final {
    Float mValue;

    constexpr explicit Pdf(const Float x) noexcept : mValue{ x } {}

public:
    [[nodiscard]] Float raw() const noexcept {
        return mValue;
    }

    static constexpr Pdf fromRaw(const Float x) noexcept {
        return Pdf{ x };
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
    [[nodiscard]] Float raw() const noexcept {
        return mValue;
    }
    static constexpr Area fromRaw(const Float x) noexcept {
        return Area{ x };
    }
};

// W/(m^2)
template <SpectrumLike Spectrum>
class Irradiance final {
    Spectrum mSpectrum;

public:
};

// W/(sr*m^2)
template <SpectrumLike Spectrum>
class Radiance final {
    Spectrum mSpectrum;

public:
};

// W/sr
template <SpectrumLike Spectrum>
class Intensity final {
    Spectrum mSpectrum;

public:
};

PIPER_NAMESPACE_END
