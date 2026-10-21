# mirick

mirick — internal cheat for MTA:SA FORKS like NEXTRP, MTA PROVINCE for offcial MTA you need update bypass.cpp/hpp.

Made as a base for your own projects and easy to understand for beginners. Everything is split into separate files, so features, hooks and menu are easy to find and edit.

## Features

* Rage
* Legit
* Visuals
* Misc
* Custom ImGui menu
* Binds
* Configs
* Lua 5.4 scripting through sol2

## Lua scripts

Place scripts in `C:\Astral\scripts`. All `.lua` files are loaded when mirick starts. Script errors and output are written to `C:\Astral\lua.log`. You can also open **Profile → Lua**, paste code into the in-game editor and run it immediately. The editor supports multiline text, clipboard paste and tab characters.

Available callbacks and API:

```lua
function on_load()
    mirick.log("script loaded; API " .. mirick.version)
end

function on_tick(delta_time)
    -- Called once per rendered frame.
end

function on_unload()
    mirick.log("script unloaded")
end
```

Lua runs with the base, coroutine, math, string, table and UTF-8 libraries. The OS, I/O, debug and package libraries are intentionally not exposed.

## Build

**Release | Win32**

Requirements:

* Visual Studio 2026 with the **Desktop development with C++** workload and the **v145** toolset;
* an internet connection during the first build.

The first build runs `setup_dependencies.bat` automatically. The script downloads [Lua 5.4.9](https://github.com/lua/lua), [sol2 3.5.0](https://github.com/ThePhD/sol2) and [Plugin-SDK](https://github.com/DK22Pac/plugin-sdk) using Git, or a PowerShell ZIP fallback when Git is unavailable. It then generates a Visual Studio 2026/v145 project and builds the required `plugin.lib`. All dependencies use repository-local paths under `thirdparty`; no user-specific absolute paths are required.

Then open `mirick.sln` (the traditional solution format with the widest Visual Studio compatibility), select **Release | Win32**, and build the project. `mirick.slnx` is also provided for Visual Studio versions that support the newer solution format.

## Capture protection

ESP, skeletons, snaplines, binds and the menu are rendered in a separate top-level overlay protected with `WDA_EXCLUDEFROMCAPTURE`. OBS Game Capture receives only the GTA render device, while Windows display/window capture excludes the protected overlay. Use **borderless/windowed mode**: Windows cannot place a separate overlay above an exclusive-fullscreen DirectX 9 surface. Capture exclusion requires Windows 10 version 2004 or newer. If protected overlay creation fails, visuals are disabled rather than rendered into the captured game frame.

If Visual Studio was already open when the dependencies were installed, close and reopen the solution so IntelliSense refreshes its include paths.

## Author

**m1racl**

## Special Thanks

* **ThatRuxe**
