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
#include <Piper/Config.hpp>
#include <memory_resource>

PIPER_NAMESPACE_BEGIN

struct Context final {
    std::pmr::memory_resource* globalAllocator;
    std::pmr::memory_resource* globalObjectAllocator;
    std::pmr::memory_resource* localAllocator;
    std::pmr::memory_resource* scopedAllocator;
};

Context& context();

class MemoryArena final {
    // std::pmr::monotonic_buffer_resource mAllocator{ context().scopedAllocator ? 0U : 4096U, context().localAllocator }; //TODO: FIXME
    std::pmr::unsynchronized_pool_resource mAllocator{ context().localAllocator };

public:
    MemoryArena() {
        auto& allocator = context().scopedAllocator;
        if(!allocator)
            allocator = &mAllocator;
    }
    MemoryArena(const MemoryArena&) = delete;
    MemoryArena(MemoryArena&&) = delete;
    MemoryArena& operator=(const MemoryArena&) = delete;
    MemoryArena& operator=(MemoryArena&&) = delete;
    ~MemoryArena() {
        if(context().scopedAllocator == &mAllocator)
            context().scopedAllocator = nullptr;
    }
};

PIPER_NAMESPACE_END
