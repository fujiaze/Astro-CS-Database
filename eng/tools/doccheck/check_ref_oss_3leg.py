#!/usr/bin/env python3
# -*- coding: utf-8 -*-
r"""G-REF-OSS-3LEG | 开源引用「三腿齐全」门（仓库 + 版本 + 文件位置）。

要判的是什么
  R-16 要求参考代码库（含许可证）清单的每一条都给「仓库 + 版本 + 文件位置」。
  本门把这一条变成能红能绿的机械判据：凡条目点名了某个开源库并给出源码 URL，
  则该**条目**（不是行）必须同时有：
    仓腿    源码 URL（github/gitlab/sourceforge/官方源码树…）
    版本腿  tag 2.8.6 / tag v2.14.0 / commit <sha7+> / 7.0.1 / v7.0.1 /
            ≥7.0.1 / 版本腿 = …  形态的版本串
    文件腿  src/back.c / photutils/centroids/core.py / isrFunctions.py /
            isrTask.py 形态的文件路径，或 <符号>() / class X / X.y 定位

**按条目聚合，不按行**（复核线教训）：同一条目常跨行书写，只看本行会漏判；
把相邻两条拼在一起又会把 A 的版本腿算给 B。故本门先做 markdown 块级切分，
每个 bullet / numbered item / 表格数据行 / 段落各算一个条目。

版本腿的识别口径（避免把许可证版本当库版本 —— 这是本门的核心假绿陷阱）
  LGPL-3.0 / GPL-3.0 / BSD-3-Clause / XISF 1.0 / FITS Standard 4.0 都含 \d+\.\d+，
  若只判「条目里有没有 \d+\.\d+」，**许可证版本**会被误判成**库版本**。
  故只认三处：① 条目出现 tag / commit / 版本腿 关键词；
  ② 版本串紧跟**源码 URL 之后**的第一个逗号位；
  ③ 形如 "7.0.1 = 控制节点实测" 的显式赋值。

版本腿 = **任何不可变标识**（RULING：版本腿判据纠错）
  前台裁定：**commit 比 tag 更强的不可变标识**（tag 可移动、commit 不会），
  故判据没认 commit = **判据错，不是文档错**。规则改为
  **tag / commit / 版本串三者任一即算版本腿；三者皆无才判红。**
  仓内先例（本门据此改判据的证据，两条都是逐字可查的）：
    · docs/science/NOISE_MODEL.md:379
      「LSST ip_isr（GPL-3.0，https://github.com/lsst/ip_isr，
        commit `28faec7dd2297d2ff9f108e543b2d55fdb046345`）」
      —— commit 被 **反引号**包住，旧正则 \\bcommit\\s+[0-9a-fA-F]{7,40}\\b 匹配不到
      ⇒ 旧判据把「有 commit」判成「缺版本腿」，**判红（假红）**。
    · docs/science/CALIBRATION.md:458
      「…版本 commit `5a3902196a7d7a701385a7113cbdce2976ae1a85`…」同因。
  同一条目里的「定位更正」留痕（NOISE_MODEL.md:379）进一步说明
  **本仓文档本来就接受 commit 形态作版本腿**。
  ⇒ 修法 = **放宽识别，不动文档**：tag / commit 允许 markdown 包裹
    （反引号、**、__、<>），commit 允许 `commit` 与 sha 之间夹 1–4 个非字母数字。

判据（fail-closed）
  O1 scan_floor   docs/ 不存在 / 扫到 0 个 .md / 0 个开源条目 => rc=2。
  O2 三腿         仓/版/文件任一缺 => 判红，逐腿列出缺哪条。
  O3 清单         逐条输出 file:line + 库名 + 缺腿。

不覆盖（如实声明）
  - **不联网**核「该版本/该文件在该 tag 上是否真的存在」——那是本轮一次性取证面
    （git ls-tree + git show 逐条实测，证据落在 run/FINAL-07/citation-fix/evidence/）。
    本门只判**引用文本自身**三腿是否齐备。
  - 不判许可证是否**正确**（那是 G-REF-LIC-CLOSED + 人工许可面）。

用法
  python3 eng/tools/doccheck/check_ref_oss_3leg.py [--root .] [--json-out F]
  python3 eng/tools/doccheck/check_ref_oss_3leg.py --self-test
"""
import argparse
import json
import os
import re
import sys
import tempfile

