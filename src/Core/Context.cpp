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

#include <Piper/Core/Context.hpp>
#include <Piper/Core/StaticFactory.hpp>
#include <oneapi/tbb/cache_aligned_allocator.h>
#include <oneapi/tbb/scalable_allocator.h>

PIPER_NAMESPACE_BEGIN

class ContextStorage final {
    Context mCtx{};
    tbb::cache_aligned_resource mAllocator{ tbb::scalable_memory_resource() };

public:
    ContextStorage() {
        std::pmr::set_default_resource(tbb::scalable_memory_resource());

        mCtx.globalObjectAllocator = &mAllocator;
        mCtx.globalAllocator = &mAllocator;
        mCtx.localAllocator = &mAllocator;
        mCtx.scopedAllocator = nullptr;
    }
    Context& get() noexcept {
        return mCtx;
    }
};

Context& context() {
    static thread_local ContextStorage ctx;
    return ctx.get();
}

PIPER_NAMESPACE_END
