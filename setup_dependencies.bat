@echo off
setlocal EnableExtensions EnableDelayedExpansion
chcp 65001 >nul
set "VSLANG=1033"

set "ROOT=%~dp0"
set "PLUGIN_SDK_DIR=%ROOT%thirdparty\plugin-sdk"
set "LUA_DIR=%ROOT%thirdparty\lua"
set "SOL2_DIR=%ROOT%thirdparty\sol2"
set "PREMAKE=%PLUGIN_SDK_DIR%\tools\premake\premake5.exe"
set "PREMAKE_FILE=%PLUGIN_SDK_DIR%\tools\premake\premake5.lua"
set "SDK_ZIP=%TEMP%\mirick-plugin-sdk.zip"
set "LUA_ZIP=%TEMP%\mirick-lua.zip"
set "SOL2_ZIP=%TEMP%\mirick-sol2.zip"

if not exist "%ROOT%thirdparty" mkdir "%ROOT%thirdparty"

if not exist "%LUA_DIR%\lua.h" (
    if exist "%LUA_DIR%" rmdir /s /q "%LUA_DIR%"
    where git >nul 2>nul
    if not errorlevel 1 (
        echo [1/5] Downloading Lua 5.4.9...
        git clone --depth 1 --branch v5.4.9 https://github.com/lua/lua.git "%LUA_DIR%"
    )
    if not exist "%LUA_DIR%\lua.h" (
        if exist "%LUA_DIR%" rmdir /s /q "%LUA_DIR%"
        call :DownloadLuaZip
        if errorlevel 1 exit /b 1
    )
) else (
    echo [1/5] Lua is already downloaded.
)

if not exist "%SOL2_DIR%\include\sol\sol.hpp" (
    if exist "%SOL2_DIR%" rmdir /s /q "%SOL2_DIR%"
    where git >nul 2>nul
    if not errorlevel 1 (
        echo [2/5] Downloading sol2 3.5.0...
        git clone --depth 1 --branch v3.5.0 https://github.com/ThePhD/sol2.git "%SOL2_DIR%"
    )
    if not exist "%SOL2_DIR%\include\sol\sol.hpp" (
        if exist "%SOL2_DIR%" rmdir /s /q "%SOL2_DIR%"
        call :DownloadSol2Zip
        if errorlevel 1 exit /b 1
    )
) else (
    echo [2/5] sol2 is already downloaded.
)

if not exist "%PLUGIN_SDK_DIR%\shared\plugin.h" (
    rem Remove a directory left behind by an interrupted download/clone.
    if exist "%PLUGIN_SDK_DIR%" rmdir /s /q "%PLUGIN_SDK_DIR%"

    where git >nul 2>nul
    if not errorlevel 1 (
        echo [3/5] Downloading Plugin-SDK with Git...
        git clone --depth 1 https://github.com/DK22Pac/plugin-sdk.git "%PLUGIN_SDK_DIR%"
        if errorlevel 1 (
            echo [WARNING] Git download failed. Trying the ZIP fallback...
            if exist "%PLUGIN_SDK_DIR%" rmdir /s /q "%PLUGIN_SDK_DIR%"
            call :DownloadZip
            if errorlevel 1 exit /b 1
        )
    ) else (
        echo [3/5] Git is not available. Downloading Plugin-SDK as a ZIP...
        call :DownloadZip
        if errorlevel 1 exit /b 1
    )
) else (
    echo [3/5] Plugin-SDK is already downloaded.
)

if not exist "%PREMAKE%" (
    echo [ERROR] Plugin-SDK download is incomplete: "%PREMAKE%" was not found.
    exit /b 1
)

if not exist "%PLUGIN_SDK_DIR%\output\lib\plugin.lib" (
    rem Generate a Visual Studio 2022 project that uses the widely available v143 toolset.
    rem Always regenerate while plugin.lib is absent: a previous attempt may have
    rem left behind a project for a toolset that is not installed.
    echo [4/5] Generating Plugin-SDK projects for Visual Studio 2022 ^(v143^)...
    "%PREMAKE%" vs2022 --file="%PREMAKE_FILE%"
    if errorlevel 1 (
        echo [ERROR] Premake could not generate the Plugin-SDK project.
        exit /b 1
    )
) else (
    echo [4/5] Plugin-SDK project and library are already available.
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
    echo [5/5] Building Plugin-SDK for GTA San Andreas ^(Release Win32^)...
    "!MSBUILD!" "%PLUGIN_SDK_DIR%\plugin_sa\Plugin_SA.vcxproj" /m /nologo /verbosity:minimal /p:Configuration=Release /p:Platform=Win32
    if errorlevel 1 (
        echo [ERROR] Plugin-SDK compilation failed. Check the errors printed above.
        exit /b 1
    )
) else (
    echo [5/5] plugin.lib is already built.
)

if not exist "%PLUGIN_SDK_DIR%\output\lib\plugin.lib" (
    echo [ERROR] Build completed without creating output\lib\plugin.lib.
    exit /b 1
)

echo.
echo Dependencies are ready. Build mirick using Release ^| Win32.
exit /b 0

:DownloadLuaZip
where powershell.exe >nul 2>nul
if errorlevel 1 (
    echo [ERROR] Lua download requires Git or Windows PowerShell.
    exit /b 1
)
echo [1/5] Downloading Lua 5.4.9 ZIP...
if exist "%LUA_ZIP%" del /q "%LUA_ZIP%"
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -Command ^
  "$ErrorActionPreference='Stop'; [Net.ServicePointManager]::SecurityProtocol=[Net.SecurityProtocolType]::Tls12;" ^
  "Invoke-WebRequest -UseBasicParsing 'https://github.com/lua/lua/archive/refs/tags/v5.4.9.zip' -OutFile '%LUA_ZIP%';" ^
  "Expand-Archive -LiteralPath '%LUA_ZIP%' -DestinationPath ('%ROOT%thirdparty') -Force;" ^
  "Move-Item -LiteralPath ('%ROOT%thirdparty\lua-5.4.9') -Destination ('%LUA_DIR%') -Force"
if errorlevel 1 exit /b 1
if exist "%LUA_ZIP%" del /q "%LUA_ZIP%"
exit /b 0

:DownloadSol2Zip
where powershell.exe >nul 2>nul
if errorlevel 1 (
    echo [ERROR] sol2 download requires Git or Windows PowerShell.
    exit /b 1
)
echo [2/5] Downloading sol2 3.5.0 ZIP...
if exist "%SOL2_ZIP%" del /q "%SOL2_ZIP%"
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -Command ^
  "$ErrorActionPreference='Stop'; [Net.ServicePointManager]::SecurityProtocol=[Net.SecurityProtocolType]::Tls12;" ^
  "Invoke-WebRequest -UseBasicParsing 'https://github.com/ThePhD/sol2/archive/refs/tags/v3.5.0.zip' -OutFile '%SOL2_ZIP%';" ^
  "Expand-Archive -LiteralPath '%SOL2_ZIP%' -DestinationPath ('%ROOT%thirdparty') -Force;" ^
  "Move-Item -LiteralPath ('%ROOT%thirdparty\sol2-3.5.0') -Destination ('%SOL2_DIR%') -Force"
if errorlevel 1 exit /b 1
if exist "%SOL2_ZIP%" del /q "%SOL2_ZIP%"
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
