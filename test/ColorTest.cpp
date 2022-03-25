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

#include <Piper/Render/ColorMatchingFunction.hpp>
#include <Piper/Render/Spectrum.hpp>
#include <Piper/Render/SpectrumUtil.hpp>
#include <Piper/Render/StandardIlluminant.hpp>
#include <Piper/Render/TestUtil.hpp>

PIPER_NAMESPACE_BEGIN

TEST(RGB, RGB2XYZ) {
    constexpr auto rgb = RGBSpectrum::fromScalar(1.0f);
    const auto xyz = RGBSpectrum::matRGB2XYZ * rgb.raw();
    ASSERT_LT(glm::distance2(xyz, glm::vec3{ 0.950456f, 1.0f, 1.08875f }), 1e-6);
    const auto rgb2 = RGBSpectrum::matXYZ2RGB * xyz;
    ASSERT_LT(glm::distance2(rgb2, rgb.raw()), 1e-6);
}

TEST(RGB, RGBStdandardIlluminat) {
    std::pmr::vector<double> xs(spectralLUTSize), ys(spectralLUTSize), zs(spectralLUTSize);

    for(uint32_t idx = 0; idx < spectralLUTSize; ++idx) {
        const auto lambda = static_cast<double>(idx + wavelengthMin);
        const auto [x, y, z] = wavelength2XYZ(lambda) * wavelength2IlluminantD65(lambda);
        std::tie(xs[idx], ys[idx], zs[idx]) = std::make_tuple(x, y, z);
    }

    const auto x = simpson(xs.data(), spectralLUTSize, wavelengthMax - wavelengthMin);
    const auto y = simpson(ys.data(), spectralLUTSize, wavelengthMax - wavelengthMin);
    const auto z = simpson(zs.data(), spectralLUTSize, wavelengthMax - wavelengthMin);

    const auto rgb = RGBSpectrum::matXYZ2RGB * glm::vec3{ x, y, z };
    ASSERT_LT(glm::distance2(rgb, glm::one<glm::vec3>()), 1e-6);
}

PIPER_NAMESPACE_END
