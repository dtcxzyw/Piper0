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

// normalized vector
template <FrameOfReference F>
class Direction final {
    template <FrameOfReference A, FrameOfReference B>
    friend class AffineTransform;
    PIPER_GUARD_BASE(Direction, glm::vec3)
    PIPER_GUARD_VEC3(Direction)

    constexpr Vector<F> operator*(const Distance x) const noexcept {
        return Vector<F>::fromRaw(mValue * x.raw());
    }
    constexpr Direction operator-() const noexcept {
        return Direction{ -mValue };
    }

    template <FrameOfReference RealF = F>
    requires(RealF == FrameOfReference::Shading) constexpr void flipZ() noexcept {
        mValue.z = -mValue.z;
    }

    friend constexpr Float dot(const Direction& lhs, const Direction& rhs) noexcept {
        return glm::dot(lhs.mValue, rhs.mValue);
    }

    friend constexpr Float absDot(const Direction& lhs, const Direction& rhs) noexcept {
        return std::fabs(glm::dot(lhs.mValue, rhs.mValue));
    }

    template <FrameOfReference RealF = F>
    requires(RealF == FrameOfReference::Shading) friend constexpr bool sameHemisphere(const Direction& lhs, const Direction& rhs) noexcept {
        return lhs.z() * rhs.z() > 0.0f;
    }

    friend constexpr Direction cross(const Direction& lhs, const Direction& rhs) noexcept {
        return Direction{ glm::cross(lhs.mValue, rhs.mValue) };
    }

    template <FrameOfReference RealF = F>
    requires(RealF == FrameOfReference::Shading) friend constexpr Float cosTheta(const Direction& x) noexcept {
        return x.z();
    }

    template <FrameOfReference RealF = F>
    requires(RealF == FrameOfReference::Shading) friend constexpr Float cos2Theta(const Direction& x) noexcept {
        return sqr(x.z());
    }

    template <FrameOfReference RealF = F>
    requires(RealF == FrameOfReference::Shading) friend constexpr Float absCosTheta(const Direction& x) noexcept {
        return std::fabs(x.z());
    }

    template <FrameOfReference RealF = F>
    requires(RealF == FrameOfReference::Shading) friend constexpr Float sin2Theta(const Direction& x) noexcept {
        return std::fmax(0.0f, 1.0f - cos2Theta(x));
    }

    template <FrameOfReference RealF = F>
    requires(RealF == FrameOfReference::Shading) friend constexpr Float sinTheta(const Direction& x) noexcept {
        return std::sqrt(sin2Theta(x));
    }

    template <FrameOfReference RealF = F>
    requires(RealF == FrameOfReference::Shading) friend constexpr Float tanTheta(const Direction& x) noexcept {
        return sinTheta(x) / cosTheta(x);
    }

    template <FrameOfReference RealF = F>
    requires(RealF == FrameOfReference::Shading) friend constexpr Float tan2Theta(const Direction& x) noexcept {
        return sin2Theta(x) / cos2Theta(x);
    }

    template <FrameOfReference RealF = F>
    requires(RealF == FrameOfReference::Shading) friend constexpr Float cosPhi(const Direction& x) noexcept {
        const auto sinT = sinTheta(x);
        return isZero(sinT) ? 1.0f : std::clamp(x.x() / sinT, -1.0f, 1.0f);
    }

    template <FrameOfReference RealF = F>
    requires(RealF == FrameOfReference::Shading) friend constexpr Float sinPhi(const Direction& x) noexcept {
        const auto sinT = sinTheta(x);
        return isZero(sinT) ? 0.0f : std::clamp(x.y() / sinT, -1.0f, 1.0f);
    }
    template <FrameOfReference RealF = F>
    requires(RealF == FrameOfReference::Shading) static Direction positiveZ() noexcept {
        return Direction{ { 0.0f, 0.0f, 1.0f } };
    }

    // (theta, phi)
    [[nodiscard]] TexCoord toSphericalCoord() const noexcept {
        return { std::acos(std::clamp(mValue.z, -1.0f, 1.0f)), std::atan2(mValue.y, mValue.x) };
    }

    static Direction fromSphericalCoord(const TexCoord texCoord) noexcept {
        const auto [theta, phi] = texCoord;
        const auto cosTheta = std::cos(theta), sinTheta = std::sin(theta);
        const auto cosPhi = std::cos(phi), sinPhi = std::sin(phi);
        return Direction::fromRaw({ sinTheta * cosPhi, sinTheta * sinPhi, cosTheta });
    }
};

template <FrameOfReference F>
constexpr auto normalize(const Vector<F>& x) noexcept {
    return Direction<F>::fromRaw(glm::normalize(x.raw()));
}

template <FrameOfReference F>
constexpr Distance dot(const Direction<F>& lhs, const Vector<F>& rhs) noexcept {
    return Distance::fromRaw(glm::dot(lhs.raw(), rhs.raw()));
}

template <FrameOfReference F>
constexpr auto faceForward(const Direction<F>& dir, const Direction<F>& ref) noexcept {
    return dot(dir, ref) > 0.0f ? dir : -dir;
}

