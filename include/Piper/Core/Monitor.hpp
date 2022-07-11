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
#include <Piper/Core/Context.hpp>
#include <optional>

PIPER_NAMESPACE_BEGIN

struct CurrentStatus final {
    std::pmr::vector<std::pair<double, double>> cores{ context().localAllocator };
    double userRatio = 0.0;
    double kernelRatio = 0.0;
    uint64_t memoryUsage = 0;
    uint64_t IOOps = 0;
    double readSpeed = 0.0;
    double writeSpeed = 0.0;
    uint64_t activeIOThread = 0;
    std::pmr::vector<std::string> customStatus;
};

class Monitor {
public:
    virtual std::optional<CurrentStatus> update() = 0;
    virtual void updateCustomStatus(void*, std::string message) = 0;
    [[nodiscard]] virtual uint32_t updateCount() const noexcept = 0;
    virtual ~Monitor() = default;
};

Monitor& getMonitor();

PIPER_NAMESPACE_END
