# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
# SPDX-License-Identifier: Apache-2.0 OR MIT
#Requires -Version 7.0
<#
.SYNOPSIS
Runs Pack Manager recovery acceptance on a private Windows desktop.
.DESCRIPTION
Requires the pinned engine, a built SDK Editor and an already prepared engine-asset
cache. Creates only synthetic authoring data under a fresh external OutputRoot.
Qt uses the normal desktop layout preferences; project user/log data is isolated.
Only checkpoint-confirmed seed phases may be forcibly terminated. Other phases require clean exit.
The private desktop is never activated, so test dialogs do not interrupt the user.
#>
param(
    [Parameter(Mandatory)][string]$EditorExecutable,
    [Parameter(Mandatory)][string]$EngineRoot,
    [Parameter(Mandatory)][string]$CacheRoot,
    [Parameter(Mandatory)][string]$OutputRoot,
    [ValidateRange(30, 300)][int]$TimeoutSeconds = 120,
    [ValidateSet('all', 'new', 'saved', 'failures', 'docked')][string]$Suite = 'all'
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
$testScript = Join-Path $PSScriptRoot 'pack_draft_recovery_live_smoke.py'
New-Item -ItemType Directory -Path $OutputRoot | Out-Null
New-Item -ItemType Directory -Path (Join-Path $OutputRoot 'tmp') | Out-Null
$environmentNames = @('LOCALAPPDATA', 'TEMP', 'TMP', 'QT_QPA_PLATFORM', 'FOA_SDK_PACK_RESULT',
    'FOA_SDK_PACK_WORKSPACE', 'FOA_SDK_PACK_DOCKED', 'FOA_SDK_RECOVERY_CASE', 'FOA_SDK_RECOVERY_RESULT', 'FOA_SDK_RECOVERY_EXPECTED')
$savedEnvironment = @{}
foreach ($name in $environmentNames) { $savedEnvironment[$name] = [Environment]::GetEnvironmentVariable($name, 'Process') }

function Invoke-RecoveryCase([string]$Name, [string]$Case, [string]$Reuse = '',
                             [string]$ExpectedResult = '', [bool]$ForceExit = $false, [bool]$Corrupt = $false) {
    $caseRoot = Join-Path $OutputRoot $Name
    New-Item -ItemType Directory -Path $caseRoot | Out-Null
    $workspaceRoot = (Join-Path $caseRoot 'local/FOA-SDK/Workspace').Replace('\', '/')
    if ($Reuse) {
        $workspaceRoot = $Reuse
        $fixture = Get-Content "$workspaceRoot/foa-sdk.tgworkspace.json" -Raw | ConvertFrom-Json
        if ($fixture.WorkspaceId -ne 'sdkqa.pack-editor-exit' -or $fixture.RootPath -ne $workspaceRoot) {
            throw 'Invalid reused synthetic workspace.'
        }
    } else {
        foreach ($leaf in @('Output','Staging','Deployment','Diagnostics','Extracted','Game/Managed','Game/BepInEx/plugins')) {
            New-Item -ItemType Directory -Force -Path "$workspaceRoot/$leaf" | Out-Null
        }
        $fixture = [ordered]@{
            SchemaVersion=1; WorkspaceId=$(if ($Case -eq 'docked-close') {'sdkqa.pack-unsaved'} else {'sdkqa.pack-editor-exit'}); DisplayName='Synthetic Editor exit acceptance'
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
    }
    foreach ($leaf in @('user', 'log')) { New-Item -ItemType Directory -Path "$caseRoot/$leaf" | Out-Null }
    $env:LOCALAPPDATA = (Get-Item -LiteralPath $workspaceRoot).Parent.Parent.FullName
    $env:TEMP = Join-Path $OutputRoot 'tmp'
    $env:TMP = $env:TEMP
    $env:FOA_SDK_RECOVERY_RESULT = "$caseRoot/result.json"
    $env:FOA_SDK_PACK_RESULT = "$caseRoot/result.json"
    $env:FOA_SDK_PACK_DOCKED = $(if ($Suite -eq 'docked') {'1'} else {'0'})
    $caseScript = $(if ($Case -eq 'docked-close') {Join-Path $PSScriptRoot 'pack_unsaved_live_smoke.py'} else {$testScript})
    $env:FOA_SDK_PACK_WORKSPACE = "$workspaceRoot/foa-sdk.tgworkspace.json"
    $env:FOA_SDK_RECOVERY_CASE = $Case
    $env:FOA_SDK_RECOVERY_EXPECTED = $ExpectedResult
    if ($Corrupt) {
        $seed = Get-Content -LiteralPath $ExpectedResult -Raw | ConvertFrom-Json
        $copy = (Resolve-Path -LiteralPath $seed.recovery_path).Path
        if (-not (Test-Inside $copy $OutputRoot) -or -not $copy.EndsWith('.packdraft.json')) {
            throw 'Refusing to corrupt anything outside the owned recovery fixture.'
        }
        [IO.File]::WriteAllText($copy, '{broken', [Text.UTF8Encoding]::new($false))
    }
    Remove-Item Env:QT_QPA_PLATFORM -ErrorAction SilentlyContinue
    $arguments = @(
        '--project-path', (Join-Path $productRoot 'TaintedGrailModdingEditor'),
        '--engine-path', $EngineRoot, '--project-cache-path', $CacheRoot,
        '--project-user-path', "$caseRoot/user", '--project-log-path', "$caseRoot/log",
        '--NullRenderer', '--skipWelcomeScreenDialog',
        '--regset=/Amazon/AzCore/Bootstrap/wait_for_connect=0',
        '--regset=/Amazon/AzCore/Bootstrap/connect_ap_timeout=1',
        '--regset=/Amazon/AzCore/Bootstrap/launch_ap_timeout=1',
        '--runpython', $caseScript
    )
    # Start-Process joins ArgumentList into one Windows command line; quote path arguments.
    $quoted = $arguments | ForEach-Object {
        if ($_ -match '"') { throw 'Unexpected quote in an Editor argument.' }
        '"' + $_ + '"'
    }
    $editor = [RecoveryTestDesktop]::Start($EditorExecutable, ($quoted -join " "), (Split-Path $EditorExecutable))
    $clock = [Diagnostics.Stopwatch]::StartNew()
    $forcedStop = $false
    $intentionalStop = $false
    [ordered]@{pid=$editor.Id;executable=$EditorExecutable;arguments=$arguments;case=$Case;workspace=$workspaceRoot} |
        ConvertTo-Json -Depth 5 | Set-Content "$caseRoot/launch.json"
    Write-Host "Testing $Name in owned Editor process $($editor.Id)"
    try {
        while (-not $editor.WaitForExit(1000)) {
            $interim = $null
            if (Test-Path -LiteralPath "$caseRoot/result.json") {
                try { $interim = Get-Content "$caseRoot/result.json" -Raw | ConvertFrom-Json } catch {}
            }
            if ($ForceExit -and $interim.status -eq 'READY_TO_TERMINATE') {
                $copy = (Resolve-Path -LiteralPath $interim.recovery_path).Path
                if (-not (Test-Inside $copy $OutputRoot) -or
                    (Get-FileHash -LiteralPath $copy -Algorithm SHA256).Hash -ne $interim.recovery_sha256 -or
                    -not $interim.editor_initialized) {
                    throw 'Forced termination requires a verified durable checkpoint.'
                }
                if (-not $editor.Path.Equals($EditorExecutable, [StringComparison]::OrdinalIgnoreCase)) {
                    throw 'Refusing to terminate an unexpected process.'
                }
                Stop-Process -Id $editor.Id
                $editor.WaitForExit()
                $intentionalStop = $true
                if ((Get-FileHash -LiteralPath $copy -Algorithm SHA256).Hash -ne $interim.recovery_sha256) {
                    throw 'Checkpoint changed during forced termination.'
                }
                break
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
    $passed = -not $forcedStop -and $test.editor_initialized -and $test.sdk_module_sha256 -eq $expectedSdkHash
    if ($ForceExit) {
        $passed = $passed -and $intentionalStop -and -not $test.about_to_quit -and
            $test.status -eq 'READY_TO_TERMINATE' -and [RecoveryTestDesktop]::ExitCode() -ne 0
    } else {
        $passed = $passed -and -not $intentionalStop -and [RecoveryTestDesktop]::ExitCode() -eq 0 -and
            $test.status -eq 'PASSED' -and $test.about_to_quit
    }
    $row = [ordered]@{
        status=$(if ($passed) {'PASSED'} else {'FAILED'}); case=$Case; pid=$editor.Id
        exit_code=[RecoveryTestDesktop]::ExitCode(); forced_stop=$forcedStop; about_to_quit=$test.about_to_quit
        editor_initialized=$test.editor_initialized; intentional_termination=$intentionalStop
        sdk_module_path=$test.sdk_module_path; sdk_module_sha256=$test.sdk_module_sha256
        elapsed_seconds=[Math]::Round($clock.Elapsed.TotalSeconds, 3)
        workspace=$workspaceRoot; checks=$test.checks; result="$caseRoot/result.json"
    }
    $row | ConvertTo-Json -Depth 6 | Set-Content "$caseRoot/process-result.json"
    if (-not $passed) { throw "Editor recovery case $Name failed; see $caseRoot." }
    return $row
}
Add-Type -TypeDefinition @'
using System;
using System.Text;
using System.Diagnostics;
using System.Runtime.InteropServices;
public static class RecoveryTestDesktop {
 [StructLayout(LayoutKind.Sequential,CharSet=CharSet.Unicode)] struct SI {
  public int cb; public string reserved,desktop,title; public int x,y,xSize,ySize,xCount,yCount,fill,flags;
  public short show,reserved2; public IntPtr reservedPtr,input,output,error;
 }
 [StructLayout(LayoutKind.Sequential)] struct PI { public IntPtr process,thread; public int pid,tid; }
 [DllImport("user32.dll",CharSet=CharSet.Unicode,SetLastError=true)] static extern IntPtr CreateDesktop(string name,IntPtr device,IntPtr mode,int flags,uint access,IntPtr attrs);
 [DllImport("user32.dll",SetLastError=true)] static extern bool CloseDesktop(IntPtr desktop);
 [DllImport("kernel32.dll",CharSet=CharSet.Unicode,SetLastError=true)] static extern bool CreateProcess(string app,StringBuilder cmd,IntPtr p,IntPtr t,bool inherit,uint flags,IntPtr env,string dir,ref SI si,out PI pi);
 [DllImport("kernel32.dll")] static extern bool CloseHandle(IntPtr handle);
 [DllImport("kernel32.dll",SetLastError=true)] static extern bool GetExitCodeProcess(IntPtr handle,out uint code);
 static IntPtr processHandle;
 public static int ExitCode() { uint code; if(!GetExitCodeProcess(processHandle,out code)) throw new System.ComponentModel.Win32Exception(Marshal.GetLastWin32Error()); return unchecked((int)code); }
 static IntPtr handle; static string name;
 public static void Open() {
  name="FOASDKRecovery-"+Guid.NewGuid().ToString("N");
  handle=CreateDesktop(name,IntPtr.Zero,IntPtr.Zero,0,0x10000000,IntPtr.Zero);
  if(handle==IntPtr.Zero) throw new System.ComponentModel.Win32Exception(Marshal.GetLastWin32Error());
 }
 public static Process Start(string exe,string args,string cwd) {
  if(processHandle!=IntPtr.Zero) { CloseHandle(processHandle); processHandle=IntPtr.Zero; }
  SI si=new SI(); si.cb=Marshal.SizeOf(si); si.desktop="WinSta0\\"+name; si.flags=1; si.show=0; PI pi;
  if(!CreateProcess(exe,new StringBuilder("\""+exe+"\" "+args),IntPtr.Zero,IntPtr.Zero,false,0,IntPtr.Zero,cwd,ref si,out pi))
   throw new System.ComponentModel.Win32Exception(Marshal.GetLastWin32Error());
  try { processHandle=pi.process; return Process.GetProcessById(pi.pid); } finally { CloseHandle(pi.thread); }
 }
 public static void Close() { if(processHandle!=IntPtr.Zero) { CloseHandle(processHandle); processHandle=IntPtr.Zero; } if(handle!=IntPtr.Zero) { CloseDesktop(handle); handle=IntPtr.Zero; } }
}
'@
[RecoveryTestDesktop]::Open()
$expectedSdkHash = (Get-FileHash -Algorithm SHA256 -LiteralPath (Join-Path (Split-Path $EditorExecutable) 'TaintedGrailModdingSDK.Editor.dll')).Hash
$rows = @()
$status = 'FAILED'
$expectedCount = 0
try {
    if ($Suite -in @('all', 'new', 'docked')) {
        $seed = Invoke-RecoveryCase -Name seed-new -Case seed-new -ForceExit $true
        $rows += $seed
        $rows += Invoke-RecoveryCase -Name isolate -Case isolate -Reuse $seed.workspace -ExpectedResult $seed.result
        $rows += Invoke-RecoveryCase -Name restore-again -Case restore-again -Reuse $seed.workspace -ExpectedResult $seed.result -ForceExit $true
        $saved = Invoke-RecoveryCase -Name restore-save -Case restore-save -Reuse $seed.workspace -ExpectedResult $seed.result
        $rows += $saved
        $rows += Invoke-RecoveryCase -Name verify-save -Case verify-save -Reuse $seed.workspace -ExpectedResult $saved.result
        $expectedCount += 5
    }
    if ($Suite -in @('all', 'saved', 'docked')) {
        $seed = Invoke-RecoveryCase -Name seed-saved -Case seed-saved -ForceExit $true
        $rows += $seed
        $saved = Invoke-RecoveryCase -Name restore-discard -Case restore-discard -Reuse $seed.workspace -ExpectedResult $seed.result
        $rows += $saved
        $rows += Invoke-RecoveryCase -Name verify-discard -Case verify-discard -Reuse $seed.workspace -ExpectedResult $saved.result
        $expectedCount += 3
    }
    if ($Suite -in @('all', 'failures', 'docked')) {
        $rows += Invoke-RecoveryCase -Name failure -Case failure
        $seed = Invoke-RecoveryCase -Name seed-corrupt -Case seed-new -ForceExit $true
        $rows += $seed
        $rows += Invoke-RecoveryCase -Name reject-corrupt -Case reject-corrupt -Reuse $seed.workspace -ExpectedResult $seed.result -Corrupt $true
        $expectedCount += 3
    }
    if ($Suite -eq 'docked') {
        $rows += Invoke-RecoveryCase -Name docked-close -Case docked-close
        $expectedCount += 1
    }
    if ($expectedCount -eq 0 -or $rows.Count -ne $expectedCount) { throw 'Missing required recovery phases.' }
    $status = 'PASSED'
} finally {
    [RecoveryTestDesktop]::Close()
    foreach ($name in $environmentNames) {
        [Environment]::SetEnvironmentVariable($name, $savedEnvironment[$name], 'Process')
    }
    [ordered]@{status=$status;suite=$Suite;engine_commit=$engineCommit;isolated_desktop=$true;cases=$rows} |
        ConvertTo-Json -Depth 8 | Set-Content (Join-Path $OutputRoot 'suite-result.json')
}
Write-Host "PASSED: $($rows.Count) recovery process phases with verified checkpoints and exit outcomes."