template <FrameOfReference F>
std::pair<Direction<F>, DistanceSquare> direction(const Point<F>& src, const Point<F>& dst) {
    const auto diff = dst - src;
    const auto distSquare = DistanceSquare::fromRaw(glm::dot(diff.raw(), diff.raw()));
    return { Direction<F>::fromRaw(diff.raw() * glm::inversesqrt(distSquare.raw())), distSquare };
}

// normal of shape, normalized
template <FrameOfReference F>
class Normal final {
    template <FrameOfReference A, FrameOfReference B>
    friend class AffineTransform;
    PIPER_GUARD_BASE(Normal, glm::vec3)
    PIPER_GUARD_VEC3(Normal)

    constexpr Normal operator-() const noexcept {
        return Normal{ -mValue };
    }

    [[nodiscard]] constexpr Direction<F> asDirection() const noexcept {
        return Direction<F>::fromRaw(mValue);
    }
};

template <FrameOfReference F>
constexpr Float dot(const Normal<F>& lhs, const Normal<F>& rhs) noexcept {
    return glm::dot(lhs.raw(), rhs.raw());
}

template <FrameOfReference F>
constexpr Float absDot(const Direction<F>& lhs, const Normal<F>& rhs) noexcept {
    return std::fabs(glm::dot(lhs.raw(), rhs.raw()));
}

template <FrameOfReference F>
constexpr Float absDot(const Normal<F>& lhs, const Direction<F>& rhs) noexcept {
    return std::fabs(glm::dot(lhs.raw(), rhs.raw()));
}

template <FrameOfReference A, FrameOfReference B>
class AffineTransform final {
    glm::mat4 mA2B, mB2A;

public:
    explicit constexpr AffineTransform(const glm::mat4& matA2B) noexcept : mA2B{ matA2B }, mB2A{ glm::inverse(matA2B) } {}
    constexpr AffineTransform(const glm::mat4& matA2B, const glm::mat4& matB2A) noexcept : mA2B{ matA2B }, mB2A{ matB2A } {}

    [[nodiscard]] constexpr AffineTransform<B, A> inverse() const noexcept {
        return { mB2A, mA2B };
    }

    template <FrameOfReference C, FrameOfReference D>
    friend class AffineTransform;

    template <FrameOfReference C>
    AffineTransform<A, C> operator*(const AffineTransform<B, C>& rhs) const noexcept {
        return { rhs.mA2B * mA2B, mB2A * rhs.mB2A };
    }

    Vector<B> operator()(const Vector<A>& x) const noexcept {
        return Vector<B>::fromRaw(glm::mat3{ mA2B } * x.raw());
    }

    Vector<A> operator()(const Vector<B>& x) const noexcept {
        return Vector<B>::fromRaw(glm::mat3{ mB2A } * x.raw());
    }

    Direction<B> operator()(const Direction<A>& x) const noexcept {
        return Direction<B>::fromRaw(glm::normalize(glm::mat3{ mA2B } * x.raw()));
    }

    Direction<A> operator()(const Direction<B>& x) const noexcept {
        return Direction<A>::fromRaw(glm::normalize(glm::mat3{ mB2A } * x.raw()));
    }

    Point<B> operator()(const Point<A>& x) const noexcept {
        return Point<B>::fromRaw(mA2B * glm::vec4{ x.raw(), 1.0f });
    }

    Point<A> operator()(const Point<B>& x) const noexcept {
        return Point<A>::fromRaw(mB2A * glm::vec4{ x.raw(), 1.0f });
    }

    Normal<B> operator()(const Normal<A>& x) const noexcept {
        return Normal<B>::fromRaw(glm::normalize(glm::transpose(glm::mat3{ mB2A }) * x.raw()));
    }

    Normal<A> operator()(const Normal<B>& x) const noexcept {
        return Normal<A>::fromRaw(glm::normalize(glm::transpose(glm::mat3{ mA2B }) * x.raw()));
    }
};

// Local to World
struct SRTTransform final {
    glm::vec3 scale;
    glm::quat rotation;
    glm::vec3 translation;

    [[nodiscard]] constexpr Direction<FrameOfReference::World> rotateOnly(const Direction<FrameOfReference::Object>& x) const noexcept {
        return Direction<FrameOfReference::World>::fromRaw(rotation * x.raw());
    }
    [[nodiscard]] /*constexpr*/ Direction<FrameOfReference::Object> rotateOnly(const Direction<FrameOfReference::World>& x) const noexcept {
        return Direction<FrameOfReference::Object>::fromRaw(glm::inverse(rotation) * x.raw());
    }

    // AffineTransform<FrameOfReference::Object, FrameOfReference::World> toAffine() const noexcept {}
};

inline SRTTransform lerp(const SRTTransform& lhs, const SRTTransform& rhs, const Float u) noexcept {
    const auto scale = glm::mix(lhs.scale, rhs.scale, u);
    const auto rotation = glm::slerp(lhs.rotation, rhs.rotation, u);
    const auto translation = glm::mix(lhs.translation, rhs.translation, u);
    return { scale, rotation, translation };
}

PIPER_NAMESPACE_END
