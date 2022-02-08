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
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <numbers>

PIPER_NAMESPACE_BEGIN

using Float = float;
using TexCoord = glm::vec2;

constexpr auto infinity = std::numeric_limits<Float>::infinity();
constexpr auto epsilon = 1e-4f;
constexpr auto e = std::numbers::e_v<Float>;
constexpr auto pi = std::numbers::pi_v<Float>;
constexpr auto sqrtTwo = std::numbers::sqrt2_v<Float>;
constexpr auto twoPi = glm::two_pi<Float>();
constexpr auto fourPi = static_cast<Float>(4.0 * std::numbers::pi);
constexpr auto invPi = std::numbers::inv_pi_v<Float>;
constexpr auto invSqrtPi = std::numbers::inv_sqrtpi_v<Float>;
constexpr auto invSqrt2 = static_cast<Float>(1.0 / std::numbers::sqrt2);
constexpr auto oneMinusEpsilon = static_cast<Float>(0x1.fffffep-1);

glm::vec3 parseVec3(const Ref<ConfigAttr>& node);
glm::quat parseQuat(const Ref<ConfigAttr>& node);

PIPER_NAMESPACE_END
