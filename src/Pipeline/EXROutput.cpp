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

#include <OpenEXR/ImfRgbaFile.h>
#include <Piper/Core/StaticFactory.hpp>
#include <Piper/Render/PipelineNode.hpp>
#include <magic_enum.hpp>
#include <oneapi/tbb/parallel_for.h>

PIPER_NAMESPACE_BEGIN

class EXROutput final : public PipelineNode {
    std::pmr::string mOutputPath;

public:
    explicit EXROutput(const Ref<ConfigNode>& node)
        : mOutputPath{ node->get("OutputPath"sv)->as<std::string_view>(), context().globalAllocator } {}
    ChannelRequirement setup(const ChannelRequirement req) override {
        if(!req.empty())
            fatal("EXROutput is a sink node");
        return { { { Channel::Color, false } }, context().globalAllocator };
    }

    Ref<Frame> transform(const Ref<Frame> frame) override {
        MemoryArena arena;
        const auto& metadata = frame->metadata();
        uint32_t stride = 0;

        const auto frameIdx = std::to_string(metadata.frameIdx);
        const auto actionIdx = std::to_string(metadata.actionIdx);
        ResolveConfiguration pathResolver{ context().scopedAllocator };
        pathResolver["${FrameIdx}"] = frameIdx;
        pathResolver["${ActionIdx}"] = actionIdx;

        for(const auto channel : metadata.channels) {
            pathResolver["${Channel}"] = magic_enum::enum_name(channel);

            if(channel != Channel::Color) {
                info(std::format("Channel {} is skipped by EXR output node.", magic_enum::enum_name(channel)));
                stride += channelSize(channel, metadata.spectrumType);
                continue;
            }

            if(!metadata.isHDR)
                fatal("LDR images are not supported by EXR output node.");

            const auto pixelCount = metadata.width * metadata.height;
            std::pmr::vector<Imf::Rgba> buffer{ pixelCount, context().scopedAllocator };
            const auto base = frame->data().data() + static_cast<size_t>(stride);

            if(metadata.spectrumType == SpectrumType::LinearRGB) {
                tbb::parallel_for(
                    tbb::blocked_range<uint32_t>{ 0, pixelCount },
                    [&](const tbb::blocked_range<uint32_t>& range) {
                        for(auto idx = range.begin(); idx != range.end(); ++idx) {
                            const auto src = base + static_cast<size_t>(idx * metadata.pixelStride);
                            buffer[idx] = Imf::Rgba{ src[0], src[1], src[2] };
                        }
                    },
                    globalAffinityPartitioner);
            } else {
                tbb::parallel_for(
                    tbb::blocked_range<uint32_t>{ 0, pixelCount },
                    [&](const tbb::blocked_range<uint32_t>& range) {
                        for(auto idx = range.begin(); idx != range.end(); ++idx) {
                            const auto src = base + static_cast<size_t>(idx * metadata.pixelStride);
                            buffer[idx] = Imf::Rgba{ src[0], src[0], src[0] };
                        }
                    },
                    globalAffinityPartitioner);
            }

            const auto fileName = resolveString(mOutputPath, pathResolver);
            Imf::RgbaOutputFile file{ fileName.c_str(), static_cast<int32_t>(metadata.width), static_cast<int32_t>(metadata.height),
                                      metadata.spectrumType == SpectrumType::LinearRGB ? Imf::WRITE_RGB : Imf::WRITE_Y };
            file.setFrameBuffer(buffer.data(), 1, metadata.width);
            file.writePixels(static_cast<int32_t>(metadata.height));

            stride += channelSize(channel, metadata.spectrumType);
        }

        return {};
    }
};

PIPER_REGISTER_CLASS(EXROutput, PipelineNode);

PIPER_NAMESPACE_END
