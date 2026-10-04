@echo off
setlocal EnableExtensions EnableDelayedExpansion
chcp 65001 >nul
set "VSLANG=1033"

set "ROOT=%~dp0"
set "PLUGIN_SDK_DIR=%ROOT%thirdparty\plugin-sdk"
set "PREMAKE=%PLUGIN_SDK_DIR%\tools\premake\premake5.exe"
set "PREMAKE_FILE=%PLUGIN_SDK_DIR%\tools\premake\premake5.lua"
set "SDK_ZIP=%TEMP%\forkhack-plugin-sdk.zip"

if not exist "%ROOT%thirdparty" mkdir "%ROOT%thirdparty"

if not exist "%PLUGIN_SDK_DIR%\shared\plugin.h" (
    rem Remove a directory left behind by an interrupted download/clone.
    if exist "%PLUGIN_SDK_DIR%" rmdir /s /q "%PLUGIN_SDK_DIR%"

    where git >nul 2>nul
    if not errorlevel 1 (
        echo [1/3] Downloading Plugin-SDK with Git...
        git clone --depth 1 https://github.com/DK22Pac/plugin-sdk.git "%PLUGIN_SDK_DIR%"
        if errorlevel 1 (
            echo [WARNING] Git download failed. Trying the ZIP fallback...
            if exist "%PLUGIN_SDK_DIR%" rmdir /s /q "%PLUGIN_SDK_DIR%"
            call :DownloadZip
            if errorlevel 1 exit /b 1
        )
    ) else (
        echo [1/3] Git is not available. Downloading Plugin-SDK as a ZIP...
        call :DownloadZip
        if errorlevel 1 exit /b 1
    )
) else (
    echo [1/3] Plugin-SDK is already downloaded.
)

if not exist "%PREMAKE%" (
    echo [ERROR] Plugin-SDK download is incomplete: "%PREMAKE%" was not found.
    exit /b 1
)

if not exist "%PLUGIN_SDK_DIR%\output\lib\plugin.lib" (
    rem ForkHack uses the v145 toolset, so generate a matching VS 2026 project.
    rem Always regenerate while plugin.lib is absent: a previous attempt may have
    rem left behind a VS 2022/v143 project that cannot be built by VS 2026.
    echo [2/3] Generating Plugin-SDK projects for Visual Studio 2026 ^(v145^)...
    "%PREMAKE%" vs2026 --file="%PREMAKE_FILE%"
    if errorlevel 1 (
        echo [ERROR] Premake could not generate the Plugin-SDK project.
        exit /b 1
    )
) else (
    echo [2/3] Plugin-SDK project and library are already available.
)

set "MSBUILD="
where msbuild >nul 2>nul
if not errorlevel 1 set "MSBUILD=msbuild"

if not defined MSBUILD (
    set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
    if exist "!VSWHERE!" (
        for /f "usebackq tokens=*" %%i in (`"!VSWHERE!" -latest -products * -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe`) do if not defined MSBUILD set "MSBUILD=%%i"
    )
)

if not defined MSBUILD (
    echo [ERROR] MSBuild was not found.
    echo Install the "Desktop development with C++" workload in Visual Studio Installer.
    exit /b 1
)

if not exist "%PLUGIN_SDK_DIR%\output\lib\plugin.lib" (
    echo [3/3] Building Plugin-SDK for GTA San Andreas ^(Release Win32^)...
    "!MSBUILD!" "%PLUGIN_SDK_DIR%\plugin_sa\Plugin_SA.vcxproj" /m /nologo /verbosity:minimal /p:Configuration=Release /p:Platform=Win32
    if errorlevel 1 (
        echo [ERROR] Plugin-SDK compilation failed. Check the errors printed above.
        exit /b 1
    )
) else (
    echo [3/3] plugin.lib is already built.
)

if not exist "%PLUGIN_SDK_DIR%\output\lib\plugin.lib" (
    echo [ERROR] Build completed without creating output\lib\plugin.lib.
    exit /b 1
)

echo.
echo Dependencies are ready. Build ForkHack using Release ^| Win32.
exit /b 0

:DownloadZip
where powershell.exe >nul 2>nul
if errorlevel 1 (
    echo [ERROR] Neither Git nor Windows PowerShell is available.
    exit /b 1
)
if exist "%SDK_ZIP%" del /q "%SDK_ZIP%"
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -Command ^
  "$ErrorActionPreference='Stop';" ^
  "[Net.ServicePointManager]::SecurityProtocol=[Net.SecurityProtocolType]::Tls12;" ^
  "Invoke-WebRequest -UseBasicParsing 'https://github.com/DK22Pac/plugin-sdk/archive/refs/heads/master.zip' -OutFile $env:SDK_ZIP;" ^
  "Expand-Archive -LiteralPath $env:SDK_ZIP -DestinationPath ('%ROOT%thirdparty') -Force;" ^
  "Move-Item -LiteralPath ('%ROOT%thirdparty\plugin-sdk-master') -Destination ('%PLUGIN_SDK_DIR%') -Force"
if errorlevel 1 (
    echo [ERROR] ZIP download failed. Check the internet connection and antivirus settings.
    exit /b 1
)
if exist "%SDK_ZIP%" del /q "%SDK_ZIP%"
exit /b 0
