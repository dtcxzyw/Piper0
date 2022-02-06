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

#include <Piper/Core/Context.hpp>
#include <Piper/Core/FileIO.hpp>
#include <Piper/Core/Report.hpp>
#include <fstream>

PIPER_NAMESPACE_BEGIN
BinaryData loadData(const fs::path& path) {
    BinaryData data{ fs::file_size(path), context().globalAllocator };
    std::ifstream in{ path, std::ios::in | std::ios::binary };
    if(!in)
        fatal(fmt::format("Failed to open file \"{}\"", path.string()));
    in.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(data.size()));
    return data;
}
PIPER_NAMESPACE_END
