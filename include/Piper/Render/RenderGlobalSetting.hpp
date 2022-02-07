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
#include <Piper/Core/StaticFactory.hpp>
#include <Piper/Render/MuellerMatrix.hpp>
#include <Piper/Render/Spectrum.hpp>
#include <type_traits>

PIPER_NAMESPACE_BEGIN

template <SpectrumLike Spectrum>
struct RenderStaticSetting final {
    using SpectrumType = Spectrum;
    using Unpolarized = Spectrum;
    static constexpr auto isPolarized = false;
};

template <SpectrumLike Spectrum>
struct RenderStaticSetting<MuellerMatrix<Spectrum>> final {
    using SpectrumType = MuellerMatrix<Spectrum>;
    using Unpolarized = Spectrum;
    static constexpr auto isPolarized = true;
};

template <typename Setting>
class RenderVariantBase : public RefCountBase {
public:
    using Spectrum = typename Setting::SpectrumType;
    using Unpolarized = typename Setting::UnpolarizedType;
    static constexpr auto isPolarized = Setting::isPolarized;
};

using RSSMono = RenderStaticSetting<MonoSpectrum>;
using RSSRGB = RenderStaticSetting<RGBSpectrum>;
using RSSSpectral = RenderStaticSetting<SpectralSpectrum>;
using RSSMonoPolarized = RenderStaticSetting<MuellerMatrix<MonoSpectrum>>;
using RSSRGBPolarized = RenderStaticSetting<MuellerMatrix<RGBSpectrum>>;
using RSSSpectralPolarized = RenderStaticSetting<MuellerMatrix<SpectralSpectrum>>;

struct RenderGlobalSetting final {
    std::pmr::string variant;
    Ref<AccelerationBuilder> accelerationBuilder;

    static RenderGlobalSetting& get() noexcept {
        static RenderGlobalSetting inst;
        return inst;
    }
};

#define PIPER_REGISTER_VARIANT_IMPL(CLASS, BASE, RSS)                         \
    static RegisterHelper<CLASS<RSS>, BASE<RSS>> CLASS##RSS##RegisterHelper { \
#CLASS                                                                \
    }

#define PIPER_REGISTER_VARIANT(CLASS, BASE)                     \
    PIPER_REGISTER_VARIANT_IMPL(CLASS, BASE, RSSMono);          \
    PIPER_REGISTER_VARIANT_IMPL(CLASS, BASE, RSSRGB);           \
    PIPER_REGISTER_VARIANT_IMPL(CLASS, BASE, RSSSpectral);      \
    PIPER_REGISTER_VARIANT_IMPL(CLASS, BASE, RSSMonoPolarized); \
    PIPER_REGISTER_VARIANT_IMPL(CLASS, BASE, RSSRGBPolarized);  \
    PIPER_REGISTER_VARIANT_IMPL(CLASS, BASE, RSSSpectralPolarized)

PIPER_NAMESPACE_END
