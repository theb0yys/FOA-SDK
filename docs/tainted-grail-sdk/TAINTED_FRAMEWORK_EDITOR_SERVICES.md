# Tainted Framework Editor Services

Status: implemented Framework-layer port; exact-head O3DE build and compiled-test execution remain pending.

## Purpose

This unit ports only reviewed, engine-neutral Tainted Framework behavior into the O3DE authoring layer. It consumes the pinned canonical knowledge pack and provides deterministic read-only projections for Editor tools.

It does not copy or invoke the upstream Mono or IL2CPP hosts.

## Service ownership

`FoundationService` owns one persistent:

```cpp
TaintedFrameworkEditorServices::Service
```

Editor code obtains it through:

```cpp
FoundationService::Get().GetTaintedFrameworkEditorServices()
```

The service has no mutable catalog, source-registry, workspace, profile, filesystem, process, BepInEx, Harmony, Unity, deployment, signing, publication, or save authority.

## Compatibility projection

`EvaluateCompatibility(gameVersion, branch, runtime)` reads the canonical knowledge pack and returns one of:

- `Ready` — only for an exact evidence-backed observation;
- `Blocked` — for a branch explicitly blocked by the pinned upstream evidence;
- `Unsupported` — for version drift, unknown branch/runtime pairs, or non-activatable observations.

The current pinned result is deliberately narrow:

- Tainted Framework `0.1.33`;
- FoA `1.23.401`;
- branch `Mono`;
- runtime `Mono`;
- upstream status `live_load_validated`.

IL2CPP remains blocked. A later game version does not inherit readiness.

## API-surface projection

`GetApiSurfaceDecisions()` returns copied, stably sorted rows from the canonical pack.

Only `runtime-report` is consumer-ready, and only for the named diagnostics-only consumer. Every other surface carries `consumer_binding_not_approved` until a separate reviewed decision updates the canonical pack.

The service does not bind, activate, load, or call any consumer.

## Configuration defaults

`GetConfigurationDefaults()` returns the three exact default-safe upstream settings as copied authoring metadata:

| Section | Key | Value |
| --- | --- | --- |
| `General` | `Enabled` | `true` |
| `Safety` | `DryRunOnly` | `true` |
| `Safety` | `ReportOnlyMode` | `true` |

These values are not written to a BepInEx configuration file and do not change a running host.

## Diagnostic vocabulary

`GetDiagnosticVocabulary()` returns a sorted, duplicate-free copy of the pinned diagnostic/event vocabulary. It does not collect logs, inspect a process, open game files, or expose private paths.

## Activation plan

`BuildActivationPlan()` combines compatibility, API surfaces, configuration defaults, and diagnostic vocabulary into one deterministic in-memory plan.

The plan permanently records:

```text
RuntimeInvocationAllowed = false
FileWriteAllowed = false
CatalogMutationAllowed = false
```

`CandidateEvidenceSubmissionEligible` means only that the exact profile is compatible with the pinned observation. It is not permission and does not submit evidence. Actual submission still requires:

1. the canonical extension registration;
2. the `SubmitCandidateEvidence` ExtensionAPI capability;
3. an existing usable source;
4. exact source/profile/version/branch/runtime binding;
5. acceptance by the governed source-evidence registry.

## Tests and repository gate

Production-linked compiled tests cover:

- exact Mono compatibility;
- IL2CPP blocking;
- game-version drift refusal;
- exact consumer-ready API surface;
- deterministic read-only activation plans;
- evidence ineligibility for blocked profiles;
- persistent Foundation ownership.

`validate_tainted_framework_editor_services.py` additionally rejects:

- mutable Foundation/catalog/source/profile authority in the public surface;
- BepInEx, Unity, Harmony, Qt-file, or standard-filesystem dependencies;
- readiness without exact game-version evidence;
- runtime, file-write, or catalog-mutation flags becoming true;
- evidence eligibility being described as submission authority;
- missing Framework/build/test ownership;
- missing local-validation integration.

## Deferred work

This unit does not include:

- UI Framework widgets or assets;
- an Editor pane for these projections;
- acquisition from local or pinned GitHub sources;
- Mono or IL2CPP runtime adapters;
- live report collection;
- host activation;
- file-backed configuration export;
- deployment or release operations.

The next unit is the reviewed UI Framework utility and embedded-asset intake. Runtime acquisition and host adapters remain later, separately reviewed work.
