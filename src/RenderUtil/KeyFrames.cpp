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

#include <Piper/Core/ConfigNode.hpp>
#include <Piper/Core/Report.hpp>
#include <Piper/Render/KeyFrames.hpp>
#include <magic_enum.hpp>

PIPER_NAMESPACE_BEGIN

ShutterKeyFrames generateTransform(const KeyFrames& keyFrames, const TimeInterval interval, const uint32_t maxCount) {
    constexpr auto cmp = [](const Float x, const KeyFrame& y) { return x < y.t; };

    auto next = std::upper_bound(keyFrames.cbegin(), keyFrames.cend(), interval.begin, cmp);
    if(next == keyFrames.cbegin())
        next = std::next(next);
    const auto base = std::prev(next);

    std::pmr::vector<SRTTransform> res;
    const auto sample = [&](const Float t) {
        switch(base->curve) {
            case InterpolationCurve::Linear:
                return lerp(base->transform, next->transform, (t - base->t) / (next->t - base->t));
            default:
                return base->transform;
        }
    };

    switch(base->curve) {
        case InterpolationCurve::Linear:
            return { { sample(interval.begin), sample(interval.end) }, context().globalAllocator };
        default:
            return { { base->transform }, context().globalAllocator };
    }
}

KeyFrames parseKeyframes(const Ref<ConfigAttr>& node) {
    const auto& arr = node->as<ConfigAttr::AttrArray>();
    KeyFrames frames{ context().globalAllocator };

    for(auto& item : arr) {
        auto& attr = item->as<Ref<ConfigNode>>();
        const auto t = static_cast<Float>(attr->get("Time"sv)->as<double>());
        auto curveType = InterpolationCurve::Linear;
        if(const auto ptr = attr->tryGet("InterpolationCurve"sv))
            curveType = magic_enum::enum_cast<InterpolationCurve>((*ptr)->as<std::string_view>()).value();

        glm::vec3 scale{ 1.0f };
        if(const auto ptr = attr->tryGet("Scale"sv))
            scale = parseVec3(*ptr);

        auto rotation = glm::identity<glm::quat>();
        if(const auto ptr = attr->tryGet("Rotation"sv))
            rotation = parseQuat(*ptr);

        glm::vec3 translation{ 0.0f };
        if(const auto ptr = attr->tryGet("Translation"sv))
            translation = parseVec3(*ptr);

        auto precision = 0.0f;
        if(const auto ptr = attr->tryGet("InterpolationPrecision"sv))
            precision = static_cast<Float>((*ptr)->as<double>());

        frames.push_back({ t, curveType, SRTTransform{ scale, rotation, translation }, precision });
    }
    return frames;
}

ResolvedTransform resolveTransform(const KeyFrames& keyFrames, TimeInterval interval) {
    constexpr auto cmp = [](const Float x, const KeyFrame& y) { return x < y.t; };

    auto next = std::upper_bound(keyFrames.cbegin(), keyFrames.cend(), interval.begin, cmp);
    if(next == keyFrames.cbegin())
        next = std::next(next);
    const auto base = std::prev(next);

    std::pmr::vector<SRTTransform> res;
    const auto sample = [&](const Float t) {
        switch(base->curve) {
            case InterpolationCurve::Linear:
                return lerp(base->transform, next->transform, (t - base->t) / (next->t - base->t));
            default:
                return base->transform;
        }
    };

    switch(base->curve) {
        case InterpolationCurve::Linear:
            return { sample(interval.begin), sample(interval.end), base->curve };
        default:
            return { base->transform, SRTTransform{}, base->curve };
    }
}

SRTTransform ResolvedTransform::operator()(const Float t) const noexcept {
    switch(curve) {
        case InterpolationCurve::Linear:
            return lerp(transformBegin, transformEnd, t);
        default:
            return transformBegin;
    }
}

PIPER_NAMESPACE_END
