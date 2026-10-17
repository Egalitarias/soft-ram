#pragma once

#include <cstdint>

namespace memory {

struct Stats {
    uint64_t totalPhys = 0;
    uint64_t availPhys = 0;
    uint64_t totalCommit = 0;
    uint64_t availCommit = 0;
    unsigned loadPercent = 0;
};

struct TrimResult {
    unsigned processesTrimmed = 0;
    unsigned processesSkipped = 0;  // access denied or protected
    uint64_t bytesReleased = 0;     // reduction in summed working sets
};

Stats GetStats();

// Empties the working set of every process we can open. Pages move to the
// standby list or pagefile; they are faulted back in if the process needs them.
TrimResult TrimWorkingSets();

// Purges the system standby list. Needs administrator rights.
// Returns false if the privilege or the call is unavailable.
bool PurgeStandbyList();

}  // namespace memory