CHECK_ID = "G-REF-OSS-3LEG"
EXCLUDE_DIRS = ("build", "run", "testdata", "gaia", ".git")

# 仓内出现过的开源库 -> 必须出现的源码 URL 片段（任一命中即认定该条目点名此库）。
LIBRARIES = {
    "astropy": ["github.com/astropy/astropy"],
    "photutils": ["github.com/astropy/photutils"],
    "astropy-healpix": ["github.com/astropy/astropy-healpix"],
    "healpy": ["github.com/healpy/healpy"],
    "HEALPix C++": ["sourceforge.net/projects/healpix"],
    "DrizzlePac": ["github.com/spacetelescope/drizzlepac"],
    "SWarp": ["github.com/astromatic/swarp"],
    "SExtractor": ["github.com/astromatic/sextractor"],
    "PSFEx": ["github.com/astromatic/psfex"],
    "SCAMP": ["github.com/astromatic/scamp"],
    "ccdproc": ["github.com/astropy/ccdproc"],
    "LSST ip_isr": ["github.com/lsst/ip_isr"],
    "Siril": ["gitlab.com/free-astro/siril"],
    "WCSLIB": ["atnf.csiro.au/people/mcalabre/WCS", "github.com/Punzo/wcslib"],
    "CFITSIO": ["heasarc.gsfc.nasa.gov/fitsio"],
    "reproject": ["github.com/astropy/reproject"],
    "SEP": ["github.com/kbarbary/sep"],
    "IRAF": ["github.com/iraf-community/iraf", "iraf-community.github.io"],
    "PCL": ["gitlab.com/pixinsight/PCL"],
    "Astrometry.net": ["dstndstn/astrometry.net"],
    "DeepSkyStacker": ["github.com/deepskystacker/DSS"],
    "properimage": ["github.com/quatrope/properimage"],
    "GSL": ["gnu.org/software/gsl"],
    "CDS/Aladin": ["github.com/cds-astro/"],
}

# 只有「参考文献 / 参考代码库 / Primary literature / References / 引用定位」类小节进分母。
SECTION_RE = re.compile(
    r"^#{1,6}\s*(?P<title>.*)$"
)
SECTION_OK = re.compile(r"参考文献|参考代码库|Primary literature|References|引用定位")
ENTRY_SPLIT = re.compile(r"^\s*(?:[-*+]\s+|\d+[.)]\s+|\|\s*)")

