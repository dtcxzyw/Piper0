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
#include <Piper/Render/Math.hpp>
#include <glm/gtc/quaternion.hpp>

PIPER_NAMESPACE_BEGIN

enum class FrameOfReference { World, Object, Shading };

template <FrameOfReference F>
class Point final {
    PIPER_GUARD_BASE(Point, glm::vec3)
    PIPER_GUARD_VEC3(Point)
};

template <FrameOfReference F>
class Vector final {
    PIPER_GUARD_BASE(Vector, glm::vec3)
    PIPER_GUARD_VEC3(Vector)
    PIPER_GUARD_BASE_OP(Vector)
};

class Distance final {
    PIPER_GUARD_BASE(Distance, Float)
    PIPER_GUARD_BASE_OP(Distance)
};

class InverseDistance final {
    PIPER_GUARD_BASE(InverseDistance, Float)
    PIPER_GUARD_BASE_OP(InverseDistance)
};

PIPER_GUARD_INVERSE(Distance, InverseDistance);

class DistanceSquare final {
    PIPER_GUARD_BASE(DistanceSquare, Float)

    friend Distance sqrt(const DistanceSquare x) noexcept {
        return Distance::fromRaw(std::sqrt(x.mValue));
    }
};

template <FrameOfReference F>
constexpr DistanceSquare dot(const Vector<F>& lhs, const Vector<F>& rhs) noexcept {
    return DistanceSquare::fromRaw(glm::dot(lhs.raw(), rhs.raw()));
}

template <FrameOfReference F>
constexpr auto cross(const Vector<F>& lhs, const Vector<F>& rhs) noexcept {
    return Vector<F>::fromRaw(glm::cross(lhs.raw(), rhs.raw()));
}

template <FrameOfReference F>
constexpr Point<F> operator+(const Point<F>& lhs, const Vector<F>& rhs) noexcept {
    return Point<F>::fromRaw(lhs.raw() + rhs.raw());
}

template <FrameOfReference F>
constexpr Point<F> operator-(const Point<F>& lhs, const Vector<F>& rhs) noexcept {
    return Point<F>::fromRaw(lhs.raw() - rhs.raw());
}

template <FrameOfReference F>
constexpr auto operator-(const Point<F>& lhs, const Point<F>& rhs) noexcept {
    return Vector<F>::fromRaw(lhs.raw() - rhs.raw());
}

template <FrameOfReference F>
class Normal final {
    template <FrameOfReference A, FrameOfReference B>
    friend class AffineTransform;
    PIPER_GUARD_BASE(Normal, glm::vec3)
    PIPER_GUARD_VEC3(Normal)

    constexpr Vector<F> operator*(const Distance x) const noexcept {
        return Vector<F>::fromRaw(mValue * x.raw());
    }
    constexpr Normal operator-() const noexcept {
        return Normal{ -mValue };
    }

    friend constexpr Float dot(const Normal& lhs, const Normal& rhs) noexcept {
        return glm::dot(lhs.mValue, rhs.mValue);
    }
    friend constexpr Normal cross(const Normal& lhs, const Normal& rhs) noexcept {
        return Normal{ glm::cross(lhs.mValue, rhs.mValue) };
    }
};

template <FrameOfReference F>
constexpr Distance dot(const Normal<F>& lhs, const Vector<F>& rhs) noexcept {
    return Distance::fromRaw(glm::dot(lhs.raw(), rhs.raw()));
}

template <FrameOfReference F>
std::pair<Normal<F>, DistanceSquare> direction(const Point<F>& src, const Point<F>& dst) {
    const auto diff = dst - src;
    const auto distSquare = DistanceSquare::fromRaw(glm::dot(diff.raw(), diff.raw()));
    return { Normal<F>::fromRaw(diff.raw() * glm::inversesqrt(distSquare.raw())), distSquare };
}

template <FrameOfReference A, FrameOfReference B>
class AffineTransform final {
    glm::mat3x4 mA2B, mB2A;

public:
    explicit constexpr AffineTransform(glm::mat3x4 matA2B) noexcept : mA2B{ matA2B }, mB2A{ glm::inverse(glm::mat4{ mB2A }) } {}
    constexpr AffineTransform(glm::mat3x4 matA2B, glm::mat3x4 matB2A) noexcept : mA2B{ matA2B }, mB2A{ matB2A } {}

    [[nodiscard]] constexpr AffineTransform<B, A> inverse() const noexcept {
        return { mB2A, mA2B };
    }

    template <FrameOfReference C, FrameOfReference D>
    friend class AffineTransform;

    template <FrameOfReference C>
    AffineTransform<A, C> operator*(const AffineTransform<B, C>& rhs) const noexcept {
        return { glm::mat3x4{ glm::mat4{ rhs.mA2B } * glm::mat4{ mA2B } }, glm::mat3x4{ glm::mat4{ mB2A } * glm::mat4{ rhs.mB2A } } };
    }

    Vector<B> operator()(const Vector<A>& x) const noexcept {
        return Vector<B>::fromRaw(glm::mat3{ mA2B } * x.raw());
    }

    Vector<A> operator()(const Vector<B>& x) const noexcept {
        return Vector<B>::fromRaw(glm::mat3{ mB2A } * x.raw());
    }

    Point<B> operator()(const Point<A>& x) const noexcept {
        return Point<B>::fromRaw(mA2B * glm::vec4{ x.raw(), 1.0f });
    }

    Point<A> operator()(const Point<B>& x) const noexcept {
        return Point<A>::fromRaw(mB2A * glm::vec4{ x.raw(), 1.0f });
    }

    Normal<B> operator()(const Normal<A>& x) const noexcept {
        return Normal<B>::fromRaw(glm::transpose(glm::mat3(mB2A)) * x.raw());
    }

    Normal<A> operator()(const Normal<B>& x) const noexcept {
        return Normal<A>::fromRaw(glm::transpose(glm::mat3(mA2B)) * x.raw());
    }
};

struct SRTTransform final {
    glm::vec3 scale;
    glm::quat rotation;
    glm::vec3 translation;
};

inline SRTTransform lerp(const SRTTransform& lhs, const SRTTransform& rhs, const Float u) noexcept {
    const auto scale = glm::mix(lhs.scale, rhs.scale, u);
    const auto rotation = glm::slerp(lhs.rotation, rhs.rotation, u);
    const auto translation = glm::mix(lhs.translation, rhs.translation, u);
    return { scale, rotation, translation };
}

PIPER_NAMESPACE_END
