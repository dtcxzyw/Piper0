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
#include <Piper/Config.hpp>
#include <cstdint>

PIPER_NAMESPACE_BEGIN

constexpr int32_t wavelengthMin = 360;
constexpr int32_t wavelengthMax = 830;
constexpr int32_t spectralLUTSize = wavelengthMax - wavelengthMin + 1;

constexpr std::tuple<uint32_t, uint32_t, double> locateWavelength(const double lambda) noexcept {
    const auto offset = lambda - wavelengthMin;

    const auto idx0 = std::min(spectralLUTSize - 2, std::max(static_cast<int32_t>(lambda) - wavelengthMin, 0));
    const auto idx1 = idx0 + 1;

    const auto u = offset - static_cast<double>(idx0);
    return { idx0, idx1, u };
}

constexpr double lerp(const double* table, const uint32_t idx0, const uint32_t idx1, const double u) {
    return table[idx0] * (1.0 - u) + table[idx1] * u;
}

PIPER_NAMESPACE_END
