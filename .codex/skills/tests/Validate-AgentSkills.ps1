param([switch]$Quiet)
$ErrorActionPreference='Stop'
$skillRoot=Resolve-Path (Join-Path $PSScriptRoot '..')
$repoRoot=Resolve-Path (Join-Path $skillRoot '../..')
$failures=New-Object 'System.Collections.Generic.List[string]'
function Fail([string]$m){[void]$failures.Add($m)}
function Text([string]$p){if(Test-Path -LiteralPath $p){Get-Content -LiteralPath $p -Raw}else{$null}}
$dirs=Get-ChildItem -LiteralPath $skillRoot -Directory|Where-Object{$_.Name-ne'tests'}|Sort-Object Name
$expected=@('foa-sdk-research-sentinel','foa-sdk-research-authority','foa-change-impact-classifier','foa-contract-persistence-compatibility-gates','foa-test-gap-enforcer','foa-performance-budget-gates','foa-evidence-pack-auditor','foa-pr-release-captain','foa-sdk-system-intake','foa-o3de-editor-gates','foa-ui-asset-route-gates','foa-unity-bridge-gates','foa-migration-release-gates')
foreach($name in $expected){if(-not($dirs.Name-contains$name)){Fail "Missing skill $name"}}
foreach($dir in $dirs){
  $skill=Join-Path $dir.FullName 'SKILL.md';$eval=Join-Path $dir.FullName 'evals/evals.json'
  if(-not(Test-Path $skill)){Fail "$($dir.Name): missing SKILL.md";continue}
  $c=Get-Content $skill -Raw
  if(-not$c.StartsWith("---`n")){Fail "$($dir.Name): missing YAML frontmatter"}
  if($c-notmatch "name:\s*$([regex]::Escape($dir.Name))"){Fail "$($dir.Name): name mismatch"}
  foreach($required in @('Research','Hard Stop','Validation','Runtime Proof')){if($c-notmatch$required){Fail "$($dir.Name): missing $required wording"}}
  if(-not(Test-Path $eval)){Fail "$($dir.Name): missing evals";continue}
  try{$j=Get-Content $eval -Raw|ConvertFrom-Json}catch{Fail "$($dir.Name): invalid eval JSON";continue}
  if($j.skill_name-ne$dir.Name){Fail "$($dir.Name): eval skill_name mismatch"}
  if($j.evals.Count-lt3){Fail "$($dir.Name): fewer than 3 evals"}
  foreach($e in $j.evals){if(-not$e.prompt-or-not$e.expected_output-or$e.assertions.Count-lt3){Fail "$($dir.Name): incomplete eval $($e.id)"}}
}
$requiredFiles=@('AGENTS.md','CURRENT_TASK.md','DECISIONS.md','docs/protected-files-policy.md','docs/systems/SYSTEM_INDEX.md','.codex/README.md','.codex/skills/README.md','.codex/workflows/foa_sdk_development_process.md','.codex/workflows/foa_research_first_process_stack.md','.codex/workflows/foa_professional_code_performance_gate.md','.codex/workflows/foa_sdk_test_gates.md','.codex/workflows/foa_artifact_deploy_gate.md','.codex/checklists/deep_review.md','.codex/checklists/review_record_template.md','.codex/checklists/deep_research_brief_template.md','.codex/checklists/evidence_pack_template.json','.codex/checklists/system_test_matrix_template.md','.codex/agents/foa_research_first_agents.md','.codex/scripts/Get-AgentSkillPlan.ps1','.codex/scripts/Get-AgentTestPlan.ps1','.codex/scripts/Get-AgentPerformancePlan.ps1','.codex/scripts/Get-AgentBuildDeployPlan.ps1')
foreach($f in $requiredFiles){if(-not(Test-Path(Join-Path $repoRoot $f))){Fail "Missing required process file: $f"}}
$all=(Get-ChildItem -LiteralPath (Join-Path $repoRoot '.codex') -Recurse -File|Get-Content -Raw)-join"`n"
foreach($forbidden in @('Bannerlord','TAOM','The Waning Realm','TaleWorlds')){if($all-match[regex]::Escape($forbidden)){Fail "Forbidden source context remains: $forbidden"}}
$skillPlan=&(Join-Path $repoRoot '.codex/scripts/Get-AgentSkillPlan.ps1') -Request 'Change O3DE UI, Unity conversion, runtime adapter, installer, migration, PR and performance gates.' -TargetPath @('Gems/TaintedGrailModdingSDK/Code/Source/Test.cpp','Plugins/RuntimeAdapters/Mono/Test.cs','Installer/Test.wxs')
foreach($name in $expected){if(($skillPlan-join"`n")-notmatch[regex]::Escape($name)){Fail "Preflight did not select $name"}}
$testPlan=&(Join-Path $repoRoot '.codex/scripts/Get-AgentTestPlan.ps1') -Request 'Change Foundation UI Unity conversion and runtime adapter.' -TargetPath @('Gems/TaintedGrailModdingSDK','Plugins/RuntimeAdapters')
foreach($phrase in @('Immediate Codex commands','Static package assertions','Manual host rows','Runtime rows','Non-runnable governed rows')){if(($testPlan-join"`n")-notmatch[regex]::Escape($phrase)){Fail "Test plan missing $phrase"}}
$perf=&(Join-Path $repoRoot '.codex/scripts/Get-AgentPerformancePlan.ps1') -Request 'Scan every asset on editor tick and bind UI.'
foreach($phrase in @('Performance risk: High','Forbidden shortcuts','Required hard performance checks')){if(($perf-join"`n")-notmatch[regex]::Escape($phrase)){Fail "Performance plan missing $phrase"}}
$build=&(Join-Path $repoRoot '.codex/scripts/Get-AgentBuildDeployPlan.ps1') -Request 'Build O3DE editor Unity conversion installer and runtime adapter.' -TargetPath @('Gems/TaintedGrailModdingSDK','Installer','Plugins/RuntimeAdapters')
foreach($phrase in @('Build root','Products','Required steps')){if(($build-join"`n")-notmatch[regex]::Escape($phrase)){Fail "Artifact plan missing $phrase"}}
if($failures.Count){$failures|%{Write-Error $_};exit 1}
if(-not$Quiet){"PASS FOA-SDK agent skill pack validation: $($dirs.Count) skills, process integration, preflight helpers, test gates, performance gates, and artifact gates checked."}
