# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Prepare valid synthetic input for the package round trip; no game data is used."""
import argparse
import json
from pathlib import Path
import assets_localisation_fixture


def prepare(output):
    workspace = assets_localisation_fixture.prepare(output)
    path = workspace.parent / "Catalog/catalog.tgcatalog.json"
    catalog = json.loads(path.read_text(encoding="utf-8"))
    # Developer Preview intentionally demonstrates a missing learn-source relationship.
    # Keep that fixture intact and omit only this known negative example in the valid
    # package fixture. Package preview must (and does) reject the original example.
    example = "preview.relationship.recipe.learned-from-unresolved"
    assert sum(row["RelationshipId"] == example for row in catalog["Relationships"]) == 1
    catalog["Relationships"] = [row for row in catalog["Relationships"] if row["RelationshipId"] != example]
    for history in ("GovernanceHistory", "ValidationHistory"):
        catalog[history] = [row for row in catalog[history] if row["SubjectId"] != example]
    path.write_text(json.dumps(catalog, indent=2), encoding="utf-8")
    return workspace


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", required=True, type=Path)
    arguments = parser.parse_args()
    print(prepare(arguments.output))
