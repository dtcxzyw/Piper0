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

#include <Piper/Core/Report.hpp>
#include <Piper/Render/Frame.hpp>
#include <Piper/Render/PipelineNode.hpp>

PIPER_NAMESPACE_BEGIN

void mergeRequirement(PipelineNode::ChannelRequirement& lhs, PipelineNode::ChannelRequirement rhs) {
    if(lhs.empty()) {
        lhs = std::move(rhs);
        return;
    }
    for(auto [channel, required] : rhs)
        if(!lhs.contains(channel) || required)
            lhs[channel] = required;
}

ChannelInfo FrameMetadata::view(const Channel channel) const {
    uint32_t stride = 0;
    for(const auto c : channels) {
        if(channel == c)
            break;
        stride += channelSize(c, spectrumType);
    }

    if(stride == pixelStride)
        fatal("Required channel doesn't exist.");
    constexpr uint32_t scalarSize = sizeof(Float);
    return ChannelInfo{ stride * scalarSize, pixelStride * scalarSize, pixelStride * width * scalarSize };
}

PIPER_NAMESPACE_END
