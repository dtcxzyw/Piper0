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

#include <Piper/Core/StaticFactory.hpp>
#include <Piper/Render/Acceleration.hpp>
#include <Piper/Render/Integrator.hpp>
#include <Piper/Render/LightSampler.hpp>
#include <Piper/Render/PipelineNode.hpp>
#include <Piper/Render/RenderGlobalSetting.hpp>
#include <Piper/Render/Sampler.hpp>
#include <Piper/Render/SceneObject.hpp>
#include <Piper/Render/Sensor.hpp>
#include <magic_enum.hpp>
#include <oneapi/tbb/parallel_for_each.h>
#include <oneapi/tbb/spin_mutex.h>
#include <unordered_set>

PIPER_NAMESPACE_BEGIN

struct FrameAction final {
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t frameCount = 0;
    Ref<RefCountBase> integrator;
    Ref<Sampler> sampler;
    double begin = 0.0;
    double fps = 0.0;
    double shutterOpen = 0.0;
    double shutterClose = 0.0;
    Channel channels = static_cast<Channel>(0);
    Ref<Sensor> sensor;
    SensorNDCAffineTransform transform;
    RenderRECT rect;
};

Ref<AccelerationBuilder> createEmbreeBackend();

class Renderer final : public SourceNode {
    ChannelRequirement mRequirement;
    std::pmr::string mCachePath;
    std::pmr::vector<Ref<SceneObject>> mSceneObjects;
    std::pmr::vector<FrameAction> mActions;
    Ref<Acceleration> mAcceleration;

    ProgressReporterHandle mProgressReporter{ "Rendering" };
    uint32_t mFrameCount = 0, mTotalFrameCount = 0;

    static std::pmr::vector<glm::uvec2> generateSpiralTiles(const uint32_t tileX, const uint32_t tileY) {
        std::pmr::vector<glm::uvec2> res{ context().globalAllocator };
        res.reserve(tileX * tileY);

        int32_t curX = static_cast<int32_t>(tileX) / 2;
        int32_t curY = static_cast<int32_t>(tileY) / 2;
        res.emplace_back(curX, curY);

        const auto tryInsert = [&] {
            if(0 <= curX && curX < static_cast<int32_t>(tileX) && 0 <= curY && curY < static_cast<int32_t>(tileY))
                res.emplace_back(curX, curY);
        };

        const auto end = std::max(tileX, tileY);
        for(uint32_t k = 1; k <= end; ++k) {
            // right
            for(uint32_t i = 0; i < k; ++i, ++curX)
                tryInsert();
            // down
            for(uint32_t i = 0; i < k; ++i, ++curY)
                tryInsert();
            // left
            for(uint32_t i = 0; i <= k; ++i, --curX)
                tryInsert();
            // up
            for(uint32_t i = 0; i <= k; ++i, --curY)
                tryInsert();
        }

        return res;
    }

    struct PrimaryRay final {
        glm::vec2 filmCoord;
        SampleProvider sampleProvider;
        Float weight;
    };

    void tracePrimary(const std::pmr::vector<PrimaryRay>& primaryRays, const RayStream& rayStream, std::pmr::vector<Float>& tileData,
                      uint32_t tileWidth, uint32_t tileHeight, int32_t x0, int32_t y0, Channel channels,
                      const Ref<RefCountBase>& integrator) {
        const auto intersections = mAcceleration->tracePrimary(rayStream);

        if(static_cast<bool>(channels & Channel::Full)) {
        }
        if(static_cast<bool>(channels & Channel::Direct)) {
            PIPER_NOT_IMPLEMENTED();
        }
        if(static_cast<bool>(channels & Channel::Indirect)) {
            PIPER_NOT_IMPLEMENTED();
        }
        if(static_cast<bool>(channels & Channel::Albedo)) {
            PIPER_NOT_IMPLEMENTED();
        }
        if(static_cast<bool>(channels & Channel::ShadingNormal)) {
            PIPER_NOT_IMPLEMENTED();
        }
        if(static_cast<bool>(channels & Channel::Position)) {
            PIPER_NOT_IMPLEMENTED();
        }
        if(static_cast<bool>(channels & Channel::Depth)) {
            PIPER_NOT_IMPLEMENTED();
        }
        if(static_cast<bool>(channels & Channel::Variance)) {
            PIPER_NOT_IMPLEMENTED();
        }
    }

