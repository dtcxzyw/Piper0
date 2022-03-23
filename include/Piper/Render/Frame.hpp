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

#pragma once
#include <Piper/Core/RefCount.hpp>
#include <Piper/Render/Spectrum.hpp>
#include <vector>

PIPER_NAMESPACE_BEGIN

enum class Channel : uint16_t {
    Color,
    Albedo,
    ShadingNormal,
    Position,
    Depth,
};

constexpr size_t channelSize(const Channel x, const SpectrumType spectrumType) {
    switch(x) {
        case Channel::Color:
            [[fallthrough]];
        case Channel::Albedo:
            return spectrumSize(spectrumType);

        case Channel::ShadingNormal:
            [[fallthrough]];
        case Channel::Position:
            return 3;

        default:
            return 1;
    }
}

struct ChannelInfo final {
    uint32_t byteStride;
    uint32_t pixelStride;
    uint32_t rowStride;
};

struct FrameMetadata final {
    uint32_t width;
    uint32_t height;
    uint32_t actionIdx;
    uint32_t frameIdx;
    std::pmr::vector<Channel> channels;
    uint32_t pixelStride;
    SpectrumType spectrumType;
    bool isHDR;

    [[nodiscard]] ChannelInfo view(Channel channel) const;
};

class Frame final : public RefCountBase {
    FrameMetadata mMetadata;
    std::pmr::vector<Float> mData;

public:
    Frame(FrameMetadata metadata, std::pmr::vector<Float> data) : mMetadata{ std::move(metadata) }, mData{ std::move(data) } {}

    const FrameMetadata& metadata() const noexcept {
        return mMetadata;
    }

    const std::pmr::vector<Float>& data() const noexcept {
        return mData;
    }
};

PIPER_NAMESPACE_END
