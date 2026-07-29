# Deep Research Brief: Multi-Game Installer and Setup Wizard

## Tainted Grail: The Fall of Avalon as the first supported game

## Status

This is a **research brief**, not the research answer, an implementation plan, or authority to modify a game installation.

- Repository: `theb0yys/FOA-SDK`
- Baseline: `e9beb347ae02835cb851bcd79e41b1bd2c60a909`
- Initial platform: Windows x64
- First game integration: Tainted Grail: The Fall of Avalon (`FoA`)
- Required outcome: one source-backed architecture recommendation ready for FOA-SDK design review

The researcher must answer the specified decisions directly. Do not substitute a generic report about installers or mod managers.

---

## 1. Objective

Determine how FOA-SDK should create a **multifunctional installer and setup-wizard platform that can support multiple games through isolated game integrations**, with FoA as the first complete integration.

The product may eventually coordinate:

- FOA-SDK install, upgrade, repair, and uninstall;
- first-run and repeatable setup;
- lawful game-install discovery and validation;
- storefront, game build, runtime, toolchain, workspace, and profile identity;
- acquisition and verification of legally redistributable components;
- optional modding prerequisites;
- component compatibility and updates;
- user-authorised deployment, verification, repair, backup, and rollback;
- diagnostics, receipts, support bundles, and explicit tool/game launch;
- later games without FoA-specific assumptions in the trusted core.

The research must classify each capability as:

1. required in the first release;
2. supported in a later reviewed phase;
3. owned by a separate executor or adapter;
4. prohibited.

“Game installer” must be defined carefully. This request does **not** authorise packaging, downloading, or redistributing the commercial game.

---

## 2. Decisions the research must resolve

1. Should the new platform extend `FOA-SDK-Installer.exe`, sit beside it, orchestrate it as one managed component, or live as a separate reusable product/package?
2. Can the existing per-user MSI remain the sole owner of FOA-SDK product files while a higher-level bootstrapper coordinates setup and optional components?
3. What belongs in the generic core, game integration, storefront provider, component manager, deployment executor, runtime adapter, launcher, and diagnostics layer?
4. Should game integrations be declarative manifests, compiled plug-ins, signed packages, out-of-process providers, or a constrained combination?
5. How can a second game be added without embedding FoA paths, loaders, runtime assumptions, or UI logic in the core?
6. How are identity, provenance, compatibility, ownership, receipts, operation journals, backups, and rollback represented durably?
7. How are operations requiring elevation isolated when the current SDK installer deliberately runs per-user as `asInvoker`?
8. What is the smallest useful first release that is genuinely multifunctional without silently acquiring deployment or runtime authority?
9. What exact FoA installations, versions, runtime routes, prerequisites, and setup actions can be supported from defensible evidence?
10. Which current FOA-SDK contracts can be reused, and which require new schemas, systems, tests, governance, or release gates?

---

## 3. Current repository boundary

The research must preserve these facts unless a separately reviewed architecture change is explicitly recommended and justified:

- FOA-SDK is the product repository. O3DE is an external pinned authoring host; FoA remains an external Unity runtime.
- `FOA-SDK-Installer.exe` verifies one reviewed embedded MSI. Windows Installer owns product-file mutation, registration, repair, upgrade, and uninstall.
- The current SDK package is per-user and does not require administrator elevation.
- The current installer excludes game discovery, game mutation, FoA launch, runtime-adapter deployment, prerequisite installation, automatic updating, telemetry, and proprietary content.
- Workspaces, generated output, diagnostics, game files, and deployment roots are external and are not installer-owned.
- The Tool Wizard records workspace, O3DE, Unity, conversion-project, and FoA paths. It records readiness only; conversion and deployment execution remain disabled.
- Proprietary game files, private installations, saves, credentials, signing material, and extracted content are protected external data.
- Mono and IL2CPP are separate runtime-adapter families. Their binaries, dependencies, compatibility evidence, generated interop, and runtime proof are not interchangeable.
- Generated installers, packages, logs, screenshots, and evidence remain outside the source checkout.
- Research may recommend a design but does not itself grant implementation, deployment, launch, signing, publication, or runtime authority.

---

## 4. Research workstreams

### A. Product definition and user journeys

Define the product and its public terminology. Map complete journeys for:

- a new mod author with FoA installed but no SDK or tools;
- a user without the game installed;
- an existing FOA-SDK user upgrading;
- multiple storefront or library installations;
- existing unmanaged mods/loaders;
- offline setup from a verified bundle;
- interrupted setup, repair, rollback, and uninstall;
- selecting a future second game;
- unattended or scripted use.

For every journey identify user decisions, permissions, network access, writes, evidence, failure recovery, and what remains unmanaged.

### B. Architecture and ownership

