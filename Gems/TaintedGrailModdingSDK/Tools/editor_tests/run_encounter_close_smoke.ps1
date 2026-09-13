# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
# SPDX-License-Identifier: Apache-2.0 OR MIT
#Requires -Version 7.0
<#
.SYNOPSIS
Tests Spawn and Encounter Editor draft protection on a private Windows desktop.
.DESCRIPTION
Requires a pinned built Editor and a prepared asset cache. Creates synthetic data
only in a fresh external OutputRoot. Never activates the test desktop. Captures
the actual loaded SDK module hash, native process exit and each acceptance result.
#>
param(
    [Parameter(Mandatory)][string]$EditorExecutable,
    [Parameter(Mandatory)][string]$EngineRoot,
    [Parameter(Mandatory)][string]$CacheRoot,
    [Parameter(Mandatory)][string]$OutputRoot,
    [ValidateRange(30, 600)][int]$TimeoutSeconds = 240,
    [ValidateSet('close','workspace-status','workspace-catalog')][string]$Suite = 'close'
)
$ErrorActionPreference = 'Stop'
if (-not $IsWindows) { throw 'Windows is required for the native file-lock cases.' }
$productRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../../..'))
$EditorExecutable = (Resolve-Path -LiteralPath $EditorExecutable).Path
$EngineRoot = (Resolve-Path -LiteralPath $EngineRoot).Path
$CacheRoot = (Resolve-Path -LiteralPath $CacheRoot).Path
$OutputRoot = [IO.Path]::GetFullPath($OutputRoot)
foreach ($path in @($OutputRoot, $CacheRoot)) {
    foreach ($source in @($productRoot, $EngineRoot)) {
        if ($path.Equals($source, [StringComparison]::OrdinalIgnoreCase) -or
            $path.StartsWith($source.TrimEnd('\', '/') + [IO.Path]::DirectorySeparatorChar,
                             [StringComparison]::OrdinalIgnoreCase)) {
            throw 'Generated data must stay outside product and engine source.'
        }
    }
}
if (Test-Path -LiteralPath $OutputRoot) { throw 'OutputRoot must be fresh.' }
$lock = Get-Content (Join-Path $productRoot 'o3de.lock.json') -Raw | ConvertFrom-Json
$engineCommit = (git -C $EngineRoot rev-parse HEAD).Trim()
if ($LASTEXITCODE -ne 0 -or $engineCommit -ne $lock.commit) { throw 'Engine pin mismatch.' }
$expectedHash = (Get-FileHash -Algorithm SHA256 -LiteralPath (Join-Path (Split-Path $EditorExecutable) 'TaintedGrailModdingSDK.Editor.dll')).Hash
$testScript = Join-Path $PSScriptRoot $(if ($Suite -eq 'close') { 'encounter_close_live_smoke.py' } else { 'encounter_workspace_live_smoke.py' })
$names = @('LOCALAPPDATA','TEMP','TMP','QT_QPA_PLATFORM','FOA_SDK_ENCOUNTER_WORKSPACE','FOA_SDK_ENCOUNTER_RESULT','FOA_SDK_ENCOUNTER_WORKSPACE_ROUTE')
$savedEnvironment = @{}
foreach ($name in $names) { $savedEnvironment[$name] = [Environment]::GetEnvironmentVariable($name, 'Process') }
$workspaceRoot = (Join-Path $OutputRoot 'a/FOA-SDK/Workspace').Replace('\', '/')
foreach ($leaf in @('Output','Staging','Deployment','Diagnostics','Extracted','Game/Managed','Game/BepInEx/plugins')) {
    New-Item -ItemType Directory -Force -Path "$workspaceRoot/$leaf" | Out-Null
}
$fixture = [ordered]@{
    SchemaVersion=1; WorkspaceId='sdkqa.encounter-close'; DisplayName='Synthetic pane close acceptance'
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
    Set-Content "$workspaceRoot/$marker" 'Synthetic marker only; not executable content.'
}
foreach ($leaf in @('tmp','user','log')) { New-Item -ItemType Directory -Path "$OutputRoot/$leaf" | Out-Null }
Add-Type -TypeDefinition @'
using System;
using System.Text;
using System.Diagnostics;
using System.Runtime.InteropServices;
public static class EncounterCloseTestDesktop {
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
  name="FOASDKEncounterClose-"+Guid.NewGuid().ToString("N");
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
[EncounterCloseTestDesktop]::Open()
$editor = $null
$status = 'FAILED'
$forcedStop = $false
$clock = [Diagnostics.Stopwatch]::StartNew()
try {
    $env:LOCALAPPDATA = Join-Path $OutputRoot 'a'
    $env:TEMP = Join-Path $OutputRoot 'tmp'
    $env:TMP = $env:TEMP
    $env:FOA_SDK_ENCOUNTER_WORKSPACE = "$workspaceRoot/foa-sdk.tgworkspace.json"
    $env:FOA_SDK_ENCOUNTER_RESULT = "$OutputRoot/result.json"
    $env:FOA_SDK_ENCOUNTER_WORKSPACE_ROUTE = $Suite.Replace("workspace-", "")
    Remove-Item Env:QT_QPA_PLATFORM -ErrorAction SilentlyContinue
    $arguments = @(
        '--project-path', (Join-Path $productRoot 'TaintedGrailModdingEditor'),
        '--engine-path', $EngineRoot, '--project-cache-path', $CacheRoot,
        '--project-user-path', "$OutputRoot/user", '--project-log-path', "$OutputRoot/log",
        '--NullRenderer', '--skipWelcomeScreenDialog',
        '--regset=/Amazon/AzCore/Bootstrap/remote_port=46027',
        '--regset=/Amazon/AzCore/Bootstrap/remote_filesystem=0',
        '--regset=/Amazon/AzCore/Bootstrap/wait_for_connect=0',
        '--regset=/Amazon/AzCore/Bootstrap/connect_ap_timeout=1',
        '--regset=/Amazon/AzCore/Bootstrap/launch_ap_timeout=1',
        '--runpython', $testScript
    )
    $quoted = $arguments | ForEach-Object {
        if ($_ -match '"') { throw 'Unexpected quote in an Editor argument.' }
        '"' + $_ + '"'
    }
    $editor = [EncounterCloseTestDesktop]::Start($EditorExecutable, ($quoted -join ' '), (Split-Path $EditorExecutable))
    [ordered]@{pid=$editor.Id;executable=$EditorExecutable;arguments=$arguments;workspace=$workspaceRoot} |
        ConvertTo-Json -Depth 5 | Set-Content "$OutputRoot/launch.json"
    while (-not $editor.WaitForExit(1000)) {
        $interim = $null
        if (Test-Path -LiteralPath "$OutputRoot/result.json") {
            try { $interim = Get-Content "$OutputRoot/result.json" -Raw | ConvertFrom-Json } catch {}
        }
        if ($interim.status -eq 'FAILED' -or $clock.Elapsed.TotalSeconds -gt $TimeoutSeconds) { break }
    }
    if (-not $editor.HasExited) {
        if (-not $editor.Path.Equals($EditorExecutable, [StringComparison]::OrdinalIgnoreCase)) {
            throw 'Unexpected process; refusing termination.'
        }
        Stop-Process -Id $editor.Id
        $forcedStop = $true
    }
    $editor.WaitForExit()
    $test = Get-Content "$OutputRoot/result.json" -Raw | ConvertFrom-Json
    $minimumChecks = $(if ($Suite -eq 'close') { 28 } else { 47 })
    if ($forcedStop -or [EncounterCloseTestDesktop]::ExitCode() -ne 0 -or
        $test.status -ne 'PASSED' -or -not $test.about_to_quit -or -not $test.editor_initialized -or
        $test.sdk_module_sha256 -ne $expectedHash -or $test.checks.Count -lt $minimumChecks) {
        throw "Encounter draft acceptance failed; see $OutputRoot."
    }
    $status = 'PASSED'
} finally {
    if ($editor -and -not $editor.HasExited -and
        $editor.Path.Equals($EditorExecutable, [StringComparison]::OrdinalIgnoreCase)) {
        Stop-Process -Id $editor.Id
        $editor.WaitForExit()
        $forcedStop = $true
    }
    $exitCode = $(if ($editor) {[EncounterCloseTestDesktop]::ExitCode()} else {$null})
    [ordered]@{status=$status;suite=$Suite;engine_commit=$engineCommit;isolated_desktop=$true;
        sdk_module_sha256=$expectedHash;forced_stop=$forcedStop;exit_code=$exitCode;
        elapsed_seconds=[Math]::Round($clock.Elapsed.TotalSeconds, 3);result="$OutputRoot/result.json"} |
        ConvertTo-Json -Depth 5 | Set-Content "$OutputRoot/process-result.json"
    [EncounterCloseTestDesktop]::Close()
    foreach ($name in $names) { [Environment]::SetEnvironmentVariable($name, $savedEnvironment[$name], 'Process') }
}
Write-Host "PASSED: $($test.checks.Count) encounter $Suite checks and clean Editor exit."
