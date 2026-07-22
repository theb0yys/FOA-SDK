# SPDX-License-Identifier: Apache-2.0 OR MIT
from __future__ import annotations
import argparse, copy, hashlib, json, math, re, sys
from pathlib import Path

SCHEMA_RELATIVE=Path("docs/tainted-grail-sdk/schemas/interchange/v1/manifest.tginterchange.schema.json")
DOCUMENTS_EXAMPLE_RELATIVE=Path("docs/tainted-grail-sdk/schemas/interchange/v1/examples/minimal-documents/manifest.tginterchange.json")
ASSET_EXAMPLE_RELATIVE=Path("docs/tainted-grail-sdk/schemas/interchange/v1/examples/minimal-asset/manifest.tginterchange.json")
README_RELATIVE=Path("docs/tainted-grail-sdk/schemas/interchange/v1/README.md")
CANONICALIZATION_RELATIVE=Path("docs/tainted-grail-sdk/schemas/interchange/v1/CANONICALIZATION.md")
MIGRATION_RELATIVE=Path("docs/tainted-grail-sdk/schemas/interchange/v1/MIGRATION.md")
TYPES_HEADER_RELATIVE=Path("Gems/TaintedGrailModdingSDK/Code/Source/CanonicalInterchangeTypes.h")
TYPES_SOURCE_RELATIVE=Path("Gems/TaintedGrailModdingSDK/Code/Source/CanonicalInterchangeTypes.cpp")
CANONICAL_SOURCE_RELATIVE=Path("Gems/TaintedGrailModdingSDK/Code/Source/CanonicalInterchangeCanonical.cpp")
VALIDATION_HEADER_RELATIVE=Path("Gems/TaintedGrailModdingSDK/Code/Source/CanonicalInterchangeValidation.h")
MIGRATION_HEADER_RELATIVE=Path("Gems/TaintedGrailModdingSDK/Code/Source/CanonicalInterchangeMigration.h")

ORDER=["schema_version","schema_profile","canonical_profile","package_kind","package_id","pack_id","display_name","producer","intended_consumers","toolchain_lock","spatial_basis","temporal_basis","documents","assets","identity_mappings","bindings","payloads","transformations","losses","provenance","licensing","evidence_refs","extensions"]
REQUIRED=[x for x in ORDER if x!="display_name"]
ENUMS={
"packageKind":["authoring-interchange"],
"hostKind":["o3de","blender","unity-editor","host-neutral"],
"bindingState":["unresolved","active","stale","conflicted","superseded","retired"],
"mappingOperation":["rename","move","duplicate","fork","replace","merge","split","aggregate","tombstone"],
"mappingCompleteness":["incomplete","complete"],
"payloadRole":["canonical-document","scene-source","mesh-source","texture-source","material-map","skeleton-source","animation-source","collider-source","metadata","preview","licence-notice"],
"transformationPhase":["authoring","export","import","normalization","reverse-export","migration"],
"reversibility":["reversible","conditionally-reversible","irreversible","unknown"],
"lossSeverity":["information","warning","error","blocker"],
"sourceKind":["project-owned","synthetic","user-supplied-reviewed","user-supplied-unreviewed","local-reference-only","upstream-reference","prohibited"],
"redistributionState":["allowed","local-only","review-required","blocked"],
"parameterKind":["boolean","signed-integer","unsigned-integer","number","string","matrix4x4","rational-rate"]}
LIMITS={"intended_consumers":64,"documents":4096,"assets":4096,"identity_mappings":8192,"bindings":16384,"payloads":8192,"transformations":16384,"losses":16384,"provenance":8192,"licensing":8192,"evidence_refs":8192,"extensions":64}
CPP=["SchemaVersionV1 = 1;",'SchemaProfileV1 = "foa-interchange-schema-v1";','CanonicalProfileV1 = "foa-interchange-canonical-json-v1";',"MaxCanonicalManifestBytesV1 = 8 * 1024 * 1024;","MaxCanonicalDepthV1 = 32;","MaxSemanticTokenBytesV1 = 128;","MaxRelativePathBytesV1 = 512;","MaxPresentationStringBytesV1 = 4096;","MaxSourceIdentityBytesV1 = 2048;","MaxExtensionCanonicalBytesV1 = 1024 * 1024;","MaxConsumersV1 = 64;","MaxToolchainComponentsV1 = 128;","MaxDocumentsV1 = 4096;","MaxAssetsV1 = 4096;","MaxIdentityMappingsV1 = 8192;","MaxBindingsV1 = 16384;","MaxPayloadsV1 = 8192;","MaxTransformationsV1 = 16384;","MaxLossesV1 = 16384;","MaxProvenanceRecordsV1 = 8192;","MaxLicensingRecordsV1 = 8192;","MaxEvidenceReferencesV1 = 8192;","MaxExtensionNamespacesV1 = 64;","MaxReferencesPerRecordV1 = 256;","MaxPayloadBytesV1 = 16ULL * 1024 * 1024 * 1024;","MaxDeclaredPackageBytesV1 = 256ULL * 1024 * 1024 * 1024;"]
MIGRATION_CODES=["interchange.migration.source-invalid","interchange.migration.unavailable","interchange.migration.semantic-drift"]
MIGRATION_STATUSES=["already-current","source-invalid","unsupported-source-version","unsupported-target-version","downgrade-forbidden","migrator-unavailable","semantic-drift","succeeded"]
IDS={"documents":"document_id","assets":"asset_id","identity_mappings":"mapping_id","bindings":"binding_id","payloads":"payload_id","transformations":"transformation_id","losses":"loss_id","provenance":"provenance_id","licensing":"licensing_id","evidence_refs":"evidence_ref_id"}

