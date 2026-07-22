#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0 OR MIT
"""Implementation for capability-gated package payload staging."""
from __future__ import annotations
import datetime as dt, hashlib, os, re, sys
from pathlib import Path, PurePosixPath
from typing import Mapping
INSTALLER_ROOT=Path(__file__).resolve().parents[3]
for root in (INSTALLER_ROOT/"Bootstrapper/PackageEngine/Source",INSTALLER_ROOT/"SuiteWizard/ViewModel/Source"):
    if str(root) not in sys.path: sys.path.insert(0,str(root))
from package_engine import PackageEngineError,validate_engine_session  # noqa:E402
from wizard_view_model import AUTHORITY_FIELDS,sha256  # noqa:E402
COPY_CAPABILITY="package-engine.copy-payload"; GRANT_SCOPE="package-payload-copy-grant"; RECEIPT_SCOPE="package-payload-copy-receipt"; MAX_GRANT_SECONDS=1800
REF_RE=re.compile(r"^[a-z][a-z0-9]*(?:[.-][a-z0-9]+)+$"); HASH_RE=re.compile(r"^[0-9a-f]{64}$")
GRANT_STATEMENT="This grant authorizes only hash-verified payload staging for one exact package-engine session. It grants no process launch, elevation, installation finalization, runtime execution, deployment, save mutation, signing, publication, catalog mutation, or evidence promotion authority."
RECEIPT_STATEMENT="This receipt records a completed hash-verified copy into a new staging directory. It does not install into a product or game directory and grants no later authority."
class PackageCopyError(RuntimeError): pass
def _obj(v,l):
    if not isinstance(v,dict): raise PackageCopyError(f"{l} must be an object.")
    return v
def _arr(v,l):
    if not isinstance(v,list): raise PackageCopyError(f"{l} must be an array.")
    return v
def _text(v,l):
    if not isinstance(v,str) or not v.strip(): raise PackageCopyError(f"{l} must be non-empty text.")
    return v
def _ref(v,l):
    r=_text(v,l)
    if len(r)>128 or REF_RE.fullmatch(r) is None: raise PackageCopyError(f"{l} must be a stable namespaced logical ID.")
    return r
def _hash(v,l):
    r=_text(v,l)
    if HASH_RE.fullmatch(r) is None: raise PackageCopyError(f"{l} must be a lowercase SHA-256 value.")
    return r
def _utc(v,l):
    r=_text(v,l)
    if len(r)>64 or not r.endswith("Z") or "T" not in r: raise PackageCopyError(f"{l} must be an ISO-8601 UTC timestamp ending in Z.")
    try: p=dt.datetime.fromisoformat(r[:-1]+"+00:00")
    except ValueError as e: raise PackageCopyError(f"{l} is invalid.") from e
    if p.utcoffset()!=dt.timedelta(0): raise PackageCopyError(f"{l} must use UTC.")
    return r
def _dt(v,l): return dt.datetime.fromisoformat(_utc(v,l)[:-1]+"+00:00")
def _authority(): return {f:False for f in AUTHORITY_FIELDS}
def _rel(v,l):
    r=_text(v,l); p=PurePosixPath(r)
    if p.is_absolute() or "\\" in r or "//" in r or any(x in {"",".",".."} for x in p.parts) or len(r)>1024: raise PackageCopyError(f"{l} must be a safe relative POSIX path.")
    return r
def _no_links(path,label):
    cur=path
    while True:
        if cur.is_symlink(): raise PackageCopyError(f"{label} contains a symbolic link: {cur}")
        if cur.parent==cur: break
        cur=cur.parent
def _digest(path):
    h=hashlib.sha256(); n=0; flags=os.O_RDONLY|(os.O_NOFOLLOW if hasattr(os,"O_NOFOLLOW") else 0); fd=os.open(path,flags)
    try:
        while True:
            b=os.read(fd,1024*1024)
            if not b: break
            h.update(b); n+=len(b)
    finally: os.close(fd)
    return h.hexdigest(),n
