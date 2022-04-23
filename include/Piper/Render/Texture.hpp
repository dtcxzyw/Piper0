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
#include <Piper/Render/Radiometry.hpp>
#include <Piper/Render/RenderGlobalSetting.hpp>
#include <Piper/Render/Sampler.hpp>

PIPER_NAMESPACE_BEGIN

struct TextureEvaluateInfo {
    TexCoord texCoord;
    Float t;
    uint32_t primitiveIdx;
    // TODO: differential
};

// TODO: prepare for time interval

class ScalarTexture2D : public RefCountBase {
public:
    virtual Float evaluate(const TextureEvaluateInfo& info) const noexcept = 0;
    virtual [[nodiscard]] std::pair<bool, Float> evaluateOneWavelength(const TextureEvaluateInfo& info, Float wavelength) const noexcept {
        return { false, evaluate(info) };
    }
};

Ref<ScalarTexture2D> getScalarTexture2D(const Ref<ConfigNode>& node, std::string_view attr, std::string_view fallbackAttr,
                                        Float defaultValue);

template <typename Setting>
class SpectrumTexture2D : public TypedRenderVariantBase<Setting> {
public:
    PIPER_IMPORT_SETTINGS();

    virtual Spectrum evaluate(const TextureEvaluateInfo& info, const Wavelength& sampledWavelength) const noexcept = 0;
    [[nodiscard]] virtual std::pair<bool, Float> evaluateOneWavelength(const TextureEvaluateInfo& info,
                                                                       Float wavelength) const noexcept = 0;
};

template <typename Setting>
struct TextureSample final {
    PIPER_IMPORT_SETTINGS();

    Direction<FrameOfReference::Object> dir;  // origin -> spherical surface
    Rational<Spectrum, PdfType::Texture> f;
    InversePdf<PdfType::Texture> inversePdf;
};

template <typename Setting>
class SphericalTexture : public TypedRenderVariantBase<Setting> {
public:
    PIPER_IMPORT_SETTINGS();

    virtual Spectrum evaluate(const TextureEvaluateInfo& info, const Wavelength& sampledWavelength) const noexcept = 0;

    [[nodiscard]] TexCoord dir2TexCoord(const Direction<FrameOfReference::Object>& dir) const noexcept {
        const auto theta = std::atan2(dir.x(), dir.z());
        const auto phi = std::acos(dir.y());
        return TexCoord{ theta * invTwoPi + 0.5f, phi * invPi };
    }

    [[nodiscard]] virtual MonoSpectrum mean() const noexcept = 0;

    virtual TextureSample<Setting> sample(SampleProvider& sampler, const Float t, const Wavelength& sampledWavelength) const noexcept {
        const auto u = sampler.sampleVec2();
        const auto phi = u.x * twoPi;
        const auto theta = std::acos(u.y * 2.0f - 1.0f);

        return TextureSample<Setting>{ Direction<FrameOfReference::Object>::fromSphericalCoord({ theta, phi }),
                                       Rational<Spectrum, PdfType::Texture>::fromRaw(
                                           evaluate({ TexCoord{ u.x, phi * invPi }, t, 0U }, sampledWavelength)),
                                       InversePdf<PdfType::Texture>::fromRaw(1.0f) };
    }
};

template <typename Setting>
class ConstantTexture : public TypedRenderVariantBase<Setting> {
    PIPER_IMPORT_SETTINGS();

public:
    virtual Spectrum evaluate(const Wavelength& sampledWavelength) const noexcept = 0;
    [[nodiscard]] virtual std::pair<bool, Float> evaluateOneWavelength(Float sampledWavelength) const noexcept = 0;
    [[nodiscard]] virtual MonoSpectrum mean() const noexcept = 0;
};

template <template <typename> typename T, typename Setting>
requires(std::is_base_of_v<ConstantTexture<Setting>, T<Setting>>) class ConstantSpectrumTexture2DWrapper final
    : public SpectrumTexture2D<Setting> {
    PIPER_IMPORT_SETTINGS();

    T<Setting> mImpl;

public:
    explicit ConstantSpectrumTexture2DWrapper(const Ref<ConfigNode>& node) : mImpl{ node } {}

    Spectrum evaluate(const TextureEvaluateInfo&, const Wavelength& sampledWavelength) const noexcept override {
        return mImpl.evaluate(sampledWavelength);
    }

    [[nodiscard]] std::pair<bool, Float> evaluateOneWavelength(const TextureEvaluateInfo&, const Float wavelength) const noexcept override {
        return mImpl.evaluateOneWavelength(wavelength);
    }
};

template <template <typename> typename T, typename Setting>
requires(std::is_base_of_v<ConstantTexture<Setting>, T<Setting>>) class ConstantSphericalTextureWrapper final
    : public SphericalTexture<Setting> {
    PIPER_IMPORT_SETTINGS();

    T<Setting> mImpl;

public:
    explicit ConstantSphericalTextureWrapper(const Ref<ConfigNode>& node) : mImpl{ node } {}

    Spectrum evaluate(const TextureEvaluateInfo&, const Wavelength& sampledWavelength) const noexcept override {
        return mImpl.evaluate(sampledWavelength);
    }

    [[nodiscard]] MonoSpectrum mean() const noexcept override {
        return mImpl.mean();
    }
};

class NormalizedTexture2D : public RefCountBase {
public:
    [[nodiscard]] virtual Direction<FrameOfReference::Shading> evaluate(const TextureEvaluateInfo& info) const noexcept = 0;
};

PIPER_NAMESPACE_END
