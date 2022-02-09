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

#include <Piper/Render/Acceleration.hpp>
#include <Piper/Render/Integrator.hpp>
#include <Piper/Render/Radiometry.hpp>

PIPER_NAMESPACE_BEGIN

template <typename Setting>
class PathIntegrator final : public Integrator<Setting> {
    uint32_t mMaxDepth;

public:
    PIPER_IMPORT_SETTINGS();

    explicit PathIntegrator(const Ref<ConfigNode>& node) : mMaxDepth{ node->get("MaxDepth"sv)->as<uint32_t>() } {}
    void preprocess() const noexcept override {}
    void estimate(const Ray& rayInit, const Intersection& intersectionInit, const Acceleration& acceleration, SampleProvider& sampler,
                  Float shutterTime, Float* output) const noexcept override {

        Ray ray = rayInit;
        Intersection intersection = intersectionInit;

        Radiance<Spectrum> result = Radiance<Spectrum>::zero();

        for(uint32_t idx = 0; idx < mMaxDepth; ++idx) {

            intersection = acceleration.trace(ray);
        }

        // Power<Spectrum> x = result;
    }
};

PIPER_REGISTER_VARIANT(PathIntegrator, Integrator);

PIPER_NAMESPACE_END
