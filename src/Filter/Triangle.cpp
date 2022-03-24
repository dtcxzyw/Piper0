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

class TriangleFilter final : public Filter {
    Float mInvRadius;

public:
    explicit TriangleFilter(const Ref<ConfigNode>& node) : mInvRadius{ 1.0f } {
        if(const auto ptr = node->tryGet("Radius"sv))
            mInvRadius = rcp((*ptr)->as<Float>());
    }
    Float evaluate(const Float dx, const Float dy) const noexcept override {
        const auto eval = [&](const Float d) noexcept { return std::fmax(0.0f, 1.0f - std::fabs(d) * mInvRadius); };
        return eval(dx) * eval(dy);
    }
};

PIPER_REGISTER_CLASS(TriangleFilter, Filter);

PIPER_NAMESPACE_END
