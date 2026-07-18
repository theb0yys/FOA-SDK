#!/usr/bin/env python3
#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#
"""Collect and verify a local, redacted Developer Preview 0 diagnostics bundle.

The command does not launch FoA, upload data, read saves, deploy files, or include
source artifact contents. Review every generated file before sharing it.
"""
from __future__ import annotations
import argparse, hashlib, json, os, platform, re, shutil, subprocess, sys, tempfile
from datetime import datetime, timezone
from pathlib import Path, PurePosixPath
from typing import Any, Callable, Sequence

SCHEMA_VERSION=1
BUNDLE_ID="tg-sdk-developer-preview-0-diagnostics"
MANIFEST_NAME="diagnostics-manifest.json"
SUMMARY_NAME="diagnostics-summary.json"
README_NAME="README.txt"
SYSTEM_NAME="system.txt"
BUILD_NAME="build-summary.json"
VALIDATION_NAME="validation-summary.json"
LAUNCH_NAME="launch-summary.json"
INVENTORY_NAME="workspace-inventory.json"
MAX_LOG_BYTES_DEFAULT=256*1024
MAX_LOG_BYTES_HARD=1024*1024
MAX_BUNDLE_FILE_BYTES=2*1024*1024
MAX_INVENTORY_FILES=5000
MAX_INVENTORY_HASH_BYTES=128*1024*1024
DURABLE_SUFFIXES=(".tgworkspace.json",".tgpack.json",".tgsource.json",".tgevidence.json",".tgcatalog.json")
SECRET_KEY_PATTERN=re.compile(r"(?i)(authorization|bearer|password|passwd|pwd|secret|token|api[_-]?key|credential|cookie|private[_-]?key)")
SECRET_ASSIGNMENT_PATTERN=re.compile(r"(?i)(authorization|password|passwd|pwd|secret|token|api[_-]?key|credential|cookie|private[_-]?key)\s*[:=]\s*(?:(?i:Bearer)\s+[A-Za-z0-9._~+/=-]+|\"[^\"]*\"|'[^']*'|[^\s,;]+)")
BEARER_PATTERN=re.compile(r"(?i)\bbearer\s+[A-Za-z0-9._~+/=-]+")
KNOWN_TOKEN_PATTERN=re.compile(r"\b(?:gh[pousr]_[A-Za-z0-9]{20,}|github_pat_[A-Za-z0-9_]{20,}|sk-[A-Za-z0-9_-]{16,}|AKIA[0-9A-Z]{16})\b")
URL_CREDENTIAL_PATTERN=re.compile(r"(?i)\b([a-z][a-z0-9+.-]*://)[^/@\s:]+:[^/@\s]+@")
WINDOWS_PATH_PATTERN=re.compile(r"(?<![A-Za-z0-9_])[A-Za-z]:[\\/][^\r\n\t\"'<>|,;]+")
UNC_PATH_PATTERN=re.compile(r"\\\\[^\\\s]+\\[^\r\n\t\"'<>|,;]+")
PRIVATE_POSIX_PATH_PATTERN=re.compile(r"(?<![A-Za-z0-9_])/(?:home|Users|tmp|private/var|var/tmp|mnt|media)/[^\r\n\t\"'<>|,;]+")
CommandRunner=Callable[[Sequence[str],Path],tuple[int,str]]
class DiagnosticsError(RuntimeError): pass

def repository_root_from_script(): return Path(__file__).resolve().parents[3]
def default_build_directory(repo_root): return repo_root/"build/tg-sdk-developer-preview-0-windows-profile"
def resolve_path(value,base):
    path=value.expanduser(); return (path if path.is_absolute() else base/path).resolve(strict=False)
def is_relative_to(path,parent):
    try: path.relative_to(parent); return True
    except ValueError: return False
def utc_now(): return datetime.now(timezone.utc).isoformat(timespec="milliseconds").replace("+00:00","Z")
def sha256_bytes(data): return hashlib.sha256(data).hexdigest()
def sha256_file(path,maximum_bytes=None):
    if maximum_bytes is not None and path.stat().st_size>maximum_bytes: raise DiagnosticsError(f"File is too large to hash for diagnostics inventory: {path}")
    h=hashlib.sha256()
    with path.open("rb") as f:
        for block in iter(lambda:f.read(1024*1024),b""): h.update(block)
    return h.hexdigest()
