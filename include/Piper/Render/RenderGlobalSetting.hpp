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
#include <Piper/Core/StaticFactory.hpp>
#include <Piper/Render/MuellerMatrix.hpp>
#include <Piper/Render/Spectrum.hpp>

PIPER_NAMESPACE_BEGIN

template <SpectrumLike Spectrum>
struct RenderStaticSetting final {
    using SpectrumType = Spectrum;
    using UnpolarizedType = Spectrum;
    static constexpr auto isPolarized = false;
};

template <SpectrumLike Spectrum>
struct RenderStaticSetting<MuellerMatrix<Spectrum>> final {
    using SpectrumType = MuellerMatrix<Spectrum>;
    using UnpolarizedType = Spectrum;
    static constexpr auto isPolarized = true;
};

// Helper base class
class RenderVariantBase : public RefCountBase {
public:
};

template <template <typename> typename T>
class Handle final {
    const RenderVariantBase* mPtr;

public:
    Handle() : mPtr{ nullptr } {}
    explicit Handle(const RenderVariantBase* ptr) noexcept : mPtr{ ptr } {}
    template <typename Settings>
    Handle(const Ref<T<Settings>>& ref) noexcept : mPtr{ ref.get() } {}

    template <typename Settings>
    auto as() const noexcept -> const T<Settings>& {
        return *reinterpret_cast<T<Settings>*>(mPtr);
    }

    [[nodiscard]] auto get() const noexcept {
        return mPtr;
    }
};

template <typename Settings, typename Base = RenderVariantBase>
class TypedRenderVariantBase : public Base {
public:
    template <template <typename> typename T>
    static auto& view(Handle<T> x) {
        return x.template as<Settings>();
    }

    template <template <typename> typename T>
    static auto make(const Ref<ConfigNode>& node) {
        return getStaticFactory().make<T<Settings>>(node);
    }
};

#define PIPER_IMPORT_SETTINGS()                            \
    using Spectrum = typename Setting::SpectrumType;       \
    using Unpolarized = typename Setting::UnpolarizedType; \
    static constexpr auto isPolarized = Setting::isPolarized

using RSSMono = RenderStaticSetting<MonoSpectrum>;
using RSSRGB = RenderStaticSetting<RGBSpectrum>;
using RSSSpectral = RenderStaticSetting<SpectralSpectrum>;
using RSSMonoPolarized = RenderStaticSetting<MuellerMatrix<MonoSpectrum>>;
using RSSRGBPolarized = RenderStaticSetting<MuellerMatrix<RGBSpectrum>>;
using RSSSpectralPolarized = RenderStaticSetting<MuellerMatrix<SpectralSpectrum>>;

struct RenderGlobalSetting final {
    std::pmr::string variant;
    SpectrumType spectrumType;
    Ref<AccelerationBuilder> accelerationBuilder;

    static RenderGlobalSetting& get() noexcept {
        static RenderGlobalSetting inst;
        return inst;
    }
};

template <typename Base, template <typename> typename T>
auto makeVariant(const Ref<ConfigNode>& node) {
    const auto& variant = RenderGlobalSetting::get().variant;
    auto& factory = getStaticFactory();

#define PIPER_PATTERN_MATCH(VARIANT) \
    if(variant == #VARIANT##sv)      \
    return Ref<Base>{ factory.make<T<VARIANT>>(node) }

    PIPER_PATTERN_MATCH(RSSMono);
    PIPER_PATTERN_MATCH(RSSRGB);
    PIPER_PATTERN_MATCH(RSSSpectral);
    PIPER_PATTERN_MATCH(RSSMonoPolarized);
    PIPER_PATTERN_MATCH(RSSRGBPolarized);
    PIPER_PATTERN_MATCH(RSSSpectralPolarized);

#undef PIPER_PATTERN_MATCH

    fatal(fmt::format("Unrecognized variant {}", variant));
}

#define PIPER_REGISTER_VARIANT_IMPL(CLASS, BASE, RSS)                         \
    static RegisterHelper<CLASS<RSS>, BASE<RSS>> CLASS##RSS##RegisterHelper { \
#CLASS                                                                \
    }

#define PIPER_REGISTER_VARIANT(CLASS, BASE)                     \
    PIPER_REGISTER_VARIANT_IMPL(CLASS, BASE, RSSMono);          \
    PIPER_REGISTER_VARIANT_IMPL(CLASS, BASE, RSSRGB);           \
    PIPER_REGISTER_VARIANT_IMPL(CLASS, BASE, RSSSpectral);      \
    PIPER_REGISTER_VARIANT_IMPL(CLASS, BASE, RSSMonoPolarized); \
    PIPER_REGISTER_VARIANT_IMPL(CLASS, BASE, RSSRGBPolarized);  \
    PIPER_REGISTER_VARIANT_IMPL(CLASS, BASE, RSSSpectralPolarized)

PIPER_NAMESPACE_END
