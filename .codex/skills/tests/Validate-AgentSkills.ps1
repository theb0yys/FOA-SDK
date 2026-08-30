param([switch]$Quiet)

$ErrorActionPreference = 'Stop'
$skillRoot = Resolve-Path (Join-Path $PSScriptRoot '..')
$repoRoot = Resolve-Path (Join-Path $skillRoot '../..')
$failures = New-Object 'System.Collections.Generic.List[string]'

function Fail([string]$Message) {
    [void]$failures.Add($Message)
}

function Require-Text([string]$RelativePath, [string[]]$Fragments) {
    $path = Join-Path $repoRoot $RelativePath
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        Fail "Missing required process file: $RelativePath"
        return
    }
    $content = Get-Content -LiteralPath $path -Raw
    foreach ($fragment in $Fragments) {
        if ($content -notmatch [regex]::Escape($fragment)) {
            Fail "$RelativePath missing required text: $fragment"
        }
    }
}

function Forbid-Text([string]$RelativePath, [string[]]$Fragments) {
    $path = Join-Path $repoRoot $RelativePath
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        Fail "Missing required process file: $RelativePath"
        return
    }
    $content = Get-Content -LiteralPath $path -Raw
    foreach ($fragment in $Fragments) {
        if ($content -match [regex]::Escape($fragment)) {
            Fail "$RelativePath retains prohibited text: $fragment"
        }
    }
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

$dirs = Get-ChildItem -LiteralPath $skillRoot -Directory |
    Where-Object { $_.Name -ne 'tests' } |
    Sort-Object Name

foreach ($name in $expected) {
    if (-not ($dirs.Name -contains $name)) {
        Fail "Missing skill $name"
    }
}

foreach ($dir in $dirs) {
    $skill = Join-Path $dir.FullName 'SKILL.md'
    $eval = Join-Path $dir.FullName 'evals/evals.json'

    if (-not (Test-Path -LiteralPath $skill -PathType Leaf)) {
        Fail "$($dir.Name): missing SKILL.md"
        continue
    }

    $content = Get-Content -LiteralPath $skill -Raw
    if ($content -notmatch '^---\r?\n') {
        Fail "$($dir.Name): missing YAML frontmatter"
    }
    $namePattern = "(?m)^name:\s*$([regex]::Escape($dir.Name))\s*$"
    if ($content -notmatch $namePattern) {
        Fail "$($dir.Name): name mismatch"
    }

    if (-not (Test-Path -LiteralPath $eval -PathType Leaf)) {
        Fail "$($dir.Name): missing evals"
        continue
    }

    try {
        $json = Get-Content -LiteralPath $eval -Raw | ConvertFrom-Json
    }
    catch {
        Fail "$($dir.Name): invalid eval JSON: $($_.Exception.Message)"
        continue
    }

    if ($json.skill_name -ne $dir.Name) {
        Fail "$($dir.Name): eval skill_name mismatch"
    }
    if ($json.evals.Count -lt 3) {
        Fail "$($dir.Name): fewer than 3 evals"
    }
    foreach ($case in $json.evals) {
        if (-not $case.prompt -or -not $case.expected_output -or $case.assertions.Count -lt 3) {
            Fail "$($dir.Name): incomplete eval $($case.id)"
        }
    }
}

$requiredFiles = @(
    'AGENTS.md',
    'CURRENT_TASK.md',
    'DECISIONS.md',
    'docs/protected-files-policy.md',
    'docs/systems/SYSTEM_INDEX.md',
    'docs/tainted-grail-sdk/ENGINEERING_PROCESS.md',
    'docs/tainted-grail-sdk/CI_AND_LOCAL_VALIDATION.md',
    'docs/tainted-grail-sdk/CAPABILITY_EXECUTION_CONTRACT.md',
    '.codex/README.md',
    '.codex/skills/README.md',
    '.codex/workflows/foa_sdk_development_process.md',
    '.codex/workflows/foa_research_first_process_stack.md',
    '.codex/workflows/foa_capability_execution_contract.md',
    '.codex/workflows/foa_professional_code_performance_gate.md',
    '.codex/workflows/foa_sdk_test_gates.md',
    '.codex/workflows/foa_artifact_deploy_gate.md',
    '.codex/checklists/deep_review.md',
    '.codex/checklists/review_record_template.md',
    '.codex/checklists/system_test_matrix_template.md',
    '.codex/checklists/deep_research_brief_template.md',
    '.codex/checklists/evidence_pack_template.json',
    '.codex/scripts/Get-AgentSkillPlan.ps1',
    '.codex/scripts/Get-AgentTestPlan.ps1',
    '.codex/scripts/Get-AgentPerformancePlan.ps1',
    '.codex/scripts/Get-AgentBuildDeployPlan.ps1'
)
foreach ($file in $requiredFiles) {
    if (-not (Test-Path -LiteralPath (Join-Path $repoRoot $file) -PathType Leaf)) {
        Fail "Missing required process file: $file"
    }
}

