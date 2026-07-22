# Complete First-Mod Path

## Scope

This path teaches the complete lifecycle of a minimal Tainted Grail: The Fall of Avalon code mod without pretending that an unverified game method is safe to patch.

The first acceptance target is deliberately small:

1. identify the active runtime profile;
2. build a project-owned plug-in;
3. load it through the correct BepInEx route;
4. emit one bounded log message;
5. bind one local configuration value;
6. remove the plug-in cleanly;
7. record evidence and package only project-owned output.

A Harmony patch is a later step and requires a promoted hook record.

## 1. Select and record the runtime profile

Start with [`../runtime/VERIFIED_PROFILES.md`](../runtime/VERIFIED_PROFILES.md).

Record:

- game version and branch;
- Mono or IL2CPP;
- BepInEx version;
- framework version, if selected;
- source of each installed dependency;
- local installation layout;
- exact profile ID or `profile-unverified`.

Stop when the route is unclear. Do not install a Mono package into IL2CPP or an IL2CPP package into Mono.

## 2. Keep dependencies local

The project may reference local BepInEx, Unity and game assemblies for compilation. Those files remain in the operator's installation and must not enter Git, examples, release packages, diagnostics, or screenshots.

Use a configurable property such as `FoAGameRoot` or an equivalent local-only build setting. Do not hard-code a private machine path into committed files.

## 3. Create the project

Create a C# class library with project-owned identity:

- unique assembly name;
- unique root namespace;
- unique BepInEx plug-in GUID;
- human-readable plug-in name;
- semantic version;
- explicit target framework matching the selected route;
- local references resolved from the operator-supplied game root;
- no copied game or loader binaries.

For the current pinned Mono baseline, the upstream template targeted `netstandard2.1`. Treat that as profile-scoped, not permanent truth.

## 4. Add the minimal plug-in lifecycle

The first plug-in should contain only:

- the BepInEx plug-in declaration;
- one `Awake` or equivalent initialization method;
- one bounded startup log message;
- one configuration binding;
- an explicit cleanup method when the selected API/lifecycle requires it.

The startup message should include the plug-in identity and version, but no private path, user name, token, save content, or machine inventory.

Do not add Harmony patches, file mutation, networking, save access, process launching, telemetry, or automatic update behavior to the first load proof.

## 5. Build

Build from source after the final change to:

- code;
- project file;
- assembly identity;
- plug-in GUID/name/version;
- dependency version;
- package layout.

A previously built DLL becomes stale after any of those changes.

Required build evidence:

- command used;
- selected profile ID;
- resulting assembly name;
- assembly SHA-256;
- build result;
- warnings relevant to runtime compatibility.

Do not treat build success as load success.

## 6. Deploy to a controlled local target

Deploy only to the selected local game profile. Preserve the previous state so removal is deterministic.

Record:

- destination relative to the selected game root;
- files added;
- files replaced, which should normally be none for a first mod;
- backup requirement;
- removal steps.

Do not package or copy dependencies owned by the game, Unity, BepInEx, another framework, or another mod.

## 7. Launch and prove load

Launch the exact selected game profile through the normal operator path.

Confirm:

- BepInEx loaded;
- the expected plug-in GUID/name/version appears;
- the plug-in startup message appears exactly once;
- the configuration file is created in the expected location when applicable;
- no dependency, reflection, type-load or duplicate-GUID error appears;
- game startup reaches the normal expected state.

Capture only approved redacted evidence. Prefer fingerprints and short relevant excerpts over whole logs.

## 8. Exercise configuration

Change the harmless configuration value, restart when required, and confirm the plug-in reads the new value.

This proves the basic configuration lane without modifying gameplay. Record:

- key name and type;
- default value;
- changed value used for the test;
- restart/reload requirement;
- observed log or behavior result.

Do not include the operator's complete config directory in evidence.

## 9. Remove and verify cleanup

Remove the project-owned plug-in files and restart the game.

Confirm:

- the plug-in no longer loads;
- no project-owned runtime object, patch or event subscription survives process restart;
- player data and unrelated configuration remain untouched;
- the game returns to the pre-test package state.

A first-mod proof is incomplete without removal evidence.

## 10. Create the mod record

Document:

- purpose and non-goals;
- exact supported profile IDs;
- source and dependency provenance;
- project identity and GUID ownership;
- build steps;
- install/remove steps;
- validation evidence;
- known limitations;
- support information;
- licence and redistribution state.

Use the handbook process and current SDK governance. Historical upstream validation labels are not automatically current SDK evidence.

## 11. Package only what the user needs

A normal minimal package should contain project-owned files only, for example:

```text
plugins/
  ExampleMod/
    ExampleMod.dll
    README.md
    CHANGELOG.md
```

Include a default configuration file only when its behavior and overwrite policy are explicit.

Never package:

- game assemblies or native files;
- BepInEx binaries;
- Unity files;
- another mod or framework without reviewed redistribution rights;
- saves, logs, local paths, credentials or machine reports;
- stale DLLs from a previous source state.

## 12. Add a hook only after promotion

To change gameplay, create or select a hook record from [`../hooks/README.md`](../hooks/README.md).

Before implementation, the record must identify:

- exact profile;
- assembly, type, member and signature;
- patch/event/reflection kind;
- expected call timing and frequency;
- ownership of parameters and return values;
- failure behavior;
- cleanup/unpatch behavior;
- known collisions and ordering;
- minimum evidence state.

Prefer, in order:

1. a public SDK/ExtensionAPI service;
2. a stable game event or service interface;
3. a narrow Harmony postfix;
4. a narrow prefix with explicit skip semantics;
5. reflection only when no stable API exists;
6. a transpiler only with a separately reviewed necessity and instruction-level evidence.

## Acceptance checklist

The first-mod path passes only when all are true:

- [ ] exact runtime profile recorded;
- [ ] dependencies remain local and uncommitted;
- [ ] unique project, assembly and plug-in identity established;
- [ ] clean source build completed;
- [ ] final DLL fingerprint recorded;
- [ ] plug-in loaded exactly once on the selected profile;
- [ ] bounded startup log verified;
- [ ] harmless configuration round trip verified;
- [ ] no runtime errors attributable to the plug-in;
- [ ] deterministic removal verified;
- [ ] package contains only reviewed project-owned files;
- [ ] compatibility and evidence claims match what was actually tested.

## Current limitation

This documentation unit defines and verifies the process contract from the pinned source and current SDK evidence. A project-owned downloadable starter package, automated local profile inspector, build runner, deployment executor and live test harness remain separate implementation units.