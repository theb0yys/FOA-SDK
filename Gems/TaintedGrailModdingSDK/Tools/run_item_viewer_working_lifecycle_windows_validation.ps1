[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string] $EngineRoot,

    [string] $BuildRoot = (Join-Path $env:LOCALAPPDATA "FOA-SDK\Build\ItemViewerLifecycle"),

    [string] $ThirdPartyRoot = "",

    [ValidateSet("debug", "profile", "release")]
    [string] $Configuration = "profile",

    [ValidateRange(1, 32)]
    [int] $Parallel = 2,

    [ValidateRange(30, 1800)]
    [int] $EditorSmokeTimeoutSeconds = 300
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$isWindowsVariable = Get-Variable -Name IsWindows -ErrorAction SilentlyContinue
$isWindowsHost = if ($isWindowsVariable) { [bool] $isWindowsVariable.Value } else { $env:OS -eq "Windows_NT" }
if (-not $isWindowsHost) {
    throw "The working item-viewer lifecycle must be validated on Windows."
}

$repoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..\..\..")).Path
$validator = Join-Path $PSScriptRoot "validate_item_viewer_working_lifecycle.py"
$alphaRunner = Join-Path $PSScriptRoot "run_alpha_item_viewer_windows_validation.ps1"

python $validator
if ($LASTEXITCODE -ne 0) {
    throw "Item-viewer lifecycle static validation failed."
}

$arguments = @{
    EngineRoot = $EngineRoot
    BuildRoot = $BuildRoot
    Configuration = $Configuration
    Parallel = $Parallel
    EditorSmokeTimeoutSeconds = $EditorSmokeTimeoutSeconds
}
if (-not [string]::IsNullOrWhiteSpace($ThirdPartyRoot)) {
    $arguments.ThirdPartyRoot = $ThirdPartyRoot
}

& $alphaRunner @arguments
if ($LASTEXITCODE -ne 0) {
    throw "Item-viewer lifecycle Windows validation failed."
}

Write-Host "Working item-viewer lifecycle validation passed."
Write-Host "Repository: $repoRoot"
