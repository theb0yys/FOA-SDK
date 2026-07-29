# Deep Research Brief: Multi-Game Installer and Setup Wizard Platform

## Tainted Grail: The Fall of Avalon as the first supported game

## Status and instruction

This document is a **research brief**. It defines the investigation that must be performed; it is not the research answer, an implementation plan, an accepted architecture decision, or authority to change a game installation.

- Repository: `theb0yys/FOA-SDK`
- Research baseline: `e9beb347ae02835cb851bcd79e41b1bd2c60a909`
- First target: **Tainted Grail: The Fall of Avalon** (`FoA`)
- Initial platform: **Windows x64**
- Product direction: a reusable, multi-game installer and setup-wizard platform whose first complete game integration is FoA
- Required result: a source-backed, decision-ready recommendation that can enter FOA-SDK design review

The researcher must not substitute a general report about installers. The work must answer the exact product, architecture, security, lifecycle, ownership, and FoA integration questions below.

---

## 1. Request

Research how FOA-SDK should create a **multifunctional Windows installer and setup wizard capable of supporting multiple games through isolated game integrations**, beginning with Tainted Grail: The Fall of Avalon.

The intended product may eventually coordinate several distinct functions:

- install, upgrade, repair, and uninstall the FOA-SDK product;
- run first-time setup and reconfiguration;
- locate and validate lawful existing game installations;
- establish game, storefront, build, runtime, toolchain, workspace, and profile identity;
- acquire and verify legally redistributable tools or components;
- configure optional modding prerequisites;
- manage component selection and compatibility;
- prepare, review, apply, verify, repair, and roll back user-authorised changes;
- maintain receipts, diagnostics, backups, and support bundles;
- launch approved tools or games only through explicit, separately governed actions;
- support later games without embedding FoA-specific assumptions in the platform core.

The research must determine which of these functions belong in the first product, which belong in later phases, which require a separate privileged or runtime executor, and which must remain prohibited.

This request does **not** authorise redistribution or installation of the commercial game itself. The research must explicitly define the product terminology so that “game installer” cannot be mistaken for permission to package proprietary game files.

---

## 2. Blocked decisions the research must resolve

Implementation is stopped until the research provides defensible answers to the following decisions:

1. Should the new platform extend `FOA-SDK-Installer.exe`, sit beside it as a separate application, consume it as one managed component, or live in a separate reusable repository/package?
2. What is the exact boundary between:
   - generic installer/setup core;
   - SDK product lifecycle;
   - game-specific integration;
   - storefront discovery;
   - prerequisite/component acquisition;
   - mod/package management;
   - deployment execution;
   - runtime adapters;
   - launch control;
   - diagnostics and evidence?
3. Can the existing per-user MSI remain the sole owner of FOA-SDK product files while a higher-level bootstrapper coordinates other components?
4. What architecture allows additional games to be added without recompiling or weakening the trusted core?
5. Should game integrations be declarative manifests, compiled adapters, signed plug-ins, out-of-process providers, or a constrained combination?
6. How are installation identity, ownership, provenance, version compatibility, receipts, backups, and rollback represented durably?
7. How should the platform handle operations that need elevation when the existing SDK installer deliberately runs per-user as `asInvoker`?
8. What is the smallest useful first release that is genuinely multifunctional but does not silently acquire deployment or runtime authority?
9. What exact FoA install types, storefronts, versions, runtime routes, modding dependencies, directory structures, launch paths, and compatibility signals can be supported from defensible evidence?
10. Which current FOA-SDK contracts can be reused and which require new governance, schemas, systems, tests, and release gates?

---

## 3. Known repository facts and non-negotiable constraints

The researcher must begin from these repository facts and verify them against the pinned baseline rather than replacing them with generic installer assumptions.

### 3.1 Product and engine boundary

- FOA-SDK is the product repository. O3DE is a separately pinned external authoring host; the target game remains an external Unity runtime.
- Generated builds, installers, caches, logs, screenshots, evidence, and packages remain outside the source checkout.
- The editor, installer, and declarations do not silently acquire gameplay, deployment, save, signing, publication, catalogue-mutation, or evidence-promotion authority.

### 3.2 Existing installer contract

