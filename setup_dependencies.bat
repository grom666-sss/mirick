@echo off
setlocal EnableExtensions

set "ROOT=%~dp0"
set "PLUGIN_SDK_DIR=%ROOT%thirdparty\plugin-sdk"
set "PREMAKE=%PLUGIN_SDK_DIR%\tools\premake\premake5.exe"
set "PREMAKE_FILE=%PLUGIN_SDK_DIR%\tools\premake\premake5.lua"

if not exist "%PLUGIN_SDK_DIR%\shared\plugin.h" (
    where git >nul 2>nul || (
        echo [ERROR] Git is not installed or is not available in PATH.
        exit /b 1
    )
    echo [1/3] Downloading Plugin-SDK...
    git clone --depth 1 https://github.com/DK22Pac/plugin-sdk.git "%PLUGIN_SDK_DIR%" || exit /b 1
) else (
    echo [1/3] Plugin-SDK is already downloaded.
)

if not exist "%PLUGIN_SDK_DIR%\plugin_sa\plugin_sa.vcxproj" (
    echo [2/3] Generating Plugin-SDK projects for Visual Studio 2022...
    "%PREMAKE%" vs2022 --file="%PREMAKE_FILE%" || exit /b 1
) else (
    echo [2/3] Plugin-SDK project is already generated.
)

where msbuild >nul 2>nul
if errorlevel 1 (
    set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
    if not exist "%VSWHERE%" (
        echo [ERROR] MSBuild was not found. Install Visual Studio with Desktop development with C++.
        exit /b 1
    )
    for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe`) do set "MSBUILD=%%i"
) else (
    set "MSBUILD=msbuild"
)

if not defined MSBUILD (
    echo [ERROR] MSBuild was not found. Install the Desktop development with C++ workload.
    exit /b 1
)

if not exist "%PLUGIN_SDK_DIR%\output\lib\plugin.lib" (
    echo [3/3] Building Plugin-SDK for GTA San Andreas ^(Release Win32^)...
    "%MSBUILD%" "%PLUGIN_SDK_DIR%\plugin_sa\plugin_sa.vcxproj" /m /p:Configuration=Release /p:Platform=Win32 || exit /b 1
) else (
    echo [3/3] plugin.lib is already built.
)

echo.
echo Dependencies are ready. Reopen ForkHack.slnx and build Release ^| Win32.
exit /b 0
