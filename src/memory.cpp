#include "memory.h"

#include <windows.h>
#include <psapi.h>

#include <vector>

namespace memory {

namespace {

constexpr ULONG kSystemMemoryListInformation = 80;
constexpr ULONG kMemoryPurgeStandbyList = 4;

using NtSetSystemInformationFn = LONG(NTAPI*)(ULONG, PVOID, ULONG);

bool EnablePrivilege(const wchar_t* name) {
    HANDLE token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &token)) {
        return false;
    }
    TOKEN_PRIVILEGES tp{};
    tp.PrivilegeCount = 1;
    tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
    bool ok = LookupPrivilegeValueW(nullptr, name, &tp.Privileges[0].Luid) &&
              AdjustTokenPrivileges(token, FALSE, &tp, sizeof(tp), nullptr, nullptr) &&
              GetLastError() == ERROR_SUCCESS;
    CloseHandle(token);
    return ok;
}

uint64_t WorkingSetBytes(HANDLE process) {
    PROCESS_MEMORY_COUNTERS pmc{};
    pmc.cb = sizeof(pmc);
    return GetProcessMemoryInfo(process, &pmc, sizeof(pmc)) ? pmc.WorkingSetSize : 0;
}

}  // namespace

Stats GetStats() {
    Stats s;
    MEMORYSTATUSEX ms{};
    ms.dwLength = sizeof(ms);
    if (GlobalMemoryStatusEx(&ms)) {
        s.totalPhys = ms.ullTotalPhys;
        s.availPhys = ms.ullAvailPhys;
        s.totalCommit = ms.ullTotalPageFile;
        s.availCommit = ms.ullAvailPageFile;
        s.loadPercent = ms.dwMemoryLoad;
    }
    return s;
}

TrimResult TrimWorkingSets() {
    TrimResult result;

    std::vector<DWORD> pids(1024);
    DWORD bytes = 0;
    while (EnumProcesses(pids.data(), static_cast<DWORD>(pids.size() * sizeof(DWORD)), &bytes) &&
           bytes == pids.size() * sizeof(DWORD)) {
        pids.resize(pids.size() * 2);
    }
    const size_t count = bytes / sizeof(DWORD);

    for (size_t i = 0; i < count; ++i) {
        if (pids[i] == 0) continue;
        HANDLE h = OpenProcess(PROCESS_SET_QUOTA | PROCESS_QUERY_INFORMATION, FALSE, pids[i]);
        if (!h) {
            ++result.processesSkipped;
            continue;
        }
        const uint64_t before = WorkingSetBytes(h);
        if (EmptyWorkingSet(h)) {
            const uint64_t after = WorkingSetBytes(h);
            if (before > after) result.bytesReleased += before - after;
            ++result.processesTrimmed;
        } else {
            ++result.processesSkipped;
        }
        CloseHandle(h);
    }
    return result;
}

bool PurgeStandbyList() {
    if (!EnablePrivilege(SE_PROF_SINGLE_PROCESS_NAME)) return false;

    HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    if (!ntdll) return false;
    auto fn = reinterpret_cast<NtSetSystemInformationFn>(
        reinterpret_cast<void*>(GetProcAddress(ntdll, "NtSetSystemInformation")));
    if (!fn) return false;

    ULONG command = kMemoryPurgeStandbyList;
    return fn(kSystemMemoryListInformation, &command, sizeof(command)) >= 0;
}

}  // namespace memory