# 版本腿 = 任何不可变标识（RULING 纠错：commit 比 tag 更强，判据没认 commit = 判据错）。
# 放宽点只有一处：**允许 markdown 包裹**（` ` ** __ <>）与 commit/sha 之间的短间隔。
# 仍**不**放行裸 d+.d+，否则 LGPL-3.0 / GPL-3.0 / BSD-3-Clause 会被误判成库版本
# （这是本门的核心假绿陷阱，见文件头）。
_MD = r"[\s\*_`<>\[\]\(\)]{0,4}"
VERSION_KEYWORD = re.compile(
    r"\btag" + _MD + r"v?\d+\.\d+"
    r"|\bcommit\b[^0-9a-fA-F\n]{0,4}[0-9a-fA-F]{7,40}\b"
    r"|版本腿" + _MD + r"="
    r"|\bv\d+\.\d+"
    r"|≥\s*v?\d+\.\d+"
    # 「版本 X.Y」形态（裁决：版本腿 = 任何不可变标识，tag/commit/版本串任一）。
    # 仍要求**版本**关键词在版本号之前 ⇒ 许可证版本（GPL-3.0/BSD-3-Clause）仍不被计入。
    r"|版本" + _MD + r"\*?\*?\s*v?\d+\.\d+"
    r"|\d+\.\d+(?:\.\d+)?\s*=\s*控制节点实测"
    r"|\bPyPI\s+v?\d+\.\d+"
)
FILE_TOKEN = re.compile(
    r"[A-Za-z0-9_][A-Za-z0-9_./-]*\.(?:c|h|py|pyx|js|cpp|cc|pyxb)\b"
    r"|\b[A-Za-z_][A-Za-z0-9_]*\(\)"
    r"|\bclass\s+[A-Za-z_][A-Za-z0-9_]*\b"
)
URL_RE = re.compile(r"https?://[^\s，）,。；;）】\]<>]+")
LICENSE_PREFIX = re.compile(r"\s*[，,]?\s*(?:LGPL|GPL|BSD|MIT|Apache|CC|CFITSIO|MPL)")


def iter_doc_files(root):
    docs = os.path.join(root, "docs")
    if not os.path.isdir(docs):
        return
    for dirpath, dirnames, filenames in os.walk(docs):
        dirnames[:] = [d for d in dirnames if d not in EXCLUDE_DIRS]
        for fn in sorted(filenames):
            if fn.endswith(".md"):
                yield os.path.join(dirpath, fn)


def iter_entries(text):
    """切出「引用条目」-> [(entry_text, first_line_no)]。只收参考文献类小节。"""
    entries = []
    cur, start, in_ref, fence = [], None, False, False
    for i, line in enumerate(text.splitlines(), 1):
        if line.lstrip().startswith("\u0060\u0060\u0060"):
            fence = not fence
            continue
        if fence:
            continue
        m = SECTION_RE.match(line)
        if m:
            if cur:
                entries.append(("\n".join(cur), start))
                cur, start = [], None
            in_ref = bool(SECTION_OK.search(m.group("title")))
            continue
        if not in_ref:
            continue
        if ENTRY_SPLIT.match(line) and line.strip():
            if cur:
                entries.append(("\n".join(cur), start))
            cur, start = [line], i
            continue
        if line.strip():
            if start is None:
                start = i
            cur.append(line)
        else:
            if cur:
                entries.append(("\n".join(cur), start))
                cur, start = [], None
    if cur:
        entries.append(("\n".join(cur), start))
    return entries


def classify(entry):
    """-> (lib or None, url or None, has_version, has_file)"""
    lib = url = None
    for name, urls in LIBRARIES.items():
        for u in urls:
            if u in entry:
                lib, url = name, u
                break
        if lib:
            break
    if not lib:
        return None, None, False, False
    has_version = bool(VERSION_KEYWORD.search(entry))
    if not has_version:
        m = URL_RE.search(entry)
        if m:
            tail = re.split(r"[。;；\n]", entry[m.end():m.end() + 80])[0]
            if not LICENSE_PREFIX.match(tail) and re.match(
                    r"\s*[，,]?\s*v?\d+\.\d+", tail):
                has_version = True
    has_file = bool(FILE_TOKEN.search(entry))
    return lib, url, has_version, has_file


def evaluate(root):
    files = list(iter_doc_files(root))
    entries = []
    for path in files:
        with open(path, "r", encoding="utf-8") as fh:
            rel = os.path.relpath(path, root)
            for text, lineno in iter_entries(fh.read()):
                lib, url, hv, hf = classify(text)
                if not lib:
                    continue
                missing = []
                if not url:
                    missing.append("仓腿")
                if not hv:
                    missing.append("版本腿")
                if not hf:
                    missing.append("文件腿")
                entries.append({
                    "file": rel, "line": lineno, "lib": lib, "url": url,
                    "has_version": hv, "has_file": hf, "missing": missing,
                    "verdict": "PASS" if not missing else "FAIL",
                    "excerpt": text.strip()[:160],
                })
    return {"files": len(files), "entries": entries}


