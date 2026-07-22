# Gate 5 Canonical Interchange Design — Current Main Reconciliation

Status: design reconciliation; no implementation authority

Design baseline: `eb840862c9d3e239dec91770495c7669c00d10df`

Reconciled `main`: `3fb95284f7cbc259a3c4ab0ba4469be0c9d7baaf`

Observed: 22 July 2026

Historical-record note: this file describes the exact main comparison above.
The non-installing Suite Wizard/Bootstrapper prototype mentioned below was later
removed from the current installer implementation and is not a supported or
normative installation path. The approved current delivery path is documented
in `WINDOWS_INSTALLER_AND_ARTIFACT_WORKFLOW_DESIGN.md`.

Comparison:

```text
eb840862c9d3e239dec91770495c7669c00d10df
→ 3fb95284f7cbc259a3c4ab0ba4469be0c9d7baaf
4 commits
```

## Changed lanes

The intervening commits add:

- a separate Mono runtime-adapter package with its own schema, fixtures, tools, validator, and documentation;
- a Suite Wizard receipt-to-execution-handoff request contract under the Installer Bootstrapper lane;
- focused tests and validators for those two packages.

They do not modify:

- `Gem::TaintedGrailModdingSDK.Core.Static` source ownership;
- `DeterministicContractJson` or `CanonicalFingerprint`;
- the Gate 0 external-tool contracts;
- the canonical-interchange research conclusion;
- the Gate 5 design question;
- the implementation gate map;
- the Phase 9 external-authoring-tools status;
- the proposed Gate 5 source, schema, test, or validator layout.

## Runtime boundary

The separate Mono package does not alter Gate 5. It is a runtime-adapter lane with independent schemas and
governance. No Mono type, package, schema, C# source, BepInEx surface, execution gate, result record, or validator may
be included, referenced, or depended on by the Gate 5 Core contract implementation.

The existence of a runtime-adapter package does not grant the interchange contract build, deployment, execution,
game API, runtime mutation, save, or compatibility authority.

## Installer boundary

The receipt execution-handoff package is owned by `Installer/Bootstrapper/`. It is not a canonical-interchange
migration, provider, process supervisor, or native-host contract. Gate 5 must not depend on Installer Python code,
receipt schemas, execution requests, installer confirmations, or installer authority.

## Gate status

The current roadmap still describes the external authoring-tools/interchange lane as:

```text
Gate 0 contract-only precursor in development
Phase 9 provider and execution work proposed
```

No explicit Phase 9/Gate 5 entry acceptance is introduced by the four commits. The pending authority record remains
false for Phase 9 entry, design acceptance, and implementation authorization.

## Validation evidence correction

The design must follow the repository’s actual validation topology:

- GitHub-hosted pull-request validation provides exact-head static, Python, repository-policy, fixture, and
  applicable native smoke evidence;
- exact-head O3DE configure, Core compilation, and compiled C++ tests require the separately approved O3DE build
  environment, currently the Windows exact-head lane;
- the first Gate 5 implementation must prove canonical golden bytes in that exact compiled environment;
- a hosted Linux O3DE compiled result is not required unless such an approved environment exists on the
  implementation head;
- cross-platform canonical-byte confidence is supplemented by public golden-byte fixtures and the standard-library
  repository validator, but documentation must not mislabel static Python comparison as compiled C++ evidence.

Where the proposed design’s checklist refers to hosted Linux and Windows compiled jobs, this reconciliation
controls: hosted static validation plus exact-head compiled O3DE evidence in the approved build environment are the
required first-slice gates.

## Design disposition

The Gate 5 design and its token/issue addendum remain technically applicable after this drift. The implementation
base must still be rechecked when authority is accepted and immediately before the implementation branch is cut.

## Permanent non-authority statement

This reconciliation grants no implementation, service, persistence, filesystem, process, provider, native-host,
runtime, deployment, save, evidence-promotion, signing, archive, upload, publication, compatibility, or release
authority.
