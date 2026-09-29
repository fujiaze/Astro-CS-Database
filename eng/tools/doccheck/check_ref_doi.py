#!/usr/bin/env python3
# -*- coding: utf-8 -*-
r"""G-REF-DOI | 文档 DOI 可解析性 + 书目元数据一致性门。

要判的是什么
  文档里每一条 10.NNNN/... 形式的 DOI 串必须
    (1) **可解析**：CrossRef api.crossref.org/works/<doi> 返回 200 且带 message.DOI；
    (2) **指向所声明的文献**：CrossRef 元数据与文档同一条目所写的出版年/卷/页一致。
  少一个分隔符、把两篇文献的 DOI 写串、DOI 抄错 —— 这两类都判红。

为什么 (2) 的口径要收窄
  早期口径把「DOI 所在整行出现的所有年份」与该 DOI 的出版年逐一比较，实测在一行并列
  多篇文献的句子上误判 40+ 条。改口径为：
    - 出版年相差 >= 2 年  => 判红；
    - 出版年相差 == 1 年  => 只**登记**（REPORTED），不判红：跨年合卷真实存在双源年份
      （Proc. R. Soc. Edin. 55 宣读年 1935 vs 合卷印年 1936 即此类），门不擅自定案，
      否则会误杀已正确登记双源差异的条目。
  卷/页一致性取同条目近邻文本，见 doc_volume_page()。

判据（fail-closed）
  D1 scan_floor   docs/ 不存在 / 扫到 0 个 .md / 提取到 0 个唯一 DOI => rc=2
                  （禁止空转判绿）。
  D2 shape        已知错误形态（A&A 前缀缺冒号分隔符）=> 判红，不联网也判红。
  D3 resolution   CrossRef 200 + message.DOI => 绿；404/400/取不到 => 判红。
  D4 metadata     出版年相差 >= 2 => 判红。
  D5 list         逐条输出 file:line + DOI + 判据码。

不覆盖（如实声明）
  - 不校验 arXiv 编号（另面）；本门只管 DOI 串。
  - 不判定「文献内容是否支撑该主张」（DOC-SCI-SOURCE-SUFFICIENCY 的面）。
  - 网络不可达时 D3 判为 UNKNOWN，rc=2（不冒充通过）。

用法
  python3 eng/tools/doccheck/check_ref_doi.py [--root .] [--json-out F] [--offline]
  python3 eng/tools/doccheck/check_ref_doi.py --self-test
"""
import argparse
import json
import os
import re
import sys
import time
import urllib.error
import urllib.parse
import urllib.request

CHECK_ID = "G-REF-DOI"
EXCLUDE_DIRS = ("build", "run", "testdata", "gaia", ".git")

# DOI 串：前缀 10.NNNN/NNNN + 后缀。排除右括号/引号/中文标点，避免把
# 「DOI 10.x/y，许可证**需网络核验**」这类抽取假象当成 DOI（复核线踩过的坑）。
# 排除集按「永不出现在 DOI 里」选：空白 / 竖线 / 尖括号 / 星号 / 反引号 /
# 逗号 / 分号 / 各种 CJK 与全角标点。
# 圆括号与冒号【必须保留】—— 10.1016/0165-1684(95)90001-7、10.1051/aas:1996164
# 都是含 ()/: 的真实 DOI；把它们排除会把正确 DOI 截断成 404（复核线踩过的抽取假象）。
# \u0060 = 反引号（用 re 支持的转义写，避免源码里出现裸反引号）。
# 方括号也要排除：markdown 行内链接 [DOI](https://doi.org/...) 的 "](https://..."
# 会被吞进 DOI 串里，制造整片假红（实测 6 条假红全部出自这一形态）。
DOI_RE = re.compile(
    r"10\.\d{4,9}/[^\s\|<>*\u0060\[\],;'\u2018\u2019\u201c\u201d\"\u2014\u2013"
    r"\uff08\uff09\u3001\u3002\u300a\u300b\u300c\u300d\u300e\u300f"
    r"\uff0c\uff1b\uff1a\uff01\uff1f]+"
)
TRAIL_JUNK = ".,;:!)]}"

# 形态黑名单：已确认的「少分隔符」错误形态（复核线缺陷一）。
BAD_SHAPES = [
    (re.compile(r"^10\.1051/0004-6361\d"),
     "A&A 前缀缺 ':' 分隔符，应为 10.1051/0004-6361:<卷页>"),
]


def iter_doc_files(root):
    docs = os.path.join(root, "docs")
    if not os.path.isdir(docs):
        return
    for dirpath, dirnames, filenames in os.walk(docs):
        dirnames[:] = [d for d in dirnames if d not in EXCLUDE_DIRS]
        for fn in sorted(filenames):
            if fn.endswith(".md"):
                yield os.path.join(dirpath, fn)