- `FOA-SDK-Installer.exe` is a self-contained Windows x64 front door that verifies one reviewed embedded MSI.
- Windows Installer is the lifecycle authority for FOA-SDK product files, registration, repair, upgrade, and uninstall.
- The current package is per-user and does not require administrator elevation.
- The existing installer explicitly excludes game discovery, game mutation, FoA launch, runtime-adapter deployment, prerequisite installation, automatic updating, telemetry, signing claims, and proprietary game content.
- Workspaces, generated output, diagnostics, game files, and deployment roots are external data and are not installer-owned.

### 3.3 Existing Tool Wizard contract

- The Tool Wizard records an external workspace and optional O3DE Editor, Unity Editor, Unity conversion project, and Tainted Grail install paths.
- It validates paths and records readiness previews in `%LOCALAPPDATA%\FOA-SDK\ToolWizard\tool-profile.local.json`.
- Conversion execution and deployment execution remain disabled.
- Current FoA path validation is deliberately shallow and is not sufficient evidence for broad compatibility or deployment authority.

### 3.4 Runtime and game-file boundary

- Proprietary Fall of Avalon files, private installations, saves, credentials, signing material, and extracted commercial content are protected external data.
- Mono and IL2CPP are separate runtime-adapter package families. Their binaries, dependencies, compatibility evidence, generated interop, and runtime proof are not interchangeable.
- An adapter declaration or successful review gate does not itself authorise process launch, installation, injection, mutation, save access, signing, or publication.

### 3.5 Governance consequence

A multifunctional multi-game platform is a significant architectural, security, persistence, dependency, installer, deployment, and release change. Research may recommend an architecture, but implementation requires a separately reviewed design and owner-authorised scope.

---

## 4. Definitions the research must settle

The final research must provide unambiguous definitions for:

- **Platform core** — trusted generic orchestration and lifecycle functionality shared by all games.
- **Product installer** — the authority that owns installed application files, such as the existing FOA-SDK MSI.
- **Bootstrapper** — an application that resolves, verifies, sequences, and reports multiple product/component operations.
- **Setup Wizard** — first-run and repeatable configuration of tools, workspaces, game profiles, and optional capabilities.
- **Game integration** — bounded target-specific discovery, identity, compatibility, configuration, and operation rules.
- **Component** — a separately versioned, licenced, verifiable installable unit.
- **Deployment** — any write, replacement, removal, backup, restore, or configuration change affecting a game or mod target.
- **Runtime adapter** — a separately governed component that can interact with the running game after exact validation and permission.
- **Profile** — an exact game/storefront/build/runtime/tooling context, not merely a display name.
- **Receipt** — the durable record of what the platform owns, changed, verified, backed up, and can reverse.
- **Repair** — restoration of platform-owned state without deleting unmanaged user or game content.
- **Rollback** — typed reversal of an exact attempted operation using verified pre-change evidence.

The research must recommend public-facing product terminology that avoids implying that FOA-SDK redistributes or owns Tainted Grail.

---

## 5. Required research areas and questions

### A. Product scope and user journeys

Define the intended users and the complete end-to-end journeys for:

1. a new mod author with the game installed but no SDK or tools;
2. a new user without the game installed;
3. an existing FOA-SDK user upgrading to the new platform;
4. a user with multiple game installations or storefront copies;
5. a user with existing unmanaged mods or loaders;
6. a user operating offline from a complete verified bundle;
7. a user whose previous operation was interrupted;
8. a user repairing, rolling back, or uninstalling;
9. a future user selecting a second supported game;
10. an advanced user running unattended or scripted setup.

For every journey, identify user decisions, required permissions, network activity, writes, evidence produced, failure paths, recovery, and what remains external or unmanaged.

Classify every proposed function as one of:

- required in the first release;
- supported later through an established contract;
- optional integration;
- explicitly prohibited.

### B. System ownership and architecture

Compare at least these architecture families without assuming one is correct:

1. expand the current WinForms executable into the complete platform;
2. preserve the current SDK installer and add a separate generic bootstrapper/setup application;
3. create a reusable installer core/CLI with a thin Windows UI and game adapters;
4. use a WiX Burn-style bundle/bootstrapper around the existing MSI and separately packaged components;
5. use another justified architecture discovered from primary evidence.

For each option, assess:

- trust boundary and blast radius;
- ability to preserve MSI ownership;
- multi-game extensibility;
- testability and headless automation;
- accessibility and UI maintainability;
- dependency and supply-chain exposure;
- elevation model;
- transactional behaviour and rollback;
- update model;
- offline support;
- code signing and SmartScreen implications;
- failure isolation;
- migration from the existing installer;
- repository ownership and release complexity.

