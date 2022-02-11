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

#include <Piper/Core/Report.hpp>
#include <Piper/Render/ColorSpace.hpp>
#include <Piper/Render/SpectrumUtil.hpp>

PIPER_NAMESPACE_BEGIN

SpectralSpectrum parseStandardSpectrum(const Ref<ConfigNode>& node) {
    const auto name = node->get("Name"sv)->as<std::string_view>();
    fatal(fmt::format("Unrecognized standard light source {}", name));
}
RGBSpectrum parseRGBSpectrum(const Ref<ConfigNode>& node, const SpectrumParseType type) {
    auto value = parseVec3(node->get("Value"sv));

    if(type == SpectrumParseType::Albedo) {
        const auto colorSpace = node->get("ColorSpace"sv)->as<std::string_view>();
        value = convertRGB2StandardLinearRGB(value, colorSpace);
    }

    return RGBSpectrum::fromRaw(value);
}
SpectralSpectrum parseSpectralSpectrum(const Ref<ConfigNode>& node, const SpectrumParseType type) {
    PIPER_NOT_IMPLEMENTED();
}

SpectralSpectrum temperatureToSpectrum(Float temperature, const SpectralSpectrum& sampledWavelengths) noexcept;

SpectralSpectrum parseBlackBodySpectrum(const Ref<ConfigNode>& node, const SpectralSpectrum& sampledWavelengths) {
    const auto temp = static_cast<Float>(node->get("Temperature"sv)->as<double>());
    const auto scale = static_cast<Float>(node->get("Scale"sv)->as<double>());

    return temperatureToSpectrum(temp, sampledWavelengths) * scale;
}

PIPER_NAMESPACE_END
