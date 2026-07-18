# Developer Preview Fixture

This directory owns the deterministic Developer Preview 0 fixture contract.

The fixture contains only project-owned synthetic data in the reserved `preview.*` and
`subject:preview:*` namespaces. It does not contain proprietary game data, extracted
assets, decompiled code, private paths, saves, credentials, native game identifiers,
runtime adapters, deployment files, or telemetry.

`Template/` is the canonical generator input. It contains one reviewed copy of each
schema-1 payload. Generated output is not committed a second time. The generator reads
only those canonical UTF-8 JSON files, validates them, copies them byte-for-byte to an
explicit output directory, and creates the SHA-256 manifest.

Generate the fixture:

```powershell
python Gems/TaintedGrailModdingSDK/Tools/developer_preview_fixture.py generate `
  --output build/tg-sdk-developer-preview-0-fixture
```

Verify an existing fixture without rewriting it:

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

`preview-fixture.manifest.json` records the relative path, byte size, and SHA-256 digest
of every payload file. The manifest itself is canonical UTF-8 JSON and is regenerated
from the reviewed template.

Generation is byte-for-byte deterministic. The command refuses to overwrite a non-empty
directory. `--replace` is accepted only when the existing directory first passes complete
manifest, hash, path, schema, identity, ownership, evidence, relationship, governance,
economy, and template-equivalence verification. A modified or unrelated directory is
never overwritten silently.

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

The fixture is research and authoring data only. Successful generation and verification
do not prove Editor load/save/reopen behavior, runtime safety, game compatibility,
deployment, or a distributable preview archive.
