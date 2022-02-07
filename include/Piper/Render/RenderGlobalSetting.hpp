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
#include <Piper/Render/Spectrum.hpp>
#include <type_traits>

PIPER_NAMESPACE_BEGIN

template <SpectrumLike Spectrum, bool Polarized>
struct RenderStaticSetting final {
    using SpectrumType = Spectrum;
};

#define PIPER_IMPORT_SETTING() using Spectrum = typename Setting::SpectrumType

using RSSMono = RenderStaticSetting<MonoSpectrum, false>;
using RSSRGB = RenderStaticSetting<RGBSpectrum, false>;
using RSSSpectral = RenderStaticSetting<SpectralSpectrum, false>;

using InstantiatedRenderGlobalSettings = std::void_t<RSSMono, RSSRGB, RSSSpectral>;

struct RenderGlobalSetting final {
    static RenderGlobalSetting& get() noexcept {
        static RenderGlobalSetting inst;
        return inst;
    }
};

PIPER_NAMESPACE_END
