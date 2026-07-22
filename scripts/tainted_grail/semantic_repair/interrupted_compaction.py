"""Crash-window compaction with deterministic rollback or finalization."""
from __future__ import annotations

import hashlib
import json
import os
import tempfile
from dataclasses import dataclass, replace
from pathlib import Path
from typing import Any

from .checkpoint_chain import (
    append_checkpoint_proof,
    chain_sha256,
    rotate_checkpoint_chain,
)
from .compaction import JournalCheckpointProof, _canonical_line, _terminal_states
from .errors import CheckpointProofError, SimulatedCrash
from .journal import CrashRecoveryJournal, JournalRecord, _ZERO_HASH, _canonical as _journal_canonical
from .ownership import ResourceLease

_SCHEMA_VERSION = 1
_STAGES = ("prepared", "journal-replaced", "proof-published", "chain-published")


def _prepare_compaction(journal: CrashRecoveryJournal):
    records, torn = journal.read_records(allow_torn_tail=True)
    if torn:
        raise CheckpointProofError("journal must be recovered before compaction")
    if not records:
        raise CheckpointProofError("cannot compact an empty journal")
    source = journal.path.read_bytes()
    states = _terminal_states(records)
    payload = {
        "checkpoint_schema_version": 1,
        "runtime_authority": "none",
        "promotion": "none",
        "source_journal_sha256": hashlib.sha256(source).hexdigest(),
        "source_record_count": len(records),
        "source_final_record_hash": records[-1].record_hash,
        "terminal_states": [item.to_dict() for item in states],
    }
    payload_sha = hashlib.sha256(_journal_canonical(payload)).hexdigest()
    core = {
        "schema_version": 1,
        "sequence": 1,
        "transaction_id": "__journal_checkpoint__",
        "phase": "compacted-checkpoint",
        "payload": payload,
        "previous_hash": _ZERO_HASH,
    }
    record_hash = hashlib.sha256(_journal_canonical(core)).hexdigest()
    checkpoint = JournalRecord(
        1,
        "__journal_checkpoint__",
        "compacted-checkpoint",
        payload,
        _ZERO_HASH,
        record_hash,
    )
    compacted = _canonical_line(checkpoint.to_dict())
    proof = JournalCheckpointProof(
        payload["source_journal_sha256"],
        len(records),
        records[-1].record_hash,
        payload_sha,
        hashlib.sha256(compacted).hexdigest(),
        record_hash,
        states,
    )
    return source, compacted, proof


def _canonical(doc: dict[str, Any]) -> bytes:
    return (json.dumps(doc, sort_keys=True, separators=(",", ":"), ensure_ascii=False) + "\n").encode("utf-8")


