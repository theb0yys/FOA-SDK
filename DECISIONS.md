# Durable Decisions

Durable architecture and process decisions live here. Active work belongs in `CURRENT_TASK.md`.

## Repository State and Current Owner Instruction

- **Decision:** Merged repository state is the durable source of truth. The repository owner's current explicit instruction controls the scope and authorized transitions of the current task.
- **Constraint:** Current instructions cannot convert unexecuted validation into a pass, static evidence into runtime proof, or unsupported facts into verified facts.
- **Status:** Accepted.

## FOA-SDK Repository Identity

- **Decision:** FOA-SDK is the product repository and is not an O3DE source fork. O3DE is a separately pinned upstream dependency identified by `o3de.lock.json`.
- **Decision:** Source and generated output remain separate: product checkout, external O3DE checkout, and external build/evidence output.
- **Status:** Accepted.

## P0 Progressive-Rigor Engineering Process

- **Decision:** `docs/tainted-grail-sdk/ENGINEERING_PROCESS.md` is the single repository engineering workflow.
- **Decision:** Changes are classified as Routine, Significant, or Critical/Runtime before implementation.
- **Decision:** Validation is selected from `CI_AND_LOCAL_VALIDATION.md` according to the changed surface and risk. Evidence from an unrelated layer is neither required nor accepted as a substitute.
- **Decision:** Routine implementation inside accepted architecture does not require a design ceremony, research-sentinel stack, Deep Research brief, skill-plan script, performance plan, evidence pack, or pre/post deep-review checklist unless the specific task actually needs that evidence.
- **Decision:** Significant changes require a short reviewed design or durable decision. Critical/Runtime changes require explicit threat/operational boundaries and exact applicable proof.
- **Rationale:** Rigor should increase with consequence. The previous process duplicated the same authority and validation concepts across multiple mandatory stacks and obstructed ordinary implementation.
- **Status:** Accepted by repository owner for P0 review.

## Research Escalation

- **Decision:** Research is required when consequential implementation claims depend on unknown external, proprietary, compatibility, licence, game-runtime, native-identity, deployment, save, signing, publication, or security facts.
- **Decision:** Repository-known implementation work does not become a research task merely because an agent performs it.
- **Decision:** Research, decompilation/static evidence, host execution, and live runtime evidence remain separate evidence lanes.
- **Status:** Accepted.

## Context-Only Process Port

- **Previous decision:** The Waning Realm agent operating model was ported to FOA-SDK without behavioral redesign.
- **Status:** **Superseded by P0 Progressive-Rigor Engineering Process.**
- **Reason:** FOA-SDK now owns a process designed around its actual product, risk classes, and validation surfaces.

## Capability Execution Contract and Shared Production Spine

- **Decision:** FOA-SDK retains its existing capability assessment, planning, build-manifest, package-preview, deployment-preview/work-order, result-evidence, verification, reconciliation, and release metadata services as the control plane. Side effects enter only through separately reviewed execution/runtime boundaries governed by `docs/tainted-grail-sdk/CAPABILITY_EXECUTION_CONTRACT.md`.
- **Decision:** The shared production path is `Build -> Package -> Deploy -> Launch -> Verify`; domain systems do not invent private parallel production spines.
- **Decision:** Existing inert V1 contracts remain inert. Executable behavior versions forward rather than flipping existing false authority flags.
- **Decision:** Support, qualification, environment readiness, policy, human authorization, execution outcome, assessment, and evidence promotion remain distinct.
- **Status:** Accepted architecture.

## Capability Execution M0 Authority for M1 Core Contracts

- **Decision:** `docs/tainted-grail-sdk/CAPABILITY_EXECUTION_M0_IMPLEMENTATION_AUTHORITY.md` defines the bounded M1 Additive Core Contracts implementation scope.
- **Repository state:** M0 merged to `main` through pull request #235 on 15 August 2026 at `1b39fa63ea63e527c4f634b79898c9bda5172f87`.
- **Decision:** M1 remains limited to the paths and product boundary recorded by that architecture decision unless the repository owner explicitly changes the current implementation scope.
- **Status:** Effective architecture authority; no longer pending maintainer merge.
