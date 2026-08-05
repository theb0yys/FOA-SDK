[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string] $EngineRoot,
    [string] $BuildRoot = (Join-Path $env:LOCALAPPDATA "FOA-SDK\Build\Stage8ActorAppearance"),
    [string] $ThirdPartyRoot = "",
    [ValidateSet("debug", "profile", "release")]
    [string] $Configuration = "profile",
    [ValidateRange(1, 32)]
    [int] $Parallel = 2,
    [ValidateRange(30, 1800)]
    [int] $EditorSmokeTimeoutSeconds = 300,
    [switch] $SkipConfigure,
    [switch] $SkipCompiledTests,
    [switch] $SkipEditorSmoke
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
$isWindowsVariable = Get-Variable -Name IsWindows -ErrorAction SilentlyContinue
$isWindowsHost = if ($isWindowsVariable) { [bool] $isWindowsVariable.Value } else { $env:OS -eq "Windows_NT" }
if (-not $isWindowsHost) { throw "Stage 8 actor appearance validation is Windows-only." }

function Invoke-Checked {
    param([Parameter(Mandatory = $true)][string] $FilePath,
          [Parameter(ValueFromRemainingArguments = $true)][string[]] $Arguments)
    Write-Host "> $FilePath $($Arguments -join ' ')"
    & $FilePath @Arguments
    if ($LASTEXITCODE -ne 0) { throw "Command failed ($LASTEXITCODE): $FilePath $($Arguments -join ' ')" }
}

function ConvertTo-ProcessArgument([string] $Value) {
    if ($Value -notmatch '[\s"]') { return $Value }
    return '"' + $Value.Replace('"', '\"') + '"'
}

$repoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..\..\..")).Path
$engine = (Resolve-Path -LiteralPath $EngineRoot).Path
$project = Join-Path $repoRoot "TaintedGrailModdingEditor"
$lockPath = Join-Path $repoRoot "o3de.lock.json"
$validator = Join-Path $PSScriptRoot "validate_actor_appearance_preview.py"
$smoke = Join-Path $PSScriptRoot "editor_tests\stage8_actor_appearance_smoke.py"
foreach ($path in @($project, $lockPath, $validator, $smoke)) {
    if (-not (Test-Path -LiteralPath $path)) { throw "Required Stage 8 path is missing: $path" }
}

$lock = Get-Content -LiteralPath $lockPath -Raw -Encoding UTF8 | ConvertFrom-Json
$expectedCommit = [string] $lock.commit
if ($expectedCommit -notmatch '^[0-9a-f]{40}$') { throw "o3de.lock.json lacks one exact commit." }
$actualCommit = (& git -C $engine rev-parse HEAD).Trim()
if ($LASTEXITCODE -ne 0 -or $actualCommit -cne $expectedCommit) {
    throw "Pinned O3DE mismatch. Expected $expectedCommit; found $actualCommit."
}

$build = [System.IO.Path]::GetFullPath($BuildRoot)
New-Item -ItemType Directory -Path $build -Force | Out-Null
Push-Location $repoRoot
try {
    Invoke-Checked python $validator --root $repoRoot
    Invoke-Checked python (Join-Path $PSScriptRoot "validate_foundation.py")
    Invoke-Checked python (Join-Path $PSScriptRoot "validate_editor_lifecycle.py")

    if (-not $SkipConfigure) {
        $arguments = @(
            "--preset", "windows-vs-unity", "-S", $engine, "-B", $build, "-A", "x64",
            "-DLY_PROJECTS=$project", "-DLY_DISABLE_TEST_MODULES=OFF", "-DO3DE_FETCHCONTENT_FORCE_GIT=ON"
        )
        if (-not [string]::IsNullOrWhiteSpace($ThirdPartyRoot)) {
            $thirdParty = [System.IO.Path]::GetFullPath($ThirdPartyRoot)
            New-Item -ItemType Directory -Path $thirdParty -Force | Out-Null
            $arguments += "-DLY_3RDPARTY_PATH=$thirdParty"
        }
        Invoke-Checked cmake @arguments
    }

    Invoke-Checked cmake --build $build --config $Configuration --target TaintedGrailModdingSDK.Editor --parallel $Parallel
    if (-not $SkipCompiledTests) {
        Invoke-Checked cmake --build $build --config $Configuration --target TaintedGrailModdingSDK.Catalog.Tests --parallel $Parallel
        Invoke-Checked ctest --test-dir $build -C $Configuration --output-on-failure --no-tests=error -R 'TaintedGrailModdingSDK\.Catalog\.Tests'
    }

    if (-not $SkipEditorSmoke) {
        Invoke-Checked cmake --build $build --config $Configuration --target Editor --parallel $Parallel
        $configDir = $Configuration.ToLowerInvariant()
        $editors = @(Get-ChildItem -LiteralPath $build -Filter Editor.exe -File -Recurse |
            Where-Object { $_.FullName -match "[\\/]bin[\\/]$([Regex]::Escape($configDir))[\\/]Editor\.exe$" })
        if ($editors.Count -ne 1) { throw "Expected exactly one $Configuration Editor.exe; found $($editors.Count)." }
        $editorArgs = @(
            "--project-path=$project", "--runpythontest", $smoke,
            "-rhi=null", "-autotest_mode", "-skipWelcomeScreenDialog"
        )
        $argumentLine = ($editorArgs | ForEach-Object { ConvertTo-ProcessArgument $_ }) -join " "
        $process = Start-Process -FilePath $editors[0].FullName -ArgumentList $argumentLine -WorkingDirectory $project -PassThru
        try { Wait-Process -Id $process.Id -Timeout $EditorSmokeTimeoutSeconds -ErrorAction Stop }
        catch {
            Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
            throw "Stage 8 Editor smoke exceeded $EditorSmokeTimeoutSeconds seconds."
        }
        $process.Refresh()
        if ($process.ExitCode -ne 0) { throw "Stage 8 Editor smoke failed with exit code $($process.ExitCode)." }
    }
}
finally { Pop-Location }

Write-Host "Stage 8 actor appearance Windows validation passed."
Write-Host "Pinned O3DE: $expectedCommit"
Write-Host "Build root: $build"
