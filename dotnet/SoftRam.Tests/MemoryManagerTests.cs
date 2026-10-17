using SoftRam.Core;
using Xunit;

namespace SoftRam.Tests;

public class MemoryManagerTests
{
    private static byte[] CommitBlock(int megabytes)
    {
        var block = new byte[megabytes * 1024 * 1024];
        Array.Fill(block, (byte)1);  // force the pages to be committed
        return block;
    }

    [Fact]
    public void StatsAreSane()
    {
        var s = MemoryManager.GetStats();

        Assert.True(s.TotalPhys > 0);
        Assert.True(s.AvailPhys <= s.TotalPhys);
        Assert.True(s.TotalCommit >= s.TotalPhys);
        Assert.True(s.AvailCommit <= s.TotalCommit);
        Assert.True(s.LoadPercent <= 100);
    }

    [Fact]
    public void StatsTrackAllocation()
    {
        ulong before = MemoryManager.GetStats().AvailCommit;

        var block = CommitBlock(64);
        ulong during = MemoryManager.GetStats().AvailCommit;
        GC.KeepAlive(block);

        Assert.True(during < before);
    }

    [Fact]
    public void TrimReleasesOwnWorkingSet()
    {
        var block = CommitBlock(64);
        ulong before = MemoryManager.CurrentProcessWorkingSet();

        var result = MemoryManager.TrimWorkingSets();

        Assert.True(result.ProcessesTrimmed >= 1);  // at minimum, this process
        Assert.True(MemoryManager.CurrentProcessWorkingSet() < before);
        Assert.Equal(1, block[0]);  // data survives, faulted back in
        Assert.Equal(1, block[^1]);
    }

    [Fact]
    public void TrimReportsBytesReleased()
    {
        var block = CommitBlock(64);

        var result = MemoryManager.TrimWorkingSets();
        GC.KeepAlive(block);

        Assert.True(result.BytesReleased > 0);
    }

    [Fact]
    public void PurgeDoesNotThrow()
    {
        // Succeeds only when elevated; either result is valid, it must just return.
        var ex = Record.Exception(() => MemoryManager.PurgeStandbyList());
        Assert.Null(ex);
    }
}
