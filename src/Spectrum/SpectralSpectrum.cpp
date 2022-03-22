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

#include <Piper/Render/ColorMatchingFunction.hpp>
#include <Piper/Render/Spectrum.hpp>

PIPER_NAMESPACE_BEGIN

template <size_t Samples, typename T, size_t... I>
static constexpr Float expandLum(const T& x, const T& w, std::index_sequence<I...>) {
    return ((static_cast<Float>(wavelength2Y(static_cast<double>(w[I]))) * x[I]) + ...) / static_cast<Float>(Samples);
}

Float luminance(const SampledSpectrum& x, const SampledSpectrum& sampledWavelengths) noexcept {
    constexpr auto indices = std::make_index_sequence<SampledSpectrum::nSamples>{};
    return expandLum<SampledSpectrum::nSamples>(x.raw(), sampledWavelengths.raw(), indices);
}

template <size_t Samples, typename T, size_t... I>
static glm::vec3 expandXYZ(const T& x, const T& w, std::index_sequence<I...>) {
    return ((static_cast<glm::vec3>(wavelength2XYZ(static_cast<double>(w[I]))) * x[I]) + ...) / static_cast<Float>(Samples);
}

RGBSpectrum toRGB(const SampledSpectrum& x, const SampledSpectrum& sampledWavelengths) noexcept {
    constexpr auto indices = std::make_index_sequence<SampledSpectrum::nSamples>{};
    const auto xyz = expandXYZ<SampledSpectrum::nSamples>(x.raw(), sampledWavelengths.raw(), indices);
    return RGBSpectrum::fromRaw(RGBSpectrum::matXYZ2RGB * xyz);
}

static Float blackBody(const Float temperature, const Float lambdaNm) noexcept {
    // Planck constant h
    // Please refer to https://physics.nist.gov/cgi-bin/cuu/Value?h
    constexpr auto h = 6.626'070'15e-34;  // J/Hz
    // speed of light in vacuum c
    // Please refer to https://physics.nist.gov/cgi-bin/cuu/Value?c
    constexpr auto c = 299'792'458.0;  // m/s
    // Boltzmann constant k
    // Please refer to https://physics.nist.gov/cgi-bin/cuu/Value?k
    constexpr auto k = 1.380'649e-23;  // J/K

    constexpr auto k1 = static_cast<Float>(2.0 * h * c * c);
    constexpr auto k2 = static_cast<Float>(h * c / k);
    const auto lambda = lambdaNm * 1e-9f;

    return k1 * pow<5>(lambda) * (exp(k2 / (lambda * temperature)) - 1.0f);
}

template <size_t Samples, typename T, size_t... I>
static auto expandBlackBody(const Float temperature, const T& x, std::index_sequence<I...>) {
    return T{ (blackBody(temperature, x[I]), ...) };
}

SampledSpectrum temperatureToSpectrum(const Float temperature, const SampledSpectrum& sampledWavelength) noexcept {
    constexpr auto indices = std::make_index_sequence<SampledSpectrum::nSamples>{};
    return SampledSpectrum::fromRaw(expandBlackBody<SampledSpectrum::nSamples>(temperature, sampledWavelength.raw(), indices));
}

PIPER_NAMESPACE_END