def compact(v): return json.dumps(v,ensure_ascii=False,separators=(",",":"),allow_nan=False)
def digest(v): return hashlib.sha256(v.encode()).hexdigest()
def ref(root,r):
    v=root
    for p in r[2:].split("/"): v=v[p.replace("~1","/").replace("~0","~")]
    return v
def semantic_id(v):
    return 3<=len(v.encode())<=128 and re.fullmatch(r"[a-z][a-z0-9]*(?:[._-][a-z0-9]+)+",v) is not None
def relative_path(v):
    if not v or len(v.encode())>512 or v[0]=="/" or v[-1]=="/" or "\\" in v or ":" in v:return False
    for s in v.split("/"):
        stem=s.split(".",1)[0].lower()
        if not s or s in {".",".."} or s.endswith(("."," ")) or stem in {"con","prn","aux","nul"} or re.fullmatch(r"(com|lpt)[1-9]",stem):return False
        if any(ord(c)<32 or ord(c)>126 for c in s):return False
    return True
def format_ok(name,v):
    if name=="semantic-id-v1":return semantic_id(v)
    if name=="optional-semantic-id-v1":return v=="" or semantic_id(v)
    if name=="version-token-v1":return bool(v) and len(v.encode())<=128 and re.fullmatch(r"[a-z0-9._-]+",v)!=None
    if name=="optional-version-token-v1":return len(v.encode())<=128 and re.fullmatch(r"[a-z0-9._-]*",v)!=None
    if name=="sha256-v1":return re.fullmatch(r"[0-9a-f]{64}",v)!=None
    if name=="optional-sha256-v1":return v=="" or re.fullmatch(r"[0-9a-f]{64}",v)!=None
    if name=="relative-package-path-v1":return relative_path(v)
    return True
def validate_instance(v,s,root=None,path="$"):
    root=root or s
    if "$ref" in s:return validate_instance(v,ref(root,s["$ref"]),root,path)
    e=[];t=s.get("type")
    ok={"object":lambda:isinstance(v,dict),"array":lambda:isinstance(v,list),"string":lambda:isinstance(v,str),"integer":lambda:isinstance(v,int) and not isinstance(v,bool),"number":lambda:isinstance(v,(int,float)) and not isinstance(v,bool) and math.isfinite(v),"boolean":lambda:isinstance(v,bool)}
    if t and not ok[t]():return [f"{path}: expected {t}"]
    if "const" in s and v!=s["const"]:e.append(f"{path}: constant mismatch")
    if "enum" in s and v not in s["enum"]:e.append(f"{path}: closed enum mismatch")
    if isinstance(v,str):
        if len(v)<s.get("minLength",0) or len(v)>s.get("maxLength",10**12):e.append(f"{path}: string length")
        if len(v.encode())>s.get("x-foa-byte-max",10**12):e.append(f"{path}: UTF-8 byte length")
        if "pattern" in s and re.search(s["pattern"],v) is None:e.append(f"{path}: pattern")
        if not format_ok(s.get("x-foa-format"),v):e.append(f"{path}: {s.get('x-foa-format')}")
    if isinstance(v,(int,float)) and not isinstance(v,bool):
        if v<s.get("minimum",-math.inf) or v>s.get("maximum",math.inf):e.append(f"{path}: numeric range")
    if isinstance(v,list):
        if len(v)<s.get("minItems",0) or len(v)>s.get("maxItems",10**12):e.append(f"{path}: array length")
        for i,x in enumerate(v):e+=validate_instance(x,s.get("items",{}),root,f"{path}[{i}]")
    if isinstance(v,dict):
        p=s.get("properties",{})
        for x in s.get("required",[]):
            if x not in v:e.append(f"{path}: missing required property {x}")
        if s.get("additionalProperties") is False:
            for x in v:
                if x not in p:e.append(f"{path}: unknown property {x}")
        for x,y in v.items():
            if x in p:e+=validate_instance(y,p[x],root,f"{path}.{x}")
    return e
