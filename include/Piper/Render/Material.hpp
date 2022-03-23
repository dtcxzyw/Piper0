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
#include <Piper/Render/BSDF.hpp>
#include <Piper/Render/Intersection.hpp>
#include <Piper/Render/RenderGlobalSetting.hpp>

PIPER_NAMESPACE_BEGIN

class MaterialBase : public RenderVariantBase {
public:
    [[nodiscard]] virtual RGBSpectrum estimateAlbedo(const SurfaceHit& intersection) const noexcept = 0;
};

template <typename Setting>
class Material : public TypedRenderVariantBase<Setting, MaterialBase> {
public:
    PIPER_IMPORT_SETTINGS();
    PIPER_IMPORT_SHADING();

    virtual BSDF<Setting> evaluate(const Wavelength& sampledWavelength, const SurfaceHit& intersection) const noexcept = 0;
};

PIPER_NAMESPACE_END
