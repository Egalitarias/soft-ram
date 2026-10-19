using System.ComponentModel;
using System.Diagnostics;
using System.Runtime.InteropServices;

namespace SoftRam.Core;

public readonly record struct MemoryStats(
    ulong TotalPhys, ulong AvailPhys, ulong TotalCommit, ulong AvailCommit, uint LoadPercent);

public readonly record struct TrimResult(int ProcessesTrimmed, int ProcessesSkipped, ulong BytesReleased);

public static class MemoryManager
{
    public static MemoryStats GetStats()
    {
        var ms = new NativeMethods.MemoryStatusEx { dwLength = (uint)Marshal.SizeOf<NativeMethods.MemoryStatusEx>() };
        if (!NativeMethods.GlobalMemoryStatusEx(ref ms))
            throw new Win32Exception(Marshal.GetLastWin32Error());
        return new MemoryStats(ms.ullTotalPhys, ms.ullAvailPhys, ms.ullTotalPageFile, ms.ullAvailPageFile, ms.dwMemoryLoad);
    }

    /// <summary>
    /// Empties the working set of every process we can open. Pages move to the standby list
    /// or pagefile and are faulted back in if the process needs them.
    /// </summary>
    public static TrimResult TrimWorkingSets()
    {
        int trimmed = 0, skipped = 0;
        ulong released = 0;

        foreach (var process in Process.GetProcesses())
        {
            using (process)
            {
                if (process.Id == 0) continue;

                var handle = NativeMethods.OpenProcess(
                    NativeMethods.ProcessSetQuota | NativeMethods.ProcessQueryInformation, false, (uint)process.Id);
                if (handle == IntPtr.Zero)
                {
                    skipped++;
                    continue;
                }

                try
                {
                    ulong before = WorkingSetBytes(handle);
                    if (NativeMethods.EmptyWorkingSet(handle))
                    {
                        ulong after = WorkingSetBytes(handle);
                        if (before > after) released += before - after;
                        trimmed++;
                    }
                    else
                    {
                        skipped++;
                    }
                }
                finally
                {
                    NativeMethods.CloseHandle(handle);
                }
            }
        }

        return new TrimResult(trimmed, skipped, released);
    }

    /// <summary>Purges the system standby list. Needs administrator rights.</summary>
    public static bool PurgeStandbyList()
    {
        if (!EnablePrivilege("SeProfileSingleProcessPrivilege")) return false;

        int command = NativeMethods.MemoryPurgeStandbyList;
        return NativeMethods.NtSetSystemInformation(
            NativeMethods.SystemMemoryListInformation, ref command, sizeof(int)) >= 0;
    }

    /// <summary>
    /// Size of Windows' own compressed memory store (the "Memory Compression" process working set),
    /// in bytes. Returns 0 if the store is not present or not readable.
    /// </summary>
    public static ulong CompressedStoreBytes()
    {
        foreach (var name in new[] { "Memory Compression", "MemCompression" })
        {
            foreach (var process in Process.GetProcessesByName(name))
            {
                using (process)
                {
                    var handle = NativeMethods.OpenProcess(NativeMethods.ProcessQueryInformation, false, (uint)process.Id);
                    if (handle == IntPtr.Zero) continue;
                    try { return WorkingSetBytes(handle); }
                    finally { NativeMethods.CloseHandle(handle); }
                }
            }
        }
        return 0;
    }

    /// <summary>Working set of the current process, in bytes.</summary>
    public static ulong CurrentProcessWorkingSet() => WorkingSetBytes(NativeMethods.GetCurrentProcess());

    private static ulong WorkingSetBytes(IntPtr process)
    {
        return NativeMethods.GetProcessMemoryInfo(
            process, out var pmc, (uint)Marshal.SizeOf<NativeMethods.ProcessMemoryCounters>())
            ? pmc.WorkingSetSize
            : 0;
    }

    private static bool EnablePrivilege(string name)
    {
        if (!NativeMethods.OpenProcessToken(
                NativeMethods.GetCurrentProcess(),
                NativeMethods.TokenAdjustPrivileges | NativeMethods.TokenQuery, out var token))
            return false;

        try
        {
            if (!NativeMethods.LookupPrivilegeValue(null, name, out var luid)) return false;
            var tp = new NativeMethods.TokenPrivileges
            {
                PrivilegeCount = 1,
                Luid = luid,
                Attributes = NativeMethods.SePrivilegeEnabled,
            };
            return NativeMethods.AdjustTokenPrivileges(token, false, ref tp, 0, IntPtr.Zero, IntPtr.Zero)
                   && Marshal.GetLastWin32Error() == 0;
        }
        finally
        {
            NativeMethods.CloseHandle(token);
        }
    }
}
