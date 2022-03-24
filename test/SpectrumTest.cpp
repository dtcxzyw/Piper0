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
#include <Piper/Render/SamplingUtil.hpp>
#include <Piper/Render/Spectrum.hpp>
#include <Piper/Render/StandardIlluminant.hpp>
#include <Piper/Render/TestUtil.hpp>

PIPER_NAMESPACE_BEGIN

TEST(Spectrum, IntegralOfY) {
    static_assert((spectralLUTSize & 1) == 1);
    const auto reference = simpson(colorMatchingFunctionY, spectralLUTSize, static_cast<double>(wavelengthMax - wavelengthMin));

    EXPECT_NEAR(reference, integralOfY, 1e-12);
}

TEST(Spectrum, IntegralOfXYZ) {
    static_assert((spectralLUTSize & 1) == 1);
    const auto referenceX = simpson(colorMatchingFunctionX, spectralLUTSize, static_cast<double>(wavelengthMax - wavelengthMin));
    const auto referenceY = simpson(colorMatchingFunctionY, spectralLUTSize, static_cast<double>(wavelengthMax - wavelengthMin));
    const auto referenceZ = simpson(colorMatchingFunctionZ, spectralLUTSize, static_cast<double>(wavelengthMax - wavelengthMin));

    constexpr auto tolerance = integralOfY * 1e-3;
    EXPECT_NEAR(referenceX, integralOfY, tolerance);
    EXPECT_NEAR(referenceY, integralOfY, tolerance);
    EXPECT_NEAR(referenceZ, integralOfY, tolerance);
}

TEST(Spectrum, SamplingVisibleWavelength) {
    constexpr uint32_t samples = 1 << 15;

    double integral = 0.0;

    for(uint32_t idx = 0; idx < samples; ++idx) {
        const auto [lambda, weight] = sampleWavelength<Float, Float>(getTestSampler());
        EXPECT_GE(weight, 0.0f);
        integral += wavelength2Y(static_cast<double>(lambda)) * static_cast<double>(weight);
    }

    integral /= samples;

    EXPECT_NEAR(integral, integralOfY, integralOfY * 5e-3);
}

TEST(Spectrum, Wavelength2XYZ) {
    constexpr uint32_t samples = 1 << 15;

    auto integral = glm::zero<glm::dvec3>();

    for(uint32_t idx = 0; idx < samples; ++idx) {
        const auto lambda = glm::mix(static_cast<Float>(wavelengthMin), static_cast<Float>(wavelengthMax), getTestSampler().sample());
        integral += wavelength2XYZ(static_cast<double>(lambda));
    }

    integral /= samples;

    integral *= static_cast<double>(wavelengthMax - wavelengthMin) / integralOfY;

    constexpr auto tolerance = 0.02;
    EXPECT_NEAR(integral.x, 1.0, tolerance);
    EXPECT_NEAR(integral.y, 1.0, tolerance);
    EXPECT_NEAR(integral.z, 1.0, tolerance);
}

PIPER_NAMESPACE_END
