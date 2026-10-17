# SoftRam

A small Win32 tray utility that monitors memory and reclaims idle pages. Written in C++17 with the plain Win32 API.

**It does not add RAM.** Windows 10/11 already compresses memory itself, and the original 1990s "SoftRAM" claimed to double memory and didn't. SoftRam here only:

- shows physical memory and commit charge live (`GlobalMemoryStatusEx`)
- trims process working sets (`EmptyWorkingSet`), manually or automatically above 85% load
- optionally purges the standby list (`NtSetSystemInformation`, needs administrator)

Trimming moves idle pages out of RAM; apps fault them back in when needed, so it can make things slower. Use it to free memory for a big task, not as a permanent fix.

## Build

```
cmake -S . -B build
cmake --build build --config Release
```

Run `build/Release/SoftRam.exe` (MSVC) or `build/SoftRam.exe` (MinGW). Run as administrator to enable the standby purge.

## Layout

- `src/memory.{h,cpp}` – memory stats, working-set trim, standby purge
- `src/main.cpp` – window, tray icon, timer
