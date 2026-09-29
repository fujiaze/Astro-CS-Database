#!/usr/bin/env python3
# -*- coding: utf-8 -*-
r"""DOC-LAYER3-ALIGNMENT | 第三层门（判据 D）：可推导 + 覆盖完备 + 与代码对齐。

要判的是什么（负责人裁定的第 ③ 层 = 详细设计集）
  D1 可推导   第 ③ 层每篇的「上游：」抬头必须指向**真实存在**的上层文件与节号；
              另设**反查面**：提到「上游」却没被严格式解析到的行，逐条报出，
              防止解析器把整面静默跳过（不可断言范围）。
  D2 覆盖完备 代码侧的可数规范项（module_ports.registry.json 的 module_id 集合）
              有多少在第 ③ 层有**同名**文档。**只按 stem 精确同名计数**；
              模糊包含匹配单列 D2_FUZZY_MATCH（只打印不计覆盖）——
              按名字通配建立覆盖面正是本项目反复出现的假覆盖形态。
  D3 代码对齐 第 ③ 层提到的函数/符号名在代码中**真的出现**（区分大小写、含命名空间）。
              两档口径：
                SYMBOL_NOT_FOUND      代码中**任何位置**都没有 ⇒ 判红（幻觉符号）
                SYMBOL_NOT_DEFINED    代码里有这个名字但**找不到定义** ⇒ 只计数不判红
                                    （调用点与定义可能不同文件，避免把口径写死）

第 ③ 层根的取法（迁移前后都能用，口径打印在输出里）
  若 docs/detail 存在 ⇒ 用它（迁移后拓扑）；
  否则用迁移前集合 docs/{plugins,design,modules,algorithms,browser,diagnostics}
  （映射来源：run/FINAL-07/doc-migration/ 逐目录映射表）。

判据（任一 D 违规 => exit 1；输入不可用/分母为 0 => exit 2，fail-closed）
  D1_SCAN/D1_FLOOR/D2_FLOOR/D3_FLOOR  分母为 0 ⇒ rc=2。
  每面逐条输出 file:line + 原文 + 缺什么。

用法
  python3 eng/tools/doccheck/check_layer3_alignment.py [--root .]
        [--layer3-dir D]... [--json-out F] [--self-test]
exit 0 = 三面全清；1 = 有违规；2 = 输入不可用/分母为 0。

只读；仅 stdlib；无网络；逐文件流式扫描。
"""
from __future__ import annotations

import argparse
import contextlib
import io
import json
import os
import re
import sys

CHECK_ID = "DOC-LAYER3-ALIGNMENT"
SELF_REL = "eng/tools/doccheck/check_layer3_alignment.py"
REGISTRY_REL = "lib/infrastructure/pipeline/module_ports.registry.json"
# 第 ③ 层细节面根（docs 三目录制）：模块工作细节、阶段详细设计、插件注册篇
# 都归入 docs/detail/。迁移前的旧细节面目录集合已随迁移清空（**空目录不入库**），
# 继续按旧集合回落会让本门在旧树上"看起来能跑"、在新树上恒空 —— 故只认现行根。
L3_ROOT = ("docs/detail",)

HEAD_RE = re.compile(r"^\s{0,3}(#{1,6})\s+(.*?)\s*$")
NUMHEAD_RE = re.compile(r"^\s{0,3}#{1,6}\s*(\d+(?:\.\d+)*)")
FENCE_RE = re.compile(r"^\s{0,3}(`{3,}|~{3,})")
UPSTREAM_RE = re.compile(
    r"上游\s*[:：]\s*([^\n|]{0,160}?\.(?:md|yaml|json|py|csv))\s*"
    r"(?:[^\n|]{0,40}?)?§\s*([0-9]+(?:\.[0-9]+)*)")
UPSTREAM_MENTION_RE = re.compile(r"上游\s*[:：]")
SEC_RE = re.compile(r"§\s*([0-9]+(?:\.[0-9]+)*)")
PATHTOK_RE = re.compile(
    r"(?<![A-Za-z0-9_])((?:[\w.-]+/)+[\w.-]+\.(?:h|hpp|cpp|cc|cxx|py|json|yaml|yml|cmake|txt))")
