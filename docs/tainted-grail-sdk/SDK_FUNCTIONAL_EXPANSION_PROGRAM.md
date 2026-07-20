# FOA SDK Functional Expansion Program

Status: proposed program direction

Target: Phase 6 domain-authoring expansion after the governed foundation and release-evidence baseline

## Decision requested

Approve a sustained Phase 6 product-expansion program that turns the current governed foundation into a broad,
usable mod-authoring SDK instead of continuing to add isolated release-status surfaces as the primary
development focus.

The program establishes the ordered domain sequence, shared implementation rules, acceptance standard, and
review boundaries for the remaining authoring capabilities. The first implementation target is the separately
specified **Actor and Troop Editor** vertical slice.

This program does not waive focused design review for later tools that introduce a new durable schema,
runtime-facing behavior, save behavior, process execution, deployment, cryptographic operations, external
dependencies, or a materially different security boundary.

## Why the development focus changes now

The repository has a mature foundation for:

- workspace and exact game-profile management;
- pack ownership and compatibility declarations;
- source and evidence intake;
- canonical catalog records and relationships;
- validation, governance, staleness, permission, prohibition, and blockers;
- transactional persistence;
- one developed authoring domain for items and recipes;
- typed adapter, build, package, deployment-preview, and release-evidence contracts.

That foundation is intentionally broad, but the user-facing authoring surface remains narrow. Most mod
creators need to work across actors, troops, encounters, factions, world locations, quests, state, assets, and
localisation. The SDK should now spend its primary development capacity converting the shared foundation into
complete vertical authoring workflows.

## Product outcome

A technically capable mod author should eventually be able to use one source-built O3DE-hosted SDK to:

1. establish a workspace, exact FoA profile, and owned pack;
2. import and review evidence;
3. create or promote canonical identities;
4. author typed content across the supported domains;
5. inspect references and cross-domain relationships;
6. see validation, permissions, prohibitions, and blockers beside every operation;
7. save, close, reopen, and reproduce the same canonical state;
8. generate reviewed non-executable plans and handoffs;
9. validate and package project-owned output without confusing editor intent with runtime authority.

## Ordered capability sequence

### 1. Actors and troops

Deliver typed actor profiles, troop profiles, troop membership, evidence-backed template binding,
lifecycle-neutral planning data, action-lane visibility, durable persistence, and the **Tainted Grail Actor
and Troop Editor**.

This is the first vertical slice because actors and groups are prerequisites for encounters, factions, routes,
quests, rewards, and world population.

### 2. Spawns and encounters

Build on exact actor and troop identities to describe spawn candidates, encounter compositions, placement
references, density, uniqueness, activation conditions, cleanup requirements, and rollback planning.

The editor may author and validate plans. It must not spawn an actor, modify a scene, launch FoA, or invoke a
runtime adapter.

### 3. Factions and authority

Add typed faction, culture, authority, membership, disposition, and jurisdiction data. Keep faction identity
distinct from actor identity and treat changing runtime disposition or membership as separately permissioned
adapter work.

### 4. World, roads, and routes

Add regions, scenes, locations, roads, route nodes, route edges, travel constraints, and world references.
Exact scene and location identifiers remain evidence-backed; display labels never become identity keys.

### 5. Quest and state inspection

Add typed quest and state-key inspection first, followed by reviewed overlay authoring. Read permission and
mutation permission remain separate. No quest advancement, state mutation, save write, or story execution
occurs in the editor.

### 6. Assets and localisation

Add project-owned asset declarations, source/cooked references, addresses, bundles, icons, models, audio,
localisation keys, language variants, provenance, redistribution state, and compatibility checks.

The SDK must not import copyrighted game assets into the repository or infer redistribution rights.

### 7. Cross-domain planning

Once the domain tools exist, extend adapter planning and build manifests with typed actor, encounter, world,
quest, asset, and localisation payloads. Existing fail-closed capability, permission, validation, evidence,
and blocker rebuilding remains mandatory.

### 8. Extension and automation surface

