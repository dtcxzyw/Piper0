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
#include <Piper/Render/SpectrumUtil.hpp>

PIPER_NAMESPACE_BEGIN

template <typename Setting>
class LambertianBSDF final : public BSDF<Setting> {
    PIPER_IMPORT_SETTINGS();
    PIPER_IMPORT_SHADING();

    Rational<Spectrum> mReflectance;

public:
    explicit LambertianBSDF(const Rational<Spectrum>& reflectance) : mReflectance{ reflectance } {}

    BxDFPart part() const noexcept override {
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

    Rational<Spectrum> mReflectance;  // TODO: Texture

public:
    explicit Diffuse(const Ref<ConfigNode>& node) {
        auto w = zero<SpectralSpectrum>();
        if(auto ptr = node->tryGet("Reflectance"sv))
            mReflectance = parseSpectrum<Spectrum>((*ptr)->as<Ref<ConfigNode>>(), w, SpectrumParseType::Albedo);
        else
            mReflectance = identity<Spectrum>();
    }

    BSDFArray<Setting> evaluate(const SurfaceHit& intersection) const noexcept {
        BSDFArray<Setting> res;
        res.emplace(LambertianBSDF<Setting>{ mReflectance });
        return res;
    }
};

PIPER_REGISTER_VARIANT(Diffuse, Material);

PIPER_NAMESPACE_END
