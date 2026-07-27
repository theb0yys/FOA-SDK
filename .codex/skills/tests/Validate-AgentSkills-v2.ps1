param([switch]$Quiet)

Set-StrictMode -Version 2.0
$ErrorActionPreference = 'Stop'

$skillRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$repoRoot = (Resolve-Path (Join-Path $skillRoot '../..')).Path
$failures = New-Object 'System.Collections.Generic.List[string]'

function Add-Failure([string]$Message) {
    [void]$failures.Add($Message)
}

$expected = @(
    'foa-sdk-research-sentinel',
    'foa-sdk-research-authority',
    'foa-change-impact-classifier',
    'foa-contract-persistence-compatibility-gates',
    'foa-test-gap-enforcer',
    'foa-performance-budget-gates',
    'foa-evidence-pack-auditor',
    'foa-pr-release-captain',
    'foa-sdk-system-intake',
    'foa-o3de-editor-gates',
    'foa-ui-asset-route-gates',
    'foa-unity-bridge-gates',
    'foa-migration-release-gates'
)

$dirs = @(Get-ChildItem -LiteralPath $skillRoot -Directory | Where-Object { $_.Name -ne 'tests' } | Sort-Object Name)

foreach ($name in $expected) {
    if ($dirs.Name -notcontains $name) {
        Add-Failure "Missing skill $name"
    }
}

foreach ($dir in $dirs) {
    $skillPath = Join-Path $dir.FullName 'SKILL.md'
    $evalPath = Join-Path $dir.FullName 'evals/evals.json'

    if (-not (Test-Path -LiteralPath $skillPath -PathType Leaf)) {
        Add-Failure "$($dir.Name): missing SKILL.md"
        continue
    }

    $content = [System.IO.File]::ReadAllText($skillPath)
    if ($content -notmatch '\A---(?:\r\n|\n)') {
        Add-Failure "$($dir.Name): missing YAML frontmatter"
    }

    $escapedName = [regex]::Escape($dir.Name)
    if ($content -notmatch "(?m)^name:\s*$escapedName\s*$") {
        Add-Failure "$($dir.Name): name mismatch"
    }

    foreach ($heading in @('Research', 'Hard Stop', 'Validation', 'Runtime Proof')) {
        $escapedHeading = [regex]::Escape($heading)
        if ($content -notmatch "(?im)^##[ \t]+$escapedHeading[ \t]*\r?$") {
            Add-Failure "$($dir.Name): missing required heading '$heading'"
        }
    }

    if (-not (Test-Path -LiteralPath $evalPath -PathType Leaf)) {
        Add-Failure "$($dir.Name): missing evals"
        continue
    }

    try {
        $json = [System.IO.File]::ReadAllText($evalPath) | ConvertFrom-Json
    }
    catch {
        Add-Failure "$($dir.Name): invalid eval JSON: $($_.Exception.Message)"
        continue
    }

    if ($json.skill_name -ne $dir.Name) {
        Add-Failure "$($dir.Name): eval skill_name mismatch"
    }
    if (@($json.evals).Count -lt 3) {
        Add-Failure "$($dir.Name): fewer than 3 evals"
    }
    foreach ($case in @($json.evals)) {
        if ([string]::IsNullOrWhiteSpace([string]$case.prompt) -or
            [string]::IsNullOrWhiteSpace([string]$case.expected_output) -or
            @($case.assertions).Count -lt 3) {
            Add-Failure "$($dir.Name): incomplete eval $($case.id)"
        }
    }
}

$requiredFiles = @(
    'AGENTS.md','CURRENT_TASK.md','DECISIONS.md','docs/protected-files-policy.md','docs/systems/SYSTEM_INDEX.md',
    '.codex/README.md','.codex/skills/README.md',
    '.codex/workflows/foa_sdk_development_process.md','.codex/workflows/foa_research_first_process_stack.md',
    '.codex/workflows/foa_professional_code_performance_gate.md','.codex/workflows/foa_sdk_test_gates.md',
    '.codex/workflows/foa_artifact_deploy_gate.md','.codex/checklists/deep_review.md',
    '.codex/checklists/review_record_template.md','.codex/checklists/deep_research_brief_template.md',
    '.codex/checklists/evidence_pack_template.json','.codex/checklists/system_test_matrix_template.md',
    '.codex/agents/foa_research_first_agents.md','.codex/scripts/Get-AgentSkillPlan.ps1',
    '.codex/scripts/Get-AgentTestPlan.ps1','.codex/scripts/Get-AgentPerformancePlan.ps1',
    '.codex/scripts/Get-AgentBuildDeployPlan.ps1'
)

