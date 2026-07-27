param(
    [string]$Request = "",
    [string[]]$TargetPath = @(),
    [switch]$AsJson
)

$ErrorActionPreference = 'Stop'
$skills = New-Object 'System.Collections.Generic.List[string]'
$docs = New-Object 'System.Collections.Generic.List[string]'
$validation = New-Object 'System.Collections.Generic.List[string]'
$warnings = New-Object 'System.Collections.Generic.List[string]'
$normalized = New-Object 'System.Collections.Generic.List[string]'

function Add-Unique([System.Collections.Generic.List[string]]$List, [string]$Value) {
    if (-not $List.Contains($Value)) { [void]$List.Add($Value) }
}

foreach ($item in $TargetPath) {
    foreach ($piece in ($item -split '[,;]')) {
        $clean = $piece.Trim().Trim("'").Trim('"')
        if (-not [string]::IsNullOrWhiteSpace($clean)) { Add-Unique $normalized $clean }
    }
}
$TargetPath = @($normalized)
$combined = (($Request, ($TargetPath -join ' ')) -join ' ').ToLowerInvariant()

foreach ($skill in @(
    '.codex/skills/foa-sdk-research-sentinel/SKILL.md',
    '.codex/skills/foa-sdk-research-authority/SKILL.md',
    '.codex/skills/foa-change-impact-classifier/SKILL.md',
    '.codex/skills/foa-evidence-pack-auditor/SKILL.md'
)) { Add-Unique $skills $skill }

foreach ($doc in @(
    'AGENTS.md','README.md','GOVERNANCE.md','CONTRIBUTING.md',
    'docs/protected-files-policy.md','docs/systems/SYSTEM_INDEX.md',
    'CURRENT_TASK.md','DECISIONS.md',
    '.codex/workflows/foa_sdk_development_process.md',
    '.codex/workflows/foa_research_first_process_stack.md',
    '.codex/checklists/deep_review.md',
    '.codex/checklists/evidence_pack_template.json'
)) { Add-Unique $docs $doc }

foreach ($gate in @(
    'Complete pre-edit and post-edit deep review.',
    'Map exact research authority, impact classification, and evidence-pack requirements.',
    'Report skipped gates and uncertainty honestly.'
)) { Add-Unique $validation $gate }

if ($combined -match '\.cpp\b|\.h\b|\.py\b|\.cs\b|\.json\b|\.xml\b|\.cmake\b|cmakelists|gems[\\/]|plugins[\\/]|installer[\\/]|taintedgrailmoddingeditor|tests?|performance|benchmark') {
    Add-Unique $docs '.codex/workflows/foa_professional_code_performance_gate.md'
    Add-Unique $docs '.codex/workflows/foa_sdk_test_gates.md'
    Add-Unique $skills '.codex/skills/foa-test-gap-enforcer/SKILL.md'
    Add-Unique $skills '.codex/skills/foa-performance-budget-gates/SKILL.md'
    Add-Unique $validation 'Run Get-AgentTestPlan.ps1 and Get-AgentPerformancePlan.ps1.'
}

if ($combined -match 'contract|schema|manifest|catalog|snapshot|command|json|xml|serializ|persist|migration|config|dependency|package|interchange|compatibility') {
    Add-Unique $skills '.codex/skills/foa-contract-persistence-compatibility-gates/SKILL.md'
    Add-Unique $validation 'Run compatibility review for public contracts, schemas, persistence, interchange, dependencies, packages, and migrations.'
}

if ($combined -match 'gems[\\/]|plugins[\\/]|taintedgrailmoddingeditor|foundation|extensionapi|system|service|component|bus') {
    Add-Unique $skills '.codex/skills/foa-sdk-system-intake/SKILL.md'
    Add-Unique $docs 'Relevant files under Research/ and docs/tainted-grail-sdk/'
}

if ($combined -match 'o3de|editor|gem|aztoolsframework|azcore|component|ebus|assetprocessor') {
    Add-Unique $skills '.codex/skills/foa-o3de-editor-gates/SKILL.md'
    Add-Unique $validation 'Run the relevant pinned-O3DE configure, build, compiled-test, and Editor gates.'
}

if ($combined -match 'ui|pane|view|widget|qt|asset|prefab|texture|material|mesh|icon|presentation') {
    Add-Unique $skills '.codex/skills/foa-ui-asset-route-gates/SKILL.md'
    Add-Unique $validation 'Run UI/asset route gates and do not claim Editor or runtime proof from static inspection.'
}

if ($combined -match 'unity|conversion|externaltoolchain|handoff|bepinex|mono|il2cpp|runtime adapter|game profile|fall of avalon') {
    Add-Unique $skills '.codex/skills/foa-unity-bridge-gates/SKILL.md'
    Add-Unique $validation 'Run canonical handoff, conversion, adapter, and exact-profile gates required by the touched layer.'
}

if ($combined -match 'migration|version|o3de.lock|release|installer|dependency|save policy|schema version|upgrade') {
    Add-Unique $skills '.codex/skills/foa-migration-release-gates/SKILL.md'
    Add-Unique $validation 'Run migration and release gates required by the touched files.'
}

if ($combined -match 'github|\.github|pull request|\bpr\b|commit|branch|release|handoff|codeowners|workflow|issue|review') {
    Add-Unique $skills '.codex/skills/foa-pr-release-captain/SKILL.md'
    Add-Unique $docs '.github/PULL_REQUEST_TEMPLATE.md'
    Add-Unique $docs '.github/CODEOWNERS'
}

if ($combined -match '\.cpp\b|\.h\b|\.py\b|\.cs\b|\.cmake\b|cmakelists|gems[\\/]|plugins[\\/]|installer[\\/]') {
    Add-Unique $docs '.codex/workflows/foa_artifact_deploy_gate.md'
    Add-Unique $validation 'Run Get-AgentBuildDeployPlan.ps1 for artifact-producing changes.'
}

if ($combined -match '\.codex|agents\.md|skill|workflow|checklist|process') {
    Add-Unique $validation 'Run .codex/skills/tests/Validate-AgentSkills.ps1.'
}

if ($combined -match 'protected|external data|game file|save|credential|private path') {
    Add-Unique $warnings 'Protected-file audit is required before editing.'
}

$result = [ordered]@{
    request = $Request; target_paths = $TargetPath; skills = @($skills);
    required_docs = @($docs); validation = @($validation); warnings = @($warnings)
}
if ($AsJson) { $result | ConvertTo-Json -Depth 5; return }

'Codex skill preflight'; '===================='; "Request: $Request"; ''
'Target paths:'; if ($TargetPath.Count -eq 0) { '- (not provided)' } else { $TargetPath | ForEach-Object { "- $_" } }
''; 'Required skills:'; $skills | ForEach-Object { "- $_" }
''; 'Required docs:'; $docs | ForEach-Object { "- $_" }
''; 'Validation:'; $validation | ForEach-Object { "- $_" }
if ($warnings.Count -gt 0) { ''; 'Warnings:'; $warnings | ForEach-Object { "- $_" } }