Require-Text 'AGENTS.md' @(
    'FOA-SDK Agent Execution Policy',
    'Research is a tool, not a universal precondition.',
    'Routine implementation inside accepted architecture does not require'
)
Require-Text 'docs/tainted-grail-sdk/ENGINEERING_PROCESS.md' @(
    'single engineering workflow',
    '### Routine',
    '### Significant',
    '### Critical/Runtime'
)
Require-Text '.codex/README.md' @(
    'not a universal pre-edit gate',
    'Routine implementation inside accepted architecture does not require'
)
Require-Text '.codex/skills/README.md' @(
    'focused helpers selected by the current task',
    'optional planning helper'
)
Require-Text '.codex/skills/foa-sdk-research-sentinel/SKILL.md' @(
    'Do not use as a universal gate',
    'The task needs research escalation'
)
Require-Text '.codex/workflows/foa_research_first_process_stack.md' @(
    'This workflow is **conditional**',
    'Do not invoke this workflow merely because'
)
Require-Text '.codex/workflows/foa_sdk_development_process.md' @(
    'The public engineering workflow is',
    'Do not automatically turn a Routine change'
)
Require-Text '.codex/scripts/Get-AgentSkillPlan.ps1' @(
    'Optional helper: selections are based on the current request and target paths.',
    'Routine work may require no selected skills',
    'Codex helper selection',
    'Suggested validation:'
)
Forbid-Text '.codex/scripts/Get-AgentSkillPlan.ps1' @(
    'Complete pre-edit and post-edit deep review.',
    'Map exact research authority, impact classification, and evidence-pack requirements.',
    'Required skills:',
    'Required docs:'
)
Require-Text '.codex/scripts/Get-AgentTestPlan.ps1' @(
    'Optional helper: map test evidence only when ownership or applicability is unclear.',
    '--keep-going --static-only --skip-source-policy',
    'This helper may be `NOT_APPLICABLE`',
    'Suggested tests:',
    'Suggested commands:'
)
Forbid-Text '.codex/scripts/Get-AgentTestPlan.ps1' @(
    'Immediate Codex commands:',
    'Required tests:'
)
Require-Text '.codex/checklists/deep_review.md' @(
    'optional checklist',
    'Do not use it as a universal pre-edit or post-edit gate',
    'Research only when consequential external facts are unresolved',
    'Do not invent or execute a follow-on task'
)
Forbid-Text '.codex/checklists/deep_review.md' @(
    'before and after every agent edit',
    'universal and narrower skills loaded',
    'next researched stop/process'
)
Require-Text '.codex/checklists/review_record_template.md' @(
    'Complete only the sections applicable to the current change.',
    'Research Escalation — Only If Triggered',
    'Maintainer-only transition remaining'
)
Forbid-Text '.codex/checklists/review_record_template.md' @(
    'Next researched stop/process'
)
Require-Text '.codex/checklists/system_test_matrix_template.md' @(
    'Select evidence by changed surface and risk.',
    'Applicable Evidence Matrix',
    'Explicitly `NOT_APPLICABLE` lanes'
)
Forbid-Text '.codex/checklists/system_test_matrix_template.md' @(
    'Full governed pack required'
)

# Check the governing process authorities for imported project context. Optional
# examples, evaluation prompts, and specialist records are not authority and are
# intentionally excluded from this semantic check.
$coreContextFiles = @(
    '.codex/README.md',
    '.codex/agents/foa_research_first_agents.md',
    '.codex/skills/README.md',
    '.codex/skills/foa-sdk-research-sentinel/SKILL.md',
    '.codex/workflows/foa_research_first_process_stack.md',
    '.codex/workflows/foa_sdk_development_process.md',
    '.codex/scripts/Get-AgentSkillPlan.ps1',
    '.codex/checklists/deep_review.md',
    '.codex/checklists/review_record_template.md',
    '.codex/checklists/system_test_matrix_template.md'
)
$coreContextText = ($coreContextFiles | ForEach-Object {
    Get-Content -LiteralPath (Join-Path $repoRoot $_) -Raw
}) -join "`n"
foreach ($forbidden in @('Bannerlord', 'TAOM', 'The Waning Realm', 'TaleWorlds')) {
    if ($coreContextText -match [regex]::Escape($forbidden)) {
        Fail "Forbidden inherited source context remains in core process authority: $forbidden"
    }
}

if ($failures.Count -gt 0) {
    foreach ($failure in $failures) {
        Write-Error $failure
    }
    exit 1
}

if (-not $Quiet) {
    "PASS FOA-SDK agent helper pack validation: $($dirs.Count) focused skills and progressive-process integration checked."
}
