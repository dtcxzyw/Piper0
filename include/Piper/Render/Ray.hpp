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
#include <Piper/Render/Transform.hpp>

PIPER_NAMESPACE_BEGIN

struct Ray final {
    Point<FrameOfReference::World> origin;
    Normal<FrameOfReference::World> direction;

    Float t;
};

struct RayStream final {
    using FloatStream = std::pmr::vector<Float>;
    FloatStream originX{ context().scopedAllocator };
    FloatStream originY{ context().scopedAllocator };
    FloatStream originZ{ context().scopedAllocator };
    FloatStream directionX{ context().scopedAllocator };
    FloatStream directionY{ context().scopedAllocator };
    FloatStream directionZ{ context().scopedAllocator };
    FloatStream sampleTime{ context().scopedAllocator };

    void resize(const size_t size) {
        originX.resize(size);
        originY.resize(size);
        originZ.resize(size);
        directionX.resize(size);
        directionY.resize(size);
        directionZ.resize(size);
        sampleTime.resize(size);
    }

    void set(const Ray& ray, const uint32_t idx) {
        originX[idx] = ray.origin.x();
        originY[idx] = ray.origin.y();
        originZ[idx] = ray.origin.z();
        directionX[idx] = ray.direction.x();
        directionY[idx] = ray.direction.y();
        directionZ[idx] = ray.direction.z();
        sampleTime[idx] = ray.t;
    }
};

PIPER_NAMESPACE_END
