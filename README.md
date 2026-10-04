# ForkHack

ForkHack — internal cheat for MTA:SA FORKS like NEXTRP, MTA PROVINCE for offcial MTA you need update bypass.cpp/hpp.

Made as a base for your own projects and easy to understand for beginners. Everything is split into separate files, so features, hooks and menu are easy to find and edit.

## Features

* Rage
* Legit
* Visuals
* Misc
* Custom ImGui menu
* Binds
* Configs

## Build

**Release | Win32**

Requirements:

* Visual Studio 2026 with the **Desktop development with C++** workload and the **v145** toolset;
* an internet connection during the first build.

The first build runs `setup_dependencies.bat` automatically. The script downloads [Plugin-SDK](https://github.com/DK22Pac/plugin-sdk) using Git, or a PowerShell ZIP fallback when Git is unavailable. It then generates a Visual Studio 2026/v145 project and builds the required `plugin.lib`. ForkHack uses the repository-local path `thirdparty\plugin-sdk`; no user-specific absolute paths are required.

Then open `ForkHack.slnx`, select **Release | Win32**, build the project and inject the DLL into `gta_sa.exe`.

## Capture protection

ESP, skeletons, snaplines, binds and the menu are rendered in a separate top-level overlay protected with `WDA_EXCLUDEFROMCAPTURE`. OBS Game Capture receives only the GTA render device, while Windows display/window capture excludes the protected overlay. Use **borderless/windowed mode**: Windows cannot place a separate overlay above an exclusive-fullscreen DirectX 9 surface. Capture exclusion requires Windows 10 version 2004 or newer. If protected overlay creation fails, visuals are disabled rather than rendered into the captured game frame.

If Visual Studio was already open when the dependencies were installed, close and reopen the solution so IntelliSense refreshes its include paths.

## Author

**Gabrik1337**

## Special Thanks

* **ShunK**
* **Akira**
* **DroidZero // NtKernelMC**
* **Kirill Sorokin // gamesnus**
