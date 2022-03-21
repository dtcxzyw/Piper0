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

#include <Piper/Render/Texture.hpp>

PIPER_NAMESPACE_BEGIN

SampledSpectrum temperatureToSpectrum(Float temperature, const SampledSpectrum& sampledWavelength) noexcept;

template <typename Setting>
class BlackBody final : public ConstantTexture<Setting> {
    PIPER_IMPORT_SETTINGS();
    Float mTemperature;
    Float mScale;

public:
    explicit BlackBody(const Ref<ConfigNode>& node)
        : mTemperature{ static_cast<Float>(node->get("Temperature"sv)->as<double>()) }, mScale{ static_cast<Float>(
                                                                                            node->get("Scale"sv)->as<double>()) } {}

    Spectrum evaluate(const Wavelength& sampledWavelength) const noexcept override {
        return temperatureToSpectrum(mTemperature, sampledWavelength) * mScale;
    }

    [[nodiscard]] MonoSpectrum mean() const noexcept override {
        // TODO
        return mScale;
    }
};

// TODO: other variant
PIPER_REGISTER_WRAPPED_VARIANT_IMPL(ConstantTexture2DWrapper, BlackBody, Texture2D, RSSSpectral);
PIPER_REGISTER_WRAPPED_VARIANT_IMPL(ConstantSphericalTextureWrapper, BlackBody, SphericalTexture, RSSSpectral);

PIPER_NAMESPACE_END