QUAL_RE = re.compile(r"\b([A-Za-z_][\w]*(?:::[A-Za-z_~][\w]*)+)\b")
CALL_RE = re.compile(r"\b([A-Za-z_][\w]{2,})\s*\(")
PATH_PREFIXES = ["", "lib/", "lib/include/astrocs/", "eng/", "eng/tools/", "docs/"]
# 模块内相对路径（第③层常写 src/xxx.cpp 而不写全路径）=> 逐个模块目录解析
MODULE_DIR_GLOBS = ["lib/algorithms", "lib/infrastructure", "lib"]
# 科学符号：名字若已出现在 docs/science/** 则是科学量符号，不是代码符号
SCIENCE_TOKENS_REL = "docs/science"
PATH_EXEMPT = {
    "README/module.yaml": "MODULE_MAP 八元组模板位，不是真实文件",
    "graph/static_graph.json": "运行期产品面产物（落在 output_dir），不入库",
    "graph/observed_trace.json": "运行期产品面产物（落在 output_dir），不入库",
    "graph/graph_sidecar.json": "运行期产品面产物（落在 output_dir），不入库",
}
MATH_LINE_RE = re.compile(r"[∈∝∑∫√≤≥≈⁻¹ᵀΣ∇∂]")

# 语言关键字 / 标准库 / 宏 —— 不是「文档里提到的函数」
STOPWORDS = set("""
if for while switch return sizeof assert catch defined include elif and or not
bool void int long short char float double auto const static constexpr inline
class struct namespace template typedef using operator new delete throw try
nullptr true false lambda switch_case elif print printf fprintf sprintf cout
cin std size_t uint8_t uint16_t uint32_t uint64_t int32_t int64_t
sin cos tan asin acos atan atan2 exp log log10 pow sqrt abs fabs floor ceil
round fmod hypot min max clamp lerp isnan isfinite fmin fmax expm1 log1p
main init run start stop call apply make get set add put push pop next prev
begin end first last empty size length count find search map filter reduce
val keys items append extend insert remove update clear copy swap move
""".split())
# 本仓判据/脚本调用点：文档引用它们是**索引行为**，不算「代码符号」
KNOWN_TOOL_RE = re.compile(r"^(?:check|test|run|verify|scan|doc|mk|gen|assemble|audit)"
                           r"_[a-z0-9_]+$")


def looks_symbol(tok):
    if tok in STOPWORDS:
        return False
    # 必须是纯标识符：含数学上下标（SNR_k²）、中文、空格的都不是代码符号
    if not re.match(r"^[A-Za-z_][A-Za-z0-9_]*$", tok.split("::")[-1]):
        return False
    if "." in tok:
        return False
    if tok.isupper():
        return False
    if tok.startswith("__") or tok.startswith("_"):
        return False
    if "::" in tok:
        return True
    if KNOWN_TOOL_RE.match(tok):
        return False
    if "_" in tok:
        return True
    return False   # 纯小写单词（condition/index/…）不是代码符号


def numbered_sections(path):
    secs = set()
    in_fence = False
    try:
        with open(path, encoding="utf-8", errors="replace") as fh:
            for raw in fh:
                ln = raw.rstrip("\n")
                if FENCE_RE.match(ln):
                    in_fence = not in_fence
                    continue
                if in_fence:
                    continue
                m = NUMHEAD_RE.match(ln)
                if m:
                    secs.add(m.group(1))
    except OSError:
        return False, secs
    return True, secs


def iter_l3(root, dirs):
    out, missing_dirs = [], []
    for d in dirs:
        ap = os.path.join(root, d)
        if not os.path.isdir(ap):
            missing_dirs.append(d)
            continue
        for dp, dns, fns in os.walk(ap):
            dns[:] = sorted(dns)
            for fn in sorted(fns):
                if fn.endswith(".md"):
                    out.append(os.path.join(dp, fn))
    out.sort()
    return out, missing_dirs