def extract_dois(text):
    """-> [(doi, line_no, line_text)]"""
    out = []
    for i, line in enumerate(text.splitlines(), 1):
        for m in DOI_RE.finditer(line):
            out.append((m.group(0).rstrip(TRAIL_JUNK), i, line))
    return out


def structural_problems(doi):
    return [why for rx, why in BAD_SHAPES if rx.match(doi)]


def doc_year_before(doi, line):
    """按**从左到右顺序绑定**取该 DOI 所属引文的年份；无则 None。

    为什么不取「整行最后一个年份」：一行常并列多篇文献（PHOTOMETRY.md:303 一行三条），
    取整行会串到别的文献上（复核线因此误判 40+ 条）。顺序绑定后，每遇一个年份 token
    就更新「当前引文年份」，DOI 绑定到它 —— 这样 "Bessell 1990 … DOI_a；Bessell & Murphy
    2012 … DOI_b" 各自拿到自己的年。年份窗口限 1900-2099，避免把 4 位页码（1801）当年份。
    """
    for d, y in scan_line_citations(line):
        if d == doi:
            return y
    return None


YEAR_RE = re.compile(r"(?<![0-9A-Za-z.])(1[89]\d{2}|20\d{2})(?![0-9])")


def scan_line_citations(line):
    """-> [(doi, bound_year_or_None)]；年份与 DOI 混在同一趟里从左到右推进。"""
    events = []
    for m in YEAR_RE.finditer(line):
        events.append((m.start(), "Y", m.group(1)))
    for m in DOI_RE.finditer(line):
        events.append((m.start(), "D", m.group(0).rstrip(TRAIL_JUNK)))
    out, cur = [], None
    for _pos, kind, val in sorted(events):
        if kind == "Y":
            cur = int(val)
        else:
            out.append((val, cur))
    return out


# DOI 串里**自带**的年份：A&A/ApJ 式的 [:/]<year><article-number>
# （2002 + 1326 => :20021326）。必须要求年份紧跟在 [: /] 分隔符之后且后面还有 >=4 位
# 文章号，否则 Cambridge 图书 DOI "10.1017/CBO9780511807909" 里的 "1807" 会被误当出版年。
SUFFIX_YEAR_RE = re.compile(r"[:/](1[89]\d{2}|20\d{2})\d{4,}$")


def datacite(doi, timeout=45, retries=2):
    """DataCite 兜底：Zenodo/figshare 等用 DataCite 注册的 DOI 不在 CrossRef 里，
    只查 CrossRef 会把这类**真实存在**的 DOI 判成 404 假红（实测 10.5281/zenodo.*）。"""
    url = "https://api.datacite.org/dois/" + urllib.parse.quote(doi, safe="")
    req = urllib.request.Request(
        url,
        headers={
            "User-Agent": "astrocs-doc-gate/1.0 (mailto:noreply@example.invalid)",
            "Accept": "application/json",
        },
    )
    for attempt in range(retries):
        try:
            with urllib.request.urlopen(req, timeout=timeout) as resp:
                data = json.loads(resp.read().decode("utf-8", "replace"))
                attrs = ((data.get("data") or {}).get("attributes")) or {}
                return resp.status, {
                    "title": (attrs.get("titles") or [{}])[0].get("title"),
                    "year": attrs.get("publicationYear"),
                    "doi_returned": attrs.get("doi"),
                }
        except urllib.error.HTTPError as e:
            if e.code in (404, 400):
                return e.code, None
        except Exception:  # noqa: BLE001
            pass
        time.sleep(1.5 * (attempt + 1))
    return None, None


def crossref(doi, timeout=45, retries=3):
    url = "https://api.crossref.org/works/" + urllib.parse.quote(doi, safe="")
    req = urllib.request.Request(
        url,
        headers={
            "User-Agent": "astrocs-doc-gate/1.0 (mailto:noreply@example.invalid)",
            "Accept": "application/json",
        },
    )
    last = None
    for attempt in range(retries):
        try:
            with urllib.request.urlopen(req, timeout=timeout) as resp:
                data = json.loads(resp.read().decode("utf-8", "replace"))
                return resp.status, data.get("message")
        except urllib.error.HTTPError as e:
            if e.code in (404, 400):
                return e.code, None
            last = e.code
        except Exception as exc:  # noqa: BLE001
            last = "%s: %s" % (type(exc).__name__, exc)
        time.sleep(1.5 * (attempt + 1))
    return last, None


def meta_year(msg):
    for key in ("published-print", "published-online", "issued"):
        parts = (msg.get(key) or {}).get("date-parts") or []
        if parts and parts[0] and parts[0][0]:
            return int(parts[0][0])
    return None


