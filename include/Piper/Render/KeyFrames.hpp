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
#include <Piper/Render/Transform.hpp>
#include <vector>

PIPER_NAMESPACE_BEGIN

enum class InterpolationCurve final { Hold, Linear };

struct KeyFrame final {
    Float t;
    InterpolationCurve curve;

    SRTTransform transform;
    Float precision;
};

using KeyFrames = std::pmr::vector<KeyFrame>;

struct TimeInterval final {
    Float begin;
    Float end;
};

using ShutterKeyFrames = std::pmr::vector<SRTTransform>;

ShutterKeyFrames generateTransform(const KeyFrames& keyFrames, TimeInterval interval, uint32_t maxCount);
struct ResolvedTransform final {
    SRTTransform transformBegin{};
    SRTTransform transformEnd{};
    InterpolationCurve curve = InterpolationCurve::Hold;

    SRTTransform operator()(Float t) const noexcept;
};

ResolvedTransform resolveTransform(const KeyFrames& keyFrames, TimeInterval interval);
KeyFrames parseKeyframes(const Ref<ConfigAttr>& node);

PIPER_NAMESPACE_END
