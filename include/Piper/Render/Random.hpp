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
#include <pcg_random.hpp>

PIPER_NAMESPACE_BEGIN

using RandomEngine = pcg64;

inline Float sample(RandomEngine& eng) {
    return std::fmin(oneMinusEpsilon, static_cast<Float>(static_cast<double>(eng()) * 0x1p-64));
}

// Please refer to https://prng.di.unimi.it/splitmix64.c
constexpr uint64_t seeding(const uint64_t x) {
    uint64_t z = (x + 0x9e3779b97f4a7c15);
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9;
    z = (z ^ (z >> 27)) * 0x94d049bb133111eb;
    return z ^ (z >> 31);
}

PIPER_NAMESPACE_END