The recommendation must name the owner for each major operation and state which current component remains unchanged.

### C. Windows packaging, bootstrapper, and application technology

Using current official documentation, compare the viable Windows technology stack, including where relevant:

- existing self-contained .NET Windows Forms delivery;
- WiX Toolset MSI and Burn capabilities;
- CPack/WiX interaction;
- WPF, WinUI 3, Windows App SDK, or another justified UI host;
- MSIX/App Installer suitability and restrictions;
- self-contained .NET deployment and native dependencies;
- a native or Rust-based core only if evidence shows a material advantage;
- signed update feeds, full-version replacement, delta packages, and rollback strategy.

The analysis must establish current supported versions, licences, maintenance status, Windows 10/11 support, accessibility implications, single-file/offline behaviour, elevation behaviour, and CI/build reproducibility. Do not select a stack because it is fashionable or because a proof of concept is easy.

### D. Multi-game integration model

Design and compare safe integration models. The research must answer:

- Which behaviour can be declarative and which requires executable code?
- Can untrusted third parties add game support without gaining arbitrary code execution inside the trusted installer?
- Should executable adapters run out of process with a versioned protocol and constrained permissions?
- How are adapter identity, publisher, version, compatibility, capabilities, minimum core version, hashes, signatures, licences, and provenance represented?
- How are adapters installed, updated, disabled, revoked, and removed?
- How does the core reject unknown fields, schema downgrades, path escapes, duplicate identities, capability escalation, or incompatible adapters?
- How are game-specific UI pages contributed without allowing the adapter to control lifecycle or security decisions?
- What stable SDK must exist for a second game integration?

The final proposal must include a draft capability and ownership model, schema boundaries, lifecycle hooks, and failure semantics. It must not create a general-purpose arbitrary-script installer.

### E. Game and storefront discovery

Research a bounded, privacy-preserving discovery model for Windows. Establish when and how the platform may inspect:

- explicit user-selected paths;
- Windows uninstall registration;
- supported storefront manifests and library metadata;
- known executable and data-directory signatures;
- file version metadata;
- content fingerprints where lawful and necessary;
- existing loader or framework markers.

Address:

- multiple storefronts and library locations;
- copied or moved installations;
- multiple branches/builds;
- stale registry entries;
- network shares, removable drives, junctions, symlinks, and case-insensitive path identity;
- non-ASCII and long paths;
- privacy and logging redaction;
- bounded scanning and user consent;
- TOCTOU protection between discovery, validation, and mutation.

Discovery must produce typed evidence and confidence, not silently promote a guessed path into deployment authority.

### F. Prerequisites, tools, and component acquisition

Determine which components the platform may legally and technically acquire, install, or configure. For each candidate component, establish:

- official source and redistribution rights;
- licence and notices;
- exact version and compatibility policy;
- immutable URL or release identity;
- expected size and SHA-256;
- publisher signature availability and verification;
- offline cache behaviour;
- dependency ordering;
- per-user versus per-machine installation;
- unattended install arguments;
- repair and uninstall ownership;
- update and end-of-life policy.

Research must distinguish:

1. components bundled with a reviewed release;
2. components downloaded from an authoritative source;
3. tools the user must install independently;
4. local proprietary or generated inputs that may only be detected and fingerprinted;
5. unsupported or legally unclear components.

No research conclusion may assume that a public download is redistributable.

### G. Transaction, deployment, backup, and rollback model

Produce an implementation-independent transaction model for any future authorised game or mod-target write. It must address:

- exact preflight inventory;
- managed versus unmanaged ownership;
- additions, replacements, removals, conflicts, and in-use files;
- pre-change fingerprints;
- backup location and capacity;
- typed inverse operation for every change;
- atomic staging and publish where possible;
- crash and power-loss recovery;
- resumability and idempotency;
- lock ownership and concurrent instances;
- verification after each step;
- rollback verification;
- partial failure and manual-recovery state;
- preservation of user-created and foreign files;
- explicit confirmation immediately before mutation;
- least-privilege elevation for only the exact operation that requires it.

Compare in-process elevation, a short-lived privileged helper, Windows Installer custom actions, a service, and other viable methods. A persistent privileged service must not be recommended without a compelling threat-modelled necessity.

### H. Updates, repair, migration, and uninstall

