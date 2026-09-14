#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
层1-A 提交级证据固定脚本（AstroCS 设计大纲取证）。
只做机械抽取，不做解释。输出：
  _evidence/commits/index.jsonl            每提交一行结构化元数据+文件清单
  _evidence/commits/cards/NNNN-sha8.md     单提交证据卡（供子子代理逐条分析）
  _evidence/commits/cross_dir_month.csv    目录 x 月份 透视
  _evidence/commits/cross_id_month.csv     编号族 x 月份 透视
  _evidence/commits/commits_table.csv      全量提交一览表
  _evidence/commits/_warnings.txt          失败/截断登记
用法: python3 01_build_commit_evidence.py [--limit N] [--offset K]
"""
import subprocess, json, re, os, sys, csv
from collections import defaultdict, OrderedDict

REPO = "/workspace/Astro CS Database"
EV = os.path.join(REPO, "设计大纲/_evidence/commits")
CARD_DIR = os.path.join(EV, "cards")

NOISE_PREFIX = ("run/", "build/", "out/", "worktrees/", ".pytest_cache/", "third_party/",
                "GaiaDR3/", "GaiaDR3SP/", "BASS DR3/", "testdata/", "AstroCS.wiki/",
                "logs/", "Testing/", "dist/", "astrocs_p1sess_", "CS/", "Database/", "graph/")
BIN_EXT = (".zip",".png",".jpg",".jpeg",".gif",".pdf",".fits",".fit",".parquet",".db",".sqlite",
           ".o",".a",".so",".dll",".exe",".obj",".lib",".gz",".bz2",".xz",".7z",".tar",".wasm",".class")
ROOT_NOISE_RE = re.compile(r"^(astrocs_run_[0-9a-f]+\.json|p\d+-files\.patch|alloc_.*|resource_(samples|summary).*|worker_balance\.csv|_commit_msg.*|_msg_.*\.txt)")

MAX_DIFF_BYTES = 260000
STRUCT_ONLY_LINES = 60000
CARD_CAP = {"S": 5200, "M": 13000, "L": 9000}

os.makedirs(CARD_DIR, exist_ok=True)
warnings = open(os.path.join(EV, "_warnings.txt"), "a", encoding="utf-8")
def warn(msg):
    warnings.write(msg + "\n"); warnings.flush()

def git(args):
    cmd = ["git", "-c", "core.quotePath=false", "-c", "color.ui=false"] + args
    p = subprocess.run(cmd, cwd=REPO, capture_output=True, text=True, errors="replace", timeout=900)
    return p.stdout

order = [s for s in git(["rev-list", "--topo-order", "--reverse", "--date-order", "--all"]).split() if s]
in_head = set(x for x in git(["rev-list", "--topo-order", "--reverse", "HEAD"]).split() if x)
parents = {}
for line in git(["log", "--all", "--topo-order", "--reverse", "--date-order", "--format=%H%x1f%P"]).splitlines():
    if "\x1f" in line:
        h, p = line.split("\x1f", 1)
        parents[h] = p.split()
roots = [r for r in order if not parents.get(r)]
lin = {}
for h in order:
    ps = parents.get(h, [])
    if not ps:
        lin[h] = {h}
    else:
        s = set()
        for p in ps: s |= lin.get(p, {p})
        lin[h] = s or {h}
MAIN_INIT = "c384de177dc549ab17feca83978c36c36a0aca6a"
ROOT_SUBJECT = {r: git(["log", "-1", "--format=%ad %s", "--date=short", r]).strip() for r in roots}

RE_HUNKCTX = re.compile(r"^@@[^\n]*@@\s*(.+?)\s*$")
RE_DECL = re.compile(r"^\s*(?:template\s*<[^>]*>\s*)?(?:(?:static|inline|constexpr|virtual|explicit|extern|noexcept|auto|void|bool|int|unsigned|long|float|double|char|size_t|uint\w*|int\w*|std::[\w:]+|[A-Z][\w:]*(?:<[^>]*>)?)(?:\s|::|[*&])+)+([A-Za-z_~][\w]*)\s*\(([^;]{0,160})")
RE_TYPE = re.compile(r"^\s*(class|struct|enum|union|namespace)\s+([A-Za-z_]\w*)")
RE_TEST = re.compile(r"\bTEST(?:_F|_P)?\s*\(\s*([A-Za-z_]\w*)\s*,\s*([^)]{1,80})\)")
RE_INC = re.compile(r"^\s*#include\s*[<\"]([^>\"]+)[>\"]")
RE_CMAKE = re.compile(r"\b(add_library|add_executable|target_link_libraries|target_sources|target_compile_definitions|target_include_directories|option|add_test|install|set)\s*\(([^)]{0,120})\)")
RE_TOL = re.compile(r"^\s*(?:constexpr|const|static)?\s*(?:double|float|auto|int|size_t)?\s*\b(\w*(?:tol|TOL|eps|EPS|epsilon|threshold|Threshold|max_|MAX_|min_|MIN_|budget|workers|Workers|chunk|block)[\w]*)\s*=\s*([^;,)]{1,40})")
RE_HEAD = re.compile(r"^\s{0,3}(#{1,5})\s+(.+?)\s*$")
RE_JSONKEY = re.compile(r"^\s*\"([A-Za-z0-9_./\-]{2,60})\"\s*:\s*[\{\[]?")
RE_TASKID = re.compile(r"\b(?:P\d+(?:\.\d+)?-\d{2,4}|V\d+(?:\.\d+)?-[A-Z]{0,3}-?\d{2,4}|[A-Z]{2,6}-\d{2,4}|GOV-\d+|R-\d+|A-\d+|M\d+|RQS|AUR\d+)\b")

def is_noise(p):
    if not p: return True
    if any(p.startswith(x) for x in NOISE_PREFIX): return True
    if "/" not in p and ROOT_NOISE_RE.match(p): return True
    return False

def is_bin(p):
    return (p or "").lower().endswith(BIN_EXT)

def collect(sha, is_merge):
    d = {}
    if is_merge and parents.get(sha):
        base = parents[sha][0]
        ns = git(["diff", "--name-status", "-M", base, sha])
        nu = git(["diff", "--numstat", "-M", base, sha])
    else:
        ns = git(["show", "--format=", "--name-status", "-M", sha])
        nu = git(["show", "--format=", "--numstat", sha])
    num = {}
    for line in nu.splitlines():
        f = line.split("\t")
        if len(f) < 3: continue
        try: a, dl = int(f[0]), int(f[1])
        except ValueError: a, dl = 0, 0
        num[f[2]] = (a, dl)
    files = []
    for line in ns.splitlines():
        if not line.strip(): continue
        f = line.split("\t")
        st = f[0][0]
        if st in ("R", "C") and len(f) >= 3: old, new = f[1], f[2]
        else:
            old = f[1] if len(f) > 1 else ""
            new = old
        p = new or old
        a, dl = num.get(p, (0, 0))
        if p not in num:
            for k, v in num.items():
                if k.endswith(p) or p.endswith(k): a, dl = v; break
        files.append((st, old, new, a, dl))
    d["files"] = files
    d["add"] = sum(x[3] for x in files); d["del"] = sum(x[4] for x in files)
    d["nfile"] = len(files)
    d["bin"] = sum(1 for x in files if is_bin(x[2] or x[1]))
    d["noise"] = sum(1 for x in files if is_noise(x[2] or x[1]))
    L = d["add"] + d["del"]
    d["cls"] = "S" if (d["nfile"] <= 6 and L <= 250) else ("M" if (d["nfile"] <= 25 and L <= 2000) else "L")
    rel = [x for x in files if not is_noise(x[2] or x[1])]
    d["dirhist"] = OrderedDict()
    for st, old, new, a, dl in rel:
        p = new or old
        top = "/".join(p.split("/")[:2]) if "/" in p else p
        e = d["dirhist"].setdefault(top, [0, 0, 0])
        e[0] += 1; e[1] += a; e[2] += dl
    textpaths = [x[2] or x[1] for x in rel if not is_bin(x[2] or x[1])]
    sym = {"hunk_ctx": [], "add_decl": [], "del_decl": [], "add_type": [], "del_type": [],
           "add_test": [], "del_test": [], "add_inc": [], "del_inc": [], "cmake": [],
           "tol_add": [], "tol_del": [], "md_add": [], "md_del": [], "csv_add": [], "csv_del": [],
           "json_add": [], "json_del": [], "cli_str": [], "content": [], "perfile": OrderedDict()}
    truncated = False
    struct_only = L > STRUCT_ONLY_LINES
    if textpaths and not struct_only:
        buf = []; total = 0
        for i in range(0, len(textpaths), 40):
            grp = textpaths[i:i+40]
            t = git(["show", "--format=", "--no-color", "--unified=0", sha, "--"] + grp)
            total += len(t.encode("utf-8")); buf.append(t)
            if total > MAX_DIFF_BYTES:
                truncated = True; break
        cur = ""
        for line in "\n".join(buf).splitlines():
            if line.startswith("+++ b/"): cur = line[6:].strip(); continue
            if line.startswith("+++") or line.startswith("---"): continue
            if line.startswith("@@"):
                m = RE_HUNKCTX.search(line)
                if m and cur and not cur.endswith("/dev/null"):
                    ctx = m.group(1)[:120]
                    if ctx and ctx not in sym["hunk_ctx"] and len(sym["hunk_ctx"]) < 45: sym["hunk_ctx"].append(ctx)
                continue
            if line[:1] not in ("+", "-"): continue
            b = line[1:].rstrip()
            if not b.strip(): continue
            if cur and not cur.endswith("/dev/null"):
                pf = sym["perfile"].setdefault(cur, [0, 0])
                pf[0 if line[0] == "+" else 1] += 1
                key = cur + "\x02" + line[0] + re.sub(r"^\s+", "", b)[:170]
                if key not in sym.setdefault("_seen", set()) and len(sym["content"]) < 110:
                    sym.setdefault("_seen", set()).add(key)
                    sym["content"].append((cur, line[0], re.sub(r"\s+", " ", b)[:170]))
            tgt = "add" if line[0] == "+" else "del"
            low = cur.lower()
            kind = ("csv" if low.endswith(".csv") else
                    "md" if low.endswith((".md", ".markdown", ".txt")) else
                    "json" if low.endswith(".json") else
                    "cmake" if (low.endswith("cmakelists.txt") or low.endswith(".cmake")) else
                    "code" if low.endswith((".h", ".hpp", ".c", ".cc", ".cpp", ".tcc", ".py", ".ps1", ".sh", ".yml", ".yaml")) else
                    "other")
            if kind == "code":
                m = RE_TYPE.match(b)
                if m:
                    k = "add_type" if tgt == "add" else "del_type"
                    v = (m.group(1) + " " + m.group(2))[:90]
                    if v not in sym[k] and len(sym[k]) < 40: sym[k].append(v)
                else:
                    m = RE_DECL.search(b)
                    if m:
                        k = "add_decl" if tgt == "add" else "del_decl"
                        v = (m.group(1) + "(" + re.sub(r"\s+", " ", m.group(2))[:110] + ")")[:150]
                        if v not in sym[k] and len(sym[k]) < 60: sym[k].append(v)
                m = RE_TEST.search(b)
                if m:
                    k = "add_test" if tgt == "add" else "del_test"
                    v = (m.group(1) + "." + m.group(2).strip())[:110]
                    if v not in sym[k] and len(sym[k]) < 60: sym[k].append(v)
                m = RE_INC.match(b)
                if m:
                    k = "add_inc" if tgt == "add" else "del_inc"
                    if m.group(1) not in sym[k] and len(sym[k]) < 25: sym[k].append(m.group(1))
                m = RE_TOL.match(b)
                if m:
                    k = "tol_add" if tgt == "add" else "tol_del"
                    v = (m.group(1) + "=" + m.group(2).strip())[:90]
                    if v not in sym[k] and len(sym[k]) < 40: sym[k].append(v)
                if kind == "code" and "/cli/" in ("/" + cur) and tgt == "add" and len(sym["cli_str"]) < 30:
                    for s in re.findall(r"\"([a-z][a-z0-9\-_]{2,24})\"", b):
                        if s not in sym["cli_str"]: sym["cli_str"].append(s)
            elif kind == "cmake" and tgt == "add":
                m = RE_CMAKE.search(b)
                if m:
                    v = (m.group(1) + " " + re.sub(r"\s+", " ", m.group(2)).strip())[:130]
                    if v not in sym["cmake"] and len(sym["cmake"]) < 40: sym["cmake"].append(v)
            elif kind == "md":
                m = RE_HEAD.match(b)
                if m:
                    k = "md_add" if tgt == "add" else "md_del"
                    v = (m.group(1) + " " + m.group(2))[:130]
                    if v not in sym[k] and len(sym[k]) < 60: sym[k].append(v)
            elif kind == "csv":
                k = "csv_add" if tgt == "add" else "csv_del"
                if len(sym[k]) < 60:
                    sym[k].append(cur.split("/")[-1] + " | " + re.sub(r"\s+", " ", b)[:150])
            elif kind == "json":
                m = RE_JSONKEY.match(b)
                if m:
                    k = "json_add" if tgt == "add" else "json_del"
                    if len(sym[k]) < 60: sym[k].append(cur.split("/")[-1] + " | " + m.group(1))
    d["sym"] = sym; d["truncated"] = truncated; d["struct_only"] = struct_only
    return d

def render(seq, sha, meta, ev):
    o = []
    o.append("# C%04d %s" % (seq, sha[:8]))
    o.append("- 时间 %s | 作者 %s <%s> | merge %s | 父 %s" % (meta["adate"], meta["aname"], meta["amail"],
             "是" if meta["merge"] else "否", ",".join(p[:8] for p in meta["parents"]) or "-"))
    o.append("- 谱系根 " + ",".join(sorted(set(r[:8] for r in meta["roots"]))))
    if meta["root_hint"]: o.append("- 所属谱系: " + meta["root_hint"])
    o.append("- 规模 文件 %d（噪声区 %d / 二进制 %d）+%d -%d | 分级 %s%s%s" % (
        ev["nfile"], ev["noise"], ev["bin"], ev["add"], ev["del"], ev["cls"],
        " | 结构模式（巨型提交，不抽符号）" if ev["struct_only"] else "",
        " | diff 读取截断" if ev["truncated"] else ""))
    if meta["ids"]: o.append("- 消息中的编号: " + ", ".join(meta["ids"][:30]))
    o.append("")
    o.append("## 提交消息（原文）")
    body = meta["subject"] + ("\n\n" + meta["body"] if meta["body"] else "")
    o.append(body[:4500] + ("\n\n（消息过长已截断，全文见 index.jsonl seq=%d）" % seq if len(body) > 4500 else ""))
    o.append("")
    o.append("## 变更面（按目录聚合，噪声区不计）")
    for k, v in sorted(ev["dirhist"].items(), key=lambda x: -x[1][0])[:18]:
        o.append("- %s : %d 文件 +%d -%d" % (k, v[0], v[1], v[2]))
    if not ev["dirhist"]: o.append("- （本提交改动全部落在排除区：影子树/构建产物/数据）")
    fl = ev["files"]
    def fmt(st, old, new):
        s = {"A": "新增", "M": "修改", "D": "删除", "R": "改名", "C": "复制", "T": "类型变更"}.get(st, st)
        return "- %s %s" % (s, (old + " -> " + new) if (old and new and old != new) else (new or old))
    for lab, sub in (("新增", [x for x in fl if x[0] == "A"]), ("删除", [x for x in fl if x[0] == "D"]),
                     ("改名或移动", [x for x in fl if x[0] in ("R", "C", "T")])):
        if sub:
            o.append("")
            o.append("## %s/变动文件（%d 项）" % (lab, len(sub)) if lab == "改名或移动" else "## %s文件（%d 项）" % (lab, len(sub)))
            for x in sub[:40]: o.append(fmt(x[0], x[1], x[2]))
            if len(sub) > 40: o.append("- …另有 %d 项未列（完整清单在 index.jsonl.files）" % (len(sub) - 40))
    s = ev["sym"]
    if any(s.values()):
        o.append("")
        o.append("## 结构要点（机械抽取，非人工判断）")
        def block(t, items, per=60):
            if items:
                o.append("**" + t + "**")
                for i in items[:per]: o.append("- " + i)
                if len(items) > per: o.append("- …共 %d 项" % len(items))
        block("触及的函数/类上下文（hunk 头）", s["hunk_ctx"], 45)
        block("新增声明", s["add_decl"]); block("删除声明", s["del_decl"])
        block("新增类型", s["add_type"]); block("删除类型", s["del_type"])
        block("新增测试", s["add_test"]); block("删除测试", s["del_test"])
        block("新增 include", s["add_inc"]); block("删除 include", s["del_inc"])
        block("构建/CMake 新增", s["cmake"])
        block("阈值或并行度赋值 新增", s["tol_add"]); block("阈值或并行度赋值 删除", s["tol_del"])
        block("文档规格标题 新增", s["md_add"]); block("文档规格标题 删除", s["md_del"])
        block("台账或CSV 行 新增", s["csv_add"], 30); block("台账或CSV 行 删除", s["csv_del"], 30)
        block("JSON 或 schema 键 新增", s["json_add"], 30); block("JSON 或 schema 键 删除", s["json_del"], 30)
        block("cli 字符串常量 新增", s["cli_str"], 20)
    if s.get("perfile"):
        o.append("**每文件变更行数**")
        for pth, v in list(s["perfile"].items())[:25]:
            o.append("- %s +%d -%d" % (pth, v[0], v[1]))
    if s.get("content"):
        o.append("")
        o.append("## 内容摘录（逐行，带方向；证据基底，非报告正文）")
        last = None
        for pth, sign, txt in s["content"][:80]:
            if pth != last:
                o.append("* " + pth); last = pth
            o.append("    %s %s" % (sign, txt))
        if len(s["content"]) >= 80: o.append("    （摘录达上限，其余见 git show）")
    o.append("")
    o.append("## 自述与变更面比对（客观）")
    msg_paths = sorted(set(re.findall(r"[\w./\-\u4e00-\u9fff]*\.(?:h|hpp|cpp|c|py|md|json|csv|txt|cmake|yml|yaml|sh|ps1)",
                     (meta["subject"] + " " + meta["body"]).replace("\n", " "))))[:12]
    o.append("- 消息点名的文件: " + (", ".join(msg_paths) if msg_paths else "（无）"))
    o.append("- 消息中的编号: " + (", ".join(meta["ids"][:20]) if meta["ids"] else "（无）"))
    o.append("- 变更面前列: " + (", ".join(list(ev["dirhist"].keys())[:6]) or "（无/全在排除区）"))
    inter = [p for p in msg_paths if any(p in k for k in ev["dirhist"])]
    o.append("- 点名与变更面交集: " + (", ".join(inter) if inter else "（无交集）"))
    o.append("")
    o.append("--- 本卡由 _tools/01_build_commit_evidence.py 生成。需要源码级确认时执行: git show " + sha[:8] + " -- <路径>")
    txt = "\n".join(o)
    cap = CARD_CAP[ev["cls"]]
    drop = ["json_del", "json_add", "csv_del", "csv_add", "md_del", "del_inc", "add_inc", "del_test", "del_decl", "del_type", "tol_del"]
    n = 0
    while len(txt.encode("utf-8")) > cap and n < len(drop):
        s[drop[n]] = []; n += 1
        txt = txt.split("--- 本卡由")[0] + "--- 本卡由脚本生成（部分抽取区块因容量上限省略，完整数据在 index.jsonl seq=%d）" % seq
    return txt

def main():
    argv = sys.argv[1:]
    def flag(name, default="0"): return argv[argv.index(name) + 1] if name in argv else default
    limit = int(flag("--limit")); offset = int(flag("--offset"))
    shas = order[offset: offset + limit] if limit else order[offset:]
    fh = open(os.path.join(EV, "index.jsonl"), "w", encoding="utf-8")
    pivot_dm = defaultdict(lambda: [0, 0, 0]); pivot_im = defaultdict(int); rows = []
    for i, sha in enumerate(shas, start=offset + 1):
        try:
            chunk = git(["show", "-s", "--format=%an%x1f%ae%x1f%aI%x1f%cI%x1f%s%x1f%b\x1e", sha]).split("\x1e")[0]
            an, ae, ai, ci, subj, body = (chunk.split("\x1f") + [""] * 6)[:6]
            meta = dict(sha=sha, seq=i, parents=parents.get(sha, []), merge=len(parents.get(sha, [])) > 1,
                        aname=an, amail=ae, adate=ai, cdate=ci, subject=subj.strip(), body=body.strip(),
                        roots=sorted(lin.get(sha, [])), in_head=sha in in_head)
            rl = meta["roots"]
            meta["root_hint"] = "" if len(rl) > 1 else ("主仓 c384de17" if rl == [MAIN_INIT] else ("上游根 " + (ROOT_SUBJECT.get(rl[0], "?")[:70] if rl else "?")))
            meta["ids"] = sorted(set(RE_TASKID.findall(meta["subject"] + " " + meta["body"])))[:40]
            ev = collect(sha, meta["merge"])
            cp = os.path.join(CARD_DIR, "%04d-%s.md" % (i, sha[:8]))
            open(cp, "w", encoding="utf-8").write(render(i, sha, meta, ev))
            rec = {k: meta[k] for k in ("seq", "sha", "parents", "merge", "aname", "amail", "adate", "cdate", "subject", "body", "ids", "roots", "in_head", "root_hint")}
            rec.update(dict(add=ev["add"], dele=ev["del"], nfile=ev["nfile"], nbin=ev["bin"], nnoise=ev["noise"],
                            cls=ev["cls"], struct_only=ev["struct_only"], truncated=ev["truncated"],
                            dirhist=ev["dirhist"], files=[list(x) for x in ev["files"]], card=os.path.relpath(cp, EV)))
            fh.write(json.dumps(rec, ensure_ascii=False) + "\n")
            month = ai[:7]
            for k, v in ev["dirhist"].items():
                e = pivot_dm[(k, month)]; e[0] += 1; e[1] += v[1]; e[2] += v[2]
            for tid in meta["ids"]: pivot_im[(tid, month)] += 1
            rows.append([i, sha[:8], ai[:10], an, ev["cls"], ev["nfile"], ev["add"], ev["del"], ";".join(meta["ids"][:8]), subj[:110]])
            if i % 200 == 0: print("done", i, flush=True)
        except Exception as e:
            warn("FAIL seq=%d sha=%s %r" % (i, sha[:8], e))
    fh.close()
    with open(os.path.join(EV, "cross_dir_month.csv"), "w", newline="", encoding="utf-8") as f:
        w = csv.writer(f); w.writerow(["目录", "月份", "提交数", "增行", "删行"])
        for (k, mth), v in sorted(pivot_dm.items()): w.writerow([k, mth] + v)
    with open(os.path.join(EV, "cross_id_month.csv"), "w", newline="", encoding="utf-8") as f:
        w = csv.writer(f); w.writerow(["编号", "月份", "出现次数"])
        for (tid, mth), v in sorted(pivot_im.items()): w.writerow([tid, mth, v])
    with open(os.path.join(EV, "commits_table.csv"), "w", newline="", encoding="utf-8") as f:
        w = csv.writer(f); w.writerow(["seq", "sha8", "日期", "作者", "分级", "文件数", "增行", "删行", "编号", "主题"])
        w.writerows(rows)
    print("OK commits=%d" % len(rows))

if __name__ == "__main__":
    main()
