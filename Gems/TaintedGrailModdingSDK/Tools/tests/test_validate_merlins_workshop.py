#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

from __future__ import annotations

import importlib.util
import json
import shutil
import sys
import tempfile
import unittest
from pathlib import Path

VALIDATOR_PATH = Path(__file__).resolve().parents[1] / "validate_merlins_workshop.py"
SPEC = importlib.util.spec_from_file_location(
    "validate_merlins_workshop_test_target",
    VALIDATOR_PATH,
)
assert SPEC and SPEC.loader
validator = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = validator
SPEC.loader.exec_module(validator)

SOURCE_ROOT = Path(__file__).resolve().parents[4]


class MerlinsWorkshopValidatorTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary = tempfile.TemporaryDirectory()
        self.repo = Path(self.temporary.name)
        for relative in (
            "Plugins/Integrations/MerlinsWorkshop",
            "Gems/TaintedGrailModdingSDK/Tools/tests/test_merlins_workshop.py",
            "Gems/TaintedGrailModdingSDK/Code/Source/GameInformationAcquisition.cpp",
            "Gems/TaintedGrailModdingSDK/Code/Tests/GameInformationAcquisitionTests.cpp",
            "docs/tainted-grail-sdk/MERLINS_WORKSHOP_PROVIDER.md",
            "Plugins/Integrations/README.md",
        ):
            source = SOURCE_ROOT / relative
            destination = self.repo / relative
            destination.parent.mkdir(parents=True, exist_ok=True)
            if source.is_dir():
                shutil.copytree(source, destination)
            else:
                shutil.copy2(source, destination)

    def tearDown(self) -> None:
        self.temporary.cleanup()

    def test_reviewed_contract_passes(self) -> None:
        validator.validate_merlins_workshop(self.repo)

    def test_source_commit_drift_fails(self) -> None:
        path = self.repo / "Plugins/Integrations/MerlinsWorkshop/merlin-source.json"
        document = json.loads(path.read_text())
        document["source"]["commit"] = "173bdab3e09d6adad5003339fc49b021738d71e6"
        path.write_text(json.dumps(document), encoding="utf-8")
        with self.assertRaisesRegex(
            validator.MerlinsWorkshopValidationError,
            "source manifest is invalid",
        ):
            validator.validate_merlins_workshop(self.repo)

    def test_process_authority_escalation_fails(self) -> None:
        path = self.repo / "Plugins/Integrations/MerlinsWorkshop/Tools/merlins_workshop.py"
        text = path.read_text().replace(
            '"process_execution_allowed": False',
            '"process_execution_allowed": True',
            1,
        )
        path.write_text(text, encoding="utf-8")
        with self.assertRaisesRegex(
            validator.MerlinsWorkshopValidationError,
            "False|authority|work order",
        ):
            validator.validate_merlins_workshop(self.repo)

    def test_subprocess_import_fails(self) -> None:
        path = self.repo / "Plugins/Integrations/MerlinsWorkshop/Tools/merlins_workshop.py"
        text = path.read_text(encoding="utf-8")
        text = text.replace(
            "from __future__ import annotations\n",
            "from __future__ import annotations\n\nimport subprocess\n",
            1,
        )
        path.write_text(text, encoding="utf-8")
        with self.assertRaisesRegex(
            validator.MerlinsWorkshopValidationError,
            "forbidden execution",
        ):
            validator.validate_merlins_workshop(self.repo)

    def test_binary_payload_insertion_fails(self) -> None:
        path = self.repo / "Plugins/Integrations/MerlinsWorkshop/official-tool.7z"
        path.write_bytes(b"not allowed")
        with self.assertRaisesRegex(
            validator.MerlinsWorkshopValidationError,
            "file set drifted|binary payload",
        ):
            validator.validate_merlins_workshop(self.repo)

    def test_cpp_provider_reversion_fails(self) -> None:
        path = (
            self.repo
            / "Gems/TaintedGrailModdingSDK/Code/Source/GameInformationAcquisition.cpp"
        )
        text = path.read_text().replace(
            "merlin.m_qualification = QualificationState::ExactInstallBound;",
            "merlin.m_qualification = QualificationState::ContractOnly;",
        )
        path.write_text(text, encoding="utf-8")
        with self.assertRaisesRegex(
            validator.MerlinsWorkshopValidationError,
            "ExactInstallBound",
        ):
            validator.validate_merlins_workshop(self.repo)

    def test_compiled_revision_drift_test_removal_fails(self) -> None:
        path = (
            self.repo
            / "Gems/TaintedGrailModdingSDK/Code/Tests/GameInformationAcquisitionTests.cpp"
        )
        path.write_text(
            path.read_text().replace("MerlinRevisionDriftFailsClosed", "Removed"),
            encoding="utf-8",
        )
        with self.assertRaisesRegex(
            validator.MerlinsWorkshopValidationError,
            "MerlinRevisionDriftFailsClosed",
        ):
            validator.validate_merlins_workshop(self.repo)

    def test_mandatory_bridge_removal_fails(self) -> None:
        path = (
            self.repo
            / "Gems/TaintedGrailModdingSDK/Tools/tests/test_merlins_workshop.py"
        )
        path.write_text(path.read_text().replace("load_tests", "removed"), encoding="utf-8")
        with self.assertRaisesRegex(
            validator.MerlinsWorkshopValidationError,
            "load_tests",
        ):
            validator.validate_merlins_workshop(self.repo)

    def test_github_zip_enablement_fails(self) -> None:
        path = self.repo / "Plugins/Integrations/MerlinsWorkshop/Tools/merlins_workshop.py"
        text = path.read_text().replace(
            '"github_autogenerated_zip_allowed": False',
            '"github_autogenerated_zip_allowed": True',
            1,
        )
        path.write_text(text, encoding="utf-8")
        with self.assertRaisesRegex(
            validator.MerlinsWorkshopValidationError,
            "False|work order",
        ):
            validator.validate_merlins_workshop(self.repo)


if __name__ == "__main__":
    unittest.main()
