# SPDX-License-Identifier: Apache-2.0 OR MIT

[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$InstallerExe,

    [Parameter(Mandatory = $true)]
    [string]$InstallRoot,

    [Parameter(Mandatory = $true)]
    [string]$EvidenceRoot,

    [Parameter(Mandatory = $false)]
    [string]$StagedManifest,

    [Parameter(Mandatory = $false)]
    [string]$ExternalWorkspace
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$script:Steps = @()
$script:Result = "failed"
$script:Failure = $null
$script:ReviewedLauncherHash = $null
$script:ProfilePath = $null

function Resolve-FullPath {
    param([Parameter(Mandatory = $true)][string]$PathValue)
    return [System.IO.Path]::GetFullPath($PathValue)
}

function Get-Sha256 {
    param([Parameter(Mandatory = $true)][string]$PathValue)
    return (Get-FileHash -LiteralPath $PathValue -Algorithm SHA256).Hash
}

function Invoke-ReadinessCommand {
    param(
        [Parameter(Mandatory = $true)][string]$Name,
        [Parameter(Mandatory = $true)][string]$FilePath,
        [Parameter(Mandatory = $false)][string[]]$Arguments = @()
    )

    $safeName = $Name.ToLowerInvariant() -replace "[^a-z0-9]+", "-"
    $stdoutPath = Join-Path $EvidenceRoot "$safeName.stdout.log"
    $stderrPath = Join-Path $EvidenceRoot "$safeName.stderr.log"
    $startedUtc = (Get-Date).ToUniversalTime().ToString("o")

    $startInfo = [System.Diagnostics.ProcessStartInfo]::new()
    $startInfo.FileName = $FilePath
    $startInfo.UseShellExecute = $false
    $startInfo.CreateNoWindow = $true
    $startInfo.RedirectStandardOutput = $true
    $startInfo.RedirectStandardError = $true
    foreach ($argument in $Arguments) {
        $startInfo.ArgumentList.Add($argument)
    }

    $process = [System.Diagnostics.Process]::Start($startInfo)
    if ($null -eq $process) {
        throw "$Name did not start."
    }
    $stdoutTask = $process.StandardOutput.ReadToEndAsync()
    $stderrTask = $process.StandardError.ReadToEndAsync()
    $process.WaitForExit()
    $stdoutTask.GetAwaiter().GetResult() | Set-Content -LiteralPath $stdoutPath -Encoding utf8
    $stderrTask.GetAwaiter().GetResult() | Set-Content -LiteralPath $stderrPath -Encoding utf8
    $exitCode = $process.ExitCode

    $step = [ordered]@{
        name = $Name
        file = $FilePath
        arguments = $Arguments
        exit_code = $exitCode
        started_utc = $startedUtc
        finished_utc = (Get-Date).ToUniversalTime().ToString("o")
        stdout = $stdoutPath
        stderr = $stderrPath
    }
    $script:Steps += $step

    if ($exitCode -ne 0) {
        $stderrText = if (Test-Path -LiteralPath $stderrPath -PathType Leaf) {
            (Get-Content -LiteralPath $stderrPath -Raw)
        } else {
            ""
        }
        throw "$Name failed with exit code $exitCode. stderr: $stderrText"
    }
}

function New-FileFixture {
    param([Parameter(Mandatory = $true)][string]$PathValue)
    New-Item -ItemType Directory -Path (Split-Path -Parent $PathValue) -Force | Out-Null
    Set-Content -LiteralPath $PathValue -Encoding ascii -NoNewline -Value "fixture"
}

function Write-ReadinessSummary {
    param([Parameter(Mandatory = $true)][string]$SummaryPath)

    $installerLogs = @()
    $logRoot = Join-Path $EvidenceRoot "installer-logs"
    if (Test-Path -LiteralPath $logRoot -PathType Container) {
        $installerLogs = Get-ChildItem -LiteralPath $logRoot -Filter "*.log" -File |
            Sort-Object Name |
            ForEach-Object { $_.FullName }
    }

    $summary = [ordered]@{
        schema = "foa.sdk.windows_functional_readiness.v1"
        result = $script:Result
        failure = $script:Failure
        generated_utc = (Get-Date).ToUniversalTime().ToString("o")
        installer_exe = $InstallerExe
        install_root = $InstallRoot
        evidence_root = $EvidenceRoot
        external_workspace = $ExternalWorkspace
        tool_profile = $script:ProfilePath
        installed_launcher_sha256 = $script:ReviewedLauncherHash
        installer_logs = $installerLogs
        steps = $script:Steps
    }
    $summary | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $SummaryPath -Encoding utf8
}

try {
    $InstallerExe = Resolve-FullPath $InstallerExe
    $InstallRoot = Resolve-FullPath $InstallRoot
    $EvidenceRoot = Resolve-FullPath $EvidenceRoot
    if ([string]::IsNullOrWhiteSpace($ExternalWorkspace)) {
        $ExternalWorkspace = Join-Path $EvidenceRoot "external-workspace"
    }
    $ExternalWorkspace = Resolve-FullPath $ExternalWorkspace

    New-Item -ItemType Directory -Path $EvidenceRoot -Force | Out-Null
    New-Item -ItemType Directory -Path $ExternalWorkspace -Force | Out-Null

    if (-not (Test-Path -LiteralPath $InstallerExe -PathType Leaf)) {
        throw "Self-contained installer wizard is missing: $InstallerExe"
    }

    Invoke-ReadinessCommand `
        -Name "installer-wizard-smoke" `
        -FilePath $InstallerExe `
        -Arguments @("--smoke-test", "--evidence-root", $EvidenceRoot)

    Invoke-ReadinessCommand `
        -Name "tool-wizard-ui-smoke" `
        -FilePath $InstallerExe `
        -Arguments @("--tool-wizard", "--smoke-test", "--install-root", $InstallRoot)

    Invoke-ReadinessCommand `
        -Name "installer-clean-install" `
        -FilePath $InstallerExe `
        -Arguments @(
            "--quiet",
            "--operation", "install",
            "--install-root", $InstallRoot,
            "--evidence-root", $EvidenceRoot,
            "--no-launch-after-install",
            "--no-open-tool-wizard-after-install"
        )

    $launcher = Join-Path $InstallRoot "bin/Windows/profile/Default/FOA-SDK.exe"
    $installedManifest = Join-Path $InstallRoot "INSTALL_MANIFEST.json"
    if (-not (Test-Path -LiteralPath $launcher -PathType Leaf)) {
        throw "FOA-SDK.exe was not installed as the SDK launcher: $launcher"
    }
    if (-not (Test-Path -LiteralPath $installedManifest -PathType Leaf)) {
        throw "MSI install did not preserve INSTALL_MANIFEST.json: $installedManifest"
    }
    if (-not [string]::IsNullOrWhiteSpace($StagedManifest)) {
        $StagedManifest = Resolve-FullPath $StagedManifest
        if (-not (Test-Path -LiteralPath $StagedManifest -PathType Leaf)) {
            throw "Reviewed staging manifest is missing: $StagedManifest"
        }
        $installedManifestHash = Get-Sha256 $installedManifest
        $stagedManifestHash = Get-Sha256 $StagedManifest
        if ($installedManifestHash -cne $stagedManifestHash) {
            throw "Installed MSI manifest differs from the exact reviewed staging manifest."
        }
    }

    $startMenuEntry = Join-Path $env:APPDATA "Microsoft/Windows/Start Menu/Programs/Tainted Grail FoA SDK/FOA-SDK.lnk"
    if (-not (Test-Path -LiteralPath $startMenuEntry -PathType Leaf)) {
        throw "MSI Start Menu entry for FOA-SDK.exe was not created: $startMenuEntry"
    }
    $shortcutShell = New-Object -ComObject WScript.Shell
    $shortcut = $shortcutShell.CreateShortcut($startMenuEntry)
    $shortcutTarget = [System.IO.Path]::GetFullPath($shortcut.TargetPath)
    $expectedTarget = [System.IO.Path]::GetFullPath($launcher)
    if (-not [string]::Equals($shortcutTarget, $expectedTarget, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "MSI Start Menu entry targets '$shortcutTarget' instead of '$expectedTarget'."
    }

    Invoke-ReadinessCommand `
        -Name "installed-launcher-self-test" `
        -FilePath $launcher `
        -Arguments @("--self-test")

    $toolRoot = Join-Path $EvidenceRoot "tool-fixtures"
    $o3deEditor = Join-Path $toolRoot "O3DE/bin/Editor.exe"
    $unityEditor = Join-Path $toolRoot "Unity/Editor/Unity.exe"
    $unityProject = Join-Path $toolRoot "UnityConversionProject"
    $tgInstall = Join-Path $toolRoot "TaintedGrail"
    New-FileFixture $o3deEditor
    New-FileFixture $unityEditor
    New-Item -ItemType Directory -Path (Join-Path $unityProject "Assets") -Force | Out-Null
    New-Item -ItemType Directory -Path (Join-Path $unityProject "ProjectSettings") -Force | Out-Null
    Set-Content -LiteralPath (Join-Path $unityProject "ProjectSettings/ProjectVersion.txt") -Encoding ascii -Value "m_EditorVersion: readiness-fixture"
    New-Item -ItemType Directory -Path $tgInstall -Force | Out-Null
    New-FileFixture (Join-Path $tgInstall "UnityPlayer.dll")

    Invoke-ReadinessCommand `
        -Name "tool-profile-save" `
        -FilePath $InstallerExe `
        -Arguments @(
            "--tool-wizard",
            "--save-tool-profile",
            "--install-root", $InstallRoot,
            "--workspace-root", $ExternalWorkspace,
            "--o3de-editor", $o3deEditor,
            "--unity-editor", $unityEditor,
            "--unity-project", $unityProject,
            "--tainted-grail-install", $tgInstall
        )

    $script:ProfilePath = Join-Path $env:LOCALAPPDATA "FOA-SDK/ToolWizard/tool-profile.local.json"
    if (-not (Test-Path -LiteralPath $script:ProfilePath -PathType Leaf)) {
        throw "Tool Wizard profile was not saved: $($script:ProfilePath)"
    }
    $profile = Get-Content -LiteralPath $script:ProfilePath -Raw | ConvertFrom-Json
    if (-not $profile.ready_for_authoring) {
        throw "Tool Wizard profile did not mark authoring readiness after validating O3DE Editor fixture."
    }
    if (-not $profile.ready_for_conversion_preview) {
        throw "Tool Wizard profile did not mark conversion preview readiness after validating Unity fixtures."
    }
    if (-not $profile.ready_for_deployment_preview) {
        throw "Tool Wizard profile did not mark deployment preview readiness after validating TG fixture."
    }
    Copy-Item -LiteralPath $script:ProfilePath -Destination (Join-Path $EvidenceRoot "tool-profile.local.json") -Force
    Set-Content -LiteralPath (Join-Path $ExternalWorkspace "preserve.txt") -Encoding ascii -Value "preserve"

    $script:ReviewedLauncherHash = Get-Sha256 $launcher
    [System.IO.File]::WriteAllBytes($launcher, [byte[]](0x4d, 0x5a, 0x00, 0x00))
    if ((Get-Sha256 $launcher) -ceq $script:ReviewedLauncherHash) {
        throw "Repair smoke test could not damage the installed FOA-SDK.exe fixture."
    }

    Invoke-ReadinessCommand `
        -Name "installer-repair" `
        -FilePath $InstallerExe `
        -Arguments @(
            "--quiet",
            "--operation", "repair",
            "--install-root", $InstallRoot,
            "--evidence-root", $EvidenceRoot,
            "--no-launch-after-install",
            "--no-open-tool-wizard-after-install"
        )

    if ((Get-Sha256 $launcher) -cne $script:ReviewedLauncherHash) {
        throw "MSI repair did not restore the reviewed product-owned launcher bytes."
    }
    Invoke-ReadinessCommand `
        -Name "repaired-launcher-self-test" `
        -FilePath $launcher `
        -Arguments @("--self-test")

    Invoke-ReadinessCommand `
        -Name "installer-uninstall" `
        -FilePath $InstallerExe `
        -Arguments @(
            "--quiet",
            "--operation", "uninstall",
            "--install-root", $InstallRoot,
            "--evidence-root", $EvidenceRoot,
            "--no-launch-after-install",
            "--no-open-tool-wizard-after-install"
        )

    if (Test-Path -LiteralPath $launcher -PathType Leaf) {
        throw "MSI uninstall left FOA-SDK.exe installed: $launcher"
    }
    if (Test-Path -LiteralPath $installedManifest -PathType Leaf) {
        throw "MSI uninstall left the product manifest installed: $installedManifest"
    }
    if (Test-Path -LiteralPath $startMenuEntry -PathType Leaf) {
        throw "MSI uninstall left the product Start Menu entry installed: $startMenuEntry"
    }
    if (-not (Test-Path -LiteralPath (Join-Path $ExternalWorkspace "preserve.txt") -PathType Leaf)) {
        throw "MSI uninstall removed external workspace data."
    }

    $script:Result = "passed"
}
catch {
    $script:Failure = $_.Exception.Message
    throw
}
finally {
    if (-not [string]::IsNullOrWhiteSpace($EvidenceRoot)) {
        New-Item -ItemType Directory -Path $EvidenceRoot -Force | Out-Null
        Write-ReadinessSummary -SummaryPath (Join-Path $EvidenceRoot "functional-readiness-summary.json")
    }
}
