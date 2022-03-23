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
#include <Piper/Core/Report.hpp>
#include <Piper/Core/Sync.hpp>
#include <Piper/Render/Random.hpp>

#ifdef PIPER_WINDOWS
// ReSharper disable once CppUnusedIncludeDirective
#include <sdkddkver.h>
#endif

#include <boost/asio.hpp>
#include <type_traits>

PIPER_NAMESPACE_BEGIN

enum class OperationType : char {
    OpenImage = 0,
    ReloadImage = 1,
    CloseImage = 2,
    UpdateImage = 3,
    CreateImage = 4,
    UpdateImageV2 = 5,  // Adds multi-channel support
    UpdateImageV3 = 6,  // Adds custom striding/offset support
    OpenImageV2 = 7,    // Explicit separation of image name and channel selector
};

static_assert(std::is_trivially_copyable_v<OperationType>);

class OStream final {
    BinaryData mData{ 4U, context().scopedAllocator };

public:
    template <typename T>
    OStream& emitRaw(const T& val) requires(std::is_trivially_copyable_v<T>) {
        const auto base = reinterpret_cast<const std::byte*>(&val);
        mData.insert(mData.cend(), base, base + sizeof(T));
        return *this;
    }

    OStream& operator<<(size_t) = delete;

    OStream& operator<<(const std::initializer_list<size_t>& val) {
        const auto base = reinterpret_cast<const std::byte*>(val.begin());
        mData.insert(mData.cend(), base, base + val.size() * sizeof(size_t));
        return *this;
    }

    OStream& operator<<(const std::initializer_list<std::string_view>& val) {
        for(auto& v : val)
            operator<<(v);
        return *this;
    }

    OStream& operator<<(const std::span<float>& val) {
        const auto base = reinterpret_cast<const std::byte*>(val.data());
        mData.insert(mData.cend(), base, base + val.size_bytes());
        return *this;
    }

    OStream& operator<<(const std::string_view val) {
        const auto base = reinterpret_cast<const std::byte*>(val.data());
        mData.insert(mData.cend(), base, base + val.size() + 1);
        mData.back() = static_cast<std::byte>('\0');
        return *this;
    }

    OStream& operator<<(const uint32_t val) {
        emitRaw(val);
        return *this;
    }

    OStream& operator<<(const OperationType val) {
        emitRaw(val);
        return *this;
    }

    OStream& operator<<(const bool val) {
        mData.push_back(static_cast<std::byte>(val ? 1 : 0));
        return *this;
    }

    const BinaryData& get() {
        *reinterpret_cast<uint32_t*>(mData.data()) = static_cast<uint32_t>(mData.size());
        return mData;
    }
};

class DisplayProviderImpl final : public DisplayProvider {
    static constexpr auto grabFocus = true;
    boost::asio::io_context mCtx;
    boost::asio::ip::tcp::socket mSocket{ mCtx };
    bool mConnected = false;
    uint16_t mUniqueID = static_cast<uint16_t>(seeding(std::chrono::high_resolution_clock::now().time_since_epoch().count()));

    template <typename... Args>
    void send(Args&&... args) {
        MemoryArena arena;
        OStream stream;
        (stream << ... << std::forward<Args>(args));

        auto& data = stream.get();

        boost::asio::async_write(mSocket, boost::asio::buffer(data.data(), data.size()),
                                 [this](const boost::system::error_code ec, size_t /*length*/) {
                                     if(ec.failed()) {
                                         error(std::format("Disconnected with tev: {}.", ec.what()));
                                         mConnected = false;
                                     }
                                 });
    }

public:
    void connect(std::string_view serverConfig) override {
        boost::asio::ip::tcp::resolver resolver{ mCtx };
        const auto pos = serverConfig.find(':');
        const auto endpoint = resolver.resolve(serverConfig.substr(0, pos), serverConfig.substr(pos + 1));
        boost::system::error_code ec;
        boost::asio::connect(mSocket, endpoint, ec);
        if(ec.failed()) {
            error(std::format("Failed to connect with tev({}). {}.", serverConfig, ec.what()));
        } else {
            info(std::format("Successfully connected with tev({}).", serverConfig));
            mConnected = true;
        }
    }
    void create(std::string_view imageName, uint32_t width, uint32_t height,
                const std::initializer_list<std::string_view>& channels) override {
        // close(imageName);
        send(OperationType::CreateImage, grabFocus, imageName, width, height, static_cast<uint32_t>(channels.size()), channels);
    }
    void update(std::string_view imageName, const std::initializer_list<std::string_view>& channels,
                const std::initializer_list<uint64_t>& offsets, const std::initializer_list<uint64_t>& strides, uint32_t x, uint32_t y,
                uint32_t width, uint32_t height, const std::span<float>& data) override {
        size_t stridedImageDataSize = 0;
        for(uint32_t c = 0; c < channels.size(); ++c)
            stridedImageDataSize = std::max(stridedImageDataSize, offsets.begin()[c] + (width * height - 1) * strides.begin()[c] + 1);

        const auto stridedData = data.subspan(0, stridedImageDataSize);

        send(OperationType::UpdateImageV3, grabFocus, imageName, static_cast<uint32_t>(channels.size()), channels, x, y, width, height,
             offsets, strides, stridedData);
    }
    void close(std::string_view imageName) override {
        send(OperationType::CloseImage, imageName);
    }
    void disconnect() override {
        boost::system::error_code ec;
        mSocket.shutdown(boost::asio::socket_base::shutdown_both, ec);
        mSocket.close(ec);
    }
    ~DisplayProviderImpl() override {
        if(mSocket.is_open())
            disconnect();
    }

    uint16_t uniqueID() const noexcept override {
        return mUniqueID;
    }

    bool isSupported() override {
        return mConnected;
    }
};

DisplayProvider& getDisplayProvider() {
    static DisplayProviderImpl provider;
    return provider;
}

PIPER_NAMESPACE_END
