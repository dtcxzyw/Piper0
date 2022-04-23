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
#include <Piper/Render/RenderGlobalSetting.hpp>
#include <Piper/Render/Texture.hpp>
#include <Piper/Render/Transform.hpp>

PIPER_NAMESPACE_BEGIN

struct SurfaceHit final {
    Point<FrameOfReference::World> hit;
    Distance distance;
    Normal<FrameOfReference::World> geometryNormal;  // NOTICE: in the same hemisphere with wo
    Normal<FrameOfReference::World> shadingNormal;   // NOTICE: always outer
    Direction<FrameOfReference::World> dpdu;
    uint32_t primitiveIdx;
    TexCoord texCoord;
    Float t;

    Handle<Material> surface;

    [[nodiscard]] Point<FrameOfReference::World> offsetOrigin(const bool reflection) const noexcept {
        return hit + geometryNormal.asDirection() * Distance::fromRaw(reflection ? epsilon : -epsilon);
    }

    [[nodiscard]] TextureEvaluateInfo makeTextureEvaluateInfo() const noexcept {
        return { texCoord, t, primitiveIdx };
    }
};

using Intersection = std::variant<std::monostate, SurfaceHit>;

PIPER_NAMESPACE_END
