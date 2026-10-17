# SoftRam

A small Windows tray utility that monitors memory and reclaims idle pages. There are two implementations of the same app: C++17 with the plain Win32 API, and C# on .NET 9 with WinForms.

**It does not add RAM.** Windows 10/11 already compresses memory itself, and the original 1990s "SoftRAM" claimed to double memory and didn't. SoftRam here only:

- shows physical memory and commit charge live (`GlobalMemoryStatusEx`)
- trims process working sets (`EmptyWorkingSet`), manually or automatically above 85% load
- optionally purges the standby list (`NtSetSystemInformation`, needs administrator)

Trimming moves idle pages out of RAM; apps fault them back in when needed, so it can make things slower. Use it to free memory for a big task, not as a permanent fix.

## C++ (Win32)

```
cmake -S . -B build
cmake --build build --config Release
ctest --test-dir build --output-on-failure
```

Run `build/Release/SoftRam.exe` (MSVC) or `build/SoftRam.exe` (MinGW). Run as administrator to enable the standby purge.

- `src/memory.{h,cpp}` – memory stats, working-set trim, standby purge
- `src/main.cpp` – window, tray icon, timer
- `tests/memory_tests.cpp` – CTest unit tests

## .NET (WinForms)

```
cd dotnet
dotnet build -c Release
dotnet test -c Release
```

Run `dotnet/SoftRam.App/bin/Release/net9.0-windows/SoftRam.exe`.

- `dotnet/SoftRam.Core` – `MemoryManager` (P/Invoke): stats, working-set trim, standby purge
- `dotnet/SoftRam.App` – WinForms window and tray icon
- `dotnet/SoftRam.Tests` – xUnit tests
