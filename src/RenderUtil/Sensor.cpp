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

#include <Piper/Render/Sensor.hpp>

PIPER_NAMESPACE_BEGIN

std::pair<SensorNDCAffineTransform, RenderRECT> calcRenderRECT(const uint32_t width, const uint32_t height, const Float deviceAspectRatio,
                                                               const FitMode fitMode) {
    RenderRECT rect;
    SensorNDCAffineTransform transform;
    const auto imageAspectRatio = static_cast<Float>(width) / static_cast<Float>(height);
    const auto iiar = 1.0f / imageAspectRatio;
    const auto idar = 1.0f / deviceAspectRatio;
    if(fitMode == FitMode::Fill) {
        rect = { 0, 0, width, height };
        if(imageAspectRatio > deviceAspectRatio) {
            transform = { 0.0f, (idar - iiar) * 0.5f * deviceAspectRatio, 1.0f, iiar * deviceAspectRatio };
        } else {
            transform = { (deviceAspectRatio - imageAspectRatio) * 0.5f * idar, 0.0f, imageAspectRatio * idar, 1.0f };
        }
    } else {
        if(imageAspectRatio > deviceAspectRatio) {
            transform = { -(imageAspectRatio - deviceAspectRatio) * 0.5f * idar, 0.0f, imageAspectRatio * idar, 1.0f };
            rect = { static_cast<uint32_t>(
                         floorf(std::max(0.0f, static_cast<Float>(width) * (imageAspectRatio - deviceAspectRatio) * 0.5f * iiar))),
                     0,
                     std::min(
                         width,
                         static_cast<uint32_t>(ceilf(static_cast<Float>(width) * (imageAspectRatio + deviceAspectRatio) * 0.5f * iiar))),
                     height };
            rect.width -= rect.left;
        } else {
            transform = { 0.0f, -(iiar - idar) * 0.5f * deviceAspectRatio, 1.0f, deviceAspectRatio * iiar };
            rect = { 0, static_cast<uint32_t>(floorf(std::max(0.0f, static_cast<Float>(height) * (iiar - idar) * 0.5f * imageAspectRatio))),
                     width, static_cast<uint32_t>(ceilf(static_cast<Float>(height) * (iiar + idar) * 0.5f * imageAspectRatio)) };
            rect.height -= rect.top;
        }
    }

    transform.sx /= static_cast<Float>(width);
    transform.sy /= static_cast<Float>(height);

    return { transform, rect };
}

void Sensor::updateTransform(const KeyFrames& keyFrames, const TimeInterval timeInterval) {
    mTransform = resolveTransform(keyFrames, timeInterval);
}

glm::vec2 parseSensorSize(const Ref<ConfigAttr>& attr) {
    if(attr->isArray())
        return parseVec2(attr) * 1e-3f;

    const auto type = attr->as<std::string_view>();
    // Please refer to https://lenspire.zeiss.com/photo/en/article/making-sense-of-sensors-full-frame-vs-aps-c
    if(type == "Full Frame"sv)
        return { 36e-3f, 24e-3f };
    if(type == "APS-C"sv)
        return { 22.2e-3f, 14.8e-3f };
    if(type == "MFT"sv)
        return { 17.3e-3f, 13e-3f };
    if(type == "APS-H"sv)
        return { 28.7e-3f, 19e-3f };
    if(type == "Foveon"sv)
        return { 20.7e-3f, 13.8e-3f };
    if(type == "1/1.7''"sv)
        return { 7.6e-3f, 5.6e-3f };
    if(type == "1/1.8''"sv)
        return { 7.18e-3f, 5.32e-3f };
    if(type == "1/2.5''"sv)
        return { 5.76e-3f, 4.29e-3f };
    fatal(fmt::format("Unrecognized sensor size \"{}\"", type));
}

PIPER_NAMESPACE_END
