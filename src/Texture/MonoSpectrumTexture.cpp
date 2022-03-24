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

template <typename Setting>
class MonoSpectrumTexture : public ConstantTexture<Setting> {
    PIPER_IMPORT_SETTINGS();
    MonoSpectrum mMean;
    Spectrum mSpectrum;

public:
    explicit MonoSpectrumTexture(const Ref<ConfigNode>& node)
        : mMean{ node->get("Value"sv)->as<Float>() }, mSpectrum{ spectrumCast<Spectrum>(mMean, std::monostate{}) } {}

    Spectrum evaluate(const Wavelength&) const noexcept override {
        return mSpectrum;
    }

    [[nodiscard]] MonoSpectrum mean() const noexcept override {
        return mMean;
    }
};

PIPER_REGISTER_WRAPPED_VARIANT(ConstantTexture2DWrapper, MonoSpectrumTexture, Texture2D);
PIPER_REGISTER_WRAPPED_VARIANT(ConstantSphericalTextureWrapper, MonoSpectrumTexture, SphericalTexture);
PIPER_NAMESPACE_END
