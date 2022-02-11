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
#include <Piper/Core/Sync.hpp>
#include <Piper/Render/PipelineNode.hpp>

PIPER_NAMESPACE_BEGIN

class Preview final : public PipelineNode {
public:
    explicit Preview(const Ref<ConfigNode>& node) {
        PIPER_NOT_IMPLEMENTED();
    }
    ChannelRequirement setup(const ChannelRequirement req) override {
        if(!req.empty())
            fatal("Preview is a sink node");
        return { { { Channel::Full, false } }, context().globalAllocator };
    }

    Ref<Frame> transform(Ref<Frame> frame) override {
        PIPER_NOT_IMPLEMENTED();
        return {};
    }
};

PIPER_REGISTER_CLASS(Preview, PipelineNode);

PIPER_NAMESPACE_END
