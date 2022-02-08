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

#include <Piper/Core/StaticFactory.hpp>
#include <Piper/Render/Filter.hpp>

PIPER_NAMESPACE_BEGIN

class LanczosFilter final : public Filter {
    Float mRadius;
    Float mInvRadius;

public:
    explicit LanczosFilter(const Ref<ConfigNode>& node) : mRadius{ 1.0f } {
        if(const auto ptr = node->tryGet("Radius"sv))
            mRadius = static_cast<Float>((*ptr)->as<double>());
        mInvRadius = 1.0f / mRadius;
    }
    Float evaluate(const Float dx, const Float dy) const noexcept override {
        const auto eval = [&](const Float d) noexcept {
            const auto absDx = std::fabs(d);
            const auto piAbsDx = pi * absDx;
            const auto res = mRadius / (piAbsDx * piAbsDx) * sin(piAbsDx) * sin(piAbsDx * mInvRadius);
            return absDx < mRadius ? (absDx > 1e-5f ? res : 1.0f) : 0.0f;
        };
        return eval(dx) * eval(dy);
    }
};

PIPER_REGISTER_CLASS(LanczosFilter, Filter);

PIPER_NAMESPACE_END
