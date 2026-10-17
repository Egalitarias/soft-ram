#include <windows.h>
#include <shellapi.h>

#include <cstdio>
#include <cwchar>
#include <initializer_list>

#include "memory.h"

namespace {

constexpr wchar_t kClassName[] = L"SoftRamWindow";
constexpr UINT WM_TRAY = WM_APP + 1;
constexpr UINT_PTR kTimerId = 1;
constexpr UINT kTimerMs = 1000;
constexpr unsigned kAutoTrimLoad = 85;  // percent
constexpr DWORD kAutoTrimCooldownMs = 60 * 1000;

enum : int {
    IdCleanBtn = 100,
    IdPurgeBtn,
    IdAutoCheck,
    IdMenuShow = 200,
    IdMenuClean,
    IdMenuExit,
};

HWND g_clean, g_purge, g_auto;
wchar_t g_status[256] = L"Ready.";
DWORD g_lastAutoTrim = 0;

double ToGiB(uint64_t b) { return static_cast<double>(b) / (1024.0 * 1024.0 * 1024.0); }

void RunTrim() {
    const auto r = memory::TrimWorkingSets();
    swprintf_s(g_status, L"Trimmed %u processes (%u skipped), released %.0f MiB of working set.",
               r.processesTrimmed, r.processesSkipped,
               static_cast<double>(r.bytesReleased) / (1024.0 * 1024.0));
}

void RunPurge() {
    if (memory::PurgeStandbyList()) {
        wcscpy_s(g_status, L"Standby list purged.");
    } else {
        wcscpy_s(g_status, L"Purge failed: run as administrator.");
    }
}

void DrawBar(HDC dc, int x, int y, int w, int h, unsigned percent, const wchar_t* label) {
    RECT frame{x, y, x + w, y + h};
    FillRect(dc, &frame, static_cast<HBRUSH>(GetStockObject(WHITE_BRUSH)));
    RECT fill{x, y, x + w * static_cast<int>(percent) / 100, y + h};
    HBRUSH b = CreateSolidBrush(percent >= kAutoTrimLoad ? RGB(200, 60, 50) : RGB(60, 140, 220));
    FillRect(dc, &fill, b);
    DeleteObject(b);
    FrameRect(dc, &frame, static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));
    SetBkMode(dc, TRANSPARENT);
    RECT text = frame;
    DrawTextW(dc, label, -1, &text, DT_SINGLELINE | DT_VCENTER | DT_CENTER);
}

void Paint(HWND hwnd) {
    PAINTSTRUCT ps;
    HDC dc = BeginPaint(hwnd, &ps);
    SelectObject(dc, GetStockObject(DEFAULT_GUI_FONT));
    SetBkMode(dc, TRANSPARENT);

    const auto s = memory::GetStats();
    wchar_t buf[160];

    swprintf_s(buf, L"Physical: %.1f / %.1f GiB used  (%u%%)", ToGiB(s.totalPhys - s.availPhys),
               ToGiB(s.totalPhys), s.loadPercent);
    TextOutW(dc, 16, 14, buf, static_cast<int>(wcslen(buf)));
    swprintf_s(buf, L"%u%%", s.loadPercent);
    DrawBar(dc, 16, 36, 368, 22, s.loadPercent, buf);

    const uint64_t commitUsed = s.totalCommit - s.availCommit;
    const unsigned commitPct =
        s.totalCommit ? static_cast<unsigned>(commitUsed * 100 / s.totalCommit) : 0;
    swprintf_s(buf, L"Commit: %.1f / %.1f GiB  (%u%%)", ToGiB(commitUsed), ToGiB(s.totalCommit),
               commitPct);
    TextOutW(dc, 16, 70, buf, static_cast<int>(wcslen(buf)));
    swprintf_s(buf, L"%u%%", commitPct);
    DrawBar(dc, 16, 92, 368, 22, commitPct, buf);

    TextOutW(dc, 16, 176, g_status, static_cast<int>(wcslen(g_status)));
    EndPaint(hwnd, &ps);
}

