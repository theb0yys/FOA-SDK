# Canonical Interchange Gate 5 Design Decisions

Status: normative addendum to the proposed Gate 5 design; pending explicit maintainer acceptance; no implementation authority

Repository baseline: `eb840862c9d3e239dec91770495c7669c00d10df` (`main`), observed 22 July 2026

Parent design: `CANONICAL_INTERCHANGE_GATE_5_DESIGN.md`

This addendum closes exact token, optional-property, and issue-code details discovered during design review. Where a
spelling or rule in the parent design conflicts with this addendum, this addendum controls.

## Exact JSON enum tokens

C++ enum member names remain idiomatic PascalCase. JSON tokens retain the hyphenated spellings accepted by the
research conclusion:

```text
PackageKindV1: authoring-interchange
HostKindV1: o3de, blender, unity-editor, host-neutral
BindingStateV1: unresolved, active, stale, conflicted, superseded, retired
MappingOperationV1: rename, move, duplicate, fork, replace, merge, split, aggregate, tombstone
MappingCompletenessV1: incomplete, complete
PayloadRoleV1: canonical-document, scene-source, mesh-source, texture-source, material-map,
               skeleton-source, animation-source, collider-source, metadata, preview, licence-notice
TransformationPhaseV1: authoring, export, import, normalization, reverse-export, migration
ReversibilityV1: reversible, conditionally-reversible, irreversible, unknown
LossSeverityV1: information, warning, error, blocker
SourceKindV1: project-owned, synthetic, user-supplied-reviewed, user-supplied-unreviewed,
              local-reference-only, upstream-reference, prohibited
RedistributionStateV1: allowed, local-only, review-required, blocked
IssueSeverityV1: information, warning, error, blocker
MigrationStatusV1: already-current, source-invalid, unsupported-source-version,
                   unsupported-target-version, downgrade-forbidden, migrator-unavailable,
                   semantic-drift, succeeded
```

The fixed basis token values are:

```text
handedness: right-handed
right_axis: +x
forward_axis: +y
up_axis: +z
linear_unit: metre
angular_unit: radian
time_unit: second
vector_convention: column-vector
multiplication_convention: matrix-on-left
storage_order: column-major
```

No implementation may substitute underscore spellings or silently accept aliases.

## Optional property canonicalization

`display_name` remains optional as accepted by the research conclusion:

- an empty C++ value means the property is absent;
- a present property must contain one or more valid UTF-8 bytes and remain within the presentation-string limit;
- the canonical writer omits the property when the value is empty;
- the writer emits it in its fixed position immediately after `pack_id` when present;
- a parser normalizes no Unicode and does not convert absence into an emitted empty string.

Other optional scalar properties follow the same omit-when-empty rule only where the public JSON Schema marks them
optional. Required arrays and required objects are always emitted.

## Exact intrinsic Core issue codes

The first Gate 5 Core validator may emit only the following intrinsic codes:

