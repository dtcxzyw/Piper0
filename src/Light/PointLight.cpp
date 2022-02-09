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

#include <Piper/Render/Light.hpp>
#include <Piper/Render/SpectrumUtil.hpp>

PIPER_NAMESPACE_BEGIN

template <typename Setting>
class PointLight final : public Light<Setting> {
    PIPER_IMPORT_SETTINGS();

    Intensity<Spectrum> mIntensity = Intensity<Spectrum>::fromRaw(zero<Spectrum>());
    ResolvedTransform mTransform{};

public:
    explicit PointLight(const Ref<ConfigNode>& node) {
        SpectralSpectrum w = zero<SpectralSpectrum>();

        if(const auto intensity = node->tryGet("Intensity"sv))
            mIntensity = Intensity<Spectrum>::fromRaw(
                parseSpectrum<Spectrum>((*intensity)->as<Ref<ConfigNode>>(), w, SpectrumParseType::Illuminant));
        else if(const auto power = node->tryGet("Power"sv))
            mIntensity =
                Power<Spectrum>::fromRaw(parseSpectrum<Spectrum>((*power)->as<Ref<ConfigNode>>(), w, SpectrumParseType::Illuminant)) /
                SolidAngle::fullSphere();
        else
            fatal("Intensity or power field is required.");
    }
    [[nodiscard]] LightAttributes attributes() const noexcept override {
        return LightAttributes::Delta;
    }

    void updateTransform(const KeyFrames& keyFrames, const TimeInterval timeInterval) override {
        mTransform = resolveTransform(keyFrames, timeInterval);
    }

    LightSample<Spectrum> sample(const Float t, const Point<FrameOfReference::World>& pos, SampleProvider&) const noexcept override {
        const auto lightSource = Point<FrameOfReference::World>::fromRaw(mTransform(t).translation);
        const auto [dir, dist2] = direction(pos, lightSource);
        const auto radiance = mIntensity.toRadiance(dist2).template importanceSampled<PdfType::Light>();
        return LightSample<Spectrum>{ dir, radiance, InversePdf<PdfType::Light>::fromRaw(1.0f), sqrt(dist2) };
    }

    Radiance<Spectrum> evaluate(const Float t, const Point<FrameOfReference::World>& pos) const noexcept override {
        return Radiance<Spectrum>::zero();
    }

    [[nodiscard]] InversePdf<PdfType::Light> pdf(Float, const Point<FrameOfReference::World>&, const Normal<FrameOfReference::World>&,
                                                 Distance) const noexcept override {
        return InversePdf<PdfType::Light>::invalid();
    }

    Power<Spectrum> power() const noexcept override {
        return mIntensity * SolidAngle::fullSphere();
    }
};

PIPER_REGISTER_VARIANT(PointLight, Light);

PIPER_NAMESPACE_END
