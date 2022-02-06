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

enum class SpectrumType : uint16_t { NotSpectrum, Mono, LinearRGB, Spectral };

class RGBSpectrum;

template <typename T>
RGBSpectrum toRGB(const T&);
template <typename T>
float luminance(const T&);
template <typename T>
SpectrumType spectrumType();
template <typename T>
T one();
template <typename T>
T zero();

template <typename T>
concept SpectrumLike = requires(T x, float y) {
    { x + x } -> std::convertible_to<T>;
    { x* x } -> std::convertible_to<T>;
    { x* y } -> std::convertible_to<T>;
    { toRGB(x) } -> std::convertible_to<RGBSpectrum>;
    { luminance(x) } -> std::convertible_to<float>;
    { spectrumType<T>() } -> std::convertible_to<SpectrumType>;
    { one<T>() } -> std::convertible_to<T>;
    { zero<T>() } -> std::convertible_to<T>;
};

using MonoSpectrum = float;

class RGBSpectrum final {
public:
    constexpr explicit RGBSpectrum(MonoSpectrum x) {}
};

class SpectralSpectrum final {
public:
    constexpr explicit SpectralSpectrum(MonoSpectrum x) {}
};

constexpr RGBSpectrum toRGB(const MonoSpectrum x) {
    return RGBSpectrum{ x };
}

constexpr const RGBSpectrum& toRGB(const RGBSpectrum& x) {
    return x;
}

constexpr float luminance(const MonoSpectrum x) {
    return x;
}

constexpr float luminance(const RGBSpectrum& x) {
    return 0.0f;
}

constexpr float luminance(const SpectralSpectrum& x) {
    return 0.0f;
}

template <>
constexpr SpectrumType spectrumType<MonoSpectrum>() {
    return SpectrumType::Mono;
}

template <>
constexpr SpectrumType spectrumType<RGBSpectrum>() {
    return SpectrumType::LinearRGB;
}

template <>
constexpr SpectrumType spectrumType<SpectralSpectrum>() {
    return SpectrumType::Spectral;
}

template <>
constexpr MonoSpectrum zero<MonoSpectrum>() {
    return 0.0f;
}

template <>
constexpr MonoSpectrum one<MonoSpectrum>() {
    return 1.0f;
}

template <>
constexpr RGBSpectrum zero<RGBSpectrum>() {
    return RGBSpectrum{ 0.0f };
}

template <>
constexpr RGBSpectrum one<RGBSpectrum>() {
    return RGBSpectrum{ 1.0f };
}

template <>
constexpr SpectralSpectrum zero<SpectralSpectrum>() {
    return SpectralSpectrum{ 0.0f };
}

template <>
constexpr SpectralSpectrum one<SpectralSpectrum>() {
    return SpectralSpectrum{ 1.0f };
}

PIPER_NAMESPACE_END
