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
$ExternalRoot = Join-Path $WorkRoot "external-workspace"
$ExternalSentinel = Join-Path $ExternalRoot "must-survive.txt"
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

    Write-Utf8NoBomLf -Path (Join-Path $PayloadRoot "INSTALL_MANIFEST.json") -Text "{`"schema_version`":1,`"product_id`":`"foa-sdk-windows-smoke`"}`n"
    $ManifestHash = (Get-FileHash -LiteralPath (Join-Path $PayloadRoot "INSTALL_MANIFEST.json") -Algorithm SHA256).Hash.ToLowerInvariant()
    $LauncherHash = (Get-FileHash -LiteralPath (Join-Path $LauncherDirectory "FOA-SDK.exe") -Algorithm SHA256).Hash.ToLowerInvariant()
    $ValidChecksums = "$ManifestHash  INSTALL_MANIFEST.json`n$LauncherHash  bin/Windows/profile/Default/FOA-SDK.exe`n"
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

    $InvalidChecksums = "$ManifestHash  INSTALL_MANIFEST.json`n$('0' * 64)  bin/Windows/profile/Default/FOA-SDK.exe`n"
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
    Ensure-Directory $EvidenceRoot
    $Result | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath $SummaryPath -Encoding utf8
    Write-Host "Windows installer smoke summary: $SummaryPath"
}
