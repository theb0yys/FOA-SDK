#!/usr/bin/env python3

from pathlib import Path

root = Path(__file__).resolve().parents[3]

# Actor/Troop design: current implemented vertical slice and remaining acceptance gate.
p = root / "docs/tainted-grail-sdk/ACTOR_TROOP_EDITOR_DESIGN.md"
s = p.read_text(encoding="utf-8")
old = """Status: active implementation — Core contracts/database, schema-2 migration/persistence, Framework
evidence-bound candidate publication, Core/Framework population-authoring test-source wiring, the immutable
population action-lane contract, and the Actor and Troop Editor pane lifecycle are implemented. The
deterministic synthetic fixture and complete local-validation integration are next. Source implementation and
test wiring do not claim that an exact-head compiled test run, Windows UI review, or the complete vertical
slice has passed."""
new = """Status: implemented vertical slice — Core contracts/database, schema-2 migration/persistence, Framework
evidence-bound candidate publication, production-linked population tests, the immutable population action-lane
contract, Actor and Troop Editor lifecycle, deterministic synthetic fixture, full local-validation integration,
and public release-readiness documentation are implemented. Exact-head O3DE configure/build, compiled Catalog
test execution, and the real Windows twenty-four-pane evidence pass remain the active acceptance gate."""
if old not in s:
    raise RuntimeError("Actor/Troop design status block does not match the reviewed source")
s = s.replace(old, new)
old = """7. **Next** — deterministic synthetic population fixture and full vertical-slice local-validation integration;
8. remaining public user documentation, changelog, and twenty-three-pane checklist updates;
9. exact-head configure/build, compiled tests, and Windows UI evidence.

Completion of units 1–6 establishes durable population contracts, persistence, Framework authoring commands,
positive/negative Core and Framework population-authoring test sources, the immutable action-lane contract,
and the registered Actor and Troop Editor pane. It does not claim that the compiled tests have run in an
exact-head configured build, that Windows UI evidence exists, or that the deterministic fixture and complete
vertical-slice validation have passed.

Unit 7 owns the deterministic population fixture and full local-validation integration. Unit 8 owns the
remaining public user guide, changelog, and twenty-three-pane checklist updates."""
new = """7. **Complete** — deterministic synthetic population fixture and full vertical-slice local-validation integration;
8. **Complete** — public user, architecture/data-format, release-readiness, changelog, and twenty-four-pane
   evidence-checklist updates;
9. **Active acceptance gate** — exact-head O3DE configure/build, compiled Catalog tests, and Windows UI evidence.

Completion of units 1–8 establishes the implemented Actor/Troop vertical slice: durable population contracts,
persistence, Framework authoring commands, positive/negative production-linked tests, immutable action lanes,
the registered Editor pane, deterministic project-owned fixture, validator integration, and public documentation.
It does not claim that compiled tests have run in an exact-head configured build or that Windows UI evidence exists.

Unit 9 owns the remaining exact-head host and real Windows twenty-four-pane acceptance evidence."""
if old not in s:
    raise RuntimeError("Actor/Troop implementation sequence does not match the reviewed source")
s = s.replace(old, new)
p.write_text(s, encoding="utf-8")

# Catalog validator: enforce current implemented state and reject obsolete wording.
p = root / "Gems/TaintedGrailModdingSDK/Tools/validate_catalog_schema2.py"
s = p.read_text(encoding="utf-8")
old = '''    require_fragments(
        actor_design,
        (
            "Status: active implementation",
            "actor/troop contracts, reflection",
            "CatalogDatabase validation, queries",
            "schema-1 migration, schema-2-only writing",
            "5. **Complete** — Core and Framework positive/negative population-authoring test sources",
            "compiled-target wiring",
            "6. **Complete** — immutable population action-lane derivation",
            "Actor and Troop Editor pane",
            "7. **Next** — deterministic synthetic population fixture",
            "do not claim that",
            "loaded candidate remains schema 1",
            "direct save is refused",
            "successful bound replacement and `BuildDocument`",
        ),
        "Actor/troop implementation status",
    )
    require_order(
        actor_design,
        (
            "5. **Complete** — Core and Framework positive/negative population-authoring test sources",
            "compiled-target wiring",
            "6. **Complete** — immutable population action-lane derivation",
            "7. **Next** — deterministic synthetic population fixture",
        ),
        "Actor/troop implementation sequence",
    )'''
