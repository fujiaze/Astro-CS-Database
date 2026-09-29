#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""CHK-NO-WEIGHT-MODE：详细文档层「权重模式」整套作废门（DOC-201 / §9.73 A44）。

权威依据
  - `工程控制/RELEASE-02/GAP_AUDIT.md` §9.73 裁决 A44（负责人逐字：不存在「权重模式」，
    全程都是 SNR；权重只在阶段二消费 SNR 时按该天位像素对应帧集合现场算出）；
  - `docs/ASTROCS_DESIGN.md` §2.1（总纲：全程只有 SNR，不存在「权重模式」这个概念；
    禁止任何「权重模式 / 权重档位 / 默认权重模式 / mode0·mode1·mode2」的键名、枚举、配置项或产物）；
  - `ENGINEERING_SPEC.md` §8（每项检查有正例与负例、能红能绿；fail-closed；锚存活）；
  - `工程控制/RELEASE-03/tasks/DOC-201.md`（验收门 1/2 + 「新增负例 ⇒ 检查器判红」）。

判据（确定性；R-49 改判据不改文档：限域 + 去字面化，负例面同步扩充）
  R1  ASCII 键族（**限域到「键名语境」**）：`docs/**` 与 `README.md` 中，只有
      `weight_mode` 处于**键名形态**的行才要求同行含留痕串「已按 §9.73 A44 作废」：
        ① 键值/赋值形态（`weight_mode:` / `weight_mode =`）；
        ② 反引号包裹的独立键名（反引号 + weight_mode + 反引号）；
        ③ 表格键列（行首单元格即该键名）；
        ④ 键名词紧跟其后（键/字段/家族/词表/开关/属性/标识）。
      **机器可读数据面不判**（R-49）：CSV **数据行**、schema/JSON 指针
      （`…#/$defs/weight_mode`）、路径字面量、函数**签名形参**里的标识符
      —— 它们不是"配置键被登记"，不得要求中文留痕。
      （历史/归档/冻结层允许保留原文 + 同行留痕；活文档层应当无该串。）
  R2  中文概念（**去字面化**，R-49）：同一扫描面中含「权重模式」的行，同行含
      **等价表述**之一即通过：`不存在` / `已作废` / `没有…这个概念` / `已按…作废`；
      无任何等价表述的提及仍判红。
  R5  数据面：`.csv` 的**数据行**（第 2 行起）不做中文留痕要求；**表头行照判**（列名 = 键表）。
  R1/R2 的**不对称是有意设计**（R-49 认可）：键名面求**确定**（字面统一留痕串，可 grep 可机器复核），
  散文面求**语义**（等价表述即通过；字面匹配在散文面才是误判源）——不得为「对称」统一两侧。
  R3  fail-closed：`docs/` 或 `README.md` 缺失 ⇒ rc=2（禁止把「文件不存在」当「无违规」）。
  R4  派生登记地图收窄（BLD-401 R4）：`docs/DOCUMENT_INDEX.yaml` 的**登记字段行**
      （path/status/duty/upstream/downstream/notes）与注释行是"地图"内容 —— 路径
      字面量、机器扫描出的引用表、所登记文档标题的截断副本 —— 不判为活键；地图里
      任何其它行（裸键 `weight_mode: 2`、正文、表格）照旧全量判红。

用法
  python3 eng/ci/check_no_weight_mode.py                 # 扫真实仓库，rc=0 全绿
  python3 eng/ci/check_no_weight_mode.py --json-out F    # 机器可读结果（原子写）
  python3 eng/ci/check_no_weight_mode.py --self-test     # 临时目录正例/负例，证明能红能绿
