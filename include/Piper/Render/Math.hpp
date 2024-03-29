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
// ReSharper disable CppClangTidyBugproneMacroParentheses
#pragma once

#include <Piper/Config.hpp>
#include <complex>
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtx/norm.hpp>
#include <numbers>

PIPER_NAMESPACE_BEGIN

using Float = float;
using TexCoord = glm::vec2;

constexpr auto infinity = std::numeric_limits<Float>::max();
constexpr auto epsilon = 1e-4f;
constexpr auto e = std::numbers::e_v<Float>;
constexpr auto pi = std::numbers::pi_v<Float>;
constexpr auto sqrtTwo = std::numbers::sqrt2_v<Float>;
constexpr auto twoPi = glm::two_pi<Float>();
constexpr auto fourPi = static_cast<Float>(4.0 * std::numbers::pi);
constexpr auto quarterPi = glm::quarter_pi<Float>();
constexpr auto halfPi = glm::half_pi<Float>();
constexpr auto invPi = std::numbers::inv_pi_v<Float>;
constexpr auto invTwoPi = glm::one_over_two_pi<Float>();
constexpr auto invSqrtPi = std::numbers::inv_sqrtpi_v<Float>;
constexpr auto invSqrt2 = static_cast<Float>(1.0 / std::numbers::sqrt2);
constexpr auto oneMinusEpsilon = static_cast<Float>(0x1.fffffep-1);

glm::vec2 parseVec2(const Ref<ConfigAttr>& node);
glm::vec3 parseVec3(const Ref<ConfigAttr>& node);
glm::quat parseQuat(const Ref<ConfigAttr>& node);
void initFloatingPointEnvironment() noexcept;

struct FloatingPointExceptionProbe final {
    static void on() noexcept;
    static void off() noexcept;
};

template <typename T>
requires(!std::is_integral_v<T>) constexpr auto rcp(T x) noexcept {
    return static_cast<T>(1.0) / x;
}

inline Float safeSqrt(const Float x) noexcept {
    return std::sqrt(std::fmax(x, 0.0f));
}

inline bool isZero(const Float x) noexcept {
    return std::fabs(x) < 1e-8f;
}

template <typename T>
constexpr auto sqr(T x) noexcept {
    return x * x;
}

template <typename T>
constexpr auto mix(const T& a, const T& b, const Float u) noexcept {
    return a * (1.0f - u) + b * u;
}

template <typename T>
constexpr auto evalPoly(T) noexcept {
    return static_cast<T>(0.0);
}

template <typename T, typename... Ts>
constexpr auto evalPoly(T x, T c0, Ts... c) noexcept {
    return c0 + x * evalPoly(x, c...);
}

namespace Impl {
    template <typename T, uint32_t P>
    struct PowCall final {
        static constexpr T eval(const T& x) noexcept {
            const auto halfPow = PowCall<T, P / 2>::eval(x);
            return P & 1 ? halfPow * halfPow * x : halfPow * halfPow;
        }
    };

    template <typename T>
    struct PowCall<T, 1> final {
        static constexpr T eval(const T& x) noexcept {
            return x;
        }
    };

}  // namespace Impl

template <uint32_t P, typename T>
constexpr auto pow(const T& x) noexcept {
    return Impl::PowCall<T, P>::eval(x);
}

template <typename T>
constexpr T undefined() noexcept {
    return T::undefined();
}

template <>
constexpr Float undefined<Float>() noexcept {
    return std::numeric_limits<Float>::signaling_NaN();
}

template <>
constexpr glm::vec3 undefined<glm::vec3>() noexcept {
    return glm::vec3{ std::numeric_limits<Float>::signaling_NaN() };
}

template <>
constexpr glm::vec4 undefined<glm::vec4>() noexcept {
    return glm::vec4{ std::numeric_limits<Float>::signaling_NaN() };
}

#define PIPER_BIT_ENUM(TYPE)                                                                                        \
    constexpr bool match(const TYPE provide, const TYPE require) noexcept {                                         \
        return (static_cast<uint32_t>(provide) & static_cast<uint32_t>(require)) == static_cast<uint32_t>(require); \
    }                                                                                                               \
    constexpr TYPE operator&(const TYPE a, const TYPE b) noexcept {                                                 \
        return static_cast<TYPE>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));                              \
    }                                                                                                               \
    constexpr TYPE operator|(const TYPE a, const TYPE b) noexcept {                                                 \
        return static_cast<TYPE>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));                              \
    }                                                                                                               \
    constexpr TYPE operator^(const TYPE a, const TYPE b) noexcept {                                                 \
        return static_cast<TYPE>(static_cast<uint32_t>(a) ^ static_cast<uint32_t>(b));                              \
    }                                                                                                               \
    constexpr TYPE operator~(const TYPE a) noexcept {                                                               \
        return a ^ TYPE::All;                                                                                       \
    }