def _sha(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def _atomic_write(path: Path, data: bytes) -> None:
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    fd, temp_name = tempfile.mkstemp(prefix=f".{path.name}.", dir=path.parent)
    try:
        with os.fdopen(fd, "wb") as handle:
            handle.write(data)
            handle.flush()
            os.fsync(handle.fileno())
        os.replace(temp_name, path)
    except Exception:
        try:
            os.unlink(temp_name)
        except FileNotFoundError:
            pass
        raise


def _path_hash(path: Path) -> str:
    return _sha(path.read_bytes()) if path.exists() else _sha(b"")


@dataclass(frozen=True)
class InterruptedCompactionState:
    stage: str
    owner_id: str
    lock_generation: int
    proof_name: str
    chain_name: str
    source_journal_sha256: str
    compacted_journal_sha256: str
    intended_proof_sha256: str
    previous_proof_exists: bool
    previous_proof_sha256: str
    previous_chain_exists: bool
    previous_chain_sha256: str
    active_chain_sha256: str | None = None

    def __post_init__(self) -> None:
        if self.stage not in _STAGES:
            raise CheckpointProofError("interrupted compaction stage is invalid")
        if not self.owner_id or type(self.lock_generation) is not int or self.lock_generation <= 0:
            raise CheckpointProofError("interrupted compaction owner is invalid")
        if Path(self.proof_name).name != self.proof_name or Path(self.chain_name).name != self.chain_name:
            raise CheckpointProofError("interrupted compaction paths must be sibling names")
        hashes = (
            self.source_journal_sha256,
            self.compacted_journal_sha256,
            self.intended_proof_sha256,
            self.previous_proof_sha256,
            self.previous_chain_sha256,
        )
        if any(not isinstance(value, str) or len(value) != 64 for value in hashes):
            raise CheckpointProofError("interrupted compaction hash is invalid")
        if self.active_chain_sha256 is not None and len(self.active_chain_sha256) != 64:
            raise CheckpointProofError("interrupted compaction active chain hash is invalid")

    def to_dict(self) -> dict[str, Any]:
        return {
            "schema_version": _SCHEMA_VERSION,
            "authority": "synthetic-interrupted-compaction-only",
            "runtime_authority": "none",
            "promotion": "none",
            "stage": self.stage,
            "owner_id": self.owner_id,
            "lock_generation": self.lock_generation,
            "proof_name": self.proof_name,
            "chain_name": self.chain_name,
            "source_journal_sha256": self.source_journal_sha256,
            "compacted_journal_sha256": self.compacted_journal_sha256,
            "intended_proof_sha256": self.intended_proof_sha256,
            "previous_proof_exists": self.previous_proof_exists,
            "previous_proof_sha256": self.previous_proof_sha256,
            "previous_chain_exists": self.previous_chain_exists,
            "previous_chain_sha256": self.previous_chain_sha256,
            "active_chain_sha256": self.active_chain_sha256,
        }

    def to_bytes(self) -> bytes:
        return _canonical(self.to_dict())

    @classmethod
    def from_bytes(cls, data: bytes) -> "InterruptedCompactionState":
        try:
            doc = json.loads(data.decode("utf-8"))
        except (UnicodeDecodeError, json.JSONDecodeError) as exc:
            raise CheckpointProofError("invalid interrupted compaction state") from exc
        expected = {
            "schema_version",
            "authority",
            "runtime_authority",
            "promotion",
            "stage",
            "owner_id",
            "lock_generation",
            "proof_name",
            "chain_name",
            "source_journal_sha256",
            "compacted_journal_sha256",
            "intended_proof_sha256",
            "previous_proof_exists",
            "previous_proof_sha256",
            "previous_chain_exists",
            "previous_chain_sha256",
            "active_chain_sha256",
        }
        if not isinstance(doc, dict) or set(doc) != expected:
            raise CheckpointProofError("interrupted compaction state has unexpected fields")
        if (
            doc["schema_version"] != _SCHEMA_VERSION
            or doc["authority"] != "synthetic-interrupted-compaction-only"
            or doc["runtime_authority"] != "none"
            or doc["promotion"] != "none"
            or type(doc["previous_proof_exists"]) is not bool
            or type(doc["previous_chain_exists"]) is not bool
        ):
            raise CheckpointProofError("interrupted compaction state boundary is invalid")
        return cls(
            doc["stage"],
            doc["owner_id"],
            doc["lock_generation"],
            doc["proof_name"],
            doc["chain_name"],
            doc["source_journal_sha256"],
            doc["compacted_journal_sha256"],
            doc["intended_proof_sha256"],
            doc["previous_proof_exists"],
            doc["previous_proof_sha256"],
            doc["previous_chain_exists"],
            doc["previous_chain_sha256"],
            doc["active_chain_sha256"],
        )


@dataclass(frozen=True)
class InterruptedCompactionReceipt:
    status: str
    proof_sha256: str
    chain_sha256: str
    source_journal_sha256: str
    compacted_journal_sha256: str

    def to_bytes(self) -> bytes:
        return _canonical(
            {
                "schema_version": 1,
                "authority": "synthetic-compaction-recovery-receipt-only",
                "runtime_authority": "none",
                "promotion": "none",
                "status": self.status,
                "proof_sha256": self.proof_sha256,
                "chain_sha256": self.chain_sha256,
                "source_journal_sha256": self.source_journal_sha256,
                "compacted_journal_sha256": self.compacted_journal_sha256,
            }
        )


@dataclass(frozen=True)
class _Paths:
    state: Path
    source_backup: Path
    proof_backup: Path
    chain_backup: Path


def _paths(journal: CrashRecoveryJournal) -> _Paths:
    stem = f".{journal.path.name}.compaction"
    return _Paths(
        journal.path.with_name(f"{stem}.state.json"),
        journal.path.with_name(f"{stem}.source.bak"),
        journal.path.with_name(f"{stem}.proof.bak"),
        journal.path.with_name(f"{stem}.chain.bak"),
    )


def _require_siblings(journal: CrashRecoveryJournal, proof_path: Path, chain_path: Path) -> None:
    parent = journal.path.parent.resolve(strict=False)
    for path in (proof_path, chain_path):
        if path.parent.resolve(strict=False) != parent or path.name in {journal.path.name, "", ".", ".."}:
            raise CheckpointProofError("compaction proof and chain must be distinct journal siblings")
    if proof_path.name == chain_path.name:
        raise CheckpointProofError("compaction proof and chain paths must differ")


def _save_state(path: Path, state: InterruptedCompactionState) -> None:
    _atomic_write(path, state.to_bytes())


def _restore_previous(path: Path, backup: Path, existed: bool, expected_hash: str) -> None:
    if existed:
        if not backup.exists() or _sha(backup.read_bytes()) != expected_hash:
            raise CheckpointProofError("compaction backup is missing or corrupt")
        _atomic_write(path, backup.read_bytes())
    else:
        path.unlink(missing_ok=True)


def _cleanup(paths: _Paths) -> None:
    for path in (paths.state, paths.source_backup, paths.proof_backup, paths.chain_backup):
        path.unlink(missing_ok=True)


def compact_terminal_journal_resilient_owned(
    journal: CrashRecoveryJournal,
    lease: ResourceLease,
    proof_path: Path,
    chain_path: Path,
    *,
    crash_after_stage: str | None = None,
    max_chain_entries: int = 8,
) -> InterruptedCompactionReceipt:
    journal.lock.assert_owned(lease)
    proof_path = Path(proof_path)
    chain_path = Path(chain_path)
    _require_siblings(journal, proof_path, chain_path)
    if crash_after_stage is not None and crash_after_stage not in _STAGES:
        raise CheckpointProofError("unknown interrupted compaction crash stage")
    files = _paths(journal)
    if files.state.exists():
        raise CheckpointProofError("an interrupted compaction already requires recovery")

    source, compacted, proof = _prepare_compaction(journal)
    previous_proof = proof_path.read_bytes() if proof_path.exists() else None
    previous_chain = chain_path.read_bytes() if chain_path.exists() else None
    _atomic_write(files.source_backup, source)
    if previous_proof is not None:
        _atomic_write(files.proof_backup, previous_proof)
    if previous_chain is not None:
        _atomic_write(files.chain_backup, previous_chain)
    state = InterruptedCompactionState(
        "prepared",
        lease.identity.owner_id,
        lease.identity.generation,
        proof_path.name,
        chain_path.name,
        _sha(source),
        _sha(compacted),
        proof.sha256,
        previous_proof is not None,
        _sha(previous_proof or b""),
        previous_chain is not None,
        _sha(previous_chain or b""),
    )
    _save_state(files.state, state)
    if crash_after_stage == state.stage:
        raise SimulatedCrash("synthetic crash after compaction preparation")

    _atomic_write(journal.path, compacted)
    state = replace(state, stage="journal-replaced")
    _save_state(files.state, state)
    if crash_after_stage == state.stage:
        raise SimulatedCrash("synthetic crash after compacted journal publication")

    _atomic_write(proof_path, proof.to_bytes())
    state = replace(state, stage="proof-published")
    _save_state(files.state, state)
    if crash_after_stage == state.stage:
        raise SimulatedCrash("synthetic crash after checkpoint proof publication")

    append_checkpoint_proof(chain_path, proof)
    rotate_checkpoint_chain(chain_path, max_entries=max_chain_entries)
    active_chain_sha = chain_sha256(chain_path)
    state = replace(state, stage="chain-published", active_chain_sha256=active_chain_sha)
    _save_state(files.state, state)
    if crash_after_stage == state.stage:
        raise SimulatedCrash("synthetic crash after checkpoint chain publication")

    _cleanup(files)
    return InterruptedCompactionReceipt(
        "committed",
        proof.sha256,
        active_chain_sha,
        state.source_journal_sha256,
        state.compacted_journal_sha256,
    )


def recover_interrupted_compaction_owned(
    journal: CrashRecoveryJournal,
    lease: ResourceLease,
) -> InterruptedCompactionReceipt:
    journal.lock.assert_owned(lease)
    files = _paths(journal)
    if not files.state.exists():
        raise CheckpointProofError("no interrupted compaction state exists")
    state = InterruptedCompactionState.from_bytes(files.state.read_bytes())
    if (
        state.owner_id != lease.identity.owner_id
        or state.lock_generation != lease.identity.generation
    ):
        raise CheckpointProofError("interrupted compaction ownership differs from active lease")
    if not files.source_backup.exists() or _sha(files.source_backup.read_bytes()) != state.source_journal_sha256:
        raise CheckpointProofError("interrupted compaction source backup is corrupt")

    proof_path = journal.path.with_name(state.proof_name)
    chain_path = journal.path.with_name(state.chain_name)
    current_journal_sha = _path_hash(journal.path)
    current_proof_sha = _path_hash(proof_path)
    current_chain_sha = _path_hash(chain_path)

    expected_journal = (
        state.source_journal_sha256
        if state.stage == "prepared"
        else state.compacted_journal_sha256
    )
    expected_proof = (
        state.intended_proof_sha256
        if state.stage in {"proof-published", "chain-published"}
        else state.previous_proof_sha256
    )
    expected_chain = (
        state.active_chain_sha256
        if state.stage == "chain-published"
        else state.previous_chain_sha256
    )
    if current_journal_sha != expected_journal:
        raise CheckpointProofError("interrupted compaction journal has unknown bytes")
    if current_proof_sha != expected_proof:
        raise CheckpointProofError("interrupted compaction proof has unknown bytes")
    if current_chain_sha != expected_chain:
        raise CheckpointProofError("interrupted compaction chain has unknown bytes")

    if state.stage == "chain-published":
        status = "finalized"
        chain_result = current_chain_sha
    else:
        _atomic_write(journal.path, files.source_backup.read_bytes())
        _restore_previous(
            proof_path,
            files.proof_backup,
            state.previous_proof_exists,
            state.previous_proof_sha256,
        )
        _restore_previous(
            chain_path,
            files.chain_backup,
            state.previous_chain_exists,
            state.previous_chain_sha256,
        )
        status = "rolled-back"
        chain_result = state.previous_chain_sha256
    _cleanup(files)
    return InterruptedCompactionReceipt(
        status,
        state.intended_proof_sha256,
        chain_result,
        state.source_journal_sha256,
        state.compacted_journal_sha256,
    )
