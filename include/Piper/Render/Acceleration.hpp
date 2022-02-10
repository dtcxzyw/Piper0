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
#include <Piper/Core/RefCount.hpp>
#include <Piper/Render/Intersection.hpp>
#include <Piper/Render/KeyFrames.hpp>
#include <Piper/Render/Ray.hpp>

PIPER_NAMESPACE_BEGIN

class PrimitiveGroup : public RefCountBase {
public:
    virtual void updateTransform(const ShutterKeyFrames& transform) = 0;
    virtual void commit() = 0;
};

class OcclusionQueryIterator final {
public:
};

class Acceleration : public RefCountBase {
public:
    virtual void commit() = 0;
    virtual Float radius() const noexcept = 0;
    virtual Intersection trace(const Ray& ray) const = 0;
    virtual bool occluded(const Ray& shadowRay, Distance dist) const = 0;
    // virtual OcclusionQueryIterator occlusions(const Ray& shadowRay, const Distance dist) const = 0;
    virtual std::pmr::vector<Intersection> tracePrimary(const RayStream& rayStream) const = 0;
};

class AccelerationBuilder : public RefCountBase {
public:
    virtual uint32_t maxStepCount() const noexcept = 0;
    virtual Ref<PrimitiveGroup>
    buildFromTriangleMesh(uint32_t vertices, uint32_t faces,
                          const std::function<void(void*, void*)>& writeCallback,  // verticesBuffer,indicesBuffer
                          const Shape& shape) const noexcept = 0;
    virtual Ref<Acceleration> buildScene(const std::pmr::vector<PrimitiveGroup*>& primitiveGroups) const noexcept = 0;
};

PIPER_NAMESPACE_END
