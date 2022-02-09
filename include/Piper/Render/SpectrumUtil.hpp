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
#include <Piper/Core/ConfigNode.hpp>
#include <Piper/Render/Radiometry.hpp>

PIPER_NAMESPACE_BEGIN

namespace impl {
    template <SpectrumLike T, SpectrumLike U>
    struct SpectrumCastCall final {};

    template <SpectrumLike T>
    struct SpectrumCastCall<T, MonoSpectrum> final {
        static MonoSpectrum cast(const T& u, const typename WavelengthType<T>::Type& w) noexcept {
            return luminance(u, w);
        }
    };

    template <SpectrumLike T>
    struct SpectrumCastCall<T, RGBSpectrum> final {
        static RGBSpectrum cast(const T& u, const typename WavelengthType<T>::Type& w) noexcept {
            return toRGB(u, w);
        }
    };

    template <SpectrumLike T>
    struct SpectrumCastCall<T, SpectralSpectrum> final {
        static SpectralSpectrum cast(const T& u, const typename WavelengthType<T>::Type& w) noexcept {
            // TODO
            return zero<SpectralSpectrum>();
        }
    };

}  // namespace impl

template <SpectrumLike T, SpectrumLike U>
auto spectrumCast(const U& u, const typename WavelengthType<U>::Type& w) {
    return impl::SpectrumCastCall<U, T>::cast(u, w);
}

enum class SpectrumParseType { Illuminant, Albedo };

SpectralSpectrum parseStandardSpectrum(const Ref<ConfigNode>& node);
RGBSpectrum parseRGBSpectrum(const Ref<ConfigNode>& node);
SpectralSpectrum parseSpectralSpectrum(const Ref<ConfigNode>& node, SpectrumParseType type);
SpectralSpectrum parseBlackBodySpectrum(const Ref<ConfigNode>& node, const SpectralSpectrum& sampledWavelengths);

template <typename T>
auto parseSpectrum(const Ref<ConfigNode>& node, const WavelengthType<SpectralSpectrum>::Type& w, const SpectrumParseType parseType) {
    const auto type = node->get("Type"sv)->as<std::string_view>();
    if(parseType == SpectrumParseType::Illuminant) {
        if(type == "Standard"sv)
            return spectrumCast<T>(parseStandardSpectrum(node), w);
        if(type == "Blackbody"sv)
            return spectrumCast<T>(parseBlackBodySpectrum(node, w), w);
    }

    if(type == "RGB"sv)
        return spectrumCast<T>(parseRGBSpectrum(node), std::monostate{});
    if(type == "Spectral"sv)
        return spectrumCast<T>(parseSpectralSpectrum(node, parseType), w);
    fatal(fmt::format("Unsupported spectrum type {}", type));
}

PIPER_NAMESPACE_END
