# Avalon AI

Avalon AI Runtime already separates V2 contracts, in-memory/Rabbit blackboards, GOAP planning, runtime packages,
observations, execution, and FoA Mono hosts. FOA-SDK should port the runtime-neutral contracts and authoring/
validation logic first. Execution backends, Blaze mappings, and FoA Mono assemblies remain adapter packages.

`AvalonAiExtension` now provides that first compiled authoring layer: API 2.0 version binding, stable package,
role, goal, action, fact, target, capability, and blackboard identities, package-local blackboard declarations,
goal/action planning inputs, deterministic validation, and canonical inert authoring plans. It rejects host
linkage and execution flags. Rabbit, Blaze, Unity lifecycle, ownership migration, observation hosts, and action
dispatch remain excluded runtime surfaces.