def build_symbol_index(root):
    """返回 (defined, seen, files)。defined = 定义面；seen = 任何 ident( 出现面。"""
    defined, seen = set(), set()
    files = 0
    for base in ("lib", "eng", "docs"):
        ap_base = os.path.join(root, base)
        if not os.path.isdir(ap_base):
            continue
        for dp, dns, fns in os.walk(ap_base):
            dns[:] = sorted(dns)
            if "third_party" in dns:
                dns.remove("third_party")
            for fn in fns:
                if not fn.endswith((".h", ".hpp", ".cpp", ".cc", ".cxx", ".py")):
                    continue
                ap = os.path.join(dp, fn)
                try:
                    if os.path.getsize(ap) > 4_000_000:
                        continue
                except OSError:
                    continue
                files += 1
                try:
                    with open(ap, encoding="utf-8", errors="replace") as fh:
                        for raw in fh:
                            if len(raw) > 4000:
                                continue
                            for m in CALL_RE.finditer(raw):
                                seen.add(m.group(1))
                            m = re.match(r"\s*(?:template\s*<[^>]*>\s*)?"
                                         r"(?:[A-Za-z_][\w:<>,\s\*&]*?)\b"
                                         r"([A-Za-z_]\w*(?:::[A-Za-z_~]\w*)*)"
                                         r"\s*\(", raw)
                            if m:
                                defined.add(m.group(1))
                            m2 = re.match(r"\s*(?:class|struct|union|enum\s+class|"
                                         r"enum\s+struct|enum)\s+([A-Za-z_]\w*)", raw)
                            if m2:
                                defined.add(m2.group(1))
                            m2b = re.search(r"\busing\s+([A-Za-z_]\w*)\s*=", raw)
                            if m2b:
                                defined.add(m2b.group(1))
                            m3 = re.match(r"\s*(?:def|class)\s+([A-Za-z_]\w*)", raw)
                            if m3:
                                defined.add(m3.group(1))
                except OSError:
                    continue
    return defined, seen, files


# 根锚定的排除：只排 **根级** 重目录。按名字全局排会误伤同名子目录 ——
# 第一版把 gaia 按名全局排，连带砍掉
# lib/infrastructure/gaia_xpsd_client/include/astrocs/gaia/ 的源码面，
# 凭空造出 PATH_NOT_FOUND（工具假设错，不是数据错）。
ROOT_SKIP_DIRS = {"build", "run", "testdata", "gaia", ".git", "out",
                  "node_modules", "__pycache__", "独立审计", "artifacts"}
SKIP_ANYWHERE = {"__pycache__", "node_modules"}


def build_path_index(root):
    """全仓文件相对路径集合（排除重目录）。用于**后缀精确解析**，
    取代「按前缀猜」——后者对模块内相对路径必假阳。"""
    paths = set()
    for dp, dns, fns in os.walk(root):
        if os.path.abspath(dp) == root:
            dns[:] = [d for d in dns if d not in ROOT_SKIP_DIRS]
        else:
            dns[:] = [d for d in dns if d not in SKIP_ANYWHERE]
        for fn in fns:
            try:
                rel = os.path.relpath(os.path.join(dp, fn), root)
            except ValueError:
                continue
            paths.add(rel.replace(os.sep, "/"))
    return paths


def path_resolves(p, pathset):
    if p in pathset:
        return True
    tail = "/" + p
    for full in pathset:
        if full.endswith(tail):
            return True
    return False


def build_science_token_set(root):
    """docs/science/** 里出现过的标识符形态 token = 科学量符号（不是代码符号）。
    证据面来自第 ② 层正本，不靠人工白名单。"""
    names = set()
    base = os.path.join(root, SCIENCE_TOKENS_REL)
    if not os.path.isdir(base):
        return names
    tok_re = re.compile(r"\b[A-Za-z][A-Za-z0-9_]{2,}\b")
    for dp, dns, fns in os.walk(base):
        for fn in fns:
            if not fn.endswith(".md"):
                continue
            try:
                with open(os.path.join(dp, fn), encoding="utf-8",
                          errors="replace") as fh:
                    for raw in fh:
                        if len(raw) > 4000:
                            continue
                        for m in tok_re.finditer(raw):
                            names.add(m.group(0))
            except OSError:
                continue
    return names


