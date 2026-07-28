param(
    [string]$Request="",
    [string[]]$TargetPath=@(),
    [string]$EngineRoot="",
    [string]$BuildRoot="",
    [string]$ExternalDestination="",
    [switch]$AsJson
)
$ErrorActionPreference='Stop'
$repoRoot=Resolve-Path (Join-Path $PSScriptRoot '../..')
if([string]::IsNullOrWhiteSpace($EngineRoot)){$EngineRoot=$env:FOA_O3DE_ROOT}
if([string]::IsNullOrWhiteSpace($BuildRoot)){$BuildRoot=$env:FOA_BUILD_ROOT}
if([string]::IsNullOrWhiteSpace($BuildRoot)){$BuildRoot=(Join-Path (Split-Path $repoRoot -Parent) 'foa-build')}
$combined=(($Request,($TargetPath -join ' '))-join ' ').ToLowerInvariant()
$products=New-Object 'System.Collections.Generic.List[object]';$steps=New-Object 'System.Collections.Generic.List[string]';$warnings=New-Object 'System.Collections.Generic.List[string]'
function Add-U($l,$v){if(-not $l.Contains($v)){[void]$l.Add($v)}}
function Add-Product($name,$command,$kind){[void]$products.Add([ordered]@{name=$name;kind=$kind;build_command=$command;output_root=$BuildRoot})}
if($combined -match 'gems[\\/]|taintedgrailmoddingeditor|o3de|editor|foundation|externaltoolchain'){Add-Product 'FOA-SDK Developer Preview' 'python Gems/TaintedGrailModdingSDK/Tools/developer_preview.py build --engine-root <pinned-o3de-root> --build-dir <foa-build-root>' 'O3DE build'}
if($combined -match 'plugins[\\/]|plugin|extensionapi'){Add-Product 'FOA-SDK plug-in package set' 'python Gems/TaintedGrailModdingSDK/Tools/run_local_validation.py --engine-root <pinned-o3de-root> --ctest-build-dir <foa-build-root>' 'Plug-in validation/package'}
if($combined -match 'unity|conversion|externaltoolchain|handoff'){Add-Product 'Unity conversion candidate output' '<reviewed Unity batch conversion command>' 'Unity conversion'}
if($combined -match 'installer|msi|wix|package'){Add-Product 'FOA-SDK installer artifacts' '<reviewed installer build command>' 'Installer'}
if($combined -match 'bepinex|mono|il2cpp|runtime adapter'){Add-Product 'FOA runtime-adapter artifact' '<adapter-specific Release build command>' 'Runtime adapter'}
if($products.Count -eq 0){Add-U $warnings 'No artifact-producing product was detected. Identify the affected product before claiming completion.'}
foreach($step in @(
'Build the complete affected Release/Profile artifact set from current source and exact pinned dependencies; the touched target alone may be insufficient.',
'Write generated output only under FOA_BUILD_ROOT or another reviewed external output directory.',
'Record source commit, O3DE lock identity, toolchain/profile identity, build configuration, artifact paths, SHA256 hashes, and timestamps.',
'Before any external write, obtain explicit current-task approval and create backup or rollback evidence.',
'Do not copy to a Fall of Avalon installation, Unity project, signing service, publication target, or release channel without explicit authority.',
'After an authorised copy, verify source and destination SHA256 hashes and timestamps.',
'Separate build proof, Editor proof, Unity conversion proof, installer proof, adapter proof, and Fall of Avalon runtime proof.'
)){Add-U $steps $step}
if(-not (Test-Path -LiteralPath $BuildRoot)){Add-U $warnings "Build root $BuildRoot is not available in this environment; artifact execution may be blocked."}
if(-not [string]::IsNullOrWhiteSpace($ExternalDestination)){Add-U $warnings 'An external destination was supplied. Confirm explicit approval before writing.'}
$result=[ordered]@{
    request=$Request
    target_paths=[string[]]$TargetPath
    engine_root=$EngineRoot
    build_root=$BuildRoot
    external_destination=$ExternalDestination
    products=[object[]]$products.ToArray()
    required_steps=[string[]]$steps.ToArray()
    warnings=[string[]]$warnings.ToArray()
}
if($AsJson){$result|ConvertTo-Json -Depth 6;return}
'Codex artifact/deployment preflight';'===================================';"Request: $Request";'';"Build root: $BuildRoot";'';'Products:';if($products.Count){$products|%{"- $($_.name) [$($_.kind)]`n  Build: $($_.build_command)"}}else{'- (not detected)'};'';'Required steps:';$steps|%{"- $_"};if($warnings.Count){'';'Warnings:';$warnings|%{"- $_"}}