Compare at least:

1. expanding the current WinForms executable;
2. preserving the current installer and adding a separate generic bootstrapper/setup application;
3. a reusable core/CLI with a thin Windows UI and isolated game providers;
4. a WiX Burn-style bundle around the existing MSI and separately packaged components;
5. any stronger option found through primary evidence.

Evaluate trust boundary, blast radius, MSI compatibility, extensibility, testability, accessibility, elevation, rollback, offline support, updates, signing, migration cost, supply-chain exposure, and maintainability.

The recommendation must provide a component/owner matrix and state which current components remain unchanged.

### C. Windows delivery technology

Using current official sources, compare the viable Windows stack, including where relevant:

- current self-contained .NET/WinForms delivery;
- WiX MSI and Burn;
- CPack/WiX interaction;
- WPF, WinUI 3/Windows App SDK, or another justified UI host;
- MSIX/App Installer constraints;
- self-contained .NET deployment;
- signed full-version and delta-update approaches;
- native or Rust components only where evidence shows a material benefit.

Verify current versions, licences, support status, Windows 10/11 compatibility, accessibility, offline behaviour, elevation, signing, CI reproducibility, and maintenance cost.

### D. Multi-game integration contract

Determine:

- what can be declarative and what requires executable code;
- whether executable providers must run out of process;
- how provider identity, publisher, version, capabilities, compatibility, hashes, signatures, licences, and provenance are represented;
- how providers are installed, updated, disabled, revoked, and removed;
- how schema downgrade, capability escalation, path escape, duplicate identity, and incompatible-core attacks fail closed;
- how game-specific UI is contributed without giving a provider lifecycle or security authority;
- what stable contract is required to add a second game.

The platform must not become a general-purpose arbitrary-script installer.

### E. Game and storefront discovery

Research bounded discovery through explicit user selection and authoritative metadata. Assess:

- Windows uninstall registration;
- supported storefront manifests and library records;
- executable/data-directory identity;
- file-version metadata and narrowly justified fingerprints;
- multiple installs, branches, copied installs, stale records, moved libraries, network/removable drives, long paths, non-ASCII paths, junctions, and reparse points;
- privacy, consent, scan limits, redacted logs, and TOCTOU protection.

Discovery must produce typed evidence and confidence. A guessed or detected path must not become deployment authority.

### F. Components and prerequisites

For every candidate component establish:

- authoritative source and redistribution rights;
- licence/notices;
- exact version and compatibility policy;
- immutable release identity, expected size, checksum, and publisher signature where available;
- offline cache behaviour;
- dependency order;
- per-user/per-machine scope and elevation;
- repair, update, uninstall, and end-of-life ownership.

Separate components into bundled, downloaded from an authoritative source, user-installed independently, local/proprietary inputs that may only be detected, and unsupported/legally unclear.

Do not infer redistribution permission from public availability.

### G. Deployment, transaction, backup, and rollback

Specify an implementation-independent operation model covering:

- exact preflight inventory;
- managed versus unmanaged ownership;
- additions, replacements, removals, conflicts, and locked files;
- pre-change fingerprints and contained backups;
- a typed inverse for every change;
- staging, publish, resumability, idempotency, cancellation, and concurrency;
- crash/power-loss recovery;
- post-write verification and rollback verification;
- preservation of foreign and user-created files;
- explicit confirmation immediately before mutation;
- least-privilege elevation for the exact operation only.

Compare a short-lived privileged helper, in-process elevation, Windows Installer mechanisms, a service, and other viable methods. Do not recommend a persistent privileged service without a compelling threat-modelled need.

### H. Updates, repair, migration, and uninstall

Define independent versioning for the core, UI, game integrations, SDK MSI, optional components, profiles, receipts, caches, and backups.

Research:

- compatibility resolution and release channels;
- authenticated update metadata and packages;
- downgrade, replay, freeze, revocation, and compromised-feed defence;
- offline update and repair;
- ownership of each repair action;
- what survives uninstall;
- profile/receipt schema migration or rejection;
- recovery from a failed core update;
- safe reporting of orphaned components and abandoned backups.

### I. Security, privacy, and legal constraints

Threat-model at minimum:

- malicious manifests/providers;
- arbitrary code through hooks;
- path traversal, archive extraction, symlink/junction/reparse attacks;
- command injection, unsafe process lookup, and DLL search hijacking;
- package substitution and TOCTOU;
- update-feed compromise, replay, and downgrade;
- malicious local paths, poisoned caches, partial downloads, and resource exhaustion;
- privilege escalation and confused-deputy behaviour;
- deletion of unmanaged files;
- backup/log leakage of proprietary data, credentials, usernames, or private paths;
- unsigned binaries, SmartScreen, dependency compromise, and revocation.

