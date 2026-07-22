# Schema-1 migration contract

Schema profile: `foa-interchange-schema-v1`

Canonical profile: `foa-interchange-canonical-json-v1`

Migration is a pure byte-to-byte Core operation. It receives caller-supplied canonical JSON and a target schema
version, then returns a value object. It has no registry, filesystem, persistence, clock, environment, network,
process, provider, host, runtime, deployment, save, evidence-promotion, signing, or publication authority.

## Schema-1 result matrix

| Source and target | Status | Candidate | Performed |
| --- | --- | --- | --- |
| valid `1 -> 1` | `already-current` | exact source bytes | false |
| invalid Schema-1 source | `source-invalid` | absent | false |
| source above the highest compiled version | `unsupported-source-version` | absent | false |
| target below source | `downgrade-forbidden` | absent | false |
| `1 -> 2` with no accepted adjacent migrator | `migrator-unavailable` | absent | false |
| target beyond the adjacent unavailable version | `unsupported-target-version` | absent | false |
| candidate violates identity/digest/evidence rules | `semantic-drift` | absent | false |

The enum reserves `succeeded` for a future, separately reviewed adjacent migrator. In Schema 1, no code path returns `succeeded`.

## Digest and evidence binding

The result records the SHA-256 digest of the immutable source bytes. A returned candidate records its own digest.
`HasCandidate()` verifies the candidate bytes against that digest; it does not authorize publication.

A valid `1 -> 1` identity route has:

- source and target version 1;
- candidate bytes exactly equal to source bytes;
- equal source and candidate digests;
- no mapping IDs;
- no transformation IDs;
- no loss IDs;
- `migration_performed` false.

Any mismatch produces the blocker `interchange.migration.semantic-drift`. Invalid source bytes produce
`interchange.migration.source-invalid`. An unavailable adjacent route produces
`interchange.migration.unavailable`.

## Future adjacent migrators

A future adjacent migrator must return new candidate bytes by value, explicit migration steps, and every identity,
transformation, or loss consequence. Target validation and semantic-drift checks must pass before `succeeded` can
become reachable. Such a migrator requires separate design and maintainer acceptance.

Candidate presence never grants authority to persist, publish, install, import, execute, promote, sign, deploy, or
certify the candidate.

Gate 6 remains closed.
