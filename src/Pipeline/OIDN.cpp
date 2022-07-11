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
#include <magic_enum.hpp>
#include <unordered_set>

PIPER_NAMESPACE_BEGIN

class IntelOpenImageDenoiser final : public PipelineNode {
    oidn::DeviceRef mDevice = oidn::newDevice();
    bool mEnable = true;

public:
    explicit IntelOpenImageDenoiser(const Ref<ConfigNode>& node) {
        if(const auto enable = node->tryGet("Enable"sv))
            mEnable = (*enable)->as<bool>();

        mDevice.setErrorFunction([](void*, const oidn::Error code, const char* message) {
            fatal(fmt::format("[ERROR] {}: {}", magic_enum::enum_name(code), message));
        });
        mDevice.commit();
    }
    Ref<Frame> transform(Ref<Frame> frame) override {
        if(!mEnable)
            return frame;

        const auto& metadata = frame->metadata();
        if(metadata.spectrumType == SpectrumType::Mono) {
            warning("Cannot denoise mono image. Skipped.");
            return frame;
        }

        const std::pmr::unordered_set<Channel> channels{ metadata.channels.cbegin(), metadata.channels.cend(), 16, context().localAllocator};
        std::pmr::vector<oidn::FilterRef> filters{ context().localAllocator };
        const auto base = const_cast<float*>(frame->data().data());
        auto data = frame->data();
        constexpr auto format = oidn::Format::Float3;

        if(channels.contains(Channel::Albedo)) {
            auto filter = mDevice.newFilter("RT");
            const auto view = metadata.view(Channel::Albedo);
            filter.setImage("albedo", base, format, metadata.width, metadata.height, view.byteStride, view.pixelStride, view.rowStride);
            filter.setImage("output", data.data(), format, metadata.width, metadata.height, view.byteStride, view.pixelStride,
                            view.rowStride);
            filter.commit();
            filters.push_back(std::move(filter));
        }

        if(channels.contains(Channel::ShadingNormal)) {
            auto filter = mDevice.newFilter("RT");
            const auto view = metadata.view(Channel::ShadingNormal);
            filter.setImage("normal", base, format, metadata.width, metadata.height, view.byteStride, view.pixelStride, view.rowStride);
            filter.setImage("output", data.data(), format, metadata.width, metadata.height, view.byteStride, view.pixelStride,
                            view.rowStride);
            filter.commit();
            filters.push_back(std::move(filter));
        }

        if(channels.contains(Channel::Color)) {
            auto filter = mDevice.newFilter("RT");
            const auto view = metadata.view(Channel::Color);
            filter.setImage("color", base, format, metadata.width, metadata.height, view.byteStride, view.pixelStride, view.rowStride);
            filter.setImage("output", data.data(), format, metadata.width, metadata.height, view.byteStride, view.pixelStride,
                            view.rowStride);
            filter.set("hdr", metadata.isHDR);

            if(channels.contains(Channel::Albedo) && channels.contains(Channel::ShadingNormal)) {
                // FIXME: prefilter doesn't work
                // filter.set("cleanAux", true);
                const auto albedoView = metadata.view(Channel::Albedo);
                filter.setImage("albedo", data.data(), format, metadata.width, metadata.height, albedoView.byteStride,
                                albedoView.pixelStride, albedoView.rowStride);

                const auto normalView = metadata.view(Channel::ShadingNormal);
                filter.setImage("normal", data.data(), format, metadata.width, metadata.height, normalView.byteStride,
                                normalView.pixelStride, normalView.rowStride);
            }

            filter.commit();
            filters.push_back(std::move(filter));
        }

        for(auto& filter : filters)
            filter.execute();

        return makeRefCount<Frame>(metadata, std::move(data));
    }

    ChannelRequirement setup(ChannelRequirement req) override {
        if(req.contains(Channel::Color) && mEnable) {
            if(!req.contains(Channel::Albedo))
                req[Channel::Albedo] = false;
            if(!req.contains(Channel::ShadingNormal))
                req[Channel::ShadingNormal] = false;
        }
        return req;
    }
};

PIPER_REGISTER_CLASS(IntelOpenImageDenoiser, PipelineNode);

PIPER_NAMESPACE_END
