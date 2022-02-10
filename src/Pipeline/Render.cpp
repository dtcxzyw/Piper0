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
#include <Piper/Render/Filter.hpp>
#include <Piper/Render/Integrator.hpp>
#include <Piper/Render/LightSampler.hpp>
#include <Piper/Render/PipelineNode.hpp>
#include <Piper/Render/RenderGlobalSetting.hpp>
#include <Piper/Render/Sampler.hpp>
#include <Piper/Render/SceneObject.hpp>
#include <Piper/Render/Sensor.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <magic_enum.hpp>
#include <oneapi/tbb/parallel_for_each.h>
#include <oneapi/tbb/spin_mutex.h>
#include <unordered_set>

PIPER_NAMESPACE_BEGIN

struct FrameAction final {
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t frameCount = 0;
    Ref<Sampler> sampler;

    double begin = 0.0;
    double fps = 0.0;
    double shutterOpen = 0.0;
    double shutterClose = 0.0;

    std::pmr::vector<Channel> channels{ context().globalAllocator };
    uint32_t channelTotalSize = 0;

    Sensor* sensor = nullptr;
    SensorNDCAffineTransform transform{};
    RenderRECT rect{};
};

Ref<AccelerationBuilder> createEmbreeBackend();

class Renderer final : public SourceNode {
    ChannelRequirement mRequirement;
    std::pmr::string mCachePath;
    std::pmr::vector<Ref<SceneObject>> mSceneObjects{ context().globalAllocator };
    std::pmr::vector<LightBase*> mLights{ context().globalAllocator };
    std::pmr::vector<FrameAction> mActions{ context().globalAllocator };
    Ref<Acceleration> mAcceleration;

