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

// Please refer to https://github.com/Tom94/tev/blob/master/src/Ipc.cpp

#include <Piper/Core/Common.hpp>
#include <Piper/Core/Context.hpp>
#include <Piper/Core/Sync.hpp>
#ifdef PIPER_WINDOWS
// ReSharper disable once CppUnusedIncludeDirective
#include <sdkddkver.h>
#endif
#include <boost/asio.hpp>
#include <type_traits>

PIPER_NAMESPACE_BEGIN

enum class OperationType : char {
    openImage = 0,
    reloadImage = 1,
    closeImage = 2,
    updateImage = 3,
    createImage = 4,
    updateImageV2 = 5,  // Adds multi-channel support
    updateImageV3 = 6,  // Adds custom striding/offset support
    openImageV2 = 7,    // Explicit separation of image name and channel selector
};

static_assert(std::is_trivially_copyable_v<OperationType>);

template <typename T>
concept ContainerLike = requires(T x) {
    typename T::value_type;
    x.data();
    x.size();
    x.begin();
    x.end();
};

class OStream final {
    BinaryData mData{ 4U, context().scopedAllocator };

public:
    template <typename T>
    requires(std::is_trivially_copyable_v<T>) OStream& operator>>(const T& val) {
        const auto base = reinterpret_cast<const std::byte*>(&val);
        mData.insert(mData.cend(), base, base + sizeof(T));
        return *this;
    }

    OStream& operator>>(size_t) = delete;

    template <ContainerLike T>
    OStream& operator>>(const T& val) {
        if constexpr(std::is_trivially_copyable_v<typename T::value_type>) {
            const auto base = reinterpret_cast<const std::byte*>(val.data());
            mData.insert(mData.cend(), base, base + val.size() * sizeof(typename T::value_type));
        } else {
            for(auto& x : val)
                operator>>(x);
        }
        return *this;
    }

    OStream& operator>>(const std::string_view val) {
        const auto base = reinterpret_cast<const std::byte*>(val.data());
        mData.insert(mData.cend(), base, base + val.size() + 1);
        mData.back() = static_cast<std::byte>('\0');
        return *this;
    }

    OStream& operator>>(const bool val) {
        mData.push_back(static_cast<std::byte>(val ? 1 : 0));
        return *this;
    }

    const BinaryData& get() {
        *reinterpret_cast<uint32_t*>(mData.data()) = static_cast<uint32_t>(mData.size()) - 4;
        return mData;
    }
};

class DisplayProviderImpl final : public DisplayProvider {
    static constexpr auto grabFocus = true;
    boost::asio::io_context mCtx;
    boost::asio::ip::tcp::socket mSocket{ mCtx };

    template <typename... Args>
    void send(Args&&... args) {
        MemoryArena arena;
        OStream stream;
        (stream >> ... >> std::forward<Args>(args));

        auto& ref = stream.get();

        mSocket.send(boost::asio::buffer(ref.data(), ref.size()));
    }

public:
    void connect(std::string_view serverConfig) override {
        boost::asio::ip::tcp::resolver resolver{ mCtx };
        const auto pos = serverConfig.find(':');
        const auto endpoint = resolver.resolve(serverConfig.substr(0, pos), serverConfig.substr(pos + 1));
        boost::asio::connect(mSocket, endpoint);
    }
    void create(std::string_view imageName, uint32_t width, uint32_t height,
                const std::initializer_list<std::string_view>& channels) override {
        send(OperationType::createImage, grabFocus, imageName, width, height, static_cast<uint32_t>(channels.size()), channels);
    }
    void update(std::string_view imageName, const std::initializer_list<std::string_view>& channels,
                const std::initializer_list<uint64_t>& offsets, const std::initializer_list<uint64_t>& strides, uint32_t x, uint32_t y,
                uint32_t width, uint32_t height, const std::pmr::vector<float>& data) override {
        send(OperationType::updateImageV3, grabFocus, imageName, static_cast<uint32_t>(channels.size()), channels, x, y, width, height,
             offsets, strides, data);
    }
    void close(std::string_view imageName) override {
        send(OperationType::closeImage, imageName);
    }
    void disconnect() override {
        mSocket.close();
    }
    ~DisplayProviderImpl() override {
        if(mSocket.is_open())
            disconnect();
    }
    bool isSupported() override {
        return mSocket.is_open();
    }
};

DisplayProvider& getDisplayProvider() {
    static DisplayProviderImpl provider;
    return provider;
}

PIPER_NAMESPACE_END
