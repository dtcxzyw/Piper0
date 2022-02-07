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
    Full = 1 << 0,
    Direct = 1 << 1,
    Indirect = 1 << 2,
    Albedo = 1 << 3,
    ShadingNormal = 1 << 4,
    Position = 1 << 5,
    Depth = 1 << 6,
    Variance = 1 << 7
};

constexpr Channel operator|(Channel a, Channel b) {
    return static_cast<Channel>(static_cast<uint16_t>(a) | static_cast<uint16_t>(b));
}

constexpr Channel operator&(Channel a, Channel b) {
    return static_cast<Channel>(static_cast<uint16_t>(a) & static_cast<uint16_t>(b));
}

struct FrameMetadata final {
    uint32_t width;
    uint32_t height;
    uint32_t frameIdx;
    Channel channel;
    SpectrumType spectrumType;
    bool isHDR;
};

class Frame final : public RefCountBase {
    FrameMetadata mMetadata;
    uint32_t mChannelStride;

    std::pmr::vector<float> mData;

public:
    Frame(const FrameMetadata& metadata, const uint32_t channelStride, std::pmr::vector<float> data)
        : mMetadata{ metadata }, mChannelStride{ channelStride }, mData{ std::move(data) } {}
    const FrameMetadata& metadata() const noexcept {
        return mMetadata;
    }

    const float* at(const uint32_t i, const uint32_t j) const {
        return mData.data() + (i * mMetadata.width + j) * mChannelStride;
    }
};

using FrameGroup = std::pmr::unordered_map<Channel, Ref<Frame>>;

template <typename T>
class FrameView final {
    Ref<Frame> mFrame;

public:
    const T& operator()(const uint32_t i, const uint32_t j) {
        return *reinterpret_cast<const T*>(mFrame->at(i, j));
    }
};

PIPER_NAMESPACE_END