def closed(s,path="$"):
    e=[]
    if isinstance(s,dict):
        if s.get("type")=="object" and s.get("additionalProperties") is not False:e.append(f"{path}: object not closed")
        for k,v in s.items():e+=closed(v,f"{path}.{k}")
    elif isinstance(s,list):
        for i,v in enumerate(s):e+=closed(v,f"{path}[{i}]")
    return e
def semantic_payload(p):
    return {"payload_id":p["payload_id"],"role":p["role"],"media_type":p["media_type"],"byte_size":p["byte_size"],"digest":p["digest"],"dependency_ids":sorted(set(p["dependency_ids"])),"subject_ids":sorted(set(p["subject_ids"]))}
def document_projection(m,d):
    p={x["payload_id"]:x for x in m["payloads"]}[d["payload_id"]]
    return {"schema_profile":m["schema_profile"],"document_id":d["document_id"],"document_kind":d["document_kind"],"document_schema_id":d["document_schema_id"],"document_schema_version":d["document_schema_version"],"payload":semantic_payload(p),"subject_refs":sorted(set(d["subject_refs"])),"asset_ids":sorted(set(d["asset_ids"]))}
def asset_projection(m,a):
    ps={x["payload_id"]:x for x in m["payloads"]};ds={x["document_id"]:x for x in m["documents"]}
    payloads=sorted([semantic_payload(ps[x]) for x in a["payload_ids"]],key=lambda x:x["payload_id"])
    documents=sorted([{"document_id":x,"revision_fingerprint":digest(compact(document_projection(m,ds[x])))} for x in a["document_ids"]],key=lambda x:x["document_id"])
    ts=[copy.deepcopy(x) for x in m["transformations"] if a["asset_id"] in x["subject_ids"]]
    ls=[copy.deepcopy(x) for x in m["losses"] if x["subject_id"]==a["asset_id"]]
    for x in ts+ls:x.pop("evidence_ref_ids",None)
    return {"schema_profile":m["schema_profile"],"asset_id":a["asset_id"],"asset_kind":a["asset_kind"],"payloads":payloads,"documents":documents,"subject_refs":sorted(set(a["subject_refs"])),"transformations":ts,"losses":ls,"extensions":[]}
def validate_example_semantics(m,label):
    e=[];ps={x["payload_id"]:x for x in m["payloads"]};ds={x["document_id"]:x for x in m["documents"]};assets={x["asset_id"]:x for x in m["assets"]}
    for c,k in IDS.items():
        values=[x[k] for x in m[c]]
        if len(values)!=len(set(values)):e.append(f"{label}.{c}: duplicate identities")
        if c not in {"identity_mappings","transformations","losses"} and values!=sorted(values):e.append(f"{label}.{c}: order")
    paths=[x["relative_path"].lower() for x in m["payloads"]]
    if len(paths)!=len(set(paths)):e.append(f"{label}.payloads: case collision")
    if sum(x["byte_size"] for x in m["payloads"])>256*1024**3:e.append(f"{label}.payloads: package limit")
    for p in m["payloads"]:
        for x in p["dependency_ids"]:
            if x not in ps:e.append(f"{label}: missing dependency {x}")
    for d in m["documents"]:
        if d["payload_id"] not in ps:e.append(f"{label}: missing payload {d['payload_id']}");continue
        for x in d["asset_ids"]:
            if x not in assets:e.append(f"{label}: missing asset {x}")
        if d["revision_fingerprint"]!=digest(compact(document_projection(m,d))):e.append(f"{label}: document revision fingerprint mismatch")
    for a in m["assets"]:
        missing=[x for x in a["payload_ids"] if x not in ps]+[x for x in a["document_ids"] if x not in ds]
        if missing:e.append(f"{label}: missing asset references {missing}")
        elif a["revision_fingerprint"]!=digest(compact(asset_projection(m,a))):e.append(f"{label}: asset revision fingerprint mismatch")
    return e
def load_json(path,e):
    try:
        raw=path.read_text(encoding="utf-8")
        if raw.startswith("\ufeff"):raise ValueError("BOM")
        return json.loads(raw)
    except (OSError,ValueError,json.JSONDecodeError) as x:e.append(f"{path}: {x}");return None
