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
#include <Piper/Render/Intersection.hpp>
#include <Piper/Render/Radiometry.hpp>
#include <Piper/Render/Ray.hpp>
#include <Piper/Render/RenderGlobalSetting.hpp>
#include <Piper/Render/SceneObject.hpp>
#include <Piper/Render/ShadingContext.hpp>

PIPER_NAMESPACE_BEGIN

enum class LightAttributes : uint32_t { None = 0, Delta = 1 << 0, Infinite = 1 << 1, Area = 1 << 2, All = (1 << 3) - 1 };
PIPER_BIT_ENUM(LightAttributes)

class LightBase : public SceneObjectComponent {
public:
    virtual LightAttributes attributes() const noexcept = 0;
    PrimitiveGroup* primitiveGroup() const noexcept override {
        return nullptr;
    }

    [[nodiscard]] virtual Power<MonoSpectrum> power() const noexcept = 0;
};

template <typename Spectrum>
struct LightLiSample final {
    Direction<FrameOfReference::World> dir;  // src -> hit
    Radiance<Spectrum, PdfType::Light | PdfType::LightSampler> rad;
    InversePdf<PdfType::Light> inversePdf;
    Distance distance;

    static LightLiSample invalid() noexcept {
        return LightLiSample{ Direction<FrameOfReference::World>::undefined(), Radiance<Spectrum, PdfType::Light>::undefined(),
                              InversePdf<PdfType::Light>::invalid(), Distance::undefined() };
    }

    [[nodiscard]] bool valid() const noexcept {
        return inversePdf.valid();
    }
};

template <typename Spectrum>
struct LightLeSample final {
    Ray ray;
    Intensity<Spectrum> intensity;
    InversePdf<PdfType::LightPos> inversePdfPos;
    InversePdf<PdfType::LightDir> inversePdfDir;

    static LightLeSample invalid() noexcept {
        return LightLeSample{ Ray::undefined(), Intensity<Spectrum>::undefined(), InversePdf<PdfType::LightPos>::invalid(),
                              InversePdf<PdfType::LightDir>::undefined() };
    }

    [[nodiscard]] bool valid() const noexcept {
        return inversePdfPos.valid() || inversePdfDir.valid();
    }
};

template <typename Setting>
class Light : public TypedRenderVariantBase<Setting, LightBase> {
public:
    PIPER_IMPORT_SETTINGS();

    virtual LightLiSample<Spectrum> sampleLi(const ShadingContext<Setting>& ctx, const Point<FrameOfReference::World>& pos,
                                             SampleProvider& sampler) const noexcept = 0;
    virtual InversePdf<PdfType::Light> inversePdfLi(const ShadingContext<Setting>& ctx,
                                                    const Direction<FrameOfReference::World>& wi) const noexcept = 0;
    virtual LightLeSample<Spectrum> sampleLe(const ShadingContext<Setting>& ctx, SampleProvider& sampler) const noexcept = 0;
    virtual std::pair<InversePdf<PdfType::LightPos>, InversePdf<PdfType::LightDir>> pdfLe(const ShadingContext<Setting>& ctx,
                                                                                          const Ray& ray) const noexcept = 0;
    // InfiniteLights only
    virtual Radiance<Spectrum> evalLe(const ShadingContext<Setting>& ctx, const Ray& ray) const noexcept {
        return Radiance<Spectrum>::zero();
    }
    // AreaLights only
    virtual Radiance<Spectrum> evalL(const ShadingContext<Setting>& ctx, const Intersection& intersection) const noexcept {
        return Radiance<Spectrum>::zero();
    }

    virtual std::pair<InversePdf<PdfType::LightPos>, InversePdf<PdfType::LightDir>>
    inversePdfLe(const ShadingContext<Setting>& ctx, const Intersection& intersection, const Ray& ray) const noexcept {
        return { InversePdf<PdfType::LightPos>::invalid(), InversePdf<PdfType::LightDir>::invalid() };
    }
};

PIPER_NAMESPACE_END