def self_test():
    """注入正例与三类负例（缺版本腿 / 缺文件腿 / 只给许可证版本），逐一核判红。"""
    root = tempfile.mkdtemp(prefix="grefoss3leg_")
    os.makedirs(os.path.join(root, "docs", "science"), exist_ok=True)
    posf = os.path.join(root, "docs", "science", "POS.md")
    head = "## 14a 参考文献与参考代码库（含许可证）\n\n"
    with open(posf, "w", encoding="utf-8") as fh:
        fh.write(head
                 + "- SExtractor（GPL-3.0，https://github.com/astromatic/sextractor，"
                   "tag 2.8.6）：背景网格；文件位置 `src/back.c`（`makeback`）。\n"
                 + "- photutils（BSD-3-Clause，https://github.com/astropy/photutils，"
                   "tag 3.0.0）：检测/质心；文件位置 `photutils/centroids/core.py`。\n"
                 + "- LSST ip_isr（GPL-3.0，https://github.com/lsst/ip_isr，"
                   "commit 28faec7d）：ISR 顺序；文件位置 `isrFunctions.py`。\n")
    pos = evaluate(root)
    pos_ok = len(pos["entries"]) == 3 and all(e["verdict"] == "PASS"
                                              for e in pos["entries"])
    print("  %-6s POS 三条全三腿（解析到 %d 条）"
          % ("PASS" if pos_ok else "FAIL", len(pos["entries"])))
    for e in pos["entries"]:
        print("        %-16s version=%s file=%s" % (e["lib"], e["has_version"],
                                                    e["has_file"]))
    negf = os.path.join(root, "docs", "science", "NEG.md")
    negs = [
        ("NEG 缺版本腿",
         "- SExtractor（GPL-3.0，https://github.com/astromatic/sextractor）："
         "背景网格；文件位置 `src/back.c`。\n", "版本腿"),
        ("NEG 缺文件腿",
         "- SCAMP（GPL-3.0，https://github.com/astromatic/scamp，tag v2.14.0）："
         "联合定标。\n", "文件腿"),
        ("NEG 只有许可证版本(假绿陷阱)",
         "- WCSLIB（LGPL-3.0，https://www.atnf.csiro.au/people/mcalabre/WCS/）："
         "投影实现。\n", "版本腿"),
    ]
    neg_ok = True
    for label, body, expect in negs:
        with open(negf, "w", encoding="utf-8") as fh:
            fh.write(head + body)
        rep = evaluate(root)
        hit = [e for e in rep["entries"] if e["file"].endswith("NEG.md")]
        good = bool(hit) and hit[0]["verdict"] == "FAIL" and expect in hit[0]["missing"]
        neg_ok = neg_ok and good
        print("  %-6s %-32s 期望缺[%s] 实得%s"
              % ("PASS" if good else "FAIL", label, expect,
                 hit[0]["missing"] if hit else "NO-ENTRY"))
    # ── RULING 纠错自证（两条，缺一不可）────────────────────────────────────
    # (a) 只留 commit、去掉 tag => 仍绿（commit 是更弱的不可变标识也必须认）
    # (b) tag 与 commit 都去掉 => 必红（否则这条门对「无版本腿」零鉴别力）
    ruling = [
        ("RULING-a 只留 commit（去 tag）仍绿",
         "- SExtractor（GPL-3.0，https://github.com/astromatic/sextractor，"
         "commit 827f8a502356669bb684c7a12d470f058e94bf7a）：背景网格；"
         "文件位置 `src/back.c`。\n", "PASS"),
        ("RULING-b tag+commit 都去必红",
         "- SExtractor（GPL-3.0，https://github.com/astromatic/sextractor）："
         "背景网格；文件位置 `src/back.c`。\n", "FAIL"),
        # 回归护栏：反引号包裹的两个真实形态必须仍绿（仓内既有假红点）
        ("RULING-c commit 带反引号仍绿",
         "- LSST ip_isr（GPL-3.0，https://github.com/lsst/ip_isr，"
         "commit `28faec7dd2297d2ff9f108e543b2d55fdb046345`）"
         "`python/lsst/ip/isr/isrFunctions.py`。\n", "PASS"),
        ("RULING-d tag 带反引号仍绿",
         "- drizzlepac（BSD-3-Clause，https://github.com/spacetelescope/drizzlepac，"
         "tag `3.11.0`）`src/cdrizzlebox.c`。\n", "PASS"),
        # 「版本 X.Y.Z」形态（无 tag/commit 关键词）：必须绿，且许可证版本仍不得顶替
        ("RULING-e 「版本 4.6.4」形态仍绿",
         "- CFITSIO（CFITSIO Software License，https://heasarc.gsfc.nasa.gov/fitsio/，"
         "**版本 4.6.4**）；文件位置 `fitsio.h`（`CFITSIO_VERSION`）。\n", "PASS"),
        ("RULING-f 许可证版本不得顶替「版本」形态",
         "- SExtractor（GPL-3.0，https://github.com/astromatic/sextractor）："
         "背景网格；文件位置 `src/back.c`。\n", "FAIL"),
    ]
    ruling_ok = True
    for label, body, expect in ruling:
        with open(negf, "w", encoding="utf-8") as fh:
            fh.write(head + body)
        rep = evaluate(root)
        hit = [e for e in rep["entries"] if e["file"].endswith("NEG.md")]
        good = bool(hit) and hit[0]["verdict"] == expect
        ruling_ok = ruling_ok and good
        print("  %-6s %-34s 期望[%s] 实得%s"
              % ("PASS" if good else "FAIL", label, expect,
                 hit[0]["verdict"] if hit else "NO-ENTRY"))
    neg_ok = neg_ok and ruling_ok
    os.remove(negf)
    os.remove(posf)
    ok = pos_ok and neg_ok
    print("SELF-TEST %s" % ("PASS" if ok else "FAIL"))
    return 0 if ok else 1


