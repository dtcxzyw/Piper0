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

#include <Piper/Core/Report.hpp>
#include <fstream>
#include <oneapi/tbb/spin_mutex.h>

PIPER_NAMESPACE_BEGIN

std::ofstream& logFile() {
    static std::ofstream logFile;
    return logFile;
}

tbb::concurrent_vector<std::pair<LogType, std::string>>& consoleOutput() {
    static tbb::concurrent_vector<std::pair<LogType, std::string>> vec;
    return vec;
}

const std::string& header(LogType type) {
    static const std::string headers[] = { "[INFO] "s, "[WARNING] "s, "[ERROR] "s, "[FATAL] "s };
    return headers[static_cast<uint32_t>(type)];
}

static void report(LogType type, std::string message, const bool forceSync = false) {
    const auto& headerOfLog = header(type);
    auto& stream = logFile();

    static tbb::speculative_spin_mutex mutex;

    if(stream) {
        tbb::speculative_spin_mutex::scoped_lock guard{ mutex };
        stream << headerOfLog << message << "\n";
        if(forceSync)
            stream.flush();
    }

    consoleOutput().push_back({ type, std::move(message) });
}

void info(std::string message) {
    report(LogType::Info, std::move(message));
}
void warning(std::string message) {
    report(LogType::Warning, std::move(message));
}
void error(std::string message) {
    report(LogType::Error, std::move(message));
}
[[noreturn]] void fatal(std::string message) {
    report(LogType::Fatal, std::move(message), true);
    std::abort();  // FIXME: interrupt for debugging
}

std::function renderCallback = [] {};

ProgressReporter::ProgressReporter() : mStart{ Clock::now() }, mProgress{ 0.0 } {}
void ProgressReporter::update(const double progress) noexcept {
    renderCallback();
    mProgress = std::fmin(1.0, progress);

    if(mProgress > 1e-7) {
        const auto delta = Clock::now() - mStart;
        mEstimatedEnd = mStart +
            Clock::duration{ static_cast<Clock::rep>(std::ceil(static_cast<double>(delta.count()) / std::fmin(0.999, mProgress))) };
    }
}
double ProgressReporter::progress() const noexcept {
    return mProgress;
}
std::optional<Clock::time_point> ProgressReporter::endTime() {
    if(refCount() == 1 && !mEnd)
        mEnd = Clock::now();

    return mEnd;
}
std::optional<Clock::duration> ProgressReporter::eta() const {
    if(mEstimatedEnd)
        return std::max(Clock::duration{ 0 }, mEstimatedEnd.value() - Clock::now());
    return std::nullopt;
}
Clock::duration ProgressReporter::elapsed() const {
    if(mEnd)
        return mEnd.value() - mStart;
    return Clock::now() - mStart;
}

tbb::concurrent_unordered_map<std::string, Ref<ProgressReporter>>& getProgressReports() {
    static tbb::concurrent_unordered_map<std::string, Ref<ProgressReporter>> inst;
    return inst;
}

ProgressReporterHandle::ProgressReporterHandle(std::string name) : mReporter{ makeRefCount<ProgressReporter>() } {
    getProgressReports().insert({ std::move(name), mReporter });
}
// ReSharper disable once CppMemberFunctionMayBeConst
void ProgressReporterHandle::update(const double progress) noexcept {
    mReporter->update(progress);
}

PIPER_NAMESPACE_END
