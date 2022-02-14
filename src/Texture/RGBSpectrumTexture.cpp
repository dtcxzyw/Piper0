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

// TODO: parseType
template <typename Setting>
class RGBSpectrumTexture : public ConstantTexture<Setting> {
    PIPER_IMPORT_SETTINGS();
    RGBSpectrum mSpectrum;

public:
    explicit RGBSpectrumTexture(const Ref<ConfigNode>& node)
        : mSpectrum{ RGBSpectrum::fromRaw(
              convertRGB2StandardLinearRGB(parseVec3(node->get("Value"sv)), node->get("ColorSpace"sv)->as<std::string_view>())) } {}

    Spectrum evaluate(const Wavelength& sampledWavelength) const noexcept override {
        return spectrumCast<Spectrum>(mSpectrum, sampledWavelength);
    }

    [[nodiscard]] MonoSpectrum mean() const noexcept override {
        return luminance(mSpectrum, {});
    }
};

PIPER_REGISTER_WRAPPED_VARIANT(ConstantTexture2DWrapper, RGBSpectrumTexture, Texture2D);
PIPER_REGISTER_WRAPPED_VARIANT(ConstantSphericalTextureWrapper, RGBSpectrumTexture, SphericalTexture);
PIPER_NAMESPACE_END
