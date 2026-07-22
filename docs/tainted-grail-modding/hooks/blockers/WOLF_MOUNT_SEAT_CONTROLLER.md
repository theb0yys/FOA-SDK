# Research Blocker — Wolf Mount Seat Frame Controller

```yaml
blocker_id: tg.blocker.mounts.wolf-seat-frame-controller
status: research-blocker
domain: mounts-movement-input-ownership-and-cleanup
source_repository: theb0yys/Tainted-Grail-The-Fall-of-Avalon-mods
source_commit: d7e740e7f167b73152b53409e483dab07d80d048
source_path: mods/avalon-mounts/src/WolfMount/WolfMountSeatBridgeController.cs
source_blob: c226dead88329eb55f3444f36c6fceecc2ed7e66
supporting_paths:
  - mods/avalon-mounts/src/Plugin.cs
  - mods/avalon-mounts/src/Safety/DiagnosticsOnlyGuard.cs
  - mods/avalon-mounts/src/WolfMount/WolfNativeMountContract.cs
supporting_blobs:
  - 896e0f4e63c57b1800eb3ac09468cefe0ca8509d
  - 5609bcff8082d362bdf6356a224d01e291e88f20
  - 65a52e309da76d5bfd9b5627ee92def688031070
primary_disposition: research
```

## Decision

The controller is not promoted to a supported adapter or lifecycle hook. Its frame loop is simple at the top level, but it owns entry into a large runtime conversion contract whose mutation and rollback surfaces are not transactionally proven.

The strict configuration gate is a source fact. It is not safety evidence for the operations performed after the gate passes.

## Controller scheduling and actions

`Plugin.Update` calls `Tick(_guard.WolfMountSeatBridgeAvailable)` every frame.

When the gate is false, the controller detaches an active mount and returns. When true, it:

1. calls `WolfNativeMountContract.TickObservation(Time.deltaTime)` when the contract exists;
2. validates the current attachment and detaches if invalid;
3. polls the configured toggle shortcut every frame;
4. detaches on a toggle while attached;
5. otherwise attempts attachment.

Dialogue execution can call the same attach/detach lane through `ToggleFromCommand`.

## Candidate selection

Attachment materializes `World.All<NpcHeroPetAlly>().ToArraySlow()`, then filters and collects locations that:

- are not discarded;
- resolve through a reflected parent-location property;
- match the hard-coded wolf template GUID or name;
- are `MarkedNotSaved`;
- still contain a managed `NpcHeroPetAlly`.

With multiple candidates and an available hero, it orders by squared distance and selects the nearest.

This scan happens on attach attempts rather than every frame, but its world-size, allocation, scene-transition, identity, and competing-controller behavior are unverified.

## Downstream native mount contract

The controller lazily creates `WolfNativeMountContract`. The selected supporting source can:

- locate a loaded native mount contract source and reuse its `MountData`;
- create GameObjects, transforms, colliders, `CharacterController`, `MountComponents`, `MountElement`, and adapters/observers;
- write private and compiler-generated backing fields by reflection;
- add a mount element to the managed wolf;
- discard `MountAction` and `MountPetAction` elements;
- call `MarkAsHeroMount`, `Mount`, and `Dismount`;
- capture and modify `Hero.OwnedMount` through reflection;
- disable NPC movement behaviours using type-name suffix/substring heuristics;
- re-disable those components during mounted frame observations;
- destroy/discard created objects and restore captured component states during cleanup.

## Deterministic source-level risks

1. **Previous-null owned mount is not transactionally restored.** Cleanup writes `Hero.OwnedMount` only when the captured prior value is non-null. A prior null state therefore has no explicit restoration write after runtime ownership changes.
2. **Native actions are discarded, not reconstructed.** The source discards existing `MountAction` and `MountPetAction` elements during conversion and does not show a restoration path that recreates their prior state.
3. **Movement suspension is heuristic.** Components are selected by type-name patterns such as `NpcController`, `NpcMovement`, `RichAI`, and `RootMotion`; false positives and false negatives are possible.
4. **Cleanup spans multiple independent operations.** Model discard, Unity destruction, movement restoration, ownership restoration, dismount, and adapter cleanup are not one rollback transaction.
5. **Private-field writes are version-fragile.** Missing or changed backing fields throw during conversion. The catch path attempts cleanup, but equivalence to the pre-conversion object graph is not established.
6. **Source mount selection is broad.** It chooses among loaded `MountElement` values with mount data, preferring hero mounts and proximity, without an exact approved source identity.
7. **Input and dialogue share one controller state.** Combined shortcut, dialogue, scene unload, gate change, plug-in unload, and external mount changes require race/order testing.
8. **Visual loading assumptions are unverified.** `OnVisualLoaded` is called and the captured transform is returned immediately; callback timing semantics require exact-profile evidence.

## Required resolution

This blocker closes only after:

1. an exact game/runtime/loader/assembly profile is bound;
2. every reflected field/property and model method is signature-verified;
3. a pre/post object-graph inventory proves exact restoration for successful mount, dismount, failed entry, gate loss, scene change, and plug-in unload;
4. prior-null and prior-non-null owned-mount cases are covered;
5. original mount/pet actions are preserved or transactionally reconstructed;
6. movement-component selection uses reviewed identities and restores exact enabled states;
7. all created Unity and model objects have deterministic ownership and destruction evidence;
8. world-scan, frame-observation, and adapter allocations are bounded;
9. compatibility is tested with mount, follower, companion, camera, movement, input, teleport, save, and scene-lifecycle modifications;
10. save and persistence effects are explicitly proven or prohibited.

## Permissions

Allowed: documentation, source review, synthetic state-machine fixtures, offline object-graph and rollback design.

Prohibited: game launch, runtime mount conversion, object creation, component disabling, private-field mutation, ownership mutation, save access, deployment, or evidence promotion.
