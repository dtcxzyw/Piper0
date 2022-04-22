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

#include <Piper/Render/BxDFs.hpp>
#include <Piper/Render/Material.hpp>
#include <Piper/Render/SpectralLUTUtil.hpp>
#include <Piper/Render/Texture.hpp>

PIPER_NAMESPACE_BEGIN

template <typename Setting>
class Diffuse final : public Material<Setting> {
    PIPER_IMPORT_SETTINGS();
    PIPER_IMPORT_SHADING();

    Ref<SpectrumTexture2D<Setting>> mReflectance;

public:
    explicit Diffuse(const Ref<ConfigNode>& node)
        : mReflectance{ this->template make<SpectrumTexture2D>(node->get("Reflectance"sv)->as<Ref<ConfigNode>>()) } {}

    BSDF<Setting> evaluate(const Wavelength& sampledWavelength, const SurfaceHit& intersection) const noexcept override {
        return BSDF<Setting>{ ShadingFrame{ intersection.shadingNormal.asDirection(), intersection.dpdu },
                              LambertianBxDF<Setting>{
                                  Rational<Spectrum>::fromRaw(mReflectance->evaluate(intersection.texCoord, sampledWavelength)) } };
    }

    [[nodiscard]] RGBSpectrum estimateAlbedo(const SurfaceHit& intersection) const noexcept override {
        if constexpr(std::is_same_v<Spectrum, SampledSpectrum>) {
            auto res = zero<RGBSpectrum>();

            for(uint32_t idx = wavelengthMin; idx < wavelengthMax; ++idx) {
                const auto base = static_cast<Float>(idx);
                const auto sampledWavelength = SampledSpectrum::fromRaw({ base, base + 0.25f, base + 0.5f, base + 0.75f });
                const auto albedo = toRGB(mReflectance->evaluate(intersection.texCoord, sampledWavelength), sampledWavelength);
                res += albedo;
            }

            res /= wavelengthMax - wavelengthMin;

            return res;
        } else if constexpr(std::is_same_v<Spectrum, MonoWavelengthSpectrum>) {
            const auto lambda = RenderGlobalSetting::get().sampledWavelength;
            return toRGB(mReflectance->evaluate(intersection.texCoord, lambda), lambda);
        } else {
            return toRGB(mReflectance->evaluate(intersection.texCoord, Wavelength{}), Wavelength{});
        }
    }
};

PIPER_REGISTER_VARIANT(Diffuse, Material);

PIPER_NAMESPACE_END
