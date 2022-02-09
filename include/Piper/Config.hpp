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

// ReSharper disable CppClangTidyCppcoreguidelinesMacroUsage
#pragma once
// ReSharper disable once CppUnusedIncludeDirective
#include <cstdlib>

#if __cplusplus < 201907L
#error "C++20 or better is required"
#endif

#ifdef __has_include
#if __has_include(<version>)
// ReSharper disable once CppUnusedIncludeDirective
#include <version>
#endif
#endif

#if _WIN32
#define PIPER_WINDOWS
#define NOMINMAX
#elif __linux__ && !__ANDROID__
#define PIPER_LINUX
#elif __APPLE__
#define PIPER_MACOS
#else
#error "Unsupported platform"
#endif

#define PIPER_NAMESPACE_BEGIN namespace Piper {
#define PIPER_NAMESPACE_END }

#ifdef _MSC_VER
[[noreturn]] inline void unreachable() {
#ifdef _DEBUG
    __debugbreak();
    __assume(false);
#else
    std::abort();
#endif
}
#define PIPER_UNREACHABLE() unreachable()
#else
#define PIPER_UNREACHABLE() __builtin__unreachable()
#endif

#define PIPER_NOT_IMPLEMENTED() PIPER_UNREACHABLE()

// ReSharper disable once CppUnusedIncludeDirective
#include <Piper/PiperFwd.hpp>
#include <filesystem>
#include <range/v3/all.hpp>
// ReSharper disable once CppInconsistentNaming
namespace views = ranges::views;
// ReSharper disable once CppInconsistentNaming
namespace fs = std::filesystem;

using namespace std::literals;  // NOLINT(clang-diagnostic-header-hygiene)
