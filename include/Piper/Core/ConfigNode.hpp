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
#include <Piper/Core/Context.hpp>
#include <Piper/Core/FileIO.hpp>
#include <Piper/Core/RefCount.hpp>
#include <variant>

PIPER_NAMESPACE_BEGIN

class ConfigAttr final : public RefCountBase {
    std::variant<uint32_t, double, std::string_view, std::pmr::vector<Ref<ConfigAttr>>, Ref<ConfigNode>> mValue;

public:
    template <typename T>
    ConfigAttr(T&& x) : mValue{ std::forward<T>(x) } {}

    template <typename T>
    const T& as() const {
        return std::get<T>(mValue);
    }
};

class ConfigNode final : public RefCountBase {
public:
    using AttrMap = std::pmr::unordered_map<std::string_view, Ref<ConfigAttr>>;

private:
    std::string_view mName;
    std::string_view mType;
    AttrMap mValue;

public:
    ConfigNode(std::string_view name, std::string_view type, AttrMap value) : mName{ name }, mType{ type }, mValue{ std::move(value) } {}

    auto type() const noexcept {
        return mType;
    }
    auto name() const noexcept {
        return mName;
    }
};

Ref<ConfigNode> parseConfigNode(const BinaryData& data);

PIPER_NAMESPACE_END
