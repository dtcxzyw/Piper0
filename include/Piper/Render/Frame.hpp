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
    Full,
    Direct,
    Indirect,
    Albedo,
    ShadingNormal,
    Position,
    Depth,
};

constexpr size_t channelSize(const Channel x, const SpectrumType spectrumType) {
    switch(x) {
        case Channel::Full:
            [[fallthrough]];
        case Channel::Direct:
            [[fallthrough]];
        case Channel::Indirect:
            return spectrumSize(spectrumType);

        case Channel::Albedo:
            [[fallthrough]];
        case Channel::ShadingNormal:
            [[fallthrough]];
        case Channel::Position:
            return 3;

        default:
            return 1;
    }
}

struct FrameMetadata final {
    uint32_t width;
    uint32_t height;
    uint32_t actionIdx;
    uint32_t frameIdx;
    std::pmr::vector<Channel> channels;
    uint32_t pixelStride;
    SpectrumType spectrumType;
    bool isHDR;
};

class Frame final : public RefCountBase {
    FrameMetadata mMetadata;
    std::pmr::vector<Float> mData;

public:
    Frame(const FrameMetadata metadata, std::pmr::vector<Float> data) : mMetadata{ std::move(metadata) }, mData{ std::move(data) } {}
    const FrameMetadata& metadata() const noexcept {
        return mMetadata;
    }

    const Float* data() const noexcept {
        return mData.data();
    }
};

/*
template <typename T>
class FrameView final {
    Ref<Frame> mFrame;

public:
    const T& operator()(const uint32_t i, const uint32_t j) {
        return *reinterpret_cast<const T*>(mFrame->at(i, j));
    }
};
*/

PIPER_NAMESPACE_END
