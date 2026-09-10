# Open PR integration

Status: all seven PR changes integrated and locally validated.

Scope: the owner explicitly authorized merging PRs #252-#258 and fixing conflicts. The combined source retains Pack Manager draft/exit protection and encounter, faction, world/route and quest authoring. No release, deployment, game/save changes, branch deletion or history rewrite is included.

Classification: Significant integration of existing authoring/persistence surfaces. The focused CI checkout correction is Routine; primary owner validation-and-evidence. Supporting owners: workspace-and-packs, content-pack-authoring, schemas-and-persistence and ui-framework.

Conflicts were confined to CURRENT_TASK. Foundation's pack-save and authoring declarations combine without changing their contracts. Item Viewer CI fetches Git LFS assets after the pinned O3DE source checkout, so it reads the pinned .lfsconfig endpoint and receives real image/icon data. Negative coverage checks missing or retargeted downloads, ordering and failure propagation.

Validation: PASSED combined Core.Static, Framework.Static, Editor, Catalog.Tests and CanonicalInterchange.Tests builds against O3DE pin 68683f23fb747380d3efa2424bd5f30242e9c5a2. Catalog: 464 passed, two explicit Windows symlink skips. Canonical Interchange: 39 passed. Static Python: 822 passed, nine explicit skips from 831 discovered. All four pinned source-policy suites passed 10 checks each. All 16 focused CI regression tests passed. Unchanged engine artifacts were reused; all SDK targets were rebuilt. The original PRs retain their per-feature live Editor evidence; no new combined-tree interactive UI or runtime sign-off is claimed. Hosted jobs retain their actual pending/skipped/passed states; the corrected engine checkout and LFS download succeeded.

Source reconciliation used isolated non-main branches. Other active worktrees and drafts were left alone. GitHub PR records hold the authorized merge outcomes and validation notes. No follow-on feature is authorized by this record.