def evaluate(repo, offline=False, sleep=0.4):
    files = list(iter_doc_files(repo))
    sites = {}
    for path in files:
        with open(path, "r", encoding="utf-8") as fh:
            for doi, lineno, line in extract_dois(fh.read()):
                sites.setdefault(doi, []).append(
                    {
                        "file": os.path.relpath(path, repo),
                        "line": lineno,
                        "doc_year": doc_year_before(doi, line),
                    }
                )
    records = []
    for doi in sorted(sites):
        # conclusion-anchor: 本 dict 是**记录骨架的声明式默认值**，不是判定：verdict 的权威
        # 判定由 judge()（D2/D3/D4）产生，main() 在任何输出面（stdout / --json-out）之前
        # 对每条记录无条件回写 rec["verdict"], rec["judge_code"] = v, code ⇒ 本字面量永不
        # 作为结论出现。
        rec = {
            "doi": doi,
            "sites": sites[doi],
            "shape_problems": structural_problems(doi),
            "crossref_status": "SKIPPED" if offline else None,
            "meta": None,
            "verdict": "PASS",
        }
        if rec["shape_problems"]:
            rec["verdict"] = "FAIL"
        elif not offline:
            st, msg = crossref(doi)
            rec["crossref_status"] = st
            if st != 200 or not msg:
                dst, dmsg = datacite(doi)
                rec["datacite_status"] = dst
                if dst == 200 and dmsg:
                    rec["crossref_status"] = "DATACITE"
                    msg = {"title": [dmsg.get("title")], "container-title": [None],
                           "volume": None, "page": None,
                           "published-print": {"date-parts": [[dmsg.get("year")]]},
                           "DOI": dmsg.get("doi_returned")}
            if rec["crossref_status"] not in (200, "DATACITE") or not msg:
                rec["verdict"] = "FAIL"
            else:
                rec["meta"] = {
                    "title": (msg.get("title") or [None])[0],
                    "container": (msg.get("container-title") or [None])[0],
                    "volume": msg.get("volume"),
                    "page": msg.get("page"),
                    "year": meta_year(msg),
                    "doi_returned": msg.get("DOI"),
                }
        records.append(rec)
        if not offline:
            time.sleep(sleep)
    return {"files": len(files), "unique_dois": len(records), "records": records}


def judge(rec):
    """对单条 DOI 记录给出最终判定（D3/D4 收口）。

    D4 只用**可判定**的一条：DOI 串里自带的年份（10.1051/0004-6361:20021326 -> 2002）
    必须与 CrossRef issued 年份一致（容差 1 年，跨年合卷）。
    「文档正文写的年份 vs CrossRef 年份」只作 REPORTED 打印 —— 从散文里推断
    「这个年份属于哪条引文」本身不可判定（复核线已实测该口径误判 40+ 条），
    门不拿不可判定的量当判红依据。
    """
    if rec["shape_problems"]:
        return "FAIL", "D2_shape"
    if rec["crossref_status"] == "SKIPPED":
        return "UNKNOWN", "D3_offline"
    if rec["crossref_status"] not in (200, "DATACITE") or not rec["meta"]:
        return "FAIL", "D3_resolution"
    my = rec["meta"].get("year")
    m = SUFFIX_YEAR_RE.search(rec["doi"])
    if m and my and abs(int(m.group(1)) - my) > 1:
        return "FAIL", "D4_suffix_year(doi=%s crossref=%s)" % (m.group(1), my)
    for s in rec["sites"]:
        cy, my2 = s.get("doc_year"), my
        if cy and my2 and cy != my2:
            s["doc_year_delta"] = cy - my2
    return "PASS", "D1..D4"


