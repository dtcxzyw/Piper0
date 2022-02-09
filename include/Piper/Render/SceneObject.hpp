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
#include <Piper/Render/KeyFrames.hpp>
#include <Piper/Render/RenderGlobalSetting.hpp>

PIPER_NAMESPACE_BEGIN

class SceneObjectComponent : public RenderVariantBase {
public:
    virtual void updateTransform(const KeyFrames& keyFrames, TimeInterval timeInterval) = 0;
    virtual PrimitiveGroup* primitiveGroup() const noexcept = 0;
};

enum class ComponentType { Sensor, Light, Shape };

class SceneObject final : public RefCountBase {
    KeyFrames mKeyFrames;
    ComponentType mComponentType;
    Ref<SceneObjectComponent> mComponent;

public:
    explicit SceneObject(const Ref<ConfigNode>& node);
    void update(TimeInterval timeInterval);
    PrimitiveGroup* primitiveGroup() const;
    Sensor* sensor() const noexcept;
    LightBase* light() const noexcept;
};

PIPER_NAMESPACE_END
