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

enum class FrameOfReference { Camera, World, Object, Shading };

template <FrameOfReference F>
class Vector;

template <FrameOfReference A, FrameOfReference B>
class AffineTransform;

template <FrameOfReference F>
class Point final {
    glm::vec3 mVec;

    explicit constexpr Point(const glm::vec3& x) noexcept : mVec{ x } {}

    template <FrameOfReference A, FrameOfReference B>
    friend class AffineTransform;

public:
    Point() = default;
    static constexpr Point fromRaw(const glm::vec3& x) noexcept {
        return Point{ x };
    }
    [[nodiscard]] glm::vec3 raw() const noexcept {
        return mVec;
    }
    [[nodiscard]] constexpr Float x() const noexcept {
        return mVec.x;
    }
    [[nodiscard]] constexpr Float y() const noexcept {
        return mVec.y;
    }
    [[nodiscard]] constexpr Float z() const noexcept {
        return mVec.z;
    }
};

template <FrameOfReference F>
class Vector final {
    glm::vec3 mVec;

    explicit constexpr Vector(const glm::vec3& x) noexcept : mVec{ x } {}

    template <FrameOfReference A, FrameOfReference B>
    friend class AffineTransform;

public:
    static constexpr Vector fromRaw(const glm::vec3& x) noexcept {
        return Vector{ x };
    }
    [[nodiscard]] glm::vec3 raw() const noexcept {
        return mVec;
    }
    [[nodiscard]] constexpr Float x() const noexcept {
        return mVec.x;
    }
    [[nodiscard]] constexpr Float y() const noexcept {
        return mVec.y;
    }
    [[nodiscard]] constexpr Float z() const noexcept {
        return mVec.z;
    }
    constexpr Vector operator*(const Float rhs) const noexcept {
        return Vector{ mVec * rhs };
    }
};

class Distance final {
    Float mValue;

    constexpr explicit Distance(const Float x) noexcept : mValue{ x } {}

public:
    [[nodiscard]] constexpr Float raw() const noexcept {
        return mValue;
    }
    static constexpr Distance fromRaw(const Float x) noexcept {
        return Distance{ x };
    }
    Distance operator*(const Float x) const noexcept {
        return Distance{ mValue * x };
    }
};

class InverseDistance final {
    Float mValue;

    constexpr explicit InverseDistance(const Float x) noexcept : mValue{ x } {}

public:
    [[nodiscard]] constexpr Float raw() const noexcept {
        return mValue;
    }
    static constexpr InverseDistance fromRaw(const Float x) noexcept {
        return InverseDistance{ x };
    }
    constexpr InverseDistance operator*(const Float x) const noexcept {
        return InverseDistance{ mValue * x };
    }
    constexpr InverseDistance operator-(const InverseDistance rhs) const noexcept {
        return InverseDistance{ mValue - rhs.mValue };
    }
};

constexpr InverseDistance rcp(const Distance x) noexcept {
    return InverseDistance::fromRaw(1.0f / x.raw());
}

constexpr Distance rcp(const InverseDistance x) noexcept {
    return Distance::fromRaw(1.0f / x.raw());
}

constexpr Float operator*(const Distance lhs, const InverseDistance rhs) noexcept {
    return lhs.raw() * rhs.raw();
}

class DistanceSquare final {
    Float mValue;

    constexpr explicit DistanceSquare(const Float x) noexcept : mValue{ x } {}

public:
    [[nodiscard]] Float raw() const noexcept {
        return mValue;
    }
    static constexpr DistanceSquare fromRaw(const Float x) noexcept {
        return DistanceSquare{ x };
    }
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
    glm::vec3 mVec;

    explicit constexpr Normal(const glm::vec3& x) noexcept : mVec{ x } {}

    template <FrameOfReference A, FrameOfReference B>
    friend class AffineTransform;

public:
    Normal() = default;

    static constexpr Normal fromRaw(const glm::vec3& x) noexcept {
        return Normal{ x };
    }
    [[nodiscard]] glm::vec3 raw() const noexcept {
        return mVec;
    }

    [[nodiscard]] constexpr Float x() const noexcept {
        return mVec.x;
    }
    [[nodiscard]] constexpr Float y() const noexcept {
        return mVec.y;
    }
    [[nodiscard]] constexpr Float z() const noexcept {
        return mVec.z;
    }
    constexpr Vector<F> operator*(const Distance x) const noexcept {
        return Vector<F>::fromRaw(mVec * x.raw());
    }
    friend constexpr Float dot(const Normal& lhs, const Normal& rhs) noexcept {
        return glm::dot(lhs.mVec, rhs.mVec);
    }
    friend constexpr Normal cross(const Normal& lhs, const Normal& rhs) noexcept {
        return Normal{ glm::cross(lhs.mVec, rhs.mVec) };
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
        return { rhs.mA2B * mA2B, mB2A * rhs.mB2A };
    }

    Vector<B> operator()(const Vector<A>& x) const noexcept {
        return Vector<B>::fromRaw(glm::mat3{ mA2B } * x.mVec);
    }

    Vector<A> operator()(const Vector<B>& x) const noexcept {
        return Vector<B>::fromRaw(glm::mat3{ mB2A } * x.mVec);
    }

    Point<B> operator()(const Point<A>& x) const noexcept {
        return Point<B>::fromRaw(mA2B * glm::vec4{ x.mVec, 1.0f });
    }

    Point<A> operator()(const Point<B>& x) const noexcept {
        return Point<A>::fromRaw(mB2A * glm::vec4{ x.mVec, 1.0f });
    }

    Normal<B> operator()(const Normal<A>& x) const noexcept {
        return Normal<B>::fromRaw(glm::transpose(glm::mat3(mB2A)) * x.mVec);
    }

    Normal<A> operator()(const Normal<B>& x) const noexcept {
        return Normal<A>::fromRaw(glm::transpose(glm::mat3(mA2B)) * x.mVec);
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