def _plan(session): return _obj(_obj(_obj(session.get("handoff"),"session.handoff").get("receipt"),"session.handoff.receipt").get("plan"),"session.handoff.receipt.plan")
def _inventory(session):
    plan=_plan(session); rows=[]; destinations=set()
    for pi,rawp in enumerate(_arr(plan.get("packages"),"plan.packages")):
        pkg=_obj(rawp,f"plan.packages[{pi}]"); pid=_ref(pkg.get("package_id"),"package_id"); source_meta=_obj(pkg.get("source"),f"{pid}.source"); package_root=_rel(source_meta.get("path"),f"{pid}.source.path")
        for fi,raw in enumerate(_arr(pkg.get("payload"),f"{pid}.payload")):
            item=_obj(raw,f"{pid}.payload[{fi}]"); dest=_rel(item.get("destination"),"destination"); key=dest.casefold()
            if key in destinations: raise PackageCopyError(f"Duplicate or case-colliding destination: {dest}")
            destinations.add(key); size=item.get("size_bytes")
            if type(size) is not int or size<0: raise PackageCopyError("size_bytes must be non-negative.")
            relative_source=_rel(item.get("source"),"source"); combined=PurePosixPath(package_root)/PurePosixPath(relative_source)
            rows.append({"package_id":pid,"source":combined.as_posix(),"destination":dest,"sha256":_hash(item.get("sha256"),"sha256"),"size_bytes":size,"redistribution":_text(item.get("redistribution"),"redistribution")})
    rows.sort(key=lambda r:(str(r["destination"]).casefold(),str(r["destination"]))); summary=_obj(plan.get("summary"),"plan.summary")
    if len(rows)!=summary.get("payload_file_count") or sum(int(r["size_bytes"]) for r in rows)!=summary.get("payload_size_bytes"): raise PackageCopyError("Payload inventory does not match resolver summary.")
    return rows
def build_copy_grant(session:Mapping[str,object],*,issuer:str,issued_at_utc:str,expires_at_utc:str,nonce:str):
    try: checked=validate_engine_session(session)
    except PackageEngineError as e: raise PackageCopyError(f"Package-engine session verification failed: {e}") from e
    issued=_dt(issued_at_utc,"issued_at_utc"); expires=_dt(expires_at_utc,"expires_at_utc"); accepted=_dt(checked["accepted_at_utc"],"session.accepted_at_utc")
    if issued<accepted: raise PackageCopyError("issued_at_utc must not precede session acceptance.")
    if expires<=issued or expires-issued>dt.timedelta(seconds=MAX_GRANT_SECONDS): raise PackageCopyError("Copy grant expiry must be after issuance and within 30 minutes.")
    inventory=_inventory(checked); base={"schema_version":1,"grant_scope":GRANT_SCOPE,"session_sha256":checked["session_sha256"],"capability":COPY_CAPABILITY,"issuer":_text(issuer,"issuer"),"issued_at_utc":_utc(issued_at_utc,"issued_at_utc"),"expires_at_utc":_utc(expires_at_utc,"expires_at_utc"),"nonce":_ref(nonce,"nonce"),"inventory_sha256":sha256(inventory),"session":checked,"statement":GRANT_STATEMENT,"authority":_authority()}
    return {**base,"grant_sha256":sha256(base)}
def validate_copy_grant(grant:Mapping[str,object]):
    d=dict(grant)
    if d.get("schema_version")!=1 or d.get("grant_scope")!=GRANT_SCOPE or d.get("capability")!=COPY_CAPABILITY or d.get("statement")!=GRANT_STATEMENT: raise PackageCopyError("Copy grant contract is invalid.")
    declared=_hash(d.get("grant_sha256"),"grant_sha256"); unsigned={k:v for k,v in d.items() if k!="grant_sha256"}
    if sha256(unsigned)!=declared: raise PackageCopyError("Copy grant fingerprint does not match its content.")
    expected=build_copy_grant(_obj(d.get("session"),"grant.session"),issuer=_text(d.get("issuer"),"issuer"),issued_at_utc=_utc(d.get("issued_at_utc"),"issued_at_utc"),expires_at_utc=_utc(d.get("expires_at_utc"),"expires_at_utc"),nonce=_ref(d.get("nonce"),"nonce"))
    if d!=expected: raise PackageCopyError("Copy grant is stale, altered, or not canonically derived.")
    return d
