import sys, os, re, json, argparse
import yaml

MAP="docs/modules/MODULE_MAP.yaml"
REGDIR="docs/modules/registry"
BASELINE=os.path.join(REGDIR,"module_id_migration_baseline.json")
NL="\n"

def load(repo):
    m=yaml.safe_load(open(os.path.join(repo,MAP),encoding="utf-8")) or {}
    map_ids={}
    for e in (m.get("modules") or []):
        mid=e.get("module_id")
        if mid: map_ids[mid]={"aliases":list(e.get("aliases") or []),"id":e.get("id"),"entry":e}
    pages={}
    for fn in sorted(os.listdir(os.path.join(repo,REGDIR))):
        if not fn.endswith(".md"): continue
        txt=open(os.path.join(repo,REGDIR,fn),encoding="utf-8").read()
        fm={}
        if txt.startswith("---"):
            end=txt.find(NL+"---",3)
            fm=yaml.safe_load(txt[3:end]) or {}
        pages[fn]={"module_id":fm.get("module_id"),"aliases":list(fm.get("aliases") or []),"class":fm.get("class")}
    return map_ids,pages

def _read(p):
    try: return open(p,encoding="utf-8",errors="ignore").read()
    except Exception: return None

def compute(repo,inject=None):
    map_ids,pages=load(repo)
    picks={}
    if inject=="drop-page-module":
        k=sorted(pages)[0]; picks["page"]=k; pages[k]["module_id"]=None
    elif inject=="unknown-page-module":
        k=sorted(pages)[0]; picks["page"]=k; picks["id"]="astrocs.p9.nonexistent"; pages[k]["module_id"]=picks["id"]
    elif inject=="dup-alias":
        ks=sorted(map_ids)[:2]; picks["module"]=ks[0]; picks["module2"]=ks[1]; picks["alias"]="astrocs.phase9.dup"
        for k in ks: map_ids[k]["aliases"].append(picks["alias"])
    elif inject=="alias-collides":
        k=sorted(map_ids)[0]; picks["module"]=k; picks["alias"]=sorted(map_ids)[1]; map_ids[k]["aliases"].append(picks["alias"])
    elif inject=="unmapped-module":
        picks["module"]="astrocs.p9.orphan"; map_ids[picks["module"]]={"aliases":[],"id":"orphan","entry":{"module_id":picks["module"]}}
    elif inject=="authority-missing":
        picks["module"]="astrocs.p9.fake"
        map_ids[picks["module"]]={"aliases":[],"id":"fake","entry":{"module_id":picks["module"],"added_by":"MOD-002","target_dir":"lib/algorithms/__no_such_dir__","module_yaml":"lib/algorithms/__no_such_dir__/module.yaml","authority_note":"fault-inject probe"}}
    errs=[]
    for fn,p in pages.items():
        if p["class"]=="non_module": continue
        if not p["module_id"]: errs.append("PAGE_MISSING_MODULE_ID %s"%fn)
    for fn,p in pages.items():
        if p["module_id"] and p["module_id"] not in map_ids:
            errs.append("PAGE_UNKNOWN_MODULE_ID %s -> %s"%(fn,p["module_id"]))
    covered={p["module_id"] for p in pages.values() if p["module_id"]}
    for mid in map_ids:
        if mid not in covered: errs.append("MODULE_WITHOUT_PAGE %s"%mid)
    claimed={}
    for mid,meta in map_ids.items():
        for a in meta["aliases"]:
            if a in map_ids: errs.append("ALIAS_COLLIDES_WITH_CANONICAL %s: %s"%(mid,a))
            if a in claimed: errs.append("ALIAS_DOUBLE_CLAIM %s: %s & %s"%(a,claimed[a],mid))
            claimed[a]=mid
    for mid,meta in map_ids.items():
        e=meta["entry"]
        if e.get("added_by")!="MOD-002": continue
        td=e.get("target_dir"); my=e.get("module_yaml"); tf=e.get("target_file"); tg=e.get("target")
        if not td or not os.path.isdir(os.path.join(repo,td)):
            errs.append("AUTHORITY_MISSING target_dir %s: %s"%(mid,td))
        if not my or not os.path.isfile(os.path.join(repo,my)):
            errs.append("AUTHORITY_MISSING module_yaml %s: %s"%(mid,my))
        else:
            body=_read(os.path.join(repo,my)) or ""
            if mid not in body: errs.append("AUTHORITY_MISSING module_id-not-in-module_yaml %s"%mid)
        if tf and os.path.isfile(os.path.join(repo,tf)) and tg:
            if tg not in (_read(os.path.join(repo,tf)) or ""):
                errs.append("AUTHORITY_MISSING target-not-in-target_file %s: %s"%(mid,tg))
        if not e.get("authority_note"):
            errs.append("AUTHORITY_MISSING authority_note %s"%mid)
    return errs,{"map_ids":len(map_ids),"pages":len(pages),"pages_with_module_id":len(covered),"aliases":len(claimed)},picks

