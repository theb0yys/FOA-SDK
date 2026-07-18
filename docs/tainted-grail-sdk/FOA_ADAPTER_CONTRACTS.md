# FoA Adapter Capability Contracts

## Status

Implemented as Correction Slice 8, the first Phase 7 capability slice. This slice defines typed adapter identity, semantic-version declarations, explicit capability declarations, fail-closed compatibility evaluation, and a read-only Editor matrix.

No runtime adapter implementation is included.

## Purpose

The contract foundation answers a planning question without executing anything:

> For each pack, adapter declaration, active runtime target, and capability, are the declared version, capability, catalog permission, validation proof, shared blockers, and adapter evidence sufficient for later planning?

The answer is transient and read-only. It is not a work order, runtime approval, deployment command, save operation, or proof that FoA behavior works.

## Typed adapter identity

An `AdapterDeclaration` contains:

- a lowercase namespaced adapter ID;
- a human-readable display name;
- one strict semantic version;
- one or more exact runtime targets: `Mono` or `IL2CPP`;
- exact adapter-identity evidence IDs;
- unique typed capability declarations and exact capability-evidence IDs.

Declarations live in a **transient registry** owned by Core. They are not persisted into workspace, pack, catalog, or a new adapter document. Closing the editor clears the registry.

## Capability vocabulary

Slice 8 defines these exact capabilities:

- `item_grant`;
- `recipe_learn`;
- `recipe_append`;
- `custom_item_registration`;
- `custom_recipe_registration`;
- `vendor_mutation`;
- `loot_mutation`;
- `reward_mutation`;
- `persistence`;
- `cleanup`;
- `rollback`.

No free-form capability strings are accepted by the typed boundary.

## Semantic-version compatibility

Pack manifests already declare `RequiredAdapterVersion`. Slice 8 does not change the pack schema.

The compatibility policy is intentionally conservative:

- both required and declared values must be strict semantic versions;
- the declared version must be greater than or equal to the required version;
- major versions must match;
- when the major version is `0`, minor versions must also match;
- prerelease precedence follows Semantic Versioning;
- build metadata does not affect precedence.

Malformed or incompatible versions produce `version_mismatch`.

## Permission mapping

Capabilities that affect governed catalog subjects require a current allowed usage and a same-subject proof chain:

| Capability | Required catalog usage |
| --- | --- |
| `item_grant` | `existing_item_grant` |
| `recipe_learn` | `existing_recipe_learn` |
| `recipe_append` | `runtime_recipe_append` |
| `custom_item_registration` | `custom_item_registration` |
| `custom_recipe_registration` | `custom_recipe_registration` |
| `vendor_mutation` | `vendor_or_loot_injection` |
| `loot_mutation` | `vendor_or_loot_injection` |
| `reward_mutation` | `quest_or_contract_reward_injection` |

The existing-native lanes remain distinct from custom registration:

- `item_grant` and `recipe_learn` do not accept synthetic records;
- custom item and recipe registration require synthetic records;
- vendor, loot, and reward readiness requires a matching current and validated canonical acquisition relationship.

`persistence`, `cleanup`, and `rollback` are adapter-level declarations. They do not require a current catalog usage in Slice 8, but they still require adapter identity and capability proof.

## Evidence, proof, and blockers

Adapter identity evidence must use:

```text
adapter:<adapter-id>
```

Capability evidence must use:

```text
adapter:<adapter-id>:capability:<capability>
```

Every evidence record must resolve to a source whose fingerprint, profile ID, game version, branch, and runtime target match the active game profile.

Catalog permissions require:

- an eligible pack-owned catalog record;
- the mapped allowed usage;
- validated and current record state;
- a permission governance event for the same record and usage;
- evidence for that event;
- one or more validation events for the same record;
- exact profile, game version, branch, evidence, and source bindings;
- no open shared blocker for that subject and usage.

Validation never grants permission automatically. A valid permission/proof chain with an applicable open blocker remains fail-closed as `proof_missing`.

## Matrix statuses

The matrix uses one deterministic status per row:

- `unsupported` — no adapter declaration exists, the capability is undeclared, or the active runtime target is unsupported;
- `version_mismatch` — the pack requirement and adapter declaration fail the semantic-version policy;
- `permission_missing` — no eligible pack-owned subject has the required allowed usage;
- `proof_missing` — permission exists without a valid proof chain, adapter identity/capability evidence is missing or invalid, or an applicable open blocker remains;
- `supported` — declaration, runtime, version, identity lane, relationship, permission, proof, and blocker gates all pass.

The evaluation order is exactly:

```text
support → version → permission → proof → supported
```

This ordering prevents a later proof result from hiding a more fundamental unsupported or version-incompatible declaration.

## Editor matrix

**Tainted Grail Adapter Capability Matrix** is read-only. It displays:

- pack and required adapter version;
- declared adapter identity and version;
- active runtime target;
- typed capability;
- compatibility status;
- eligible catalog subjects;
- declaration evidence;
- permission evidence;
- validation proof IDs;
- actionable reasons.

With no registered declaration, every pack receives deterministic `unsupported` rows for all eleven capabilities.

## Build ownership

- `AdapterContractRegistry` and `AdapterCompatibilityService` belong to `TaintedGrailModdingSDK.Core.Static`.
- `AdapterCapabilityMatrixWidget` belongs to the Editor target.
- Tests link the production Framework target and receive Core transitively.

Core contains no Qt, AzToolsFramework, Foundation, persistence, filesystem, process, deployment, or game dependency.

## Runtime boundary

Slice 8 is contract metadata and read-only compatibility analysis only. It cannot execute an adapter, affect a running game, deploy content, launch the game, alter saves, or generate work orders.

## Slice 9 boundary

Slice 9 may generate deterministic work-order **plans only** from reviewed catalog records against these typed contracts. It must not execute those plans. Runtime results, when a later real adapter exists, must return as new evidence rather than silently changing validation or permission.
