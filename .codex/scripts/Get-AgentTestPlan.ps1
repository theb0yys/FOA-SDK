param(
    [string]$Request = "",
    [string[]]$TargetPath = @(),
    [switch]$ListSystems,
    [switch]$AsJson
)

$ErrorActionPreference = 'Stop'
$catalog = @(
    @{ key='foundation-services'; path='Gems/TaintedGrailModdingSDK'; test='python Gems/TaintedGrailModdingSDK/Tools/validate_foundation.py' },
    @{ key='external-toolchain'; path='Gems/ExternalToolchain'; test='python Gems/TaintedGrailModdingSDK/Tools/run_local_validation.py' },
    @{ key='extension-api'; path='Gems/TaintedGrailModdingSDK'; test='python Gems/TaintedGrailModdingSDK/Tools/validate_foundation.py' },
    @{ key='plugins'; path='Plugins'; test='python Gems/TaintedGrailModdingSDK/Tools/run_local_validation.py' },
    @{ key='installer'; path='Installer'; test='python Gems/TaintedGrailModdingSDK/Tools/run_local_validation.py' },
    @{ key='editor-project'; path='TaintedGrailModdingEditor'; test='python Gems/TaintedGrailModdingSDK/Tools/developer_preview.py validate' },
    @{ key='unity-bridge'; path='Plugins/RuntimeAdapters'; test='python Gems/TaintedGrailModdingSDK/Tools/run_local_validation.py' },
    @{ key='test-harness'; path='Gems/TaintedGrailModdingSDK/Tests'; test='python Gems/TaintedGrailModdingSDK/Tools/run_local_validation.py' }
)

if ($ListSystems) {
    if ($AsJson) { $catalog | ConvertTo-Json -Depth 4; return }
    'Codex system test catalog'; '========================='; $catalog | ForEach-Object { "- key=$($_.key); path=$($_.path); immediate=$($_.test)" }; return
}

$combined = (($Request, ($TargetPath -join ' ')) -join ' ').ToLowerInvariant()
$systems = New-Object 'System.Collections.Generic.List[string]'
$surfaces = New-Object 'System.Collections.Generic.List[string]'
$tests = New-Object 'System.Collections.Generic.List[string]'
$commands = New-Object 'System.Collections.Generic.List[string]'
$static = New-Object 'System.Collections.Generic.List[string]'
$manual = New-Object 'System.Collections.Generic.List[string]'
$runtime = New-Object 'System.Collections.Generic.List[string]'
$nonRunnable = New-Object 'System.Collections.Generic.List[string]'
$warnings = New-Object 'System.Collections.Generic.List[string]'
function Add-U($l,$v){if(-not $l.Contains($v)){[void]$l.Add($v)}}

foreach($entry in $catalog){if($combined -match [regex]::Escape($entry.key) -or $combined -match [regex]::Escape($entry.path.ToLowerInvariant())){Add-U $systems $entry.key; Add-U $commands $entry.test}}
if($combined -match 'gems[\\/]|foundation|service|catalog|schema|contract'){Add-U $surfaces 'Foundation or owner core';Add-U $tests 'Owner tests: owned truth, forbidden domain, contracts, malformed input, lifecycle, persistence, and degraded dependencies.'}
if($combined -match 'plugins[\\/]|plugin\.json|extensionapi'){Add-U $surfaces 'Plug-in package';Add-U $tests 'Plug-in tests: manifest schema, deterministic registration, capabilities, provenance, licence, compatibility, and no implicit authority.'}
if($combined -match 'ui|pane|widget|qt|view'){Add-U $surfaces 'Editor UI route';Add-U $tests 'UI tests: binding, command forwarding, no domain truth ownership, copy hygiene, and route behaviour.'}
if($combined -match 'unity|conversion|externaltoolchain|handoff'){Add-U $surfaces 'External toolchain or Unity conversion';Add-U $tests 'Toolchain tests: deterministic plans, bounded process execution, canonical handoff, candidate evidence, and no automatic promotion.';Add-U $manual 'Run the reviewed Unity batch conversion lane when the exact conversion project is available.'}
if($combined -match 'installer|msi|wix|package'){Add-U $surfaces 'Installer or package';Add-U $tests 'Installer tests: deterministic payload, repair/upgrade/uninstall, rollback, signatures as evidence only, and no runtime authority.';Add-U $manual 'Run the host-heavy installer workflow and preserve its artifact receipts.'}
if($combined -match 'bepinex|mono|il2cpp|runtime adapter'){Add-U $surfaces 'Runtime adapter';Add-U $tests 'Adapter tests: exact profile, contract conformance, fail-closed detection, packaging, and no unsupported mutation.';Add-U $runtime 'Execute the lawful exact-install adapter proof lane with operation, expected/observed result, diagnostics, and pass/fail evidence.'}
if($combined -match 'o3de|editor|assetprocessor|taintedgrailmoddingeditor'){Add-U $surfaces 'O3DE host';Add-U $commands 'python Gems/TaintedGrailModdingSDK/Tools/developer_preview.py validate';Add-U $manual 'Open the pinned O3DE Editor and perform the documented acceptance lane when required.'}
if($combined -match 'schema|manifest|json|xml|interchange|package'){Add-U $static 'Validate schema, canonical serialization, stable IDs, package layout, and malformed-input rejection.'}
if($surfaces.Count -eq 0){Add-U $surfaces 'Unclassified';Add-U $warnings 'No specific test surface detected. Identify the owner before editing.'}
if($commands.Count -eq 0 -and $combined -match '\.cpp|\.h|\.py|\.cs|\.json|\.xml|gems[\\/]|plugins[\\/]|installer[\\/]'){Add-U $commands 'powershell -ExecutionPolicy Bypass -File .codex/scripts/Get-AgentTestPlan.ps1 -ListSystems';Add-U $warnings 'No immediate owner command set was produced. Identify the system and rerun.'}
Add-U $nonRunnable 'Exact Fall of Avalon runtime rows are non-runnable without a lawful local installation and matching adapter profile; local build evidence does not prove them.'

$result=[ordered]@{request=$Request;target_paths=$TargetPath;changed_systems=@($systems);surfaces=@($surfaces);required_tests=@($tests);immediate_codex_commands=@($commands);static_package_assertions=@($static);manual_host_rows=@($manual);runtime_rows=@($runtime);non_runnable_governed_rows=@($nonRunnable);warnings=@($warnings)}
if($AsJson){$result|ConvertTo-Json -Depth 5;return}
'Codex test preflight';'===================';"Request: $Request";'';'Changed systems:';if($systems.Count){$systems|%{"- $_"}}else{'- (not parsed)'};'';'Changed surfaces:';$surfaces|%{"- $_"};'';'Required tests:';$tests|%{"- $_"};'';'Immediate Codex commands:';if($commands.Count){$commands|%{"- $_"}}else{'- (none)'};'';'Static package assertions:';if($static.Count){$static|%{"- $_"}}else{'- (none)'};'';'Manual host rows:';if($manual.Count){$manual|%{"- $_"}}else{'- (none)'};'';'Runtime rows:';if($runtime.Count){$runtime|%{"- $_"}}else{'- (none mapped)'};'';'Non-runnable governed rows:';$nonRunnable|%{"- $_"};if($warnings.Count){'';'Warnings:';$warnings|%{"- $_"}}