def validate_repository(root):
    e=[];paths=[SCHEMA_RELATIVE,DOCUMENTS_EXAMPLE_RELATIVE,ASSET_EXAMPLE_RELATIVE,README_RELATIVE,CANONICALIZATION_RELATIVE,MIGRATION_RELATIVE,TYPES_HEADER_RELATIVE,TYPES_SOURCE_RELATIVE,CANONICAL_SOURCE_RELATIVE,VALIDATION_HEADER_RELATIVE,MIGRATION_HEADER_RELATIVE]
    for p in paths:
        if not (root/p).is_file():e.append(f"missing required file: {p}")
    if e:return e
    s=load_json(root/SCHEMA_RELATIVE,e)
    if not isinstance(s,dict):return e
    if s.get("$schema")!="https://json-schema.org/draft/2020-12/schema":e.append("schema dialect")
    if s.get("$id")!="https://foa-sdk.local/schemas/interchange/v1/manifest.tginterchange.schema.json":e.append("schema id")
    if list(s.get("properties",{}))!=ORDER:e.append("schema property order")
    if s.get("required")!=REQUIRED:e.append("schema required")
    e+=closed(s)
    for k,v in LIMITS.items():
        if s["properties"].get(k,{}).get("maxItems")!=v:e.append(f"schema limit {k}")
    for k,v in ENUMS.items():
        if s["$defs"].get(k,{}).get("enum")!=v:e.append(f"schema enum {k}")
    th=(root/TYPES_HEADER_RELATIVE).read_text();ts=(root/TYPES_SOURCE_RELATIVE).read_text();cs=(root/CANONICAL_SOURCE_RELATIVE).read_text();vh=(root/VALIDATION_HEADER_RELATIVE).read_text();mh=(root/MIGRATION_HEADER_RELATIVE).read_text()
    for x in CPP:
        if x not in th:e.append(f"C++ constant {x}")
    for x in {y for z in ENUMS.values() for y in z}|set(MIGRATION_STATUSES):
        if f'"{x}"' not in ts:e.append(f"C++ token {x}")
    for x in MIGRATION_CODES:
        if x not in vh:e.append(f"migration code {x}")
    for x in ["MigrationResultV1","MigrateCanonicalInterchange","ValidateMigrationCandidateV1"]:
        if x not in mh:e.append(f"migration surface {x}")
    a=cs.find("AZStd::string SerializeCanonicalManifestV1");b=cs.find("return output;",a)
    fields=re.findall(r'object\.(?:Unsigned|String|Token|Object|Array)\("([^"]+)"',cs[a:b])
    if fields!=ORDER:e.append(f"canonical writer order {fields}")
    docs=[(README_RELATIVE,(root/README_RELATIVE).read_text()),(CANONICALIZATION_RELATIVE,(root/CANONICALIZATION_RELATIVE).read_text()),(MIGRATION_RELATIVE,(root/MIGRATION_RELATIVE).read_text())]
    for p,t in docs:
        for x in ["foa-interchange-schema-v1","foa-interchange-canonical-json-v1","Gate 6 remains closed"]:
            if x not in t:e.append(f"{p}: {x}")
    mt=docs[-1][1]
    for x in MIGRATION_STATUSES[:-1]:
        if x not in mt:e.append(f"migration doc {x}")
    if "no code path returns `succeeded`" not in mt:e.append("migration succeeded prohibition")
    for p,label in [(DOCUMENTS_EXAMPLE_RELATIVE,"minimal-documents"),(ASSET_EXAMPLE_RELATIVE,"minimal-asset")]:
        m=load_json(root/p,e)
        if not isinstance(m,dict):continue
        if (root/p).read_text()!=compact(m):e.append(f"{label}: not compact canonical JSON")
        if list(m)!=[x for x in ORDER if x!="display_name"]:e.append(f"{label}: property order")
        e += [f"{label}: {x}" for x in validate_instance(m,s)]
        e += validate_example_semantics(m,label)
    return e
def main(argv=None):
    p=argparse.ArgumentParser();p.add_argument("--repository-root",type=Path,default=Path(__file__).resolve().parents[3]);a=p.parse_args(argv)
    e=validate_repository(a.repository_root.resolve())
    if e:
        print("Canonical interchange Schema-1 validation failed:",file=sys.stderr)
        for x in e:print(f"- {x}",file=sys.stderr)
        return 1
    print("Canonical interchange Schema-1 package is consistent.");return 0
if __name__=="__main__":raise SystemExit(main())
