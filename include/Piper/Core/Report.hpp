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
#include <Piper/Config.hpp>
#include <Piper/Core/RefCount.hpp>
#include <fmt/format.h>
#include <oneapi/tbb/concurrent_unordered_map.h>
#include <oneapi/tbb/concurrent_vector.h>

PIPER_NAMESPACE_BEGIN

enum class LogType { Info = 0, Warning = 1, Error = 2, Fatal = 3 };

const std::string& header(LogType type);
void info(std::string message);
void warning(std::string message);
void error(std::string message);
[[noreturn]] void fatal(std::string message);

tbb::concurrent_vector<std::pair<LogType, std::string>>& consoleOutput();
std::ofstream& logFile();

using Clock = std::chrono::system_clock;

class ProgressReporter final : public RefCountBase {
    Clock::time_point mStart;
    double mProgress;
    std::optional<Clock::time_point> mEstimatedEnd;
    std::optional<Clock::time_point> mEnd;

public:
    explicit ProgressReporter();
    void update(double progress) noexcept;
    double progress() const noexcept;
    std::optional<Clock::time_point> endTime();
    std::optional<Clock::duration> eta() const;
    Clock::duration elapsed() const;
};

class ProgressReporterHandle final {
    Ref<ProgressReporter> mReporter;

public:
    explicit ProgressReporterHandle(std::string name);
    void update(double progress) noexcept;
};

tbb::concurrent_unordered_map<std::string, Ref<ProgressReporter>>& getProgressReports();

PIPER_NAMESPACE_END