Define trust levels for repository-owned, publisher-signed, checksum-only, locally generated, user-selected, proprietary, and unknown inputs, and state what each level may permit.

Identify trademark, EULA, redistribution, API-term, and third-party-licence constraints. Separate technical possibility from legal permission.

### J. UX, accessibility, diagnostics, and performance

Specify:

- first-run setup versus ongoing management;
- simple and advanced modes;
- per-game dashboards and component selection;
- exact state vocabulary: detected, validated, ready, blocked, installed, verified, failed, rolled back;
- review of exact planned changes before execution;
- nested progress, cancellation rules, recovery, repair, rollback, and support flow;
- keyboard navigation, screen readers, scaling, high contrast, and localisation;
- privacy-safe structured logs, operation IDs, receipts, result envelopes, and support bundles with user preview;
- measurable budgets for cold start, bounded discovery, memory, hashing, downloads, disk amplification, UI responsiveness, and forced-termination recovery.

Avoid full-disk scans, repeated hashing, synchronous UI-thread I/O, misleading defaults, or hidden telemetry.

### K. FoA first-game integration

Using public primary sources, exact repository evidence, and only separately authorised local experiments, establish:

- supported Windows distribution channels and authoritative install metadata;
- executable, data-directory, version, and Unity/runtime identity signals;
- exact Mono and IL2CPP routes and how they are distinguished;
- game-build compatibility and unsupported-state behaviour;
- BepInEx, Tainted Framework, generated interop, toolkit, and other prerequisite ownership/redistribution constraints;
- existing loader/mod-directory detection;
- safe profile creation from a user-selected installation;
- actions that remain read-only;
- actions requiring a future deployment executor and exact-install evidence;
- launch method only if justified and separately gated;
- game-update invalidation, repair, uninstall, rollback, and synthetic test fixtures.

Repository Mono/IL2CPP records are version-specific evidence, not permanent universal compatibility claims.

---

## 5. Mandatory scenarios

Test the recommendation against:

1. clean supported Windows machine;
2. no game present;
3. one valid FoA install;
4. multiple storefront/library installs;
5. unsupported or stale game build;
6. uncertain Mono/IL2CPP identity;
7. existing unmanaged loader/mods;
8. moved, partial, junction-backed, or invalid path;
9. no administrator rights;
10. exact operation requiring elevation;
11. offline verified bundle;
12. interrupted/corrupted download;
13. cancellation before mutation;
14. process termination during mutation;
15. locked target file or antivirus interference;
16. insufficient staging/backup space;
17. repair after deliberate product damage;
18. partial deployment failure and rollback;
19. uninstall preserving workspace and unmanaged content;
20. incompatible, revoked, or compromised integration/component;
21. second game added without FoA-specific core changes;
22. non-ASCII, long-path, and case-collision conditions.

For each, state expected UI, operation state, writes, evidence, recovery, and exit-code semantics.

---

## 6. Research method

### Source priority

1. Official Microsoft, WiX, .NET, O3DE, Unity, storefront, GitHub, BepInEx, and relevant tool documentation.
2. Authoritative repositories, release notes, schemas, security advisories, and maintainer statements.
3. FOA-SDK records pinned to exact commit and path.
4. Reproducible experiments with synthetic fixtures.
5. Community reports only as labelled secondary evidence.

For each load-bearing source record title, publisher, URL/path, version/tag/commit, date, retrieval date, claim supported, limitations/conflicts, and licence relevance.

Keep proven facts, experiments, inferences, recommendations, unknowns, and rejected assumptions separate. Reverify all current versions, licences, support status, tool releases, storefront behaviour, and game-build claims at research time.

Do not scan, extract, decompile, broadly hash, modify, launch, or publish data from a private FoA installation without explicit permission for the exact path and experiment. Where local proof is necessary, provide an experiment plan and required evidence rather than inventing the result.

---

## 7. Required output

The research must return one decision package containing:

1. executive recommendation answering every decision in section 2;
2. precise product terminology and capability scope;
3. current-state and ownership map;
4. source register;
5. comparison of relevant installer/bootstrapper/mod-manager patterns;
6. architecture options matrix and rejected alternatives;
7. recommended system architecture and trust boundaries;
8. multi-game provider/manifest/protocol contract;
9. FoA integration dossier with supported facts and unresolved evidence;
10. operation state machine and privilege model;
11. schemas/migration model for profiles, components, receipts, journals, caches, backups, and diagnostics;
12. security threat model and legal/redistribution matrix;
13. UX page flow and accessibility requirements;
14. testing/evidence matrix, including interruption and recovery;
15. performance/reliability budgets;
16. migration path from the current installer and Tool Wizard;
17. phased implementation sequence with the first safe vertical slice;
18. risk/unknowns register and exact next design or experiment gate.