foreach ($file in $requiredFiles) {
    if (-not (Test-Path -LiteralPath (Join-Path $repoRoot $file) -PathType Leaf)) {
        Add-Failure "Missing required process file: $file"
    }
}

$codexFiles = @(Get-ChildItem -LiteralPath (Join-Path $repoRoot '.codex') -Recurse -File)
$allCodexText = ($codexFiles | ForEach-Object { [System.IO.File]::ReadAllText($_.FullName) }) -join "`n"
foreach ($forbidden in @('Bannerlord', 'TAOM', 'The Waning Realm', 'TaleWorlds')) {
    if ($allCodexText -match [regex]::Escape($forbidden)) {
        Add-Failure "Forbidden source context remains: $forbidden"
    }
}

try {
    $skillPlan = & (Join-Path $repoRoot '.codex/scripts/Get-AgentSkillPlan.ps1') -Request 'Change O3DE UI, Unity conversion, runtime adapter, installer, migration, PR and performance gates.' -TargetPath @('Gems/TaintedGrailModdingSDK/Code/Source/Test.cpp','Plugins/RuntimeAdapters/Mono/Test.cs','Installer/Test.wxs')
    $skillPlanText = $skillPlan -join "`n"
    foreach ($name in $expected) {
        if ($skillPlanText -notmatch [regex]::Escape($name)) { Add-Failure "Preflight did not select $name" }
    }
}
catch { Add-Failure "Skill-plan execution failed: $($_.Exception.Message)" }

try {
    $testPlanText = (& (Join-Path $repoRoot '.codex/scripts/Get-AgentTestPlan.ps1') -Request 'Change Foundation UI Unity conversion and runtime adapter.' -TargetPath @('Gems/TaintedGrailModdingSDK','Plugins/RuntimeAdapters')) -join "`n"
    foreach ($phrase in @('Immediate Codex commands','Static package assertions','Manual host rows','Runtime rows','Non-runnable governed rows')) {
        if ($testPlanText -notmatch [regex]::Escape($phrase)) { Add-Failure "Test plan missing $phrase" }
    }
}
catch { Add-Failure "Test-plan execution failed: $($_.Exception.Message)" }

try {
    $performancePlanText = (& (Join-Path $repoRoot '.codex/scripts/Get-AgentPerformancePlan.ps1') -Request 'Scan every asset on editor tick and bind UI.') -join "`n"
    foreach ($phrase in @('Performance risk: High','Forbidden shortcuts','Required hard performance checks')) {
        if ($performancePlanText -notmatch [regex]::Escape($phrase)) { Add-Failure "Performance plan missing $phrase" }
    }
}
catch { Add-Failure "Performance-plan execution failed: $($_.Exception.Message)" }

try {
    $buildPlanText = (& (Join-Path $repoRoot '.codex/scripts/Get-AgentBuildDeployPlan.ps1') -Request 'Build O3DE editor Unity conversion installer and runtime adapter.' -TargetPath @('Gems/TaintedGrailModdingSDK','Installer','Plugins/RuntimeAdapters')) -join "`n"
    foreach ($phrase in @('Build root','Products','Required steps')) {
        if ($buildPlanText -notmatch [regex]::Escape($phrase)) { Add-Failure "Artifact plan missing $phrase" }
    }
}
catch { Add-Failure "Build/deploy-plan execution failed: $($_.Exception.Message)" }

if ($failures.Count -gt 0) {
    Write-Host "VALIDATOR_V2_FAILURE_COUNT=$($failures.Count)"
    foreach ($failure in $failures) { Write-Host "::error::$failure" }
    exit 1
}

if (-not $Quiet) {
    Write-Host "VALIDATOR_V2_PASS"
    Write-Host "PASS FOA-SDK agent skill pack validation: $($dirs.Count) skills checked."
}
