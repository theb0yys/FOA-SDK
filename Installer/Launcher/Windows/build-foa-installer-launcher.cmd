@echo off
setlocal EnableExtensions

rem SPDX-License-Identifier: Apache-2.0 OR MIT
rem Build the self-contained FOA-SDK installer wizard without relying on PowerShell policy.

set "SCRIPT_DIR=%~dp0"
set "PROJECT=%SCRIPT_DIR%FOAInstallerLauncher.csproj"
set "PRODUCT_ROOT=%SCRIPT_DIR%..\..\.."
set "OUTPUT=%PRODUCT_ROOT%\..\foa-build\installer-wizard"
set "CONFIGURATION=Release"
set "RUNTIME=win-x64"
set "SELF_CONTAINED=true"
set "INSTALLER_MSI="

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
if /I "%~1"=="-OutputDirectory" (
  set "OUTPUT=%~2"
  shift
  shift
  goto parse
)
if /I "%~1"=="--output" (
  set "OUTPUT=%~2"
  shift
  shift
  goto parse
)
if /I "%~1"=="-InstallerMsi" (
  set "INSTALLER_MSI=%~2"
  shift
  shift
  goto parse
)
if /I "%~1"=="--msi" (
  set "INSTALLER_MSI=%~2"
  shift
  shift
  goto parse
)
if /I "%~1"=="--framework-dependent" (
  set "SELF_CONTAINED=false"
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
  echo Installer wizard project was not found: %PROJECT% 1>&2
  exit /b 1
)

if not exist "%OUTPUT%" mkdir "%OUTPUT%"
for %%I in ("%OUTPUT%\..") do set "OUTPUT_PARENT=%%~fI"
set "BUILD_ROOT=%OUTPUT_PARENT%\installer-wizard-build"
set "BUILD_ROOT_MSBUILD=%BUILD_ROOT:\=/%"

echo Building FOA-SDK-Installer.exe...
echo Project: %PROJECT%
echo Configuration: %CONFIGURATION%
echo Runtime: %RUNTIME%
echo SelfContained: %SELF_CONTAINED%

set "MSI_ARGS="
if not "%INSTALLER_MSI%"=="" (
  if not exist "%INSTALLER_MSI%" (
    echo Reviewed MSI was not found: %INSTALLER_MSI% 1>&2
    exit /b 1
  )
  if not exist "%INSTALLER_MSI%.sha256" (
    echo MSI checksum was not found: %INSTALLER_MSI%.sha256 1>&2
    exit /b 1
  )
  set "MSI_ARGS=/p:InstallerMsiPath=%INSTALLER_MSI% /p:InstallerMsiChecksumPath=%INSTALLER_MSI%.sha256"
)

dotnet publish "%PROJECT%" ^
  -c "%CONFIGURATION%" ^
  -r "%RUNTIME%" ^
  --self-contained "%SELF_CONTAINED%" ^
  /p:PublishSingleFile=true ^
  /p:BaseIntermediateOutputPath="%BUILD_ROOT_MSBUILD%/obj/" ^
  /p:BaseOutputPath="%BUILD_ROOT_MSBUILD%/bin/" ^
  /p:IncludeNativeLibrariesForSelfExtract=true ^
  /p:DebugType=embedded ^
  /p:AssemblyName=FOA-SDK-Installer ^
  /p:SelfContained=%SELF_CONTAINED% ^
  %MSI_ARGS% ^
  -o "%OUTPUT%"

if errorlevel 1 exit /b %errorlevel%

if not exist "%OUTPUT%\FOA-SDK-Installer.exe" (
  echo Build completed but FOA-SDK-Installer.exe was not found under %OUTPUT%. 1>&2
  exit /b 1
)

echo.
echo Built: %OUTPUT%\FOA-SDK-Installer.exe
if "%INSTALLER_MSI%"=="" echo WARNING: no MSI was embedded; place one reviewed MSI and sidecar beside the EXE.
exit /b 0

:help
echo Usage: build-foa-installer-launcher.cmd [-Configuration Release] [-RuntimeIdentifier win-x64] [-OutputDirectory path] [-InstallerMsi reviewed.msi] [--framework-dependent]
exit /b 0

:help_error
echo Usage: build-foa-installer-launcher.cmd [-Configuration Release] [-RuntimeIdentifier win-x64] [-OutputDirectory path] [-InstallerMsi reviewed.msi] [--framework-dependent] 1>&2
exit /b 2
