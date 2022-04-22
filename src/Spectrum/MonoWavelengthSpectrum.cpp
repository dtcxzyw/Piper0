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

Float luminance(const MonoWavelengthSpectrum& x, const MonoWavelengthSpectrum& sampledWavelengths) noexcept {
    constexpr auto scale = static_cast<Float>(rcp(integralOfY));
    return static_cast<Float>(wavelength2Y(static_cast<double>(sampledWavelengths.raw()))) * (x.raw() * scale);
}

RGBSpectrum toRGB(const MonoWavelengthSpectrum& x, const MonoWavelengthSpectrum& sampledWavelengths) noexcept {
    constexpr auto scale = static_cast<Float>(rcp(integralOfY));
    const auto xyz = static_cast<glm::vec3>(wavelength2XYZ(static_cast<double>(sampledWavelengths.raw()))) * (x.raw() * scale);
    return RGBSpectrum::fromRaw(glm::max(RGBSpectrum::matXYZ2RGB * xyz, glm::zero<glm::vec3>()));
}

Float temperatureToSpectrum(Float temperature, Float sampledWavelength) noexcept;

MonoWavelengthSpectrum temperatureToSpectrum(const Float temperature, const MonoWavelengthSpectrum sampledWavelength) noexcept {
    const auto value = temperatureToSpectrum(temperature, sampledWavelength.raw());
    return MonoWavelengthSpectrum::fromScalar(value);
}

PIPER_NAMESPACE_END
