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

#include <Piper/Render/SpectrumUtil.hpp>
#include <Piper/Render/Texture.hpp>

PIPER_NAMESPACE_BEGIN

SampledSpectrum temperatureToSpectrum(Float temperature, const SampledSpectrum& sampledWavelength) noexcept;
RGBSpectrum temperatureToSpectrum(Float temperature) noexcept;

template <typename Setting>
class BlackBody final : public ConstantTexture<Setting> {
    PIPER_IMPORT_SETTINGS();
    Float mTemperature;
    Float mScale;
    Float mMean;

    std::optional<Spectrum> mCached;

public:
    explicit BlackBody(const Ref<ConfigNode>& node)
        : mTemperature{ node->get("Temperature"sv)->as<Float>() }, mScale{ node->get("Scale"sv)->as<Float>() } {
        mMean = luminance(temperatureToSpectrum(mTemperature), std::monostate{});

        if constexpr(!std::is_same_v<Spectrum, SampledSpectrum>)
            mCached = spectrumCast<Spectrum>(temperatureToSpectrum(mTemperature), Wavelength{}) * mScale;
    }

    Spectrum evaluate(const Wavelength& sampledWavelength) const noexcept override {
        if constexpr(std::is_same_v<Spectrum, SampledSpectrum>)
            return spectrumCast<Spectrum>(temperatureToSpectrum(mTemperature, sampledWavelength), sampledWavelength) * mScale;
        else
            return mCached.value();
    }

    [[nodiscard]] MonoSpectrum mean() const noexcept override {
        return mMean;
    }
};

PIPER_REGISTER_WRAPPED_VARIANT(ConstantSphericalTextureWrapper, BlackBody, SphericalTexture);

PIPER_NAMESPACE_END
