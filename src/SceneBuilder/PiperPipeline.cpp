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
#include <Piper/Render/Pipeline.hpp>
#include <Piper/Render/PipelineNode.hpp>
#include <oneapi/tbb/flow_graph.h>
#include <ranges>

PIPER_NAMESPACE_BEGIN

class PiperPipeline final : public Pipeline {
    static constexpr auto noPrevNode = std::numeric_limits<uint32_t>::max();
    std::pmr::vector<std::pair<Ref<PipelineNode>, uint32_t>> mNodes{ context().localAllocator };

public:
    explicit PiperPipeline(const Ref<ConfigNode>& config) {
        MemoryArena arena;

        const auto path = config->get("InputFile"sv)->as<std::string_view>();
        // TODO: load configuration from CLI
        LoadConfiguration cfg{ context().globalAllocator };
        const auto base = fs::path{ path }.parent_path().string();
        cfg.insert({ "${BaseDir}"sv, base });

        const auto pipelineDesc = parseJSONConfigNode(path, cfg);
        const auto pipeline = pipelineDesc->get("Pipeline"sv);
        const auto nodes = pipeline->as<ConfigAttr::AttrArray>();
        mNodes.reserve(nodes.size());

        std::pmr::unordered_map<std::string_view, uint32_t> nodeMapping{ context().scopedAllocator };

        uint32_t idx = 0;
        for(auto& node : nodes) {
            const auto& desc = node->as<Ref<ConfigNode>>();
            auto prevNode = noPrevNode;
            if(auto prev = desc->tryGet("PrevNode"sv))
                prevNode = nodeMapping.find((*prev)->as<std::string_view>())->second;

            auto pipelineNode = getStaticFactory().make<PipelineNode>(desc);
            nodeMapping.insert({ desc->name(), idx });
            mNodes.push_back({ std::move(pipelineNode), prevNode });
            ++idx;
        }
    }

    void execute(const std::pmr::string& outputDir) override {
        {
            MemoryArena arena;

            std::pmr::vector<PipelineNode::ChannelRequirement> requirements(
                mNodes.size(), PipelineNode::ChannelRequirement{ context().globalAllocator }, context().scopedAllocator);

            for(int32_t idx = static_cast<int32_t>(mNodes.size()) - 1; idx >= 0; --idx) {
                const auto& [node, prev] = mNodes[idx];
                auto req = node->setup(outputDir, requirements[idx]);
                if(prev != noPrevNode)
                    mergeRequirement(requirements[prev], std::move(req));
            }

            if(!requirements.front().empty() || mNodes.front().second != noPrevNode)
                fatal("No pipeline source");
        }

        const auto frameCount = dynamic_cast<SourceNode*>(mNodes.front().first.get())->frameCount();

        tbb::flow::graph g;

        std::pmr::vector<tbb::flow::function_node<FrameGroup, FrameGroup>> nodes{ context().localAllocator };
        nodes.reserve(mNodes.size());
        for(auto& [node, prevIdx] : mNodes) {
            nodes.push_back({ g, 1, [&](FrameGroup frames) { return node->transform(std::move(frames)); } });
            auto& inserted = nodes.back();
            if(prevIdx != noPrevNode)
                tbb::flow::make_edge(nodes[prevIdx], inserted);
        }

        for(uint32_t idx = 0; idx < frameCount; ++idx)
            nodes.front().try_put({});

        g.wait_for_all();
    }
};

PIPER_REGISTER_CLASS(PiperPipeline, Pipeline);

PIPER_NAMESPACE_END
