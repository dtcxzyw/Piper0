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
#include <algorithm>
#include <atomic>

PIPER_NAMESPACE_BEGIN

template <typename T>
class Ref;

namespace Impl {
    constexpr struct OwnsTag {
    } owns;

    struct PostSetter;
}  // namespace impl

class RefCountBase {
    std::atomic_uint32_t mRefCount{ 0u };
    size_t mAllocatedSize = 0;

    template <typename T>
    friend class Ref;

    friend struct Impl::PostSetter;

    void addRef() noexcept {
        ++mRefCount;
    }
    void decRef() noexcept {
        if((--mRefCount) == 0) {
            std::destroy_at(this);
            const auto allocator = context().globalObjectAllocator;
            allocator->deallocate(this, mAllocatedSize);
        }
    }

public:
    RefCountBase() noexcept = default;
    virtual ~RefCountBase() noexcept = default;
    RefCountBase(RefCountBase&) = delete;
    RefCountBase(RefCountBase&&) = delete;
    RefCountBase& operator=(RefCountBase&) = delete;
    RefCountBase& operator=(RefCountBase&&) = delete;
    uint32_t refCount() const noexcept {
        return mRefCount;
    }
};

namespace Impl {
    struct PostSetter final {
        static void setAllocatedSize(RefCountBase* base, const size_t size) noexcept {
            base->mAllocatedSize = size;
        }
    };
}  // namespace impl

template <typename T>
class Ref final {
    T* mPtr;

    template <typename U>
    friend class Ref;

public:
    Ref() noexcept : mPtr{ nullptr } {}
    explicit Ref(nullptr_t) noexcept : mPtr{ nullptr } {}
    explicit Ref(T* ptr) noexcept : mPtr{ ptr } {
        if(mPtr)
            mPtr->addRef();
    }
    explicit Ref(T* ptr, Impl::OwnsTag) noexcept : mPtr{ ptr } {}
    ~Ref() noexcept {
        if(mPtr)
            mPtr->decRef();
    }
    Ref(const Ref& rhs) noexcept : Ref{ rhs.mPtr } {}
    template <typename U>
    Ref(const Ref<U>& rhs) noexcept : Ref{ rhs.mPtr } {}

    Ref(Ref&& rhs) noexcept : mPtr{ rhs.mPtr } {
        rhs.mPtr = nullptr;
    }

    void swap(Ref& rhs) noexcept {
        std::swap(mPtr, rhs.mPtr);
    }

    Ref& operator=(Ref&& rhs) noexcept {
        this->swap(rhs);
        return *this;
    }

    Ref& operator=(const Ref& rhs) noexcept {  // NOLINT(bugprone-unhandled-self-assignment)
        if(mPtr != rhs.mPtr) {
            Ref copy{ rhs };
            this->swap(copy);
        }

        return *this;
    }

    bool operator==(const Ref& rhs) const noexcept {
        return mPtr == rhs.mPtr;
    }

    bool operator!=(const Ref& rhs) const noexcept {
        return mPtr != rhs.mPtr;
    }

    T* release() {
        return std::exchange(mPtr, nullptr);
    }

    T& operator*() const noexcept {
        return *mPtr;
    }

    T* operator->() const noexcept {
        return mPtr;
    }

    [[nodiscard]] T* get() const noexcept {
        return mPtr;
    }

    explicit operator bool() const noexcept {
        return mPtr != nullptr;
    }
};

template <typename T>
concept RefCountable = std::is_base_of_v<RefCountBase, T>;

#ifdef _DEBUG
void error(std::string message);
#endif

template <RefCountable T, RefCountable U = T, typename... Args>
[[nodiscard]] auto makeRefCount(Args&&... args) -> Ref<U> {
    const auto allocator = context().globalObjectAllocator;
    const auto ptr = allocator->allocate(sizeof(T), alignof(T));
    try {
        new(ptr) T{ std::forward<Args>(args)... };
    }
#ifdef _DEBUG
    catch(const std::exception& ex) {
        error(ex.what());

        allocator->deallocate(ptr, sizeof(T), alignof(T));
        throw;
    }
#endif
    catch(...) {
        allocator->deallocate(ptr, sizeof(T), alignof(T));
        throw;
    }
    const auto typedPtr = static_cast<U*>(ptr);
    Impl::PostSetter::setAllocatedSize(typedPtr, sizeof(T));
    return Ref<U>{ typedPtr };
}

template <typename T, typename U>
Ref<T> dynamicCast(Ref<U> ptr) {
    if(const auto newPtr = dynamic_cast<T*>(ptr.get())) {
        ptr.release();
        return Ref<T>{ newPtr, Impl::owns };
    }
    return Ref<T>{};
}

PIPER_NAMESPACE_END
