#include <windows.h>
#include <psapi.h>

#include <cstdio>
#include <cstring>
#include <vector>

#include "memory.h"

namespace {

int g_failures = 0;

#define CHECK(cond)                                                              \
    do {                                                                         \
        if (!(cond)) {                                                           \
            std::printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);         \
            ++g_failures;                                                        \
        }                                                                        \
    } while (0)

uint64_t OwnWorkingSet() {
    PROCESS_MEMORY_COUNTERS pmc{};
    pmc.cb = sizeof(pmc);
    GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc));
    return pmc.WorkingSetSize;
}

void TestStatsAreSane() {
    const auto s = memory::GetStats();
    CHECK(s.totalPhys > 0);
    CHECK(s.availPhys <= s.totalPhys);
    CHECK(s.totalCommit >= s.totalPhys);
    CHECK(s.availCommit <= s.totalCommit);
    CHECK(s.loadPercent <= 100);
}

void TestStatsTrackAllocation() {
    const uint64_t before = memory::GetStats().availCommit;
    std::vector<char> block(64u * 1024 * 1024);
    std::memset(block.data(), 1, block.size());  // force the pages to be committed
    const uint64_t during = memory::GetStats().availCommit;
    CHECK(during < before);
}

void TestTrimReleasesOwnWorkingSet() {
    std::vector<char> block(64u * 1024 * 1024);
    std::memset(block.data(), 1, block.size());
    const uint64_t before = OwnWorkingSet();

    const auto r = memory::TrimWorkingSets();

    CHECK(r.processesTrimmed >= 1);  // at minimum, this process
    CHECK(OwnWorkingSet() < before);
    CHECK(block[0] == 1 && block.back() == 1);  // data survives, faulted back in
}

void TestTrimReportsBytesReleased() {
    std::vector<char> block(64u * 1024 * 1024);
    std::memset(block.data(), 1, block.size());
    const auto r = memory::TrimWorkingSets();
    CHECK(r.bytesReleased > 0);
}

void TestPurgeDoesNotCrash() {
    // Succeeds only when elevated; either result is valid, it must just return.
    const bool ok = memory::PurgeStandbyList();
    std::printf("  (purge standby returned %s)\n", ok ? "true" : "false");
}

struct Test {
    const char* name;
    void (*fn)();
};

}  // namespace

int main() {
    const Test tests[] = {
        {"stats are sane", TestStatsAreSane},
        {"stats track allocation", TestStatsTrackAllocation},
        {"trim releases own working set", TestTrimReleasesOwnWorkingSet},
        {"trim reports bytes released", TestTrimReportsBytesReleased},
        {"purge does not crash", TestPurgeDoesNotCrash},
    };
    for (const auto& t : tests) {
        const int before = g_failures;
        std::printf("[ RUN  ] %s\n", t.name);
        t.fn();
        std::printf("[ %s ] %s\n", g_failures == before ? " OK " : "FAIL", t.name);
    }
    std::printf("%d failure(s)\n", g_failures);
    return g_failures == 0 ? 0 : 1;
}
