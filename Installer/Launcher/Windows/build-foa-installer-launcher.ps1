# SPDX-License-Identifier: Apache-2.0 OR MIT
[CmdletBinding()]
param(
    [string]$Configuration = "Release",
    [string]$RuntimeIdentifier = "win-x64",
    [string]$OutputDirectory = "",
    [switch]$SelfContained
)

$ErrorActionPreference = "Stop"
$Project = Join-Path $PSScriptRoot "FOAInstallerLauncher.csproj"
if (-not (Test-Path -LiteralPath $Project -PathType Leaf)) {
    throw "Launcher project not found: $Project"
}

if ([string]::IsNullOrWhiteSpace($OutputDirectory)) {
    $OutputDirectory = Join-Path $PSScriptRoot "artifacts"
}
New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null

$PublishArgs = @(
    "publish",
    $Project,
    "-c", $Configuration,
    "-r", $RuntimeIdentifier,
    "-o", $OutputDirectory,
    "/p:PublishSingleFile=true",
    "/p:IncludeNativeLibrariesForSelfExtract=true",
    "/p:DebugType=embedded"
)
if ($SelfContained) {
    $PublishArgs += "--self-contained=true"
} else {
    $PublishArgs += "--self-contained=false"
}

& dotnet @PublishArgs
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

$Exe = Join-Path $OutputDirectory "FOA-SDK-Installer.exe"
if (-not (Test-Path -LiteralPath $Exe -PathType Leaf)) {
    throw "Expected launcher executable was not produced: $Exe"
}

Write-Host "Built $Exe"
