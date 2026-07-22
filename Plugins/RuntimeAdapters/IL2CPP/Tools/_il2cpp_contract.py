# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Shared immutable package, interop, and build-plan contracts for IL2CPP."""
from __future__ import annotations
import hashlib, json, re
from datetime import datetime, timezone
from pathlib import Path
from typing import Mapping

PACKAGE_ROOT = Path(__file__).resolve().parents[1]
ADAPTER_ID = "adapter.foa.il2cpp"
PACKAGE_ID = "extension.foa-il2cpp-runtime-adapter"
PACKAGE_VERSION = "0.1.0"
STARTUP_MARKER = "foa-sdk.il2cpp-adapter.ready"
PROFILE = {"bepinex_version": "6.0.0-be.735", "branch": "il2cpp", "evidence_state": "PackageInstallValidated", "framework_version": "0.1.36", "game_version": "1.23.401", "profile_id": "foa-1.23.401-il2cpp-bepinex6-tf-0.1.36", "runtime_target": "IL2CPP", "unity_version": "6000.0.64f1"}
AUTHORITY_FIELDS = ('build', 'deployment', 'game_launch', 'process_execution', 'runtime_execution', 'runtime_mutation', 'save_access', 'signing', 'publication', 'catalog_mutation', 'evidence_promotion')
MANIFEST_SHA256 = "sha256:9efdbe1b6e1010829712d879a2343640e1cff2db54e3105d39201c75f6c711f0"
SHA256_RE = re.compile(r"^sha256:[0-9a-f]{64}$")
ID_RE = re.compile(r"^[a-z0-9]+(?:[._-][a-z0-9]+)*$")
UTC_RE = re.compile(r"^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}Z$")
SAFE_PATH_RE = re.compile(r"^[A-Za-z0-9_.-]+(?:/[A-Za-z0-9_.-]+)*$")
REQUIRED_INTEROP = {"assembly-csharp":"BepInEx/interop/Assembly-CSharp.dll","tg-main":"BepInEx/interop/TG.Main.dll"}

class Il2CppAdapterError(RuntimeError):
    pass

def canonical_json(document: Mapping[str, object]) -> bytes:
    return (json.dumps(document, sort_keys=True, separators=(",", ":"), ensure_ascii=False) + "\n").encode("utf-8")

def sha256_bytes(data: bytes) -> str:
    return "sha256:" + hashlib.sha256(data).hexdigest()

def fingerprint(document: Mapping[str, object]) -> str:
    return sha256_bytes(canonical_json(document))

def authority() -> dict[str, bool]:
    return {field: False for field in AUTHORITY_FIELDS}

def object_value(value: object, label: str) -> dict[str, object]:
    if not isinstance(value, dict): raise Il2CppAdapterError(f"{label} must be an object.")
    return dict(value)

def text(value: object, label: str, maximum: int, allow_empty: bool=False) -> str:
    if not isinstance(value, str) or (not allow_empty and not value) or len(value) > maximum:
        raise Il2CppAdapterError(f"{label} is invalid or unbounded.")
    if any(ord(char) < 0x20 or ord(char) == 0x7F for char in value):
        raise Il2CppAdapterError(f"{label} contains control characters.")
    return value

def sha256(value: object, label: str) -> str:
    checked=text(value,label,71)
    if not SHA256_RE.fullmatch(checked): raise Il2CppAdapterError(f"{label} must be a lowercase SHA-256 fingerprint.")
    return checked

def stable_id(value: object, label: str) -> str:
    checked=text(value,label,128)
    if not ID_RE.fullmatch(checked): raise Il2CppAdapterError(f"{label} must be a stable lowercase identifier.")
    return checked

def utc(value: object, label: str) -> datetime:
    checked=text(value,label,20)
    if not UTC_RE.fullmatch(checked): raise Il2CppAdapterError(f"{label} must be whole-second UTC.")
    return datetime.strptime(checked, "%Y-%m-%dT%H:%M:%SZ").replace(tzinfo=timezone.utc)

def safe_reference(value: object, label: str, allow_empty: bool=False) -> str:
    checked=text(value,label,512,allow_empty)
    if checked or not allow_empty:
        if not SAFE_PATH_RE.fullmatch(checked) or ".." in checked.split("/") or ":" in checked or "\\" in checked:
            raise Il2CppAdapterError(f"{label} must be a safe relative logical reference.")
    return checked

def exact_authority(value: object, label: str) -> None:
    record=object_value(value,label)
    if set(record) != set(AUTHORITY_FIELDS) or any(record[field] is not False for field in AUTHORITY_FIELDS):
        raise Il2CppAdapterError(f"{label} must contain the exact all-false authority record.")

def unique_ids(value: object, label: str) -> list[str]:
    if not isinstance(value,list) or not 1 <= len(value) <= 64: raise Il2CppAdapterError(f"{label} must contain between 1 and 64 identifiers.")
    checked=[stable_id(row,f"{label}[{index}]") for index,row in enumerate(value)]
    if len(set(checked)) != len(checked): raise Il2CppAdapterError(f"{label} contains duplicate identifiers.")
    return sorted(checked)

def positive_int(value: object, label: str, maximum: int=4*1024*1024*1024) -> int:
    if not isinstance(value,int) or isinstance(value,bool) or value <= 0 or value > maximum: raise Il2CppAdapterError(f"{label} must be a positive bounded integer.")
    return value

def load_json(path: Path) -> dict[str, object]:
    try:
        raw=path.read_bytes()
        if len(raw) > 512*1024: raise Il2CppAdapterError(f"{path} exceeds the JSON byte limit.")
        return object_value(json.loads(raw.decode("utf-8",errors="strict")),str(path))
    except (OSError,UnicodeDecodeError,json.JSONDecodeError) as exc:
        raise Il2CppAdapterError(f"Unable to load strict JSON from {path}.") from exc