Research lifecycle rules for the core, UI, game integrations, SDK MSI, optional components, profiles, receipts, caches, and backups.

Answer:

- Which units version independently?
- How are compatibility constraints resolved?
- How are update channels represented?
- How are update metadata and packages authenticated?
- How are downgrade, replay, freeze, revocation, and compromised-feed attacks prevented?
- Can updates be fully offline?
- What is repaired by MSI, by the platform, by an adapter, or only manually?
- What survives uninstall?
- How are old profiles and receipts migrated or rejected?
- How does a failed core update restore the previous runnable version?
- How are orphaned components and abandoned backups reported without unsafe deletion?

### I. Security, privacy, trust, and legal analysis

Create a threat model covering at minimum:

- malicious or compromised component manifests;
- arbitrary-code execution through adapters or install hooks;
- path traversal, archive extraction, junction, reparse-point, and symlink attacks;
- command-line injection and unsafe process lookup;
- DLL search-order hijacking;
- package substitution and TOCTOU;
- update-feed compromise, replay, and downgrade;
- malicious local game paths;
- archive bombs and resource exhaustion;
- poisoned caches and partial downloads;
- privilege escalation and confused-deputy behaviour;
- deletion of unmanaged files;
- backup disclosure of proprietary or personal data;
- log leakage of usernames, private paths, tokens, or game content;
- unsigned binaries and SmartScreen reputation;
- dependency abandonment or supply-chain compromise.

Define trust levels for repository-owned, publisher-signed, checksum-only, locally generated, user-selected, proprietary, and unknown inputs. State exactly what each trust level may permit.

The research must also identify trademark, EULA, redistribution, API terms, and third-party licence constraints. It must clearly separate technical possibility from legal permission.

### J. UX, accessibility, and information architecture

Research a wizard and management experience that remains usable as functions and supported games expand. The output must cover:

- first-run setup versus ongoing management;
- game selection and per-game dashboards;
- simple path for ordinary users and inspectable advanced mode;
- capability selection without misleading defaults;
- clear separation of “detected,” “validated,” “ready,” “blocked,” “installed,” and “verified”;
- review page showing exact planned changes before execution;
- progress reporting for nested operations;
- cancellation rules and points of no return;
- actionable failure messages;
- repair, rollback, and support workflows;
- keyboard navigation, screen-reader semantics, scaling, high contrast, and localisation;
- avoiding dark patterns around telemetry, launch, updates, or optional components.

Provide wireframes or structured page specifications, not decorative mock-ups alone.

### K. Diagnostics, observability, and support

Define a privacy-safe evidence and diagnostics model:

- structured operation IDs and timestamps;
- machine-readable result envelopes;
- redacted human logs;
- component and adapter versions;
- exact operation plan and receipts;
- package hashes and signature results;
- Windows Installer logs where applicable;
- recovery state and next action;
- support bundle creation with user preview and explicit consent;
- no proprietary game files, credentials, saves, or unnecessary absolute paths.

Establish how automated readiness tests can consume the same contracts as the UI without claiming runtime proof.

### L. Performance, reliability, and scale

Set measurable budgets for:

- cold start;
- discovery duration and scan bounds;
- memory use;
- manifest and receipt size;
- hashing throughput and when hashing is justified;
- download concurrency and bandwidth controls;
- disk-space preflight;
- staging and backup amplification;
- UI responsiveness;
- operation recovery after forced termination;
- support for many components, profiles, and future games.

Require deterministic benchmarks and machine context for any performance claim. Avoid hidden full-disk scans, repeated hashing, unbounded directory walks, or synchronous UI-thread I/O.

### M. FoA first-game integration

Produce a dedicated FoA integration study based on public primary sources, exact repository evidence, and only separately authorised local observations.

It must establish or explicitly leave unresolved:

- lawful Windows distribution channels and storefront identities to support;
- exact install-location evidence available for each channel;
- executable, data-directory, version, and Unity-runtime identity signals;
- currently relevant Mono and IL2CPP routes and how the platform distinguishes them;
- exact game build compatibility rules and unsupported-state behaviour;
- BepInEx, Tainted Framework, generated interop, and other prerequisite ownership and redistribution constraints;
- official or community toolkit integration boundaries, including Merlin's Workshop where relevant;
- existing mod-loader and mod-directory detection;
- safe profile creation from a user-selected install;
- what can be validated read-only;
- what future setup or deployment actions would require a separate executor and exact-install evidence;
- launch method and arguments, only if launch is justified and separately gated;
- update behaviour that can invalidate profiles or deployed components;
- uninstall and rollback expectations;
- synthetic fixtures needed to test the adapter without committing game files.

