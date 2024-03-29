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

#include <Piper/Core/ConfigNode.hpp>
#include <simdjson.h>

PIPER_NAMESPACE_BEGIN
Ref<ConfigNode> parseNode(const simdjson::dom::object& obj, const ResolveConfiguration& config, Ref<RefCountBase> holder = {});

// TODO: resolve expression
std::pmr::string resolveString(const std::string_view string, const ResolveConfiguration& config) {
    std::pmr::string str{ string, context().scopedAllocator };
    for(auto [pattern, replace] : config) {
        while(true) {
            const auto pos = str.find(pattern);
            if(pos == std::pmr::string::npos)
                break;
            str.replace(pos, pattern.size(), replace);
        }
        if(str.find("${"sv) == std::pmr::string::npos)
            break;
    }
    return str;
}

static Ref<ConfigAttr> parseAttr(const simdjson::dom::element& element, const ResolveConfiguration& config) {
    switch(element.type()) {
        case simdjson::dom::element_type::ARRAY: {
            ConfigAttr::AttrArray arr{ context().globalAllocator };
            const auto arrayRef = element.get_array();
            arr.reserve(arrayRef.size());
            for(auto item : arrayRef)
                arr.push_back(parseAttr(item, config));
            return makeRefCount<ConfigAttr>(std::move(arr));
        }
        case simdjson::dom::element_type::OBJECT:
            return makeRefCount<ConfigAttr>(parseNode(element.get_object(), config));
        case simdjson::dom::element_type::UINT64:
            return makeRefCount<ConfigAttr>(static_cast<uint32_t>(element.get_uint64()));
        case simdjson::dom::element_type::INT64:
            return makeRefCount<ConfigAttr>(static_cast<uint32_t>(element.get_int64()));
        case simdjson::dom::element_type::DOUBLE:
            return makeRefCount<ConfigAttr>(element.get_double());
        case simdjson::dom::element_type::STRING: {
            const auto str = element.get_string().value();
            if(str.find("${"sv) != std::string_view::npos)
                return makeRefCount<ConfigAttr>(resolveString(str, config));
            return makeRefCount<ConfigAttr>(str);
        }
        case simdjson::dom::element_type::BOOL:
            return makeRefCount<ConfigAttr>(element.get_bool());
        default:
            PIPER_UNREACHABLE();
    }
}

Ref<ConfigNode> parseNode(const simdjson::dom::object& obj, const ResolveConfiguration& config,
                                 Ref<RefCountBase> holder) {  // NOLINT(performance-unnecessary-value-param)
    constexpr auto valueOr = [](const simdjson::simdjson_result<std::string_view>& res, const std::string_view fallback) {
        return res.error() == simdjson::SUCCESS ? res.value_unsafe() : fallback;
    };

    auto type = valueOr(obj.at_key("Type"sv).get_string(), "Unspecified"sv);
    if(type == "Include"sv) {
        const auto path = resolveString(obj.at_key("FileName"sv), config);
        ResolveConfiguration newConfig{ config, context().scopedAllocator };
        const auto base = fs::path{ path }.parent_path().string();
        newConfig["${BaseDir}"] = base;

        return parseJSONConfigNode(path, newConfig);
    }

    auto name = valueOr(obj.at_key("Name"sv).get_string(), "Unnamed"sv);
    ConfigNode::AttrMap attrs{ context().globalAllocator };

    for(auto& [key, value] : obj)
        if(key != "Name"sv && key != "Type"sv)
            attrs.insert({ key, parseAttr(value, config) });
    return makeRefCount<ConfigNode>(name, type, std::move(attrs), std::move(holder));
}

Ref<ConfigNode> parseJSONConfigNodeFromStr(const std::string_view str, const ResolveConfiguration& config) {
    MemoryArena arena;

    struct Holder final : RefCountBase {
        simdjson::dom::parser parser;
    };

    auto holder = makeRefCount<Holder>();
    const auto res = holder->parser.parse(str.data(), str.size());

    return parseNode(res.get_object(), config, std::move(holder));
}

Ref<ConfigNode> parseJSONConfigNode(const std::string_view path, const ResolveConfiguration& config) {
    const auto data = loadData(path);
    return parseJSONConfigNodeFromStr(std::string_view{ reinterpret_cast<const char*>(data.data()), data.size() }, config);
}

PIPER_NAMESPACE_END