def validate_manifest(document: Mapping[str, object]) -> dict[str, object]:
    manifest=dict(document)
    if fingerprint(manifest) != MANIFEST_SHA256: raise Il2CppAdapterError("il2cpp-adapter.json differs from the reviewed canonical manifest.")
    exact_authority(manifest.get("authority"),"manifest.authority")
    return manifest

def validate_package(package_root: Path=PACKAGE_ROOT) -> dict[str, object]:
    manifest=validate_manifest(load_json(package_root/"il2cpp-adapter.json"))
    source=object_value(manifest["source_project"],"source_project")
    for raw in source["source_files"]:
        row=object_value(raw,"source file"); relative=safe_reference(row["path"],"source file path"); path=package_root/relative
        if path.is_symlink() or not path.is_file() or path.stat().st_size > 256*1024: raise Il2CppAdapterError(f"Required IL2CPP source file is missing, linked, or unbounded: {relative}")
        if sha256_bytes(path.read_bytes()) != row["sha256"]: raise Il2CppAdapterError(f"IL2CPP source fingerprint mismatch: {relative}")
    forbidden={".dll",".exe",".pdb",".so",".dylib",".zip",".7z",".nupkg"}
    for path in package_root.rglob("*"):
        if path.is_file() and path.suffix.lower() in forbidden: raise Il2CppAdapterError(f"Generated or binary payload is forbidden: {path.name}")
    return manifest

def validate_interop_manifest(document: Mapping[str, object]) -> dict[str, object]:
    record=dict(document); expected_keys={"schema_version","manifest_kind","profile","generator","materials","authority","interop_manifest_sha256"}
    if set(record) != expected_keys or record.get("schema_version") != 1 or record.get("manifest_kind") != "foa-il2cpp-generated-interop": raise Il2CppAdapterError("IL2CPP interop manifest identity or field set is invalid.")
    if record.get("profile") != PROFILE: raise Il2CppAdapterError("IL2CPP interop manifest requires the exact IL2CPP profile.")
    generator=object_value(record.get("generator"),"interop.generator")
    if set(generator) != {"id","version","generation_input_sha256"}: raise Il2CppAdapterError("IL2CPP interop generator field set is invalid.")
    stable_id(generator.get("id"),"interop.generator.id"); text(generator.get("version"),"interop.generator.version",128); sha256(generator.get("generation_input_sha256"),"interop.generator.generation_input_sha256")
    rows=record.get("materials")
    if not isinstance(rows,list) or len(rows) != len(REQUIRED_INTEROP): raise Il2CppAdapterError("IL2CPP interop manifest must contain the exact required material set.")
    seen={}
    ordered_roles=[]
    for index,raw in enumerate(rows):
        row=object_value(raw,f"interop.materials[{index}]")
        if set(row) != {"role","relative_path","sha256","bytes","redistributable"}: raise Il2CppAdapterError("IL2CPP interop material field set is invalid.")
        role=stable_id(row.get("role"),f"interop.materials[{index}].role")
        path=safe_reference(row.get("relative_path"),f"interop.materials[{index}].relative_path")
        if role not in REQUIRED_INTEROP or REQUIRED_INTEROP[role] != path or role in seen: raise Il2CppAdapterError("IL2CPP interop material identity, path, or uniqueness is invalid.")
        sha256(row.get("sha256"),f"interop.materials[{index}].sha256"); positive_int(row.get("bytes"),f"interop.materials[{index}].bytes")
        if row.get("redistributable") is not False: raise Il2CppAdapterError("Generated IL2CPP interop inputs are local and non-redistributable.")
        seen[role]=row; ordered_roles.append(role)
    if ordered_roles != sorted(REQUIRED_INTEROP): raise Il2CppAdapterError("IL2CPP interop materials must use canonical role order.")
    exact_authority(record.get("authority"),"interop.authority")
    base={k:record[k] for k in record if k != "interop_manifest_sha256"}
    if record.get("interop_manifest_sha256") != fingerprint(base): raise Il2CppAdapterError("IL2CPP interop manifest fingerprint is stale or altered.")
    return record

def build_plan(interop_manifest: Mapping[str, object], package_root: Path=PACKAGE_ROOT) -> dict[str, object]:
    manifest=validate_package(package_root); interop=validate_interop_manifest(interop_manifest); source=object_value(manifest["source_project"],"source_project")
    base={"schema_version":1,"plan_kind":"foa-il2cpp-adapter-build-plan","adapter_id":ADAPTER_ID,"package_id":PACKAGE_ID,"package_version":PACKAGE_VERSION,"profile":PROFILE,"route_contract_version":"1.0.0","source_files":source["source_files"],"dependencies":manifest["dependencies"],"generated_interop":interop,"expected_binaries":manifest["expected_binaries"],"steps":[{"step_id":"validate-exact-local-il2cpp-references","tool":"dotnet","arguments":["build",source["project"],"-c","Release","-p:FoAGameRoot=${FOA_GAME_ROOT}","-p:ContinuousIntegrationBuild=true","-p:UseSharedCompilation=false"],"execution_allowed":False}],"authority":authority()}
    return {**base,"build_plan_sha256":fingerprint(base)}

def validate_plan(plan: Mapping[str, object], package_root: Path=PACKAGE_ROOT) -> dict[str, object]:
    document=dict(plan); interop=object_value(document.get("generated_interop"),"generated_interop")
    if document != build_plan(interop,package_root): raise Il2CppAdapterError("IL2CPP build plan is stale, altered, or noncanonical.")
    return document