The repository currently records distinct Mono and IL2CPP package routes. The research must treat those records as version-specific evidence, not as permanent universal compatibility claims.

---

## 6. Mandatory comparative benchmarking

Study current, maintained examples from the following categories using official documentation, source repositories, and release records where available:

- Windows bootstrapper and installer frameworks;
- game/mod setup tools such as Wabbajack-style reproducible installers;
- profile-based mod managers such as Mod Organizer-style systems;
- Nexus/Vortex or successor application architecture where publicly documented;
- Thunderstore/r2modman-style game-adapter and package models;
- game launchers that separate core lifecycle from per-game integrations;
- package managers with signed metadata, content-addressed caches, transactions, and rollback.

The purpose is to extract proven design patterns and failure lessons, not to copy branding, proprietary behaviour, or unsupported assumptions. Record whether each observation is documented, inferred from source, reported by maintainers, or merely observed by users.

---

## 7. Mandatory scenarios and adverse cases

The recommendation must be tested conceptually against at least these scenarios:

1. clean supported Windows machine;
2. no game present;
3. one valid FoA installation;
4. two valid installations from different channels;
5. unsupported or stale game version;
6. uncertain Mono/IL2CPP identity;
7. existing unmanaged loader and mods;
8. invalid, moved, junction-backed, or partially deleted path;
9. no administrator rights;
10. exact operation requiring elevation;
11. offline installation from a verified bundle;
12. interrupted download;
13. corrupted cache or package;
14. operation cancelled before mutation;
15. process terminated during mutation;
16. target file locked by the game or antivirus;
17. insufficient disk space during staging or backup;
18. repair after deliberate product-file damage;
19. rollback after partial deployment failure;
20. uninstall while preserving workspaces and unmanaged content;
21. core update with an incompatible game integration;
22. revoked or compromised component;
23. second game added without FoA-specific core changes;
24. non-ASCII user and install paths;
25. Windows long-path and case-collision conditions.

For each, state expected UI, operation state, writes, evidence, recovery, and exit code semantics.

---

## 8. Research method and source standard

### Source priority

1. Official Microsoft, WiX, .NET, O3DE, Unity, storefront, GitHub, BepInEx, and relevant tool documentation.
2. Authoritative source repositories, schemas, release notes, security advisories, and maintainer statements.
3. FOA-SDK repository records pinned to exact commit and path.
4. Reproducible experiments using synthetic fixtures.
5. Community reports only as secondary evidence, clearly labelled and corroborated where possible.

### Required source-register fields

For every load-bearing source, record:

- title;
- publisher/maintainer;
- direct URL or repository path;
- document version, release, tag, or commit;
- publication/update date where available;
- retrieval date;
- exact claim supported;
- relevant quotation or precise paraphrase;
- limitations or conflicts;
- licence or terms relevance.

Current versions, support status, game builds, tool releases, licences, and storefront behaviour are time-sensitive and must be reverified at research time.

### Evidence discipline

Keep these categories visibly separate:

- proven repository fact;
- proven external fact;
- direct experiment result;
- inference;
- recommendation;
- unresolved unknown;
- rejected assumption.

Do not present source inspection, compilation, package creation, or a preview command as proof that a game integration installs, launches, loads, deploys, repairs, or rolls back correctly.

### Protected-data rule

Do not scan, extract, decompile, copy, hash broadly, modify, launch, or publish data from a private FoA installation during this research unless the repository owner grants explicit permission for the exact path and experiment. Where local proof is required, specify a controlled experiment plan and the evidence it must return.

---

## 9. Required research-agent output

The completed research must deliver one coherent decision package containing:

