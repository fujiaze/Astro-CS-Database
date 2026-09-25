# -*- coding: utf-8 -*-
"""D8 聚合 · 步骤2：对象规范化 + 两层联接（可复用模块）。

用法：cd 产出 && python -B 复算/d8agg/s2_index.py
只读：只读 findings_base.csv、raw/复核-*.md、复算/d8agg/tracked_files.txt。
"""
import csv, sys, os, re, json, collections

sys.stdout.reconfigure(encoding="utf-8", errors="replace")

INV = "inventory/findings_base.csv"
TRACKED = "复算/d8agg/tracked_files.txt"

tracked = [l.rstrip("\n") for l in open(TRACKED, encoding="utf-8") if l.strip()]
tracked_set = set(tracked)
tracked_dirs = set()
for t in tracked:
    p = t.split("/")
    for i in range(1, len(p)):
        tracked_dirs.add("/".join(p[:i]) + "/")
ROOT_FILES = {t for t in tracked if "/" not in t}
TOP_PREFIX = tuple(sorted({t.split("/")[0] for t in tracked if "/" in t},
                          key=len, reverse=True)) + tuple(ROOT_FILES)

NOISE_PREFIX = {
    "docs/", "docs/science/", "docs/algorithms/", "docs/contracts/",
    "docs/design/", "docs/ci/", "docs/modules/", "docs/standards/",
    "docs/plugins/", "docs/research/", "docs/validation/", "docs/owner/",
    "docs/quality/", "docs/reference/", "docs/references/", "docs/algorithms",
    "eng/", "lib/", "run/", "实验/", "testdata/", "artifacts/", "产出/",
    "工程控制/", "reports/", "archive_deliverables/",
}
KNOWN_EXT = (".md", ".cpp", ".h", ".hpp", ".json", ".py", ".yaml", ".yml",
             ".txt", ".csv", ".cmake", ".in", ".def", ".map", ".sh", ".ps1",
             ".jsonl", ".toml", ".cfg", ".inc", ".cxx", ".ipp", ".rst")
PATH_RE = re.compile(
    r"(?:%s)/(?:[\w.\-一-鿿]+/)*[\w.\-一-鿿]+\.(?:md|cpp|h|hpp|json|py|yaml|yml|csv|txt|cmake|inc|def|sh|ps1|toml|jsonl|ipp|cxx|in)"
    % "|".join(re.escape(d) for d in TOP_PREFIX))


def looks_pathish(v):
    if not v:
        return False
    if "/" in v:
        head = v.split("/")[0]
        return head in TOP_PREFIX
    return v in ROOT_FILES


def strip_anchor(s):
    s = s.strip().strip("`").strip()
    s = s.strip("「」《》()（）[]【】,，。;；:：*—~ ").lstrip("./")
    anchor = None
    m = re.search(r"^(.*?)[::](\d+(?:\s*[-–,，]\s*\d+)?)$", s)
    if m and looks_pathish(m.group(1)) and m.group(1):
        anchor = re.sub(r"\s+", "", m.group(2))
        s = m.group(1)
    s = s.rstrip(".,;:、")
    if s.endswith("/") and not s.endswith("//"):
        return s, anchor
    return s.rstrip("/"), anchor


def norm_obj(tok):
    v, anchor = strip_anchor(tok)
    if not v:
        return ("junk", None, None)
    if " " in v:
        cand = [c for c in v.split() if looks_pathish(c)]
        v = cand[-1] if cand else v
    if not looks_pathish(v):
        if re.match(r"^[a-z][a-z0-9_]*(\.[a-z0-9_]+){1,4}$", v):
            return ("config", v, anchor)
        return ("junk", None, None)
    v = v.replace("\\", "/")
    base = v[:-1] if v.endswith("/") else v
    if v.endswith("/") or (base + "/") in tracked_dirs:
        return ("dir", base + "/", anchor)
    return ("file", base, anchor)


def recover_from_text(text):
    return sorted({m.group(0) for m in PATH_RE.finditer(text or "")
                   if m.group(0) in tracked_set})