#define PIPER_GUARD_BASE(TYPE, STORAGE)                                 \
    STORAGE mValue;                                                     \
    constexpr explicit TYPE(const STORAGE& x) noexcept : mValue{ x } {} \
                                                                        \
public:                                                                 \
    constexpr const STORAGE& raw() const noexcept {                     \
        return mValue;                                                  \
    }                                                                   \
    static constexpr TYPE undefined() noexcept {                        \
        return TYPE{ Piper::undefined<STORAGE>() };                     \
    }                                                                   \
    static constexpr TYPE fromRaw(const STORAGE& x) noexcept {          \
        return TYPE{ x };                                               \
    }

#define PIPER_GUARD_SCALAR_MULTIPLY(TYPE)                                        \
    constexpr TYPE operator*(const Float rhs) const noexcept {                   \
        return TYPE{ mValue * rhs };                                             \
    }                                                                            \
    constexpr TYPE operator/(const Float rhs) const noexcept {                   \
        return TYPE{ mValue * rcp(rhs) };                                        \
    }                                                                            \
    friend constexpr TYPE operator*(const Float lhs, const TYPE& rhs) noexcept { \
        return TYPE{ rhs.mValue * lhs };                                         \
    }                                                                            \
    constexpr TYPE& operator*=(const Float rhs) noexcept {                       \
        mValue *= rhs;                                                           \
        return *this;                                                            \
    }                                                                            \
    constexpr TYPE operator/=(const Float rhs) noexcept {                        \
        mValue *= rcp(rhs);                                                      \
        return *this;                                                            \
    }

#define PIPER_GUARD_BASE_OP(TYPE)                              \
    constexpr TYPE operator+(const TYPE& rhs) const noexcept { \
        return TYPE{ mValue + rhs.mValue };                    \
    }                                                          \
    constexpr TYPE& operator+=(const TYPE& rhs) noexcept {     \
        mValue += rhs.mValue;                                  \
        return *this;                                          \
    }                                                          \
    constexpr TYPE operator-(const TYPE& rhs) const noexcept { \
        return TYPE{ mValue + rhs.mValue };                    \
    }                                                          \
    PIPER_GUARD_SCALAR_MULTIPLY(TYPE)

// TA*TB -> TC
#define PIPER_GUARD_MULTIPLY(TA, TB, TC)                              \
    constexpr auto operator*(const TA& lhs, const TB& rhs) noexcept { \
        return TC::fromRaw(lhs.raw() * rhs.raw());                    \
    }

#define PIPER_GUARD_MULTIPLY_COMMUTATIVE(HEADER, TA, TB, TC) HEADER PIPER_GUARD_MULTIPLY(TA, TB, TC) HEADER PIPER_GUARD_MULTIPLY(TB, TA, TC)

#define PIPER_GUARD_ELEMENT_VISE_MULTIPLY(TYPE)                \
    constexpr TYPE operator*(const TYPE& rhs) const noexcept { \
        return TYPE{ mValue * rhs.mValue };                    \
    }                                                          \
    constexpr TYPE& operator*=(const TYPE& rhs) noexcept {     \
        mValue *= rhs.mValue;                                  \
        return *this;                                          \
    }

// TA*TB -> Dimensionless
#define PIPER_GUARD_INVERSE(TA, TB)                                    \
    constexpr auto rcp(const TA& x) noexcept {                         \
        return TB::fromRaw(rcp(x.raw()));                              \
    }                                                                  \
    constexpr auto rcp(const TB& x) noexcept {                         \
        return TA::fromRaw(rcp(x.raw()));                              \
    }                                                                  \
    constexpr Float operator*(const TA& lhs, const TB& rhs) noexcept { \
        return lhs.raw() * rhs.raw();                                  \
    }

#define PIPER_GUARD_VEC3(TYPE)                         \
    [[nodiscard]] constexpr Float x() const noexcept { \
        return mValue.x;                               \
    }                                                  \
    [[nodiscard]] constexpr Float y() const noexcept { \
        return mValue.y;                               \
    }                                                  \
    [[nodiscard]] constexpr Float z() const noexcept { \
        return mValue.z;                               \
    }                                                  \
    TYPE() = default;                                  \
    static TYPE zero() {                               \
        return TYPE{ glm::vec3{ 0.0f, 0.0f, 0.0f } };  \
    }

PIPER_NAMESPACE_END
