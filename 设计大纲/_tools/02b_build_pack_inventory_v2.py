#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
层1-B（v2）控制包取证脚本：机械清点，不做解释。
覆盖四态：worktree 解包件 / zip 原件（逐个 zip 一条） / 影子树副本 / 仅存 Git 历史（复原）。
包身份归一：去掉复原前缀与 .zip 后缀；容器型目录（内含子包）单独标记 nested_packs。
输出：
  _evidence/packs/pack_inventory.csv      身份 x 存形态 清单
  _evidence/packs/pack_instances.json     每身份的完整机械摘要
  _evidence/packs/pack_files.jsonl        每实例文件清单
  _evidence/packs/digest/<id>.md          每身份取证摘要（供子子代理阅读）
  _evidence/packs/history/<rev8>/…        历史仅存包复原件（附 _PROVENANCE.json）
  _evidence/packs/history_zip/<name>.zip  历史仅存 zip 复原（附 .provenance.json）
"""
import subprocess, os, re, sys, json, csv, hashlib, zipfile, io
from collections import defaultdict

REPO = "/workspace/Astro CS Database"
P = os.path.join(REPO, "设计大纲/_evidence/packs")
HIST = os.path.join(P, "history"); HZIP = os.path.join(P, "history_zip"); DIG = os.path.join(P, "digest")
EXCLUDE = ("run/", "build/", "out/", "worktrees/", ".pytest_cache/", "third_party/", "GaiaDR3/",
           "GaiaDR3SP/", "BASS DR3/", "testdata/", "AstroCS.wiki/", "logs/", "Testing/", "dist/", ".git", "设计大纲/")
MARKERS = ("00_READ_FIRST.md", "control-pack.json", "MANIFEST.json", "START_PROMPT.txt",
           "00_AGENT_START_PROMPT.txt", "AUTONOMOUS_ENTRY.md")
LEDGER_RE = re.compile(r"(^|/)([0-9]{2}_)?TASK_LEDGER[^/]*\.csv$")
PKG_RE = re.compile(r"(CONTROL_V\d|_CONTROL_|CONTROL_PACKAGE|Development_Pack|Agent_Package|_agent_package|_new_pack|HISS|REAUDIT|REVIEWPACK|AUDIT_SUPPLEMENT|AUDIT_PACK|RESCUE|Delivery_|engineering_v1\.|engineering_archive|engineering_authoritative|cprun|Wiki_Freeze|CP0)")
os.makedirs(HIST, exist_ok=True); os.makedirs(HZIP, exist_ok=True); os.makedirs(DIG, exist_ok=True)
warnf = open(os.path.join(P, "_warnings.txt"), "a", encoding="utf-8")
def warn(m): warnf.write(m + "\n"); warnf.flush()
def git(args, timeout=1800):
    return subprocess.run(["git", "-c", "core.quotePath=false"] + args, cwd=REPO,
                          capture_output=True, text=True, errors="replace", timeout=timeout).stdout
def gitbin(args, timeout=600):
    return subprocess.run(["git", "-c", "core.quotePath=false"] + args, cwd=REPO, capture_output=True, timeout=timeout)
def sha256_file(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for b in iter(lambda: f.read(1 << 20), b""): h.update(b)
    return h.hexdigest()
def excluded(rel):
    return rel == ".git" or any(rel == x.rstrip("/") or rel.startswith(x) for x in EXCLUDE)

def norm_identity(path):
    p = path
    p = re.sub(r"^packs/", "", p)
    p = re.sub(r"^history/[0-9a-f]{6,8}/", "", p)
    p = re.sub(r"^history_zip/", "", p)
    p = re.sub(r"\.zip$", "", p)
    parts = [x for x in p.split("/") if x]
    if not parts: return "REPO_ROOT"
    base = parts[-1]
    if base.startswith("AstroCS_") or "CONTROL" in base.upper() or base.upper().startswith("ACR") or "PACK" in base.upper():
        return base
    return "/".join(parts[-2:]) if len(parts) > 1 else base

# ---------- 1. worktree 包目录（不剪枝，允许嵌套） ----------
print("[1] 扫描 worktree", flush=True)
pack_dirs = []
for root, dirs, files in os.walk(REPO):
    rel = os.path.relpath(root, REPO)
    if excluded(rel):
        dirs[:] = []; continue
    if any(f in MARKERS for f in files):
        pack_dirs.append(rel)
print("   包目录 %d 个" % len(pack_dirs), flush=True)

def scan_pack(absdir, rel, form):
    out = {"form": form, "path": rel, "abs": absdir, "files": [], "nfile": 0, "bytes": 0}
    for root, dirs, files in os.walk(absdir):
        dirs[:] = [d for d in dirs if d not in ("__pycache__", ".git")]
        for fn in files:
            ap = os.path.join(root, fn)
            rp = os.path.relpath(ap, absdir).replace(os.sep, "/")
            try: sz = os.path.getsize(ap)
            except OSError: sz = 0
            out["files"].append((rp, sz)); out["nfile"] += 1; out["bytes"] += sz
    out["files"].sort()
    out["identity"] = norm_identity(rel)
    return out

inst = []
for rel in sorted(pack_dirs):
    inst.append(scan_pack(os.path.join(REPO, rel), rel, "shadow_copy" if rel.startswith("run/") else "worktree"))
pdset = set(pack_dirs)
for it in inst:
    par = it["path"]
    it["parent_pack"] = next((q for q in pdset if q != par and par.startswith(q + "/")), None)
    it["nested_packs"] = sorted(q for q in pdset if q != par and q.startswith(par + "/"))

# ---------- 2. 现存 zip 原件（逐个一条） ----------
print("[2] 扫描 zip", flush=True)
zip_dirs_seen = set()
for root, dirs, files in os.walk(REPO):
    rel = os.path.relpath(root, REPO)
    if excluded(rel) or re.search(r"(capsules|ci_artifacts|logs/round|history_zip)", rel):
        dirs[:] = []; continue
    for fn in sorted(files):
        if not fn.lower().endswith(".zip"): continue
        if not PKG_RE.search(fn): continue
        ap = os.path.join(root, fn); rp = os.path.relpath(ap, REPO)
        try:
            with zipfile.ZipFile(ap) as z:
                names = [(i.filename, i.file_size) for i in z.infolist() if not i.is_dir()]
            it = {"form": "zip", "path": rp, "abs": ap, "files": sorted(names), "nfile": len(names),
                  "bytes": sum(n[1] for n in names), "sha256": sha256_file(ap), "identity": norm_identity(os.path.basename(fn))}
            inst.append(it); zip_dirs_seen.add(rp)
        except Exception as e: warn("ZIP FAIL %s %r" % (rp, e))
print("   zip %d 个" % len(zip_dirs_seen), flush=True)

# ---------- 3. 历史 marker / 包名事件 ----------
print("[3] 走历史 marker 事件", flush=True)
events = defaultdict(lambda: {"adds": [], "dels": [], "mods": [], "first": None, "last": None})
cur = None
for line in git(["log", "--all", "-M", "--name-status", "--date=iso-strict", "--format=C%x1f%H%x1f%aI"]).splitlines():
    if line.startswith("C\x1f"):
        _, cur, date = line.split("\x1f"); continue
    if not line.strip() or not cur: continue
    f = line.rstrip("\n").split("\t")
    st = f[0][:1]; tgt = f[-1]
    if not tgt: continue
    keys = []
    if tgt.split("/")[-1] in MARKERS or LEDGER_RE.search(tgt): keys.append(os.path.dirname(tgt))
    parts = tgt.split("/")
    for i in range(len(parts) - 1):
        if PKG_RE.search(parts[i]): keys.append("/".join(parts[:i + 1])); break
    for k in keys:
        if not k or excluded(k.split("/")[0] + "/"): continue
        e = events[k]
        (e["adds"] if st == "A" else e["dels"] if st == "D" else e["mods"]).append((cur, date))
        if not e["first"]: e["first"] = (cur, date)
        e["last"] = (cur, date)

# ---------- 4. 历史仅存包目录复原（整树） ----------
print("[4] 复原历史仅存包", flush=True)
def tree_files(rev, packrel):
    return [x.strip() for x in git(["ls-tree", "-r", "--name-only", rev, "--", packrel]).splitlines() if x.strip()]
recovered = {}
cands = [(pr, ev) for pr, ev in sorted(events.items())
         if pr and not excluded(pr.split("/")[0] + "/") and not os.path.isdir(os.path.join(REPO, pr))]
print("   候选 %d 个" % len(cands), flush=True)
for packrel, ev in cands:
    alive = None; files = []
    tries = [d for d, _ in ev["dels"]] + [(ev["last"] or [None])[0]] + [a for a, _ in ev["adds"]]
    for cand in [t for t in tries if t]:
        for rev in (cand + "^", cand + "^1", cand + "^2", cand):
            fs = tree_files(rev, packrel)
            if len(fs) >= 2: alive = rev; files = fs; break
        if alive: break
    if not alive:
        last = git(["log", "--all", "-1", "--format=%H", "--", packrel]).strip()
        if last:
            for rev in (last + "^", last):
                fs = tree_files(rev, packrel)
                if len(fs) >= 2: alive = rev; files = fs; break
    if not alive: warn("NOPACKTREE %s" % packrel); continue
    seen = files
    if len(seen) > 400:
        key = [x for x in seen if x.split("/")[-1] in MARKERS or LEDGER_RE.search(x) or re.search(r"/[0-9]{2}_[A-Z]", x) or ("/tasks/" in x)]
        seen = (sorted(set(key)) + [x for x in seen if x not in key])[:400]
        warn("CAP %s -> 400（关键优先）" % packrel)
    outbase = None
    shm = {}
    for line in git(["ls-tree", "-r", "-l", alive, "--", packrel]).splitlines():
        m = re.match(r"^[0-7]+ blob ([0-9a-f]{40})\s+(\d+)\t(.+)$", line)
        if m: shm[m.group(3)] = (m.group(1), int(m.group(2)))
    n = 0
    for rel in seen:
        got = shm.get(rel)
        if not got: continue
        r = gitbin(["cat-file", "blob", got[0]])
        if r.returncode != 0: continue
        if outbase is None: outbase = os.path.join(HIST, alive.split("^")[0][:8], packrel)
        dest = os.path.join(HIST, alive.split("^")[0][:8], rel)
        os.makedirs(os.path.dirname(dest), exist_ok=True)
        try: open(dest, "wb").write(r.stdout); n += 1
        except Exception as ex: warn("WDIS %s %r" % (rel, ex))
    if outbase is None: warn("NODATA %s" % packrel); continue
    prov = {"pack_rel_path": packrel, "identity": norm_identity(packrel), "recovered_files": n,
            "recovered_from_rev": alive, "delete_commits": [c for c, d in ev["dels"]][:6],
            "first_commit": (ev["first"] or [None])[0], "last_commit": (ev["last"] or [None])[0]}
    open(os.path.join(outbase, "_PROVENANCE.json"), "w", encoding="utf-8").write(json.dumps(prov, ensure_ascii=False, indent=1))
    recovered[packrel] = prov
    print("   [恢复] %s -> %d 文件 (rev %s)" % (packrel, n, alive.split("^")[0][:8]), flush=True)
    it = scan_pack(outbase, os.path.relpath(outbase, P), "recovered_from_history")
    it["identity"] = norm_identity(packrel)
    it["git_events"] = {"first": ev["first"], "last": ev["last"], "adds": ev["adds"][:6], "dels": ev["dels"][:6], "mod_n": len(ev["mods"])}
    inst.append(it)

# ---------- 4b. 历史仅存 zip 复原 ----------
print("[4b] 复原历史 zip", flush=True)
hz = {}
for line in git(["log", "--all", "--format=", "--name-only", "--", "*.zip"]).splitlines():
    p = line.strip()
    if p and PKG_RE.search(p) and not os.path.exists(os.path.join(REPO, p)): hz[p] = None
for p in sorted(hz):
    rev = git(["rev-list", "--all", "-1", "--", p]).strip()
    if not rev: continue
    blob = None
    for r in (rev, rev + "^", rev + "^1"):
        o = gitbin(["cat-file", "-p", r + ":" + p])
        if o.returncode == 0 and o.stdout[:2] == b"PK": blob = o.stdout; break
    if blob is None: warn("NOZIPBLOB %s" % p); continue
    dest = os.path.join(HZIP, os.path.basename(p))
    open(dest, "wb").write(blob)
    prov = {"orig_path_in_repo": p, "recovered_from_rev": rev, "bytes": len(blob), "sha256": sha256_file(dest)}
    open(dest + ".provenance.json", "w", encoding="utf-8").write(json.dumps(prov, ensure_ascii=False, indent=1))
    try:
        with zipfile.ZipFile(dest) as z: names = [(i.filename, i.file_size) for i in z.infolist() if not i.is_dir()]
        inst.append({"form": "zip_recovered_from_history", "path": "history_zip/" + os.path.basename(p), "abs": dest,
                     "files": sorted(names), "nfile": len(names), "bytes": sum(x[1] for x in names),
                     "sha256": prov["sha256"], "identity": norm_identity(os.path.basename(p)),
                     "git_events": {"touch_last": rev}})
        print("   [恢复-zip] %s -> %d 文件" % (os.path.basename(p), len(names)), flush=True)
    except Exception as ex: warn("ZOPEN %s %r" % (p, ex))

# ---------- 5. 每实例机械摘要 ----------
print("[5] 逐实例机械摘要", flush=True)
def digest_ledger(absdir, rel):
    try: rows = list(csv.reader(open(os.path.join(absdir, rel), encoding="utf-8", errors="replace")))
    except Exception as e: return {"file": rel, "error": repr(e)}
    if not rows: return {"file": rel, "rows": 0}
    hdr = [h.strip() for h in rows[0]]
    body = [r for r in rows[1:] if any(x.strip() for x in r)]
    def col(*names):
        for i, h in enumerate(hdr):
            if any(n.lower() in h.lower() for n in names): return i
        return None
    si = col("status", "状态"); ii = col("task_id", "id")
    if ii is None: ii = 0
    dist = {}
    if si is not None:
        for r in body:
            if len(r) > si: dist[r[si].strip()] = dist.get(r[si].strip(), 0) + 1
    ids = [r[ii].strip() for r in body if len(r) > ii and r[ii].strip()]
    return {"file": rel, "header": hdr, "rows": len(body), "status_col": hdr[si] if si is not None else None,
            "status_dist": dist, "n_id": len(ids), "ids_sample": ids[:40], "all_ids": ids[:400]}

def md_headings(absdir, rel, cap=50):
    try: txt = open(os.path.join(absdir, rel), encoding="utf-8", errors="replace").read()
    except Exception: return []
    return [(len(a), b[:110]) for a, b in re.findall(r"^\s{0,3}(#{1,5})\s+(.+?)\s*$", txt, re.M)[:cap]]

def verify_sums(absdir):
    cands = [f for f in os.listdir(absdir) if f.upper().startswith("SHA256")]
    if not cands: return None
    ok = bad = miss = 0; bl = []
    for line in open(os.path.join(absdir, cands[0]), encoding="utf-8", errors="replace"):
        m = re.match(r"^([0-9a-f]{64})\s+\*?(.+?)\s*$", line.strip())
        if not m: continue
        h, pth = m.group(1), m.group(2); t = os.path.join(absdir, pth)
        if not os.path.exists(t): miss += 1; continue
        try:
            if sha256_file(t) == h: ok += 1
            else: bad += 1; bl.append(pth)
        except Exception: miss += 1
    return {"file": cands[0], "ok": ok, "mismatch": bad, "missing": miss, "mismatch_list": bl[:15]}

for it in inst:
    if it["form"] in ("zip", "zip_recovered_from_history"):
        try:
            with zipfile.ZipFile(it["abs"]) as z:
                names = [n for n, _ in it["files"]]
                it["zip"] = {"tops": sorted(set(n.split("/")[0] for n in names if n))[:6],
                             "markers": [n for n in names if n.split("/")[-1] in MARKERS][:10],
                             "spec_files": sorted(n for n in names if re.match(r"^[^/]*/[0-9]{2}_[A-Z]", n) or re.match(r"^[0-9]{2}_[A-Z]", n))[:45]}
                mk = it["zip"]["markers"]
                if mk:
                    it["zip"]["read_first_head"] = z.read(sorted(mk)[0]).decode("utf-8", "replace")[:1200]
                led = [n for n in names if LEDGER_RE.search(n)]
                if led:
                    rows = list(csv.reader(io.TextIOWrapper(z.open(led[0]), encoding="utf-8", errors="replace")))
                    it["zip"]["ledger"] = {"file": led[0], "header": [x.strip() for x in rows[0]] if rows else [],
                                           "rows": max(0, len(rows) - 1),
                                           "n_id": len([r for r in rows[1:] if r and r[0].strip()])}
        except Exception as e: warn("ZDUMP %s %r" % (it["path"], e))
        continue
    a = it["abs"]
    if not os.path.isdir(a): continue
    names = [n for n, _ in it["files"]]
    it["ledgers"] = [digest_ledger(a, n) for n in [x for x in names if LEDGER_RE.search(x)][:5]]
    rf = next((n for n in names if n.endswith("00_READ_FIRST.md")), None)
    it["read_first"] = rf
    it["read_first_headings"] = md_headings(a, rf) if rf else None
    it["spec_files"] = [n for n in names if re.match(r"^[0-9]{2}_[A-Z]", n) and n.endswith(".md")][:50]
    it["tasks_files"] = [n for n in names if n.startswith("tasks/") and n.endswith(".md")][:300]
    it["scripts"] = [n for n in names if n.startswith("scripts/")][:70]
    it["schemas"] = [n for n in names if n.startswith("schemas/")][:70]
    it["templates"] = [n for n in names if n.startswith("templates/")][:70]
    it["sums"] = verify_sums(a)
    for jf, key, cap in (("control-pack.json", "cpj", 900), ("MANIFEST.json", "manifest", 700)):
        if os.path.exists(os.path.join(a, jf)):
            try:
                j = json.load(open(os.path.join(a, jf), encoding="utf-8"))
                it[key] = {k: (v if not isinstance(v, (list, dict)) else ("[%d 项] " % len(v) + str(v)[:150]))
                           for k, v in list(j.items())[:30]} if isinstance(j, dict) else str(j)[:cap]
            except Exception as e: it[key] = {"error": repr(e)}
    if os.path.exists(os.path.join(a, "START_PROMPT.txt")):
        it["start_prompt"] = open(os.path.join(a, "START_PROMPT.txt"), encoding="utf-8", errors="replace").read()[:700]
    if os.path.exists(os.path.join(a, "PACKAGE_VERSION.txt")):
        it["package_version"] = open(os.path.join(a, "PACKAGE_VERSION.txt"), encoding="utf-8", errors="replace").read()[:200]

# ---------- 6. 归并与输出 ----------
by_ident = defaultdict(list)
for it in inst: by_ident[it["identity"]].append(it)
for ident, items in by_ident.items():
    for it in items:
        if "git_events" in it: continue
        ev = events.get(it["path"].rstrip("/"))
        if ev: it["git_events"] = {"first": ev["first"], "last": ev["last"], "adds": ev["adds"][:6], "dels": ev["dels"][:6], "mod_n": len(ev["mods"])}
        else:
            lp = git(["log", "--all", "-1", "--format=%H %aI", "--date=iso-strict", "--", (it["path"] or ".").split("/history_zip/")[-1]]).strip()
            it["git_events"] = {"touch_last": lp or None}

def esc(x): return str(x).replace("|", "/").replace("\n", " ⏎ ")
out = []
with open(os.path.join(P, "pack_inventory.csv"), "w", newline="", encoding="utf-8") as f:
    w = csv.writer(f)
    w.writerow(["身份", "存形态", "路径", "文件数", "字节", "zip或自带sha256", "首次提交", "首次日期", "最后提交", "删除提交",
                "容器(子包数)", "父包", "台账行数", "状态分布", "自带SHA256核对"])
    for ident in sorted(by_ident):
        for it in sorted(by_ident[ident], key=lambda x: (x["form"], x["path"])):
            ge = it.get("git_events") or {}
            lg = (it.get("ledgers") or [{}])[0]
            if not lg.get("rows") and it.get("zip", {}).get("ledger"): lg = it["zip"]["ledger"]
            fsh = str(ge.get("first") or ["", ""]); lsh = str(ge.get("last") or "")
            w.writerow([ident, it["form"], it["path"], it["nfile"], it["bytes"], it.get("sha256", "")[:16],
                        (fsh.split("'")[1][:8] if "'" in fsh else str(ge.get("touch_last") or "")[:8]),
                        (fsh.split("'")[3][:10] if fsh.count("'") > 2 else ""),
                        (lsh.split("'")[1][:8] if "'" in lsh else ""),
                        ";".join(c[:8] for c, d in (ge.get("dels") or [])),
                        len(it.get("nested_packs") or []) if it.get("nested_packs") is not None else "",
                        it.get("parent_pack") or "", lg.get("rows", ""),
                        json.dumps(lg.get("status_dist", {}), ensure_ascii=False)[:200],
                        json.dumps(it.get("sums"), ensure_ascii=False)[:160] if it.get("sums") else ""])
            out.append(it)
json.dump([{k: v for k, v in it.items() if k != "files"} for it in out],
          open(os.path.join(P, "pack_instances.json"), "w", encoding="utf-8"), ensure_ascii=False, indent=1)
with open(os.path.join(P, "pack_files.jsonl"), "w", encoding="utf-8") as f:
    for it in out:
        f.write(json.dumps({"identity": it["identity"], "path": it["path"], "form": it["form"], "files": it["files"]}, ensure_ascii=False) + "\n")
for ident, items in sorted(by_ident.items()):
    o = ["# 包取证摘要：" + ident, ""]
    o.append("身份由脚本归一（去复原前缀与 .zip 后缀）。共 %d 个实例。" % len(items))
    for it in sorted(items, key=lambda x: (x["form"], x["path"])):
        ge = it.get("git_events") or {}
        o.append("")
        o.append("## 【%s】%s" % (it["form"], it["path"]))
        o.append("- 文件 %d | %.1f KB%s" % (it["nfile"], it["bytes"] / 1024.0,
                 " | 容器（含子包 %d 个）" % len(it["nested_packs"]) if it.get("nested_packs") else ""))
        if it.get("parent_pack"): o.append("- 父包目录: " + it["parent_pack"])
        if it.get("sha256"): o.append("- sha256: " + it["sha256"])
        o.append("- git 事件: " + esc({"first": ge.get("first"), "last": ge.get("last"), "dels": ge.get("dels"),
                                       "adds_n": len(ge.get("adds") or []), "mod_n": ge.get("mod_n", len(ge.get("mods") or []) if isinstance(ge.get("mods"), list) else None),
                                       "touch_last": ge.get("touch_last")}))
        if it.get("package_version"): o.append("- PACKAGE_VERSION: " + esc(it["package_version"][:180]))
        if it.get("zip"):
            z = it["zip"]
            o.append("- zip 顶层: " + ", ".join(z.get("tops", [])[:6]))
            if z.get("markers"): o.append("- zip 内 marker: " + ", ".join(n.split("/")[-1] for n in z["markers"][:8]))
            if z.get("spec_files"): o.append("- zip 内规格文件: " + ", ".join(n.split("/")[-1] for n in z["spec_files"][:40]))
            if z.get("ledger"): o.append("- zip 内台账: %s 表头 %s 行 %s" % (z["ledger"].get("file"), z["ledger"].get("header"), z["ledger"].get("rows")))
            if z.get("read_first_head"): o.append("- 00_READ_FIRST 开头:\n\n" + z["read_first_head"][:1200] + "\n")
        for lg in (it.get("ledgers") or []):
            if not lg: continue
            o.append("- 台账 %s: 行 %s 状态列 %s 分布 %s 任务号样例 %s" % (lg.get("file"), lg.get("rows"), lg.get("status_col"),
                     json.dumps(lg.get("status_dist", {}), ensure_ascii=False), ", ".join(lg.get("ids_sample", [])[:16])))
        if it.get("spec_files"): o.append("- 规格文件: " + ", ".join(it["spec_files"]))
        if it.get("tasks_files"): o.append("- tasks/ 共 %d: %s" % (len(it["tasks_files"]), ", ".join(x.split("/")[-1] for x in it["tasks_files"][:35])))
        if it.get("scripts"): o.append("- scripts/: " + ", ".join(x.split("/")[-1] for x in it["scripts"]))
        if it.get("schemas"): o.append("- schemas/: " + ", ".join(x.split("/")[-1] for x in it["schemas"]))
        if it.get("templates"): o.append("- templates/: " + ", ".join(x.split("/")[-1] for x in it["templates"]))
        if it.get("sums"): o.append("- 包内 SHA256SUMS 核对: " + json.dumps(it["sums"], ensure_ascii=False))
        if it.get("cpj"): o.append("- control-pack.json: " + esc(json.dumps(it["cpj"], ensure_ascii=False)[:900]))
        if it.get("manifest"): o.append("- MANIFEST.json: " + esc(json.dumps(it["manifest"], ensure_ascii=False)[:700]))
        if it.get("start_prompt"): o.append("- START_PROMPT 开头: " + esc(it["start_prompt"][:700]))
        if it.get("read_first_headings"): o.append("- 00_READ_FIRST 标题: " + " ; ".join("#" * a + " " + b for a, b in it["read_first_headings"]))
    open(os.path.join(DIG, re.sub(r"[^\w.\-]+", "__", ident)[:90] + ".md"), "w", encoding="utf-8").write("\n".join(o) + "\n")
json.dump(recovered, open(os.path.join(P, "recovered.json"), "w", encoding="utf-8"), ensure_ascii=False, indent=1)
print("DONE identities=%d instances=%d recovered=%d" % (len(by_ident), len(inst), len(recovered)), flush=True)
