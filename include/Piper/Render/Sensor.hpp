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

#include <Piper/Render/Ray.hpp>
#include <Piper/Render/SceneObject.hpp>

PIPER_NAMESPACE_BEGIN

enum class FitMode : uint32_t { Fill, OverScan };

struct SensorNDCAffineTransform final {
    Float ox, oy, sx, sy;

    [[nodiscard]] constexpr glm::vec2 toNDC(const glm::vec2 filmPoint) const noexcept {
        return { ox + filmPoint.x * sx, oy + filmPoint.y * sy };
    }
};

struct RenderRECT final {
    uint32_t left, top, width, height;
};

[[nodiscard]] std::pair<SensorNDCAffineTransform, RenderRECT> calcRenderRECT(uint32_t width, uint32_t height, Float deviceAspectRatio,
                                                                             FitMode fitMode);
class Sensor : public SceneObjectComponent {
protected:
    ResolvedTransform mTransform{};

public:
    virtual Float deviceAspectRatio() const noexcept = 0;
    virtual std::pair<Ray, Float> sample(glm::vec2 sensorNDC, SampleProvider& sampler) const noexcept = 0;
    PrimitiveGroup* primitiveGroup() const noexcept final {
        return nullptr;
    }
    void updateTransform(const KeyFrames& keyFrames, TimeInterval timeInterval) final;
};

// TODO: Sensor Controller

glm::vec2 parseSensorSize(const Ref<ConfigAttr>& attr);

PIPER_NAMESPACE_END
