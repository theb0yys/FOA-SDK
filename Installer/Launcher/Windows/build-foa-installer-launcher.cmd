@echo off
setlocal EnableExtensions

rem SPDX-License-Identifier: Apache-2.0 OR MIT
rem Build FOA-SDK-Installer.exe without relying on PowerShell script execution policy.

set "SCRIPT_DIR=%~dp0"
set "PROJECT=%SCRIPT_DIR%FOAInstallerLauncher.csproj"
set "OUTPUT=%SCRIPT_DIR%artifacts"
set "CONFIGURATION=Release"
set "RUNTIME=win-x64"
set "SELF_CONTAINED=false"

:parse
if "%~1"=="" goto build
if /I "%~1"=="-Configuration" (
  set "CONFIGURATION=%~2"
  shift
  shift
  goto parse
)
if /I "%~1"=="--configuration" (
  set "CONFIGURATION=%~2"
  shift
  shift
  goto parse
)
if /I "%~1"=="-RuntimeIdentifier" (
  set "RUNTIME=%~2"
  shift
  shift
  goto parse
)
if /I "%~1"=="--runtime" (
  set "RUNTIME=%~2"
  shift
  shift
  goto parse
)
if /I "%~1"=="-SelfContained" (
  set "SELF_CONTAINED=true"
  shift
  goto parse
)
if /I "%~1"=="--self-contained" (
  set "SELF_CONTAINED=true"
  shift
  goto parse
)
if /I "%~1"=="-h" goto help
if /I "%~1"=="--help" goto help

echo Unknown option: %~1 1>&2
goto help_error

:build
where dotnet >nul 2>nul
if errorlevel 1 (
  echo dotnet was not found on PATH. Install the .NET SDK, then run this command again. 1>&2
  exit /b 1
)

if not exist "%PROJECT%" (
  echo Launcher project was not found: %PROJECT% 1>&2
  exit /b 1
)

if not exist "%OUTPUT%" mkdir "%OUTPUT%"

echo Building FOA-SDK-Installer.exe...
echo Project: %PROJECT%
echo Configuration: %CONFIGURATION%
echo Runtime: %RUNTIME%
echo SelfContained: %SELF_CONTAINED%

dotnet publish "%PROJECT%" ^
  -c "%CONFIGURATION%" ^
  -r "%RUNTIME%" ^
  --self-contained "%SELF_CONTAINED%" ^
  /p:PublishSingleFile=true ^
  /p:AssemblyName=FOA-SDK-Installer ^
  /p:SelfContained=%SELF_CONTAINED% ^
  -o "%OUTPUT%"

if errorlevel 1 exit /b %errorlevel%

if not exist "%OUTPUT%\FOA-SDK-Installer.exe" (
  echo Build completed but FOA-SDK-Installer.exe was not found under %OUTPUT%. 1>&2
  exit /b 1
)

echo.
echo Built: %OUTPUT%\FOA-SDK-Installer.exe
exit /b 0

:help
echo Usage: build-foa-installer-launcher.cmd [-Configuration Release] [-RuntimeIdentifier win-x64] [-SelfContained]
exit /b 0

:help_error
echo Usage: build-foa-installer-launcher.cmd [-Configuration Release] [-RuntimeIdentifier win-x64] [-SelfContained] 1>&2
exit /b 2
