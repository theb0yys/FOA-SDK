# Tainted Grail Modding Infrastructure

This folder is the shared infrastructure boundary for systems used by the Tainted Grail Modding SDK and Editor.

## Modules

- `TaintedFramework/` - framework-level contracts and composition.
- `TaintedCore/` - shared domain primitives and core services.
- `TaintedUI/` - reusable Editor UI components and presentation contracts.
- `Exceptions/` - error types, diagnostics, and failure-reporting contracts.
- `AI/` - AI-facing contracts and integrations approved by project research.

The folders are scaffolding only. Implementations must remain evidence-gated: each system needs a documented contract, ownership boundary, dependencies, and validation requirements before it is added to a build target.

## Dependency direction

`TaintedCore` is the lowest shared layer. `TaintedFramework`, `TaintedUI`, `Exceptions`, and `AI` may depend on explicit core contracts, but core code must not depend on UI or AI integrations. Cross-module behavior must be exposed through documented interfaces rather than direct implementation coupling.

## Editor integration

The Modding SDK may consume these modules after their contracts and build integration are documented. This scaffold does not change the current Gem targets or runtime behavior.
