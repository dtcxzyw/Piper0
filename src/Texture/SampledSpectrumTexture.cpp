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

#include <Piper/Render/ColorMatchingFunction.hpp>
#include <Piper/Render/ColorSpace.hpp>
#include <Piper/Render/Texture.hpp>

PIPER_NAMESPACE_BEGIN

class SampledSpectrumTextureScalar final : public ScalarTexture2D {
    std::pmr::vector<glm::vec2> mLUT{ context().globalAllocator };  // wavelength, measurement
    Float mMean = 0.0f;

public:
    explicit SampledSpectrumTextureScalar(const Ref<ConfigNode>& node) {
        auto& arr = node->get("Array"sv)->as<ConfigAttr::AttrArray>();
        mLUT.reserve(arr.size());
        for(auto& item : arr)
            mLUT.push_back(parseVec2(item));
        std::ranges::sort(mLUT, [](const glm::vec2 lhs, const glm::vec2 rhs) { return lhs.x < rhs.x; });

        // TODO: use numerical integration?
        for(const auto& item : mLUT)
            mMean += item.y;
        mMean /= static_cast<Float>(mLUT.size());
    }

    [[nodiscard]] std::pair<bool, Float> evaluateOneWavelength(const TextureEvaluateInfo&, const Float wavelength) const noexcept override {
        const auto idx = static_cast<int32_t>(
            std::lower_bound(mLUT.cbegin(), mLUT.cend(), wavelength, [](const glm::vec2 lhs, const Float rhs) { return lhs.x < rhs; }) -
            mLUT.cbegin());
        const auto idx1 = std::clamp(idx, 0, static_cast<int32_t>(mLUT.size()) - 1);
        const auto idx0 = std::max(idx1 - 1, 0);
        if(idx0 == idx1)
            return { false, mLUT[idx0].y };
        return { true, glm::mix(mLUT[idx0].y, mLUT[idx1].y, (wavelength - mLUT[idx0].x) / (mLUT[idx1].x - mLUT[idx0].x)) };
    }

    Float evaluate(const TextureEvaluateInfo&) const noexcept override {
        return mMean;
    }
};

PIPER_REGISTER_CLASS_IMPL("SampledSpectrumTexture", SampledSpectrumTextureScalar, ScalarTexture2D, SampledSpectrumTextureScalar);

template <typename Setting>
class SampledSpectrumTexture final : public ConstantTexture<Setting> {
    PIPER_IMPORT_SETTINGS();
    SampledSpectrumTextureScalar mImpl;
    RGBSpectrum mRGBSpectrum = zero<RGBSpectrum>();

public:
    explicit SampledSpectrumTexture(const Ref<ConfigNode>& node) : mImpl{ node } {
        if constexpr(std::is_same_v<Spectrum, RGBSpectrum>) {
            for(uint32_t idx = wavelengthMin; idx < wavelengthMax; ++idx) {
                const auto lambda = static_cast<Float>(idx);
                mRGBSpectrum += RGBSpectrum::fromRaw(
                    RGBSpectrum::matXYZ2RGB *
                    (mImpl.evaluateOneWavelength({}, lambda).second * glm::vec3{ wavelength2XYZ(static_cast<double>(lambda)) }));
            }

            mRGBSpectrum /= wavelengthMax - wavelengthMin;
        }
    }

    Spectrum evaluate(const Wavelength& sampledWavelength) const noexcept override {
        if constexpr(std::is_same_v<Spectrum, SampledSpectrum>) {
            const auto lambdas = sampledWavelength.raw();

            SampledSpectrum::VecType measurement;

            for(int32_t idx = 0; idx < SampledSpectrum::nSamples; ++idx)
                measurement[idx] = mImpl.evaluateOneWavelength({}, lambdas[idx]).second;

            return Spectrum::fromRaw(measurement);
        } else if constexpr(std::is_same_v<Spectrum, MonoSpectrum>) {
            return mImpl.evaluate({});
        } else if constexpr(std::is_same_v<Spectrum, RGBSpectrum>) {
            return mRGBSpectrum;
        } else {
            return Spectrum::fromRaw(mImpl.evaluateOneWavelength({}, sampledWavelength.raw()).second);
        }
    }

    std::pair<bool, Float> evaluateOneWavelength(const Float sampledWavelength) const noexcept override {
        return mImpl.evaluateOneWavelength({}, sampledWavelength);
    }

    [[nodiscard]] MonoSpectrum mean() const noexcept override {
        return mImpl.evaluate({});
    }
};

PIPER_REGISTER_WRAPPED_VARIANT(ConstantSpectrumTexture2DWrapper, SampledSpectrumTexture, SpectrumTexture2D);
PIPER_REGISTER_WRAPPED_VARIANT(ConstantSphericalTextureWrapper, SampledSpectrumTexture, SphericalTexture);

PIPER_NAMESPACE_END
