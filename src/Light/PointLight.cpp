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
#include <Piper/Render/Texture.hpp>

PIPER_NAMESPACE_BEGIN

template <typename Setting>
class PointLight final : public Light<Setting> {
    PIPER_IMPORT_SETTINGS();

    Ref<SphericalTexture<Setting>> mIntensity;
    ResolvedTransform mTransform{};

public:
    explicit PointLight(const Ref<ConfigNode>& node)
        : mIntensity{ this->template make<SphericalTexture>(node->get("Intensity"sv)->as<Ref<ConfigNode>>()) } {}
    [[nodiscard]] LightAttributes attributes() const noexcept override {
        return LightAttributes::Delta;
    }

    void updateTransform(const KeyFrames& keyFrames, const TimeInterval timeInterval) override {
        mTransform = resolveTransform(keyFrames, timeInterval);
    }

    LightSample<Spectrum> sample(const Float t, const Wavelength& sampledWavelength, const Point<FrameOfReference::World>& pos,
                                 SampleProvider&) const noexcept override {
        const auto transform = mTransform(t);
        const auto lightSource = Point<FrameOfReference::World>::fromRaw(transform.translation);
        const auto [dir, dist2] = direction(pos, lightSource);
        const auto intensity = mIntensity->evaluate(transform.rotateOnly(dir), sampledWavelength);
        const auto radiance =
            importanceSampled<PdfType::Light | PdfType::LightSampler>(Intensity<Spectrum>::fromRaw(intensity).toRadiance(dist2));
        return LightSample<Spectrum>{ dir, radiance, InversePdf<PdfType::Light>::fromRaw(1.0f), sqrt(dist2) };
    }

    Radiance<Spectrum> evaluate(const Float, const Wavelength&, const Point<FrameOfReference::World>&) const noexcept override {
        return Radiance<Spectrum>::zero();
    }

    [[nodiscard]] InversePdf<PdfType::Light> pdf(Float, const Wavelength&, const Point<FrameOfReference::World>&,
                                                 const Direction<FrameOfReference::World>&, Distance) const noexcept override {
        return InversePdf<PdfType::Light>::invalid();
    }

    [[nodiscard]] Power<MonoSpectrum> power() const noexcept override {
        return Intensity<MonoSpectrum>::fromRaw(mIntensity->mean()) * SolidAngle::fullSphere();
    }
};

PIPER_REGISTER_VARIANT(PointLight, Light);

PIPER_NAMESPACE_END