# ------------------------------------------------------------ 复核件判定抽取
SEC_RE = re.compile(
    r"^##\s+(V\d+|W\d+|R\d+|P\d+|§\d|(?:一|二|三|四|五|六|七|八|九|十)、|附[:：]|"
    r"本批汇总|进 P|推翻或降级|必须负责人裁|需负责人裁|.*收工.*|.*开工.*|.*基线.*|"
    r".*改判一览|.*结论汇总)\s*(.*)$")
VERD_RE = re.compile(r"\*\*\s*判定\s*[:：]\s*\*{0,2}([^\n]{0,90})")
VERD_RE2 = re.compile(r"^#{3,4}\s*判定\s*[:：]\s*\*{0,2}([^\n]{0,90})", re.M)
CLS = [("VOID", ("推翻", "撤销", "不成立", "作废")),
       ("MIXED", ("部分确认", "部分推翻", "部分成立")),
       ("PASS_DEGRADED", ("降级",)),
       ("PASS", ("确认",)),
       ("NOTCHECKED", ("未核", "待证", "本次未核"))]


def vclass(v):
    if not v:
        return None
    for name, keys in [("VOID", CLS[0][1]) if "推翻" not in v[:3] else (None, ())]:
        pass
    head = v
    # "确认存在…＋定性降级" 之类：以首个出现的强判词定档，但 VOID 只在全条以推翻为主时给
    if v.lstrip().startswith(("推翻", "撤销")) or re.match(r"^\s*\*{0,2}(推翻|撤销)", v):
        return "VOID"
    if "部分确认" in v or "部分推翻" in v:
        return "MIXED"
    if "确认" in v and "降级" in v:
        return "PASS_DEGRADED" if v.index("降级") < v.index("确认") + 6 else "PASS"
    if "确认" in v:
        return "PASS"
    if "降级" in v:
        return "PASS_DEGRADED"
    if "未核" in v or "待证" in v:
        return "NOTCHECKED"
    if "不成立" in v or "作废" in v or "撤销" in v:
        return "VOID"
    return None


def parse_review(path):
    lines = open(path, encoding="utf-8").read().splitlines()
    secs = []
    for i, ln in enumerate(lines, 1):
        m = SEC_RE.match(ln)
        if m:
            sid = m.group(1).rstrip("、").strip()
            secs.append({"id": sid, "title": m.group(2)[:70], "start": i,
                         "end": len(lines) + 1, "verdict": None, "cls": None,
                         "grade": None})
    for k in range(len(secs) - 1):
        secs[k]["end"] = secs[k + 1]["start"]
    for s in secs:
        body = "\n".join(lines[s["start"] - 1:s["end"] - 1])
        # 判定行可能出现在小节任意处（如"## 三、判定"里）——取小节内首个
        mv = VERD_RE.search(body) or VERD_RE2.search(body)
        if mv:
            s["verdict"] = mv.group(1).strip().rstrip("*").strip()
            s["cls"] = vclass(s["verdict"])
        mg = re.search(r"(?<![\w])(P[0-2])(?![\w])", body[:400])
        if mg:
            s["grade"] = mg.group(1)
    # 汇总兜底：正文无 **判定** 的 section，用文件尾部汇总表 / 表格行补
    txt = "\n".join(lines)
    for s in secs:
        if s["verdict"]:
            continue
        sid = re.escape(s["id"])
        row = re.search(r"^\|\s*\**%s\b[^|]*\*{0,2}\s*\|([^|]*)\|([^|]*)\|" % sid,
                        txt, re.M)
        if not row:
            row = re.search(r"^\|\s*%s\s*\|" % sid, txt, re.M)
        if row:
            s["verdict"] = row.group(1).strip().strip("*").strip()
            s["cls"] = vclass(s["verdict"])
            g = re.search(r"P[0-2]", row.group(0))
            if g:
                s["grade"] = g.group(0)
    return lines, secs


def load_reviews():
    out = {}
    for fn in sorted(os.listdir("raw")):
        if fn.startswith("复核-") and fn.endswith(".md"):
            lines, secs = parse_review("raw/" + fn)
            out[fn] = {"lines": lines, "secs": secs}
    return out


