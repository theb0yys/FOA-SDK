# SPDX-License-Identifier: Apache-2.0 OR MIT
[CmdletBinding()]
param(
    [string]$EvidenceRoot = ""
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

if (-not $IsWindows) {
    throw "FOA-SDK installer operational smoke requires Windows."
}

Add-Type -AssemblyName UIAutomationClient
Add-Type -AssemblyName UIAutomationTypes

$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..\..")).Path
$SourceCommit = (& git -C $RepoRoot rev-parse HEAD).Trim()
if ($LASTEXITCODE -ne 0 -or $SourceCommit -notmatch '^[0-9a-f]{40}$') {
    throw "Unable to resolve the exact repository source commit."
}

$WorkRoot = Join-Path ([IO.Path]::GetTempPath()) ("foa-sdk-installer-windows-smoke-" + [Guid]::NewGuid().ToString("N"))
if ([string]::IsNullOrWhiteSpace($EvidenceRoot)) {
    $EvidenceRoot = Join-Path $WorkRoot "evidence"
} else {
    $EvidenceRoot = [IO.Path]::GetFullPath($EvidenceRoot)
}
$PayloadRoot = Join-Path $WorkRoot "payload"
$LauncherDirectory = Join-Path $PayloadRoot "bin\Windows\profile\Default"
$StubSource = Join-Path $WorkRoot "stub-source"
$StubOutput = Join-Path $WorkRoot "stub-output"
$WixToolRoot = Join-Path $WorkRoot "wix"
$ValidMsi = Join-Path $WorkRoot "foa-sdk-windows-smoke-valid.msi"
$InvalidMsi = Join-Path $WorkRoot "foa-sdk-windows-smoke-invalid.msi"
$InstallerOutput = Join-Path $WorkRoot "installer"
$InstallRoot = Join-Path $WorkRoot "installed-foa-sdk"
$InvalidInstallRoot = Join-Path $WorkRoot "installed-foa-sdk-invalid"
$GuidedInstallRoot = Join-Path $WorkRoot "installed-foa-sdk-guided"
$ExternalRoot = Join-Path $WorkRoot "external-workspace"
$ExternalSentinel = Join-Path $ExternalRoot "must-survive.txt"
$LaunchMarker = Join-Path $WorkRoot "guided-launch-marker.txt"
$DesktopRoot = [Environment]::GetFolderPath([Environment+SpecialFolder]::DesktopDirectory)
$DesktopShortcut = Join-Path $DesktopRoot "FOA-SDK.lnk"
$SummaryPath = Join-Path $EvidenceRoot "windows-installer-smoke-summary.json"
$Result = [ordered]@{
    schema = "foa.sdk.windows_installer_smoke.v1"
    source_commit = $SourceCommit
    status = "FAILED"
    installer_build = "NOT_RUN"
    wizard_construction = "NOT_RUN"
    clean_install = "NOT_RUN"
    installed_integrity_and_startup_validation = "NOT_RUN"
    repair = "NOT_RUN"
    invalid_integrity_rejection = "NOT_RUN"
    uninstall = "NOT_RUN"
    guided_user_flow = "NOT_RUN"
    external_workspace_preserved = "NOT_RUN"
}

function Ensure-Directory([string]$Path) {
    New-Item -ItemType Directory -Force -Path $Path | Out-Null
}

function Write-Utf8NoBomLf([string]$Path, [string]$Text) {
    $Normalized = $Text -replace "`r`n", "`n"
    [IO.File]::WriteAllText($Path, $Normalized, [Text.UTF8Encoding]::new($false))
}

function Invoke-Checked([string]$Label, [scriptblock]$Command) {
    Write-Host "=== $Label ==="
    & $Command
    if ($LASTEXITCODE -ne 0) {
        throw "$Label failed with exit code $LASTEXITCODE."
    }
}

function Write-MsiChecksum([string]$MsiPath) {
    $Hash = (Get-FileHash -LiteralPath $MsiPath -Algorithm SHA256).Hash.ToLowerInvariant()
    Write-Utf8NoBomLf -Path "$MsiPath.sha256" -Text "$Hash  $([IO.Path]::GetFileName($MsiPath))`n"
}

function Escape-Xml([string]$Value) {
    return [Security.SecurityElement]::Escape($Value)
}

function Build-FixtureMsi([string]$OutputPath) {
    $ManifestPath = Join-Path $PayloadRoot "INSTALL_MANIFEST.json"
    $ChecksumsPath = Join-Path $PayloadRoot "SHA256SUMS"
    $LauncherPath = Join-Path $LauncherDirectory "FOA-SDK.exe"
    $ControlPanelPath = Join-Path $PayloadRoot "FOA-SDK-ControlPanel.exe"
    $WxsPath = Join-Path $WorkRoot (([IO.Path]::GetFileNameWithoutExtension($OutputPath)) + ".wxs")
    $UpgradeCode = "E34992B7-97D0-4CA7-9744-43CA130F9B66"

    $Wxs = @"
<Wix xmlns="http://wixtoolset.org/schemas/v4/wxs">
  <Package Name="FOA-SDK Windows Smoke Payload" Manufacturer="FOA-SDK" Version="1.0.0" UpgradeCode="$UpgradeCode" Scope="perUser">
    <MajorUpgrade DowngradeErrorMessage="A newer smoke payload is already installed." />
    <MediaTemplate EmbedCab="yes" />
    <StandardDirectory Id="LocalAppDataFolder">
      <Directory Id="INSTALL_ROOT" Name="FOA-SDK-Windows-Smoke">
        <Component Id="ManifestComponent" Guid="*">
          <File Id="ManifestFile" Source="$(Escape-Xml $ManifestPath)" Name="INSTALL_MANIFEST.json" KeyPath="yes" />
        </Component>
        <Component Id="ChecksumsComponent" Guid="*">
          <File Id="ChecksumsFile" Source="$(Escape-Xml $ChecksumsPath)" Name="SHA256SUMS" KeyPath="yes" />
        </Component>
        <Component Id="ControlPanelComponent" Guid="*">
          <File Id="ControlPanelFile" Source="$(Escape-Xml $ControlPanelPath)" Name="FOA-SDK-ControlPanel.exe" KeyPath="yes" />
        </Component>
        <Directory Id="BinDirectory" Name="bin">
          <Directory Id="WindowsDirectory" Name="Windows">
            <Directory Id="ProfileDirectory" Name="profile">
              <Directory Id="DefaultDirectory" Name="Default">
                <Component Id="LauncherComponent" Guid="*">
                  <File Id="LauncherFile" Source="$(Escape-Xml $LauncherPath)" Name="FOA-SDK.exe" KeyPath="yes" />
                </Component>
              </Directory>
            </Directory>
          </Directory>
        </Directory>
      </Directory>
    </StandardDirectory>
    <Feature Id="Main" Title="FOA-SDK Windows Smoke Payload" Level="1">
      <ComponentRef Id="ManifestComponent" />
      <ComponentRef Id="ChecksumsComponent" />
      <ComponentRef Id="ControlPanelComponent" />
      <ComponentRef Id="LauncherComponent" />
    </Feature>
  </Package>
</Wix>
"@
    Write-Utf8NoBomLf -Path $WxsPath -Text $Wxs

    $WixExe = Join-Path $WixToolRoot "wix.exe"
    Invoke-Checked "Build fixture MSI" { & $WixExe build -arch x64 -o $OutputPath $WxsPath }
    if (-not (Test-Path -LiteralPath $OutputPath -PathType Leaf)) {
        throw "WiX did not produce the expected fixture MSI: $OutputPath"
    }
    Write-MsiChecksum $OutputPath
}

function Write-LatestInstallerLog {
    $LogRoot = Join-Path $EvidenceRoot "installer-logs"
    if (-not (Test-Path -LiteralPath $LogRoot -PathType Container)) {
        return
    }
    $Latest = Get-ChildItem -LiteralPath $LogRoot -Filter "*.log" -File |
        Sort-Object LastWriteTimeUtc -Descending |
        Select-Object -First 1
    if ($null -eq $Latest) {
        return
    }
    Write-Host "=== Latest MSI log tail: $($Latest.Name) ==="
    Get-Content -LiteralPath $Latest.FullName -Tail 160 | ForEach-Object { Write-Host $_ }
}

function Invoke-Installer([string[]]$Arguments, [int]$ExpectedExitCode = 0) {
    $InstallerExe = Join-Path $InstallerOutput "FOA-SDK-Installer.exe"
    if (-not (Test-Path -LiteralPath $InstallerExe -PathType Leaf)) {
        throw "FOA-SDK installer executable is missing: $InstallerExe"
    }

    $StartInfo = [Diagnostics.ProcessStartInfo]::new()
    $StartInfo.FileName = $InstallerExe
    $StartInfo.UseShellExecute = $false
    $StartInfo.CreateNoWindow = $true
    $StartInfo.RedirectStandardOutput = $true
    $StartInfo.RedirectStandardError = $true
    $StartInfo.Environment["FOA_SDK_INSTALLER_DEBUG_ERRORS"] = "1"
    foreach ($Argument in $Arguments) {
        $StartInfo.ArgumentList.Add($Argument)
    }

    $Process = [Diagnostics.Process]::Start($StartInfo)
    if ($null -eq $Process) {
        throw "FOA-SDK installer process did not start."
    }
    try {
        $StdoutTask = $Process.StandardOutput.ReadToEndAsync()
        $StderrTask = $Process.StandardError.ReadToEndAsync()
        $Process.WaitForExit()
        $Stdout = $StdoutTask.GetAwaiter().GetResult()
        $Stderr = $StderrTask.GetAwaiter().GetResult()
        if (-not [string]::IsNullOrWhiteSpace($Stdout)) {
            Write-Host $Stdout.TrimEnd()
        }
        if (-not [string]::IsNullOrWhiteSpace($Stderr)) {
            Write-Host $Stderr.TrimEnd()
        }
        $ExitCode = $Process.ExitCode
    }
    finally {
        $Process.Dispose()
    }

    if ($ExitCode -ne $ExpectedExitCode) {
        Write-LatestInstallerLog
        throw "FOA-SDK installer returned $ExitCode; expected $ExpectedExitCode. Arguments: $($Arguments -join ' ')"
    }
}

function Find-AutomationControl([object]$Root, [string]$Name, [object]$ControlType) {
    $NameCondition = [System.Windows.Automation.PropertyCondition]::new(
        [System.Windows.Automation.AutomationElement]::NameProperty,
        $Name)
    $TypeCondition = [System.Windows.Automation.PropertyCondition]::new(
        [System.Windows.Automation.AutomationElement]::ControlTypeProperty,
        $ControlType)
    $Condition = [System.Windows.Automation.AndCondition]::new($NameCondition, $TypeCondition)
    return $Root.FindFirst([System.Windows.Automation.TreeScope]::Descendants, $Condition)
}

function Wait-AutomationControl(
    [object]$Root,
    [string]$Name,
    [object]$ControlType,
    [int]$TimeoutSeconds = 30,
    [switch]$AllowDisabled) {
    $Deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    while ([DateTime]::UtcNow -lt $Deadline) {
        $Control = Find-AutomationControl -Root $Root -Name $Name -ControlType $ControlType
        if ($null -ne $Control) {
            $Current = $Control.Current
            if (-not $Current.IsOffscreen -and ($AllowDisabled -or $Current.IsEnabled)) {
                return $Control
            }
        }
        Start-Sleep -Milliseconds 250
    }
    return $null
}

function Invoke-AutomationButton([object]$Button) {
    $Pattern = $Button.GetCurrentPattern([System.Windows.Automation.InvokePattern]::Pattern)
    ([System.Windows.Automation.InvokePattern]$Pattern).Invoke()
}

function Assert-AutomationCheckboxChecked([object]$Checkbox, [string]$Label) {
    $Pattern = $Checkbox.GetCurrentPattern([System.Windows.Automation.TogglePattern]::Pattern)
    $Toggle = [System.Windows.Automation.TogglePattern]$Pattern
    if ($Toggle.Current.ToggleState -ne [System.Windows.Automation.ToggleState]::On) {
        throw "$Label is not selected on the installer finish screen."
    }
}

function Describe-AutomationWindow([object]$Window) {
    $TextCondition = [System.Windows.Automation.PropertyCondition]::new(
        [System.Windows.Automation.AutomationElement]::ControlTypeProperty,
        [System.Windows.Automation.ControlType]::Text)
    $Items = $Window.FindAll([System.Windows.Automation.TreeScope]::Descendants, $TextCondition)
    $Values = [System.Collections.Generic.List[string]]::new()
    foreach ($Item in $Items) {
        $Name = $Item.Current.Name
        if (-not [string]::IsNullOrWhiteSpace($Name)) {
            $Values.Add($Name)
        }
    }
    return ($Values -join " | ")
}

function Invoke-GuidedInstallerUserFlow {
    $InstallerExe = Join-Path $InstallerOutput "FOA-SDK-Installer.exe"
    if (-not (Test-Path -LiteralPath $InstallerExe -PathType Leaf)) {
        throw "FOA-SDK installer executable is missing before guided UI smoke."
    }
    if ([string]::IsNullOrWhiteSpace($DesktopRoot)) {
        throw "Windows did not provide a desktop directory for guided UI smoke."
    }
    Remove-Item -LiteralPath $DesktopShortcut -Force -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath $LaunchMarker -Force -ErrorAction SilentlyContinue

    $StartInfo = [Diagnostics.ProcessStartInfo]::new()
    $StartInfo.FileName = $InstallerExe
    $StartInfo.WorkingDirectory = $InstallerOutput
    $StartInfo.UseShellExecute = $false
    $StartInfo.Environment["FOA_SDK_SMOKE_LAUNCH_MARKER"] = $LaunchMarker
    foreach ($Argument in @(
        "--install-root", $GuidedInstallRoot,
        "--evidence-root", $EvidenceRoot,
        "--no-open-tool-wizard-after-install")) {
        $StartInfo.ArgumentList.Add($Argument)
    }

    $Process = [Diagnostics.Process]::Start($StartInfo)
    if ($null -eq $Process) {
        throw "FOA-SDK guided installer process did not start."
    }
    try {
        $Root = [System.Windows.Automation.AutomationElement]::RootElement
        $Window = Wait-AutomationControl -Root $Root -Name "FOA-SDK Setup" -ControlType ([System.Windows.Automation.ControlType]::Window) -TimeoutSeconds 30
        if ($null -eq $Window) {
            throw "The FOA-SDK Setup window did not appear."
        }

        $InstallButton = Wait-AutomationControl -Root $Window -Name "Install" -ControlType ([System.Windows.Automation.ControlType]::Button) -TimeoutSeconds 15
        if ($null -eq $InstallButton) {
            throw "The normal installer did not expose its Install button."
        }
        Invoke-AutomationButton $InstallButton

        $FinishButton = $null
        $ReadyText = $null
        $Deadline = [DateTime]::UtcNow.AddSeconds(120)
        while ([DateTime]::UtcNow -lt $Deadline) {
            if ($Process.HasExited) {
                throw "The guided installer exited before reaching its finish screen. Exit code: $($Process.ExitCode)."
            }

            $FailureText = Find-AutomationControl -Root $Window -Name "Setup could not be completed" -ControlType ([System.Windows.Automation.ControlType]::Text)
            if ($null -ne $FailureText -and -not $FailureText.Current.IsOffscreen) {
                throw "The guided installer reached a failure screen: $(Describe-AutomationWindow $Window)"
            }

            $ReadyText = Find-AutomationControl -Root $Window -Name "FOA-SDK is ready" -ControlType ([System.Windows.Automation.ControlType]::Text)
            $FinishButton = Find-AutomationControl -Root $Window -Name "Finish" -ControlType ([System.Windows.Automation.ControlType]::Button)
            if ($null -ne $ReadyText -and -not $ReadyText.Current.IsOffscreen -and $null -ne $FinishButton -and -not $FinishButton.Current.IsOffscreen -and $FinishButton.Current.IsEnabled) {
                break
            }
            Start-Sleep -Milliseconds 250
        }
        if ($null -eq $ReadyText -or $ReadyText.Current.IsOffscreen -or $null -eq $FinishButton -or $FinishButton.Current.IsOffscreen -or -not $FinishButton.Current.IsEnabled) {
            throw "The guided installer did not reach the FOA-SDK ready finish screen: $(Describe-AutomationWindow $Window)"
        }

        $ControlPanelCheckbox = Wait-AutomationControl -Root $Window -Name "Open FOA-SDK Control Panel" -ControlType ([System.Windows.Automation.ControlType]::CheckBox) -TimeoutSeconds 10
        $OpenCheckbox = Wait-AutomationControl -Root $Window -Name "Open FOA-SDK" -ControlType ([System.Windows.Automation.ControlType]::CheckBox) -TimeoutSeconds 10
        $ShortcutCheckbox = Wait-AutomationControl -Root $Window -Name "Create desktop shortcut" -ControlType ([System.Windows.Automation.ControlType]::CheckBox) -TimeoutSeconds 10
        if ($null -eq $ControlPanelCheckbox -or $null -eq $OpenCheckbox -or $null -eq $ShortcutCheckbox) {
            throw "The guided installer finish options were not available."
        }
        Assert-AutomationCheckboxChecked -Checkbox $ControlPanelCheckbox -Label "Open FOA-SDK Control Panel"
        Assert-AutomationCheckboxChecked -Checkbox $ShortcutCheckbox -Label "Create desktop shortcut"

        Invoke-AutomationButton $FinishButton
        if (-not $Process.WaitForExit(30000)) {
            throw "The guided installer did not close after Finish was selected."
        }
        if ($Process.ExitCode -ne 0) {
            throw "The guided installer returned exit code $($Process.ExitCode) after Finish."
        }
    }
    finally {
        if (-not $Process.HasExited) {
            try {
                $Process.Kill($true)
                $Process.WaitForExit()
            }
            catch {
            }
        }
        $Process.Dispose()
    }

    $InstalledLauncher = Join-Path $GuidedInstallRoot "bin\Windows\profile\Default\FOA-SDK.exe"
    if (-not (Test-Path -LiteralPath $InstalledLauncher -PathType Leaf)) {
        throw "The guided installer did not install FOA-SDK.exe."
    }

    $LaunchDeadline = [DateTime]::UtcNow.AddSeconds(15)
    while (-not (Test-Path -LiteralPath $LaunchMarker -PathType Leaf) -and [DateTime]::UtcNow -lt $LaunchDeadline) {
        Start-Sleep -Milliseconds 250
    }
    if (-not (Test-Path -LiteralPath $LaunchMarker -PathType Leaf)) {
        throw "The checked Open FOA-SDK Control Panel finish option did not launch the installed entry point."
    }

    if (-not (Test-Path -LiteralPath $DesktopShortcut -PathType Leaf)) {
        throw "The checked Create desktop shortcut finish option did not create FOA-SDK.lnk."
    }
    $Shell = New-Object -ComObject WScript.Shell
    $Shortcut = $null
    try {
        $Shortcut = $Shell.CreateShortcut($DesktopShortcut)
        $Target = [IO.Path]::GetFullPath([string]$Shortcut.TargetPath)
        if ($Target -cne [IO.Path]::GetFullPath($InstalledLauncher)) {
            throw "The guided installer desktop shortcut does not target the installed FOA-SDK.exe."
        }
    }
    finally {
        if ($null -ne $Shortcut) {
            [void][Runtime.InteropServices.Marshal]::FinalReleaseComObject($Shortcut)
        }
        [void][Runtime.InteropServices.Marshal]::FinalReleaseComObject($Shell)
    }
}

function Assert-InstalledLauncherHash([string]$ExpectedHash) {
    $InstalledLauncher = Join-Path $InstallRoot "bin\Windows\profile\Default\FOA-SDK.exe"
    if (-not (Test-Path -LiteralPath $InstalledLauncher -PathType Leaf)) {
        throw "Installed launcher is missing after setup: $InstalledLauncher"
    }
    $Actual = (Get-FileHash -LiteralPath $InstalledLauncher -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($Actual -cne $ExpectedHash) {
        throw "Installed launcher hash does not match the fixture payload after repair."
    }
}

Ensure-Directory $WorkRoot
Ensure-Directory $EvidenceRoot
Ensure-Directory $LauncherDirectory
Ensure-Directory $StubSource
Ensure-Directory $StubOutput
Ensure-Directory $ExternalRoot
Write-Utf8NoBomLf -Path $ExternalSentinel -Text "external workspace sentinel`n"

try {
    $StubProject = @"
<Project Sdk="Microsoft.NET.Sdk">
  <PropertyGroup>
    <OutputType>WinExe</OutputType>
    <TargetFramework>net8.0-windows</TargetFramework>
    <RuntimeIdentifier>win-x64</RuntimeIdentifier>
    <SelfContained>true</SelfContained>
    <PublishSingleFile>true</PublishSingleFile>
    <IncludeNativeLibrariesForSelfExtract>true</IncludeNativeLibrariesForSelfExtract>
    <AssemblyName>FOA-SDK</AssemblyName>
    <Nullable>enable</Nullable>
    <ImplicitUsings>enable</ImplicitUsings>
    <DebugType>none</DebugType>
    <DebugSymbols>false</DebugSymbols>
  </PropertyGroup>
</Project>
"@
    $StubProgram = @"
internal static class Program
{
    private static int Main(string[] args)
    {
        if (args.Length == 1 && string.Equals(args[0], "--self-test", StringComparison.Ordinal))
        {
            return 0;
        }
        string? marker = Environment.GetEnvironmentVariable("FOA_SDK_SMOKE_LAUNCH_MARKER");
        if (!string.IsNullOrWhiteSpace(marker))
        {
            string? directory = Path.GetDirectoryName(marker);
            if (!string.IsNullOrWhiteSpace(directory))
            {
                Directory.CreateDirectory(directory);
            }
            File.WriteAllText(marker, "launched");
        }
        return 0;
    }
}
"@
    Write-Utf8NoBomLf -Path (Join-Path $StubSource "FoaSdkSmokeStub.csproj") -Text $StubProject
    Write-Utf8NoBomLf -Path (Join-Path $StubSource "Program.cs") -Text $StubProgram
    Invoke-Checked "Build self-contained FOA-SDK smoke launcher" {
        & dotnet publish (Join-Path $StubSource "FoaSdkSmokeStub.csproj") -c Release -r win-x64 --self-contained true -o $StubOutput
    }

    $BuiltStub = Join-Path $StubOutput "FOA-SDK.exe"
    if (-not (Test-Path -LiteralPath $BuiltStub -PathType Leaf)) {
        throw "Smoke launcher build did not produce FOA-SDK.exe."
    }
    Copy-Item -LiteralPath $BuiltStub -Destination (Join-Path $LauncherDirectory "FOA-SDK.exe") -Force
    Copy-Item -LiteralPath $BuiltStub -Destination (Join-Path $PayloadRoot "FOA-SDK-ControlPanel.exe") -Force

    Write-Utf8NoBomLf -Path (Join-Path $PayloadRoot "INSTALL_MANIFEST.json") -Text "{`"schema_version`":1,`"product_id`":`"foa-sdk-windows-smoke`"}`n"
    $ManifestHash = (Get-FileHash -LiteralPath (Join-Path $PayloadRoot "INSTALL_MANIFEST.json") -Algorithm SHA256).Hash.ToLowerInvariant()
    $LauncherHash = (Get-FileHash -LiteralPath (Join-Path $LauncherDirectory "FOA-SDK.exe") -Algorithm SHA256).Hash.ToLowerInvariant()
    $ControlPanelHash = (Get-FileHash -LiteralPath (Join-Path $PayloadRoot "FOA-SDK-ControlPanel.exe") -Algorithm SHA256).Hash.ToLowerInvariant()
    $ValidChecksums = "$ManifestHash  INSTALL_MANIFEST.json`n$ControlPanelHash  FOA-SDK-ControlPanel.exe`n$LauncherHash  bin/Windows/profile/Default/FOA-SDK.exe`n"
    Write-Utf8NoBomLf -Path (Join-Path $PayloadRoot "SHA256SUMS") -Text $ValidChecksums

    Ensure-Directory $WixToolRoot
    Invoke-Checked "Install WiX 4 test tool" { & dotnet tool install wix --tool-path $WixToolRoot --version 4.0.4 }
    Build-FixtureMsi $ValidMsi

    $BuildScript = Join-Path $RepoRoot "Installer\Launcher\Windows\build-foa-installer-launcher.ps1"
    Invoke-Checked "Build exact self-contained FOA-SDK installer" {
        & $BuildScript -Configuration Release -RuntimeIdentifier win-x64 -InstallerMsi $ValidMsi -InstallerMsiChecksum "$ValidMsi.sha256" -OutputDirectory $InstallerOutput
    }
    $Result.installer_build = "PASSED"

    Invoke-Installer -Arguments @("--smoke-test", "--install-root", $InstallRoot, "--no-dialog")
    $Result.wizard_construction = "PASSED"

    Invoke-Installer -Arguments @(
        "--install-root", $InstallRoot,
        "--evidence-root", $EvidenceRoot,
        "--operation", "install",
        "--quiet",
        "--no-launch-after-install"
    )
    $Result.clean_install = "PASSED"
    $Result.installed_integrity_and_startup_validation = "PASSED"
    Assert-InstalledLauncherHash $LauncherHash

    $InstalledLauncher = Join-Path $InstallRoot "bin\Windows\profile\Default\FOA-SDK.exe"
    [IO.File]::AppendAllText($InstalledLauncher, "tamper", [Text.UTF8Encoding]::new($false))
    $TamperedHash = (Get-FileHash -LiteralPath $InstalledLauncher -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($TamperedHash -ceq $LauncherHash) {
        throw "Unable to damage the installed launcher before repair smoke."
    }
    Invoke-Installer -Arguments @(
        "--install-root", $InstallRoot,
        "--evidence-root", $EvidenceRoot,
        "--operation", "repair",
        "--quiet",
        "--no-launch-after-install"
    )
    Assert-InstalledLauncherHash $LauncherHash
    $Result.repair = "PASSED"

    Invoke-Installer -Arguments @(
        "--install-root", $InstallRoot,
        "--evidence-root", $EvidenceRoot,
        "--operation", "uninstall",
        "--quiet"
    )
    if (Test-Path -LiteralPath (Join-Path $InstallRoot "INSTALL_MANIFEST.json")) {
        throw "Uninstall left the smoke product manifest installed."
    }
    $Result.uninstall = "PASSED"

    $InvalidChecksums = "$ManifestHash  INSTALL_MANIFEST.json`n$ControlPanelHash  FOA-SDK-ControlPanel.exe`n$('0' * 64)  bin/Windows/profile/Default/FOA-SDK.exe`n"
    Write-Utf8NoBomLf -Path (Join-Path $PayloadRoot "SHA256SUMS") -Text $InvalidChecksums
    Build-FixtureMsi $InvalidMsi
    Invoke-Installer -Arguments @(
        "--msi", $InvalidMsi,
        "--install-root", $InvalidInstallRoot,
        "--evidence-root", $EvidenceRoot,
        "--operation", "install",
        "--quiet",
        "--no-launch-after-install"
    ) -ExpectedExitCode 1
    $Result.invalid_integrity_rejection = "PASSED"

    Invoke-Installer -Arguments @(
        "--msi", $InvalidMsi,
        "--install-root", $InvalidInstallRoot,
        "--evidence-root", $EvidenceRoot,
        "--operation", "uninstall",
        "--quiet"
    )

    Invoke-GuidedInstallerUserFlow
    $Result.guided_user_flow = "PASSED"
    Invoke-Installer -Arguments @(
        "--install-root", $GuidedInstallRoot,
        "--evidence-root", $EvidenceRoot,
        "--operation", "uninstall",
        "--quiet"
    )
    Remove-Item -LiteralPath $DesktopShortcut -Force -ErrorAction SilentlyContinue

    if (-not (Test-Path -LiteralPath $ExternalSentinel -PathType Leaf)) {
        throw "Installer lifecycle removed external workspace data."
    }
    $Result.external_workspace_preserved = "PASSED"
    $Result.status = "PASSED"
}
catch {
    $Result.error = $_.Exception.Message
    throw
}
finally {
    Remove-Item -LiteralPath $DesktopShortcut -Force -ErrorAction SilentlyContinue
    Ensure-Directory $EvidenceRoot
    $Result | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath $SummaryPath -Encoding utf8
    Write-Host "Windows installer smoke summary: $SummaryPath"
}
