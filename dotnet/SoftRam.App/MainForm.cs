using SoftRam.Core;

namespace SoftRam.App;

public sealed class MainForm : Form
{
    private const uint AutoTrimLoad = 85;  // percent
    private static readonly TimeSpan AutoTrimCooldown = TimeSpan.FromMinutes(1);

    private readonly Label _physLabel = new() { AutoSize = true, Location = new Point(16, 14) };
    private readonly ProgressBar _physBar = new() { Location = new Point(16, 36), Size = new Size(368, 22) };
    private readonly Label _commitLabel = new() { AutoSize = true, Location = new Point(16, 70) };
    private readonly ProgressBar _commitBar = new() { Location = new Point(16, 92), Size = new Size(368, 22) };
    private readonly Button _cleanButton = new() { Text = "Clean now", Location = new Point(16, 130), Size = new Size(110, 28) };
    private readonly Button _purgeButton = new() { Text = "Purge standby (admin)", Location = new Point(136, 130), Size = new Size(160, 28) };
    private readonly Label _compressedLabel = new() { AutoSize = true, Location = new Point(16, 232) };
    private readonly Label _reclaimedLabel = new() { AutoSize = true, Location = new Point(16, 254) };
    private readonly Label _status = new() { Text = "Ready.", AutoSize = true, Location = new Point(16, 176) };
    private readonly CheckBox _autoTrim = new() { Text = "SoftRam active (auto-trim at 85% load)", AutoSize = true, Location = new Point(16, 200), Checked = true };
    private readonly NotifyIcon _tray = new() { Text = "SoftRam", Icon = SystemIcons.Application, Visible = true };
    private readonly System.Windows.Forms.Timer _timer = new() { Interval = 1000 };

    private DateTime _lastAutoTrim = DateTime.MinValue;
    private ulong _reclaimedTotal;
    private bool _exiting;

    public MainForm()
    {
        Text = "SoftRam";
        ClientSize = new Size(400, 290);
        FormBorderStyle = FormBorderStyle.FixedSingle;
        MaximizeBox = false;

        Controls.AddRange([_physLabel, _physBar, _commitLabel, _commitBar, _cleanButton, _purgeButton, _status, _autoTrim, _compressedLabel, _reclaimedLabel]);

        var menu = new ContextMenuStrip();
        menu.Items.Add("Show", null, (_, _) => ShowForm());
        menu.Items.Add("Clean now", null, (_, _) => RunTrim());
        menu.Items.Add(new ToolStripSeparator());
        menu.Items.Add("Exit", null, (_, _) => { _exiting = true; Close(); });
        _tray.ContextMenuStrip = menu;
        _tray.DoubleClick += (_, _) => ShowForm();

        _cleanButton.Click += (_, _) => RunTrim();
        _purgeButton.Click += (_, _) => RunPurge();
        _timer.Tick += (_, _) => OnTick();
        _timer.Start();
        UpdateStats();
    }

    protected override void OnFormClosing(FormClosingEventArgs e)
    {
        if (!_exiting && e.CloseReason == CloseReason.UserClosing)
        {
            e.Cancel = true;  // hide to tray; Exit lives in the tray menu
            Hide();
        }
        base.OnFormClosing(e);
    }

    protected override void Dispose(bool disposing)
    {
        if (disposing)
        {
            _timer.Dispose();
            _tray.Dispose();
        }
        base.Dispose(disposing);
    }

    private void ShowForm()
    {
        Show();
        WindowState = FormWindowState.Normal;
        Activate();
    }

    private void OnTick()
    {
        var s = UpdateStats();
        if (_autoTrim.Checked
            && s.LoadPercent >= AutoTrimLoad
            && DateTime.UtcNow - _lastAutoTrim > AutoTrimCooldown)
        {
            _lastAutoTrim = DateTime.UtcNow;
            RunTrim();
        }
    }

    private MemoryStats UpdateStats()
    {
        var s = MemoryManager.GetStats();
        _physLabel.Text = $"Physical: {ToGiB(s.TotalPhys - s.AvailPhys):F1} / {ToGiB(s.TotalPhys):F1} GiB used  ({s.LoadPercent}%)";
        _physBar.Value = (int)Math.Min(s.LoadPercent, 100);

        ulong commitUsed = s.TotalCommit - s.AvailCommit;
        int commitPct = s.TotalCommit == 0 ? 0 : (int)(commitUsed * 100 / s.TotalCommit);
        _commitLabel.Text = $"Commit: {ToGiB(commitUsed):F1} / {ToGiB(s.TotalCommit):F1} GiB  ({commitPct}%)";
        _commitBar.Value = Math.Min(commitPct, 100);

        ulong compressed = MemoryManager.CompressedStoreBytes();
        _compressedLabel.Text = compressed == 0
            ? "Windows compressed store: not available"
            : $"Windows compressed store: {ToGiB(compressed):F2} GiB (built-in, not added by SoftRam)";
        _reclaimedLabel.Text = $"Reclaimed this session: {_reclaimedTotal / (1024.0 * 1024.0):F0} MiB (pages moved out of RAM, not extra RAM)";
        return s;
    }

    private void RunTrim()
    {
        var r = MemoryManager.TrimWorkingSets();
        _reclaimedTotal += r.BytesReleased;
        _status.Text = $"Trimmed {r.ProcessesTrimmed} processes ({r.ProcessesSkipped} skipped), " +
                       $"released {r.BytesReleased / (1024.0 * 1024.0):F0} MiB of working set.";
    }

    private void RunPurge()
    {
        _status.Text = MemoryManager.PurgeStandbyList() ? "Standby list purged." : "Purge failed: run as administrator.";
    }

    private static double ToGiB(ulong bytes) => bytes / (1024.0 * 1024.0 * 1024.0);
}
