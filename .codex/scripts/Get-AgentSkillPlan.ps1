param(
    [string]$Request = "",
    [string[]]$TargetPath = @(),
    [switch]$AsJson
)

$ErrorActionPreference = 'Stop'
# Optional helper: selections are based on the current request and target paths.
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
        if (-not [string]::IsNullOrWhiteSpace($clean)) {
            Add-Unique $normalized $clean
        }
    }
}
$TargetPath = @($normalized)
$combined = (($Request, ($TargetPath -join ' ')) -join ' ').ToLowerInvariant()
$selectionModel = 'optional'
$classification = 'routine'

foreach ($doc in @(
    'AGENTS.md',
    'CURRENT_TASK.md',
    'docs/tainted-grail-sdk/ENGINEERING_PROCESS.md',
    'docs/tainted-grail-sdk/CI_AND_LOCAL_VALIDATION.md'
)) { Add-Unique $docs $doc }

if ($combined -match 'external process|provider execution|deploy\b|deployment\b|save mutation|runtime adapter|launch foa|game launch|signing|publication|release execution|permission boundary|security-sensitive') {
    $classification = 'critical-runtime'
}
elseif ($combined -match 'public api|new subsystem|schema|migration|new dependency|architecture|ownership boundary|build graph|project integration|process|governance|validation policy') {
    $classification = 'significant'
}

$researchNeeded = $combined -match '\bdeep research\b|\bresearch\b|unresolved|unknown consequential|contradict|native identity|proprietary format|third-party compatibility|licen[cs]e fact|protected external|fall of avalon runtime|game runtime fact|security claim'
if ($researchNeeded) {
    foreach ($skill in @(
        '.codex/skills/foa-sdk-research-sentinel/SKILL.md',
        '.codex/skills/foa-sdk-research-authority/SKILL.md'
    )) { Add-Unique $skills $skill }
    Add-Unique $docs '.codex/workflows/foa_research_first_process_stack.md'
    Add-Unique $docs 'docs/protected-files-policy.md'
    Add-Unique $validation 'Resolve the exact unanswered claim through the evidence lane appropriate to that claim before implementation relies on it.'
}

if ($combined -match 'impact|blast radius|owner system|new subsystem|public api|architecture|ownership boundary') {
    Add-Unique $skills '.codex/skills/foa-change-impact-classifier/SKILL.md'
    Add-Unique $validation 'Map affected owners, consumers, contracts, and blast radius where the change is Significant or Critical/Runtime.'
}

if ($combined -match 'evidence pack|receipt|candidate evidence|promotion|independent review') {
    Add-Unique $skills '.codex/skills/foa-evidence-pack-auditor/SKILL.md'
    Add-Unique $docs '.codex/checklists/evidence_pack_template.json'
    Add-Unique $validation 'Audit structured evidence only for the claim and execution layer it records.'
}

if ($combined -match 'contract|schema|manifest|catalog|snapshot|json|xml|serializ|persist|migration|config|dependency|package|interchange|compatibility') {
    Add-Unique $skills '.codex/skills/foa-contract-persistence-compatibility-gates/SKILL.md'
    Add-Unique $validation 'Review compatibility, migration or rejection, producers, and consumers for the affected contract.'
}

if ($combined -match 'test|coverage|harness|ctest|unittest|pytest|missing lane') {
    Add-Unique $skills '.codex/skills/foa-test-gap-enforcer/SKILL.md'
    Add-Unique $docs '.codex/workflows/foa_sdk_test_gates.md'
    Add-Unique $validation 'Map focused tests and missing evidence only for the affected system.'
}
elseif ($combined -match '\.cpp\b|\.h\b|\.py\b|\.cs\b|\.cmake\b|cmakelists|gems[\\/]|plugins[\\/]|installer[\\/]') {
    Add-Unique $validation 'Run the focused owner tests and validation required by the changed surface.'
}

if ($combined -match 'performance|benchmark|hot path|latency|memory|throughput|startup time|build time|frame|tick|render loop|large scan|scaling') {
    Add-Unique $skills '.codex/skills/foa-performance-budget-gates/SKILL.md'
    Add-Unique $docs '.codex/workflows/foa_professional_code_performance_gate.md'
    Add-Unique $validation 'Run a bounded performance review or guard only for the material performance risk identified.'
}

if ($combined -match 'capability execution|capability-execution|runtime adapter|adapter build|build manifest|package assembly|package preview|staging deployment|deployment work order|deploy\b|deployment\b|launch foa|game launch|post-deployment|runtime result|execution result|release assembly|release signing|process supervisor|provider resolution|provider execution|artifact ownership|execution receipt|rollback') {
    Add-Unique $docs 'docs/tainted-grail-sdk/CAPABILITY_EXECUTION_CONTRACT.md'
    Add-Unique $docs '.codex/workflows/foa_capability_execution_contract.md'
    Add-Unique $skills '.codex/skills/foa-contract-persistence-compatibility-gates/SKILL.md'
    Add-Unique $validation 'Apply the accepted capability-execution boundary without treating plans, previews, receipts, or hashes as execution authority.'
}

