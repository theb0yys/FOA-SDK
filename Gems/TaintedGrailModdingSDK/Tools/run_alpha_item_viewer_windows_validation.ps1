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

        [Parameter(ValueFromRemainingArguments = $true)]
        [string[]] $Arguments
    )

    Write-Host "> $FilePath $($Arguments -join ' ')"
    & $FilePath @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "Command failed with exit code $LASTEXITCODE: $FilePath $($Arguments -join ' ')"
    }
}

function ConvertTo-ProcessArgument {
    param([Parameter(Mandatory = $true)][string] $Value)

    if ($Value -notmatch '[\s"]') {
        return $Value
    }
    return '"' + $Value.Replace('"', '\"') + '"'
}

$repoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..\..\..")).Path
$engineRootResolved = (Resolve-Path -LiteralPath $EngineRoot).Path
$projectRoot = Join-Path $repoRoot "TaintedGrailModdingEditor"
$lockPath = Join-Path $repoRoot "o3de.lock.json"
$smokeScript = Join-Path $repoRoot "Gems\TaintedGrailModdingSDK\Tools\editor_tests\alpha_item_viewer_live_smoke.py"
$toolRoot = Join-Path $repoRoot "Gems\TaintedGrailModdingSDK\Tools"
$testRoot = Join-Path $toolRoot "tests"

foreach ($requiredPath in @($projectRoot, $lockPath, $smokeScript, $toolRoot, $testRoot)) {
    if (-not (Test-Path -LiteralPath $requiredPath)) {
        throw "Required Alpha item viewer path is missing: $requiredPath"
    }
}

$lock = Get-Content -LiteralPath $lockPath -Raw -Encoding UTF8 | ConvertFrom-Json
$expectedO3deCommit = [string] $lock.commit
if ($expectedO3deCommit -notmatch '^[0-9a-f]{40}$') {
    throw "o3de.lock.json does not contain one exact 40-character commit."
}

Invoke-Checked git -C $engineRootResolved rev-parse --is-inside-work-tree
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
                Invoke-Checked python -m unittest discover -s $testRoot -p $pattern -v
            }

            $validators = @(
                "validate_foa_visual_asset_discovery_index.py",
                "validate_foa_thumbnail_artifact_extractor.py",
                "validate_foa_neutral_preview_handoff.py",
                "validate_foa_o3de_preview_conversion.py",
                "validate_foa_o3de_asset_processor_import_proof.py",
                "validate_foa_asset_browser_pane_model.py",
                "validate_foa_asset_browser_pane_ui_render.py",
                "validate_foa_3d_preview_viewport.py",
                "validate_editor_lifecycle.py",
                "validate_foundation.py"
            )
            foreach ($validator in $validators) {
                Invoke-Checked python (Join-Path $toolRoot $validator)
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
        Invoke-Checked cmake @configureArguments
    }

    Invoke-Checked cmake --build $buildRootResolved --config $Configuration --target TaintedGrailModdingSDK.Editor --parallel $Parallel

    if (-not $SkipCompiledTests) {
        Invoke-Checked cmake --build $buildRootResolved --config $Configuration --target TaintedGrailModdingSDK.Catalog.Tests --parallel $Parallel
        Invoke-Checked ctest --test-dir $buildRootResolved -C $Configuration --output-on-failure --no-tests=error -R 'TaintedGrailModdingSDK\.Catalog\.Tests'
    }

    if (-not $SkipEditorSmoke) {
        Invoke-Checked cmake --build $buildRootResolved --config $Configuration --target Editor --parallel $Parallel

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
            Get-Content -LiteralPath $editorLog.FullName -Tail 200
        }
        if ($editorProcess.ExitCode -ne 0) {
            throw "Alpha item viewer Editor smoke failed with exit code $($editorProcess.ExitCode)."
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
