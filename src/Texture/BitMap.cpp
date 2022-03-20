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

#include <Piper/Core/Stats.hpp>
#include <Piper/Render/ColorSpace.hpp>
#include <Piper/Render/SpectrumUtil.hpp>
#include <Piper/Render/Texture.hpp>
#pragma warning(push, 0)
#include <OpenImageIO/imagebuf.h>
#include <OpenImageIO/imagebufalgo.h>
#include <OpenImageIO/texture.h>
#include <oneapi/tbb/enumerable_thread_specific.h>
#pragma warning(pop)

PIPER_NAMESPACE_BEGIN

struct TextureSystemDeleter final {
    void operator()(OIIO::TextureSystem* ptr) const {
        OIIO::TextureSystem::destroy(ptr);
    }
};

// TODO: ptex, video, mipmap, anisotropic, color space, etc.

class TextureSystem final {
    std::unique_ptr<OIIO::TextureSystem, TextureSystemDeleter> mSystem;
    tbb::enumerable_thread_specific<OIIO::TextureSystem::Perthread*> mPerThread;
    OIIO::TextureOpt mOptions;

    OIIO::TextureSystem::Perthread* local() {
        auto& ref = mPerThread.local();
        if(ref == nullptr)
            ref = mSystem->create_thread_info();
        return ref;
    }

public:
    TextureSystem() : mSystem{ OIIO::TextureSystem::create() } {}
    TextureSystem(const TextureSystem&) = delete;
    TextureSystem(TextureSystem&&) = delete;
    TextureSystem& operator=(const TextureSystem&) = delete;
    TextureSystem& operator=(TextureSystem&&) = delete;

    ~TextureSystem() {
        for(const auto ptr : mPerThread)
            mSystem->destroy_thread_info(ptr);
    }

    OIIO::TextureSystem::TextureHandle* load(const std::string_view path) {
        return mSystem->get_texture_handle({ path.data(), path.size() }, local());
    }

    void texture(OIIO::TextureSystem::TextureHandle* handle, const TexCoord texCoord, const int channels, Float* res) {
        Counter<StatsType::Texture2D> counter;
        [[maybe_unused]] const auto ok =
            mSystem->texture(handle, local(), mOptions, texCoord.x, texCoord.y, 0.0f, 0.0f, 0.0f, 0.0f, channels, res);
        assert(ok);
    }

    static TextureSystem& get() noexcept {
        static TextureSystem system;
        return system;
    }
};

// TODO: parseType
template <typename Setting>
class BitMap : public Texture2D<Setting> {
    PIPER_IMPORT_SETTINGS();

    TextureSystem& mSystem;
    OIIO::TextureSystem::TextureHandle* mHandle;

public:
    explicit BitMap(const Ref<ConfigNode>& node)
        : mSystem{ TextureSystem::get() }, mHandle{ mSystem.load(node->get("FilePath"sv)->as<std::string_view>()) } {}

    Spectrum evaluate(TexCoord texCoord, const Wavelength& sampledWavelength) const noexcept override {
        if constexpr(std::is_same_v<Spectrum, MonoSpectrum>) {
            MonoSpectrum res;
            mSystem.texture(mHandle, texCoord, 1, &res);
            return res;
        } else {
            RGBSpectrum res{ uninitialized };
            static_assert(sizeof(RGBSpectrum) == 3 * sizeof(Float));
            mSystem.texture(mHandle, texCoord, 3, reinterpret_cast<Float*>(&res));

            if constexpr(std::is_same_v<Spectrum, RGBSpectrum>)
                return res;
            else {
                return spectrumCast<Spectrum>(res, sampledWavelength);
            }
        }
    }
};

PIPER_REGISTER_VARIANT(BitMap, Texture2D);
PIPER_NAMESPACE_END
