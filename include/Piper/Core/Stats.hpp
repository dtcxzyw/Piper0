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
#include <Piper/Core/Report.hpp>

PIPER_NAMESPACE_BEGIN

enum class StatsCategory { Tracing, Shading, Texturing };

enum class StatsType {
    TracingBegin,
    Intersection,
    Occlusion,
    TraceDepth,
    TracingEnd,
    ShadingBegin,
    ShadingEnd,
    TexturingBegin,
    Texture2D,
    TexturingEnd,
};

class LocalStatsBase {
public:
    LocalStatsBase();
    LocalStatsBase(const LocalStatsBase&) = delete;
    LocalStatsBase(LocalStatsBase&&) = delete;
    LocalStatsBase& operator=(LocalStatsBase&&) = delete;
    LocalStatsBase& operator=(const LocalStatsBase&) = delete;

    virtual void accumulate() = 0;
    virtual ~LocalStatsBase() = default;
};

class StatsBase {
protected:
    StatsType mType;

public:
    explicit StatsBase(StatsType type);
    StatsBase(const StatsBase&) = delete;
    StatsBase(StatsBase&&) = delete;
    StatsBase& operator=(StatsBase&&) = delete;
    StatsBase& operator=(const StatsBase&) = delete;

    [[nodiscard]] StatsType type() const noexcept {
        return mType;
    }
    virtual void print() = 0;
    virtual ~StatsBase() = default;
};

class CounterBase final : public StatsBase {
    uint64_t mCount = 0;

public:
    explicit CounterBase(const StatsType type) : StatsBase{ type } {}
    void add(const uint64_t count) noexcept {
        mCount += count;
    }
    void print() override;
};

class LocalCounterBase final : public LocalStatsBase {
    CounterBase& mBase;
    uint64_t mCount = 0;

public:
    explicit LocalCounterBase(CounterBase& base) : mBase{ base } {}
    void count() noexcept {
        ++mCount;
    }
    void accumulate() override {
        mBase.add(mCount);
    }
};

template <StatsType T>
class Counter final {
    static CounterBase& base() {
        static CounterBase base{ T };
        return base;
    }
    static LocalCounterBase& localBase() {
        thread_local static LocalCounterBase base{ Counter::base() };
        return base;
    }

public:
    Counter() = delete;
    static void count() noexcept {
        localBase().count();
    }
};

class BoolCounterBase final : public StatsBase {
    uint64_t mCount = 0, mPositiveCount = 0;

public:
    explicit BoolCounterBase(const StatsType type) : StatsBase{ type } {}
    void add(const uint64_t count, const uint64_t positiveCount) noexcept {
        mCount += count;
        mPositiveCount += positiveCount;
    }
    void print() override;
};

class LocalBoolCounterBase final : public LocalStatsBase {
    BoolCounterBase& mBase;
    uint64_t mCount = 0, mPositiveCount = 0;

public:
    explicit LocalBoolCounterBase(BoolCounterBase& base) : mBase{ base } {}
    void count(const bool res) noexcept {
        ++mCount;
        mPositiveCount += res;
    }
    void accumulate() override {
        mBase.add(mCount, mPositiveCount);
    }
};

template <StatsType T>
class BoolCounter final {
    static BoolCounterBase& base() {
        static BoolCounterBase base{ T };
        return base;
    }
    static LocalBoolCounterBase& localBase() {
        thread_local static LocalBoolCounterBase base{ BoolCounter::base() };
        return base;
    }

public:
    BoolCounter() = delete;
    static void count(const bool res) noexcept {
        localBase().count(res);
    }
};

class HistogramBase final : public StatsBase {
    std::array<uint64_t, 64> mCount = {};

public:
    explicit HistogramBase(const StatsType type) : StatsBase{ type } {}
    void add(const std::array<uint64_t, 64>& count) noexcept {
        for(size_t idx = 0; idx < mCount.size(); ++idx)
            mCount[idx] += count[idx];
    }
    void print() override;
};

class LocalHistogramBase final : public LocalStatsBase {
    HistogramBase& mBase;
    std::array<uint64_t, 64> mCount = {};

public:
    explicit LocalHistogramBase(HistogramBase& base) : mBase{ base } {}
    void count(const uint32_t idx) noexcept {
        ++mCount[std::min(63U, idx)];
    }
    void accumulate() override {
        mBase.add(mCount);
    }
};

template <StatsType T>
class Histogram final {
    static HistogramBase& base() {
        static HistogramBase base{ T };
        return base;
    }
    static LocalHistogramBase& localBase() {
        thread_local static LocalHistogramBase base{ Histogram::base() };
        return base;
    }

public:
    Histogram() = delete;
    static void count(const uint32_t idx) noexcept {
        localBase().count(idx);
    }
};

class TimerBase final : public StatsBase {
    uint64_t mSum = 0, mCount = 0;

public:
    explicit TimerBase(const StatsType type) : StatsBase{ type } {}
    void add(const uint64_t sum, const uint64_t count) noexcept {
        mSum += sum;
        mCount += count;
    }
    void print() override;
};

class LocalTimerBase final : public LocalStatsBase {
    TimerBase& mBase;
    uint64_t mSum = 0, mCount = 0;

public:
    explicit LocalTimerBase(TimerBase& base) : mBase{ base } {}
    void count(const uint64_t time) noexcept {
        mSum += time;
        ++mCount;
    }
    void accumulate() override {
        mBase.add(mSum, mCount);
    }
};

using HClock = std::chrono::high_resolution_clock;

template <StatsType T>
class Timer final {
    static TimerBase& base() {
        static TimerBase base{ T };
        return base;
    }
    static LocalTimerBase& localBase() {
        thread_local static LocalTimerBase base{ Timer::base() };
        return base;
    }

    HClock::time_point mBegin;

public:
    Timer() : mBegin{ HClock::now() } {}

    Timer(const Timer&) = delete;
    Timer(Timer&&) = delete;
    Timer& operator=(const Timer&) = delete;
    Timer& operator=(Timer&&) = delete;
    ~Timer() {
        localBase().count((HClock::now() - mBegin).count());
    }
};

class TickTimerBase final : public StatsBase {
    uint64_t mValue = std::numeric_limits<uint64_t>::max();

public:
    explicit TickTimerBase(const StatsType type) : StatsBase{ type } {}
    void add(const uint64_t val) noexcept {
        mValue = std::min(mValue, val);
    }
    void print() override;
};

class LocalTickTimerBase final : public LocalStatsBase {
    TickTimerBase& mBase;
    uint64_t mValue = std::numeric_limits<uint64_t>::max();

public:
    explicit LocalTickTimerBase(TickTimerBase& base) : mBase{ base } {}
    void count(const uint64_t val) noexcept {
        mValue = std::min(mValue, val);
    }
    void accumulate() override {
        mBase.add(mValue);
    }
};

uint64_t rdtsc();

template <StatsType T>
class TickTimer final {
    static TickTimerBase& base() {
        static TickTimerBase base{ T };
        return base;
    }
    static LocalTickTimerBase& localBase() {
        thread_local static LocalTickTimerBase base{ TickTimer::base() };
        return base;
    }

    uint64_t mBegin;

public:
    TickTimer() : mBegin{ rdtsc() } {}

    TickTimer(const TickTimer&) = delete;
    TickTimer(TickTimer&&) = delete;
    TickTimer& operator=(const TickTimer&) = delete;
    TickTimer& operator=(TickTimer&&) = delete;
    ~TickTimer() {
        localBase().count(rdtsc() - mBegin);
    }
};

void printStats();

PIPER_NAMESPACE_END
