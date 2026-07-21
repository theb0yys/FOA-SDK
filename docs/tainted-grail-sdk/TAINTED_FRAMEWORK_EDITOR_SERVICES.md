# Tainted Framework Editor Services

Status: source-complete Framework-layer port; exact-head O3DE configure, build, and compiled-test execution remain required before acceptance.

## Purpose

This unit ports only reviewed, engine-neutral Tainted Framework behavior into the O3DE authoring layer. It consumes the pinned canonical knowledge pack and provides deterministic read-only projections for Editor tools.

It does not copy, link, load, or invoke the upstream Mono or IL2CPP hosts.

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

## Safe profile binding

The preferred binding lane uses the public `ExtensionAPI`:

1. register the canonical `extension.tainted-framework` declaration;
2. request the active profile through `ExtensionAPI::GetActiveProfile()`;
3. pass the returned sanitized `ExtensionAPI::ProfileView` copy to `EvaluateCompatibility()` or `BuildActivationPlan()`.

The service never receives a mutable `WorkspaceModel`, `GameProfile`, catalog, or source-registry reference. The typed profile overload projects only the sanitized game version, branch, runtime target, and BepInEx version fields needed for the decision.

## Compatibility projection

`EvaluateCompatibility(gameVersion, branch, runtime, bepInExVersion)` reads the canonical knowledge pack and returns one of:

- `Ready` — only for one exact evidence-backed observation;
- `Blocked` — for a branch explicitly blocked by the pinned upstream evidence;
- `Unsupported` — for game-version drift, BepInEx-version drift, unknown branch/runtime pairs, ambiguous exact observations, or non-activatable observations.

The current pinned ready result is deliberately narrow:

- Tainted Framework `0.1.33`;
- FoA `1.23.401`;
- branch `Mono`;
- runtime `Mono`;
- BepInEx `5.4.23.3`;
- upstream status `live_load_validated`.

IL2CPP remains blocked. A later game or BepInEx version does not inherit readiness. Conflicting exact observations fail closed rather than depending on source-row order.

## API-surface projection

`GetApiSurfaceDecisions()` returns copied, stably sorted rows from the canonical pack.

A surface is consumer-ready only when all canonical fields agree: the surface is marked ready, has `candidate` status, and names a non-empty, non-disabled consumer binding. Only `runtime-report` currently passes those checks for the named diagnostics-only consumer.

Blocked surfaces carry `surface_status_blocked`. Other unapproved surfaces carry `consumer_binding_not_approved`. The service does not bind, activate, load, or call a consumer.

## Configuration defaults

`GetConfigurationDefaults()` returns the three exact default-safe upstream settings as copied authoring metadata:

| Section | Key | Value |
| --- | --- | --- |
| `General` | `Enabled` | `true` |
| `Safety` | `DryRunOnly` | `true` |
| `Safety` | `ReportOnlyMode` | `true` |

These values are bound by the canonical golden fixture. They are not written to a BepInEx configuration file and do not change a running host.

## Diagnostic vocabulary

`GetDiagnosticVocabulary()` returns a sorted, duplicate-free copy of the pinned diagnostic/event vocabulary. Its exact result is bound by the canonical golden fixture. It does not collect logs, inspect a process, open game files, or expose private paths.

## Activation plan

`BuildActivationPlan()` combines compatibility, API surfaces, configuration defaults, and diagnostic vocabulary into one deterministic in-memory plan. The plan identity includes the exact framework, branch, runtime, game, and BepInEx versions.

The plan permanently records:

```text
RuntimeInvocationAllowed = false
FileWriteAllowed = false
CatalogMutationAllowed = false
```

`CandidateEvidenceSubmissionEligible` is true only when the exact profile is ready and at least one API surface is consumer-ready. It is not permission and does not submit evidence. Actual submission still requires:

1. canonical extension registration;
2. the `SubmitCandidateEvidence` ExtensionAPI capability;
3. an existing usable source;
4. exact source/profile/version/branch/runtime binding;
5. acceptance by the governed source-evidence registry.

## Compiled coverage and repository gate

Production-linked compiled tests cover:

- exact Mono game and BepInEx compatibility;
- IL2CPP blocking;
- game-version drift refusal;
- BepInEx-version drift refusal;
- exact consumer-ready API-surface selection;
- stable blocked-surface reasons;
- deterministic read-only activation plans;
- evidence ineligibility for blocked profiles;
- sanitized `ProfileView` binding;
- fail-closed sanitized-profile BepInEx drift;
- persistent Foundation ownership.

`validate_tainted_framework_editor_services.py` additionally rejects:

- mutable Foundation/catalog/source/profile authority in the public surface;
- BepInEx, Unity, Harmony, Qt-file, or standard-filesystem dependencies;
- readiness without exact game and BepInEx evidence;
- order-dependent or ambiguous compatibility promotion;
- consumer readiness with blocked or disabled surface metadata;
- runtime, file-write, or catalog-mutation flags becoming true;
- evidence eligibility without a consumer-ready surface;
- evidence eligibility being described as submission authority;
- compiled-test, Framework-manifest, golden-fixture, documentation, or local-validation drift.

## Acceptance boundary

Source registration, validation wiring, documentation, deterministic projections, golden-fixture binding, and compiled-test registration are complete on the development branch. Acceptance still requires the exact-head local validation command, reviewed-range whitespace check, approved O3DE configure/build, production-linked CTest execution, and final authority-drift review.

No acquisition work may be treated as complete until those checks are attached to the exact head.

## Deferred work

This unit does not include:

- acquisition from local or pinned GitHub sources;
- operational Merlin Workshop acquisition;
- Mono or IL2CPP runtime adapters;
- live report collection;
- host activation;
- file-backed configuration export;
- deployment or release operations.

The next governed unit is the acquisition-provider layer: local filesystem, pinned GitHub, and optional Merlin routes with exact provenance, licence, branch, version, runtime applicability, and candidate-evidence return without automatic promotion.
