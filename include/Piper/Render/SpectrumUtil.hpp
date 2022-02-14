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
        static SpectralSpectrum cast(const T& u, const WavelengthType<SpectralSpectrum>::Type& w) noexcept {
            // TODO
            return zero<SpectralSpectrum>();
        }
    };

}  // namespace impl

template <SpectrumLike T, SpectrumLike U, typename Wavelength>
auto spectrumCast(const U& u, const Wavelength& w) noexcept {
    return impl::SpectrumCastCall<U, T>::cast(u, w);
}

enum class SpectrumParseType { Illuminant, Albedo };

PIPER_NAMESPACE_END
