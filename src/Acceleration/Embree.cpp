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
#include <Piper/Core/Stats.hpp>
#include <Piper/Render/Acceleration.hpp>
#include <Piper/Render/Material.hpp>
#include <Piper/Render/Shape.hpp>
#include <embree3/rtcore.h>
#include <glm/gtc/type_ptr.hpp>

PIPER_NAMESPACE_BEGIN

class DeviceInstance final {
    RTCDevice mDevice = nullptr;
    std::atomic<ssize_t> mUsedMemory = 0;
    std::atomic_uint32_t mUpdateCount = 0;

public:
    DeviceInstance() {
        initFloatingPointEnvironment();

        mDevice = rtcNewDevice("start_threads=1,set_affinity=1");
        rtcSetDeviceMemoryMonitorFunction(
            mDevice,
            [](void* ptr, const ssize_t bytes, bool) {
                auto& self = *static_cast<DeviceInstance*>(ptr);
                self.mUsedMemory += bytes;

                auto& count = self.mUpdateCount;
                const auto newCount = getMonitor().updateCount();
                auto old = count.load();
                do {
                    if(old >= newCount)
                        return true;
                } while(!count.compare_exchange_strong(old, newCount));

                const auto used = self.mUsedMemory.load();

                if(used >= 1'000'000)
                    getMonitor().updateCustomStatus(ptr, fmt::format(" Embree memory usage: {:.3f} MB", static_cast<double>(used) * 1e-6));
                else
                    getMonitor().updateCustomStatus(ptr, fmt::format(" Embree memory usage: {:.3f} KB", static_cast<double>(used) * 1e-3));
                return true;
            },
            this);

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

static const RTCDevice forceInitBeforeMain = device();  // For option "start_threads=1"

class EmbreeGeometry final : public PrimitiveGroup {
    RTCGeometry mGeometry;
    RTCScene mInstancedScene;
    RTCGeometry mMotionBlurGeometry;

public:
    explicit EmbreeGeometry(const RTCGeometry geometry, const Shape* shape) : mGeometry{ geometry } {
        rtcSetGeometryBuildQuality(mGeometry, RTC_BUILD_QUALITY_HIGH);

        const auto dev = device();
        mInstancedScene = rtcNewScene(dev);

        // TODO: progress monitor
        rtcSetSceneProgressMonitorFunction(
            mInstancedScene, [](void* ptr, double progress) { return true; }, this);
        rtcSetSceneBuildQuality(mInstancedScene, RTC_BUILD_QUALITY_HIGH);
        rtcSetSceneFlags(mInstancedScene, RTC_SCENE_FLAG_COMPACT);

        rtcAttachGeometry(mInstancedScene, mGeometry);

        mMotionBlurGeometry = rtcNewGeometry(dev, RTC_GEOMETRY_TYPE_INSTANCE);
        rtcSetGeometryBuildQuality(mMotionBlurGeometry, RTC_BUILD_QUALITY_HIGH);
        rtcSetGeometryInstancedScene(mMotionBlurGeometry, mInstancedScene);
        rtcSetGeometryUserData(mMotionBlurGeometry, const_cast<Shape*>(shape));
    }

    ~EmbreeGeometry() override {
        rtcReleaseGeometry(mMotionBlurGeometry);
        rtcReleaseScene(mInstancedScene);
        rtcReleaseGeometry(mGeometry);
    }

    RTCGeometry getGeometry() const noexcept {
        return mMotionBlurGeometry;
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
        rtcCommitGeometry(mMotionBlurGeometry);
    }
};

class EmbreeScene final : public Acceleration {
    RTCScene mScene;

public:
    explicit EmbreeScene(const RTCScene scene) : mScene{ scene } {

        rtcSetSceneBuildQuality(mScene, RTC_BUILD_QUALITY_LOW);
        rtcSetSceneFlags(mScene, RTC_SCENE_FLAG_DYNAMIC);

        // TODO: progress monitor
        rtcSetSceneProgressMonitorFunction(
            scene, [](void* ptr, double progress) { return true; }, this);
    }

    ~EmbreeScene() override {
        rtcReleaseScene(mScene);
    }

    Float radius() const noexcept override {
        RTCLinearBounds linearBounds;
        rtcGetSceneLinearBounds(mScene, &linearBounds);
        constexpr auto evalRadius = [](const RTCBounds& bounds) {
            return glm::distance(glm::vec3{ bounds.lower_x, bounds.lower_y, bounds.lower_z },
                                 glm::vec3{ bounds.upper_x, bounds.upper_y, bounds.upper_z }) *
                0.5f;
        };
        return (evalRadius(linearBounds.bounds0) + evalRadius(linearBounds.bounds1)) * 0.5f;
    }

    void commit() override {
        rtcCommitScene(mScene);
    }

    Intersection processHitInfo(const Ray& ray, const RTCHit& hitInfo, const Distance distance) const {
        if(distance.raw() < infinity) {  // NOLINT(clang-diagnostic-float-equal)
            // surface
            BoolCounter<StatsType::Intersection>::count(true);

            const auto geo = rtcGetGeometry(mScene, hitInfo.instID[0]);
            const auto& shape = *static_cast<const Shape*>(rtcGetGeometryUserData(geo));

            glm::mat4 mat;
            rtcGetGeometryTransform(geo, ray.t, RTC_FORMAT_FLOAT4X4_COLUMN_MAJOR, glm::value_ptr(mat));
            const AffineTransform<FrameOfReference::Object, FrameOfReference::World> trans{ mat };
            // NOTICE: Ng_x/y/z are not normalized!!!
            auto geometryNormal =
                Normal<FrameOfReference::World>::fromRaw(glm::normalize(glm::vec3{ hitInfo.Ng_x, hitInfo.Ng_y, hitInfo.Ng_z }));

            if(dot(geometryNormal.asDirection(), ray.direction) > 0.0f)
                geometryNormal = -geometryNormal;

            return shape.generateIntersection(ray, distance, trans, geometryNormal, { hitInfo.u, hitInfo.v }, hitInfo.primID);
        }

        // missing
        BoolCounter<StatsType::Intersection>::count(false);
        return std::monostate{};
    }

    Intersection trace(const Ray& ray) const override {
        RTCIntersectContext ctx{};  // TODO: filter
        rtcInitIntersectContext(&ctx);

        RTCRayHit hit = { { ray.origin.x(), ray.origin.y(), ray.origin.z(), epsilon, ray.direction.x(), ray.direction.y(),
                            ray.direction.z(), ray.t, infinity, 0, 0, 0 },
                          {} };

        FloatingPointExceptionProbe::off();
        rtcIntersect1(mScene, &ctx, &hit);
        FloatingPointExceptionProbe::on();

        return processHitInfo(ray, hit.hit, Distance::fromRaw(hit.ray.tfar));
    }

    std::pmr::vector<Intersection> tracePrimary(const RayStream& rayStream) const override {
        RTCIntersectContext ctx{};  // TODO: filter
        rtcInitIntersectContext(&ctx);
        ctx.flags = RTC_INTERSECT_CONTEXT_FLAG_COHERENT;

        std::pmr::vector<RTCRayHit> hit{ rayStream.size(), context().scopedAllocator };
        for(uint32_t idx = 0; idx < hit.size(); ++idx) {
            const auto& [origin, direction, t] = rayStream[idx];
            hit[idx].ray = {
                origin.x(), origin.y(), origin.z(), epsilon, direction.x(), direction.y(), direction.z(), t, infinity, 0, 0, 0
            };
        }

        FloatingPointExceptionProbe::off();
        rtcIntersect1M(mScene, &ctx, hit.data(), static_cast<uint32_t>(hit.size()), sizeof(RTCRayHit));
        FloatingPointExceptionProbe::on();

        std::pmr::vector<Intersection> res{ rayStream.size(), context().scopedAllocator };

        for(uint32_t idx = 0; idx < hit.size(); ++idx) {
            const auto& ray = rayStream[idx];
            res[idx] = processHitInfo(ray, hit[idx].hit, Distance::fromRaw(hit[idx].ray.tfar));
        }

        return res;
    }

    bool occluded(const Ray& shadowRay, const Distance dist) const override {
        RTCIntersectContext ctx{};
        rtcInitIntersectContext(&ctx);

        RTCRay ray{ shadowRay.origin.x(),
                    shadowRay.origin.y(),
                    shadowRay.origin.z(),
                    epsilon,
                    shadowRay.direction.x(),
                    shadowRay.direction.y(),
                    shadowRay.direction.z(),
                    shadowRay.t,
                    dist.raw(),
                    0,
                    0,
                    0 };
        FloatingPointExceptionProbe::off();
        rtcOccluded1(mScene, &ctx, &ray);
        FloatingPointExceptionProbe::on();

        return ray.tfar < dist.raw();
    }
};

class EmbreeBuilder final : public AccelerationBuilder {
public:
    uint32_t maxStepCount() const noexcept override {
        return 129u;
    }

    Ref<PrimitiveGroup> buildFromTriangleMesh(const uint32_t vertices, const uint32_t faces,
                                              const std::function<void(void*, void*)>& writeCallback,
                                              const Shape& shape) const noexcept override {
        const auto dev = device();
        const auto geometry = rtcNewGeometry(dev, RTC_GEOMETRY_TYPE_TRIANGLE);
        const auto verticesBuffer =
            rtcSetNewGeometryBuffer(geometry, RTC_BUFFER_TYPE_VERTEX, 0, RTC_FORMAT_FLOAT3, sizeof(glm::vec3), vertices);
        const auto indicesBuffer = rtcSetNewGeometryBuffer(geometry, RTC_BUFFER_TYPE_INDEX, 0, RTC_FORMAT_UINT3, sizeof(glm::uvec3), faces);
        writeCallback(verticesBuffer, indicesBuffer);
        return makeRefCount<EmbreeGeometry>(geometry, &shape);
    }

    Ref<Acceleration> buildScene(const std::pmr::vector<PrimitiveGroup*>& primitiveGroups) const noexcept override {
        const auto dev = device();
        const auto scene = rtcNewScene(dev);

        for(auto& group : primitiveGroups) {
            const auto geometry = dynamic_cast<EmbreeGeometry*>(group)->getGeometry();
            rtcAttachGeometry(scene, geometry);
        }

        return makeRefCount<EmbreeScene>(scene);
    }
};

Ref<AccelerationBuilder> createEmbreeBackend() {
    return makeRefCount<EmbreeBuilder>();
}

PIPER_NAMESPACE_END
