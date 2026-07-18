# Developer Preview Fixture

This directory owns the deterministic Developer Preview 0 fixture contract.

The fixture contains only project-owned synthetic data in the reserved `preview.*` and `subject:preview:*` namespaces. It does not contain proprietary game data, extracted assets, decompiled code, private paths, saves, credentials, native game identifiers, runtime adapters, deployment files, or telemetry.

`Template/` is the canonical generator input. Its workspace, pack, source, evidence, and catalog documents are plain schema-1 JSON matching the public data-format documentation. The editor persistence services accept those plain objects and existing O3DE `JsonSerialization` envelopes through the same reflected model types. Generated output is not committed a second time.

Generate the fixture:

```powershell
python Gems/TaintedGrailModdingSDK/Tools/developer_preview_fixture.py generate `
  --output build/tg-sdk-developer-preview-0-fixture
```

Verify it without rewriting:

```powershell
python Gems/TaintedGrailModdingSDK/Tools/developer_preview_fixture.py verify `
  --output build/tg-sdk-developer-preview-0-fixture
```

The generated tree contains:

```text
preview.tgworkspace.json
Packs/preview.developer-preview-0.tgpack.json
Input/preview-source.json
Sources/preview.source.synthetic/source.tgsource.json
Sources/preview.source.synthetic/evidence.tgevidence.json
Catalog/catalog.tgcatalog.json
preview-fixture.manifest.json
```

The manifest records the relative path, byte size, and SHA-256 digest of every payload. Generation is byte-for-byte deterministic and refuses to overwrite unrelated, partial, modified, or symlink-containing output.

The fixture demonstrates:

- one portable workspace and placeholder profile;
- one runtime-disabled synthetic pack;
- one structured source document and eight evidence records;
- five canonical synthetic records;
- resolved `crafted_at` and `learned_from` relationships;
- one explicitly unresolved `learned_from` relationship;
- validation and governance histories;
- allowed, forbidden, blocked, stale, and unresolved states;
- two item profiles, one recipe profile, one ingredient, and one output.

The compiled Developer Preview service-level smoke loads these plain schema documents through the real persistence services, saves them to a temporary workspace, discards the in-memory state as a close-equivalent step, reopens the saved envelope documents, and verifies canonical state equivalence. It also proves that current proof-backed allowed usages survive reload while unproven allowances fail closed.

This service-level load, save, close-equivalent, and reopen proof does not replace the later manual Editor UI pass or prove runtime safety, game compatibility, deployment, or a distributable preview archive.
