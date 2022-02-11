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

    Normal<FrameOfReference::Object> dir;  // origin -> spherical surface
    Rational<Spectrum, PdfType::Texture> f;
    InversePdf<PdfType::Texture> inversePdf;
};

inline Normal<FrameOfReference::Object> sphericalMapping(const Float theta, const Float phi) noexcept {
    const auto cosTheta = std::cos(theta), sinTheta = std::sin(theta);
    const auto cosPhi = std::cos(phi), sinPhi = std::sin(phi);
    return Normal<FrameOfReference::Object>::fromRaw(glm::vec3{
        sinTheta * sinPhi,
        cosPhi,
        cosTheta * sinPhi,
    });
}

template <typename Setting>
class SphericalTexture : public TypedRenderVariantBase<Setting> {
public:
    PIPER_IMPORT_SETTINGS();

    virtual void prepareForSampling() {}

    virtual Spectrum evaluate(TexCoord texCoord, const Wavelength& sampledWavelength) const noexcept = 0;

    Spectrum evaluate(const Normal<FrameOfReference::Object>& dir, const Wavelength& sampledWavelength) const noexcept {
        const auto theta = std::atan2(dir.x(), dir.z());
        const auto phi = std::acos(dir.y());
        return evaluate(TexCoord{ theta * invTwoPi + 0.5f, phi * invPi }, sampledWavelength);
    }

    [[nodiscard]] virtual MonoSpectrum mean() const noexcept = 0;

    virtual TextureSample<Setting> sample(SampleProvider& sampler, const Wavelength& sampledWavelength) const noexcept {
        const auto u = sampler.sampleVec2();
        const auto theta = u.x * twoPi;
        const auto phi = std::acos(u.y * 2.0f - 1.0f);

        return TextureSample<Setting>{ sphericalMapping(theta, phi),
                                       Rational<Spectrum, PdfType::Texture>::fromRaw(
                                           evaluate(glm::vec2{ u.x, phi * invPi }, sampledWavelength)),
                                       InversePdf<PdfType::Texture>::fromRaw(1.0f) };
    }
};

PIPER_NAMESPACE_END
