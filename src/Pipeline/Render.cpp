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
#include <Piper/Render/RenderGlobalSetting.hpp>
#include <Piper/Render/SceneObject.hpp>
#include <magic_enum.hpp>
#include <unordered_set>

PIPER_NAMESPACE_BEGIN

struct FrameAction final {
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t frameCount = 0;
    double begin = 0.0;
    double fps = 0.0;
    double shutterOpen = 0.0;
    double shutterClose = 0.0;
    Channel channels;

    SpectrumType spectrumType = SpectrumType::Mono;
};

class Renderer final : public SourceNode {
    ChannelRequirement mRequirement;
    std::pmr::string mCachePath;
    std::pmr::vector<Ref<SceneObject>> mSceneObjects;
    std::pmr::vector<FrameAction> mActions;

public:
    explicit Renderer(const Ref<ConfigNode>& node) {

        for(auto& object : node->get("Scene"sv)->as<ConfigAttr::AttrArray>()) {
        }

        for(auto& action : node->get("Action"sv)->as<ConfigAttr::AttrArray>()) {
            const auto& attrs = action->as<Ref<ConfigNode>>();
            FrameAction res;
            res.width = attrs->get("Width"sv)->as<uint32_t>();
            res.height = attrs->get("Height"sv)->as<uint32_t>();
            res.frameCount = attrs->get("FrameCount"sv)->as<uint32_t>();
            res.begin = attrs->get("Begin"sv)->as<double>();
            res.fps = attrs->get("Height"sv)->as<double>();
            res.shutterOpen = attrs->get("ShutterOpen"sv)->as<uint32_t>();
            res.shutterClose = attrs->get("ShutterClose"sv)->as<uint32_t>();
            for(auto& channel : attrs->get("Channels"sv)->as<ConfigAttr::AttrArray>())
                res.channels = res.channels | magic_enum::enum_cast<Channel>(channel->as<std::string_view>()).value();

            if(static_cast<bool>(res.channels & (Channel::Full | Channel::Direct | Channel::Indirect | Channel::Albedo))) {
                res.spectrumType = magic_enum::enum_cast<SpectrumType>(attrs->get("SpectrumType")->as<std::string_view>()).value();
                // TODO: other render global settings
            }

            mActions.push_back(res);
        }
    }
    FrameGroup transform(FrameGroup) override {
        PIPER_NOT_IMPLEMENTED();
    }
    uint32_t frameCount() override {
        uint32_t sum = 0;
        for(const auto& action : mActions)
            sum += action.frameCount;
        return sum;
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
