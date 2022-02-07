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
#include <Piper/Render/ColorSpace.hpp>
#include <glm/glm.hpp>

PIPER_NAMESPACE_BEGIN

using Float = float;

enum class SpectrumType : uint16_t { NotSpectrum, Mono, LinearRGB, Spectral };

class RGBSpectrum;

template <typename T>
constexpr RGBSpectrum toRGB(const T&) noexcept;
template <typename T>
Float luminance(const T&) noexcept;
template <typename T>
constexpr SpectrumType spectrumType() noexcept;
template <typename T>
constexpr T one() noexcept;
template <typename T>
constexpr T zero() noexcept;

template <typename T>
concept SpectrumLike = requires(T x, Float y) {
    { x + x } -> std::convertible_to<T>;
    { x* x } -> std::convertible_to<T>;
    { x* y } -> std::convertible_to<T>;
    { toRGB(x) } -> std::convertible_to<RGBSpectrum>;
    { luminance(x) } -> std::convertible_to<Float>;  // Y of CIE 1931 XYZ
    { spectrumType<T>() } -> std::convertible_to<SpectrumType>;
    { one<T>() } -> std::convertible_to<T>;
    { zero<T>() } -> std::convertible_to<T>;
};

// ITU-R Rec. BT.709 linear RGB, used by pbrt/mitsuba
// Please refer to section "1 opto-electronic conversion" of https://www.itu.int/rec/R-REC-BT.709-6-201506-I/en
class RGBSpectrum final {
    glm::vec3 mVec;

    explicit constexpr RGBSpectrum(const glm::vec3& x) noexcept : mVec{ x } {}

public:
    // Please refer to https://pbr-book.org/3ed-2018/Color_and_Radiometry/The_SampledSpectrum_Class
    static constexpr glm::mat3 matXYZ2RGB = {
        3.240479f,  -1.537150f, -0.498535f,  //
        -0.969256f, 1.875991f,  0.041556f,   //
        0.055648f,  -0.204043f, 1.057311f    //
    };
    static constexpr glm::mat3 matRGB2XYZ = {
        0.412453f, 0.357580f, 0.180423f,  //
        0.212671f, 0.715160f, 0.072169f,  //
        0.019334f, 0.119193f, 0.950227f   //
    };

    constexpr explicit RGBSpectrum(const Float x) noexcept : mVec{ x } {}

    static RGBSpectrum fromRaw(const glm::vec3& x) noexcept {
        return RGBSpectrum{ x };
    }

    constexpr RGBSpectrum operator+(const RGBSpectrum& rhs) const noexcept {
        return RGBSpectrum{ mVec + rhs.mVec };
    }
    constexpr RGBSpectrum operator*(const RGBSpectrum& rhs) const noexcept {
        return RGBSpectrum{ mVec * rhs.mVec };
    }
    constexpr RGBSpectrum operator*(const Float rhs) const noexcept {
        return RGBSpectrum{ mVec * rhs };
    }
    friend Float luminance(const RGBSpectrum& x) noexcept;
};
constexpr const RGBSpectrum& toRGB(const RGBSpectrum& x) noexcept {
    return x;
}
template <>
constexpr SpectrumType spectrumType<RGBSpectrum>() noexcept {
    return SpectrumType::LinearRGB;
}
template <>
constexpr RGBSpectrum zero<RGBSpectrum>() noexcept {
    return RGBSpectrum{ 0.0f };
}

template <>
constexpr RGBSpectrum one<RGBSpectrum>() noexcept {
    return RGBSpectrum{ 1.0f };
}

using MonoSpectrum = float;
constexpr RGBSpectrum toRGB(const MonoSpectrum x) noexcept {
    return RGBSpectrum{ x };
}

constexpr Float luminance(const MonoSpectrum x) noexcept {
    return x;
}

template <>
constexpr MonoSpectrum zero<MonoSpectrum>() noexcept {
    return 0.0f;
}

template <>
constexpr MonoSpectrum one<MonoSpectrum>() noexcept {
    return 1.0f;
}

template <>
constexpr SpectrumType spectrumType<MonoSpectrum>() noexcept {
    return SpectrumType::Mono;
}

// TODO: single wavelength sampling

constexpr auto sampleWavelengthMin = 360.0f;
constexpr auto sampleWavelengthMax = 830.0f;

class SpectralSpectrum final {
    static constexpr auto nSamples = 4;

    glm::vec<nSamples, Float> mVec;

    explicit constexpr SpectralSpectrum(const glm::vec<nSamples, Float>& x) noexcept : mVec{ x } {}

public:
    constexpr explicit SpectralSpectrum(const Float x) noexcept : mVec{ x } {}
    constexpr SpectralSpectrum operator+(const SpectralSpectrum& rhs) const noexcept {
        return SpectralSpectrum{ mVec + rhs.mVec };
    }
    constexpr SpectralSpectrum operator*(const SpectralSpectrum& rhs) const noexcept {
        return SpectralSpectrum{ mVec * rhs.mVec };
    }
    constexpr SpectralSpectrum operator*(const Float rhs) const noexcept {
        return SpectralSpectrum{ mVec * rhs };
    }
    friend Float luminance(const SpectralSpectrum& x) noexcept;
    friend RGBSpectrum toRGB(const SpectralSpectrum& x) noexcept;
};

template <>
constexpr SpectrumType spectrumType<SpectralSpectrum>() noexcept {
    return SpectrumType::Spectral;
}

template <>
constexpr SpectralSpectrum zero<SpectralSpectrum>() noexcept {
    return SpectralSpectrum{ 0.0f };
}

template <>
constexpr SpectralSpectrum one<SpectralSpectrum>() noexcept {
    return SpectralSpectrum{ 1.0f };
}

PIPER_NAMESPACE_END