def counts(errs):
    c={}
    for e in errs:
        c[e.split()[0]]=c.get(e.split()[0],0)+1
    return c

EXPECT={"drop-page-module":"PAGE_MISSING_MODULE_ID","unknown-page-module":"PAGE_UNKNOWN_MODULE_ID",
        "dup-alias":"ALIAS_DOUBLE_CLAIM","alias-collides":"ALIAS_COLLIDES_WITH_CANONICAL",
        "unmapped-module":"MODULE_WITHOUT_PAGE","authority-missing":"AUTHORITY_MISSING"}

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument("--root",default=".")
    ap.add_argument("--fault-inject",default=None)
    ap.add_argument("--json-out",default=None)
    ap.add_argument("--self-test",action="store_true")
    a=ap.parse_args()
    if a.self_test:
        ok=True
        for case,cls in EXPECT.items():
            errs,stat,picks=compute(a.root,case)
            hit=[e for e in errs if e.startswith(cls) and any(str(v) in e for v in picks.values())]
            active=bool(hit) and bool(errs)
            print("SELFTEST %-20s expect=%-30s rc=%d errors=%d named=%d %s"%(case,cls,1 if errs else 0,len(errs),len(hit),"ACTIVE" if active else "DEAD(空匹配)"))
            for e in hit[:2]: print("    seen:",e)
            ok = ok and active
        print("SELFTEST_SUMMARY","PASS" if ok else "FAIL")
        sys.exit(0 if ok else 1)
    errs,stat,picks=compute(a.root,a.fault_inject)
    cur=counts(errs)
    bp=os.path.join(a.root,BASELINE)
    base=json.load(open(bp,encoding="utf-8")) if (os.path.exists(bp) and not a.fault_inject) else None
    over=[]
    if base is None:
        if errs: over=["NO_BASELINE: 全量强制，任何 error 即红"]+sorted(cur)
    else:
        bcls=base.get("classes") or {}
        for k,v in sorted(cur.items()):
            if v > int(bcls.get(k,0)): over.append("%s %d > baseline %d"%(k,v,int(bcls.get(k,0))))
    status="FAIL" if over else "PASS"
    out={"tool":"check_module_id_normalization","inject":a.fault_inject,"status":status,
         "baseline":(base or {}).get("baseline_id"),"current":cur,"baseline_classes":(base or {}).get("classes"),
         "stats":stat,"over_baseline":over,"errors":errs,"examples":errs[:5]}
    if a.json_out:
        # 产物目录不存在时自建：缺目录不是「无违规」，抛 traceback 会把工具故障
        # 报成判据结果（01_CHECKS §1：不得 traceback、不得静默降级）。
        import os as _os
        _d = _os.path.dirname(a.json_out)
        if _d: _os.makedirs(_d, exist_ok=True)
        open(a.json_out,"w",encoding="utf-8").write(json.dumps(out,ensure_ascii=False,indent=1))
    print(json.dumps({k:out[k] for k in ("tool","status","baseline","current","baseline_classes","over_baseline","stats")},ensure_ascii=False))
    for e in errs[:5]: print("  -",e)
    sys.exit(1 if over else 0)

if __name__=="__main__":
    main()
