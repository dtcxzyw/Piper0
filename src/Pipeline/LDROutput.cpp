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
#include <Piper/Render/PipelineNode.hpp>

PIPER_NAMESPACE_BEGIN

class LDROutput final : public PipelineNode {
    std::pmr::string mOutputPath;

public:
    explicit LDROutput(const Ref<ConfigNode>& node)
        : mOutputPath{ node->get("OutputPath"sv)->as<std::string_view>(), context().globalAllocator } {}
    ChannelRequirement setup(const ChannelRequirement req) override {
        if(!req.empty())
            fatal("LDROutput is a sink node");
        return { { { Channel::Full, false } }, context().globalAllocator };
    }

    FrameGroup transform(FrameGroup group) override {
        PIPER_NOT_IMPLEMENTED();
        return {};
    }
};

PIPER_REGISTER_CLASS(LDROutput, PipelineNode);

PIPER_NAMESPACE_END