The recommendation must be explicit and implementation-oriented. Do not bury it inside a broad essay.

---

## 8. Acceptance criteria

The research is complete only when:

- every blocked decision is answered or tied to exact missing evidence;
- one architecture is recommended against credible alternatives;
- generic core, SDK lifecycle, game integration, deployment executor, runtime adapter, and launcher have distinct owners;
- the existing MSI is preserved or replacement is justified with migration evidence;
- a second game can be added without FoA-specific core assumptions;
- detection never silently becomes mutation authority;
- writes are planned, consented, contained, attributable, journalled, verified, and reversible where claimed;
- unmanaged files are preserved by default;
- elevation is bounded and least-privilege;
- update authenticity, anti-downgrade, revocation, and recovery are addressed;
- redistribution rights are proven per component;
- FoA compatibility binds to exact versions/evidence and keeps Mono/IL2CPP distinct;
- security, privacy, accessibility, offline use, repair, rollback, and interruption recovery are first-class;
- the first implementation slice has explicit tests, evidence, performance limits, rollback, and stop conditions;
- research evidence is not presented as implementation or runtime proof.

---

## 9. Explicit exclusions

Do not:

- implement or modify installer code;
- produce an installer binary;
- change MSI identity or packaging;
- deploy BepInEx, Tainted Framework, adapters, mods, or interop;
- launch FoA or access saves;
- redistribute proprietary game/toolkit files;
- add telemetry, signing keys, releases, or publication;
- support Linux, macOS, Steam Deck, consoles, or mobile in the first architecture;
- treat checksums as publisher identity;
- treat path detection, compilation, packaging, or preview output as runtime proof;
- use broad scans or unrestricted scripting.

---

## 10. Repository path map

Use the pinned baseline when recording repository observations.

| Path | Research relevance |
| --- | --- |
| `README.md` | Product identity, external O3DE boundary, installer and runtime limits |
| `AGENTS.md` | Research-first gate, document authority, branch/PR and truth requirements |
| `CURRENT_TASK.md`, `DECISIONS.md` | Existing active task and durable research-authority decisions; do not overwrite |
| `GOVERNANCE.md`, `CONTRIBUTING.md` | Significant-change review, architecture, security, migration, and test requirements |
| `docs/protected-files-policy.md` | Private game installations and proprietary content remain protected |
| `Research/README.md` | Research source rules and lack of implementation authority |
| `docs/systems/SYSTEM_INDEX.md` | Existing owner systems and need for any new system classification |
| `docs/tainted-grail-sdk/ARCHITECTURE.md` | Runtime separation, exact identity, persistence, permission, and adapters |
| `docs/tainted-grail-sdk/WINDOWS_INSTALLER_AND_ARTIFACT_WORKFLOW_DESIGN.md` | Approved SDK-only installer authority and exclusions |
| `Installer/README.md` | Current lifecycle, user contract, acceptance, and external-data boundary |
| `Installer/Launcher/Windows/README.md` | Current CLI, Tool Wizard, smoke, and security contract |
| `Installer/Launcher/Windows/InstallerWizardForm.cs` | Current SDK lifecycle UI surface |
| `Installer/Launcher/Windows/ToolSetupWizardForm.cs` | Existing non-mutating setup journey |
| `Installer/Launcher/Windows/ToolSetupProfile.cs` | Current profile/readiness schema and path validation |
| `SECURITY.md` | Minimum threat and path-containment baseline |
| `docs/tainted-grail-sdk/LEGAL_AND_CONTENT_POLICY.md` | Redistribution, fixtures, trademarks, and proprietary content |
| `docs/tainted-grail-sdk/RELEASE_PROCESS.md` | Deployment, signing, evidence, rollback, and release gates |
| `Plugins/RuntimeAdapters/README.md` | Runtime adapter separation and authority limits |
| `Plugins/RuntimeAdapters/Mono/README.md` | Version-specific Mono route evidence |
| `Plugins/RuntimeAdapters/IL2CPP/README.md` | Independent version-specific IL2CPP route evidence |

Pinned GitHub base:

`https://github.com/theb0yys/FOA-SDK/blob/e9beb347ae02835cb851bcd79e41b1bd2c60a909/`

---

## 11. Handoff

- Consuming gate: significant-change architecture/design review.
- Primary owner: `installer`.
- Required cross-review: `workspace-and-packs`, `schemas-and-persistence`, `permissions-and-risk`, `external-toolchain`, `deployment-review`, `runtime-adapter-contracts`, `diagnostics`, `test-harness`, and `release-governance`.
- Next action only if the research proves authority: create a separately reviewed design record for the approved product boundary and smallest implementation slice.

No implementation follows directly from this brief alone.