1. **Executive decision memo** — the recommended product definition and architecture, with a direct answer to every blocked decision.
2. **Terminology and scope table** — exact meaning of installer, bootstrapper, setup, component, game integration, deployment, adapter, profile, receipt, repair, and rollback.
3. **Current-state map** — existing FOA-SDK installer, Tool Wizard, runtime-adapter, workspace, package, deployment, and release boundaries.
4. **Source register** — durable primary-source references and repository bindings.
5. **Market/pattern comparison** — relevant existing tools and the lessons that apply or do not apply.
6. **Architecture options matrix** — scored criteria, trade-offs, risks, migration cost, and rejected alternatives.
7. **Recommended system architecture** — system context, component diagram, trust boundaries, process boundaries, and ownership table.
8. **Multi-game integration contract** — proposed manifests/protocols, capabilities, lifecycle, versioning, validation, and extension security.
9. **FoA integration dossier** — supported evidence, storefront/install discovery, profile identity, runtime routes, prerequisites, compatibility, unknowns, and experiments still required.
10. **Operation state machine** — planning, consent, acquisition, verification, staging, elevation, mutation, verification, rollback, completion, cancellation, and recovery states.
11. **Persistence model** — schemas and migration strategy for settings, profiles, components, receipts, operation journals, caches, backups, and diagnostics.
12. **Security threat model** — assets, actors, attack surfaces, abuse cases, mitigations, residual risk, signing, update trust, privilege, and privacy.
13. **Legal and redistribution matrix** — each component class, source, licence, rights, bundling status, downloading status, and unresolved legal question.
14. **UX specification** — principal journeys, page flow, state vocabulary, wireframes, accessibility, review and recovery experience.
15. **Testing and evidence matrix** — unit, contract, malformed-input, security, migration, installer, UI, interruption, recovery, performance, clean-machine, exact-install, and manual gates.
16. **Performance and reliability budgets** — measurable thresholds and benchmark plans.
17. **Migration plan** — how existing `FOA-SDK-Installer.exe`, MSI identity, Tool Wizard profile, user workspaces, and current evidence are preserved or migrated.
18. **Phased implementation sequence** — independently reviewable slices, explicit stop conditions, dependencies, and the first safe vertical slice.
19. **Risk and unknowns register** — unresolved facts, stale evidence, contradictions, decisions needing maintainer authority, and prohibited shortcuts.
20. **Recommended next researched stop/process** — the exact design-review or experiment gate that should follow, or a statement that implementation authority still does not exist.

The output must be sufficiently precise for a subsequent design document and implementation issue. It must not bury the recommendation inside a broad essay.

---

## 10. Acceptance criteria for the research

The research is acceptable only when:

- every blocked decision is answered directly or explicitly marked unresolved with the exact missing evidence;
- a single recommended architecture is named and compared against credible alternatives;
- generic core, SDK lifecycle, FoA integration, deployment executor, runtime adapter, and launch authority have distinct owners;
- the existing MSI lifecycle is either preserved with a clear integration path or replaced only with compelling, source-backed migration evidence;
- the proposal supports a second game without embedding FoA-specific paths, names, loaders, runtime assumptions, or UI into the core;
- no game installation is mutated merely because it was detected;
- all writes are planned, consented, contained, attributable, journalled, verified, and reversible where claimed;
- unmanaged files are preserved by default;
- elevation is least-privilege, bounded, and auditable;
- update and component metadata have a defensible authenticity and anti-downgrade model;
- legal redistribution status is proven per component rather than inferred;
- FoA compatibility claims bind to exact versions and evidence;
- Mono and IL2CPP remain independent where the evidence requires it;
- protected game files are not committed or redistributed;
- security, privacy, accessibility, offline use, repair, rollback, and interrupted-operation recovery are first-class requirements;
- the first implementation slice has explicit tests, evidence, performance limits, rollback, and stop conditions;
- the final document distinguishes research evidence from implementation and runtime authority.

---

## 11. Explicit exclusions from this research task

Do not:

- implement or modify installer code;
- produce an installer executable;
- modify the current MSI identity or packaging pipeline;
- deploy BepInEx, Tainted Framework, adapters, mods, or generated interop;
- launch FoA;
- inspect or change saves;
- redistribute game or proprietary toolkit files;
- add telemetry;
- create signing keys or claim signed-release trust;
- publish a release;
- support Linux, macOS, Steam Deck, consoles, or mobile in the first architecture unless only noting future portability consequences;
- treat a checksum as publisher authentication;
- treat a detected path as a validated profile;
- treat a successful build as exact-install or runtime proof;
- use broad system scans when explicit selection or bounded authoritative discovery is sufficient;
- turn the extension model into unrestricted script execution.

---

## 12. Repository path map for the researcher

Use the exact pinned baseline when recording repository observations.

