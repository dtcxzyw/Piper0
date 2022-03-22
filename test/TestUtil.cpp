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
#include <Piper/Render/TestUtil.hpp>
#include <chrono>

PIPER_NAMESPACE_BEGIN

double simpson(const double* table, const uint32_t size, const double width) noexcept {
    const auto n = (size - 1) / 2;
    double sum = table[0] + table[2ULL * n];
    for(uint32_t idx = 0; idx < n; ++idx)
        sum += 4.0 * table[2 * idx + 1] + 2.0 * table[2 * idx + 2];
    return width * sum / static_cast<double>(6 * n);
}

SampleProvider& testSampler() noexcept {
    static MemoryArena arena;
    static SampleProvider sampler{ {}, 0ULL };
    return sampler;
}

PIPER_NAMESPACE_END
