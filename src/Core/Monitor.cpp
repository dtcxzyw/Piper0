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
#include <Piper/Core/Monitor.hpp>
#include <memory_resource>
#include <vector>

#ifdef PIPER_WINDOWS
#include <Windows.h>
//
#include <Psapi.h>
#include <TlHelp32.h>
#include <winternl.h>
#endif

PIPER_NAMESPACE_BEGIN

struct CoreInfo final {
    uint64_t userTime = 0;
    uint64_t kernelTime = 0;
    uint64_t totalTime = 0;
};

struct CurrentCheckpoint final {
    uint64_t recordTime = 0;

    std::pmr::vector<CoreInfo> cores{ context().localAllocator };
    uint64_t userTime = 0;
    uint64_t kernelTime = 0;
    uint64_t memoryUsage = 0;
    uint64_t IOOps = 0;
    uint64_t readCount = 0;   // in bytes
    uint64_t writeCount = 0;  // in bytes
    uint64_t activeIOThread = 0;
};

class MonitorImpl final : public Monitor {
    static constexpr auto unavailable = std::numeric_limits<uint64_t>::max();

#ifdef PIPER_WINDOWS

    static constexpr auto increment = 100ULL;  // 100ns
    static uint64_t toNanosecond(const FILETIME& fileTime) {
        return ((static_cast<uint64_t>(fileTime.dwHighDateTime) << 32) | static_cast<uint64_t>(fileTime.dwLowDateTime)) * increment;
    }

    static NTSTATUS NtQuerySystemInformation(IN SYSTEM_INFORMATION_CLASS SystemInformationClass, OUT PVOID SystemInformation,
                                             IN ULONG SystemInformationLength, OUT PULONG ReturnLength OPTIONAL) {
        const auto mod = LoadLibraryA("ntdll.dll");
        const auto address = GetProcAddress(mod, "NtQuerySystemInformation");
        FreeLibrary(mod);
        return reinterpret_cast<decltype(&::NtQuerySystemInformation)>(address)(SystemInformationClass, SystemInformation,
                                                                                SystemInformationLength, ReturnLength);
    }

    static CurrentCheckpoint checkpoint() {
        // TODO: EnableThreadProfiling
        // TODO: error handling

        CurrentCheckpoint res;

        LARGE_INTEGER timestamp;
        QueryPerformanceCounter(&timestamp);
        LARGE_INTEGER frequency;
        QueryPerformanceFrequency(&frequency);
        constexpr auto ns = 1'000'000'000LL;
        if(frequency.QuadPart > ns)
            res.recordTime = timestamp.QuadPart / (frequency.QuadPart / ns);
        else
            res.recordTime = timestamp.QuadPart * (ns / frequency.QuadPart);

        SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION coreInfo[128];
        ULONG returnLength = 0;
        NtQuerySystemInformation(SYSTEM_INFORMATION_CLASS::SystemProcessorPerformanceInformation, &coreInfo, sizeof(coreInfo),
                                 &returnLength);
        const auto cores = returnLength / sizeof(coreInfo[0]);
        res.cores.resize(cores);
        for(ULONG coreIdx = 0; coreIdx < cores; ++coreIdx) {
            const auto& ref = coreInfo[coreIdx];
            const uint64_t idle = ref.IdleTime.QuadPart;
            const uint64_t user = ref.UserTime.QuadPart;
            const uint64_t kernel = ref.KernelTime.QuadPart;
            res.cores[coreIdx] = { user, kernel - idle, user + kernel };
        }

        const auto process = GetCurrentProcess();

        FILETIME creation, exit, user, kernel;
        GetProcessTimes(process, &creation, &exit, &kernel, &user);

        res.userTime = toNanosecond(user);
        res.kernelTime = toNanosecond(kernel);

        PROCESS_MEMORY_COUNTERS memCounter;
        GetProcessMemoryInfo(process, &memCounter, sizeof(memCounter));
        res.memoryUsage = memCounter.WorkingSetSize;

        IO_COUNTERS ioCounters;
        GetProcessIoCounters(process, &ioCounters);
        res.IOOps = ioCounters.ReadOperationCount + ioCounters.WriteOperationCount + ioCounters.OtherOperationCount;
        res.readCount = ioCounters.ReadTransferCount;
        res.writeCount = ioCounters.WriteOperationCount;

        const auto hThreadSnap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);

        THREADENTRY32 te32;
        te32.dwSize = sizeof(THREADENTRY32);
        Thread32First(hThreadSnap, &te32);
        const auto owner = GetCurrentProcessId();

        do {
            if(te32.th32OwnerProcessID == owner) {
                const auto handle = OpenThread(THREAD_QUERY_INFORMATION, FALSE, te32.th32ThreadID);
                BOOL pending = FALSE;
                GetThreadIOPendingFlag(handle, &pending);
                if(pending)
                    ++res.activeIOThread;
                CloseHandle(handle);
            }
        } while(Thread32Next(hThreadSnap, &te32));

        CloseHandle(hThreadSnap);

        return res;
    }
#endif

    static CurrentStatus diff(const CurrentCheckpoint& last, const CurrentCheckpoint& now) {
        CurrentStatus res;
        const auto dt = static_cast<double>(now.recordTime - last.recordTime);
        const uint32_t cores = now.cores.size();
        res.cores.resize(cores);
        for(uint32_t idx = 0; idx < cores; ++idx) {
            const auto [user0, kernel0, total0] = last.cores[idx];
            const auto [user1, kernel1, total1] = now.cores[idx];
            const auto user = static_cast<double>(user1 - user0), kernel = static_cast<double>(kernel1 - kernel0),
                       total = static_cast<double>(total1 - total0);
            res.cores[idx] = { user / total, kernel / total };
        }

        res.userRatio = static_cast<double>(now.userTime - last.userTime) / (cores * dt);
        res.kernelRatio = static_cast<double>(now.kernelTime - last.kernelTime) / (cores * dt);
        res.memoryUsage = now.memoryUsage;
        res.IOOps = now.IOOps - last.IOOps;
        res.readSpeed = static_cast<double>(now.readCount - last.readCount) / (1e-9 * dt);
        res.writeSpeed = static_cast<double>(now.writeCount - last.writeCount) / (1e-9 * dt);
        res.activeIOThread = last.activeIOThread;
        return res;
    }

    std::optional<CurrentCheckpoint> mLastCheckpoint;

public:
    std::optional<CurrentStatus> update() override {
        auto current = checkpoint();
        std::optional<CurrentStatus> res;
        if(mLastCheckpoint)
            res = diff(mLastCheckpoint.value(), current);

        mLastCheckpoint = std::move(current);
        return res;
    }
};

Monitor& getMonitor() {
    static MonitorImpl monitor;
    return monitor;
}

PIPER_NAMESPACE_END
