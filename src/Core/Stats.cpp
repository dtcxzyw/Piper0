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
#include <Piper/Core/Stats.hpp>
#include <fmt/chrono.h>
#include <fstream>
#include <magic_enum.hpp>
#include <map>
#include <numeric>
#include <oneapi/tbb/concurrent_vector.h>
#include <ranges>

PIPER_NAMESPACE_BEGIN

static tbb::concurrent_vector<StatsBase*>& getGlobalStats() {
    static tbb::concurrent_vector<StatsBase*> inst;
    return inst;
}
static tbb::concurrent_vector<LocalStatsBase*>& getLocalStats() {
    static tbb::concurrent_vector<LocalStatsBase*> inst;
    return inst;
}

static StatsCategory toCategory(const StatsType type) noexcept {
    if(StatsType::TracingBegin < type && type < StatsType::TracingEnd)
        return StatsCategory::Tracing;
    if(StatsType::ShadingBegin < type && type < StatsType::ShadingEnd)
        return StatsCategory::Shading;
    if(StatsType::TexturingBegin < type && type < StatsType::TexturingEnd)
        return StatsCategory::Texturing;
    PIPER_UNREACHABLE();
}

StatsBase::StatsBase(const StatsType type) : mType{ type } {
    getGlobalStats().push_back(this);
}

LocalStatsBase::LocalStatsBase() {
    getLocalStats().push_back(this);
}

void printStats() {
    for(const auto local : getLocalStats())
        local->accumulate();

    std::pmr::map<StatsCategory, std::pmr::map<StatsType, StatsBase*>> order{ context().globalAllocator };
    for(const auto stats : getGlobalStats())
        order[toCategory(stats->type())].insert({ stats->type(), stats });

    info("==================[Statistics]==================");

    for(auto& [category, stats] : order) {
        info(fmt::format("==================<{}>==================", magic_enum::enum_name(category)));
        for(const auto base : stats | std::views::values)
            base->print();
    }

    logFile().flush();
}

uint64_t rdtsc() {
    return __rdtsc();
}

void CounterBase::print() {
    info(fmt::format("{}: {}", magic_enum::enum_name(mType), mCount));
}

void HistogramBase::print() {
    const auto end = std::ranges::find(std::as_const(mCount), 0);
    const auto sum = std::accumulate(mCount.cbegin(), end, 0ULL);
    info(fmt::format("{}: {} count", magic_enum::enum_name(mType), sum));
    if(sum == 0)
        return;
    info("===========================================");
    uint32_t idx = 0;
    for(auto iter = mCount.begin(); iter != end; ++iter)
        info(fmt::format("{}: {} ({:.2f}%)", idx++, *iter, static_cast<double>(*iter) / static_cast<double>(sum) * 100.0));
    info("===========================================");
}

void TimerBase::print() {
    if(mCount == 0) {
        info(fmt::format("{}: 0 records", magic_enum::enum_name(mType)));
        return;
    }

    const auto mean = std::chrono::ceil<std::chrono::nanoseconds>(HClock::duration{ mSum / mCount });
    info(fmt::format("{}: {}, {:.2f} Mop/s ({} counts)", magic_enum::enum_name(mType), mean,
                     1'000.0 / static_cast<double>(std::max(1LL, mean.count())), mCount));
}

void TickTimerBase::print() {
    info(fmt::format("{}: {} cycles/op", magic_enum::enum_name(mType), mValue));
}

void BoolCounterBase::print() {
    const auto percent = static_cast<double>(mPositiveCount) / static_cast<double>(mCount) * 100.0;
    info(fmt::format("{}: positive {:.2f}% ({} count) | negative {:.2f}% ({} count) | total {}", magic_enum::enum_name(mType), percent,
                     mPositiveCount, 100.0 - percent, mCount - mPositiveCount, mCount));
}

PIPER_NAMESPACE_END