    Ref<IntegratorBase> mIntegrator;
    Ref<LightSampler> mLightSampler;
    Ref<Filter> mFilter;

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
        glm::vec2 filmCoord{};
        SampleProvider sampleProvider;
        Float weight = 0.0f;
    };

    void tracePrimary(std::pmr::vector<PrimaryRay>& primaryRays, const RayStream& rayStream, const uint32_t tileWidth, const Float x0,
                      const Float y0, Float* tileData, const std::pmr::vector<Channel>& channels, const uint32_t pixelStride,
                      const uint32_t usedSpectrumSize, const Float shutterTime) {
        const auto intersections = mAcceleration->tracePrimary(rayStream);

        const auto locale = [&](const uint32_t x, const uint32_t y, const uint32_t offset) noexcept -> Float& {
            return tileData[(x + y * tileWidth) * pixelStride + offset];
        };

        for(uint32_t rayIdx = 0; rayIdx < primaryRays.size(); ++rayIdx) {
            auto& [filmCoord, sampleProvider, weight] = primaryRays[rayIdx];
            const auto& ray = rayStream[rayIdx];
            const auto& intersection = intersections[rayIdx];

            const auto ix = static_cast<uint32_t>(filmCoord.x - x0 - 0.5f);
            const auto iy = static_cast<uint32_t>(filmCoord.y - y0 - 0.5f);

            const auto evalWeight = [&](const uint32_t x, const uint32_t y) noexcept {
                return mFilter->evaluate(filmCoord.x - (static_cast<Float>(x) + 0.5f), filmCoord.y - (static_cast<Float>(y) + 0.5f)) *
                    weight;
            };

            const std::tuple<uint32_t, uint32_t, Float> points[4] = {
                { ix, iy, evalWeight(ix, iy) },
                { ix + 1, iy, evalWeight(ix + 1, iy) },
                { ix, iy + 1, evalWeight(ix, iy + 1) },
                { ix + 1, iy + 1, evalWeight(ix + 1, iy + 1) },
            };

            for(auto [x, y, w] : points)
                locale(x, y, 0) += w;

            uint32_t offset = 1;

            const auto writeData = [&](const Float* base, const uint32_t size) {
                for(auto [x, y, w] : points) {
                    for(uint32_t i = 0; i < size; ++i)
                        locale(x, y, i + offset) += w * base[i];
                }
                offset += size;
            };

            for(const auto channel : channels) {
                switch(channel) {
                    case Channel::Full:
                        [[fallthrough]];
                    case Channel::Direct:
                        [[fallthrough]];
                    case Channel::Indirect: {
                        if(channel != Channel::Full) {
                            PIPER_NOT_IMPLEMENTED();
                        }

                        Float base[spectrumSize(SpectrumType::Spectral)];
                        mIntegrator->estimate(ray, intersection, *mAcceleration, *mLightSampler, sampleProvider, base);
                        // TODO: convert radiance to irradiance (W/(m^2)) or energy density (J/pixel) ?
                        writeData(base, usedSpectrumSize);
                    } break;
                    case Channel::Albedo: {
                        PIPER_NOT_IMPLEMENTED();
                        writeData(nullptr, 3);
                    } break;
                    case Channel::ShadingNormal: {
                        PIPER_NOT_IMPLEMENTED();
                        writeData(nullptr, 3);
                    } break;
                    case Channel::Position: {
                        Point<FrameOfReference::World> point = ray.origin + ray.direction * Distance::fromRaw(1e5f);
                        if(const auto ptr = std::get_if<SurfaceHit>(&intersection))
                            point = ptr->hit;

                        writeData(glm::value_ptr(glm::vec3{ point.x(), point.y(), point.z() }), 3);
                    } break;
                    case Channel::Depth: {
                        Float distance = 1e5f;
                        if(const auto ptr = std::get_if<SurfaceHit>(&intersection))
                            distance = ptr->distance.raw();
                        writeData(&distance, 1);
                    } break;
                }
            }
        }
    }

    std::pmr::vector<Float> renderTile(const std::pmr ::vector<Channel>& channels, const uint32_t channelTotalSize, const int32_t x0,
                                       const int32_t y0, const int32_t x1, const int32_t y1, const SensorNDCAffineTransform& transform,
                                       const Sensor* sensor, const Ref<TileSampler>& sampler, const Float shutterTime) {
        MemoryArena arena;

        const uint32_t tileWidth = x1 - x0, tileHeight = y1 - y0;

        const auto pixelStride = channelTotalSize + 1;
        std::pmr::vector<Float> tileData{ tileWidth * tileHeight * pixelStride, context().scopedAllocator };

        const auto sampleXEnd = tileWidth - 1;
        const auto sampleYEnd = tileHeight - 1;
        const auto localSampler = sampler->clone();
        const auto sampleCount = localSampler->samples();

        std::pmr::vector<PrimaryRay> primaryRays{ context().scopedAllocator };
        RayStream stream;

        const auto prepareRay = [&](const uint32_t filmX, const uint32_t filmY, const uint32_t sampleIdx, const uint32_t rayIdx) {
            auto [sample, sampleProvider] = localSampler->generate(filmX, filmY, sampleIdx);

            auto& payload = primaryRays[rayIdx];
            payload.filmCoord = sample;
            payload.sampleProvider = std::move(sampleProvider);
            const auto sensorNDC = transform.toNDC(sample);
            const auto [ray, weight] = sensor->sample(sensorNDC, payload.sampleProvider);
            payload.weight = weight;
            stream[rayIdx] = ray;
        };

        const auto syncTile = [](uint32_t h) {
            // TODO: sync tile
        };

        const auto usedSpectrumSize = spectrumSize(RenderGlobalSetting::get().spectrumType);

        if(sampleCount * sampleXEnd > 32) {
            primaryRays.resize(sampleCount);
            stream.resize(sampleCount);

            // trace per pixel
            for(uint32_t y = 1; y <= sampleYEnd; ++y) {
                for(uint32_t x = 1; x <= sampleXEnd; ++x) {
                    const auto filmX = x0 + x, filmY = y0 + y;

                    for(uint32_t sampleIdx = 0; sampleIdx < sampleCount; ++sampleIdx)
                        prepareRay(filmX, filmY, sampleIdx, sampleIdx);

                    tracePrimary(primaryRays, stream, tileWidth, static_cast<Float>(x0), static_cast<Float>(y0), tileData.data(), channels,
                                 pixelStride, usedSpectrumSize, shutterTime);
                }
                syncTile(y);
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

                tracePrimary(primaryRays, stream, tileWidth, static_cast<Float>(x0), static_cast<Float>(y0), tileData.data(), channels,
                             pixelStride, usedSpectrumSize, shutterTime);

                syncTile(y);
            }
        }

        return tileData;
    }

    FrameGroup render(const uint32_t actionIdx, const uint32_t frameIdx) {
        constexpr uint32_t tileSize = 32;

        // TODO: sync frame

        const auto& [width, height, frameCount, sampler, begin, fps, shutterOpen, shutterClose, channelDesc, channelTotalSize, sensor,
                     transform, rect] = mActions[actionIdx];

        const auto tileX = (rect.width + tileSize - 1) / tileSize;
        const auto tileY = (rect.height + tileSize - 1) / tileSize;
        const auto shutterTime = shutterClose - shutterOpen;

        info(fmt::format("Updating scene for action {} frame {}", frameIdx));

        const TimeInterval shutterInterval{ static_cast<Float>(begin + static_cast<double>(frameIdx) / fps + shutterOpen),
                                            static_cast<Float>(begin + static_cast<double>(frameIdx) / fps + shutterClose) };

        tbb::parallel_for_each(mSceneObjects, [&](const auto& object) { object->update(shutterInterval); });

        mAcceleration->commit();

        mLightSampler->preprocess(mLights);
        mIntegrator->preprocess();

        info(fmt::format("Rendering scene for action{} frame {}", frameIdx));

        std::pmr::vector<glm::uvec2> blocks = generateSpiralTiles(tileX, tileY);

        const auto progressBase = static_cast<double>(mFrameCount - 1) / static_cast<double>(mTotalFrameCount),
                   progressIncr = 1.0 / static_cast<double>(tileX * tileY * mTotalFrameCount);

        tbb::speculative_spin_mutex mutex;

        std::uint32_t tileCount = 0;
        const auto tileSampler = sampler->prepare(frameIdx, width, height, frameCount);

        tbb::parallel_for_each(blocks, [&](const glm::uvec2 tile) {
            const auto x0 = static_cast<int32_t>(rect.left + tile.x * tileSize) - 1;
            const auto y0 = static_cast<int32_t>(rect.top + tile.y * tileSize) - 1;
            const auto x1 = std::min(static_cast<int32_t>(rect.width), static_cast<int32_t>(rect.left + (tile.x + 1) * tileSize) + 1);
            const auto y1 = std::min(static_cast<int32_t>(rect.height), static_cast<int32_t>(rect.top + (tile.y + 1) * tileSize) + 1);

            const auto res =
                renderTile(channelDesc, channelTotalSize, x0, y0, x1, y1, transform, sensor, tileSampler, static_cast<Float>(shutterTime));

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

        if(settings.variant.find("Mono"sv) != std::pmr::string::npos)
            settings.spectrumType = SpectrumType::Mono;
        else if(settings.variant.find("RGB"sv) != std::pmr::string::npos)
            settings.spectrumType = SpectrumType::LinearRGB;
        else
            settings.spectrumType = SpectrumType::Spectral;

        settings.accelerationBuilder = createEmbreeBackend();

        const auto& objects = node->get("Scene"sv)->as<ConfigAttr::AttrArray>();
        mSceneObjects.reserve(objects.size());

        std::pmr::unordered_map<std::string_view, Sensor*> sensors{ context().localAllocator };

        {
            tbb::speculative_spin_mutex mutex;

            std::pmr::vector<PrimitiveGroup*> groups{ context().localAllocator };

            tbb::parallel_for_each(objects, [&](const Ref<ConfigAttr>& attr) {
                const auto& ref = attr->as<Ref<ConfigNode>>();

                auto object = getStaticFactory().make<SceneObject>(ref);
                decltype(mutex)::scoped_lock guard{ mutex };

                if(const auto group = object->primitiveGroup())
                    groups.push_back(group);

                if(const auto light = object->light())
                    mLights.push_back(light);
                else if(auto sensor = object->sensor())
                    sensors.insert({ ref->name(), sensor });

                mSceneObjects.push_back(std::move(object));
            });

            mAcceleration = settings.accelerationBuilder->buildScene(groups);
        }

        mIntegrator = makeVariant<IntegratorBase, Integrator>(node->get("Integrator"sv)->as<Ref<ConfigNode>>());
        mLightSampler = getStaticFactory().make<LightSampler>(node->get("LightSampler"sv)->as<Ref<ConfigNode>>());
        mFilter = getStaticFactory().make<Filter>(node->get("Filter"sv)->as<Ref<ConfigNode>>());

        for(auto& action : node->get("Action"sv)->as<ConfigAttr::AttrArray>()) {
            const auto& attrs = action->as<Ref<ConfigNode>>();
            FrameAction res;
            res.width = attrs->get("Width"sv)->as<uint32_t>();
            res.height = attrs->get("Height"sv)->as<uint32_t>();
            res.frameCount = attrs->get("FrameCount"sv)->as<uint32_t>();

            res.sampler = getStaticFactory().make<Sampler>(attrs->get("Sampler"sv)->as<Ref<ConfigNode>>());

            res.begin = attrs->get("Begin"sv)->as<double>();
            res.fps = attrs->get("Height"sv)->as<double>();
            res.shutterOpen = attrs->get("ShutterOpen"sv)->as<uint32_t>();
            res.shutterClose = attrs->get("ShutterClose"sv)->as<uint32_t>();

            const auto& channels = attrs->get("Channels"sv)->as<ConfigAttr::AttrArray>();
            res.channels.reserve(channels.size());
            for(auto& channel : channels) {
                const auto channelEnum = magic_enum::enum_cast<Channel>(channel->as<std::string_view>()).value();
                const auto size = channelSize(channelEnum, RenderGlobalSetting::get().spectrumType);
                res.channels.push_back(channelEnum);
                res.channelTotalSize += size;
            }

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
    ChannelRequirement setup(ChannelRequirement req) override {
        if(!fs::exists(mCachePath) && !fs::create_directories(mCachePath))
            fatal(fmt::format("Failed to create cache directory {}.", mCachePath));

        for(auto [channel, force] : req)
            if(channel != Channel::Full && force)
                fatal("Unsupported channel");
        return {};
    }
};

PIPER_REGISTER_CLASS(Renderer, PipelineNode);

PIPER_NAMESPACE_END