退出码：0 PASS；1 FAIL；2 输入不可用（fail-closed）。
"""
from __future__ import annotations

import argparse
import json
import os
import re
import shutil
import sys
import tempfile

ASCII_TOKEN = "weight_mode"
# R1 的 ASCII 键族判据是**裸键**（`weight_mode` / `weight_mode_version` / `weight_mode: 2` …），
# 不是连字符复合标识符。门/检查项 id 里的 `NO-WEIGHT-MODE-CODE`、`AHPX-WEIGHT-RETIRED`
# 是**门的名字**而非配置键，按裸键判据不得命中（否则"给门起名"会被当成"复活键"）。
# 收窄只去掉连字符/下划线复合标识符（`CHK-NO-WEIGHT-MODE-CODE`、
# `check_no_weight_mode.py`）；裸键及其蛇形家族（`weight_mode` / `weight_mode_version` /
# `weight_mode: 2`）仍是命中，判据不会因此放过任何活键。
ASCII_KEY_RE = re.compile(r"(?<![-_A-Za-z0-9])" + re.escape(ASCII_TOKEN) + r"(?![-_])")
ASCII_MARK = "已按 §9.73 A44 作废"
# ── R1 与 R2 的不对称是**有意设计**（R-49 原话：保持不对称；禁止「顺手统一」）──
# 两侧各按**其语境的观测方式**设计判据：
#   R1（ASCII 键族）的语境 = **键名面**（键表行 / REJECT 表行 / 句内键名）——
#     那里要的是**可 grep、可机器复核、判据确定**；一个**字面统一**的留痕串正是为此服务，
#     引入等价类会把「脚本能搜到」降级成「要靠语义判断」。
#   R2（中文概念）的语境 = **散文面**——自然语言天然多样，**字面匹配在那里才是误判源**，
#     故接受等价表述。
# 任何「统一两侧」的改动都是判据变更，须走裁决；不得为对称打开 R1 等价类或收回 R2。
# R1 键名语境（R-49 限域）：只判「键名形态」的出现。判据是**正面清单**——
# 清单之外的提及（schema/JSON 指针、路径字面量、函数形参、散文里的普通提及）
# 不再是本门的对象；这比"任意提及都判红"更精确，不是更宽松：
# 任何**活键**（配置键 / 键表列 / 注册表键名）仍会被下列形态命中。
KEY_CTX_RES = (
    re.compile(r"(?<![-_A-Za-z0-9])" + re.escape(ASCII_TOKEN) + r"(?![-_])\s*[:=](?!=)"),
    re.compile(r"`\s*" + re.escape(ASCII_TOKEN) + r"\s*`"),
    re.compile(r"^\s*\|\s*`?" + re.escape(ASCII_TOKEN) + r"`?\s*(?:\||[（(])"),
    re.compile(r"(?<![-_A-Za-z0-9])" + re.escape(ASCII_TOKEN)
               + r"(?![-_])\s*`?\s*(?:键|字段|家族|词表|开关|属性|标识)"),
)
ZH_TOKEN = "权重模式"
# R2 等价表述（R-49 去字面化）：语义等价即通过；无等价表述仍判红（见 --self-test）。
ZH_EQUIV_RES = (
    re.compile(r"不存在"),
    re.compile(r"已作废"),
    re.compile(r"没有[^。\n]{0,16}概念"),
    re.compile(r"已按[^。\n]{0,24}作废"),
)
# R5 机器可读数据面：CSV 数据行（第 2 行起）不判；表头行照判。
DATA_ROW_SKIP_EXT = {".csv"}
SCAN_DIR = "docs"
SCAN_FILE = "README.md"
SKIP_DIRS = {".git", "build", "run", "__pycache__", ".venv", "node_modules"}
TEXT_EXT = {".md", ".json", ".yaml", ".yml", ".csv", ".txt", ".py", ".rst"}

# ── 派生登记地图（不是活文档）──────────────────────────────────────────────
# docs/DOCUMENT_INDEX.yaml 是全文档集的「地图」（docs/ASTROCS_DESIGN.md §0.2：全文档集
# 唯一索引地图，由 eng/tools/doccheck/check_doc_index.py 校验）。它的字段内容是
# **登记与派生**面：path = 仓库路径字面量；downstream = 机器扫描出的引用方路径表；
# duty = 所登记文档标题的截断副本。它们不是在引入 weight_mode 作为活键/枚举/
# 配置项/产物。
# 判定依据（BLD-401 R4 实测的 4 处命中形态）：
#   ① README.md 条目的 downstream 计数串里出现 eng/ci/check_no_weight_mode.py
#      —— **门自己的文件名**自指误报；
#   ② 被登记文档条目的 downstream 里出现**其它文档路径**（路径字面量，不是活键）；
#   ③ 该路径的 path: 条目本身；
#   ④ duty: 字段是所登记文档标题的 100 字截断副本（截断会把源文档 H1 同行的
#      「已按 §9.73 A44 作废：该概念不存在」切掉）。
# 不遮蔽（关键）：地图 path 登记的文档本体仍在 docs/** 扫描面内被逐行直扫，且其
#   duty/H1 自带 A44 留痕；故排除地图的登记字段行不会放过任何活文档里的裸键。
# 收窄（只准更精确、不准更宽松）：只跳过地图自身的**登记字段行与注释行**；地图里
#   任何其它行（裸键 weight_mode: 2、正文、表格）照旧全量判红（负例见 --self-test）。
# 锚存活：DERIVED_MAP_REL 不存在时豁免集自然为空 ⇒ 判据只会更严，不会更松。
DERIVED_MAP_REL = "docs/DOCUMENT_INDEX.yaml"
DERIVED_MAP_FIELD_RE = re.compile(
    r"^\s*(?:#|-\s*path\s*:|(?:path|status|duty|upstream|downstream|notes)\s*:)")


def iter_scan_files(root):
    """扫描面 = docs/**（跳过重型目录）+ 仓库根 README.md。确定性顺序。"""
    files = []
    docs = os.path.join(root, SCAN_DIR)
    if not os.path.isdir(docs):
        return None
    for dirpath, dirnames, filenames in os.walk(docs):
        dirnames[:] = sorted(d for d in dirnames if d not in SKIP_DIRS)
        for name in sorted(filenames):
            if os.path.splitext(name)[1].lower() in TEXT_EXT:
                files.append(os.path.join(dirpath, name))
    readme = os.path.join(root, SCAN_FILE)
    if not os.path.isfile(readme):
        return None
    files.append(readme)
    return files


def scan(root):
    """返回 (findings, scanned_files, scanned_lines)。findings 为稳定排序的 dict 列表。"""
    files = iter_scan_files(root)
    if files is None:
        return None, 0, 0
    findings, lines_total = [], 0
    for path in files:
        try:
            with open(path, encoding="utf-8", errors="replace") as fh:
                lines = fh.read().split("\n")
        except OSError:
            return None, 0, 0
        rel = os.path.relpath(path, root).replace(os.sep, "/")
        is_derived_map = (rel == DERIVED_MAP_REL)
        ext = os.path.splitext(path)[1].lower()
        skip_data_rows = ext in DATA_ROW_SKIP_EXT
        for idx, line in enumerate(lines, start=1):
            lines_total += 1
            if is_derived_map and DERIVED_MAP_FIELD_RE.match(line):
                continue  # R4：地图的登记字段行/注释行（路径字面量与标题截断副本）
            if skip_data_rows and idx > 1:
                continue  # R5：CSV 数据行 = 机器可读数据面（表头行照判）
            # 表头行（列名 = 键表）：裸键即判；其余按 R1 键名语境判（R-49 限域）
            if skip_data_rows:
                ascii_key_ctx = ASCII_KEY_RE.search(line) is not None
            else:
                ascii_key_ctx = any(r.search(line) for r in KEY_CTX_RES)
            if ascii_key_ctx and ASCII_MARK not in line:
                findings.append({"rule": "R1-ASCII-KEY", "file": rel, "line": idx,
                                 "observed": line.strip()[:160],
                                 "expected": "键名语境同行含「%s」留痕（§9.73 A44；R-49 限域）" % ASCII_MARK})
            if ZH_TOKEN in line and not any(r.search(line) for r in ZH_EQUIV_RES):
                findings.append({"rule": "R2-ZH-CONCEPT", "file": rel, "line": idx,
                                 "observed": line.strip()[:160],
                                 "expected": "同行含等价表述（不存在／没有…这个概念／已作废／已按…作废；R-49 去字面化）"})
    findings.sort(key=lambda f: (f["file"], f["line"], f["rule"]))
    return findings, len(files), lines_total


def run(root, json_out=None):
    findings, nfiles, nlines = scan(root)
    if findings is None:
        print("CHK-NO-WEIGHT-MODE_FAIL: 扫描面不可用（缺 %s/ 或 %s）—— fail-closed"
              % (SCAN_DIR, SCAN_FILE), file=sys.stderr)
        return 2
    result = {
        "tool": "check_no_weight_mode",
        "authority": ["GAP_AUDIT.md §9.73 A44", "docs/ASTROCS_DESIGN.md §2.1",
                      "工程控制/RELEASE-03/tasks/DOC-201.md",
                      "R-49（改判据不改文档：R1 限域到键名语境 / R2 去字面化 / 保留负例）"],
        "root": os.path.abspath(root),
        "files_scanned": nfiles,
        "lines_scanned": nlines,
        "findings": findings,
        "passed": not findings,
    }
    if json_out:
        os.makedirs(os.path.dirname(os.path.abspath(json_out)) or ".", exist_ok=True)
        tmp = json_out + ".tmp"
        with open(tmp, "w", encoding="utf-8") as fh:
            json.dump(result, fh, ensure_ascii=False, indent=2, sort_keys=True)
            fh.write("\n")
        os.replace(tmp, json_out)
    if findings:
        print("CHK-NO-WEIGHT-MODE_FAIL: %d 处无留痕命中（§9.73 A44）" % len(findings))
        for f in findings[:20]:
            print("  [%s] %s:%d  %s" % (f["rule"], f["file"], f["line"], f["observed"]))
        if len(findings) > 20:
            print("  ... 其余 %d 处见 --json-out" % (len(findings) - 20))
        return 1
    print("CHK-NO-WEIGHT-MODE_PASS: files=%d lines=%d 无未留痕命中（§9.73 A44）"
          % (nfiles, nlines))
    return 0


def _write(path, text):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", encoding="utf-8") as fh:
        fh.write(text)


def self_test():
    """临时目录正例/负例：证明本检查器能红能绿（ENGINEERING_SPEC §8）。"""
    tmp = tempfile.mkdtemp(prefix="chk-no-weight-mode-")
    cases, problems = [], []
    try:
        # 正例（绿）：冻结层留痕 + 活文档层无该串
        green = os.path.join(tmp, "green")
        _write(os.path.join(green, "docs", "frozen", "a.md"),
               "# 历史件\nweight_mode = 2（已按 §9.73 A44 作废：该概念不存在）\n")
        _write(os.path.join(green, "docs", "live", "b.md"),
               "# 活文档\n权重 = 阶段二按像素集合现场算的派生量\n")
        _write(os.path.join(green, "README.md"), "# 仓库\n")
        rc = run(green)
        cases.append(("green_clean", rc, 0))
        # 负例（红）：一处裸 weight_mode
        red = os.path.join(tmp, "red")
        _write(os.path.join(red, "docs", "live", "b.md"),
               "# 活文档\n| weight_mode | 2 |\n")
        _write(os.path.join(red, "README.md"), "# 仓库\n")
        rc = run(red)
        cases.append(("red_bare_weight_mode", rc, 1))
        # 负例（红）：一处裸「权重模式」
        red2 = os.path.join(tmp, "red2")
        _write(os.path.join(red2, "docs", "live", "c.md"),
               "# 活文档\nPhase2 生产权重模式 = point_information\n")
        _write(os.path.join(red2, "README.md"), "# 仓库\n")
        rc = run(red2)
        cases.append(("red_bare_zh_concept", rc, 1))
        # 负例（红）：留痕串被删（仅剩「已作废」泛词不含 §9.73 A44）
        red3 = os.path.join(tmp, "red3")
        _write(os.path.join(red3, "docs", "frozen", "d.md"),
               "# 历史件\nweight_mode = 2（已作废）\n")
        _write(os.path.join(red3, "README.md"), "# 仓库\n")
        rc = run(red3)
        cases.append(("red_marker_without_a44", rc, 1))
        # 正例（绿）：派生登记地图 docs/DOCUMENT_INDEX.yaml 的 4 处实测误报形态
        # （门自己的文件名 / 被登记文档路径 / path 条目 / duty 截断副本）
        green2 = os.path.join(tmp, "green-map")
        _write(os.path.join(green2, "docs", "DOCUMENT_INDEX.yaml"),
               "# ACSD 文档机器索引（DOCUMENT_INDEX）——全文档集唯一索引地图\n"
               "doc_index:\n"
               "  active:\n"
               "    - path: \"README.md\"\n"
               "      status: ACTIVE_INFORMATIVE\n"
               "      duty: \"仓库入口说明\"\n"
               "      downstream: \"eng/ci/check_no_weight_mode.py、eng/ci/check_version.py 等 161 处\"\n"
               "    - path: \"docs/science/DATA_SEMANTICS.md\"\n"
               "      status: ACTIVE_NORMATIVE\n"
               "      duty: \"DATA-UNC-001 — 跨 Phase 数据合同（weight_mode / effective PSF / provenanc\"\n"
               "      downstream: \"docs/DOCUMENT_INDEX.yaml、docs/engineering/PUBLIC_API.md\"\n"
               "    - path: \"docs/engineering/PUBLIC_API.md\"\n"
               "      status: ACTIVE_NORMATIVE\n"
               "      duty: \"公共 API 消费面（weight_mode 已按 §9.73 A44 作废：该概念不存在）\"\n")
        _write(os.path.join(green2, "README.md"), "# 仓库\n")
        rc = run(green2)
        cases.append(("green_derived_index_map_registration", rc, 0))
        # 负例（红）：同一地图里出现**裸活键** ⇒ 地图豁免不得变成免检区
        red4 = os.path.join(tmp, "red-map-bare-key")
        _write(os.path.join(red4, "docs", "DOCUMENT_INDEX.yaml"),
               "doc_index:\n  active:\n    - path: \"README.md\"\n"
               "      weight_mode: 2\n")
        _write(os.path.join(red4, "README.md"), "# 仓库\n")
        rc = run(red4)
        cases.append(("red_map_bare_live_key", rc, 1))
        # 负例（红）：同一地图里出现含该词的**正文** ⇒ 照旧判红
        red5 = os.path.join(tmp, "red-map-prose")
        _write(os.path.join(red5, "docs", "DOCUMENT_INDEX.yaml"),
               "doc_index:\n  active:\n    - path: \"README.md\"\n"
               "      说明：Phase2 生产 weight_mode = point_information\n")
        _write(os.path.join(red5, "README.md"), "# 仓库\n")
        rc = run(red5)
        cases.append(("red_map_prose_mention", rc, 1))
        # 正例（绿）：门/检查项 id 与门自身文件名里的连字符/下划线复合标识符不是裸键
        # （"给门起名"不得被当成"复活键"）；同一行的裸键仍照旧判红（见负例 red_bare_*）。
        green3 = os.path.join(tmp, "green-compound-id")
        _write(os.path.join(green3, "docs", "ci", "checks.md"),
               "| CHK-NO-WEIGHT-MODE-CODE | 代码面单一权重口径 | "
               "`python3 eng/ci/check_no_weight_mode_code.py` | P0 |\n")
        _write(os.path.join(green3, "README.md"), "# 仓库\n")
        rc = run(green3)
        cases.append(("green_compound_check_id", rc, 0))
        # ── R-49 新增正例/负例：限域（数据面/形参）与去字面化都必须能红能绿 ──
        # 正例（绿）：CSV **数据行**里的标识符 = 机器可读数据面，不得要求中文留痕
        green4 = os.path.join(tmp, "green-csv-data-row")
        _write(os.path.join(green4, "docs", "contracts", "api.csv"),
               "id,signature,notes\n"
               "API-x,f,\"weight_mode/weight_data/grid_w 不进 C ABI\"\n")
        _write(os.path.join(green4, "README.md"), "# 仓库\n")
        cases.append(("green_csv_data_row", run(green4), 0))
        # 负例（红）：CSV **表头**列名 = 键表 ⇒ 裸键判红（数据面限域不得变成免检区）
        red6 = os.path.join(tmp, "red-csv-header")
        _write(os.path.join(red6, "docs", "contracts", "api.csv"),
               "id,weight_mode,notes\nAPI-x,2,—\n")
        _write(os.path.join(red6, "README.md"), "# 仓库\n")
        cases.append(("red_csv_header_key", run(red6), 1))
        # 正例（绿）：函数**签名形参**里的同名标识符（形参名 ≠ 配置键被登记）
        green5 = os.path.join(tmp, "green-signature-param")
        _write(os.path.join(green5, "docs", "contracts", "api.md"),
               "# 接口\n```c\nint aio_w(const float *snr, float weight_mode, int n);\n```\n")
        _write(os.path.join(green5, "README.md"), "# 仓库\n")
        cases.append(("green_signature_param", run(green5), 0))
        # 正例（绿）：schema / JSON 指针（`…#/$defs/<key>`）不是活键
        green6 = os.path.join(tmp, "green-schema-pointer")
        _write(os.path.join(green6, "docs", "contracts", "c.md"),
               "# 合同\n属性见 `eng/contracts/schemas/x.schema.json#/$defs/weight_mode` 的词表登记。\n")
        _write(os.path.join(green6, "README.md"), "# 仓库\n")
        cases.append(("green_schema_pointer", run(green6), 0))
        # 负例（红）：散文里的**活键赋值**（限域后仍必须判红）
        red7 = os.path.join(tmp, "red-prose-assign")
        _write(os.path.join(red7, "docs", "live", "e.md"),
               "# 活文档\n生产读 `weight_mode = point_information` 作为归约口径。\n")
        _write(os.path.join(red7, "README.md"), "# 仓库\n")
        cases.append(("red_prose_key_assign", run(red7), 1))
        # 正例（绿）：R2 等价表述之一「没有…这个概念」
        green7 = os.path.join(tmp, "green-zh-equiv")
        _write(os.path.join(green7, "docs", "live", "f.md"),
               "# 活文档\n全程只有 SNR，没有「权重模式」这个概念。\n")
        _write(os.path.join(green7, "README.md"), "# 仓库\n")
        cases.append(("green_zh_equiv_phrase", run(green7), 0))
        # 负例（红）：R2 去字面化**不得变成恒绿** —— 无等价表述的提及仍判红
        red8 = os.path.join(tmp, "red-zh-no-equiv")
        _write(os.path.join(red8, "docs", "live", "g.md"),
               "# 活文档\n权重模式的作用是选择归约口径。\n")
        _write(os.path.join(red8, "README.md"), "# 仓库\n")
        cases.append(("red_zh_mention_without_equiv", run(red8), 1))
        # fail-closed：缺 docs/
        bad = os.path.join(tmp, "failclosed")
        _write(os.path.join(bad, "README.md"), "# 仓库\n")
        rc = run(bad)
        cases.append(("failclosed_missing_docs", rc, 2))
    finally:
        shutil.rmtree(tmp, ignore_errors=True)
    for name, got, want in cases:
        ok = got == want
        print("SELFTEST_%s %s (rc=%d want=%d)" % ("PASS" if ok else "FAIL", name, got, want))
        if not ok:
            problems.append(name)
    if problems:
        print("SELF_TEST FAIL cases=%d problems=%s" % (len(cases), problems))
        return 1
    print("SELF_TEST PASS cases=%d" % len(cases))
    return 0


def main(argv=None):
    ap = argparse.ArgumentParser(description="§9.73 A44「权重模式」作废门（DOC-201）")
    ap.add_argument("--root", default=".", help="仓库根（默认 .）")
    ap.add_argument("--json-out", default=None, help="机器可读结果输出路径（原子写）")
    ap.add_argument("--self-test", action="store_true", help="临时目录正例/负例自检")
    args = ap.parse_args(argv)
    if args.self_test:
        return self_test()
    return run(args.root, args.json_out)


if __name__ == "__main__":
    sys.exit(main())
