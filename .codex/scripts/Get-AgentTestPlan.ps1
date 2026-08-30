param(
    [string]$Request = "",
    [string[]]$TargetPath = @(),
    [switch]$ListSystems,
    [switch]$AsJson
)

$ErrorActionPreference = 'Stop'
# Optional helper: map test evidence only when ownership or applicability is unclear.
$staticValidation = 'python Gems/TaintedGrailModdingSDK/Tools/run_local_validation.py --keep-going --static-only --skip-source-policy'
$catalog = @(
    @{ key='foundation-services'; path='Gems/TaintedGrailModdingSDK'; test='python Gems/TaintedGrailModdingSDK/Tools/validate_foundation.py' },
    @{ key='external-toolchain'; path='Gems/ExternalToolchain'; test=$staticValidation },
    @{ key='extension-api'; path='Gems/TaintedGrailModdingSDK'; test='python Gems/TaintedGrailModdingSDK/Tools/validate_foundation.py' },
    @{ key='plugins'; path='Plugins'; test=$staticValidation },
    @{ key='installer'; path='Installer'; test=$staticValidation },
    @{ key='editor-project'; path='TaintedGrailModdingEditor'; test='python Gems/TaintedGrailModdingSDK/Tools/developer_preview.py validate --engine-root <pinned-o3de-root> --build-dir <foa-build-root>' },
    @{ key='unity-bridge'; path='Plugins/RuntimeAdapters'; test=$staticValidation },
    @{ key='test-harness'; path='Gems/TaintedGrailModdingSDK/Code/Tests'; test=$staticValidation }
)