def canonical_json_bytes(value): return (json.dumps(value,indent=2,sort_keys=True,ensure_ascii=False)+"\n").encode()
def atomic_write(path,data):
    path.parent.mkdir(parents=True,exist_ok=True); fd,name=tempfile.mkstemp(prefix=f".{path.name}.",suffix=".tmp",dir=path.parent); tmp=Path(name)
    try:
        with os.fdopen(fd,"wb") as f: f.write(data); f.flush(); os.fsync(f.fileno())
        os.replace(tmp,path)
    finally:
        if tmp.exists(): tmp.unlink()
def normalize_relative_path(value):
    if not isinstance(value,str) or not value: raise DiagnosticsError("Diagnostics manifest paths must be non-empty strings.")
    path=PurePosixPath(value.replace("\\","/"))
    if path.is_absolute() or ".." in path.parts or "." in path.parts or str(path).startswith("/") or ":" in path.parts[0]: raise DiagnosticsError(f"Unsafe diagnostics manifest path: {value}")
    return path.as_posix()
def ensure_no_symlinks(root):
    if root.is_symlink(): raise DiagnosticsError(f"Diagnostics path must not be a symbolic link: {root}")
    if root.exists():
        for path in root.rglob("*"):
            if path.is_symlink(): raise DiagnosticsError(f"Diagnostics input or output contains a symbolic link: {path}")
def known_path_replacements(*,repo_root,build_dir,project_root,workspace_root):
    pairs=[]; seen=set()
    for path,token in ((workspace_root,"<WORKSPACE_ROOT>"),(project_root,"<PROJECT_ROOT>"),(build_dir,"<BUILD_DIR>"),(repo_root,"<REPO_ROOT>"),(Path.home(),"<HOME>"),(Path(tempfile.gettempdir()),"<TEMP>")):
        if path is None: continue
        base=str(path.resolve(strict=False))
        for variant in {base,base.replace("\\","/"),base.replace("/","\\")}:
            if variant and variant not in seen: seen.add(variant); pairs.append((variant,token))
    return sorted(pairs,key=lambda pair:len(pair[0]),reverse=True)
def redact_argument_list(arguments):
    result=[]; hide=False
    for raw in arguments:
        value=str(raw)
        if hide: result.append("<REDACTED>"); hide=False; continue
        if value.startswith("-"):
            if "=" in value:
                key,_=value.split("=",1); result.append(f"{key}=<REDACTED>" if SECRET_KEY_PATTERN.search(key) else value); continue
            if SECRET_KEY_PATTERN.search(value): hide=True
        result.append(value)
    return result
def redact_text(text,*,replacements):
    out=text.replace("\x00","")
    for source,token in replacements: out=re.sub(re.escape(source)+r"(?![A-Za-z0-9._-])",token,out,flags=re.I)
    out=URL_CREDENTIAL_PATTERN.sub(r"\1<REDACTED>@",out)
    out=SECRET_ASSIGNMENT_PATTERN.sub(lambda m:f"{m.group(1)}=<REDACTED>",out)
    out=BEARER_PATTERN.sub("Bearer <REDACTED>",out); out=KNOWN_TOKEN_PATTERN.sub("<REDACTED>",out)
    out=UNC_PATH_PATTERN.sub("<ABSOLUTE_PATH>",out); out=WINDOWS_PATH_PATTERN.sub("<ABSOLUTE_PATH>",out); out=PRIVATE_POSIX_PATH_PATTERN.sub("<ABSOLUTE_PATH>",out)
    return out
def redact_value(value,*,replacements,key=""):
    if SECRET_KEY_PATTERN.search(key): return "<REDACTED>"
    if isinstance(value,dict): return {str(k):redact_value(v,replacements=replacements,key=str(k)) for k,v in sorted(value.items(),key=lambda x:str(x[0]))}
    if isinstance(value,list):
        if key.casefold() in {"args","arguments","argv","command"}: return [redact_text(x,replacements=replacements) for x in redact_argument_list(value)]
        return [redact_value(x,replacements=replacements,key=key) for x in value]
    if isinstance(value,str): return redact_text(value,replacements=replacements)
    return value if value is None or isinstance(value,(bool,int,float)) else redact_text(str(value),replacements=replacements)
