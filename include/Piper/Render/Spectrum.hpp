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

PIPER_NAMESPACE_BEGIN

enum class SpectrumType : uint16_t { Mono, LinearRGB };

class RGBSpectrum;

template <typename T>
struct WavelengthType final {
    using Type = std::monostate;
};

template <typename T>
constexpr RGBSpectrum toRGB(const T&, const typename WavelengthType<T>::Type&) noexcept;
template <typename T>
Float luminance(const T&, const typename WavelengthType<T>::Type&) noexcept;
template <typename T>
constexpr SpectrumType spectrumType() noexcept = delete;
template <typename T>
constexpr T identity() noexcept = delete;
template <typename T>
constexpr T zero() noexcept = delete;
template <typename T>
constexpr Float maxComponentValue(const T&) noexcept;

template <typename T>
concept SpectrumLike = requires(const T& x, Float y, const typename WavelengthType<T>::Type& w) {
    { x + x } -> std::convertible_to<T>;
    { x* x } -> std::convertible_to<T>;
    { x* y } -> std::convertible_to<T>;
    { toRGB(x, w) } -> std::convertible_to<RGBSpectrum>;
    { luminance(x, w) } -> std::convertible_to<Float>;  // Y of CIE 1931 XYZ
    { spectrumType<T>() } -> std::convertible_to<SpectrumType>;
    { identity<T>() } -> std::convertible_to<T>;
    { zero<T>() } -> std::convertible_to<T>;
    { maxComponentValue<T>(x) } -> std::convertible_to<Float>;
    typename WavelengthType<T>::Type;
};

// ITU-R Rec. BT.709 linear RGB, used by pbrt/mitsuba
// Please refer to section "1 opto-electronic conversion" of https://www.itu.int/rec/R-REC-BT.709-6-201506-I/en
static constexpr const char* nameOfStandardLinearRGB = "lin_rec709";  // TODO: check again

class RGBSpectrum final {
    PIPER_GUARD_BASE(RGBSpectrum, glm::vec3)
    PIPER_GUARD_BASE_OP(RGBSpectrum)

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

    static constexpr RGBSpectrum fromScalar(const Float x) noexcept {
        return RGBSpectrum{ glm::vec3{ x } };
    }
    PIPER_GUARD_ELEMENT_VISE_MULTIPLY(RGBSpectrum)
};

Float luminance(const RGBSpectrum& x, const std::monostate&) noexcept;

inline Float maxComponentValue(const RGBSpectrum& x) noexcept {
    const auto vec = x.raw();
    return std::fmax(vec.x, std::fmax(vec.y, vec.z));
}

constexpr const RGBSpectrum& toRGB(const RGBSpectrum& x, const std::monostate&) noexcept {
    return x;
}

template <>
constexpr SpectrumType spectrumType<RGBSpectrum>() noexcept {
    return SpectrumType::LinearRGB;
}
template <>
constexpr RGBSpectrum zero<RGBSpectrum>() noexcept {
    return RGBSpectrum::fromScalar(0.0f);
}

template <>
constexpr RGBSpectrum identity<RGBSpectrum>() noexcept {
    return RGBSpectrum::fromScalar(1.0f);
}

using MonoSpectrum = Float;

constexpr RGBSpectrum toRGB(const MonoSpectrum x, const std::monostate&) noexcept {
    return RGBSpectrum::fromScalar(x);
}

constexpr Float luminance(const MonoSpectrum x, const std::monostate&) noexcept {
    return x;
}

constexpr Float maxComponentValue(const MonoSpectrum x) noexcept {
    return x;
}

template <>
constexpr MonoSpectrum zero<MonoSpectrum>() noexcept {
    return 0.0f;
}

template <>
constexpr MonoSpectrum identity<MonoSpectrum>() noexcept {
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
public:
    static constexpr auto nSamples = 4;
    using VecType = glm::vec<nSamples, Float>;

private:
    PIPER_GUARD_BASE(SpectralSpectrum, VecType)
    PIPER_GUARD_BASE_OP(SpectralSpectrum)
    PIPER_GUARD_ELEMENT_VISE_MULTIPLY(SpectralSpectrum)

    static constexpr SpectralSpectrum fromScalar(const Float x) noexcept {
        return SpectralSpectrum{ VecType{ x } };
    }
};

Float luminance(const SpectralSpectrum& x, const SpectralSpectrum& sampledWavelengths) noexcept;
RGBSpectrum toRGB(const SpectralSpectrum& x, const SpectralSpectrum& sampledWavelengths) noexcept;

inline Float maxComponentValue(const SpectralSpectrum& x) noexcept {
    const auto vec = x.raw();
    static_assert(std::is_same_v<std::decay_t<decltype(vec)>, glm::vec4>);
    return std::fmax(std::fmax(vec.x, vec.y), std::fmax(vec.z, vec.w));
}

template <>
struct WavelengthType<SpectralSpectrum> final {
    using Type = SpectralSpectrum;
};

template <>
constexpr SpectrumType spectrumType<SpectralSpectrum>() noexcept {
    return SpectrumType::LinearRGB;
}

template <>
constexpr SpectralSpectrum zero<SpectralSpectrum>() noexcept {
    return SpectralSpectrum::fromScalar(0.0f);
}

template <>
constexpr SpectralSpectrum identity<SpectralSpectrum>() noexcept {
    return SpectralSpectrum::fromScalar(1.0f);
}

constexpr uint32_t spectrumSize(const SpectrumType type) noexcept {
    switch(type) {
        case SpectrumType::Mono:
            return 1;
        case SpectrumType::LinearRGB:
            return 3;
    }
    return 0;
}

PIPER_NAMESPACE_END
