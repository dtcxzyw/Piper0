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

#include <Piper/Render/LightSampler.hpp>
#include <Piper/Render/Sampler.hpp>

PIPER_NAMESPACE_BEGIN

class UniformLightSampler final : public LightSampler {
    std::pmr::vector<Handle<Light>> mLights{ context().globalAllocator };
    std::pmr::vector<Handle<Light>> mInfiniteLights{ context().globalAllocator };

public:
    explicit UniformLightSampler(const Ref<ConfigNode>&) {}
    void preprocess(const std::pmr::vector<LightBase*>& lights, const Float &sceneRadius) override {
        mLights.clear();
        mLights.reserve(lights.size());
        for(const auto light : lights) {
            light->preprocess(sceneRadius);
            mLights.push_back(Handle<Light>{ light });
            if(match(light->attributes(), LightAttributes::Infinite))
                mInfiniteLights.push_back(Handle<Light>{ light });
        }
    }
    std::pair<Handle<Light>, InversePdf<PdfType::LightSampler>> sample(SampleProvider& sampler) const noexcept override {
        const auto idx = sampler.sampleIdx(static_cast<uint32_t>(mLights.size()));
        return { mLights[idx], InversePdf<PdfType::LightSampler>::fromRaw(static_cast<Float>(mLights.size())) };
    }
    std::span<const Handle<Light>> infiniteLights() const noexcept override {
        return { mInfiniteLights.data(), mInfiniteLights.size() };
    }
};

PIPER_REGISTER_CLASS(UniformLightSampler, LightSampler);

PIPER_NAMESPACE_END
