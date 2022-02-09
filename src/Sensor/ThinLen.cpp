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

#include <Piper/Render/SampleUtil.hpp>
#include <Piper/Render/Sampler.hpp>
#include <Piper/Render/Sensor.hpp>

PIPER_NAMESPACE_BEGIN

class ThinLen final : public Sensor {
    glm::vec2 mSensorSize;
    Point<FrameOfReference::World> mLookAt;
    Normal<FrameOfReference::World> mUpRef;
    Distance mFocalLength;
    Distance mApertureRadius;

public:
    explicit ThinLen(const Ref<ConfigNode>& node)
        : mSensorSize{ parseSensorSize(node->get("SensorSize"sv)) }, mLookAt{ Point<FrameOfReference::World>::fromRaw(
                                                                         parseVec3(node->get("LookAt"sv))) },
          mUpRef{ Normal<FrameOfReference::World>::fromRaw(glm::normalize(parseVec3(node->get("UpRef"sv)))) },
          mFocalLength{ Distance::fromRaw(static_cast<Float>(node->get("FocalLength"sv)->as<double>() * 1e-3)) }, mApertureRadius{
              Distance::fromRaw(mFocalLength.raw() / static_cast<Float>(node->get("FStop"sv)->as<double>() * 2.0))
          } {}

    Float deviceAspectRatio() const noexcept override {
        return mSensorSize.x / mSensorSize.y;
    }
    std::pair<Ray, Float> sample(glm::vec2 sensorNDC, SampleProvider& sampler) const noexcept override {
        const auto t = sampler.sample();
        const auto base = Point<FrameOfReference::World>::fromRaw(mTransform(t).translation);

        const auto [forward, dist] = direction(base, mLookAt);
        const auto right = cross(forward, mUpRef);
        const auto up = cross(right, forward);
        const auto filmHit = base + right * Distance::fromRaw(mSensorSize.x * (0.5f - sensorNDC.x)) +
            up * Distance::fromRaw(mSensorSize.y * (sensorNDC.y - 0.5f));
        // TODO:AF/MF mode support
        const auto focalDistance = dot(forward, mLookAt - base);
        const auto filmDistance = rcp(rcp(mFocalLength) - rcp(focalDistance));
        const auto lensCenter = base + forward * filmDistance;

        const auto lensOffset = sampleUniformDisk(sampler.sampleVec2());
        const auto lensHit = lensCenter + right * (mApertureRadius * lensOffset.x) + up * (mApertureRadius * lensOffset.y);
        const auto dir = lensCenter - filmHit;
        const auto planeOfFocusHit = lensCenter + dir * (focalDistance * rcp(dot(forward, dir)));

        const auto [rayDir, _] = direction(lensHit, planeOfFocusHit);

        return { Ray{ lensHit, rayDir, t }, 1.0f };
    }
};

PIPER_REGISTER_CLASS(ThinLen, Sensor);

PIPER_NAMESPACE_END
