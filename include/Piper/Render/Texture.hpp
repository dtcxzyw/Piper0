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
#include <Piper/Render/SamplingUtil.hpp>

PIPER_NAMESPACE_BEGIN

// TODO: differential

template <typename Setting>
class Texture2D : public TypedRenderVariantBase<Setting> {
public:
    PIPER_IMPORT_SETTINGS();

    virtual Spectrum evaluate(TexCoord texCoord, const Wavelength& sampledWavelength) const noexcept = 0;
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

    virtual void prepareForSampling() {}

    virtual Spectrum evaluate(TexCoord texCoord, const Wavelength& sampledWavelength) const noexcept = 0;

    Spectrum evaluate(const Direction<FrameOfReference::Object>& dir, const Wavelength& sampledWavelength) const noexcept {
        const auto theta = std::atan2(dir.x(), dir.z());
        const auto phi = std::acos(dir.y());
        return evaluate(TexCoord{ theta * invTwoPi + 0.5f, phi * invPi }, sampledWavelength);
    }

    [[nodiscard]] virtual MonoSpectrum mean() const noexcept = 0;

    virtual TextureSample<Setting> sample(SampleProvider& sampler, const Wavelength& sampledWavelength) const noexcept {
        const auto u = sampler.sampleVec2();
        const auto phi = u.x * twoPi;
        const auto theta = std::acos(u.y * 2.0f - 1.0f);

        return TextureSample<Setting>{ Direction<FrameOfReference::Object>::fromSphericalCoord({ theta, phi }),
                                       Rational<Spectrum, PdfType::Texture>::fromRaw(
                                           evaluate(glm::vec2{ u.x, phi * invPi }, sampledWavelength)),
                                       InversePdf<PdfType::Texture>::fromRaw(1.0f) };
    }
};

template <typename Setting>
class ConstantTexture : public TypedRenderVariantBase<Setting> {
    PIPER_IMPORT_SETTINGS();

public:
    virtual Spectrum evaluate(const Wavelength& sampledWavelength) const noexcept = 0;
    [[nodiscard]] virtual MonoSpectrum mean() const noexcept = 0;
};

template <typename T, typename Setting, typename = std::is_base_of<ConstantTexture<Setting>, T>>
class ConstantTexture2DWrapper : public Texture2D<Setting> {
    PIPER_IMPORT_SETTINGS();

    T mImpl;

public:
    explicit ConstantTexture2DWrapper(const Ref<ConfigNode>& node) : mImpl{ node } {}

    Spectrum evaluate(TexCoord, const Wavelength& sampledWavelength) const noexcept final {
        return mImpl.evaluate(sampledWavelength);
    }
};

template <typename T, typename Setting, typename = std::is_base_of<ConstantTexture<Setting>, T>>
class ConstantSphericalTextureWrapper : public SphericalTexture<Setting> {
    PIPER_IMPORT_SETTINGS();

    T mImpl;

public:
    explicit ConstantSphericalTextureWrapper(const Ref<ConfigNode>& node) : mImpl{ node } {}

    Spectrum evaluate(TexCoord, const Wavelength& sampledWavelength) const noexcept final {
        return mImpl.evaluate(sampledWavelength);
    }

    [[nodiscard]] MonoSpectrum mean() const noexcept final {
        return mImpl.mean();
    }
};

PIPER_NAMESPACE_END