| Precedence | Code | Default severity | Meaning |
| ---: | --- | --- | --- |
| 0 | `interchange.schema.unsupported-version` | blocker | schema version is not exactly supported |
| 0 | `interchange.schema.unsupported-profile` | blocker | schema or canonical profile is not accepted |
| 0 | `interchange.schema.unknown-field` | error | a closed object contains an unknown field |
| 0 | `interchange.schema.forbidden-field` | blocker | an authority, command, credential, runtime, or other prohibited field appears |
| 1 | `interchange.canonical.bom-forbidden` | error | input begins with a UTF-8 BOM |
| 1 | `interchange.canonical.duplicate-key` | error | a JSON object contains a duplicate property name |
| 1 | `interchange.canonical.invalid-json` | error | JSON syntax or trailing content is invalid |
| 1 | `interchange.canonical.invalid-string` | error | UTF-8 or field grammar is invalid |
| 1 | `interchange.canonical.size-exceeded` | error | canonical input or a bounded string exceeds its limit |
| 1 | `interchange.canonical.depth-exceeded` | error | JSON nesting exceeds `MaxCanonicalDepthV1` |
| 1 | `interchange.canonical.non-finite-number` | error | a numeric value is NaN or infinite |
| 1 | `interchange.canonical.negative-zero` | error | supplied verification bytes use noncanonical negative zero |
| 1 | `interchange.canonical.order-invalid` | error | canonical object or array order is wrong |
| 2 | `interchange.identity.invalid` | error | an ID, digest, or revision grammar is invalid |
| 2 | `interchange.identity.duplicate` | error | an identity occurs more than once in its scope |
| 2 | `interchange.fingerprint.revision-mismatch` | error | a declared semantic revision differs from the computed projection |
| 3 | `interchange.path.absolute` | error | a path is absolute or uses a drive, UNC, URI, or device prefix |
| 3 | `interchange.path.traversal` | error | a path contains an empty, `.` or `..` segment or escapes the package model |
| 3 | `interchange.path.case-collision` | error | two paths collide under the declared Windows equivalence key |
| 3 | `interchange.path.windows-alias` | error | a path uses a reserved name, trailing dot/space, or alias form |
| 4 | `interchange.reference.missing` | error | a typed record references an absent subject or record |
| 4 | `interchange.dependency.missing` | error | a payload dependency does not resolve |
| 4 | `interchange.dependency.cycle` | error | the declared payload dependency graph contains a cycle |
| 4 | `interchange.payload.limit-exceeded` | error | payload count, individual size, or declared package total exceeds Schema-1 limits |
| 5 | `interchange.identity.mapping-invalid` | blocker | lineage mapping cardinality, completeness, revision, or reference rules fail |
| 5 | `interchange.binding.invalid` | error | binding shape, host, native kind/value, or state is invalid |
| 5 | `interchange.binding.unresolved-required` | blocker | a required consumer binding is not active |
| 5 | `interchange.binding.conflict` | blocker | competing active bindings exist for the same binding key |
| 5 | `interchange.binding.supersession-invalid` | error | supersession is missing, self-referential, non-older, or cyclic |
| 6 | `interchange.transform.sequence-invalid` | error | transformation sequence is missing, duplicate, or unordered |
| 6 | `interchange.transform.matrix-invalid` | blocker | matrix size, finiteness, basis, direction, or fixed convention fails |
| 6 | `interchange.transform.parameter-invalid` | error | typed parameter discriminator and active value are inconsistent |
| 6 | `interchange.transform.tolerance-invalid` | error | a tolerance is negative, non-finite, unit-inconsistent, or duplicated |
| 6 | `interchange.loss.incomplete` | error | a loss lacks required subject, reason, result, reversibility, or references |
| 6 | `interchange.loss.blocker` | blocker | a blocker-level loss prevents progression |
| 7 | `interchange.provenance.incomplete` | blocker | a material subject lacks complete provenance |
| 7 | `interchange.licensing.invalid` | error | licence expression/reference, redistribution state, or required binding is invalid |
| 7 | `interchange.licensing.noassertion-blocked` | blocker | the declared use exceeds a `NOASSERTION` or review-required state |
| 7 | `interchange.content.prohibited` | blocker | prohibited content is referenced or contained |
| 8 | `interchange.lock.incomplete` | blocker | the exact toolchain lock lacks a required component or digest |
| 8 | `interchange.extension.unapproved` | blocker | the namespace is not admitted by Schema 1 |
| 8 | `interchange.extension.digest-mismatch` | error | extension canonical bytes do not match the declared digest |
| 9 | `interchange.fingerprint.package-declaration-mismatch` | error | the declared package-fingerprint input is inconsistent |
| 9 | `interchange.migration.source-invalid` | blocker | migration source bytes do not form a valid canonical source document |
| 9 | `interchange.migration.unavailable` | blocker | no accepted adjacent migration is compiled |
| 9 | `interchange.migration.semantic-drift` | blocker | migration changes semantics without complete mapping, transformation, and loss records |

The intrinsic validator does not emit warning-only advisory strings. Validity is a pure binary result derived from
whether the ordered issue vector is empty. `IsBlocked()` is true when any issue has blocker severity.

## Reserved non-Core issue codes

These codes remain part of the public cross-layer vocabulary but are not emitted by the first Core implementation:

```text
interchange.path.symlink
interchange.payload.unlisted
interchange.payload.missing
interchange.payload.digest-mismatch
interchange.payload.size-mismatch
interchange.lock.mismatch
interchange.evidence.binding-mismatch
interchange.package.partial
```

Framework or host validation may emit them later but may not downgrade or clear a Core issue.

## Deterministic ordering

Issues sort by:

1. precedence number above;
2. exact code bytes;
3. exact subject ID bytes;
4. exact property-path bytes;
5. lexicographically sorted related IDs.

Duplicate issue identities collapse to one record. Validation never uses traversal order, hash-container order,
locale, clock, filesystem order, or pointer identity.

## Implementation consequence

The implementation PR must reproduce this addendum in:

- enum `ToString` and `TryParse` tests;
- fixed-basis validation;
- JSON Schema enum values;
- canonical golden bytes;
- validator code constants and precedence tests;
- public documentation.

Any mismatch is a release blocker and requires design review rather than an implementation-only spelling change.

## Permanent non-authority statement

This addendum grants no implementation or operational authority. It introduces no service, persistence, filesystem,
provider, process, Blender or Unity operation, Asset Processor mutation, game access, runtime adapter, deployment,
launch, save access, evidence promotion, signing, archive, upload, publication, or compatibility claim.