void AddTrayIcon(HWND hwnd) {
    NOTIFYICONDATAW nid{};
    nid.cbSize = sizeof(nid);
    nid.hWnd = hwnd;
    nid.uID = 1;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = WM_TRAY;
    nid.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    wcscpy_s(nid.szTip, L"SoftRam");
    Shell_NotifyIconW(NIM_ADD, &nid);
}

void RemoveTrayIcon(HWND hwnd) {
    NOTIFYICONDATAW nid{};
    nid.cbSize = sizeof(nid);
    nid.hWnd = hwnd;
    nid.uID = 1;
    Shell_NotifyIconW(NIM_DELETE, &nid);
}

void ShowTrayMenu(HWND hwnd) {
    HMENU menu = CreatePopupMenu();
    AppendMenuW(menu, MF_STRING, IdMenuShow, L"Show");
    AppendMenuW(menu, MF_STRING, IdMenuClean, L"Clean now");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, IdMenuExit, L"Exit");
    POINT pt;
    GetCursorPos(&pt);
    SetForegroundWindow(hwnd);
    TrackPopupMenu(menu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, nullptr);
    DestroyMenu(menu);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE:
        g_clean = CreateWindowW(L"BUTTON", L"Clean now", WS_CHILD | WS_VISIBLE, 16, 130, 110, 28,
                                hwnd, reinterpret_cast<HMENU>(IdCleanBtn), nullptr, nullptr);
        g_purge = CreateWindowW(L"BUTTON", L"Purge standby (admin)", WS_CHILD | WS_VISIBLE, 136,
                                130, 160, 28, hwnd, reinterpret_cast<HMENU>(IdPurgeBtn), nullptr,
                                nullptr);
        g_auto = CreateWindowW(L"BUTTON", L"Auto-trim at 85% load",
                               WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 16, 200, 200, 22, hwnd,
                               reinterpret_cast<HMENU>(IdAutoCheck), nullptr, nullptr);
        for (HWND h : {g_clean, g_purge, g_auto}) {
            SendMessageW(h, WM_SETFONT, reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)),
                         TRUE);
        }
        AddTrayIcon(hwnd);
        SetTimer(hwnd, kTimerId, kTimerMs, nullptr);
        return 0;

    case WM_TIMER:
        if (SendMessageW(g_auto, BM_GETCHECK, 0, 0) == BST_CHECKED &&
            memory::GetStats().loadPercent >= kAutoTrimLoad &&
            GetTickCount() - g_lastAutoTrim > kAutoTrimCooldownMs) {
            g_lastAutoTrim = GetTickCount();
            RunTrim();
        }
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;

    case WM_COMMAND:
        switch (LOWORD(wp)) {
        case IdCleanBtn:
        case IdMenuClean: RunTrim(); break;
        case IdPurgeBtn: RunPurge(); break;
        case IdMenuShow:
            ShowWindow(hwnd, SW_SHOW);
            SetForegroundWindow(hwnd);
            break;
        case IdMenuExit: DestroyWindow(hwnd); return 0;
        }
        InvalidateRect(hwnd, nullptr, TRUE);
        return 0;

    case WM_TRAY:
        if (lp == WM_LBUTTONDBLCLK) {
            ShowWindow(hwnd, SW_SHOW);
            SetForegroundWindow(hwnd);
        } else if (lp == WM_RBUTTONUP) {
            ShowTrayMenu(hwnd);
        }
        return 0;

    case WM_PAINT: Paint(hwnd); return 0;

    case WM_CLOSE:  // hide to tray; Exit lives in the tray menu
        ShowWindow(hwnd, SW_HIDE);
        return 0;

    case WM_DESTROY:
        KillTimer(hwnd, kTimerId);
        RemoveTrayIcon(hwnd);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

}  // namespace

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE, PWSTR, int show) {
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
    wc.lpszClassName = kClassName;
    wc.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    if (!RegisterClassExW(&wc)) return 1;

    HWND hwnd = CreateWindowExW(0, kClassName, L"SoftRam", WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
                                CW_USEDEFAULT, CW_USEDEFAULT, 416, 290, nullptr, nullptr, inst,
                                nullptr);
    if (!hwnd) return 1;
    ShowWindow(hwnd, show);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return static_cast<int>(msg.wParam);
}
