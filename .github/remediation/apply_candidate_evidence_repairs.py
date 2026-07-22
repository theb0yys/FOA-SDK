#!/usr/bin/env python3
from __future__ import annotations

from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def replace_once(relative: str, old: str, new: str) -> None:
    path = ROOT / relative
    text = path.read_text(encoding="utf-8", errors="strict")
    if text.count(old) != 1:
        raise SystemExit(f"Expected exactly one candidate-evidence anchor in {relative!r}.")
    path.write_text(text.replace(old, new, 1), encoding="utf-8")


replace_once(
    "Gems/TaintedGrailModdingSDK/Code/Source/ExtensionAPI.cpp",
    "        return m_sourceRegistry.RegisterEvidence(evidence, error);\n",
    "        return m_sourceRegistry.RegisterCandidateEvidence(evidence, error);\n",
)

replace_once(
    "Gems/TaintedGrailModdingSDK/Code/Tests/ExtensionAPITests.cpp",
    "        EXPECT_NE(m_foundation.GetSourceRegistry().FindEvidence(evidence.m_evidenceId), nullptr);\n",
    "        EXPECT_EQ(m_foundation.GetSourceRegistry().FindEvidence(evidence.m_evidenceId), nullptr);\n"
    "        EXPECT_NE(\n"
    "            m_foundation.GetSourceRegistry().FindCandidateEvidence(evidence.m_evidenceId),\n"
    "            nullptr);\n",
)