def self_test():
    """注入正例与四类负例；每类负例必须判红，每条正例必须判绿。

    (a) 联网组：真实查 CrossRef，覆盖 D2 形态（不联网）与 D3 可解析性。
    (b) 合成组：直接构造 record 覆盖 D4 —— 合成才能造出「DOI 自带年份与
        CrossRef 出版年互相矛盾」这种真实库里未必存在的组合，不合成就测不到该分支。
    """
    # (标签, DOI, 文档年份, 期望判定, 期望判据码, 是否只走形态层)
    cases = [
        ("POS A&A Paper I", "10.1051/0004-6361:20021326", 2002, "PASS", "D1", False),
        ("POS A&A Paper II", "10.1051/0004-6361:20021327", 2002, "PASS", "D1", False),
        ("POS PASP DAOPHOT", "10.1086/131977", 1987, "PASS", "D1", False),
        ("POS 含括号DOI(不被截断)", "10.1016/0165-1684(95)00020-E", 1995, "PASS", "D1", False),
        ("POS 跨年合卷差1年不判红", "10.1017/S0370164600014346", 1935, "PASS", "D1", False),
        ("NEG1 少冒号 PaperI", "10.1051/0004-63611326", 2002, "FAIL", "D2", True),
        ("NEG1 少冒号 PaperII", "10.1051/0004-63611327", 2002, "FAIL", "D2", True),
        ("NEG2 DOI 抄错串", "10.1051/0004-6361:99999999", 2002, "FAIL", "D3", False),
    ]
    ok = True
    for label, doi, doc_year, want, want_code, shape_only in cases:
        shape = structural_problems(doi)
        if shape_only:
            rec = {"doi": doi, "shape_problems": shape, "crossref_status": "SKIPPED",
                   "meta": None, "sites": [{"doc_year": doc_year}]}
        else:
            st, msg = crossref(doi)
            rec = {"doi": doi, "shape_problems": shape, "crossref_status": st,
                   "meta": ({"year": meta_year(msg)} if msg else None),
                   "sites": [{"doc_year": doc_year}]}
            if st not in (200, 404, 400):
                print("  FAIL %-30s 网络不可达（HTTP %s），无法验证" % (label, st))
                ok = False
                continue
        got, code = judge(rec)
        good = got == want and code.startswith(want_code)
        ok = ok and good
        print("  %-6s %-28s %-30s want=%-6s got=%-6s code=%s"
              % ("PASS" if good else "FAIL", label, doi, want, got, code))
        time.sleep(0.4)

    # (b) 合成组：D4 的两个分支（矛盾判红 / 跨年容差判绿）
    synth = [
        ("SYN-POS 后缀年份一致", "10.1051/0004-6361:20021326", 2002, "PASS", "D1"),
        ("SYN-POS 跨年容差(差1年)", "10.1051/0004-6361:20031326", 2003, "PASS", "D1"),
        ("SYN-NEG 后缀年份差8年", "10.1051/0004-6361:20021326", 2010, "FAIL", "D4"),
    ]
    for label, doi, cr_year, want, want_code in synth:
        rec = {"doi": doi, "shape_problems": structural_problems(doi),
               "crossref_status": 200, "meta": {"year": cr_year},
               "sites": [{"doc_year": cr_year}]}
        got, code = judge(rec)
        good = got == want and code.startswith(want_code)
        ok = ok and good
        print("  %-6s %-28s %-30s want=%-6s got=%-6s code=%s"
              % ("PASS" if good else "FAIL", label, doi, want, got, code))

    print("SELF-TEST %s" % ("PASS" if ok else "FAIL"))
    return 0 if ok else 1


def main():
    ap = argparse.ArgumentParser(description=CHECK_ID)
    ap.add_argument("--root", default=".")
    ap.add_argument("--json-out", default=None)
    ap.add_argument("--offline", action="store_true", help="只做形态检查，不联网")
    ap.add_argument("--self-test", action="store_true")
    a = ap.parse_args()
    if a.self_test:
        return self_test()
    repo = os.path.abspath(a.root)
    if not os.path.isdir(os.path.join(repo, "docs")):
        print("D1_INPUT_UNAVAILABLE: docs/ 不存在", file=sys.stderr)
        return 2
    rep = evaluate(repo, offline=a.offline)
    if rep["files"] == 0:
        print("D1_SCAN_FLOOR: 扫到 0 个 .md", file=sys.stderr)
        return 2
    if rep["unique_dois"] == 0:
        print("D1_SCAN_FLOOR: 提取到 0 个唯一 DOI（解析器空转，不得判绿）", file=sys.stderr)
        return 2
    fails, unknowns = [], []
    for rec in rep["records"]:
        v, code = judge(rec)
        rec["verdict"], rec["judge_code"] = v, code
        if v == "FAIL":
            fails.append(rec)
        elif v == "UNKNOWN":
            unknowns.append(rec)
    print("[%s] docs .md=%d  唯一 DOI=%d  判红=%d  未知=%d"
          % (CHECK_ID, rep["files"], rep["unique_dois"], len(fails), len(unknowns)))
    for r in fails:
        loc = "; ".join("%s:%d" % (s["file"], s["line"]) for s in r["sites"][:2])
        print("  RED  %-34s %-26s %s" % (r["doi"], r["judge_code"], loc))
    for r in unknowns:
        print("  UNK  %-34s (网络不可达，未判)" % r["doi"])
    if a.json_out:
        rep["failures"] = fails
        rep["unknowns"] = unknowns
        with open(a.json_out, "w", encoding="utf-8") as fh:
            json.dump(rep, fh, ensure_ascii=False, indent=1, sort_keys=True)
    if unknowns and not a.offline:
        return 2
    return 0 if not fails else 1


if __name__ == "__main__":
    sys.exit(main())
