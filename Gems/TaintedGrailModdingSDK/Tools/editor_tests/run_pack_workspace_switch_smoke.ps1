# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
# SPDX-License-Identifier: Apache-2.0 OR MIT
#Requires -Version 7.0
<#
.SYNOPSIS
Runs Pack Manager workspace-switch acceptance in separate compiled Windows Editor processes.
.DESCRIPTION
Requires the pinned engine, a built SDK Editor and an already prepared engine-asset
cache. Creates only synthetic authoring data under a fresh external OutputRoot.
Qt uses the normal desktop layout preferences; project user/log data is isolated.
Exercises both workspace opening panes and requires a clean exit after assertions.
#>
param(
    [Parameter(Mandatory)][string]$EditorExecutable,
    [Parameter(Mandatory)][string]$EngineRoot,
    [Parameter(Mandatory)][string]$CacheRoot,
    [Parameter(Mandatory)][string]$OutputRoot,
    [ValidateRange(30, 300)][int]$TimeoutSeconds = 120
)
$ErrorActionPreference = 'Stop'
if (-not $IsWindows) { throw 'The file-lock and Editor acceptance cases require Windows.' }
$productRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../../..'))
$EditorExecutable = (Resolve-Path -LiteralPath $EditorExecutable).Path
$EngineRoot = (Resolve-Path -LiteralPath $EngineRoot).Path
$CacheRoot = (Resolve-Path -LiteralPath $CacheRoot).Path
$OutputRoot = [IO.Path]::GetFullPath($OutputRoot)
function Test-Inside([string]$Path, [string]$Root) {
    return $Path.Equals($Root, [StringComparison]::OrdinalIgnoreCase) -or
        $Path.StartsWith($Root.TrimEnd('\', '/') + [IO.Path]::DirectorySeparatorChar,
                         [StringComparison]::OrdinalIgnoreCase)
}
foreach ($generatedPath in @($OutputRoot, $CacheRoot)) {
    if ((Test-Inside $generatedPath $productRoot) -or (Test-Inside $generatedPath $EngineRoot)) {
        throw 'Generated test output and cache must stay outside product and engine source.'
    }
}
if (Test-Path -LiteralPath $OutputRoot) { throw 'OutputRoot must be a fresh directory.' }
$lock = Get-Content (Join-Path $productRoot 'o3de.lock.json') -Raw | ConvertFrom-Json
$engineCommit = (git -C $EngineRoot rev-parse HEAD).Trim()
if ($LASTEXITCODE -ne 0 -or $engineCommit -ne $lock.commit) { throw 'Engine does not match o3de.lock.json.' }
$testScript = Join-Path $PSScriptRoot 'pack_workspace_switch_live_smoke.py'
New-Item -ItemType Directory -Path $OutputRoot | Out-Null
New-Item -ItemType Directory -Path (Join-Path $OutputRoot 'tmp') | Out-Null
$environmentNames = @('LOCALAPPDATA', 'TEMP', 'TMP', 'QT_QPA_PLATFORM', 'FOA_SDK_PACK_RESULT',
    'FOA_SDK_PACK_WORKSPACE', 'FOA_SDK_PACK_WORKSPACE_ROUTE')
$savedEnvironment = @{}
foreach ($name in $environmentNames) { $savedEnvironment[$name] = [Environment]::GetEnvironmentVariable($name, 'Process') }

function Invoke-WorkspaceCase([string]$Route) {
    $caseRoot = Join-Path $OutputRoot $Route
    New-Item -ItemType Directory -Path $caseRoot | Out-Null
    $workspaceRoot = (Join-Path $caseRoot 'local/FOA-SDK/Workspace').Replace('\', '/')
    foreach ($leaf in @('Output','Staging','Deployment','Diagnostics','Extracted','Game/Managed','Game/BepInEx/plugins')) {
        New-Item -ItemType Directory -Force -Path "$workspaceRoot/$leaf" | Out-Null
    }
    $fixture = [ordered]@{
        SchemaVersion=1; WorkspaceId='sdkqa.pack-workspace-switch'; DisplayName='Synthetic workspace-switch acceptance'
        RootPath=$workspaceRoot; OutputPath="$workspaceRoot/Output"; StagingPath="$workspaceRoot/Staging"
        DeploymentPath="$workspaceRoot/Deployment"; ActiveGameProfileId='sdkqa.synthetic'
        GameProfiles=@(@{
            ProfileId='sdkqa.synthetic'; DisplayName='Synthetic fixture, no runtime authority'
            GameVersion='1.0.0'; Branch='mono'; RuntimeTarget='Mono'
            UnityVersion='2022.3.22f1'; BepInExVersion='5.4.23.3'; DlcScopes=@('base-game')
            InstallPath="$workspaceRoot/Game"; ManagedAssembliesPath="$workspaceRoot/Game/Managed"
            PluginPath="$workspaceRoot/Game/BepInEx/plugins"; DiagnosticsPath="$workspaceRoot/Diagnostics"
            ExtractedDataPath="$workspaceRoot/Extracted"
        })
    }
    $fixture | ConvertTo-Json -Depth 6 | Set-Content "$workspaceRoot/foa-sdk.tgworkspace.json" -Encoding utf8
    foreach ($marker in @('Game/Managed/Assembly-CSharp.dll', 'Game/UnityPlayer.dll')) {
        Set-Content "$workspaceRoot/$marker" 'Synthetic authoring marker only; not executable content.'
    }
    foreach ($leaf in @('user', 'log')) { New-Item -ItemType Directory -Path "$caseRoot/$leaf" | Out-Null }
    $env:LOCALAPPDATA = (Get-Item -LiteralPath $workspaceRoot).Parent.Parent.FullName
    $env:TEMP = Join-Path $OutputRoot 'tmp'
    $env:TMP = $env:TEMP
    $env:FOA_SDK_PACK_RESULT = "$caseRoot/result.json"
    $env:FOA_SDK_PACK_WORKSPACE = "$workspaceRoot/foa-sdk.tgworkspace.json"
    $env:FOA_SDK_PACK_WORKSPACE_ROUTE = $Route
    Remove-Item Env:QT_QPA_PLATFORM -ErrorAction SilentlyContinue
    $arguments = @(
        '--project-path', (Join-Path $productRoot 'TaintedGrailModdingEditor'),
        '--engine-path', $EngineRoot, '--project-cache-path', $CacheRoot,
        '--project-user-path', "$caseRoot/user", '--project-log-path', "$caseRoot/log",
        '--NullRenderer', '--skipWelcomeScreenDialog',
        '--regset=/Amazon/AzCore/Bootstrap/wait_for_connect=0',
        '--regset=/Amazon/AzCore/Bootstrap/connect_ap_timeout=1',
        '--regset=/Amazon/AzCore/Bootstrap/launch_ap_timeout=1',
        '--runpython', $testScript
    )
    # Start-Process joins ArgumentList into one Windows command line; quote path arguments.
    $quoted = $arguments | ForEach-Object {
        if ($_ -match '"') { throw 'Unexpected quote in an Editor argument.' }
        '"' + $_ + '"'
    }
    $editor = Start-Process -FilePath $EditorExecutable -WorkingDirectory (Split-Path $EditorExecutable) -ArgumentList $quoted -WindowStyle Hidden -RedirectStandardOutput "$caseRoot/stdout.log" -RedirectStandardError "$caseRoot/stderr.log" -PassThru
    $clock = [Diagnostics.Stopwatch]::StartNew()
    $forcedStop = $false
    $loadedSdk = $null
    $expectedSdk = Join-Path (Split-Path $EditorExecutable) 'TaintedGrailModdingSDK.Editor.dll'
    [ordered]@{pid=$editor.Id;executable=$EditorExecutable;arguments=$arguments;route=$Route;workspace=$workspaceRoot} |
        ConvertTo-Json -Depth 5 | Set-Content "$caseRoot/launch.json"
    Write-Host "Testing $Route in owned Editor process $($editor.Id)"
    try {
        while (-not $editor.WaitForExit(1000)) {
            if (-not $loadedSdk) {
                $editor.Refresh()
                $module = $editor.Modules | Where-Object ModuleName -eq 'TaintedGrailModdingSDK.Editor.dll'
                if ($module) {
                    $loadedSdk = [ordered]@{
                        path=$module.FileName
                        sha256=(Get-FileHash -LiteralPath $module.FileName -Algorithm SHA256).Hash
                    }
                    if (-not $module.FileName.Equals($expectedSdk, [StringComparison]::OrdinalIgnoreCase)) {
                        throw 'Editor loaded an unexpected SDK module.'
                    }
                }
            }
            $interim = $null
            if (Test-Path -LiteralPath "$caseRoot/result.json") {
                try { $interim = Get-Content "$caseRoot/result.json" -Raw | ConvertFrom-Json } catch {}
            }
            if ($interim.status -eq 'FAILED' -or $clock.Elapsed.TotalSeconds -gt $TimeoutSeconds) {
                $forcedStop = $true
                break
            }
        }
    } finally {
        if (-not $editor.HasExited) {
            if (-not $editor.Path.Equals($EditorExecutable, [StringComparison]::OrdinalIgnoreCase)) {
                throw 'Refusing to stop an unexpected Editor process.'
            }
            Stop-Process -Id $editor.Id
            $forcedStop = $true
        }
        $editor.WaitForExit()
    }
    $test = $null
    if (Test-Path -LiteralPath "$caseRoot/result.json") { $test = Get-Content "$caseRoot/result.json" -Raw | ConvertFrom-Json }
    $passed = -not $forcedStop -and $editor.ExitCode -eq 0 -and $test.status -eq 'PASSED' -and $test.about_to_quit -and $loadedSdk
    $row = [ordered]@{
        status=$(if ($passed) {'PASSED'} else {'FAILED'}); route=$Route; pid=$editor.Id
        exit_code=$editor.ExitCode; forced_stop=$forcedStop; about_to_quit=$test.about_to_quit; sdk_module=$loadedSdk
        elapsed_seconds=[Math]::Round($clock.Elapsed.TotalSeconds, 3)
        workspace=$workspaceRoot; checks=$test.checks; result="$caseRoot/result.json"
    }
    $row | ConvertTo-Json -Depth 6 | Set-Content "$caseRoot/process-result.json"
    if (-not $passed) { throw "Workspace route $Route failed; see $caseRoot." }
    return $row
}
$rows = @()
$status = 'FAILED'
try {
    foreach ($route in @('status', 'catalog')) {
        $rows += Invoke-WorkspaceCase -Route $route
    }
    if ($rows.Count -ne 2) { throw 'Expected two complete Editor route runs.' }
    $status = 'PASSED'
} finally {
    foreach ($name in $environmentNames) {
        [Environment]::SetEnvironmentVariable($name, $savedEnvironment[$name], 'Process')
    }
    [ordered]@{status=$status;engine_commit=$engineCommit;cases=$rows} |
        ConvertTo-Json -Depth 8 | Set-Content (Join-Path $OutputRoot 'suite-result.json')
}
Write-Host "PASSED: both workspace routes preserve drafts and saved data, with clean Editor exits."
