# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
# SPDX-License-Identifier: Apache-2.0 OR MIT
#Requires -Version 7.0
<#
.SYNOPSIS
Tests Item and Recipe Editor draft protection on a private Windows desktop.
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
    [ValidateSet('close','workspace-status','workspace-catalog','exit-save','exit-discard','exit-clean','exit-rollback')][string]$Suite = 'close'
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
$testScript = Join-Path $PSScriptRoot $(if ($Suite -eq 'close') {
    'item_recipe_close_live_smoke.py'
} elseif ($Suite.StartsWith('exit-')) { 'item_recipe_editor_exit_live_smoke.py' }
else { 'item_recipe_workspace_live_smoke.py' })
$names = @('LOCALAPPDATA','TEMP','TMP','QT_QPA_PLATFORM','FOA_SDK_ECONOMY_WORKSPACE','FOA_SDK_ECONOMY_RESULT','FOA_SDK_ECONOMY_WORKSPACE_ROUTE')
$savedEnvironment = @{}
foreach ($name in $names) { $savedEnvironment[$name] = [Environment]::GetEnvironmentVariable($name, 'Process') }
$workspaceRoot = (Join-Path $OutputRoot 'a/FOA-SDK/Workspace').Replace('\', '/')
foreach ($leaf in @('Output','Staging','Deployment','Diagnostics','Extracted','Game/Managed','Game/BepInEx/plugins')) {
    New-Item -ItemType Directory -Force -Path "$workspaceRoot/$leaf" | Out-Null
}
$fixture = [ordered]@{
    SchemaVersion=1; WorkspaceId='sdkqa.item-recipe-close'; DisplayName='Synthetic pane close acceptance'
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
# Exact-association synthetic evidence is independent of the random authored item
# identities. It authorizes no game action and is loaded through normal startup.
$sourceId = 'source.sdkqa.acquisition'
$sourceRoot = "$workspaceRoot/Sources/$sourceId"
New-Item -ItemType Directory -Force -Path $sourceRoot | Out-Null
$artifact = "$sourceRoot/fixture.json"
$captured = [DateTime]::UtcNow.ToString('yyyy-MM-ddTHH:mm:ssZ')
$observations = @('docked','floating') | ForEach-Object {
    [ordered]@{evidence_id="sdkqa.evidence.$_";subject_ref="relationship:sdkqa.$_";
        claim='Synthetic acquisition association for pane acceptance only';
        evidence_kind='synthetic-fixture';confidence='documented'}
}
[ordered]@{evidence=@($observations)} | ConvertTo-Json -Depth 5 | Set-Content $artifact -Encoding utf8
$fingerprint = 'sha256:' + (Get-FileHash $artifact -Algorithm SHA256).Hash.ToLowerInvariant()
$source = [ordered]@{
    SourceId=$sourceId;Title='Synthetic acquisition associations';SourceKind='json-register'
    Locator=$artifact;Fingerprint=$fingerprint;ProfileId='sdkqa.synthetic';GameVersion='1.0.0';Branch='mono'
    RuntimeTarget='Mono';ToolName='SDK pane acceptance fixture';ToolVersion='1.0.0'
    ImporterId='tg.structured-json';ImporterVersion='1.0.0';CapturedAt=$captured;ImportedAt=$captured
    Limitations='Synthetic Editor acceptance only; no game or runtime authority.'
    MediaType='application/json';ByteSize=(Get-Item $artifact).Length;ImportStatus='imported'
}
[ordered]@{Type='JsonSerialization';Version=1;ClassName='SourceDocument';ClassData=@{Source=$source}} |
    ConvertTo-Json -Depth 6 | Set-Content "$sourceRoot/source.tgsource.json" -Encoding utf8
$evidence = @('docked','floating') | ForEach-Object {
    [ordered]@{EvidenceId="sdkqa.evidence.$_";SourceId=$sourceId;SourceFingerprint=$fingerprint
        ProfileId='sdkqa.synthetic';GameVersion='1.0.0';Branch='mono';SubjectRef="relationship:sdkqa.$_"
        Claim='Synthetic acquisition association for pane acceptance only';EvidenceKind='synthetic-fixture'
        Confidence='documented';Locator=$artifact;RecordPath=$(if ($_ -eq 'docked') {'$.evidence[0]'} else {'$.evidence[1]'})
        ExtractedAt=$captured}
}
[ordered]@{Type='JsonSerialization';Version=1;ClassName='EvidenceDocument';ClassData=@{
    SourceId=$sourceId;SourceFingerprint=$fingerprint;ProfileId='sdkqa.synthetic';GameVersion='1.0.0'
    Branch='mono';Evidence=@($evidence)}} | ConvertTo-Json -Depth 6 |
    Set-Content "$sourceRoot/evidence.tgevidence.json" -Encoding utf8
foreach ($leaf in @('tmp','user','log')) { New-Item -ItemType Directory -Path "$OutputRoot/$leaf" | Out-Null }
Add-Type -TypeDefinition @'
using System;
using System.Text;
using System.Diagnostics;
using System.Runtime.InteropServices;
public static class EconomyCloseTestDesktop {
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
  name="FOASDKEconomyClose-"+Guid.NewGuid().ToString("N");
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
[EconomyCloseTestDesktop]::Open()
$editor = $null
$status = 'FAILED'
$forcedStop = $false
$clock = [Diagnostics.Stopwatch]::StartNew()
try {
    $env:LOCALAPPDATA = Join-Path $OutputRoot 'a'
    $env:TEMP = Join-Path $OutputRoot 'tmp'
    $env:TMP = $env:TEMP
    $env:FOA_SDK_ECONOMY_WORKSPACE = "$workspaceRoot/foa-sdk.tgworkspace.json"
    $env:FOA_SDK_ECONOMY_RESULT = "$OutputRoot/result.json"
    $env:FOA_SDK_ECONOMY_WORKSPACE_ROUTE = $Suite.Replace('workspace-', '')
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
    $editor = [EconomyCloseTestDesktop]::Start($EditorExecutable, ($quoted -join ' '), (Split-Path $EditorExecutable))
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
    $minimumChecks = switch ($Suite) {
        'close' {25}
        'exit-save' {27}
        'exit-discard' {2}
        'exit-clean' {3}
        'exit-rollback' {6}
        default {20}
    }
    if ($forcedStop -or [EconomyCloseTestDesktop]::ExitCode() -ne 0 -or
        $test.status -ne 'PASSED' -or -not $test.about_to_quit -or -not $test.editor_initialized -or
        $test.sdk_module_sha256 -ne $expectedHash -or $test.checks.Count -lt $minimumChecks) {
        throw "Pane close acceptance failed; see $OutputRoot."
    }
    $status = 'PASSED'
} finally {
    if ($editor -and -not $editor.HasExited -and
        $editor.Path.Equals($EditorExecutable, [StringComparison]::OrdinalIgnoreCase)) {
        Stop-Process -Id $editor.Id
        $editor.WaitForExit()
        $forcedStop = $true
    }
    $exitCode = $(if ($editor) {[EconomyCloseTestDesktop]::ExitCode()} else {$null})
    [ordered]@{status=$status;suite=$Suite;engine_commit=$engineCommit;isolated_desktop=$true;
        sdk_module_sha256=$expectedHash;forced_stop=$forcedStop;exit_code=$exitCode;
        elapsed_seconds=[Math]::Round($clock.Elapsed.TotalSeconds, 3);result="$OutputRoot/result.json"} |
        ConvertTo-Json -Depth 5 | Set-Content "$OutputRoot/process-result.json"
    [EconomyCloseTestDesktop]::Close()
    foreach ($name in $names) { [Environment]::SetEnvironmentVariable($name, $savedEnvironment[$name], 'Process') }
}
Write-Host "PASSED: $($test.checks.Count) $Suite checks and clean Editor exit."