def assert_redacted_text(text,label):
    for match in SECRET_ASSIGNMENT_PATTERN.finditer(text):
        if "<REDACTED>" not in match.group(0): raise DiagnosticsError(f"Diagnostics redaction failure in {label}: secret-like material remains.")
    if BEARER_PATTERN.search(text) or KNOWN_TOKEN_PATTERN.search(text) or URL_CREDENTIAL_PATTERN.search(text): raise DiagnosticsError(f"Diagnostics redaction failure in {label}: secret-like material remains.")
    if WINDOWS_PATH_PATTERN.search(text) or UNC_PATH_PATTERN.search(text) or PRIVATE_POSIX_PATH_PATTERN.search(text): raise DiagnosticsError(f"Diagnostics redaction failure in {label}: private absolute path remains.")
    if "\x00" in text: raise DiagnosticsError(f"Diagnostics redaction failure in {label}: NUL remains.")
def default_command_runner(command,cwd):
    try:
        p=subprocess.run(list(command),cwd=str(cwd),check=False,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,encoding="utf-8",errors="replace",timeout=30)
        return int(p.returncode),p.stdout.strip()
    except (OSError,subprocess.TimeoutExpired) as exc: return 127,str(exc)
def collect_tool_summary(repo_root,runner=default_command_runner):
    commands={"cmake":("cmake","--version"),"git":("git","--version"),"git-lfs":("git","lfs","version"),"source-commit":("git","rev-parse","HEAD"),"source-status":("git","status","--short")}
    summary={"python":{"exit_code":0,"output":sys.version.splitlines()[0]},"host":{"exit_code":0,"output":f"{platform.system()} {platform.release()} {platform.machine()}"}}
    for name,command in commands.items():
        code,out=runner(command,repo_root); summary[name]={"exit_code":code,"output":out}
    return summary
def parse_cmake_cache(build_dir):
    cache=build_dir/"CMakeCache.txt"
    if not cache.is_file(): return {"status":"not-configured"}
    allowed={"CMAKE_GENERATOR","CMAKE_HOME_DIRECTORY","CMAKE_BUILD_TYPE","LY_UNITY_BUILD","LY_MONOLITHIC_GAME","PAL_PLATFORM_NAME"}; result={"status":"configured"}
    for line in cache.read_text(encoding="utf-8",errors="replace").splitlines():
        if line.startswith("//") or line.startswith("#") or "=" not in line: continue
        key_type,value=line.split("=",1); key=key_type.split(":",1)[0]
        if key in allowed: result[key]=value
    return result
def is_durable_document(path): return path.name=="preview-fixture.manifest.json" or path.name.endswith(DURABLE_SUFFIXES)
def workspace_inventory(workspace_root):
    if not workspace_root.is_dir(): raise DiagnosticsError(f"Workspace root does not exist: {workspace_root}")
    ensure_no_symlinks(workspace_root); files=[]
    for path in sorted(workspace_root.rglob("*")):
        if not path.is_file() or not is_durable_document(path): continue
        if len(files)>=MAX_INVENTORY_FILES: raise DiagnosticsError("Workspace inventory exceeds the file limit.")
        files.append({"path":path.relative_to(workspace_root).as_posix(),"size_bytes":path.stat().st_size,"sha256":sha256_file(path,maximum_bytes=MAX_INVENTORY_HASH_BYTES)})
    return {"schema_version":SCHEMA_VERSION,"files":files}
def read_log_excerpt(path,maximum_bytes):
    if maximum_bytes<=0 or maximum_bytes>MAX_LOG_BYTES_HARD: raise DiagnosticsError(f"Editor log excerpt limit must be between 1 and {MAX_LOG_BYTES_HARD} bytes.")
    if not path.is_file(): raise DiagnosticsError(f"Editor log does not exist: {path}")
    with path.open("rb") as f:
        size=path.stat().st_size; f.seek(max(0,size-maximum_bytes)); data=f.read(maximum_bytes)
    text=data.decode("utf-8",errors="replace")
    return ("[Earlier log content omitted.]\n" if size>maximum_bytes else "")+text
