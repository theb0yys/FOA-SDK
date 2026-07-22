# Canonicalization contract

Schema profile: `foa-interchange-schema-v1`

Canonical profile: `foa-interchange-canonical-json-v1`

The public JSON Schema describes structure. Exact canonical bytes are produced and checked by
`CanonicalInterchangeCanonical` and `CanonicalInterchangeValidation`.

## Encoding and lexical rules

- Input and output are UTF-8 without a byte-order mark.
- Presentation strings are preserved as supplied; NFC or other Unicode normalization is forbidden.
- JSON object keys are unique.
- Unknown properties are rejected because every Schema-1 object is closed.
- Control characters, quotation marks, and reverse solidus characters use the canonical JSON escapes.
- Non-ASCII UTF-8 is emitted as UTF-8, not rewritten into byte-wise escape sequences.
- Numbers are finite shortest round-trip decimal values.
- `-0` and negative-zero floating values canonicalize to `0`.
- Exponents use lowercase `e`, omit a positive sign, and omit redundant leading zeroes.
- No insignificant whitespace is emitted.

## Top-level property order

Properties are emitted exactly in this order. `display_name` is omitted when it is empty.

```text
schema_version
schema_profile
canonical_profile
package_kind
package_id
pack_id
display_name
producer
intended_consumers
toolchain_lock
spatial_basis
temporal_basis
documents
assets
identity_mappings
bindings
payloads
transformations
losses
provenance
licensing
evidence_refs
extensions
```

Required arrays are emitted even when empty.

## Collection ordering

Set-like collections are copied, sorted, and deduplicated without mutating caller input. Important stable keys are:

- intended consumers: host kind, consumer ID, consumer version, profile ID;
- toolchain components: component kind, component ID, version, revision, digest;
- documents, assets, bindings, payloads, provenance, licensing, and evidence: their canonical IDs;
- extensions: namespace then schema version;
- subject/reference/ID/notices arrays: lexical value order;
- parameters: parameter name;
- tolerances: subject property path, comparison norm, unit, scope.

Identity mappings, transformations, losses, and their explicit sequence fields retain caller-defined sequence
semantics. Validation rejects non-canonical collection order or sequence inconsistencies.

## Fingerprints

A SHA-256 digest is lowercase hexadecimal without a prefix.

- canonical manifest digest: SHA-256 over exact canonical manifest bytes;
- declared package input: canonical manifest bytes plus relative-path-sorted declared payload tuples;
- document revision: the Schema profile, document semantics, referenced semantic payload, subject references, and
  asset IDs;
- asset revision: asset semantics, referenced semantic payloads, recalculated document revisions, relevant
  transformations and losses, and admitted extensions.

Schema 1 admits no extension namespace. A non-empty extension envelope is structurally representable for
fail-closed validation but is not admitted for asset semantic fingerprinting.

The two examples are compact canonical JSON. Their revision fingerprints are calculated from the projections above
and are checked by the repository validator.

## Purity boundary

Canonicalization reads only caller-supplied values and bytes. It does not query the filesystem, clock,
environment, registry, network, process state, provider state, Blender, Unity, O3DE Editor state, Asset Processor,
runtime, deployment, saves, evidence stores, signing state, or publication state.

Gate 6 remains closed.
