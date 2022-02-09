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

PIPER_NAMESPACE_BEGIN

class EXROutput final : public PipelineNode {
    std::pmr::string mOutputPath;

public:
    explicit EXROutput(const Ref<ConfigNode>& node)
        : mOutputPath{ node->get("OutputPath"sv)->as<std::string_view>(), context().globalAllocator } {}
    ChannelRequirement setup(const ChannelRequirement req) override {
        if(!req.empty())
            fatal("EXROutput is a sink node");
        return { { { Channel::Full, false } }, context().globalAllocator };
    }

    FrameGroup transform(FrameGroup group) override {
        for(const auto& [channel, frame] : group) {
            const auto& metadata = frame->metadata();
            if(channel == Channel::Full) {
                if(!metadata.isHDR)
                    fatal("LDR images are not supported by EXR output node.");

                const auto fileName = fmt::format("{}/action{}/frame{}.exr", mOutputPath, metadata.actionIdx, metadata.frameIdx);
                Imf::RgbaOutputFile file{ fileName.c_str(), static_cast<int32_t>(metadata.width), static_cast<int32_t>(metadata.height),
                                          Imf::WRITE_RGB };

                std::pmr::vector<Imf::Rgba> buffer{ metadata.width * metadata.height, context().localAllocator };

                // TODO: fill buffer

                file.setFrameBuffer(buffer.data(), 1, metadata.width);
                file.writePixels(static_cast<int32_t>(metadata.height));

            } else {
                PIPER_NOT_IMPLEMENTED();
            }
        }

        return {};
    }
};

PIPER_REGISTER_CLASS(EXROutput, PipelineNode);

PIPER_NAMESPACE_END
