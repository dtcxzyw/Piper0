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

#pragma once
#include <Piper/Config.hpp>

PIPER_NAMESPACE_BEGIN

class DisplayProvider {
public:
    virtual bool isSupported() = 0;
    virtual void connect(std::string_view serverConfig) = 0;
    virtual void create(std::string_view imageName, uint32_t width, uint32_t height,
                        const std::initializer_list<std::string_view>& channels) = 0;
    virtual void update(std::string_view imageName, const std::initializer_list<std::string_view>& channels,
                        const std::initializer_list<uint64_t>& offsets, const std::initializer_list<uint64_t>& strides, uint32_t x,
                        uint32_t y, uint32_t width, uint32_t height, const std::pmr::vector<float>& data) = 0;
    virtual void close(std::string_view imageName) = 0;
    virtual void disconnect() = 0;
    virtual ~DisplayProvider() = default;
};

DisplayProvider& getDisplayProvider();

PIPER_NAMESPACE_END
