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

#include <Piper/Render/Texture.hpp>

PIPER_NAMESPACE_BEGIN

template <typename Setting>
class CheckerBoard final : public SpectrumTexture2D<Setting> {
    PIPER_IMPORT_SETTINGS();

    Ref<SpectrumTexture2D<Setting>> mWhite, mBlack;
    glm::vec2 mInvSize;

public:
    explicit CheckerBoard(const Ref<ConfigNode>& node)
        : mWhite{ this->template make<SpectrumTexture2D>(node->get("White"sv)->as<Ref<ConfigNode>>()) },
          mBlack{ this->template make<SpectrumTexture2D>(node->get("Black"sv)->as<Ref<ConfigNode>>()) }, mInvSize{ rcp(
                                                                                                     parseVec2(node->get("Size"sv))) } {}

    Spectrum evaluate(const TexCoord texCoord, const Wavelength& sampledWavelength) const noexcept override {
        glm::vec2 intCoord;
        const auto newTexCoord = glm::modf(texCoord * mInvSize, intCoord);
        return ((static_cast<uint32_t>(intCoord.x) ^ static_cast<uint32_t>(intCoord.y)) & 1 ? mWhite : mBlack)
            ->evaluate(newTexCoord, sampledWavelength);
    }
};

PIPER_REGISTER_VARIANT(CheckerBoard, SpectrumTexture2D);

PIPER_NAMESPACE_END