def load_json_result(path,label):
    if not path.is_file(): raise DiagnosticsError(f"The {label} does not exist: {path}")
    try: value=json.loads(path.read_text(encoding="utf-8"))
    except (OSError,json.JSONDecodeError) as exc: raise DiagnosticsError(f"The {label} is not valid UTF-8 JSON: {path}: {exc}") from exc
    if not isinstance(value,dict): raise DiagnosticsError(f"The {label} must contain a JSON object: {path}")
    return value
def allowed_bundle_path(relative):
    path=PurePosixPath(relative)
    return relative in {README_NAME,SUMMARY_NAME,SYSTEM_NAME,BUILD_NAME,VALIDATION_NAME,LAUNCH_NAME,INVENTORY_NAME} or (len(path.parts)==2 and path.parts[0]=="logs" and path.suffix==".log")
def build_manifest(output_dir,generated_at):
    entries=[]
    for path in sorted(output_dir.rglob("*")):
        if not path.is_file() or path.name==MANIFEST_NAME: continue
        relative=path.relative_to(output_dir).as_posix()
        if not allowed_bundle_path(relative): raise DiagnosticsError(f"Diagnostics output contains a prohibited file: {relative}")
        entries.append({"path":relative,"size_bytes":path.stat().st_size,"sha256":sha256_file(path)})
    return {"schema_version":SCHEMA_VERSION,"bundle_id":BUNDLE_ID,"generated_at_utc":generated_at,"files":entries}
def prepare_output_directory(output_dir,replace):
    if output_dir.is_symlink(): raise DiagnosticsError(f"Diagnostics output must not be a symbolic link: {output_dir}")
    if output_dir.exists() and not output_dir.is_dir(): raise DiagnosticsError(f"Diagnostics output is not a directory: {output_dir}")
    if not output_dir.exists(): output_dir.mkdir(parents=True); return
    ensure_no_symlinks(output_dir)
    if not any(output_dir.iterdir()): return
    if not replace: raise DiagnosticsError(f"Diagnostics output is not empty: {output_dir}")
    verify_bundle(output_dir); shutil.rmtree(output_dir); output_dir.mkdir(parents=True)
def verify_bundle(output_dir):
    if not output_dir.is_dir(): raise DiagnosticsError(f"Diagnostics bundle does not exist: {output_dir}")
    ensure_no_symlinks(output_dir); manifest_path=output_dir/MANIFEST_NAME
    manifest=load_json_result(manifest_path,"diagnostics manifest")
    if manifest.get("schema_version")!=SCHEMA_VERSION or manifest.get("bundle_id")!=BUNDLE_ID: raise DiagnosticsError("Unexpected diagnostics manifest identity or schema.")
    paths=[]
    for entry in manifest.get("files",[]):
        if not isinstance(entry,dict): raise DiagnosticsError("Diagnostics manifest entries must be objects.")
        relative=normalize_relative_path(entry.get("path")); paths.append(relative)
        if not allowed_bundle_path(relative): raise DiagnosticsError(f"Diagnostics manifest contains a prohibited path: {relative}")
        path=output_dir/relative
        if not path.is_file(): raise DiagnosticsError(f"Diagnostics bundle file is missing: {relative}")
        size=path.stat().st_size
        if size>MAX_BUNDLE_FILE_BYTES: raise DiagnosticsError(f"Diagnostics bundle file is too large: {relative}")
        if size!=entry.get("size_bytes"): raise DiagnosticsError(f"Diagnostics size mismatch: {relative}")
        if sha256_file(path)!=entry.get("sha256"): raise DiagnosticsError(f"Diagnostics hash mismatch: {relative}")
        try: text=path.read_text(encoding="utf-8")
        except UnicodeDecodeError as exc: raise DiagnosticsError(f"Diagnostics bundle file is not UTF-8 text: {relative}") from exc
        assert_redacted_text(text,relative)
    if paths!=sorted(paths) or len(paths)!=len(set(paths)): raise DiagnosticsError("Diagnostics manifest paths must be sorted and unique.")
    actual=sorted(path.relative_to(output_dir).as_posix() for path in output_dir.rglob("*") if path.is_file() and path.name!=MANIFEST_NAME)
    if actual!=paths: raise DiagnosticsError("Diagnostics bundle file set does not match its manifest.")
    return manifest
