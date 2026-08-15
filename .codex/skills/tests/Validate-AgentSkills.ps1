param([switch]$Quiet)

$ErrorActionPreference = 'Stop'
$skillRoot = Resolve-Path (Join-Path $PSScriptRoot '..')
$repoRoot = Resolve-Path (Join-Path $skillRoot '../..')
$failures = New-Object 'System.Collections.Generic.List[string]'

function Fail([string]$Message) {
    [void]$failures.Add($Message)
}

$dirs = Get-ChildItem -LiteralPath $skillRoot -Directory |
    Where-Object { $_.Name -ne 'tests' } |
    Sort-Object Name

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

foreach ($name in $expected) {
    if (-not ($dirs.Name -contains $name)) {
        Fail "Missing skill $name"
    }
}

foreach ($dir in $dirs) {
    $skill = Join-Path $dir.FullName 'SKILL.md'
    $eval = Join-Path $dir.FullName 'evals/evals.json'

    if (-not (Test-Path -LiteralPath $skill)) {
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

    if (-not (Test-Path -LiteralPath $eval)) {
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
    '.codex/checklists/deep_research_brief_template.md',
    '.codex/checklists/evidence_pack_template.json',
    '.codex/checklists/system_test_matrix_template.md',
    '.codex/agents/foa_research_first_agents.md',
    '.codex/scripts/Get-AgentSkillPlan.ps1',
    '.codex/scripts/Get-AgentTestPlan.ps1',
    '.codex/scripts/Get-AgentPerformancePlan.ps1',
    '.codex/scripts/Get-AgentBuildDeployPlan.ps1'
)

foreach ($file in $requiredFiles) {
    if (-not (Test-Path -LiteralPath (Join-Path $repoRoot $file))) {
        Fail "Missing required process file: $file"
    }
}

$allCodexText = (
    Get-ChildItem -LiteralPath (Join-Path $repoRoot '.codex') -Recurse -File |
        Get-Content -Raw
) -join "`n"
foreach ($forbidden in @('Bannerlord', 'TAOM', 'The Waning Realm', 'TaleWorlds')) {
    if ($allCodexText -match [regex]::Escape($forbidden)) {
        Fail "Forbidden source context remains: $forbidden"
    }
}

$skillPlan = & (Join-Path $repoRoot '.codex/scripts/Get-AgentSkillPlan.ps1') `
    -Request 'Change O3DE UI, Unity conversion, runtime adapter, installer, migration, PR and performance gates.' `
    -TargetPath @('Gems/TaintedGrailModdingSDK/Code/Source/Test.cpp', 'Plugins/RuntimeAdapters/Mono/Test.cs', 'Installer/Test.wxs')
$skillPlanText = $skillPlan -join "`n"
foreach ($name in $expected) {
    if ($skillPlanText -notmatch [regex]::Escape($name)) {
        Fail "Preflight did not select $name"
    }
}
foreach ($path in @(
    'docs/tainted-grail-sdk/CAPABILITY_EXECUTION_CONTRACT.md',
    '.codex/workflows/foa_capability_execution_contract.md'
)) {
    if ($skillPlanText -notmatch [regex]::Escape($path)) {
        Fail "Preflight did not select capability-execution authority $path"
    }
}

$capabilityWorkflowPath = Join-Path $repoRoot '.codex/workflows/foa_capability_execution_contract.md'
if (Test-Path -LiteralPath $capabilityWorkflowPath) {
    $capabilityWorkflow = Get-Content -LiteralPath $capabilityWorkflowPath -Raw
    foreach ($phrase in @(
        'Build',
        'Package',
        'Deploy',
        'Launch',
        'Verify',
        'inert V1',
        'Preview And Execute',
        'Deterministic Provider Resolution',
        'Artifact Ownership And Idempotency',
        'Rollback Before Execution',
        'Receipts, Evidence, Assessment, And Promotion'
    )) {
        if ($capabilityWorkflow -notmatch [regex]::Escape($phrase)) {
            Fail "Capability execution workflow missing $phrase"
        }
    }
}

$capabilityContractPath = Join-Path $repoRoot 'docs/tainted-grail-sdk/CAPABILITY_EXECUTION_CONTRACT.md'
if (Test-Path -LiteralPath $capabilityContractPath) {
    $capabilityContract = Get-Content -LiteralPath $capabilityContractPath -Raw
    foreach ($phrase in @(
        'Existing Service Disposition',
        'Canonical Contract Model',
        'Lifecycle States',
        'Deterministic Provider Resolution',
        'Policy Separation',
        'Shared Build -> Package -> Deploy -> Launch -> Verify Spine',
        'Artifact Ownership',
        'Idempotency',
        'Rollback',
        'Migration Batches',
        'Acceptance Gates',
        'Prohibited Shortcuts'
    )) {
        if ($capabilityContract -notmatch [regex]::Escape($phrase)) {
            Fail "Capability execution contract missing $phrase"
        }
    }
}

$testPlan = & (Join-Path $repoRoot '.codex/scripts/Get-AgentTestPlan.ps1') `
    -Request 'Change Foundation UI Unity conversion and runtime adapter.' `
    -TargetPath @('Gems/TaintedGrailModdingSDK', 'Plugins/RuntimeAdapters')
$testPlanText = $testPlan -join "`n"
foreach ($phrase in @('Immediate Codex commands', 'Static package assertions', 'Manual host rows', 'Runtime rows', 'Non-runnable governed rows')) {
    if ($testPlanText -notmatch [regex]::Escape($phrase)) {
        Fail "Test plan missing $phrase"
    }
}

$performancePlan = & (Join-Path $repoRoot '.codex/scripts/Get-AgentPerformancePlan.ps1') `
    -Request 'Scan every asset on editor tick and bind UI.'
$performancePlanText = $performancePlan -join "`n"
foreach ($phrase in @('Performance risk: High', 'Forbidden shortcuts', 'Required hard performance checks')) {
    if ($performancePlanText -notmatch [regex]::Escape($phrase)) {
        Fail "Performance plan missing $phrase"
    }
}

$buildPlan = & (Join-Path $repoRoot '.codex/scripts/Get-AgentBuildDeployPlan.ps1') `
    -Request 'Build O3DE editor Unity conversion installer and runtime adapter.' `
    -TargetPath @('Gems/TaintedGrailModdingSDK', 'Installer', 'Plugins/RuntimeAdapters')
$buildPlanText = $buildPlan -join "`n"
foreach ($phrase in @('Build root', 'Products', 'Required steps')) {
    if ($buildPlanText -notmatch [regex]::Escape($phrase)) {
        Fail "Artifact plan missing $phrase"
    }
}

if ($failures.Count -gt 0) {
    foreach ($failure in $failures) {
        Write-Error $failure
    }
    exit 1
}

if (-not $Quiet) {
    "PASS FOA-SDK agent skill pack validation: $($dirs.Count) skills, process integration, capability-execution contract, preflight helpers, test gates, performance gates, and artifact gates checked."
}
