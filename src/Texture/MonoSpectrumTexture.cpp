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

class MonoSpectrumTextureScalar final : public ScalarTexture2D {
    MonoSpectrum mValue;

public:
    explicit MonoSpectrumTextureScalar(const Ref<ConfigNode>& node) : mValue{ node->get("Value"sv)->as<Float>() } {}
    explicit MonoSpectrumTextureScalar(const Float value) : mValue{ value } {}

    Float evaluate(TexCoord) const noexcept override {
        return mValue;
    }
};

PIPER_REGISTER_CLASS_IMPL("MonoSpectrumTexture", MonoSpectrumTextureScalar, ScalarTexture2D, MonoSpectrumTextureScalar);

Ref<ScalarTexture2D> getScalarTexture2D(const Ref<ConfigNode>& node, const std::string_view attr, const std::string_view fallbackAttr,
                                        const Float defaultValue) {
    constexpr auto parseAttr = [](const Ref<ConfigAttr>& sub) {
        if(sub->convertibleTo<Float>())
            return makeRefCount<MonoSpectrumTextureScalar, ScalarTexture2D>(sub->as<Float>());
        return getStaticFactory().make<ScalarTexture2D>(sub->as<Ref<ConfigNode>>());
    };

    if(const auto ptr = node->tryGet(attr))
        return parseAttr(*ptr);

    if(!fallbackAttr.empty())
        if(const auto ptr = node->tryGet(fallbackAttr))
            return parseAttr(*ptr);
    return makeRefCount<MonoSpectrumTextureScalar, ScalarTexture2D>(defaultValue);
}

template <typename Setting>
class MonoSpectrumTexture final : public ConstantTexture<Setting> {
    PIPER_IMPORT_SETTINGS();
    MonoSpectrum mMean;
    Spectrum mSpectrum;

public:
    explicit MonoSpectrumTexture(const Ref<ConfigNode>& node)
        : mMean{ node->get("Value"sv)->as<Float>() }, mSpectrum{ spectrumCast<Spectrum>(mMean, std::monostate{}) } {}

    Spectrum evaluate(const Wavelength&) const noexcept override {
        return mSpectrum;
    }

    [[nodiscard]] std::pair<bool, Float> evaluateOneWavelength(Float) const noexcept override {
        return { false, mMean };
    }

    [[nodiscard]] MonoSpectrum mean() const noexcept override {
        return mMean;
    }
};

PIPER_REGISTER_WRAPPED_VARIANT(ConstantSpectrumTexture2DWrapper, MonoSpectrumTexture, SpectrumTexture2D);
PIPER_REGISTER_WRAPPED_VARIANT(ConstantSphericalTextureWrapper, MonoSpectrumTexture, SphericalTexture);
PIPER_NAMESPACE_END