new = '''    require_fragments(
        actor_design,
        (
            "Status: implemented vertical slice",
            "actor/troop contracts, reflection",
            "CatalogDatabase validation, queries",
            "schema-1 migration, schema-2-only writing",
            "5. **Complete** — Core and Framework positive/negative population-authoring test sources",
            "compiled-target wiring",
            "6. **Complete** — immutable population action-lane derivation",
            "Actor and Troop Editor pane",
            "7. **Complete** — deterministic synthetic population fixture",
            "8. **Complete** — public user, architecture/data-format, release-readiness",
            "9. **Active acceptance gate** — exact-head O3DE configure/build",
            "twenty-four-pane",
            "does not claim that compiled tests have run",
            "loaded candidate remains schema 1",
            "direct save is refused",
            "successful bound replacement and `BuildDocument`",
        ),
        "Actor/troop implementation status",
    )
    reject_fragments(
        actor_design,
        (
            "Status: active implementation",
            "7. **Next** — deterministic synthetic population fixture",
            "twenty-three-pane checklist",
        ),
        "Actor/troop implementation status",
    )
    require_order(
        actor_design,
        (
            "5. **Complete** — Core and Framework positive/negative population-authoring test sources",
            "compiled-target wiring",
            "6. **Complete** — immutable population action-lane derivation",
            "7. **Complete** — deterministic synthetic population fixture",
            "8. **Complete** — public user, architecture/data-format, release-readiness",
            "9. **Active acceptance gate** — exact-head O3DE configure/build",
        ),
        "Actor/troop implementation sequence",
    )'''
if old not in s:
    raise RuntimeError("Schema-2 Actor/Troop status validator does not match the reviewed source")
s = s.replace(old, new)
old = '''    require_fragments(
        docs_hub,
        (
            "Actor and Troop Editor Design",
            "ACTOR_TROOP_EDITOR_DESIGN.md",
            "completed Core, schema-2 persistence, Framework candidate-publication, population-authoring test-source",
            "immutable action-lane, and registered Actor/Troop pane units",
        ),
        "Documentation hub",
    )'''
new = '''    require_fragments(
        docs_hub,
        (
            "Actor and Troop Editor Design",
            "ACTOR_TROOP_EDITOR_DESIGN.md",
            "approved population design and implementation history",
            "deterministic fixture",
            "registered Actor/Troop pane",
            "exact-head O3DE configure, build, compiled tests",
        ),
        "Documentation hub",
    )
    reject_fragments(
        docs_hub,
        (
            "completed Core, schema-2 persistence, Framework candidate-publication, population-authoring test-source",
        ),
        "Documentation hub",
    )'''
if old not in s:
    raise RuntimeError("Schema-2 documentation-hub validator does not match the reviewed source")
s = s.replace(old, new)
p.write_text(s, encoding="utf-8")