def _remove_tree(root):
    if not root.exists() or root.is_symlink(): return
    for child in sorted(root.rglob("*"),key=lambda p:len(p.parts),reverse=True):
        if child.is_file() or child.is_symlink(): child.unlink()
        elif child.is_dir(): child.rmdir()
    root.rmdir()
def stage_payload(grant:Mapping[str,object],source_root:Path,staging_root:Path,*,copied_at_utc:str):
    checked=validate_copy_grant(grant); copied=_dt(copied_at_utc,"copied_at_utc")
    if copied<_dt(checked["issued_at_utc"],"issued_at_utc"): raise PackageCopyError("copied_at_utc must not precede grant issuance.")
    if copied>_dt(checked["expires_at_utc"],"expires_at_utc"): raise PackageCopyError("Copy grant expired before staging began.")
    source=Path(source_root); target=Path(staging_root)
    if not source.is_dir() or source.is_symlink(): raise PackageCopyError("Source root must be an existing non-symlink directory.")
    _no_links(source,"Source root")
    if target.exists() or target.is_symlink(): raise PackageCopyError("Staging root must not already exist.")
    if not target.parent.is_dir() or target.parent.is_symlink(): raise PackageCopyError("Staging parent must be an existing non-symlink directory.")
    _no_links(target.parent,"Staging parent"); temp=target.parent/f".{target.name}.{checked['grant_sha256']}.tmp"
    if temp.exists() or temp.is_symlink(): raise PackageCopyError("Deterministic temporary staging directory already exists.")
    inventory=_inventory(_obj(checked.get("session"),"grant.session")); temp.mkdir(mode=0o700); copied_rows=[]
    try:
        for row in inventory:
            src=source.joinpath(*PurePosixPath(str(row["source"])).parts); dst=temp.joinpath(*PurePosixPath(str(row["destination"])).parts)
            if src.is_symlink() or not src.is_file(): raise PackageCopyError(f"Payload source is not a regular non-symlink file: {row['source']}")
            _no_links(src.parent,"Payload source"); h,n=_digest(src)
            if h!=row["sha256"] or n!=row["size_bytes"]: raise PackageCopyError(f"Payload source hash or size mismatch: {row['source']}")
            dst.parent.mkdir(parents=True,exist_ok=True); flags=os.O_WRONLY|os.O_CREAT|os.O_EXCL|(os.O_NOFOLLOW if hasattr(os,"O_NOFOLLOW") else 0); out=os.open(dst,flags,0o600); inp=os.open(src,os.O_RDONLY|(os.O_NOFOLLOW if hasattr(os,"O_NOFOLLOW") else 0))
            try:
                while True:
                    b=os.read(inp,1024*1024)
                    if not b: break
                    view=memoryview(b)
                    while view:
                        w=os.write(out,view)
                        if w<=0: raise PackageCopyError("Payload copy made no forward progress.")
                        view=view[w:]
                os.fsync(out)
            finally: os.close(inp); os.close(out)
            if _digest(dst)!=(row["sha256"],row["size_bytes"]): raise PackageCopyError(f"Staged payload verification failed: {row['destination']}")
            copied_rows.append(dict(row))
        os.rename(temp,target)
    except Exception:
        _remove_tree(temp); raise
    base={"schema_version":1,"receipt_scope":RECEIPT_SCOPE,"grant_sha256":checked["grant_sha256"],"session_sha256":checked["session_sha256"],"inventory_sha256":checked["inventory_sha256"],"copied_at_utc":_utc(copied_at_utc,"copied_at_utc"),"staging_reference":_ref("staging.foa-sdk.payload","staging_reference"),"file_count":len(copied_rows),"size_bytes":sum(int(r["size_bytes"]) for r in copied_rows),"files":copied_rows,"copy_performed":True,"process_launched":False,"elevation_requested":False,"installation_finalized":False,"statement":RECEIPT_STATEMENT,"authority":_authority()}
    return {**base,"copy_receipt_sha256":sha256(base)}
__all__=["COPY_CAPABILITY","PackageCopyError","build_copy_grant","validate_copy_grant","stage_payload"]