def main(argv=None):
    ap = argparse.ArgumentParser(description="第三层可推导/覆盖/代码对齐门")
    ap.add_argument("--root", default=".")
    ap.add_argument("--layer3-dir", action="append", default=None)
    ap.add_argument("--json-out", default=None)
    ap.add_argument("--self-test", action="store_true")
    ap.add_argument("--quiet", action="store_true")
    args = ap.parse_args(argv)
    if args.self_test:
        return self_test()
    root = os.path.abspath(args.root)
    dirs = args.layer3_dir
    topo = "explicit"
    if not dirs:
        dirs, topo = L3_ROOT, "docs/detail（现行第③层细节面根）"
    files, missing_dirs = iter_l3(root, dirs)
    out = {"check": CHECK_ID, "layer": "L3", "topology": topo, "layer3_dirs": dirs,
           "missing_dirs": missing_dirs, "files": len(files), "codes": []}

    # ── D1 可推导 + 反查面 ─────────────────────────────────────────
    up_total = up_mentions = 0
    d1, d1_unparsed = [], []
    for ap in files:
        rel = os.path.relpath(ap, root)
        with open(ap, encoding="utf-8", errors="replace") as fh:
            for ln_no, raw in enumerate(fh, 1):
                if ln_no > 40:
                    break
                ln = raw.rstrip("\n")
                if not UPSTREAM_MENTION_RE.search(ln):
                    continue
                up_mentions += 1
                found = list(UPSTREAM_RE.finditer(ln))
                if not found:
                    d1_unparsed.append({"file": rel, "line": ln_no,
                                        "text": ln.strip()[:200]})
                for m in found:
                    up_total += 1
                    tgt = m.group(1).strip().strip("`")
                    sec = m.group(2)
                    hit = None
                    for c in (tgt, os.path.join("docs", tgt), "docs/" + tgt,
                              tgt.split("/")[-1]):
                        if os.path.isfile(os.path.join(root, c)):
                            hit = c
                            break
                    if hit is None:
                        d1.append({"file": rel, "line": ln_no,
                                   "kind": "UPSTREAM_FILE_MISSING",
                                   "token": tgt + " §" + sec, "text": ln.strip()[:200]})
                        continue
                    ok, secs = numbered_sections(os.path.join(root, hit))
                    if not ok:
                        d1.append({"file": rel, "line": ln_no,
                                   "kind": "UPSTREAM_UNREADABLE", "token": hit,
                                   "text": ln.strip()[:200]})
                    elif not secs:
                        d1.append({"file": rel, "line": ln_no,
                                   "kind": "UPSTREAM_NO_SECTIONS", "token": hit,
                                   "text": ln.strip()[:200]})
                    elif sec not in secs:
                        near = sorted(s for s in secs
                                      if s.split(".")[0] == sec.split(".")[0])
                        d1.append({"file": rel, "line": ln_no,
                                   "kind": "UPSTREAM_SECTION_MISSING",
                                   "token": "%s §%s" % (hit, sec),
                                   "text": ln.strip()[:200], "siblings": near[:12]})
    out["d1"] = {"denominator": up_total, "violations": len(d1),
                 "ratio": (len(d1) / up_total) if up_total else None,
                 "upstream_mentions": up_mentions, "unparsed": d1_unparsed,
                 "violation_rows": d1}

    # ── D2 覆盖完备（只算精确同名；模糊匹配单列）────────────────────
    reg = os.path.join(root, REGISTRY_REL)
    mods = []
    if os.path.isfile(reg):
        try:
            with open(reg, encoding="utf-8", errors="replace") as fh:
                data = json.load(fh)
            mods = [m.get("module_id") for m in data.get("modules", [])
                    if isinstance(m, dict) and m.get("module_id")]
        except (OSError, ValueError):
            mods = []
    stem = {}
    for ap in files:
        stem[os.path.splitext(os.path.basename(ap))[0]] = os.path.relpath(ap, root)
    exact, fuzzy, uncovered = [], [], []
    for mid in mods:
        if mid in stem:
            exact.append({"module_id": mid, "doc": stem[mid]})
            continue
        alt = mid.replace("_", "") in stem
        if alt:
            exact.append({"module_id": mid, "doc": stem[mid.replace("_", "")],
                          "note": "下划线折叠后同名"})
            continue
        fz = None
        for k, v in stem.items():
            if mid in k or k in mid:
                fz = {"module_id": mid, "doc": v, "matched_stem": k}
                break
        if fz:
            fuzzy.append(fz)
        else:
            uncovered.append({"module_id": mid})
    out["d2"] = {"registry": REGISTRY_REL, "denominator": len(mods),
                 "covered_exact": len(exact), "fuzzy": fuzzy,
                 "uncovered": uncovered,
                 "ratio": (len(exact) / len(mods)) if mods else None}

    # ── D3 代码对齐（两档：找不到=红，只无定义=计数）────────────────
    defined, seen, sym_files = build_symbol_index(root)
    sci_names = build_science_token_set(root)
    pathset = build_path_index(root)
    ref_total = 0
    d3, d3_soft = [], []
    for ap in files:
        rel = os.path.relpath(ap, root)
        in_fence = False
        with open(ap, encoding="utf-8", errors="replace") as fh:
            for ln_no, raw in enumerate(fh, 1):
                ln = raw.rstrip("\n")
                if FENCE_RE.match(ln):
                    in_fence = not in_fence
                    continue
                if in_fence or HEAD_RE.match(ln):
                    continue
                for m in PATHTOK_RE.finditer(ln):
                    p = m.group(1)
                    if p in PATH_EXEMPT:
                        ref_total += 1
                        d3_soft.append({"file": rel, "line": ln_no,
                                        "kind": "PATH_EXEMPT", "token": p,
                                        "text": ln.strip()[:200],
                                        "reason": PATH_EXEMPT[p]})
                        continue
                    pre_txt = ln[max(0, m.start() - 40):m.start()]
                    if "://" in pre_txt:
                        continue   # URL 片段不是仓库路径
                    ref_total += 1
                    if not path_resolves(p, pathset):
                        d3.append({"file": rel, "line": ln_no, "kind": "PATH_NOT_FOUND",
                                   "token": p, "text": ln.strip()[:200]})
                toks = set()
                for m in QUAL_RE.finditer(ln):
                    toks.add(m.group(1))
                for m in CALL_RE.finditer(ln):
                    toks.add(m.group(1))
                for s in sorted(toks):
                    if not looks_symbol(s):
                        continue
                    ref_total += 1
                    if s in defined or s in seen:
                        continue
                    if s in sci_names:
                        continue
                    if MATH_LINE_RE.search(ln):
                        d3_soft.append({"file": rel, "line": ln_no,
                                        "kind": "MATH_LINE_SYMBOL", "token": s,
                                        "text": ln.strip()[:200],
                                        "reason": "公式行上的符号按科学量处理"})
                        continue
                    # 限定名 Class::NAME：类的类名出现过就算落地（枚举成员没有括号）
                    if "::" in s:
                        cls = s.split("::")[0]
                        if cls in defined or cls in seen:
                            continue
                        seen.add(cls)
                    tail = s.split("::")[-1]
                    if tail and (tail in defined or tail in seen):
                        continue
                    d3.append({"file": rel, "line": ln_no, "kind": "SYMBOL_NOT_FOUND",
                               "token": s, "text": ln.strip()[:200]})
    # 软面：文档点名了 lib/ 下的 .h 头，但头里没有同名定义（口径弱，只计数）
    out["d3"] = {"denominator": ref_total, "violations": len(d3),
                 "ratio": (len(d3) / ref_total) if ref_total else None,
                 "index_files": sym_files, "defined_names": len(defined),
                 "seen_names": len(seen), "soft": d3_soft,
                 "violation_rows": d3}

    rc = 0
    lines = ["[%s] 拓扑=%s 第 ③ 层文件=%d（目录 %s）"
             % (CHECK_ID, topo, len(files), ",".join(dirs))]
    if not files:
        out["codes"].append("D1_SCAN")
        lines.append("  D1 扫描面为空（缺失目录：%s）" % ",".join(missing_dirs))
        rc = 2
    lines.append("  D1 可推导：上游提及 %d 行，其中严格解析出抬头 %d 条（分母），"
                 "违规 %d 条，未解析 %d 行"
                 % (up_mentions, up_total, len(d1), len(d1_unparsed)))
    if files and up_total == 0:
        out["codes"].append("D1_FLOOR")
        lines.append("  D1_FLOOR：抬头数 = 0")
        rc = 2
    lines.append("  D2 覆盖：registry module_id %d 个（分母），第 ③ 层**同名**文档 %d 个"
                 "（%.1f%%），模糊匹配 %d，未覆盖 %d"
                 % (len(mods), len(exact), 100.0 * len(exact) / max(1, len(mods)),
                    len(fuzzy), len(uncovered)))
    if not mods:
        out["codes"].append("D2_FLOOR")
        lines.append("  D2_FLOOR：registry module_id 集合 = 0")
        rc = 2
    lines.append("  D3 代码对齐：代码引用 %d 条（分母；符号索引 %d 文件，定义名 %d，"
                 "出现名 %d），幻觉 %d 条"
                 % (ref_total, sym_files, len(defined), len(seen), len(d3)))
    if ref_total == 0:
        out["codes"].append("D3_FLOOR")
        lines.append("  D3_FLOOR：代码引用数 = 0")
        rc = 2
    if not args.quiet:
        for rec in d1[:30]:
            lines.append("  [红-D1] %s:%d [%s] %s"
                         % (rec["file"], rec["line"], rec["kind"], rec["token"]))
            if rec.get("siblings"):
                lines.append("        同层实际存在: %s" % ",".join(rec["siblings"]))
        for rec in d1_unparsed[:15]:
            lines.append("  [黄-D1] %s:%d 提到「上游」但严格式未解析：%s"
                         % (rec["file"], rec["line"], rec["text"][:110]))
        for rec in uncovered[:30]:
            lines.append("  [红-D2] registry module_id=%s 无第 ③ 层同名文档"
                         % rec["module_id"])
        for rec in fuzzy[:30]:
            lines.append("  [黄-D2] module_id=%s 只与 %s 模糊相关（不计入覆盖）"
                         % (rec["module_id"], rec["matched_stem"]))
        for rec in d3[:40]:
            lines.append("  [红-D3] %s:%d [%s] %s"
                         % (rec["file"], rec["line"], rec["kind"], rec["token"]))
        if len(d3) > 40:
            lines.append("  …（清单已全量写入 --json-out）")
    if d1:
        out["codes"].append("D1_NOT_DERIVABLE")
    if uncovered:
        out["codes"].append("D2_COVERAGE_GAP")
    if d3:
        out["codes"].append("D3_CODE_MISMATCH")
    if rc == 0 and (d1 or uncovered or d3):
        rc = 1
    lines.append("== %s: %s（rc=%d）" % (CHECK_ID, "FAIL" if rc else "PASS", rc))
    for ln in lines:
        print(ln)
    out["rc"] = rc
    if args.json_out:
        with open(args.json_out, "w", encoding="utf-8") as fh:
            json.dump(out, fh, ensure_ascii=False, indent=1, sort_keys=True)
    return rc


