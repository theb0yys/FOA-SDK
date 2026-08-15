# FOA-SDK System Index

This registry defines canonical ownership groups and system identifiers used by agent routing, research, testing, evidence, and handoff.

## Foundation

- `foundation-services`
- `catalog-and-identity`
- `workspace-and-packs`
- `schemas-and-persistence`
- `validation-and-evidence`
- `permissions-and-risk`
- `extension-api`
- `artifact-ownership`

## Authoring

- `world-authoring`
- `entity-placement`
- `content-pack-authoring`
- `road-atlas`
- `ui-framework`
- `avalon-ai`
- `medusa-reconstruction`

## Toolchain And Conversion

- `external-toolchain`
- `capability-execution`
- `unity-provider`
- `unity-conversion-project`
- `canonical-interchange`
- `packaging-preview`
- `installer`

## Runtime Integration

- `runtime-adapter-contracts`
- `bepinex-mono-adapter`
- `bepinex-il2cpp-adapter`
- `deployment-review`
- `runtime-evidence`
- `execution-receipts`
- `runtime-verification`

## Integrations

- `merlin-workshop`
- `acquisition-providers`
- `third-party-tool-providers`

## Support

- `diagnostics`
- `documentation`
- `test-harness`
- `release-governance`

System names are governance keys, not display labels. A system entry establishes a classification target; it does not by itself authorise implementation or runtime action.

`capability-execution` owns the shared preview/execute lifecycle and Build -> Package -> Deploy -> Launch -> Verify spine. `artifact-ownership` owns immutable artifact identity and custody records. `execution-receipts` and `runtime-verification` own observations only; they do not promote evidence, grant permission, or approve release.