if ($ListSystems) {
    if ($AsJson) {
        $catalog | ConvertTo-Json -Depth 4
        return
    }
    'Codex system test catalog'
    '========================='
    $catalog | ForEach-Object {
        "- key=$($_.key); path=$($_.path); suggested=$($_.test)"
    }
    return
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

function Add-Unique([System.Collections.Generic.List[string]]$List, [string]$Value) {
    if (-not $List.Contains($Value)) { [void]$List.Add($Value) }
}

foreach ($entry in $catalog) {
    if (
        $combined -match [regex]::Escape($entry.key)
        -or $combined -match [regex]::Escape($entry.path.ToLowerInvariant())
    ) {
        Add-Unique $systems $entry.key
        Add-Unique $commands $entry.test
    }
}

if ($combined -match 'gems[\\/].+[\\/]code|foundation|service|catalog|schema|contract') {
    Add-Unique $surfaces 'Foundation or owner code'
    Add-Unique $tests 'Positive, negative, malformed-input, lifecycle, persistence, and degraded-dependency tests applicable to the owned behavior.'
}
if ($combined -match 'plugins[\\/]|plugin\.json|extensionapi') {
    Add-Unique $surfaces 'Plug-in package'
    Add-Unique $tests 'Manifest, registration, capability, provenance, licence, compatibility, lifecycle, and no-implicit-authority coverage.'
}
if ($combined -match '\bui\b|\bpane\b|\bwidget\b|\bqt\b|\bview\b') {
    Add-Unique $surfaces 'Editor UI route'
    Add-Unique $tests 'Binding, command forwarding, lifecycle, accessibility, error-state, and no-domain-truth-ownership coverage.'
}
if ($combined -match 'unity|conversion|externaltoolchain|handoff') {
    Add-Unique $surfaces 'External toolchain or Unity conversion'
    Add-Unique $tests 'Deterministic plans, bounded execution, canonical handoff, failure cleanup, and no-automatic-promotion coverage.'
    Add-Unique $manual 'Run the reviewed conversion lane only when the exact project and owning design make it applicable.'
}
if ($combined -match 'installer|msi|wix|package') {
    Add-Unique $surfaces 'Installer or package'
    Add-Unique $tests 'Payload determinism, install/repair/upgrade/uninstall, failure recovery, and authority-boundary coverage.'
    Add-Unique $manual 'Run the applicable installer/package workflow when artifact or lifecycle behavior can change.'
}
$runtimeApplicable = $combined -match 'bepinex|mono|il2cpp|runtime adapter|game runtime|launch foa|fall of avalon'
if ($runtimeApplicable) {
    Add-Unique $surfaces 'Runtime adapter or live game operation'
    Add-Unique $tests 'Exact-profile detection, contract conformance, fail-closed behavior, packaging, cleanup, and unsupported-mutation refusal.'
    Add-Unique $runtime 'Execute only the lawful exact-install operational lane required by the owning design.'
    Add-Unique $nonRunnable 'Without a lawful matching installation and profile, live Fall of Avalon evidence is `NOT_RUN`; host or build evidence does not substitute.'
}
if ($combined -match 'o3de|taintedgrailmoddingeditor|gem\.json|gems[\\/].+[\\/]code|assetprocessor') {
    Add-Unique $surfaces 'O3DE host'
    Add-Unique $manual 'Run configure/build/compiled or Editor evidence only when the changed host layer requires it.'
}
if ($combined -match 'schema|manifest|json|xml|interchange|package') {
    Add-Unique $static 'Validate canonical serialization, stable identity, schema/package constraints, and malformed-input rejection.'
}
if ($combined -match 'documentation|docs[\\/]|markdown|readme') {
    Add-Unique $surfaces 'Documentation or process text'
    Add-Unique $tests 'Targeted documentation, policy, link, or structure validation only.'
}

if ($surfaces.Count -eq 0) {
    Add-Unique $surfaces 'No helper-specific test surface selected'
    Add-Unique $warnings 'This helper may be `NOT_APPLICABLE`; use the owning tests and CI validation matrix directly.'
}
if ($commands.Count -eq 0 -and $surfaces[0] -ne 'No helper-specific test surface selected') {
    Add-Unique $warnings 'No immediate command was selected. Identify the owning test target rather than running an unrelated broad suite.'
}

$result = [ordered]@{
    request = $Request
    target_paths = $TargetPath
    selection_model = 'optional'
    changed_systems = @($systems)
    surfaces = @($surfaces)
    required_tests = @($tests)
    immediate_codex_commands = @($commands)
    static_package_assertions = @($static)
    manual_host_rows = @($manual)
    runtime_rows = @($runtime)
    non_runnable_governed_rows = @($nonRunnable)
    warnings = @($warnings)
}
if ($AsJson) {
    $result | ConvertTo-Json -Depth 5
    return
}

'Codex test helper'
'================='
"Request: $Request"
'Selection model: optional'
''
'Changed systems:'
if ($systems.Count -gt 0) { $systems | ForEach-Object { "- $_" } } else { '- (none selected)' }
''
'Changed surfaces:'
$surfaces | ForEach-Object { "- $_" }
''
'Suggested tests:'
if ($tests.Count -gt 0) { $tests | ForEach-Object { "- $_" } } else { '- (none selected)' }
''
'Suggested commands:'
if ($commands.Count -gt 0) { $commands | ForEach-Object { "- $_" } } else { '- (none selected)' }
''
'Static/package assertions:'
if ($static.Count -gt 0) { $static | ForEach-Object { "- $_" } } else { '- (none selected)' }
''
'Manual host rows:'
if ($manual.Count -gt 0) { $manual | ForEach-Object { "- $_" } } else { '- (none selected)' }
''
'Runtime rows:'
if ($runtime.Count -gt 0) { $runtime | ForEach-Object { "- $_" } } else { '- (none selected)' }
''
'Non-runnable rows:'
if ($nonRunnable.Count -gt 0) { $nonRunnable | ForEach-Object { "- $_" } } else { '- (none selected)' }
if ($warnings.Count -gt 0) {
    ''
    'Warnings:'
    $warnings | ForEach-Object { "- $_" }
}
