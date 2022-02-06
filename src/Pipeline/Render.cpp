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

class Renderer final : public SourceNode {
    ChannelRequirement mRequirement;
    std::pmr::string mCachePath;

public:
    explicit Renderer(const Ref<ConfigNode>& node) {
        PIPER_NOT_IMPLEMENTED();
    }
    FrameGroup transform(FrameGroup) override {
        PIPER_NOT_IMPLEMENTED();
    }
    uint32_t frameCount() override {
        return 0;
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
