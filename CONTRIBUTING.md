# Contributing to the Tainted Grail Modding Editor and SDK

Thank you for helping build FOA-SDK, an unofficial open-source authoring and mod-development platform for **Tainted Grail: The Fall of Avalon**.

FOA-SDK is the product repository. It is **not** an O3DE source fork. The project uses a separately pinned upstream O3DE checkout; see `README.md` and `o3de.lock.json`.
This repository is the FOA-SDK product repository, not an O3DE source fork. Contributions must satisfy both this project's rules and the applicable O3DE licence, source, build, and Developer Certificate of Origin requirements.

## Read before contributing

For most changes, read:

1. [Engineering Process](docs/tainted-grail-sdk/ENGINEERING_PROCESS.md)
2. [Development Guide](docs/tainted-grail-sdk/DEVELOPMENT_GUIDE.md)
3. [Code Quality](docs/tainted-grail-sdk/CODE_QUALITY.md)
4. [CI and Local Validation](docs/tainted-grail-sdk/CI_AND_LOCAL_VALIDATION.md)
5. the architecture/design document for the system being changed
6. [Security Policy](SECURITY.md) when security-sensitive behavior is involved

Automated agents also follow `AGENTS.md`.

## What contributions are welcome

- editor models, services, tools, and user interfaces;
- catalog, validation, evidence, identity, maturity, risk, and permission logic;
- importers and durable data formats;
- authoring and preview workflows;
- external-tool and adapter contracts within their reviewed boundaries;
- build, test, diagnostics, accessibility, reliability, and performance improvements;
- documentation and legally distributable examples or fixtures.

## Contributions that are not accepted

- proprietary game assets or source material without redistribution rights;
- malware, credential theft, destructive payloads, or telemetry without consent;
- silent game-installation or save mutation;
- invented native references, game facts, runtime permission, validation results, or test results;
- display-name-only identity;
- unbounded scanning of a user's machine or installation;
- dependencies with unresolved licence or supply-chain risk.

## Engineering classification

Every change is classified before implementation:

- **Routine** — work inside accepted architecture: bug fixes, tests, internal refactors, build repairs, and ordinary docs.
- **Significant** — new public APIs/subsystems, persistence or schema changes, new dependencies, architecture changes, or substantial build behavior.
- **Critical/Runtime** — process execution, deployment, saves, runtime adapters, signing/publication, permission/security boundaries, or live game-runtime claims.

See `ENGINEERING_PROCESS.md` for the required workflow for each class.

## Contribution lifecycle

### 1. Define the change

State:

- the problem and desired outcome;
- intended files/systems;
- classification;
- explicit out-of-scope behavior;
- validation needed for the changed surface.

Routine changes do not require a ceremonial design document.

### 2. Design when required

Significant and Critical/Runtime changes require a short reviewed design or durable decision covering the affected ownership, compatibility, failure behavior, data/migration implications, and validation plan.

### 3. Implement on a focused branch

Create a non-`main` working branch from the accepted integration state. Keep the diff focused and avoid unrelated cleanup.

### 4. Validate the changed surface

Use the matrix in `CI_AND_LOCAL_VALIDATION.md`. Run focused checks first and add host, UI, runtime, deployment, installer, or release proof only when the change can affect those layers.

Never describe a narrower result as a broader pass.

### 5. Open a pull request

The pull request must state:

- classification;
- summary and scope;
- design/architecture impact when applicable;
- exact validation actually performed;
- compatibility/migration/rollback impact when applicable;
- documentation changes.

### 6. Review and merge

Resolve blocking review findings and failed required checks. A maintainer makes the final merge decision.

## Branch model

- `main` — reviewed integration state.
- focused non-`main` branches — normal implementation units.
- `foa-development` — optional maintainer convenience branch; not a required base and not an authority source.

## Developer Certificate of Origin

Commits require DCO sign-off:

```shell
git commit -s -m "Describe the change"
```

Use concise imperative commit summaries and explain important constraints in the body when needed.

## Code and data requirements

Follow `CODE_QUALITY.md`. In particular:

- public/durable identities are stable and exact;
- persistence formats have explicit versions and migration/rejection behavior;
- file writes stay in owned persistence/execution boundaries;
- UI delegates domain logic to services;
- new dependencies receive licence and maintenance review;
- errors and blockers are actionable;
- user-controlled input is bounded and validated.

## Testing

Testing is change-specific, not universal.

Examples:

- docs/process-only: reviewed-range and targeted static/policy validation;
- Python/tooling: targeted unit tests plus static validation;
- C++ behavior: focused compiled tests for the affected target;
- build graph/Gem integration: configure/build plus focused compiled tests;
- UI: host build plus applicable interaction evidence;
- persistence/schema: malformed input, round trip, migration/rejection, and affected compiled tests;
- runtime/deployment/release: the exact operational evidence defined for that surface.

See `CI_AND_LOCAL_VALIDATION.md` for the authoritative matrix.

## Documentation

Behavior, public contracts, and durable formats must be documented in the same review unit when they change. Do not update unrelated roadmap/history merely to create process churn.

## Security and privacy

Do not place secrets, private paths, personal data, protected game content, signing material, or vulnerability details that increase risk in public repository content. Follow `SECURITY.md`.
