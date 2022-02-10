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
#include <Piper/Render/Spectrum.hpp>
#include <glm/gtc/matrix_transform.hpp>

PIPER_NAMESPACE_BEGIN

template <SpectrumLike T>
class StokesVector final {
    glm::vec<4, T> mVec;

public:
};

template <SpectrumLike T>
class MuellerMatrix final {
    glm::mat<4, 4, T> mMat;

    explicit MuellerMatrix(const glm::mat<4, 4, T>& x) : mMat{ x } {}

public:
    constexpr MuellerMatrix operator+(const MuellerMatrix& rhs) const noexcept {
        return MuellerMatrix{ mMat + rhs.mMat };
    }
    constexpr MuellerMatrix operator*(const MuellerMatrix& rhs) const noexcept {
        return MuellerMatrix{ mMat * rhs.mMat };
    }
    constexpr MuellerMatrix operator*(const Float rhs) const noexcept {
        return MuellerMatrix{ mMat * rhs };
    }

    friend constexpr RGBSpectrum toRGB(const MuellerMatrix& x) noexcept {
        return toRGB(x.mMat[0][0]);
    }
    friend Float luminance(const MuellerMatrix& x) noexcept {
        return luminance(x.mMat[0][0]);
    }
    static constexpr SpectrumType spectrumType() noexcept {
        return Piper::spectrumType<T>();
    }
    static constexpr MuellerMatrix identity() noexcept {
        return MuellerMatrix{ Piper::identity<T>() };
    }
    static constexpr MuellerMatrix zero() noexcept {
        return MuellerMatrix{ Piper::zero<T>() };
    }
};

template <typename T>
struct IsMullerMatrix final {
    static constexpr auto value = false;
};

template <typename T>
struct IsMullerMatrix<MuellerMatrix<T>> final {
    static constexpr auto value = true;
};

template <typename T, typename = std::enable_if_t<IsMullerMatrix<T>::value>>
constexpr SpectrumType spectrumType() noexcept {
    return T::spectrumType();
}

template <typename T, typename = std::enable_if_t<IsMullerMatrix<T>::value>>
constexpr T one() noexcept {
    return T::one();
}

template <typename T, typename = std::enable_if_t<IsMullerMatrix<T>::value>>
constexpr T zero() noexcept {
    return T::zero();
}

PIPER_NAMESPACE_END
