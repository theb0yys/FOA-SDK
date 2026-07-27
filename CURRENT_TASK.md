# Current Task

This file records the active FOA-SDK task state so work can continue without treating conversation history as authority.

## Goal

- Port the Waning Realm repository agent operating model into FOA-SDK.
- Preserve process behaviour exactly.
- Make contextual substitutions only for FOA-SDK, O3DE, Unity, and Tainted Grail: The Fall of Avalon.

## Files Currently Involved

- `AGENTS.md`
- `CURRENT_TASK.md`
- `DECISIONS.md`
- `.codex/`
- `docs/protected-files-policy.md`
- `docs/systems/SYSTEM_INDEX.md`
- `.github/PULL_REQUEST_TEMPLATE.md`
- `.github/CODEOWNERS`

## Constraints

- No Bannerlord or TAOM-specific context may remain.
- No process weakening, redesign, simplification, expansion, or unrelated cleanup is allowed.
- Existing FOA-SDK architecture and authority boundaries remain controlling.
- Work must remain on a non-`main` branch and enter `main` only through maintainer-audited pull request.
- Runtime mutation, silent deployment, save modification, signing, publication, catalog mutation, and evidence promotion remain prohibited.

## Completed Work

- Source governance/process architecture inspected.
- FOA-SDK governing documents read.
- Working branch created: `governance/foa-sdk-context-port`.

## Remaining Work

- Port the complete root Codex pack and behavioural evaluations.
- Validate parity and absence of TAOM/Bannerlord context.
- Open a pull request for maintainer audit.

## Do Not Change

- Product implementation source.
- Existing FOA-SDK architecture boundaries.
- Runtime, deployment, save, signing, publication, or evidence-promotion authority.
- Unrelated repository files.

## Next Concrete Step

- Create the context-only FOA-SDK process pack and run its structural validator.