    std::pmr::vector<Float> renderTile(uint32_t frameIdx, uint32_t frameCount, const int32_t x0, const int32_t y0, const int32_t x1,
                                       const int32_t y1, uint32_t width, uint32_t height, const SensorNDCAffineTransform& transform,
                                       Channel channels, const Ref<Sensor>& sensor, const Ref<Sampler>& sampler,
                                       const Ref<RefCountBase>& integrator) {
        MemoryArena arena;

        const uint32_t tileWidth = x1 - x0, tileHeight = y1 - y0;

        std::pmr::vector<Float> tileData{ tileWidth * tileHeight * 4, context().localAllocator };

        const auto sampleXEnd = tileWidth - 1;
        const auto sampleYEnd = tileHeight - 1;
        const auto sampleCount = sampler->samples();

        std::pmr::vector<PrimaryRay> primaryRays{ context().scopedAllocator };
        RayStream stream;

        const auto prepareRay = [&](const uint32_t filmX, const uint32_t filmY, const uint32_t sampleIdx, const uint32_t rayIdx) {
            auto [sample, sampleProvider] = sampler->generate(frameIdx, filmX, filmY, sampleIdx, width, height, frameCount);

            auto& payload = primaryRays[rayIdx];
            payload.filmCoord = { sample.x + static_cast<Float>(filmX), sample.y + static_cast<Float>(filmY) };
            payload.sampleProvider = std::move(sampleProvider);
            const auto sensorNDC = transform.toNDC(payload.filmCoord);
            const auto [ray, weight] = sensor->sample(sensorNDC, payload.sampleProvider);
            payload.weight = weight;
            stream.set(ray, rayIdx);
        };

        const auto syncTile = [] {
            // TODO: sync tile
        };

        if(sampleCount * sampleXEnd > 32) {
            primaryRays.resize(sampleCount);
            stream.resize(sampleCount);

            // trace per pixel
            for(uint32_t y = 1; y <= sampleYEnd; ++y) {
                for(uint32_t x = 1; x <= sampleXEnd; ++x) {
                    const auto filmX = x0 + x, filmY = y0 + y;

                    for(uint32_t sampleIdx = 0; sampleIdx < sampleCount; ++sampleIdx)
                        prepareRay(filmX, filmY, sampleIdx, sampleIdx);

                    tracePrimary(primaryRays, stream, tileData, tileWidth, tileHeight, x0, y0, channels, integrator);
                }
                syncTile();
            }
        } else {
            primaryRays.resize(sampleCount * sampleXEnd);
            stream.resize(sampleCount * sampleXEnd);

            // trace whole line
            for(uint32_t y = 1; y <= sampleYEnd; ++y) {
                for(uint32_t x = 1; x <= sampleXEnd; ++x) {
                    const auto filmX = x0 + x, filmY = y0 + y;
                    for(uint32_t sampleIdx = 0; sampleIdx < sampleCount; ++sampleIdx)
                        prepareRay(filmX, filmY, sampleIdx, sampleIdx + sampleCount * (x - 1));
                }

                tracePrimary(primaryRays, stream, tileData, tileWidth, tileHeight, x0, y0, channels, integrator);

                syncTile();
            }
        }

        return tileData;
    }

