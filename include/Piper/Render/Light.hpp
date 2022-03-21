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
#include <Piper/Render/RenderGlobalSetting.hpp>
#include <Piper/Render/SceneObject.hpp>

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
struct LightSample final {
    Direction<FrameOfReference::World> dir;  // src -> hit
    Radiance<Spectrum, PdfType::Light | PdfType::LightSampler> rad;
    InversePdf<PdfType::Light> inversePdf;
    Distance distance;

    static LightSample invalid() noexcept {
        return LightSample{ {},
                            Radiance<Spectrum, PdfType::Light>::fromRaw(zero<Spectrum>()),
                            InversePdf<PdfType::Light>::invalid(),
                            Distance::fromRaw(0.0f) };
    }

    [[nodiscard]] bool valid() const noexcept {
        return inversePdf.valid();
    }
};

template <typename Setting>
class Light : public TypedRenderVariantBase<Setting, LightBase> {
public:
    PIPER_IMPORT_SETTINGS();

    virtual LightSample<Spectrum> sample(Float t, const Wavelength& sampledWavelength, const Point<FrameOfReference::World>& pos,
                                         SampleProvider& sampler) const noexcept = 0;
    virtual Radiance<Spectrum> evaluate(Float t, const Wavelength& sampledWavelength,
                                        const Point<FrameOfReference::World>& pos) const noexcept = 0;
    [[nodiscard]] virtual InversePdf<PdfType::Light> pdf(Float t, const Wavelength& sampledWavelength,
                                                         const Point<FrameOfReference::World>& pos,
                                                         const Direction<FrameOfReference::World>& dir,
                                                         Distance distance) const noexcept = 0;
};

PIPER_NAMESPACE_END
