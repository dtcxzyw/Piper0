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

#include <Piper/Render/Light.hpp>
#include <Piper/Render/SceneObject.hpp>
#include <Piper/Render/Sensor.hpp>
#include <Piper/Render/Shape.hpp>
#include <magic_enum.hpp>

PIPER_NAMESPACE_BEGIN

SceneObject::SceneObject(const Ref<ConfigNode>& node)
    : mKeyFrames{ parseKeyframes(node->get("KeyFrames"sv)) }, mComponentType{
          magic_enum::enum_cast<ComponentType>(node->get("ComponentType"sv)->as<std::string_view>()).value()
      } {
    auto& factory = getStaticFactory();
    const auto& attr = node->get("Component"sv)->as<Ref<ConfigNode>>();

    switch(mComponentType) {
        case ComponentType::Sensor:
            mComponent = factory.make<Sensor>(attr);
            break;
        case ComponentType::Light:
            mComponent = makeVariant<LightBase, Light>(attr);
            break;
        case ComponentType::Shape:
            mComponent = factory.make<Shape>(attr);
            break;
    }
}

// ReSharper disable once CppMemberFunctionMayBeConst
void SceneObject::update(const TimeInterval timeInterval) {
    mComponent->updateTransform(mKeyFrames, timeInterval);
}

PrimitiveGroup* SceneObject::primitiveGroup() const {
    return mComponent->primitiveGroup();
}

Sensor* SceneObject::sensor() const noexcept {
    return mComponentType == ComponentType::Sensor ? dynamic_cast<Sensor*>(mComponent.get()) : nullptr;
}

LightBase* SceneObject::light() const noexcept {
    return mComponentType == ComponentType::Light ? dynamic_cast<LightBase*>(mComponent.get()) : nullptr;
}

PIPER_NAMESPACE_END