def main():
    ap = argparse.ArgumentParser(description=CHECK_ID)
    ap.add_argument("--root", default=".")
    ap.add_argument("--json-out", default=None)
    ap.add_argument("--self-test", action="store_true")
    a = ap.parse_args()
    if a.self_test:
        return self_test()
    root = os.path.abspath(a.root)
    if not os.path.isdir(os.path.join(root, "docs")):
        print("O1_INPUT_UNAVAILABLE: docs/ 不存在", file=sys.stderr)
        return 2
    rep = evaluate(root)
    if rep["files"] == 0:
        print("O1_SCAN_FLOOR: 扫到 0 个 .md", file=sys.stderr)
        return 2
    if not rep["entries"]:
        print("O1_SCAN_FLOOR: 提取到 0 个开源条目（解析器空转，不得判绿）", file=sys.stderr)
        return 2
    fails = [e for e in rep["entries"] if e["verdict"] == "FAIL"]
    ok = len(rep["entries"]) - len(fails)
    print("[%s] 开源条目=%d  三腿齐全=%d (%.1f%%)  判红=%d"
          % (CHECK_ID, len(rep["entries"]), ok,
             100.0 * ok / len(rep["entries"]), len(fails)))
    for e in fails:
        print("  RED  %s:%d  %-16s 缺 %s"
              % (e["file"], e["line"], e["lib"], "/".join(e["missing"])))
    if a.json_out:
        rep["failures"] = fails
        with open(a.json_out, "w", encoding="utf-8") as fh:
            json.dump(rep, fh, ensure_ascii=False, indent=1, sort_keys=True)
    return 0 if not fails else 1


if __name__ == "__main__":
    sys.exit(main())
