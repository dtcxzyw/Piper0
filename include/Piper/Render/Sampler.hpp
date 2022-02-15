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
#include <Piper/Render/Random.hpp>

PIPER_NAMESPACE_BEGIN

class SampleProvider final {
    std::pmr::vector<Float> mGeneratedSamples;
    uint32_t mIndex = 0;
    RandomEngine mFallback;

public:
    SampleProvider() = default;
    ~SampleProvider() = default;
    explicit SampleProvider(std::pmr::vector<Float> samples, const uint64_t seed)
        : mGeneratedSamples{ std::move(samples) }, mFallback{ seeding(seed) } {
        if(mGeneratedSamples.empty())
            mGeneratedSamples = std::pmr::vector<Float>(1, context().scopedAllocator);
    }
    SampleProvider(SampleProvider&&) = default;
    SampleProvider& operator=(SampleProvider&&) = default;
    SampleProvider(const SampleProvider&) = delete;
    SampleProvider& operator=(const SampleProvider&) = delete;

    Float sample() noexcept {
        if(mIndex == mGeneratedSamples.size())
            return Piper::sample(mFallback);
        return mGeneratedSamples[mIndex++];
    }

    void reuse(const Float u) noexcept {
        mGeneratedSamples[--mIndex] = u;
    }

    uint32_t sampleIdx(const uint32_t size) noexcept {
        const auto u = sample();
        const auto scaled = u * static_cast<Float>(size);
        const auto idx = std::min(size - 1, static_cast<uint32_t>(scaled));
        reuse(scaled - idx);
        return idx;
    }

    glm::vec2 sampleVec2() noexcept {
        if(mIndex + 2 <= mGeneratedSamples.size()) {
            const glm::vec2 res = { mGeneratedSamples[mIndex], mGeneratedSamples[mIndex + 1] };
            mIndex += 2;
            return res;
        }
        return { sample(), sample() };
    }

    glm::vec4 sampleVec4() noexcept {
        if(mIndex + 4 <= mGeneratedSamples.size()) {
            const glm::vec4 res = { mGeneratedSamples[mIndex], mGeneratedSamples[mIndex + 1], mGeneratedSamples[mIndex + 2],
                                    mGeneratedSamples[mIndex + 3] };
            mIndex += 4;
            return res;
        }
        return { sample(), sample(), sample(), sample() };
    }
};

class TileSampler : public RefCountBase {
public:
    virtual uint32_t samples() const noexcept = 0;
    virtual std::pair<glm::vec2, SampleProvider> generate(uint32_t filmX, uint32_t filmY, uint32_t sampleIdx) const = 0;
    virtual Ref<TileSampler> clone() const = 0;
};

class Sampler : public RefCountBase {
public:
    virtual Ref<TileSampler> prepare(uint32_t frameIdx, uint32_t width, uint32_t height, uint32_t frameCount) = 0;
};

PIPER_NAMESPACE_END
