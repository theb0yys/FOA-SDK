# FOA-SDK canonical interchange Schema 1

Status: public Gate 5 structural contract.

Schema profile: `foa-interchange-schema-v1`

Canonical profile: `foa-interchange-canonical-json-v1`

This directory publishes the reviewed, closed structural representation of a Schema-1 canonical interchange
manifest. It is intended for third-party tooling that needs to inspect field names, JSON types, required fields,
closed enums, record limits, and example documents without linking the FOA-SDK C++ targets.

## Files

- `manifest.tginterchange.schema.json` is the normative structural JSON Schema.
- `CANONICALIZATION.md` defines the canonical byte and ordering boundary owned by the C++ implementation.
- `MIGRATION.md` defines the pure Schema-1 migration result matrix.
- `examples/minimal-documents/manifest.tginterchange.json` is a synthetic documents-only canonical example.
- `examples/minimal-asset/manifest.tginterchange.json` is a synthetic asset-and-document canonical example.

The JSON Schema is normative for object closure, field names, JSON types, required properties, enum values,
per-record limits, and numeric ranges represented in the schema. The C++ contract remains normative for exact
UTF-8 byte limits, canonical property order, sorting, finite-number formatting, negative-zero handling, path
semantics, cross-reference rules, revision projections, issue precedence, and migration outcomes. A disagreement
between the schema and C++ contract is a release blocker; neither silently overrides the other.

## Validation

Run:

```text
python Gems/TaintedGrailModdingSDK/Tools/validate_canonical_interchange_schema.py
python -m unittest discover -s Gems/TaintedGrailModdingSDK/Tools/tests -p "test_validate_canonical_interchange_schema.py"
```

The validator uses only the Python standard library. It checks the schema, both examples, C++ limits and enum
tokens, canonical writer order, migration vocabulary, documentation boundary, reference integrity, and the
synthetic document/asset revision fingerprints.

## Compatibility and authority

Publishing Schema 1 does not claim ABI stability, native-host compatibility, provider support, Blender support,
Unity support, Asset Processor integration, runtime support, deployment support, game compatibility, save access,
signing, evidence promotion, or publication authority.

Gate 6 remains closed. This package describes data; it does not load, persist, publish, install, import, execute,
promote, sign, deploy, or certify that data.