# Mutation fixture/tests: lock the current implemented state.
p = root / "Gems/TaintedGrailModdingSDK/Tools/tests/test_validate_catalog_schema2.py"
s = p.read_text(encoding="utf-8")
start = s.index('        self._write(\n            "docs/tainted-grail-sdk/ACTOR_TROOP_EDITOR_DESIGN.md",')
end = s.index('        self._write(\n            "ROADMAP.md",', start)
block = '''        self._write(
            "docs/tainted-grail-sdk/ACTOR_TROOP_EDITOR_DESIGN.md",
            "Status: implemented vertical slice\\n"
            "actor/troop contracts, reflection\\n"
            "CatalogDatabase validation, queries\\n"
            "schema-1 migration, schema-2-only writing\\n"
            "4. **Complete** — Framework evidence-bound authoring, atomic troop-definition bootstrap\\n"
            "5. **Complete** — Core and Framework positive/negative population-authoring test sources "
            "and compiled-target wiring\\n"
            "6. **Complete** — immutable population action-lane derivation, "
            "Actor and Troop Editor pane, and lifecycle registration\\n"
            "7. **Complete** — deterministic synthetic population fixture and local validation\\n"
            "8. **Complete** — public user, architecture/data-format, release-readiness documentation and twenty-four-pane checklist\\n"
            "9. **Active acceptance gate** — exact-head O3DE configure/build and compiled tests\\n"
            "does not claim that compiled tests have run\\n"
            "loaded candidate remains schema 1\\n"
            "direct save is refused\\n"
            "successful bound replacement and `BuildDocument`\\n",
        )
'''
s = s[:start] + block + s[end:]
start = s.index('        self._write(\n            "docs/tainted-grail-sdk/README.md",')
end = s.index('\n\n    def test_valid_contract_passes', start)
block = '''        self._write(
            "docs/tainted-grail-sdk/README.md",
            "Actor and Troop Editor Design ACTOR_TROOP_EDITOR_DESIGN.md "
            "approved population design and implementation history with schema-2 persistence, "
            "deterministic fixture, and registered Actor/Troop pane; exact-head O3DE configure, "
            "build, compiled tests, and Windows evidence remain mandatory\\n",
        )'''
s = s[:start] + block + s[end:]
s = s.replace(
    '    def test_valid_contract_passes(self) -> None:\n    def test_valid_contract_passes(self) -> None:\n',
    '    def test_valid_contract_passes(self) -> None:\n',
)
marker = '    def test_rejects_stale_roadmap_future_status(self) -> None:\n'
if 'def test_rejects_fixture_regressed_to_next_work' not in s:
    insert = '''    def test_rejects_fixture_regressed_to_next_work(self) -> None:
        path = self.repo_root / "docs/tainted-grail-sdk/ACTOR_TROOP_EDITOR_DESIGN.md"
        text = path.read_text(encoding="utf-8").replace(
            "7. **Complete** — deterministic synthetic population fixture",
            "7. **Next** — deterministic synthetic population fixture",
        )
        path.write_text(text, encoding="utf-8")
        with self.assertRaisesRegex(
            CatalogSchema2ContractError,
            r"7\\. \\*\\*Complete|7\\. \\*\\*Next",
        ):
            validate_catalog_schema2(self.repo_root)

    def test_rejects_docs_hub_without_current_population_status(self) -> None:
        path = self.repo_root / "docs/tainted-grail-sdk/README.md"
        text = path.read_text(encoding="utf-8").replace(
            "registered Actor/Troop pane",
            "Actor/Troop pane planned",
        )
        path.write_text(text, encoding="utf-8")
        with self.assertRaisesRegex(CatalogSchema2ContractError, "Documentation hub"):
            validate_catalog_schema2(self.repo_root)

'''
    s = s.replace(marker, insert + marker)
p.write_text(s, encoding="utf-8")

for path in (
    root / "Gems/TaintedGrailModdingSDK/Tools/validate_catalog_schema2.py",
    root / "Gems/TaintedGrailModdingSDK/Tools/tests/test_validate_catalog_schema2.py",
    root / "docs/tainted-grail-sdk/ACTOR_TROOP_EDITOR_DESIGN.md",
):
    text = path.read_text(encoding="utf-8")
    if not text.endswith("\n"):
        path.write_text(text + "\n", encoding="utf-8")

print("Applied current Actor/Troop schema-2 validation state repair.")
