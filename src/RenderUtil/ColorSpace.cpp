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

#include <OpenColorIO/OpenColorIO.h>
#include <Piper/Render/ColorSpace.hpp>
#include <Piper/Render/Spectrum.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <tbb/concurrent_unordered_map.h>

namespace color = OCIO_NAMESPACE;

PIPER_NAMESPACE_BEGIN

struct ProcessorPack final {
    color::ConstProcessorRcPtr processor;
    color::ConstCPUProcessorRcPtr cpuFloatProcessor;

    explicit ProcessorPack(color::ConstProcessorRcPtr ptr)
        : processor{ std::move(ptr) }, cpuFloatProcessor{ processor->getOptimizedCPUProcessor(color::BIT_DEPTH_F32, color::BIT_DEPTH_F32,
                                                                                              color::OPTIMIZATION_VERY_GOOD) } {}
};

class ColorSpaceConvertContext final {
    color::ConstConfigRcPtr mConfig;
    tbb::concurrent_unordered_map<std::string_view, ProcessorPack> mToStandardLinearRGB;
    tbb::concurrent_unordered_map<std::string_view, ProcessorPack> mToRGB;

public:
    ColorSpaceConvertContext() : mConfig{ color::GetCurrentConfig() } {}
    const ProcessorPack& getRGB2StandardLinearRGB(std::string_view colorSpace) {
        auto iter = mToStandardLinearRGB.find(colorSpace);
        if(iter == mToStandardLinearRGB.cend()) {
            iter =
                mToStandardLinearRGB.emplace(colorSpace, ProcessorPack{ mConfig->getProcessor(nameOfStandardLinearRGB, colorSpace.data()) })
                    .first;
        }
        return iter->second;
    }

    const ProcessorPack& getStandardLinearRGB2RGB(std::string_view colorSpace) {
        auto iter = mToRGB.find(colorSpace);
        if(iter == mToRGB.cend()) {
            iter = mToRGB.emplace(colorSpace, ProcessorPack{ mConfig->getProcessor(colorSpace.data(), nameOfStandardLinearRGB) }).first;
        }
        return iter->second;
    }
};

static ColorSpaceConvertContext& context() {
    static ColorSpaceConvertContext ctx;
    return ctx;
}

glm::vec3 convertRGB2StandardLinearRGB(const glm::vec3& valueRGB, const std::string_view colorSpace) {
    glm::vec3 res = valueRGB;
    const auto& processor = context().getRGB2StandardLinearRGB(colorSpace);
    processor.cpuFloatProcessor->applyRGB(glm::value_ptr(res));
    return res;
}

glm::vec3 convertStandardLinearRGB2RGB(const glm::vec3& valueStandardLinearRGB, const std::string_view colorSpace) {
    glm::vec3 res = valueStandardLinearRGB;
    const auto& processor = context().getStandardLinearRGB2RGB(colorSpace);
    processor.cpuFloatProcessor->applyRGB(glm::value_ptr(res));
    return res;
}

PIPER_NAMESPACE_END