# ------------------------------------------------------------ 底表载入 + 聚合
def build():
    rows = list(csv.DictReader(open(INV, newline="", encoding="utf-8-sig")))
    reviews = load_reviews()
    L2 = lambda r: r["来源成稿"].startswith("复核-")
    L1 = lambda r: r["来源成稿"].startswith("AUD-")
    AUX = lambda r: not L1(r) and not L2(r)

    # 复核底表行 -> section
    def sec_of(fn, lineno):
        rv = reviews.get(fn)
        if not rv:
            return None
        try:
            ln = int(lineno)
        except Exception:
            return None
        for s in rv["secs"]:
            if s["start"] <= ln < s["end"]:
                return s
        return None

    objs = {}

    def slot(key):
        return objs.setdefault(key, {"l1": [], "l2": [], "noise": [],
                                     "offrepo": False, "cfg": False})

    stat = collections.Counter()
    for r in rows:
        src, ln = r["来源成稿"], r["行"]
        toks = [t for t in (r["被点名对象"] or "").split() if t] or \
               [t for t in (r["全部对象"] or "").split() if t]
        rec = {"src": src, "line": ln, "entry": (r["条目"] or "").strip(),
               "title": (r["短标题"] or "").strip()[:120],
               "grade": (r["定级"] or "").strip(),
               "conf": (r["置信"] or "").strip(),
               "text": (r["原文"] or "").strip()}
        hit = False
        for tok in toks:
            kind, val, anc = norm_obj(tok)
            if kind == "junk":
                stat["junk"] += 1
                continue
            rec["anchor"] = anc
            rec["kind"] = kind
            if kind == "config":
                stat["config"] += 1
                slot("cfg:" + val)["cfg"] = True
                (rec.update(obj=None))
                (slot("cfg:" + val)["l2" if L2(r) else "l1"].append(rec))
                hit = True
                continue
            if kind == "dir" and val in NOISE_PREFIX:
                stat["noise_tok"] += 1
                rr = recover_from_text(rec["text"])
                if rr:
                    stat["noise_recovered"] += len(rr)
                    for p in rr:
                        slot(p)["l2" if L2(r) else "l1"].append(rec)
                    hit = True
                    continue
                slot("__NOISE__" + val)["noise"].append(rec)
                hit = True
                continue
            if kind == "file" and val not in tracked_set:
                o = slot(val)
                if not (val in tracked_dirs or any(t.startswith(val + "/") for t in tracked)):
                    o["offrepo"] = True
                    stat["offrepo"] += 1
            stat[kind] += 1
            slot(val)["l2" if L2(r) else "l1"].append(rec)
            hit = True
        if not hit:
            stat["row_no_obj"] += 1
        if AUX(r):
            stat["aux_rows"] += 1
        # 复核行的 section 归属
        if L2(r):
            s = sec_of(src, ln)
            if s:
                for tok in toks:
                    kind, val, anc = norm_obj(tok)
                    if kind in ("file", "dir", "config") and val:
                        if kind == "dir" and val in NOISE_PREFIX:
                            for p in recover_from_text(rec["text"]):
                                slot(p)["l2"].append(dict(rec, sec=s["id"],
                                                          secverdict=s["verdict"],
                                                          seccls=s["cls"],
                                                          secgrade=s["grade"]))
                            continue
                        slot(val)["l2"].append(dict(rec, sec=s["id"],
                                                    secverdict=s["verdict"],
                                                    seccls=s["cls"],
                                                    secgrade=s["grade"]))
    return rows, reviews, objs, stat


if __name__ == "__main__":
    rows, reviews, objs, stat = build()
    print("== 复核件 section/判定 ==")
    for fn, rv in sorted(reviews.items()):
        got = [s for s in rv["secs"] if s["verdict"]]
        print("%-26s sec=%-3d 判定=%-3d %s" % (fn, len(rv["secs"]), len(got),
              "  ".join("%s=%s%s" % (s["id"], (s["cls"] or "?")[:4],
                                     "/" + s["grade"] if s["grade"] else "")
                        for s in got)))
    print()
    print("token:", dict(stat))
    l1o = {k for k, v in objs.items() if v["l1"]}
    l2o = {k for k, v in objs.items() if v["l2"]}
    print("对象总数 %d | 仅L1 %d | 仅L2 %d | 双层 %d | 噪声桶 %d"
          % (len(objs), len(l1o - l2o), len(l2o - l1o), len(l1o & l2o),
             sum(1 for k in objs if k.startswith("__NOISE__"))))
