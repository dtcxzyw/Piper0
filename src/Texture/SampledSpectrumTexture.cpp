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

#include <Piper/Render/ColorSpace.hpp>
#include <Piper/Render/SpectrumUtil.hpp>
#include <Piper/Render/Texture.hpp>

PIPER_NAMESPACE_BEGIN

// FIXME: only for IOR LUT now
template <typename Setting>
class SampledSpectrumTexture : public ConstantTexture<Setting> {
    PIPER_IMPORT_SETTINGS();
    std::pmr::vector<glm::vec2> mLUT{ context().globalAllocator };  // wavelength, measurement
    Float mMean = 0.0f;

public:
    explicit SampledSpectrumTexture(const Ref<ConfigNode>& node) {
        auto& arr = node->get("Array"sv)->as<ConfigAttr::AttrArray>();
        mLUT.reserve(arr.size());
        for(auto& item : arr)
            mLUT.push_back(parseVec2(item));
        std::sort(mLUT.begin(), mLUT.end(), [](const glm::vec2 lhs, const glm::vec2 rhs) { return lhs.x < rhs.x; });

        for(uint32_t idx = 0; idx < mLUT.size(); ++idx)
            mMean += mLUT[idx].y;
        mMean /= static_cast<Float>(mLUT.size());
    }

    Spectrum evaluate(const Wavelength& sampledWavelength) const noexcept override {
        if constexpr(std::is_same_v<Spectrum, SampledSpectrum>) {

            const auto evaluate = [&](Float lambda) {
                const auto idx = static_cast<int32_t>(
                    std::lower_bound(mLUT.cbegin(), mLUT.cend(), lambda, [](const glm::vec2 lhs, const Float rhs) { return lhs.x < rhs; }) -
                    mLUT.cbegin());
                const auto idx1 = std::clamp(idx, 0, static_cast<int32_t>(mLUT.size()) - 1);
                const auto idx0 = std::max(idx1 - 1, 0);
                if(idx0 == idx1)
                    return mLUT[idx0].y;
                return glm::mix(mLUT[idx0].y, mLUT[idx1].y, (lambda - mLUT[idx0].x) / (mLUT[idx1].x - mLUT[idx0].x));
            };

            const auto lambdas = sampledWavelength.raw();

            SampledSpectrum::VecType measurement;

            for(int32_t idx = 0; idx < SampledSpectrum::nSamples; ++idx)
                measurement[idx] = evaluate(lambdas[idx]);

            return Spectrum::fromRaw(measurement);
        } else
            return spectrumCast<Spectrum>(mMean, std::monostate{});
    }

    [[nodiscard]] MonoSpectrum mean() const noexcept override {
        return mMean;
    }
};

PIPER_REGISTER_WRAPPED_VARIANT(ConstantTexture2DWrapper, SampledSpectrumTexture, Texture2D);
PIPER_NAMESPACE_END
