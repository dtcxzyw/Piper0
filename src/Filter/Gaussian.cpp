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

class GaussianFilter final : public Filter {
    Float mAlpha;
    Float mDiff;

public:
    explicit GaussianFilter(const Ref<ConfigNode>& node) : mAlpha{ node->get("Alpha"sv)->as<Float>() } {
        Float radius = 1.0f;
        if(const auto ptr = node->tryGet("Radius"sv))
            radius = (*ptr)->as<Float>();
        mDiff = std::exp(-mAlpha * radius * radius);
    }
    Float evaluate(const Float dx, const Float dy) const noexcept override {
        const auto eval = [&](const Float d) noexcept { return std::fmax(0.0f, exp(-mAlpha * d * d) - mDiff); };
        return eval(dx) * eval(dy);
    }
};

PIPER_REGISTER_CLASS(GaussianFilter, Filter);

PIPER_NAMESPACE_END
