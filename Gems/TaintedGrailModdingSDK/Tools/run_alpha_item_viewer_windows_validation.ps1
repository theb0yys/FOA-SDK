[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string] $EngineRoot,

    [string] $BuildRoot = (Join-Path $env:LOCALAPPDATA "FOA-SDK\Build\AlphaItemViewer"),

    [string] $ThirdPartyRoot = "",

    [ValidateSet("debug", "profile", "release")]
    [string] $Configuration = "profile",

    [ValidateRange(1, 32)]
    [int] $Parallel = 2,

    [ValidateRange(30, 1800)]
    [int] $EditorSmokeTimeoutSeconds = 300,

    [switch] $SkipConfigure,
    [switch] $SkipPythonValidation,
    [switch] $SkipCompiledTests,
    [switch] $SkipEditorSmoke
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$isWindowsVariable = Get-Variable -Name IsWindows -ErrorAction SilentlyContinue
$isWindowsHost = if ($isWindowsVariable) {
    [bool] $isWindowsVariable.Value
}
else {
    $env:OS -eq "Windows_NT"
}
if (-not $isWindowsHost) {
    throw "Alpha item viewer validation is Windows-only."
}

function Invoke-Checked {
    param(
        [Parameter(Mandatory = $true)]
        [string] $FilePath,

        [string[]] $Arguments = @()
    )

    Write-Host "> $FilePath $($Arguments -join ' ')"
    & $FilePath @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "Command failed with exit code $($LASTEXITCODE): $FilePath $($Arguments -join ' ')"
    }
}

function ConvertTo-ProcessArgument {
    param([Parameter(Mandatory = $true)][string] $Value)

    if ($Value -notmatch '[\s"]') {
        return $Value
    }
    return '"' + $Value.Replace('"', '\"') + '"'
}

function Write-SmokeWorkspace {
    param(
        [Parameter(Mandatory = $true)][string] $WorkspaceRoot,
        [Parameter(Mandatory = $true)][string] $GameRoot
    )

    $managedRoot = Join-Path $GameRoot "Fall of Avalon_Data\Managed"
    $pluginRoot = Join-Path $GameRoot "BepInEx\plugins"
    $diagnosticsRoot = Join-Path $WorkspaceRoot "Diagnostics"
    $extractedRoot = Join-Path $WorkspaceRoot "Extracted"
    foreach ($directory in @(
        $WorkspaceRoot,
        (Join-Path $WorkspaceRoot "Build"),
        (Join-Path $WorkspaceRoot "Staging"),
        (Join-Path $WorkspaceRoot "Deployment"),
        $diagnosticsRoot,
        $extractedRoot,
        $managedRoot,
        $pluginRoot
    )) {
        New-Item -ItemType Directory -Path $directory -Force | Out-Null
    }

    Set-Content -LiteralPath (Join-Path $GameRoot "UnityPlayer.dll") -Value "fixture" -Encoding ASCII
    Set-Content -LiteralPath (Join-Path $managedRoot "Assembly-CSharp.dll") -Value "fixture" -Encoding ASCII

    $workspace = [ordered]@{
        SchemaVersion = 1
        WorkspaceId = "fixture.workspace"
        DisplayName = "Item Viewer Refresh Fixture"
        RootPath = $WorkspaceRoot
        OutputPath = (Join-Path $WorkspaceRoot "Build")
        StagingPath = (Join-Path $WorkspaceRoot "Staging")
        DeploymentPath = (Join-Path $WorkspaceRoot "Deployment")
        ActiveGameProfileId = "foa.mono.fixture"
        GameProfiles = @(
            [ordered]@{
                ProfileId = "foa.mono.fixture"
                DisplayName = "Fixture"
                InstallPath = $GameRoot
                GameVersion = "1.23.401"
                Branch = "mono"
                RuntimeTarget = "Mono"
                UnityVersion = "6000.0.64f1"
                BepInExVersion = "5.4.23.3"
                ManagedAssembliesPath = $managedRoot
                PluginPath = $pluginRoot
                DiagnosticsPath = $diagnosticsRoot
                ExtractedDataPath = $extractedRoot
                DlcScopes = @("base-game")
            }
        )
    }

    $workspacePath = Join-Path $WorkspaceRoot "foa-sdk.tgworkspace.json"
    $workspace | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath $workspacePath -Encoding UTF8
    return $workspacePath
}

$repoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..\..\..")).Path
$engineRootResolved = (Resolve-Path -LiteralPath $EngineRoot).Path
$projectRoot = Join-Path $repoRoot "TaintedGrailModdingEditor"
$lockPath = Join-Path $repoRoot "o3de.lock.json"
$smokeScript = Join-Path $repoRoot "Gems\TaintedGrailModdingSDK\Tools\editor_tests\alpha_item_viewer_live_smoke.py"
$toolRoot = Join-Path $repoRoot "Gems\TaintedGrailModdingSDK\Tools"
$testRoot = Join-Path $toolRoot "tests"
$paneModelTool = Join-Path $toolRoot "foa_asset_browser_pane_model.py"

foreach ($requiredPath in @($projectRoot, $lockPath, $smokeScript, $toolRoot, $testRoot, $paneModelTool)) {
    if (-not (Test-Path -LiteralPath $requiredPath)) {
        throw "Required Alpha item viewer path is missing: $requiredPath"
    }
}

$lock = Get-Content -LiteralPath $lockPath -Raw -Encoding UTF8 | ConvertFrom-Json
$expectedO3deCommit = [string] $lock.commit
if ($expectedO3deCommit -notmatch '^[0-9a-f]{40}$') {
    throw "o3de.lock.json does not contain one exact 40-character commit."
}

Invoke-Checked -FilePath git -Arguments @("-C", $engineRootResolved, "rev-parse", "--is-inside-work-tree")
$actualO3deCommit = (& git -C $engineRootResolved rev-parse HEAD).Trim()
if ($LASTEXITCODE -ne 0) {
    throw "Unable to resolve the O3DE checkout commit."
}
if ($actualO3deCommit -cne $expectedO3deCommit) {
    throw "O3DE checkout mismatch. Expected $expectedO3deCommit but found $actualO3deCommit."
}

$buildRootResolved = [System.IO.Path]::GetFullPath($BuildRoot)
New-Item -ItemType Directory -Path $buildRootResolved -Force | Out-Null

