#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
层1-B 控制包取证脚本（机械清点，不做解释）。
覆盖：现存解包文档 / zip 原件 / 归档副本 / 影子树副本登记 / 仅存 Git 历史的包（复原导出）。
输出：
  _evidence/packs/pack_inventory.csv      包身份 x 存形态 清单
  _evidence/packs/pack_instances.jsonl    每身份的详细机械摘要
  _evidence/packs/pack_files.jsonl        每实例的文件清单
  _evidence/packs/digest/<id>.md          每身份的取证摘要（供子子代理阅读）
  _evidence/packs/history/<commit8>/...   仅存历史的包复原件（附 _PROVENANCE.json）
用法: python3 02_build_pack_inventory.py
"""
import subprocess, os, re, sys, json, csv, hashlib, zipfile, io
from collections import defaultdict

REPO = "/workspace/Astro CS Database"
P = os.path.join(REPO, "设计大纲/_evidence/packs")
HIST = os.path.join(P, "history"); DIG = os.path.join(P, "digest")
EXCLUDE = ("run/", "build/", "out/", "worktrees/", ".pytest_cache/", "third_party/", "GaiaDR3/",
           "GaiaDR3SP/", "BASS DR3/", "testdata/", "AstroCS.wiki/", "logs/", "Testing/", "dist/", ".git")
MARKERS = ("00_READ_FIRST.md", "control-pack.json", "MANIFEST.json", "START_PROMPT.txt",
           "00_AGENT_START_PROMPT.txt", "AGENTS_REPLACEMENT.md", "AUTONOMOUS_ENTRY.md", "包规范.md")
LEDGER_RE = re.compile(r"(^|/)([0-9]{2}_)?TASK_LEDGER[^/]*\.csv$")
PKG_RE = re.compile(r"(CONTROL_V\d|_CONTROL_|CONTROL_PACKAGE|Development_Pack|Agent_Package|_agent_package|_new_pack|HISS|REAUDIT|REVIEWPACK|AUDIT_SUPPLEMENT|AUDIT_PACK|RESCUE|Delivery_|engineering_v1\.|engineering_archive|engineering_authoritative|cprun|工程控制)")
os.makedirs(HIST, exist_ok=True); os.makedirs(DIG, exist_ok=True)
warnf = open(os.path.join(P, "_warnings.txt"), "a", encoding="utf-8")
def warn(m): warnf.write(m + "\n"); warnf.flush()

def git(args, timeout=1800):
    return subprocess.run(["git", "-c", "core.quotePath=false"] + args, cwd=REPO,
                          capture_output=True, text=True, errors="replace", timeout=timeout).stdout

def sha256_file(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for b in iter(lambda: f.read(1 << 20), b""): h.update(b)
    return h.hexdigest()

def excluded(rel):
    return rel == ".git" or any(rel == x.rstrip("/") or rel.startswith(x) for x in EXCLUDE)

def identity_of(packrel):
    parts = [x for x in packrel.split("/") if x]
    if not parts: return "REPO_ROOT", packrel
    base = parts[-1]
    if base.startswith("AstroCS_") or "CONTROL" in base.upper() or base.upper().startswith("ACR") or "PACK" in base.upper():
        return base, packrel
    return ("/".join(parts[-2:]) if len(parts) > 1 else base), packrel

# ---- 1. 现存 worktree 包目录 ----
pack_dirs = set()
for root, dirs, files in os.walk(REPO):
    rel = os.path.relpath(root, REPO)
    if excluded(rel):
        dirs[:] = []; continue
    if os.path.basename(root) in MARKERS or any(f in MARKERS for f in files):
        pack_dirs.add(rel)
        dirs[:] = []
    dirs[:] = [d for d in dirs if os.path.relpath(os.path.join(root, d), REPO) not in pack_dirs]

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
    return out

inst = []
for rel in sorted(pack_dirs):
    it = scan_pack(os.path.join(REPO, rel), rel, "shadow_copy" if rel.startswith("run/") else "worktree")
    inst.append(it)

# ---- 2. zip 原件（现存磁盘）----
for root, dirs, files in os.walk(REPO):
    rel = os.path.relpath(root, REPO)
    if excluded(rel) or re.search(r"(capsules|ci_artifacts|logs/round)", rel):
        dirs[:] = []; continue
    for fn in files:
        if not fn.lower().endswith(".zip"): continue
        if not re.search(r"(CONTROL|PACK|Delivery|HISS|AUDIT|REVIEW|RESCUE|REFOUND)", fn, re.I): continue
        ap = os.path.join(root, fn); rp = os.path.relpath(ap, REPO)
        try:
            with zipfile.ZipFile(ap) as z:
                names = [(i.filename, i.file_size) for i in z.infolist() if not i.is_dir()]
            inst.append({"form": "zip", "path": rp, "abs": ap, "files": sorted(names),
                         "nfile": len(names), "bytes": sum(n[1] for n in names), "sha256": sha256_file(ap)})
        except Exception as e:
            warn("ZIP FAIL %s %r" % (rp, e))

# ---- 3. 历史 marker 事件 ----
events = defaultdict(lambda: {"adds": [], "dels": [], "mods": [], "first": None, "last": None})
cur = None
for line in git(["log", "--all", "-M", "--name-status", "--date=iso-strict", "--format=C%x1f%H%x1f%aI"]).splitlines():
    if line.startswith("C\x1f"):
        _, cur, date = line.split("\x1f"); continue
    if not line.strip() or not cur: continue
    f = line.rstrip("\n").split("\t")
    st = f[0][:1]; tgt = f[-1]
    base = tgt.split("/")[-1]
    keys = []
    if base in MARKERS or LEDGER_RE.search(tgt): keys.append(os.path.dirname(tgt))
    parts = tgt.split("/")
    for i in range(len(parts) - 1):
        if PKG_RE.search(parts[i]):
            keys.append("/".join(parts[:i + 1])); break
    for k in keys:
        if not k or excluded(k.split("/")[0] + "/"): continue
        e = events[k]
        (e["adds"] if st == "A" else e["dels"] if st == "D" else e["mods"]).append((cur, date))
        if not e["first"]: e["first"] = (cur, date)
        e["last"] = (cur, date)

# ---- 4. 仅存历史的包：复原（树对象法，逐包一次 ls-tree）----
recovered = {}
print("[recover] 候选包目录（当前不存在）计数中", flush=True)
cands = []
for packrel, ev in sorted(events.items()):
    if not packrel or excluded(packrel.split("/")[0] + "/"): continue
    if os.path.isdir(os.path.join(REPO, packrel)): continue
    if packrel.startswith("设计大纲/") or packrel.startswith("_evidence"): continue
    cands.append((packrel, ev))
print("[recover] 候选 %d 个" % len(cands), flush=True)

def tree_files(rev, packrel):
    out = git(["ls-tree", "-r", "--name-only", rev, "--", packrel])
    return [x for x in out.splitlines() if x.strip()]

for packrel, ev in cands:
    alive_rev = None; files = []
    for cand in [d for d, _ in ev["dels"]] + [ev["last"][0]] + [a for a, _ in ev["adds"]]:
        if not cand: continue
        for rev in (cand + "^", cand + "^1", cand + "^2"):
            fs = tree_files(rev, packrel)
            if len(fs) >= 2:
                alive_rev = rev; files = fs; break
        if alive_rev: break
    if not alive_rev:
        # 退回：用该路径最近一次出现的提交
        last = git(["log", "--all", "-1", "--format=%H", "--", packrel]).strip()
        if last:
            for rev in (last, last + "^"):
                fs = tree_files(rev, packrel)
                if len(fs) >= 2: alive_rev = rev; files = fs; break
    if not alive_rev:
        warn("NOPACKTREE %s" % packrel); continue
    allf = [x.strip() for x in tree_files(alive_rev, packrel) if x.strip()]
    if len(allf) >= len(files): files = allf
    seen = []
    for x in files:
        p = x[len(packrel) + 1:] if x.startswith(packrel + "/") else x.split("/")[-1]
        key = packrel + "/" + p
        if key not in seen: seen.append(key)
    if len(seen) > 400:
        keyfirst = [x for x in seen if x.split("/")[-1] in MARKERS or LEDGER_RE.search(x) or re.search(r"/[0-9]{2}_[A-Z]", x) or x.startswith(packrel + "/tasks/")]
        rest = [x for x in seen if x not in keyfirst]
        warn("CAP %s %d 文件 -> 关键 400" % (packrel, len(seen))); seen = (keyfirst + rest)[:400]
    outbase = None
    bloblist = git(["ls-tree", "-r", alive_rev, "--", packrel]).splitlines()
    shm = {}
    for line in bloblist:
        m = re.match(r"^[0-7]+ blob ([0-9a-f]{40})\t(.+)$", line)
        if m: shm[m.group(2)] = m.group(1)
    for rel in seen:
        sha = shm.get(rel)
        if not sha: continue
        content = subprocess.run(["git", "cat-file", "blob", sha], cwd=REPO,
                                 capture_output=True, text=True, errors="replace").stdout
        if content is None: continue
        if outbase is None: outbase = os.path.join(HIST, alive_rev.split("^")[0][:8], packrel)
        dest = os.path.join(HIST, alive_rev.split("^")[0][:8], rel)
        os.makedirs(os.path.dirname(dest), exist_ok=True)
        open(dest, "w", encoding="utf-8").write(content)
    if outbase is None: continue
    prov = {"pack_rel_path": packrel, "identity": identity_of(packrel)[0],
            "recovered_files": len(seen), "recovered_from_rev": alive_rev,
            "delete_commits": [c for c, d in ev["dels"]][:6],
            "first_commit": (ev["first"] or [None])[0], "last_commit": (ev["last"] or [None])[0]}
    open(os.path.join(outbase, "_PROVENANCE.json"), "w", encoding="utf-8").write(json.dumps(prov, ensure_ascii=False, indent=1))
    recovered[packrel] = prov
    print("[recover] %s -> %s (%d 文件)" % (packrel, os.path.relpath(outbase, P), len(seen)), flush=True)
    it = scan_pack(outbase, os.path.relpath(outbase, P), "recovered_from_history")
    it["git_events"] = {"first": ev["first"], "last": ev["last"], "adds": ev["adds"][:6], "dels": ev["dels"][:6], "mod_n": len(ev["mods"])}
    inst.append(it)

# ---- 4b. 历史中的 zip 包原件（当前磁盘已无）复原到 history_zip/ ----
hzip = {}
for line in git(["log", "--all", "--format=", "--name-only", "--", "*.zip"]).splitlines():
    p = line.strip()
    if p and PKG_RE.search(p) and not os.path.exists(os.path.join(REPO, p)): hzip[p] = None
zpats = sorted(hzip)
print("[recover] 历史 zip 候选 %d 个" % len(zpats), flush=True)
HZ = os.path.join(P, "history_zip"); os.makedirs(HZ, exist_ok=True)
for p in zpats:
    rev = git(["rev-list", "--all", "-1", "--", p]).strip()
    if not rev: warn("NOZIPREV %s" % p); continue
    blob = None
    for r in (rev, rev + "^", rev + "^1"):
        out = subprocess.run(["git", "cat-file", "-p", r + ":" + p], cwd=REPO, capture_output=True)
        if out.returncode == 0 and out.stdout[:2] == b"PK": blob = out.stdout; break
    if blob is None: warn("NOZIPBLOB %s (rev %s)" % (p, rev[:8])); continue
    dest = os.path.join(HZ, os.path.basename(p))
    open(dest, "wb").write(blob)
    prov = {"orig_path_in_repo": p, "recovered_from_rev": rev, "bytes": len(blob),
            "sha256": sha256_file(dest), "note": "当前磁盘已无此 zip，从 git blob 复原"}
    open(dest + ".provenance.json", "w", encoding="utf-8").write(json.dumps(prov, ensure_ascii=False, indent=1))
    try:
        with zipfile.ZipFile(dest) as z: names = [(i.filename, i.file_size) for i in z.infolist() if not i.is_dir()]
        inst.append({"form": "zip_recovered_from_history", "path": "history_zip/" + os.path.basename(p), "abs": dest,
                     "files": sorted(names), "nfile": len(names), "bytes": sum(n[1] for n in names),
                     "sha256": prov["sha256"], "git_events": {"touch_last": rev}})
        print("[recover] zip %s -> %d 文件" % (os.path.basename(p), len(names)), flush=True)
    except Exception as ex: warn("ZOPEN %s %r" % (p, ex))

# ---- 5. 每实例机械摘要 ----
def digest_ledger(absdir, rel):
    try:
        rows = list(csv.reader(open(os.path.join(absdir, rel), encoding="utf-8", errors="replace")))
    except Exception as e:
        return {"error": repr(e)}
    if not rows: return {"rows": 0}
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
            "status_dist": dist, "n_id": len(ids), "ids_sample": ids[:40]}

def md_headings(absdir, rel, cap=45):
    try: txt = open(os.path.join(absdir, rel), encoding="utf-8", errors="replace").read()
    except Exception: return []
    return [(len(a), b[:110]) for a, b in re.findall(r"^\s{0,3}(#{1,5})\s+(.+?)\s*$", txt, re.M)[:cap]]

def verify_sums(absdir):
    cands = [f for f in os.listdir(absdir) if f.upper().startswith("SHA256")]
    if not cands: return None
    ok = bad = miss = 0; bl = []
    try:
        for line in open(os.path.join(absdir, cands[0]), encoding="utf-8", errors="replace"):
            m = re.match(r"^([0-9a-f]{64})\s+\*?(.+?)\s*$", line.strip())
            if not m: continue
            h, pth = m.group(1), m.group(2)
            t = os.path.join(absdir, pth)
            if not os.path.exists(t): miss += 1; continue
            try:
                if sha256_file(t) == h: ok += 1
                else: bad += 1; bl.append(pth)
            except Exception: miss += 1
    except Exception as e:
        return {"error": repr(e)}
    return {"ok": ok, "mismatch": bad, "missing": miss, "mismatch_list": bl[:15]}

for it in inst:
    if it["form"] == "zip":
        try:
            with zipfile.ZipFile(it["abs"]) as z:
                names = [n for n, _ in it["files"]]
                tops = sorted(set(n.split("/")[0] for n in names if n))
                mk = [n for n in names if n.split("/")[-1] in MARKERS]
                led = [n for n in names if LEDGER_RE.search(n)]
                head = z.read(sorted(mk)[0]).decode("utf-8", "replace")[:1400] if mk else None
                ledsum = None
                if led:
                    rows = list(csv.reader(io.TextIOWrapper(z.open(led[0]), encoding="utf-8", errors="replace")))
                    ledsum = {"file": led[0], "header": [x.strip() for x in rows[0]] if rows else [],
                              "rows": max(0, len(rows) - 1), "n_id": len([r for r in rows[1:] if r and r[0].strip()])}
                it["zip"] = {"tops": tops[:6], "markers": mk[:8], "ledger": ledsum, "read_first_head": head,
                             "spec_files": sorted(n for n in names if re.match(r"^[^/]*/[0-9]{2}_[A-Z]", n))[:40]}
        except Exception as e: warn("ZDUMP %s %r" % (it["path"], e))
        continue
    a = it["abs"]
    if not os.path.isdir(a): continue
    names = [n for n, _ in it["files"]]
    it["ledgers"] = [digest_ledger(a, n) for n in [x for x in names if LEDGER_RE.search(x)][:4]]
    rf = next((n for n in names if n.endswith("00_READ_FIRST.md")), None)
    it["read_first"] = rf
    it["read_first_headings"] = md_headings(a, rf) if rf else None
    it["spec_files"] = [n for n in names if re.match(r"^[0-9]{2}_[A-Z]", n) and n.endswith(".md")][:45]
    it["tasks_files"] = [n for n in names if n.startswith("tasks/") and n.endswith(".md")][:250]
    it["scripts"] = [n for n in names if n.startswith("scripts/")][:60]
    it["schemas"] = [n for n in names if n.startswith("schemas/")][:60]
    it["templates"] = [n for n in names if n.startswith("templates/")][:60]
    it["sums"] = verify_sums(a)
    if os.path.exists(os.path.join(a, "control-pack.json")):
        try:
            j = json.load(open(os.path.join(a, "control-pack.json"), encoding="utf-8"))
            it["cpj"] = {k: (v if not isinstance(v, (list, dict)) else ("[" + str(len(v)) + " items] " + str(v)[:160]))
                         for k, v in list(j.items())[:28]}
        except Exception as e: it["cpj"] = {"error": repr(e)}
    if os.path.exists(os.path.join(a, "MANIFEST.json")):
        try: it["manifest"] = str(json.load(open(os.path.join(a, "MANIFEST.json"), encoding="utf-8")))[:800]
        except Exception as e: it["manifest"] = "ERR " + repr(e)
    if os.path.exists(os.path.join(a, "START_PROMPT.txt")):
        it["start_prompt"] = open(os.path.join(a, "START_PROMPT.txt"), encoding="utf-8", errors="replace").read()[:800]

# ---- 6. 归并身份、写产物 ----
by_ident = defaultdict(list)
for it in inst:
    ident, _ = identity_of(it["path"] if it["form"] != "zip" else os.path.dirname(it["path"]))
    it["identity"] = ident
    by_ident[ident].append(it)
for ident, items in by_ident.items():
    for it in items:
        ev = events.get(it["path"].rstrip("/"))
        if ev and "git_events" not in it:
            it["git_events"] = {"first": ev["first"], "last": ev["last"], "adds": ev["adds"][:6], "dels": ev["dels"][:6], "mod_n": len(ev["mods"])}
        elif "git_events" not in it:
            lp = git(["log", "--all", "-1", "--format=%H %aI", "--date=iso-strict", "--", it["path"] or "."]).strip()
            it["git_events"] = {"touch_last": lp or None}

def esc(x): return str(x).replace("|", "/").replace("\n", " ⏎ ")
small = []
for it in inst:
    d = {k: v for k, v in it.items() if k not in ("files", "abs")}
    small.append(d)
json.dump(small, open(os.path.join(P, "pack_instances.jsonl"), "w", encoding="utf-8"), ensure_ascii=False, indent=1)
with open(os.path.join(P, "pack_files.jsonl"), "w", encoding="utf-8") as f:
    for it in inst:
        f.write(json.dumps({"identity": it["identity"], "path": it["path"], "form": it["form"], "files": it["files"]}, ensure_ascii=False) + "\n")
with open(os.path.join(P, "pack_inventory.csv"), "w", newline="", encoding="utf-8") as f:
    w = csv.writer(f)
    w.writerow(["身份", "存形态", "路径", "文件数", "字节", "zip_sha256", "首次提交", "最后提交", "删除提交", "台账行数", "状态分布", "自带SHA256核对"])
    for ident in sorted(by_ident):
        for it in sorted(by_ident[ident], key=lambda x: (x["form"], x["path"])):
            ge = it.get("git_events") or {}
            lg = (it.get("ledgers") or [{}])[0]
            if not lg and it.get("zip", {}).get("ledger"): lg = it["zip"]["ledger"]
            w.writerow([ident, it["form"], it["path"], it["nfile"], it["bytes"], it.get("sha256", ""),
                        (ge.get("first") or ["", ""])[0][:8] + " " + str((ge.get("first") or ["", ""])[1])[:10],
                        (ge.get("last") or ["", ""])[0][:8] + " " + str((ge.get("last") or ["", ""])[1])[:10],
                        ";".join(c[:8] for c, d in (ge.get("dels") or [])),
                        lg.get("rows", ""), json.dumps(lg.get("status_dist", {}), ensure_ascii=False),
                        json.dumps(it.get("sums"), ensure_ascii=False) if it.get("sums") else ""])
for ident, items in sorted(by_ident.items()):
    o = ["# 包取证摘要：" + ident, ""]
    o.append("## 存形态（%d 个实例）" % len(items))
    for it in sorted(items, key=lambda x: (x["form"], x["path"])):
        ge = it.get("git_events") or {}
        o.append("- 【%s】%s | 文件 %d | %.1f KB" % (it["form"], it["path"], it["nfile"], it["bytes"] / 1024.0))
        if it.get("sha256"): o.append("    - zip sha256: " + it["sha256"])
        if ge:
            o.append("    - git 事件: 首次 %s / 最后 %s / A %d D %d M %s" % (
                esc(ge.get("first") or ge.get("touch_last")), esc(ge.get("last")),
                len(ge.get("adds") or []), len(ge.get("dels") or []), ge.get("mod_n", "-")))
        if it.get("zip"):
            z = it["zip"]
            o.append("    - zip 顶层: %s ; marker: %s" % (", ".join(z.get("tops", [])[:5]), ", ".join(n.split("/")[-1] for n in z.get("markers", [])[:6])))
            if z.get("spec_files"): o.append("    - zip 内规格文件: " + ", ".join(n.split("/")[-1] for n in z["spec_files"][:35]))
            if z.get("ledger"): o.append("    - zip 内台账: %s 表头 %s 行 %s" % (z["ledger"].get("file"), z["ledger"].get("header"), z["ledger"].get("rows")))
            if z.get("read_first_head"): o.append("    - 00_READ_FIRST 开头 1400 字: " + esc(z["read_first_head"][:1400]))
        for lg in (it.get("ledgers") or []):
            if not lg: continue
            o.append("    - 台账 %s: 行 %s 状态列 %s 分布 %s 任务号样例 %s" % (
                lg.get("file"), lg.get("rows"), lg.get("status_col"), json.dumps(lg.get("status_dist", {}), ensure_ascii=False),
                ", ".join(lg.get("ids_sample", [])[:14])))
        if it.get("spec_files"): o.append("    - 规格文件: " + ", ".join(it["spec_files"]))
        if it.get("tasks_files"): o.append("    - tasks/ 文件 %d: %s" % (len(it["tasks_files"]), ", ".join(x.split("/")[-1] for x in it["tasks_files"][:30])))
        if it.get("scripts"): o.append("    - scripts/: " + ", ".join(x.split("/")[-1] for x in it["scripts"]))
        if it.get("schemas"): o.append("    - schemas/: " + ", ".join(x.split("/")[-1] for x in it["schemas"]))
        if it.get("templates"): o.append("    - templates/: " + ", ".join(x.split("/")[-1] for x in it["templates"]))
        if it.get("sums"): o.append("    - 包内 SHA256SUMS 核对: " + json.dumps(it["sums"], ensure_ascii=False))
        if it.get("cpj"): o.append("    - control-pack.json: " + esc(json.dumps(it["cpj"], ensure_ascii=False)[:900]))
        if it.get("manifest"): o.append("    - MANIFEST.json: " + esc(it["manifest"][:600]))
        if it.get("start_prompt"): o.append("    - START_PROMPT 开头: " + esc(it["start_prompt"][:600]))
        if it.get("read_first_headings"): o.append("    - 00_READ_FIRST 标题结构: " + " ; ".join("#" * a + " " + b for a, b in it["read_first_headings"]))
    open(os.path.join(DIG, re.sub(r"[^\w.\-]+", "__", ident)[:90] + ".md"), "w", encoding="utf-8").write("\n".join(o) + "\n")
json.dump(recovered, open(os.path.join(P, "recovered.json"), "w", encoding="utf-8"), ensure_ascii=False, indent=1)
print("identities=%d instances=%d recovered_dirs=%d" % (len(by_ident), len(inst), len(recovered)))