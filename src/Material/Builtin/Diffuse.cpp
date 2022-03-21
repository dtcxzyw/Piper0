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

#include <Piper/Render/BSDF.hpp>
#include <Piper/Render/Material.hpp>
#include <Piper/Render/Texture.hpp>

PIPER_NAMESPACE_BEGIN

template <typename Setting>
class LambertianBxDF final : public BxDF<Setting> {
    PIPER_IMPORT_SETTINGS();
    PIPER_IMPORT_SHADING();

    Rational<Spectrum> mReflectance;

public:
    explicit LambertianBxDF(const Rational<Spectrum>& reflectance) : mReflectance{ reflectance } {}

    [[nodiscard]] BxDFPart part() const noexcept override {
        return BxDFPart::Diffuse | BxDFPart::Reflection;
    }
    Rational<Spectrum> evaluate(const Direction&, const Direction&) const noexcept override {
        return mReflectance * invPi;
    }
};

template <typename Setting>
class Diffuse final : public Material<Setting> {
    PIPER_IMPORT_SETTINGS();
    PIPER_IMPORT_SHADING();

    Ref<Texture2D<Setting>> mReflectance;

public:
    explicit Diffuse(const Ref<ConfigNode>& node)
        : mReflectance{ this->template make<Texture2D>(node->get("Reflectance"sv)->as<Ref<ConfigNode>>()) } {}

    BSDF<Setting> evaluate(const Wavelength& sampledWavelength, const SurfaceHit& intersection) const noexcept override {
        return BSDF<Setting>{ ShadingFrame{ intersection.shadingNormal.asDirection(), intersection.dpdu },
                              LambertianBxDF<Setting>{
                                  Rational<Spectrum>::fromRaw(mReflectance->evaluate(intersection.texCoord, sampledWavelength)) } };
    }
};

PIPER_REGISTER_VARIANT(Diffuse, Material);

PIPER_NAMESPACE_END
