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
    using WavelengthType = typename WavelengthType<Spectrum>::Type;
    static constexpr auto isSpectral = std::is_same_v<Spectrum, SampledSpectrum>;
    static constexpr auto isPolarized = false;
};

template <SpectrumLike Spectrum>
struct RenderStaticSetting<MuellerMatrix<Spectrum>> final {
    using SpectrumType = MuellerMatrix<Spectrum>;
    using UnpolarizedType = Spectrum;
    using WavelengthType = typename RenderStaticSetting<Spectrum>::WavelengthType;
    static constexpr auto isSpectral = std::is_same_v<Spectrum, SampledSpectrum>;
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
    template <typename Setting>
    explicit Handle(const Ref<T<Setting>>& ref) noexcept : mPtr{ ref.get() } {}

    template <typename Setting>
    [[nodiscard]] auto as() const noexcept -> const T<Setting>& {
        return *reinterpret_cast<const T<Setting>*>(mPtr);
    }
    template <typename Base>
    [[nodiscard]] auto getBase() const noexcept -> const Base& {
        static_assert(std::is_base_of_v<Base, T<RenderStaticSetting<MonoSpectrum>>>);
        return *reinterpret_cast<const Base*>(mPtr);
    }
    [[nodiscard]] auto get() const noexcept {
        return mPtr;
    }
};

template <typename Setting, typename Base = RenderVariantBase>
class TypedRenderVariantBase : public Base {
public:
    template <template <typename> typename T>
    auto& view(Handle<T> x) const {
        return x.template as<Setting>();
    }

    template <template <typename> typename T>
    auto make(const Ref<ConfigNode>& node) const {
        return getStaticFactory().make<T<Setting>>(node);
    }
};

#define PIPER_IMPORT_SETTINGS()                               \
    using Spectrum = typename Setting::SpectrumType;          \
    using Unpolarized = typename Setting::UnpolarizedType;    \
    using Wavelength = typename Setting::WavelengthType;      \
    static constexpr auto isPolarized = Setting::isPolarized; \
    static constexpr auto isSpectral = Setting::isSpectral

using RSSMono = RenderStaticSetting<MonoSpectrum>;
using RSSRGB = RenderStaticSetting<RGBSpectrum>;
using RSSSpectral = RenderStaticSetting<SampledSpectrum>;
/*
using RSSMonoPolarized = RenderStaticSetting<MuellerMatrix<MonoSpectrum>>;
using RSSRGBPolarized = RenderStaticSetting<MuellerMatrix<RGBSpectrum>>;
using RSSSpectralPolarized = RenderStaticSetting<MuellerMatrix<SpectralSpectrum>>;
*/

struct RenderGlobalSetting final {
    std::pmr::string variant;
    SpectrumType spectrumType;
    Ref<AccelerationBuilder> accelerationBuilder;

    static RenderGlobalSetting& get() noexcept;
};

template <typename Base, template <typename> typename T>
auto makeVariant(const Ref<ConfigNode>& node) {
    const auto& variant = RenderGlobalSetting::get().variant;
    auto& factory = getStaticFactory();

#define PIPER_VARIANT_FUNC(VARIANT) \
    if(variant == #VARIANT##sv)     \
        return Ref<Base>{ factory.make<T<VARIANT>>(node) };

#include <Piper/Render/Variants.hpp>

#undef PIPER_VARIANT_FUNC

    fatal(fmt::format("Unrecognized variant {}", variant));
}

template <template <typename> typename Class, template <typename> typename Base>
class TemplateRegisterHelper {
public:
    explicit TemplateRegisterHelper(const std::string_view name) {
#define PIPER_VARIANT_FUNC(VARIANT) getStaticFactory().registerClass<Class<VARIANT>, Base<VARIANT>>(name);
#include <Piper/Render/Variants.hpp>
#undef PIPER_VARIANT_FUNC
    }
};

#define PIPER_REGISTER_VARIANT_IMPL(NAME, CLASS, BASE, UNIQUE_ID)          \
    static TemplateRegisterHelper<CLASS, BASE> UNIQUE_ID##RegisterHelper { \
        NAME                                                               \
    }

#define PIPER_REGISTER_VARIANT(CLASS, BASE) PIPER_REGISTER_VARIANT_IMPL(#CLASS, CLASS, BASE, CLASS)

#define PIPER_REGISTER_WRAPPED_VARIANT_IMPL(NAME, WRAPPER, CLASS, BASE, UNIQUE_ID)    \
    template <typename RSS>                                                           \
    using UNIQUE_ID##Alias = WRAPPER<CLASS, RSS>;                                     \
    static TemplateRegisterHelper<UNIQUE_ID##Alias, BASE> UNIQUE_ID##RegisterHelper { \
        NAME                                                                          \
    }

#define PIPER_REGISTER_WRAPPED_VARIANT(WRAPPER, CLASS, BASE) \
    PIPER_REGISTER_WRAPPED_VARIANT_IMPL(#CLASS, WRAPPER, CLASS, BASE, WRAPPER##CLASS)

PIPER_NAMESPACE_END
