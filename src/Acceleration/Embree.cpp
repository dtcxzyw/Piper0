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

#include <Piper/Core/Monitor.hpp>
#include <Piper/Core/Report.hpp>
#include <Piper/Core/StaticFactory.hpp>
#include <Piper/Render/Acceleration.hpp>
#include <embree3/rtcore.h>

PIPER_NAMESPACE_BEGIN

class DeviceInstance final {
    RTCDevice mDevice = nullptr;
    std::atomic_uint32_t mUpdateCount = 0;

public:
    DeviceInstance() {
        mDevice = rtcNewDevice("start_threads=1,set_affinity=1");
        rtcSetDeviceMemoryMonitorFunction(
            mDevice,
            [](void* ptr, const ssize_t bytes, bool) {
                auto& count = *static_cast<std::atomic_uint32_t*>(ptr);
                const auto newCount = getMonitor().updateCount();
                auto old = count.load();
                do {
                    if(old >= newCount)
                        return true;
                } while(!count.compare_exchange_strong(old, newCount));

                getMonitor().updateCustomStatus(ptr, fmt::format("Embree memory usage: {:.3f} MB", static_cast<double>(bytes) * 1e-6));
                return true;
            },
            &mUpdateCount);

        rtcSetDeviceErrorFunction(
            mDevice, [](void*, enum RTCError code, const char* str) { fatal(fmt::format("Embree error (code = {}): {}", code, str)); },
            nullptr);
    }
    DeviceInstance(const DeviceInstance&) = delete;
    DeviceInstance(DeviceInstance&&) = delete;
    DeviceInstance operator=(const DeviceInstance&) = delete;
    DeviceInstance operator=(DeviceInstance&&) = delete;

    ~DeviceInstance() {
        rtcReleaseDevice(mDevice);
    }

    [[nodiscard]] RTCDevice device() const noexcept {
        return mDevice;
    }
};

static RTCDevice device() {
    static DeviceInstance inst;
    return inst.device();
}

class EmbreeGeometry final : public PrimitiveGroup {
    RTCGeometry mGeometry;
    RTCScene mInstancedScene;
    RTCGeometry mMotionBlurGeometry;

public:
    explicit EmbreeGeometry(const RTCGeometry geometry) : mGeometry{ geometry } {
        const auto dev = device();
        mInstancedScene = rtcNewScene(dev);
        rtcAttachGeometry(mInstancedScene, mGeometry);

        mMotionBlurGeometry = rtcNewGeometry(dev, RTC_GEOMETRY_TYPE_INSTANCE);
        rtcSetGeometryInstancedScene(mGeometry, mInstancedScene);
    }

    ~EmbreeGeometry() override {
        rtcReleaseGeometry(mMotionBlurGeometry);
        rtcReleaseScene(mInstancedScene);
        rtcReleaseGeometry(mGeometry);
    }

    RTCGeometry getGeometry() const noexcept {
        return mMotionBlurGeometry ? mMotionBlurGeometry : mGeometry;
    }

    void updateTransform(const ShutterKeyFrames& transform) override {
        RTCQuaternionDecomposition transSRT{};
        rtcInitQuaternionDecomposition(&transSRT);

        const auto convertSRT = [&](const SRTTransform& trans) {
            rtcQuaternionDecompositionSetScale(&transSRT, trans.scale.x, trans.scale.y, trans.scale.z);
            rtcQuaternionDecompositionSetQuaternion(&transSRT, trans.rotation.w, trans.rotation.x, trans.rotation.y, trans.rotation.z);
            rtcQuaternionDecompositionSetTranslation(&transSRT, trans.translation.x, trans.translation.y, trans.translation.z);
            return &transSRT;
        };

        rtcSetGeometryTimeStepCount(mMotionBlurGeometry, static_cast<uint32_t>(transform.size()));

        for(uint32_t idx = 0; idx < transform.size(); ++idx)
            rtcSetGeometryTransformQuaternion(mMotionBlurGeometry, idx, convertSRT(transform[idx]));
    }

    void commit() override {
        rtcCommitGeometry(mGeometry);
        rtcCommitScene(mInstancedScene);
        rtcCommitGeometry(mGeometry);
    }
};

class EmbreeScene final : public Acceleration {
    RTCScene mScene;

public:
    explicit EmbreeScene(const RTCScene scene) : mScene{ scene } {
        rtcSetSceneProgressMonitorFunction(
            scene, [](void* ptr, double progress) { return true; }, this);
    }

    ~EmbreeScene() override {
        rtcReleaseScene(mScene);
    }

    void commit() override {
        rtcCommitScene(mScene);
    }

    std::optional<Intersection> trace(const Ray& ray) const override {

        return std::nullopt;
    }
    std::pmr::vector<std::optional<Intersection>> tracePrimary(const RayStream& rayStream) const override {
        std::pmr::vector<std::optional<Intersection>> res{ context().scopedAllocator };

        return {};
    }
};

class EmbreeBuilder final : public AccelerationBuilder {
public:
    uint32_t maxStepsCount() const noexcept override {
        return 129u;
    }

    Ref<PrimitiveGroup> buildFromTriangleMesh(const uint32_t vertices, const uint32_t faces,
                                              const std::function<void(void*, void*)>& writeCallback) const noexcept override {
        const auto dev = device();
        const auto geometry = rtcNewGeometry(dev, RTC_GEOMETRY_TYPE_TRIANGLE);
        const auto verticesBuffer =
            rtcSetNewGeometryBuffer(geometry, RTC_BUFFER_TYPE_VERTEX, 0, RTC_FORMAT_FLOAT3, sizeof(glm::vec3), vertices);
        const auto indicesBuffer = rtcSetNewGeometryBuffer(geometry, RTC_BUFFER_TYPE_INDEX, 0, RTC_FORMAT_UINT3, sizeof(glm::uvec3), faces);
        writeCallback(verticesBuffer, indicesBuffer);
        return makeRefCount<EmbreeGeometry>(geometry);
    }

    Ref<Acceleration> buildScene(const std::pmr::vector<Ref<PrimitiveGroup>>& primitiveGroups) const noexcept override {
        const auto dev = device();
        const auto scene = rtcNewScene(dev);

        for(auto& group : primitiveGroups) {
            const auto geometry = dynamic_cast<EmbreeGeometry*>(group.get())->getGeometry();

            rtcAttachGeometry(scene, geometry);
        }

        return makeRefCount<EmbreeScene>(scene);
    }
};

Ref<AccelerationBuilder> createEmbreeBackend() {
    return makeRefCount<EmbreeBuilder>();
}

PIPER_NAMESPACE_END
