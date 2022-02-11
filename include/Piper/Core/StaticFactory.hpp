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
#include <PIper/Core/ConfigNode.hpp>
#include <Piper/Core/RefCount.hpp>
#include <Piper/Core/Report.hpp>
#include <oneapi/tbb/concurrent_unordered_map.h>

PIPER_NAMESPACE_BEGIN

class StaticFactory final {
    tbb::concurrent_unordered_map<uint64_t, std::function<Ref<RefCountBase>(const Ref<ConfigNode>&)>> mLUT{};

public:
    template <typename T, typename Base>
    void registerClass(const std::string_view name) {
        const auto hashValue = std::hash<std::string_view>{}(name) ^ typeid(Base).hash_code();
        mLUT.insert({ hashValue, [](const Ref<ConfigNode>& node) { return makeRefCount<T, RefCountBase>(node); } });
    }

    template <typename Base>
    Ref<Base> make(const Ref<ConfigNode>& node) {
        const auto hashValue = std::hash<std::string_view>{}(node->type()) ^ typeid(Base).hash_code();

        const auto iter = mLUT.find(hashValue);
        if(iter == mLUT.cend())
            fatal(fmt::format("Failed to instantiate object \"{}\" [class = {}, interface = {}] ", node->name(), node->type(),
                              typeid(Base).name()));

        return dynamicCast<Base>(iter->second(node));
    }
};

StaticFactory& getStaticFactory();

template <typename T, typename Base>
requires(std::is_base_of_v<Base, T>) struct RegisterHelper final {
    explicit RegisterHelper(const std::string_view name) {
        getStaticFactory().registerClass<T, Base>(name);
    }
};

#define PIPER_REGISTER_CLASS(CLASS, BASE)                      \
    static RegisterHelper<CLASS, BASE> CLASS##RegisterHelper { \
#CLASS                                                 \
    }

PIPER_NAMESPACE_END