After the first-party domain contracts stabilize, expose bounded importer extension points, headless
validation, public schema packages, deterministic fixtures, and documented third-party adapter examples.

## Vertical-slice completion standard

A domain is not considered implemented merely because a model, pane, or menu item exists. Every vertical slice
must include the applicable parts of this stack:

- typed Core contracts with deterministic validation;
- exact catalog identity and relationship binding;
- source/evidence checks;
- governance and blocker integration;
- Framework authoring services;
- candidate-before-publish transactional persistence;
- durable schema versioning and migration or explicit rejection;
- a usable Editor workflow;
- service, persistence, malformed-input, and negative tests;
- deterministic synthetic fixtures;
- focused repository validation;
- public user, architecture, data-format, and development documentation;
- Windows UI coverage for the accepted exact head;
- explicit runtime, deployment, save, privacy, and legal boundaries.

## Shared implementation architecture

### Core

Core owns typed domain models, intrinsic validation, deterministic queries, relationship analysis, action-lane
derivation, and blocker inputs. Core must not depend on Qt, filesystem mutation, process launch, network
access, runtime libraries, or private game assemblies.

### Framework

Framework owns evidence-bound authoring services, persistence orchestration, schema migration, candidate
database transactions, active workspace/profile/pack checks, and change publication through
`FoundationService`.

### Editor

Editor widgets remain thin views. They select canonical records, collect bounded typed input, call Framework
services, display errors and blockers, and react to foundation notifications. They do not own canonical state
or implement domain validation rules.

### Runtime adapters

Runtime adapters remain separate. A domain editor can describe a candidate action and its requirements, but it
cannot grant runtime permission, call FoA APIs, patch the game, mutate a save, place content, or perform
cleanup.

## Shared domain rules

Every new domain tool must:

- attach typed profiles to existing canonical records rather than inventing a private identity store;
- preserve exact native references and pack ownership;
- use first-class catalog relationships for independently governed associations;
- require evidence for authoring fields and joins where the current economy services require evidence;
- keep validation, permission, risk, staleness, and maturity independent;
- use deterministic ordering and canonical persistence;
- reject unknown, duplicate, conflicting, or cross-profile references;
- publish state only after all required durable writes succeed;
- remain reproducible after save, close-equivalent reset, and reopen;
- expose no optimistic runtime action when proof is missing.

## Common authoring UX

The remaining first-party editors should share a predictable layout:

- canonical-record browser and filters;
- typed profile form;
- relationship or membership table;
- evidence selector and evidence detail;
- validation and blocker summary;
- read-only action-lane matrix;
- save/revert controls with actionable errors;
- no hidden runtime or deployment button.

Accessibility requirements include keyboard traversal, clear labels, non-colour-only status communication,
readable error text, and stable control ordering.

## Program-level non-goals

This program does not itself authorise:

- FoA process launch;
- BepInEx or Harmony execution;
- Unity or game API calls;
- actor spawning or encounter activation;
- quest or world-state mutation;
- save editing;
- deployment or rollback execution;
- automatic game-install discovery;
- telemetry or automatic upload;
- copyrighted or proprietary fixtures;
- an unreviewed generic scripting console;
- a second plugin loader outside O3DE Gems.

## Review and delivery model

Development remains on `foa-development`. Each major domain starts with a focused design record when required
by the review policy, then proceeds through reviewable vertical commits. One slice should reach durable,
tested usability before the next dependent slice begins.

The preferred order within a slice is:

1. contracts and schema decision;
2. Core validation and database integration;
3. Framework authoring and persistence;
4. migration and round-trip tests;
5. Editor workflow;
6. validators and deterministic fixtures;
7. documentation and Windows UI evidence;
8. exact-head validation and merge-readiness review.

## Program acceptance

The expansion program is successful when the SDK is no longer an economy editor surrounded by planning and
release evidence, but a coherent multi-domain authoring environment whose actor, population, world, narrative,
asset, and localisation workflows all use the same governed foundation.
