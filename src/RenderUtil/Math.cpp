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

#include <Piper/Core/ConfigNode.hpp>
#include <Piper/Core/Report.hpp>
#include <Piper/Render/Math.hpp>
#include <glm/gtc/quaternion.hpp>
#include <pmmintrin.h>
#include <xmmintrin.h>

PIPER_NAMESPACE_BEGIN

void initFloatingPointEnvironment() noexcept {
    _MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_ON);
    _MM_SET_DENORMALS_ZERO_MODE(_MM_DENORMALS_ZERO_ON);
}

glm::vec2 parseVec2(const Ref<ConfigAttr>& node) {
    const auto& arr = node->as<ConfigAttr::AttrArray>();
    if(arr.size() != 2)
        fatal("Bad vector2");

    return { static_cast<Float>(arr[0]->as<double>()), static_cast<Float>(arr[1]->as<double>()) };
}

glm::vec3 parseVec3(const Ref<ConfigAttr>& node) {
    const auto& arr = node->as<ConfigAttr::AttrArray>();
    if(arr.size() != 3)
        fatal("Bad vector3");

    return { static_cast<Float>(arr[0]->as<double>()), static_cast<Float>(arr[1]->as<double>()), static_cast<Float>(arr[2]->as<double>()) };
}

glm::quat parseQuat(const Ref<ConfigAttr>& node) {
    const auto& arr = node->as<ConfigAttr::AttrArray>();
    if(arr.size() != 4)
        fatal("Bad quaternion");
    return glm::quat{ static_cast<Float>(arr[0]->as<double>()), static_cast<Float>(arr[1]->as<double>()),
                      static_cast<Float>(arr[2]->as<double>()), static_cast<Float>(arr[3]->as<double>()) };
}

PIPER_NAMESPACE_END