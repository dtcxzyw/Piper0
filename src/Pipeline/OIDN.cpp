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

#include <OpenImageDenoise/oidn.hpp>
#include <Piper/Core/StaticFactory.hpp>
#include <Piper/Render/PipelineNode.hpp>

PIPER_NAMESPACE_BEGIN

class IntelOpenImageDenoiser final : public PipelineNode {
    oidn::DeviceRef mDevice = oidn::newDevice();

public:
    explicit IntelOpenImageDenoiser(const Ref<ConfigNode>& node) {}
    FrameGroup transform(FrameGroup group) override {
        PIPER_NOT_IMPLEMENTED();
    }

    ChannelRequirement setup(ChannelRequirement req) override {
        if(req.count(Channel::Full) || req.count(Channel::Direct) || req.count(Channel::Indirect)) {
            if(!req.count(Channel::Albedo))
                req[Channel::Albedo] = false;
            if(!req.count(Channel::ShadingNormal))
                req[Channel::ShadingNormal] = false;
        }
        return req;
    }
};

PIPER_REGISTER_CLASS(IntelOpenImageDenoiser, PipelineNode);

PIPER_NAMESPACE_END