Push-Location $repoRoot
try {
    if (-not $SkipPythonValidation) {
        $previousPythonPath = $env:PYTHONPATH
        try {
            $env:PYTHONPATH = if ([string]::IsNullOrWhiteSpace($previousPythonPath)) {
                $toolRoot
            }
            else {
                "$toolRoot;$previousPythonPath"
            }

            $unitPatterns = @(
                "test_validate_item_viewer_working_lifecycle.py",
                "test_foa_visual_asset_discovery_index.py",
                "test_foa_thumbnail_artifact_extractor.py",
                "test_foa_neutral_preview_handoff.py",
                "test_foa_o3de_preview_conversion.py",
                "test_foa_o3de_asset_processor_import_proof.py",
                "test_foa_asset_browser_pane_model.py",
                "test_foa_asset_browser_pane_ui_render.py",
                "test_foa_3d_preview_viewport.py"
            )
            foreach ($pattern in $unitPatterns) {
                Invoke-Checked -FilePath python -Arguments @("-m", "unittest", "discover", "-s", $testRoot, "-p", $pattern, "-v")
            }

            $validators = @(
                "validate_item_viewer_working_lifecycle.py",
                "validate_foa_visual_asset_discovery_index.py",
                "validate_foa_thumbnail_artifact_extractor.py",
                "validate_foa_neutral_preview_handoff.py",
                "validate_foa_o3de_preview_conversion.py",
                "validate_foa_o3de_asset_processor_import_proof.py",
                "validate_foa_asset_browser_pane_model.py",
                "validate_foa_asset_browser_pane_ui_render.py",
                "validate_foa_3d_preview_viewport.py",
                "validate_editor_lifecycle.py"
            )
            foreach ($validator in $validators) {
                Invoke-Checked -FilePath python -Arguments @((Join-Path $toolRoot $validator))
            }
        }
        finally {
            $env:PYTHONPATH = $previousPythonPath
        }
    }

    if (-not $SkipConfigure) {
        $configureArguments = @(
            "--preset", "windows-vs-unity",
            "-S", $engineRootResolved,
            "-B", $buildRootResolved,
            "-A", "x64",
            "-DLY_PROJECTS=$projectRoot",
            "-DLY_DISABLE_TEST_MODULES=OFF",
            "-DO3DE_FETCHCONTENT_FORCE_GIT=ON"
        )
        if (-not [string]::IsNullOrWhiteSpace($ThirdPartyRoot)) {
            $thirdPartyResolved = [System.IO.Path]::GetFullPath($ThirdPartyRoot)
            New-Item -ItemType Directory -Path $thirdPartyResolved -Force | Out-Null
            $configureArguments += "-DLY_3RDPARTY_PATH=$thirdPartyResolved"
        }
        Invoke-Checked -FilePath cmake -Arguments $configureArguments
    }

    Invoke-Checked -FilePath cmake -Arguments @("--build", $buildRootResolved, "--config", $Configuration, "--target", "TaintedGrailModdingSDK.Editor", "--parallel", [string] $Parallel)

    if (-not $SkipCompiledTests) {
        Invoke-Checked -FilePath cmake -Arguments @("--build", $buildRootResolved, "--config", $Configuration, "--target", "TaintedGrailModdingSDK.Catalog.Tests", "--parallel", [string] $Parallel)
        Invoke-Checked -FilePath ctest -Arguments @("--test-dir", $buildRootResolved, "-C", $Configuration, "--output-on-failure", "--no-tests=error", "-R", "TaintedGrailModdingSDK\.Catalog\.Tests")
    }

    if (-not $SkipEditorSmoke) {
        Invoke-Checked -FilePath cmake -Arguments @("--build", $buildRootResolved, "--config", $Configuration, "--target", "Editor", "--parallel", [string] $Parallel)

        $configurationDirectory = $Configuration.ToLowerInvariant()
        $editorCandidates = @(
            Get-ChildItem -LiteralPath $buildRootResolved -Filter Editor.exe -File -Recurse -ErrorAction Stop |
                Where-Object {
                    $_.FullName -match "[\\/]bin[\\/]$([Regex]::Escape($configurationDirectory))[\\/]Editor\.exe$"
                }
        )
        if ($editorCandidates.Count -ne 1) {
            $found = ($editorCandidates | ForEach-Object FullName) -join "; "
            throw "Expected exactly one $Configuration Editor.exe; found $($editorCandidates.Count): $found"
        }

        # Isolate all automatic workspace and user-state discovery from the runner's
        # real LocalAppData, then seed only the exact-profile import proof required
        # to prove that Refresh Assets regenerates the pane model inside the Editor.
        $originalLocalAppData = $env:LOCALAPPDATA
        $validationLocalAppData = Join-Path $buildRootResolved "item-viewer-smoke-localappdata"
        $seedRoot = Join-Path $buildRootResolved "item-viewer-refresh-seed"
        Remove-Item -LiteralPath $validationLocalAppData -Recurse -Force -ErrorAction SilentlyContinue
        Remove-Item -LiteralPath $seedRoot -Recurse -Force -ErrorAction SilentlyContinue
        New-Item -ItemType Directory -Path $validationLocalAppData -Force | Out-Null

        Invoke-Checked -FilePath python -Arguments @($paneModelTool, "fixture", "--output", $seedRoot, "--replace")
        $proofCandidates = @(
            Get-ChildItem -LiteralPath $seedRoot -Filter "foa-o3de-asset-processor-import-proof.json" -File -Recurse -ErrorAction Stop
        )
        if ($proofCandidates.Count -ne 1) {
            throw "Expected exactly one synthetic Item Viewer import proof; found $($proofCandidates.Count)."
        }
        $proofDocument = Get-Content -LiteralPath $proofCandidates[0].FullName -Raw -Encoding UTF8 | ConvertFrom-Json
        $expectedProofId = [string] $proofDocument.ImportProofId
        if ([string]::IsNullOrWhiteSpace($expectedProofId)) {
            throw "Synthetic Item Viewer import proof is missing ImportProofId."
        }

        $autoWorkspaceRoot = Join-Path $validationLocalAppData "FOA-SDK\Workspace"
        $fakeGameRoot = Join-Path $validationLocalAppData "FOA-SDK\FakeGame"
        $workspacePath = Write-SmokeWorkspace -WorkspaceRoot $autoWorkspaceRoot -GameRoot $fakeGameRoot
        $extractedRoot = Join-Path $autoWorkspaceRoot "Extracted"
        $o3deRoot = Join-Path $extractedRoot "PreviewArtifacts\O3DE"
        New-Item -ItemType Directory -Path $o3deRoot -Force | Out-Null
        $seedO3deRoot = Join-Path $seedRoot "workspace\Extracted\PreviewArtifacts\O3DE"
        Copy-Item -LiteralPath (Join-Path $seedO3deRoot "*") -Destination $o3deRoot -Recurse -Force
        $assetBrowserRoot = Join-Path $extractedRoot "PreviewArtifacts\AssetBrowser"
        Remove-Item -LiteralPath $assetBrowserRoot -Recurse -Force -ErrorAction SilentlyContinue

        $env:LOCALAPPDATA = $validationLocalAppData
        $env:FOA_SDK_ITEM_VIEWER_REFRESH_WORKSPACE = $workspacePath
        $env:FOA_SDK_ITEM_VIEWER_REFRESH_MODEL_ROOT = $assetBrowserRoot
        $env:FOA_SDK_ITEM_VIEWER_REFRESH_EXPECTED_PROOF_ID = $expectedProofId

        $editorArguments = @(
            "--project-path=$projectRoot",
            "--runpythontest",
            $smokeScript,
            "-rhi=null",
            "-autotest_mode",
            "-skipWelcomeScreenDialog"
        )
        $argumentLine = ($editorArguments | ForEach-Object { ConvertTo-ProcessArgument $_ }) -join " "
        Write-Host "> $($editorCandidates[0].FullName) $argumentLine"
        try {
            $editorProcess = Start-Process -FilePath $editorCandidates[0].FullName `
                -ArgumentList $argumentLine `
                -WorkingDirectory $projectRoot `
                -PassThru
            try {
                Wait-Process -Id $editorProcess.Id -Timeout $EditorSmokeTimeoutSeconds -ErrorAction Stop
            }
            catch {
                Stop-Process -Id $editorProcess.Id -Force -ErrorAction SilentlyContinue
                throw "Alpha item viewer Editor smoke exceeded $EditorSmokeTimeoutSeconds seconds."
            }
            $editorProcess.Refresh()

            $editorLog = Get-ChildItem -LiteralPath $projectRoot -Filter Editor.log -File -Recurse -ErrorAction SilentlyContinue |
                Sort-Object LastWriteTime -Descending |
                Select-Object -First 1
            if ($editorLog) {
                Write-Host "--- Editor.log tail ---"
                Get-Content -LiteralPath $editorLog.FullName -Tail 240
            }
            if ($editorProcess.ExitCode -ne 0) {
                throw "Alpha item viewer Editor smoke failed with exit code $($editorProcess.ExitCode)."
            }
        }
        finally {
            $env:LOCALAPPDATA = $originalLocalAppData
            Remove-Item Env:FOA_SDK_ITEM_VIEWER_REFRESH_WORKSPACE -ErrorAction SilentlyContinue
            Remove-Item Env:FOA_SDK_ITEM_VIEWER_REFRESH_MODEL_ROOT -ErrorAction SilentlyContinue
            Remove-Item Env:FOA_SDK_ITEM_VIEWER_REFRESH_EXPECTED_PROOF_ID -ErrorAction SilentlyContinue
        }
    }
}
finally {
    Pop-Location
}

Write-Host "Alpha item viewer Windows validation passed."
Write-Host "Branch source: $repoRoot"
Write-Host "Pinned O3DE: $expectedO3deCommit"
Write-Host "Build root: $buildRootResolved"