| Repository path | Highlighted area | Why it matters |
| --- | --- | --- |
| `README.md` | Product identity, external O3DE boundary, installer ownership, architecture boundary | Establishes FOA-SDK as the product and forbids silent runtime/deployment authority |
| `AGENTS.md` | Research-first gate, branch/PR rules, document authority, completion truth | Controls all repository handoff and prevents implementation without authority |
| `CURRENT_TASK.md` | Current unrelated active task | Must not be overwritten or treated as authority for this new installer research |
| `DECISIONS.md` | Repository state and research authority decisions | Requires implementation to stop when authority is missing |
| `GOVERNANCE.md` | Significant changes and architecture invariants | Makes this a design-review and maintainer-approval matter |
| `CONTRIBUTING.md` | Significant-change questions, security, data, tests | Defines required review considerations; note any stale product-identity wording rather than silently resolving it |
| `docs/protected-files-policy.md` | Protected external data | Prevents unauthorised reads/writes of private game installs, saves, and proprietary material |
| `Research/README.md` | Research authority and source rules | Confirms that research does not grant implementation or runtime authority |
| `docs/systems/SYSTEM_INDEX.md` | Current `installer`, runtime, deployment, diagnostics, and release systems | Used to classify ownership and determine whether new system keys are required |
| `docs/tainted-grail-sdk/ARCHITECTURE.md` | Core invariants, layer model, workspaces, persistence, runtime adapters | Governs separation of knowledge, permission, deployment, and runtime action |
| `docs/tainted-grail-sdk/WINDOWS_INSTALLER_AND_ARTIFACT_WORKFLOW_DESIGN.md` | Approved SDK-only installer pipeline and explicit exclusions | Current installer authority that must not be enlarged accidentally |
| `Installer/README.md` | User flow, lifecycle authority, acceptance, boundaries | Defines current installer behaviour and evidence expectations |
| `Installer/Launcher/Windows/README.md` | CLI, Tool Wizard, security controls, readiness smoke | Defines the current executable and automation contract |
| `Installer/Launcher/Windows/InstallerWizardForm.cs` | SDK lifecycle UI and post-install handoff | Shows current coupling and likely migration surface |
| `Installer/Launcher/Windows/ToolSetupWizardForm.cs` | Workspace/tool/game-path setup flow | Shows the existing non-mutating setup experience |
| `Installer/Launcher/Windows/ToolSetupProfile.cs` | Profile schema, path validation, readiness flags | Candidate migration input; not a general multi-game schema |
| `docs/tainted-grail-sdk/RELEASE_PROCESS.md` | Package, deployment, execution-result, signing, and release gates | Prevents preview or metadata from being mistaken for execution authority |
| `docs/tainted-grail-sdk/LEGAL_AND_CONTENT_POLICY.md` | Redistribution, proprietary content, fixtures, trademarks | Controls bundling, acquisition, testing, and branding |
| `SECURITY.md` | Threats, path containment, silent mutation, dependencies | Minimum security baseline for the platform threat model |
| `Plugins/RuntimeAdapters/README.md` | Adapter separation and authority limits | Prevents setup logic from absorbing runtime execution |
| `Plugins/RuntimeAdapters/Mono/README.md` | Exact current Mono profile and external execution gate | Version-specific evidence for the FoA integration study |
| `Plugins/RuntimeAdapters/IL2CPP/README.md` | Independent IL2CPP route and local interop boundary | Version-specific evidence that cannot be satisfied by Mono assumptions |

GitHub base URL for pinned references:

`https://github.com/theb0yys/FOA-SDK/blob/e9beb347ae02835cb851bcd79e41b1bd2c60a909/`

---

## 13. Handoff back to FOA-SDK

- **Consuming gate:** significant-change design review covering installer, persistence, security, dependencies, game integration, deployment, runtime boundary, and release governance.
- **Primary owner classification:** `installer`, with required cross-review from `workspace-and-packs`, `schemas-and-persistence`, `permissions-and-risk`, `external-toolchain`, `deployment-review`, `runtime-adapter-contracts`, `diagnostics`, `test-harness`, and `release-governance`.
- **Evidence-pack fields:** research authority, impact classification, compatibility, persistence/migration, test gaps, performance budgets, dependency and licence review, build/artifact plan, security, protected data, runtime proof, and blocked gates.
- **Next action only if research proves authority:** create a separately reviewed architecture/design record defining the approved product boundary and the smallest implementation slice. No implementation follows directly from this brief alone.