    FrameGroup render(const uint32_t actionIdx, const uint32_t frameIdx) {
        constexpr uint32_t tileSize = 32;

        // TODO: sync frame

        const auto& [width, height, frameCount, integrator, sampler, begin, fps, shutterOpen, shutterClose, channels, sensor, transform,
                     rect] = mActions[actionIdx];

        const auto tileX = (rect.width + tileSize - 1) / tileSize;
        const auto tileY = (rect.height + tileSize - 1) / tileSize;

        info(fmt::format("Updating scene for action {} frame {}", frameIdx));

        const TimeInterval shutterInterval{ static_cast<Float>(begin + static_cast<double>(frameIdx) / fps + shutterOpen),
                                            static_cast<Float>(begin + static_cast<double>(frameIdx) / fps + shutterClose) };

        info(fmt::format("Rendering scene for action{} frame {}", frameIdx));
        for(const auto& object : mSceneObjects)
            object->update(shutterInterval);
        mAcceleration->commit();

        std::pmr::vector<glm::uvec2> blocks = generateSpiralTiles(tileX, tileY);

        const auto progressBase = static_cast<double>(mFrameCount - 1) / static_cast<double>(mTotalFrameCount),
                   progressIncr = 1.0 / static_cast<double>(tileX * tileY * mTotalFrameCount);

        tbb::speculative_spin_mutex mutex;

        std::uint32_t tileCount = 0;

        tbb::parallel_for_each(blocks.begin(), blocks.end(), [&](const glm::uvec2 tile) {
            const auto x0 = static_cast<int32_t>(rect.left + tile.x * tileSize) - 1;
            const auto y0 = static_cast<int32_t>(rect.top + tile.y * tileSize) - 1;
            const auto x1 = std::min(static_cast<int32_t>(rect.width), static_cast<int32_t>(rect.left + (tile.x + 1) * tileSize) + 1);
            const auto y1 = std::min(static_cast<int32_t>(rect.height), static_cast<int32_t>(rect.top + (tile.y + 1) * tileSize) + 1);

            const auto res =
                renderTile(frameIdx, frameCount, x0, y0, x1, y1, width, height, transform, channels, sensor, sampler, integrator);

            decltype(mutex)::scoped_lock guard{ mutex };

            // TODO: merge tile

            mProgressReporter.update(progressBase + progressIncr * (++tileCount));
        });

        FrameGroup res;

        mProgressReporter.update(static_cast<double>(mFrameCount) / static_cast<double>(mTotalFrameCount));

        return res;
    }

public:
    explicit Renderer(const Ref<ConfigNode>& node) {
        auto& settings = RenderGlobalSetting::get();

        settings.variant = std::pmr::string{ node->get("Variant"sv)->as<std::string_view>(), context().globalAllocator };
        settings.accelerationBuilder = createEmbreeBackend();

        const auto& objects = node->get("Scene"sv)->as<ConfigAttr::AttrArray>();
        mSceneObjects.reserve(objects.size());

        std::pmr::unordered_map<std::string_view, Ref<Sensor>> sensors{ context().localAllocator };

        {
            tbb::speculative_spin_mutex mutex;

            std::pmr::vector<Ref<PrimitiveGroup>> groups{ context().localAllocator };

            tbb::parallel_for_each(objects.cbegin(), objects.cend(), [&](const Ref<ConfigAttr>& attr) {
                const auto& ref = attr->as<Ref<ConfigNode>>();

                auto object = getStaticFactory().make<SceneObject>(ref);
                decltype(mutex)::scoped_lock guard{ mutex };

                if(auto group = object->primitiveGroup())
                    groups.push_back(std::move(group));

                if(auto sensor = dynamicCast<Sensor>(object))
                    sensors.insert({ ref->name(), sensor });

                mSceneObjects.push_back(std::move(object));
            });

            mAcceleration = settings.accelerationBuilder->buildScene(groups);
        }

        for(auto& action : node->get("Action"sv)->as<ConfigAttr::AttrArray>()) {
            const auto& attrs = action->as<Ref<ConfigNode>>();
            FrameAction res;
            res.width = attrs->get("Width"sv)->as<uint32_t>();
            res.height = attrs->get("Height"sv)->as<uint32_t>();
            res.frameCount = attrs->get("FrameCount"sv)->as<uint32_t>();

            res.sampler = getStaticFactory().make<Sampler>(attrs->get("Sampler")->as<Ref<ConfigNode>>());

            // TODO: make variant
            res.integrator = getStaticFactory().make<RefCountBase>(attrs->get("Integrator")->as<Ref<ConfigNode>>());

            res.begin = attrs->get("Begin"sv)->as<double>();
            res.fps = attrs->get("Height"sv)->as<double>();
            res.shutterOpen = attrs->get("ShutterOpen"sv)->as<uint32_t>();
            res.shutterClose = attrs->get("ShutterClose"sv)->as<uint32_t>();
            for(auto& channel : attrs->get("Channels"sv)->as<ConfigAttr::AttrArray>())
                res.channels = res.channels | magic_enum::enum_cast<Channel>(channel->as<std::string_view>()).value();

            res.sensor = sensors.find(attrs->get("Sensor"sv)->as<std::string_view>())->second;

            auto fitMode = FitMode::Fill;
            if(const auto ptr = attrs->tryGet("FitMode"sv))
                fitMode = magic_enum::enum_cast<FitMode>((*ptr)->as<std::string_view>()).value();

            auto [transform, rect] = calcRenderRECT(res.width, res.height, res.sensor->deviceAspectRatio(), fitMode);

            res.transform = transform;
            res.rect = rect;

            mActions.push_back(res);
            mTotalFrameCount += res.frameCount;
        }
    }

    FrameGroup transform(FrameGroup) override {
        uint32_t idx = 0;
        uint32_t frameIdx = mFrameCount;
        while(frameIdx >= mActions[idx].frameCount) {
            frameIdx -= mActions[idx].frameCount;
            ++idx;
        }
        ++mFrameCount;

        return render(idx, frameIdx);
    }
    uint32_t frameCount() override {
        return mTotalFrameCount;
    }
    ChannelRequirement setup(const std::pmr::string& path, ChannelRequirement req) override {
        mCachePath = path + "/cache/render";
        if(!fs::exists(mCachePath))
            fs::create_directories(mCachePath);

        for(auto [channel, force] : req)
            if(channel != Channel::Full && force)
                fatal("Unsupported channel");
        return {};
    }
};

PIPER_REGISTER_CLASS(Renderer, PipelineNode);

PIPER_NAMESPACE_END
