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
#include <Piper/Render/Intersection.hpp>
#include <Piper/Render/Ray.hpp>
#include <Piper/Render/SceneObject.hpp>

PIPER_NAMESPACE_BEGIN

class Shape : public SceneObjectComponent {
public:
    virtual Intersection generateIntersection(const Ray& ray, Distance hitDistance,
                                              const AffineTransform<FrameOfReference::Object, FrameOfReference::World>& transform,
                                              const Normal<FrameOfReference::World>& geometryNormal, glm::vec2 barycentric,
                                              uint32_t primitiveIndex) const noexcept = 0;
};

PIPER_NAMESPACE_END