if ($combined -match 'system intake|owner unclear|new system|new subsystem|new service|new component|new bus') {
    Add-Unique $skills '.codex/skills/foa-sdk-system-intake/SKILL.md'
    Add-Unique $docs 'Relevant owning architecture or design under docs/tainted-grail-sdk/'
}

if ($combined -match 'o3de|taintedgrailmoddingeditor|gem\.json|gems[\\/][^ ]+[\\/]code|plugins[\\/].+[\\/]gem|aztoolsframework|azcore|ebus|assetprocessor') {
    Add-Unique $skills '.codex/skills/foa-o3de-editor-gates/SKILL.md'
    Add-Unique $validation 'Run only the pinned-O3DE configure, build, compiled-test, or Editor evidence applicable to the touched host layer.'
}

if ($combined -match '\bui\b|\bpane\b|\bview\b|\bwidget\b|\bqt\b|\basset\b|\bprefab\b|\btexture\b|\bmaterial\b|\bmesh\b|\bicon\b|\bpresentation\b') {
    Add-Unique $skills '.codex/skills/foa-ui-asset-route-gates/SKILL.md'
    Add-Unique $validation 'Use UI or asset-route evidence only when interaction, rendering, or asset behavior can change.'
}

if ($combined -match 'unity|conversion|externaltoolchain|handoff|bepinex|mono|il2cpp|runtime adapter|game profile') {
    Add-Unique $skills '.codex/skills/foa-unity-bridge-gates/SKILL.md'
    Add-Unique $validation 'Keep conversion, adapter, host, and exact-profile evidence separate.'
}

if ($combined -match 'migration|version|o3de\.lock|release|installer|dependency|save policy|schema version|upgrade') {
    Add-Unique $skills '.codex/skills/foa-migration-release-gates/SKILL.md'
    Add-Unique $validation 'Run migration or release checks only for the affected contract or operation.'
}

if ($combined -match 'github|\.github|pull request|\bpr\b|commit|branch|release|handoff|codeowners|workflow|issue|review') {
    Add-Unique $skills '.codex/skills/foa-pr-release-captain/SKILL.md'
    Add-Unique $docs '.github/PULL_REQUEST_TEMPLATE.md'
    Add-Unique $docs '.github/CODEOWNERS'
}

if ($combined -match 'build graph|cmakelists|\.cmake\b|project integration|package|installer|conversion|adapter build|release assembly|deploy\b|deployment\b|artifact') {
    Add-Unique $docs '.codex/workflows/foa_artifact_deploy_gate.md'
    Add-Unique $validation 'Use the artifact/deployment helper only for outputs or external operations affected by the change.'
}

if ($combined -match 'repository identity|checkout|engine root|build root|setup|clone') {
    Add-Unique $docs 'README.md'
    Add-Unique $docs 'CONTRIBUTING.md'
    Add-Unique $docs 'docs/tainted-grail-sdk/DEVELOPMENT_GUIDE.md'
}

if ($combined -match 'governance|process|decision|branch policy|merge policy') {
    Add-Unique $docs 'GOVERNANCE.md'
    Add-Unique $docs 'DECISIONS.md'
}

if ($combined -match '\.codex|agents\.md|skill|workflow|checklist|process') {
    Add-Unique $validation 'Run .codex/skills/tests/Validate-AgentSkills.ps1 for changed helper-pack semantics.'
}

if ($combined -match 'protected|external data|game file|save|credential|private path|proprietary') {
    Add-Unique $docs 'docs/protected-files-policy.md'
    Add-Unique $warnings 'Inspect the protected-file boundary before any external read or write.'
}

if ($skills.Count -eq 0) {
    Add-Unique $warnings 'Routine work may require no selected skills; use the owning implementation, tests, and validation matrix directly.'
}
Add-Unique $validation 'Use CI_AND_LOCAL_VALIDATION.md and run only evidence applicable to the changed surface.'

$result = [ordered]@{
    request = $Request
    target_paths = $TargetPath
    selection_model = $selectionModel
    suggested_classification = $classification
    skills = @($skills)
    required_docs = @($docs)
    validation = @($validation)
    warnings = @($warnings)
}
if ($AsJson) {
    $result | ConvertTo-Json -Depth 5
    return
}

'Codex helper selection'
'======================'
"Request: $Request"
"Selection model: $selectionModel"
"Suggested classification: $classification"
''
'Target paths:'
if ($TargetPath.Count -eq 0) { '- (not provided)' } else { $TargetPath | ForEach-Object { "- $_" } }
''
'Selected skills:'
if ($skills.Count -eq 0) { '- (none selected)' } else { $skills | ForEach-Object { "- $_" } }
''
'Relevant docs:'
$docs | ForEach-Object { "- $_" }
''
'Suggested validation:'
$validation | ForEach-Object { "- $_" }
if ($warnings.Count -gt 0) {
    ''
    'Warnings:'
    $warnings | ForEach-Object { "- $_" }
}
