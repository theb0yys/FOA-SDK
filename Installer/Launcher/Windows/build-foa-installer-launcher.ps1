# SPDX-License-Identifier: Apache-2.0 OR MIT
[CmdletBinding()]
param(
    [string]$Configuration = "Release",
    [string]$RuntimeIdentifier = "win-x64",
    [string]$OutputDirectory = "",
    [string]$InstallerMsi = "",
    [string]$InstallerMsiChecksum = "",
    [switch]$FrameworkDependent
)

$ErrorActionPreference = "Stop"
$Project = Join-Path $PSScriptRoot "FOAInstallerLauncher.csproj"
if (-not (Test-Path -LiteralPath $Project -PathType Leaf)) {
    throw "Installer wizard project not found: $Project"
}

if ([string]::IsNullOrWhiteSpace($OutputDirectory)) {
    $ProductRoot = Resolve-Path (Join-Path $PSScriptRoot "..\..\..")
    $OutputDirectory = Join-Path (Split-Path $ProductRoot -Parent) "foa-build\installer-wizard"
}
New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
$BuildRoot = Join-Path (Split-Path $OutputDirectory -Parent) "installer-wizard-build"
$IntermediatePath = (Join-Path $BuildRoot "obj") + [IO.Path]::DirectorySeparatorChar
$BinaryPath = (Join-Path $BuildRoot "bin") + [IO.Path]::DirectorySeparatorChar

$PublishArgs = @(
    "publish",
    $Project,
    "-c", $Configuration,
    "-r", $RuntimeIdentifier,
    "-o", $OutputDirectory,
    "/p:BaseIntermediateOutputPath=$IntermediatePath",
    "/p:BaseOutputPath=$BinaryPath",
    "/p:PublishSingleFile=true",
    "/p:IncludeNativeLibrariesForSelfExtract=true",
    "/p:DebugType=embedded",
    "--self-contained=$(-not $FrameworkDependent)"
)

if (-not [string]::IsNullOrWhiteSpace($InstallerMsi)) {
    $InstallerMsi = (Resolve-Path -LiteralPath $InstallerMsi).Path
    if ([string]::IsNullOrWhiteSpace($InstallerMsiChecksum)) {
        $InstallerMsiChecksum = "$InstallerMsi.sha256"
    }
    $InstallerMsiChecksum = (Resolve-Path -LiteralPath $InstallerMsiChecksum).Path
    $PublishArgs += "/p:InstallerMsiPath=$InstallerMsi"
    $PublishArgs += "/p:InstallerMsiChecksumPath=$InstallerMsiChecksum"
}

& dotnet @PublishArgs
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

$Exe = Join-Path $OutputDirectory "FOA-SDK-Installer.exe"
if (-not (Test-Path -LiteralPath $Exe -PathType Leaf)) {
    throw "Expected installer wizard executable was not produced: $Exe"
}

Write-Host "Built $Exe"
if ([string]::IsNullOrWhiteSpace($InstallerMsi)) {
    Write-Warning "No MSI was embedded. Place exactly one reviewed MSI and its .sha256 sidecar beside the EXE before running it."
}
