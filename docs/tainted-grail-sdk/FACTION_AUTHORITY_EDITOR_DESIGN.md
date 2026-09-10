# Faction and Authority Editor

Status: implemented; local static, compiled and running Editor authoring acceptance passed on 2026-09-10. Maintainer review pending.

## Ownership and scope

This Significant change is owned by content-pack-authoring. Core owns culture/faction profiles, stable typed faction links and deterministic validation. Foundation owns pack checks, authoring evidence, atomic catalog publication and persistence. The Editor presents saved choices and bounded drafts. Catalog, persistence, workspace and UI owners support those boundaries.

The workflow creates local cultures and factions, assigns actors/troops, describes leadership and authority, edits directed dispositions and jurisdiction plans, and saves/reopens complete faction definitions. Existing game-derived actors are read-only inputs. No native faction importer or game behavior is inferred.

## Contract

Catalog schema 4 adds CultureProfiles, FactionProfiles and FactionLinks. Profiles attach to pack-owned synthetic society/culture and society/faction canonical records. A culture has a description and language description. A faction optionally references a saved culture and has description/authority notes.

FactionLinks are independently identified, evidence-bound associations owned by their source faction. Each has a stable link ID, exact source/target record identity (or an explicitly unverified jurisdiction reference), kind, value and notes. Membership targets saved actors/troops with member/officer/leader roles; at most one leader is allowed and it must be an actor. Disposition targets another saved faction with friendly/neutral/hostile values. Dispositions are directed: a reverse relationship is never invented. Jurisdiction targets a saved world location/scene/region or a bounded unverified reference, with controls/claims/protects intent.

Culture/faction edits require the active saved authoring pack to own the canonical record. References may read other packs' saved data without acquiring write authority. A complete faction save preserves surviving link IDs, removes only omitted links owned by that faction, and rejects moving IDs between factions or changing a link's kind. No actor or troop profile is rewritten by faction membership.

Limits: at most 1,000 cultures, 1,000 factions and 10,000 links per catalog; at most 128 links of each kind in one faction; descriptions and authority notes at most 2,048 bytes; link notes/references at most 1,024 bytes. Inputs reject control characters, malformed IDs, unknown enum values, duplicate identities/targets, self-dispositions, missing/wrong-kind targets, disagreeing exact subjects and cross-owner edits. Empty factions can be saved with a visible warning.

Authoring evidence covers each profile and exact faction-link subject, using the established protected workspace evidence writer and catalog transaction. Existing canonical evidence establishes referenced actors, troops, cultures, factions and world records. Authored descriptions and enum values are SDK planning concepts, not discovered game enums or runtime permissions.

## Compatibility and failure behavior

Schemas 1, 2 and 3 remain validated load-only inputs. A successful save emits schema 4; the existing migration writer preserves and verifies an exact original catalog backup before replacement. Older schemas containing society collections and future versions are rejected. Downgrade requires restoring the pre-migration backup; new society data are not representable by older Editors.

Workspace/pack schemas, canonical interchange, the pinned engine and runtime contracts remain unchanged. Existing economy, population, encounters, canonical identities, evidence and governance history are preserved. Failed input/evidence/persistence leaves published state unchanged. The UI retains failed drafts, protects dirty selection/closing, and refuses saves after workspace/profile changes.

## Validation

Add compiled culture/faction CRUD, membership/role/disposition/jurisdiction, identity/ownership/evidence, deterministic round-trip, schema-1/2/3 migration, malformed/future input, complete-definition removal and failed-write tests. Run the existing catalog/interchange suites, repository/static policy and pinned SDK builds.

Run the actual pane with an isolated authoring workspace: create Town Guard and Bandits, assign a culture and captain/guards, declare leadership/hostility/territory, edit and remove links, save/reopen, reject bad input, preserve dirty drafts and failed writes, and check constrained window size/keyboard access. Budget the measured real-data workflow to a maximum UI timer gap below three seconds; do not infer maximum-catalog performance from compilation.

Acceptance results on 2026-09-10: 827 Python tests discovered (818 passed, nine explicit skips); all enabled pinned source-policy checks passed; changed SDK targets compiled; Catalog tests 445 passed with two Windows symlink-fixture skips; CanonicalInterchange tests 39 passed. Seven live Editor workflow groups passed, including schema-3 backup/preservation and editing after reopening. All tabs were visually reviewed and a floating 860x640 logical-pixel dock retained scrolling, keyboard focus and Save/Revert controls. The maximum measured UI timer gap was 2.172 seconds on a private input catalog with 887 actors, 3,914 items, 356 recipes and one encounter. This does not establish maximum-catalog performance. The normal authoring catalog hash was unchanged.

Keep source observations, copied catalogs, screenshots, logs and binaries outside Git. Runtime, game/save mutation, installer and release proof are not applicable to this authoring slice.