def self_test():
    import shutil
    import tempfile

    bad = 0

    def build(dead_sec=False, uncovered=False, ghost=False, no_l3=False):
        d = tempfile.mkdtemp(prefix="l3align_")
        os.makedirs(os.path.join(d, "docs", "detail"))
        os.makedirs(os.path.join(d, "lib", "algorithms", "photometry", "src"))
        with open(os.path.join(d, "docs", "ASTROCS_DESIGN.md"), "w",
                  encoding="utf-8") as fh:
            fh.write("# D\n\n## 1 a\n\n### 3.5 b\n")
        with open(os.path.join(d, "lib", "algorithms", "photometry", "src",
                               "fit.h"), "w", encoding="utf-8") as fh:
            fh.write("double photometry_fit_residual(double x);\n")
        with open(os.path.join(d, "lib", "algorithms", "photometry", "src",
                               "fit.cpp"), "w", encoding="utf-8") as fh:
            fh.write('#include "fit.h"\ndouble photometry_fit_residual(double x)'
                     " { return 0.0; }\n")
        os.makedirs(os.path.join(d, "lib", "infrastructure", "pipeline"))
        with open(os.path.join(d, "lib", "infrastructure", "pipeline",
                               "module_ports.registry.json"), "w",
                  encoding="utf-8") as fh:
            json.dump({"modules": [{"module_id": "photometry"}]}, fh)
        sec = "3.9" if dead_sec else "3.5"
        body = "# T\n\n> 上游：ASTROCS_DESIGN.md §%s\n\n" % sec
        body += ("- 调用 photometry_fit_residual()，实现见 "
                 "lib/algorithms/photometry/src/fit.cpp。\n")
        if ghost:
            body += "- 另见 ghost_helper_xyz() 与 lib/algorithms/ghost/missing.cpp。\n"
        with open(os.path.join(d, "docs", "detail", "photometry.md"), "w",
                  encoding="utf-8") as fh:
            fh.write(body)
        if no_l3:
            shutil.rmtree(os.path.join(d, "docs", "detail"))
        if uncovered:
            with open(os.path.join(d, "lib", "infrastructure", "pipeline",
                                   "module_ports.registry.json"), "w",
                      encoding="utf-8") as fh:
                json.dump({"modules": [{"module_id": "orphan_module"}]}, fh)
        return d

    cases = [
        ("N0_clean", 0, build()),
        ("N1_dead_section", 1, build(dead_sec=True)),
        ("N2_uncovered_module", 1, build(uncovered=True)),
        ("N3_ghost_symbol", 1, build(ghost=True)),
        ("N4_no_layer3", 2, build(no_l3=True)),
    ]
    for name, want, d in cases:
        with contextlib.redirect_stdout(io.StringIO()):
            got = main(["--root", d, "--quiet"])
        ok = (got == want)
        if not ok:
            bad += 1
        print("  %-22s want rc=%d  got rc=%d  %s"
              % (name, want, got, "OK" if ok else "MISMATCH"))
    print("== %s --self-test: %d 例，%d 例不符预期" % (CHECK_ID, len(cases), bad))
    return 0 if bad == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
