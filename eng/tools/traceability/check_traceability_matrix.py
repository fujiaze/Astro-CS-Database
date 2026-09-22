#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""DOC-001 机器追溯合同检查器：docs/traceability/TRACEABILITY_MATRIX.json
（八层：SCI→ALG→DATA/API/ARCH→MOD/SRC→TEST→EVIDENCE）全量机器校验。

规则（详见 docs/traceability/TRACEABILITY_SPEC.md §7，exit 0 = PASS）：
  C1 MISSING_FILE     矩阵 JSON 存在且被 Git 跟踪；LAYERS.csv、schema 存在；
  C2 SCHEMA_VIOLATION 每行必需列齐全、层状态取值合法、无附加字段；
  C3 EMPTY_CELL_VIOLATION 任何单元格为 ""/纯空白/'-'/'?'/'TBD'/'TODO' → FAIL
                         （空缺必须显式 MISSING / NONE，禁止空字符串通过）；
  C4 ID_FORMAT_VIOLATION 各层 ID 匹配本层正则（占位 *-MISSING 亦须匹配）；
  C5 DUPLICATE_ID     module_id 行键唯一；SRC id、EVID id 全矩阵唯一；
  C6 CHAIN_BREAK      (a) SRC VERIFIED→同一行 TEST 不得 MISSING；(b) TEST VERIFIED
                      而行 SRC 非 VERIFIED；(c) TEST VERIFIED 而行 MOD 行缺失（不存在）；
                      (d) SCI VERIFIED→ALG 不得 MISSING（conformance/service/provider
                      显式豁免见 SPEC §4 例外，行 notes 声明即可）；
  C7 DANGLING_REF     VERIFIED 层锚可解析：src_path/test_path 文件存在且被 Git 跟踪；
                      src_path 的 SOURCE SYMBOL path::symbol 中 symbol 在文件内可见
                      （MISSING 占位 path::MISSING 只校验路径）；
  C8 REF_OUT_OF_SCOPE WARN（--strict 为 ERROR）：VERIFIED 的 sci/alg/data/api/arch/
                      test/evidence ID 若既不是占位也不是已建 authority（见
                      AUTHORITY_DIRS），登记缺 authority 文件；不崩溃。

Git 面与 fail-closed（GITDECOUPLE-02；规范依据 docs/ci/01_CHECKS.md §1、
ENGINEERING_SPEC.md §10、docs/traceability/TRACEABILITY_SPEC.md §7:123/:151）：
  * 规则 C1/C7 逐字要求「文件存在**且受 Git 跟踪**」——"被跟踪" 是判据的一部分，
    不是纯枚举源，因此**不**把 git 去掉（去掉等于放宽判据）；
  * 但 git 不可用（无 git 可执行文件 / 非 git 工作树）时，原实现把
    `git ls-files --error-unmatch` 的非零返回一律当 "未跟踪" ⇒ 真因被报成
    MISSING_FILE / DANGLING_REF（实测：无 .git 镜像树里 28 条误导性错误），
    且 git 缺失时抛 FileNotFoundError traceback（§1「不得 traceback」）；
  * 现行为：启动时探测 git 面一次；不可用 ⇒ **显式降级 + 留痕**（打印
    `GIT_UNAVAILABLE: <原因>`，JSON 记 git_face）——"被跟踪" 子判据整面跳过，
    存在性等其余判据照常执行 —— 并 **fail-closed rc=2**（依赖不可用 ⇒ 不给结论）。
  * CMakeLists 枚举面（ctest 目标注册判据的输入）改确定性 os.walk + 剪枝，
    不依赖 git；枚举为空 ⇒ 判红点名 CMAKE_FACE_EMPTY（§1 scanned==0 ⇒ rc!=0）。
退出码：0 PASS / 1 FAIL（判据命中）/ 2 依赖不可用（GIT_UNAVAILABLE，fail-closed）/
        3 TOOLING_FAILURE（未捕获异常，不允许伪 PASS）。
任何未捕获异常 → 打印 TOOLING_FAILURE 并 exit 3（不允许伪 PASS）。
用法：
  python3 eng/tools/traceability/check_traceability_matrix.py [--root <repo>]
      [--json-out <file>] [--strict]
依赖：Python 3.10+ 标准库；无网络；不依赖 cwd 之外路径。
"""
from __future__ import annotations

import argparse
import csv
import hashlib
import json
import os
import re
import subprocess
import sys

MATRIX_REL = "docs/traceability/TRACEABILITY_MATRIX.json"
MATRIX_CSV_REL = "docs/traceability/TRACEABILITY_MATRIX.csv"
LAYERS_REL = "docs/traceability/TRACEABILITY_LAYERS.csv"
SCHEMA_REL = "eng/contracts/schemas/traceability_matrix.schema.json"
CSV_COLS = ["module_id", "module_kind", "module_anchor",
            "science_id", "science_doc", "science_status",
            "algorithm_id", "algorithm_doc", "algorithm_status",
            "data_id", "data_status",
            "api_id", "api_status",
            "arch_id", "arch_status",
            "src_id", "src_path", "src_status",
            "test_id", "test_path", "test_status",
            "evidence_id", "evidence_status", "notes"]

STATUS_OK = {"VERIFIED", "MISSING", "NONE"}
# 每层: (id_key, status_key, path_key?, id_regex, path_required_when_verified)
LAYERS = [
    ("SCI",  "science_id",  "science_status",  "science_doc",   r"^SCI-[A-Z0-9]+(-[A-Z0-9]+)*$"),
    ("ALG",  "algorithm_id", "algorithm_status", "algorithm_doc", r"^ALG-[A-Z0-9]+(-[A-Z0-9]+)*$"),
    ("DATA", "data_id",     "data_status",     None,            r"^DATA-[A-Z0-9]+(-[A-Z0-9]+)*$"),
    ("API",  "api_id",      "api_status",      None,            r"^API-[A-Z0-9]+(-[A-Z0-9]+)*$"),
    ("ARCH", "arch_id",     "arch_status",     None,            r"^ARCH-[A-Z0-9]+(-[A-Z0-9]+)*$"),
    ("MOD",  "module_id",   None,              None,            r"^MOD-[A-Za-z0-9]+(-[A-Za-z0-9]+)*$"),
    ("SRC",  "src_id",      "src_status",      "src_path",      r"^SRC-[A-Z0-9]+(-[A-Z0-9]+)*$"),
    ("TEST", "test_id",     "test_status",     "test_path",     r"^TEST-[A-Z0-9]+(-[A-Z0-9]+)*$"),
    ("EVID", "evidence_id", "evidence_status", None,            r"^EVID-[A-Z0-9]+(-[A-Z0-9]+)*$"),
]
REQUIRED_KEYS = [k for _, idk, stk, pathk, _ in LAYERS
                 for k in ((idk,) if idk else ()) + ((stk,) if stk else ()) + ((pathk,) if pathk else ())]
REQUIRED_KEYS += ["module_id", "module_kind", "module_anchor", "notes"]
# 豁免层（conformance/service/provider 允许 SCI/ALG MISSING 而 DATA/API/ARCH VERIFIED）
EXEMPT_KINDS = {"conformance", "service", "provider"}
# 合法模块来源目录：矩阵模块清单须能在其中发现（防孤行/防遗漏）
MODULE_ANCHOR_DIRS = [
    "eng/tests/conformance", "lib/infrastructure/aio/io", "lib/infrastructure/benchmark/cpu",
    "docs/modules/registry",  # registry production 模块（module_adapters.cpp 唯一源）
]
AUTHORITY_DIRS = {  # 各合同层 authority 文档搜索目录（id 需在其中一个文件文本中出现）
    "SCI": ["docs/science"],
    "ALG": ["docs/algorithms", "docs/science"],
    "DATA": ["docs/contracts", "docs/interfaces/data", "docs/api", "lib/core/src"],
    "API": ["docs/api", "docs/contracts", "docs/interfaces/io", "docs/architecture/cpu",
            "docs/modules/registry", "lib/core/src", "eng/tests/conformance",
            "lib/infrastructure/benchmark/cpu"],
    "ARCH": ["docs/architecture", "docs/architecture/cpu", "docs/contracts", "docs/api"],
    "MOD": ["eng/tests/conformance", "lib/infrastructure", "docs/modules/registry"],
    "SRC": ["lib", "include", "tests"],
    "TEST": ["tests", "lib", "docs/interfaces", "docs/contracts", "docs/modules/registry"],
    "EVID": ["evidence", "reports", "returns"],
}
EMPTY_BAD = {"", "-", "?", "TBD", "TODO", "N/A", "NA", "n/a"}
PLACEHOLDER_RE = re.compile(r"^[A-Z]+-MISSING$")


def err(prefix: str, msg: str, subcode: str = None, sig: str = None) -> dict:
    """错误记录。

    `code` 是**稳定的判定类别**（对外合同，测试与 §7 规则表按它断言）；
    `subcode` 是 W4-A3 引入的**细分粒度**（同一类别下的具体失效型），
    用于把"路径不存在"与"用例/目标不存在"分开 —— 两者同属悬空引用（DANGLING_REF）
    这一类别，但根因与处置完全不同（前者改锚，后者补用例/改目标名）。
    """
    rec = {"severity": "ERROR", "code": prefix, "detail": msg}
    if subcode:
        rec["subcode"] = subcode
    if sig:
        rec["sig"] = sig
    return rec


def warn(prefix: str, msg: str, subcode: str = None, sig: str = None) -> dict:
    """警告记录。subcode 口径与 err() 一致（W4-A3 引入，--strict 升级为 ERROR）。"""
    rec = {"severity": "WARN", "code": prefix, "detail": msg}
    if subcode:
        rec["subcode"] = subcode
    if sig:
        rec["sig"] = sig
    return rec


class GitUnavailable(Exception):
    """git 不可用 / 非 git 工作树 —— C1/C7 的「被 Git 跟踪」子判据无法执行。

    规范依据：docs/traceability/TRACEABILITY_SPEC.md §7:123/:151 把「受 Git 跟踪」
    列为判据的一部分，故 git 面不可用时**不得**把它静默当成「未跟踪」（那会把
    真因报成 MISSING_FILE/DANGLING_REF），也不得 traceback（docs/ci/01_CHECKS.md §1）。
    """


class AnchorStale(Exception):
    """锚失效 / 枚举面为空 —— fail-closed（§1 锚存活 + scanned==0 ⇒ rc!=0）。"""


# git 面探测缓存（结论与 rel 无关，只探一次）
_GIT_FACE: dict = {}
# 「被 Git 跟踪」子判据的可用性（main 启动时置位；不可用 ⇒ 整面跳过 + 留痕 + rc=2）
_TRACKING: dict = {"probed": False, "available": True, "reason": "git 工作树"}


def probe_tracking(root: str) -> tuple[bool, str]:
    """探测 (git 面可用?, 原因)。缓存；git 可执行文件缺失也在此被显式吸收（不 traceback）。"""
    key = os.path.abspath(root)
    if key in _GIT_FACE:
        return _GIT_FACE[key]
    try:
        p = subprocess.run(["git", "-C", key, "rev-parse", "--is-inside-work-tree"],
                           capture_output=True, text=True, timeout=60)
    except (OSError, subprocess.SubprocessError) as exc:
        res = (False, f"git 调用失败（{exc.__class__.__name__}: {exc}）")
    else:
        if p.returncode != 0:
            last = (p.stderr.strip().splitlines() or [""])[-1]
            res = (False, f"非 git 工作树（git rev-parse rc={p.returncode}: {last[:140]}）")
        elif p.stdout.strip() != "true":
            res = (False, f"git rev-parse 输出非 true: {p.stdout.strip()[:60]!r}")
        else:
            res = (True, "git 工作树")
    _GIT_FACE[key] = res
    return res


def tracking_available() -> bool:
    return bool(_TRACKING["available"])


def git_tracked(root: str, rel: str) -> bool:
    """rel 是否被 git 跟踪。git 面不可用 ⇒ GitUnavailable（调用方显式降级 + 留痕）。"""
    ok, why = probe_tracking(root)
    if not ok:
        raise GitUnavailable(why)
    try:
        r = subprocess.run(["git", "ls-files", "--error-unmatch", "--", rel],
                           cwd=root, capture_output=True, text=True, timeout=60)
    except (OSError, subprocess.SubprocessError) as exc:
        raise GitUnavailable(f"git ls-files 调用失败（{exc.__class__.__name__}: {exc}）") from None
    if r.returncode == 0:
        return True
    if r.returncode == 1:
        return False  # git 的正常否定回答：确实未跟踪
    last = (r.stderr.strip().splitlines() or [""])[-1]
    raise GitUnavailable(f"git ls-files --error-unmatch rc={r.returncode}: {last[:140]}")


def check_file(root: str, rel: str, results: list[dict]) -> None:
    full = os.path.join(root, rel)
    if not os.path.isfile(full):
        results.append(err("MISSING_FILE", f"{rel} 不存在"))
        return
    if not tracking_available():
        return  # 显式降级：由 main 统一打印 GIT_UNAVAILABLE 留痕 + rc=2，不在这里刷误导性错误
    if not git_tracked(root, rel):
        results.append(err("MISSING_FILE", f"{rel} 存在但未受 Git 跟踪（git ls-files --error-unmatch 失败）"))


def parse_matrix(root: str, results: list[dict]) -> dict | None:
    full = os.path.join(root, MATRIX_REL)
    try:
        with open(full, encoding="utf-8") as f:
            data = json.load(f)
    except Exception as exc:  # noqa: BLE001
        results.append(err("TOOLING_FAILURE", f"{MATRIX_REL} JSON 解析失败: {exc}"))
        return None
    if not isinstance(data, dict) or data.get("schema") != "astrocs.traceability-matrix/v1":
        results.append(err("SCHEMA_VIOLATION", f"{MATRIX_REL} schema 声明缺失或错误"))
        return None
    mods = data.get("modules")
    if not isinstance(mods, list):
        results.append(err("SCHEMA_VIOLATION", f"{MATRIX_REL} modules 必须是数组"))
        return None
    return data


def is_empty(v) -> bool:
    if v is None:
        return True
    s = str(v).strip()
    return s == "" or s in EMPTY_BAD


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--root", default=".")
    ap.add_argument("--json-out", default=None)
    ap.add_argument("--strict", action="store_true")
    ap.add_argument("--ignore-baseline", action="store_true", dest="ignore_baseline",
                    help="负例注入面：绕过烧毁式 WARN 基线（存量 WARN 必须判红）")
    args = ap.parse_args()
    root = os.path.abspath(args.root)
    results: list[dict] = []

    # git 面探测（一次）—— 显式降级 + 留痕（§1：不得 traceback、不得静默降级）
    ok, why = probe_tracking(root)
    _TRACKING.update(probed=True, available=ok, reason=why)
    if not ok:
        print(f"GIT_UNAVAILABLE: {why}\n  ⇒ C1/C7 的「受 Git 跟踪」子判据整面跳过（存在性等"
              f"其余判据照常执行）；依赖不可用 ⇒ fail-closed rc=2，不给出 PASS 结论",
              file=sys.stderr)

    # C1 文件存在 + 被跟踪
    check_file(root, MATRIX_REL, results)
    check_file(root, MATRIX_CSV_REL, results)
    check_file(root, LAYERS_REL, results)
    check_file(root, SCHEMA_REL, results)

    data = parse_matrix(root, results)
    if data is None:
        n_errors, _n_warns = _finish(root, args, results)
        return _exit_code(n_errors)

    _check_csv_parity(root, results)

    mods: list[dict] = data["modules"]
    rows = []
    # CSV 行（供 C2/C3 行级定位与视图）
    seen_module = {}
    for i, row in enumerate(mods):
        if not isinstance(row, dict):
            results.append(err("SCHEMA_VIOLATION", f"modules[{i}] 非对象"))
            continue
        mid = row.get("module_id")
        if not isinstance(mid, str) or not mid:
            results.append(err("SCHEMA_VIOLATION", f"modules[{i}] module_id 缺失/非法: {mid!r}"))
            continue
        if mid in seen_module:
            results.append(err("DUPLICATE_ID", f"模块行键重复: {mid} (modules[{seen_module[mid]}] 与 modules[{i}])"))
        seen_module[mid] = i
        rows.append(row)

    # C2 必需列 + C3 空值 + C4 ID 格式 + 状态取值
    for row in rows:
        mid = row.get("module_id", "?")
        for k in REQUIRED_KEYS:
            if k not in row:
                results.append(err("SCHEMA_VIOLATION", f"{mid}: 缺必需列 {k}"))
        for idk, stk, pathk, regex in [
            (idk, stk, pathk, rx) for _, idk, stk, pathk, rx in LAYERS
        ]:
            if idk not in row or stk not in row:
                continue  # 已报缺列
            ident, st = row[idk], row[stk]
            # C3 空值
            for label, v in ((idk, ident), (stk, st)) + (((pathk, row[pathk]),) if pathk else ()):
                if is_empty(v):
                    results.append(err("EMPTY_CELL_VIOLATION",
                                       f"{mid} [{idk} 层] 列 {label} 为空值 {v!r} → 必须显式 MISSING/NONE"))
            if st not in STATUS_OK:
                results.append(err("SCHEMA_VIOLATION",
                                   f"{mid} [{idk} 层] 非法状态 {st!r} ∈ {{VERIFIED,MISSING,NONE}}"))
            # C4 ID 格式
            # 复合形态（W4-A3）：`ID-A;ID-B` = 同一层由多个已登记 ID 共同锚定
            # （与 test_path 的复合形态同源）。**逐个**切片校验，任一元素不匹配即判红；
            # 空元素由 C3 抓。这是"支持复合形态"，不是放宽单 ID 正则。
            id_parts = [x.strip() for x in str(ident).split(";")] if ";" in str(ident) else [str(ident)]
            if not all(
                    PLACEHOLDER_RE.fullmatch(x) or re.fullmatch(regex, x)
                    for x in id_parts) or not all(id_parts):
                results.append(err("ID_FORMAT_VIOLATION",
                                   f"{mid} [{idk} 层] ID {ident!r} 不匹配 {regex}"))
            if pathk and is_empty(row.get(pathk)):
                results.append(err("EMPTY_CELL_VIOLATION",
                                   f"{mid} [{idk} 层] 列 {pathk} 为空 → 必须显式 MISSING"))
        if row.get("module_kind") not in {"phase1", "phase2", "phase3", "service", "conformance", "provider"}:
            results.append(err("SCHEMA_VIOLATION", f"{mid}: module_kind 非法 {row.get('module_kind')!r}"))
        if is_empty(row.get("module_anchor")):
            results.append(err("EMPTY_CELL_VIOLATION", f"{mid}: module_anchor 为空"))

    # C5 唯一性（行键已在上面；SRC/EVID id 唯一）
    for idk, label in (("src_id", "SRC"), ("evidence_id", "EVIDENCE")):
        seen = {}
        for row in rows:
            v = row.get(idk)
            if not v or str(v).endswith("-MISSING"):
                continue
            if v in seen:
                results.append(err("DUPLICATE_ID", f"{label} id 重复: {v} ({seen[v]} 与 {row.get('module_id')})"))
            seen[v] = row.get("module_id")

    # C6 链完整（非豁免行）
    for row in rows:
        mid = row.get("module_id", "?")
        kind = row.get("module_kind")
        sci_st, alg_st = row.get("science_status"), row.get("algorithm_status")
        src_st, test_st = row.get("src_status"), row.get("test_status")
        # (d) SCI VERIFIED → ALG 不得 MISSING
        if sci_st == "VERIFIED" and alg_st == "MISSING" and kind not in EXEMPT_KINDS:
            results.append(err("CHAIN_BREAK",
                               f"{mid}: SCI VERIFIED 但 ALG MISSING（下层依赖上层缺口）"))
        # (a) SRC VERIFIED → TEST 不得 MISSING
        if src_st == "VERIFIED" and test_st == "MISSING":
            results.append(err("CHAIN_BREAK", f"{mid}: SRC VERIFIED 但 TEST MISSING（有实现无测试）"))
        # (b) TEST VERIFIED → SRC 必须 VERIFIED
        if test_st == "VERIFIED" and src_st != "VERIFIED":
            results.append(err("CHAIN_BREAK", f"{mid}: TEST VERIFIED 但 SRC 非 VERIFIED（无实现却有测试证据）"))

    # C7 输入面：ctest 目标注册判据的 CMakeLists 枚举面（确定性枚举，不依赖 git）。
    # 空面 ⇒ 判红点名（§1 scanned==0 ⇒ rc!=0），且不在 _check_anchor 里刷 N 条同因错误。
    try:
        cmakelists_face(root)
    except AnchorStale as exc:
        results.append(err("DANGLING_REF",
                           f"ctest 目标注册判据的枚举面不可用: {exc}", "CMAKE_FACE_EMPTY"))

    # C7 DANGLING_REF：VERIFIED 层锚可解析
    src_seen = {}
    for row in rows:
        mid = row.get("module_id", "?")
        # SRC
        if row.get("src_status") == "VERIFIED":
            p = row.get("src_path", "")
            _check_anchor(root, mid, "SRC", p, results, src_seen)
        # TEST
        if row.get("test_status") == "VERIFIED":
            p = row.get("test_path", "")
            _check_anchor(root, mid, "TEST", p, results, {})
        # MOD anchor
        a = row.get("module_anchor", "")
        if a and a != "MISSING" and not os.path.isfile(os.path.join(root, a)):
            results.append(err("DANGLING_REF", f"{mid}: MOD anchor 路径不存在 {a}"))

    # C8 越界/缺 authority：VERIFIED 的真实 id 应有 authority（strict 为 ERROR，否则 WARN）
    for row in rows:
        mid = row.get("module_id", "?")
        for layer, idk, stk, pathk, _rx in LAYERS:
            if layer in ("MOD", "SRC", "TEST"):
                # MOD=行键；SRC/TEST 的“权威”= src_path/test_path 文件存在性（C7 已校验），
                # 不另查 id 文本（合成测试锚由 test_path 文件承载）
                continue
            st = row.get(stk)
            ident = row.get(idk, "")
            if st != "VERIFIED" or not ident or PLACEHOLDER_RE.fullmatch(str(ident)):
                continue
            if _id_has_authority(root, layer, str(ident), row.get(pathk) if pathk else None):
                continue
            # 一律先记 WARN，由 _finish 统一做"strict 升级 + 烧毁式基线"判定 ——
            # 若在这里就按 args.strict 直接记 ERROR，基线条目永远匹配不到 WARN，
            # 会把自己的基线判成 BASELINE_STALE（实测过）。
            results.append(warn("REF_OUT_OF_SCOPE",
                                f"{mid} [{layer}] id {ident} 无 authority 文件可见（WARN）",
                                sig=f"REF_OUT_OF_SCOPE|{mid}|{layer}|{ident}"))

    # 模块清单完整性：每个 registry/已建模块被覆盖（不判孤行）
    _check_module_coverage(root, rows, results)

    # W4-A3（C 类：打印结论↔退出码一致）：退出码必须用 _finish **判后**的结论。
    # 旧写法在 _finish 之后从原 results 重算 errors —— strict 升级与基线判定的
    # 结果不进退出码 ⇒ 打印 FAIL 却 rc=0（该检查器自身的 fail-open）。
    n_errors, _n_warns = _finish(root, args, results)
    return _exit_code(n_errors)


# —— 复合 test_path / src_path 解析（W4-A3） -------------------------------------
# 矩阵的锚列允许三种形态（全部在现行矩阵中出现，必须机器可解析）：
#   ① 单文件          `eng/tests/unit/x.cpp`
#   ② 文件::用例/符号  `docs/algorithms/X.md::TEST-X-DESIGN-001`
#   ③ **复合**        `<目录> (ctest <目标>/<目标>); <文件>; <源文件说明> <文件>:<行> <符号>`
#     例：`lib/algorithms/calibration/tests/p1cal (ctest p1cal_units/…);
#          eng/tests/backend/test_calibration_oracle.py;
#          eng/tests/unit/CMakeLists.txt:441 master_flat_median`
# 旧实现把 ③ **整串**当单一路径 ⇒ 恒报「路径不存在」（DANGLING_REF 4 条，
# 且净计数与 BOM/ID 格式混在一起掩盖了它）。本解析器把 ③ 拆成逐项引用，
# 并**区分**「路径不存在（PATH_NOT_FOUND）」与「用例/符号不存在（CASE_NOT_FOUND）」。
COMPOSITE_SPLIT_RE = re.compile(r"\s*;\s*")
CTEST_GROUP_RE = re.compile(r"[(（]\s*ctest\s+([^)）]*)[)）]")
CTEST_TARGET_RE = re.compile(r"[A-Za-z0-9_][A-Za-z0-9_.\-*?\[\]]*")
FILE_REF_RE = re.compile(r"[A-Za-z0-9_][A-Za-z0-9_.\-/]*\.(?:cpp|cc|cxx|h|hpp|py|md|json|yaml|yml|csv|txt|sh|ps1|cmake|in)")
PATH_WITH_LINE_RE = re.compile(r"^([A-Za-z0-9_][A-Za-z0-9_.\-/]*):(\d+)(?:\s+(.*))?$")
PATH_PREFIXES = ("lib/", "eng/tests/", "docs/", "eng/contracts/", "lib/include/", "eng/tools/", "eng/ci/",
                 "eng/cmake/", "scripts/", "eng/packaging/", "eng/packaging/config/", "artifacts/", "工程控制/")


def _path_kind(root: str, rel: str) -> str:
    """引用形态判定：`dir` / `file` / `absent`。"""
    full = os.path.join(root, rel)
    if os.path.isdir(full):
        return "dir"
    if os.path.isfile(full):
        return "file"
    return "absent"


def _dir_tracked(root: str, rel: str) -> bool:
    """目录引用算「已跟踪」当且仅当目录下至少有一个 git 跟踪文件。"""
    ok, why = probe_tracking(root)
    if not ok:
        raise GitUnavailable(why)
    try:
        r = subprocess.run(["git", "ls-files", "--", rel + "/"], cwd=root,
                           capture_output=True, text=True, timeout=60)
    except (OSError, subprocess.SubprocessError) as exc:
        raise GitUnavailable(f"git ls-files 调用失败（{exc.__class__.__name__}: {exc}）") from None
    if r.returncode != 0:
        last = (r.stderr.strip().splitlines() or [""])[-1]
        raise GitUnavailable(f"git ls-files -- {rel}/ rc={r.returncode}: {last[:140]}")
    return bool(r.stdout.strip())


def _git_tracked_dir(root: str, rel: str) -> bool:
    """路径是否属于「受 Git 跟踪」面；git 面不可用 ⇒ False（调用方已整面跳过）。"""
    if not tracking_available():
        return False
    return _dir_tracked(root, rel) if _path_kind(root, rel) == "dir" else git_tracked(root, rel)


def parse_matrix_anchor(p: str) -> dict:
    """把锚串解析成结构化引用（纯函数，供自测与判据共用）。

    返回 {"entries": [ {"kind": "path"|"case"|"ctest", "raw":…, "path":…, "case":…,
                        "line":…, "target":…} ], "unparsed": [...]}
    """
    out: dict = {"entries": [], "unparsed": []}
    p = (p or "").strip()
    if not p:
        return out
    # ② 文件::用例（可能多段，用 ';' 分隔多组）
    if "::" in p:
        for chunk in COMPOSITE_SPLIT_RE.split(p):
            chunk = chunk.strip()
            if not chunk:
                continue
            if "::" in chunk:
                path, syms = chunk.split("::", 1)
                out["entries"].append({"kind": "path", "raw": path.strip(),
                                       "path": path.strip()})
                for sym in syms.split(","):
                    sym = sym.strip()
                    if sym and sym != "MISSING":
                        out["entries"].append({"kind": "case", "raw": sym,
                                               "path": path.strip(), "case": sym})
            elif any(ch in chunk for ch in "(（"):
                # 样例：docs/algorithms/X.md (TEST-X-DESIGN-001)
                m = re.match(r"^([^\s(（]+)\s*[(（]([^)）]*)[)）]", chunk)
                if m:
                    out["entries"].append({"kind": "path", "raw": m.group(1).strip(),
                                           "path": m.group(1).strip()})
                    for sym in re.split(r"[,，、/\s]+", m.group(2)):
                        sym = sym.strip()
                        if sym:
                            out["entries"].append({"kind": "case", "raw": sym,
                                                   "path": m.group(1).strip(), "case": sym})
                else:
                    out["entries"].append({"kind": "path", "raw": chunk, "path": chunk})
            else:
                out["entries"].append({"kind": "path", "raw": chunk, "path": chunk})
        return out

    # ③ 复合形态：按 ';' 分段
    for chunk in COMPOSITE_SPLIT_RE.split(p):
        chunk = chunk.strip()
        if not chunk:
            continue
        ctest = CTEST_GROUP_RE.search(chunk)
        head = CTEST_GROUP_RE.sub("", chunk).strip()
        if ctest:
            for target in CTEST_TARGET_RE.findall(ctest.group(1).replace("/", " ")):
                out["entries"].append({"kind": "ctest", "raw": target, "target": target})
        if "::" in head:
            path, syms = head.split("::", 1)
            out["entries"].append({"kind": "path", "raw": path.strip(), "path": path.strip()})
            for sym in syms.split(","):
                sym = sym.strip()
                if sym and sym != "MISSING":
                    out["entries"].append({"kind": "case", "raw": sym,
                                           "path": path.strip(), "case": sym})
            continue
        # 段内可含**多个** `<文件>:<行> <符号>` 引用（以 '+' 等连接），逐项抽全
        line_refs = list(PATH_WITH_LINE_RE.finditer(head))
        if line_refs:
            for j, m in enumerate(line_refs):
                ref = m.group(1)
                out["entries"].append({"kind": "path", "raw": ref, "path": ref,
                                       "line": int(m.group(2))})
                # 符号段 = 到下一个引用起点或段尾
                stop = (line_refs[j + 1].start() if j + 1 < len(line_refs) else len(head))
                tail = head[m.end():stop].strip(" \t+")
                if tail:
                    out["entries"].append({"kind": "case", "raw": tail, "path": ref,
                                           "case": tail})
            continue
        # 纯路径 / 带说明前缀的路径：抽出所有路径形态词元；一个都没有则整段当路径
        refs = [t for t in FILE_REF_RE.findall(head)
                if t.startswith(PATH_PREFIXES) or "/" in t]
        if refs:
            for ref in refs:
                out["entries"].append({"kind": "path", "raw": ref, "path": ref})
        elif head.startswith(PATH_PREFIXES) or "/" in head:
            out["entries"].append({"kind": "path", "raw": head, "path": head})
        else:
            out["unparsed"].append(chunk)
    return out


def _check_anchor(root, mid, layer, p, results, seen):
    if is_empty(p) or p == "MISSING":
        results.append(err("DANGLING_REF", f"{mid} [{layer}]: VERIFIED 但路径为空/为 MISSING"))
        return
    parsed = parse_matrix_anchor(p)
    _reported_absent = set()
    _reported_untracked = set()  # 同路径只报一次（多处共锚同一文件）
    for item in parsed["unparsed"]:
        # fail-closed：不认识的形态不得静默通过；逐字点名，便于登记新形态
        results.append(err("DANGLING_REF",
                           f"{mid} [{layer}]: 无法解析的复合锚形态（须显式登记）: {item[:90]}", "UNPARSED_COMPOUND_FORM"))
    for ent in parsed["entries"]:
        if ent["kind"] == "ctest":
            tgt = ent["target"]
            if any(ch in tgt for ch in "*?["):
                continue  # glob 目标不逐名判定
            if not _cmake_face_available():
                continue  # 枚举面不可用：main 已统一判红并留痕（CMAKE_FACE_EMPTY），不在此刷 N 条
            if not cmakelists_has_target(root, tgt):
                results.append(err("DANGLING_REF",
                                   f"{mid} [{layer}]: ctest 目标 {tgt} 未在任何 CMakeLists 中注册", "CTEST_TARGET_NOT_FOUND"))
            continue
        rel = ent.get("path") or ""
        kind = _path_kind(root, rel)
        if kind == "absent":
            if rel not in _reported_absent:  # 同一路径只报一次（路径 + N 个用例共锚）  # noqa: E501
                _reported_absent.add(rel)
                results.append(err("DANGLING_REF",
                                   f"{mid} [{layer}]: 路径不存在 {rel}", "PATH_NOT_FOUND"))
            continue
        if tracking_available() and not _git_tracked_dir(root, rel):
            # W4-A3 判据分级：「路径不存在」是硬错（ERROR）；「路径在盘上但尚未纳入
            # 版本库」是**工作区瞬时状态**（前台提交前必然出现，如 W4-A9 的迁移文件），
            # 与同文件既有的 REF_OUT_OF_SCOPE 同例：默认 WARN，--strict 升级 ERROR。
            # 同一路径只报一次（路径 + N 个用例共锚的场景会重复 N 行）。
            if rel not in _reported_untracked:
                _reported_untracked.add(rel)
                results.append(warn("DANGLING_REF",
                                    f"{mid} [{layer}]: 路径存在但未受 Git 跟踪 {rel}",
                                    "PATH_NOT_TRACKED",
                                    sig=f"DANGLING_REF|PATH_NOT_TRACKED|{mid}|{layer}|{rel}"))
            continue
        if ent["kind"] == "case":
            sym = ent["case"]
            if kind == "dir":
                # 目录引用 + 用例名：在该目录的 CMakeLists 中查找 ctest 目标/用例名
                if not _cmake_face_available():
                    continue
                if not cmakelists_has_target(root, sym, subdir=rel):
                    results.append(err("DANGLING_REF",
                                       f"{mid} [{layer}]: 用例/目标 {sym} 不在 {rel} 的 CMakeLists", "CASE_NOT_FOUND"))
                continue
            try:
                text = open(os.path.join(root, rel), encoding="utf-8",
                            errors="replace").read()
            except Exception:  # noqa: BLE001
                text = ""
            # 符号/用例词的判定：允许"符号 紧随的参数"（如 `add_subdirectory (if EXISTS 门卫)`）
            # 与"多词说明"两种写法 —— 逐词找首个命中词，全不命中才判 CASE_NOT_FOUND。
            words = [w for w in re.split(r"[^\w]+", sym) if w]
            if not words:
                continue
            if not any(re.search(r"\b" + re.escape(w) + r"\b", text) for w in words):
                results.append(err("DANGLING_REF",
                                   f"{mid} [{layer}]: 用例/符号 {sym} 不在 {rel}", "CASE_NOT_FOUND"))


_CMAKELISTS_TARGET_CACHE: dict = {}
# CMakeLists 枚举面的剪枝排除面（构建区 / 临时产物区 / VCS 元数据；AGENTS.md §3/§7）
CMAKE_FACE_EXCLUDED_DIRS = ("build", "run", ".git")
ADD_TEST_NAME_RE = re.compile(r"add_test\s*\(\s*NAME\s+([^\s()#]+)")


def _cmake_walk_error(exc: OSError) -> None:
    """遍历期错误 ⇒ 枚举面可能静默缩小 —— fail-closed，不静默跳过。"""
    raise AnchorStale(f"CMAKE_SCAN_FACE_WALK_ERROR: {exc}")


def cmakelists_face(root: str) -> list[str]:
    """CMakeLists.txt 枚举面（仓库相对路径；确定性 os.walk + 剪枝，**不依赖 git**）。

    GITDECOUPLE-02：原实现用 `git ls-files -- CMakeLists.txt */CMakeLists.txt` 取面 ——
    那是**纯枚举源**（判据是 "目标是否在任一 CMakeLists 的 add_test(NAME …) 中注册"），
    却在非 git 树里退化成空面 ⇒ 全部 ctest 目标被判 "未在任何 CMakeLists 中注册"（误导）。
    实测真仓库：剪枝枚举 80 个 == `git ls-files` 80 个（集合逐元素相等），
    add_test(NAME) 目标 325 == 325（无漂移）；且 worktree 枚举不再受
    git pathspec `*/CMakeLists.txt` 只匹配一层深的限制。

    枚举为空 ⇒ AnchorStale（fail-closed；§1「scanned == 0 ⇒ rc != 0」），
    调用方（main）转成点名判红 CMAKE_FACE_EMPTY。
    """
    if "names" in _CMAKELISTS_TARGET_CACHE:
        return _CMAKELISTS_TARGET_CACHE.get("face", [])
    found: list[str] = []
    for dirpath, dirnames, filenames in os.walk(root, onerror=_cmake_walk_error,
                                               followlinks=False):
        dirnames[:] = sorted(d for d in dirnames if d not in CMAKE_FACE_EXCLUDED_DIRS)
        if "CMakeLists.txt" in filenames:
            found.append(os.path.relpath(os.path.join(dirpath, "CMakeLists.txt"), root)
                         .replace(os.sep, "/"))
    if not found:
        raise AnchorStale(
            f"CMAKE_SCAN_FACE_EMPTY: {root} 下未枚举到任何 CMakeLists.txt"
            f"（排除面 {CMAKE_FACE_EXCLUDED_DIRS}）—— ctest 目标注册判据无输入")
    names = set()
    for rel in sorted(found):
        try:
            text = open(os.path.join(root, rel), encoding="utf-8",
                        errors="replace").read()
        except OSError:
            continue
        for m in ADD_TEST_NAME_RE.finditer(text):
            names.add((m.group(1).strip().strip('"'), rel))
    _CMAKELISTS_TARGET_CACHE["face"] = sorted(found)
    _CMAKELISTS_TARGET_CACHE["names"] = names
    return _CMAKELISTS_TARGET_CACHE["face"]


def _cmake_face_available() -> bool:
    return "names" in _CMAKELISTS_TARGET_CACHE


def cmakelists_has_target(root: str, target: str, subdir: str = None) -> bool:
    """目标名是否出现在枚举面内任一 CMakeLists.txt 的 add_test(NAME …) 中。

    带缓存与一次全树扫描；用于区分「路径不存在」与「用例/目标不存在」。
    枚举面为空 ⇒ AnchorStale（不静默返回 False 把真因报成 "目标未注册"）。
    """
    if not _cmake_face_available():
        cmakelists_face(root)
    names = _CMAKELISTS_TARGET_CACHE.get("names", set())
    if subdir is not None:
        pre = subdir.rstrip("/") + "/"
        return any(n == target and r.startswith(pre) for n, r in names)
    return any(n == target for n, _r in names)


def _id_has_authority(root, layer, ident, pathhint):
    if pathhint and pathhint != "MISSING" and not is_empty(pathhint):
        base = pathhint.split("::", 1)[0]
        full = os.path.join(root, base)
        if os.path.isfile(full):
            try:
                if re.search(r"\b" + re.escape(ident) + r"\b", open(full, encoding="utf-8", errors="ignore").read()):
                    return True
            except Exception:  # noqa: BLE001
                pass
    for d in AUTHORITY_DIRS.get(layer, []):
        base = os.path.join(root, d)
        if not os.path.isdir(base):
            continue
        for dirpath, _dirs, files in os.walk(base):
            for fn in files:
                if not fn.endswith((".md", ".csv", ".yaml", ".yml", ".json", ".c", ".h", ".cpp")):
                    continue
                full = os.path.join(dirpath, fn)
                try:
                    text = open(full, encoding="utf-8", errors="ignore").read()
                except Exception:  # noqa: BLE001
                    continue
                if re.search(r"\b" + re.escape(ident) + r"\b", text):
                    return True
    return False


def _check_module_coverage(root, rows, results):
    """已知模块清单（真实目录/registry front-matter）必须在矩阵中有一行。"""
    known = set()
    reg = os.path.join(root, "docs/modules/registry")
    if os.path.isdir(reg):
        for fn in sorted(os.listdir(reg)):
            if fn.endswith(".md"):
                p = os.path.join(reg, fn)
                try:
                    head = open(p, encoding="utf-8", errors="ignore").read(2000)
                except Exception:  # noqa: BLE001
                    continue
                m = re.search(r"^id:\s*(MOD-\S+)", head, re.M)
                if m:
                    known.add(m.group(1).strip())
    # 服务/conformance/provider 目录 → MOD- 行键
    if os.path.isfile(os.path.join(root, "eng/tests/conformance/noop/module.yaml")):
        known.add("MOD-astrocs-conformance-noop")
    if os.path.isdir(os.path.join(root, "lib/infrastructure/aio/io")):
        known.add("MOD-astrocs-services-io")
    if os.path.isdir(os.path.join(root, "lib/infrastructure/benchmark/cpu/common")):
        known.add("MOD-astrocs-providers-cpu")
    present = {r.get("module_id") for r in rows}
    missing = sorted(known - present)
    if missing:
        results.append(err("MISSING_FILE", f"矩阵缺少已知模块行: {missing}"))


def _check_csv_parity(root, results) -> None:
    """JSON(权威) 与 CSV(视图) 必须同构：表头=CSV_COLS，每行逐列一致。"""
    full = os.path.join(root, MATRIX_CSV_REL)
    if not os.path.isfile(full):
        return  # MISSING_FILE 已报
    try:
        with open(full, encoding="utf-8", newline="") as f:
            reader = csv.reader(f)
            header = next(reader)
            csv_rows = [r for r in reader if r and any(c.strip() for c in r)]
    except Exception as exc:  # noqa: BLE001
        results.append(err("SCHEMA_VIOLATION", f"{MATRIX_CSV_REL} 解析失败: {exc}"))
        return
    if header != CSV_COLS:
        results.append(err("SCHEMA_VIOLATION",
                           f"{MATRIX_CSV_REL} 表头与 JSON 列不一致: 缺={set(CSV_COLS)-set(header)} 多={set(header)-set(CSV_COLS)}"))
        return
    try:
        with open(os.path.join(root, MATRIX_REL), encoding="utf-8") as f:
            mods = json.load(f)["modules"]
    except Exception:  # noqa: BLE001
        return
    json_map = {}
    for row in sorted(mods, key=lambda r: r["module_id"]):
        json_map[row["module_id"]] = [str(row.get(c, "")) for c in CSV_COLS]
    if len(csv_rows) != len(json_map):
        results.append(err("SCHEMA_VIOLATION",
                           f"{MATRIX_CSV_REL} 行数 {len(csv_rows)} != JSON 模块数 {len(json_map)}（请重跑 gen_traceability_csv.py）"))
        return
    for i, r in enumerate(csv_rows):
        mid = r[0]
        if mid not in json_map:
            results.append(err("SCHEMA_VIOLATION", f"{MATRIX_CSV_REL} 行 {i} 模块 {mid} 不在 JSON"))
            continue
        if r != json_map[mid]:
            diffs = [CSV_COLS[j] for j in range(len(CSV_COLS)) if j < len(r) and j < len(json_map[mid]) and r[j] != json_map[mid][j]]
            results.append(err("SCHEMA_VIOLATION",
                               f"{MATRIX_CSV_REL} 行 {i} {mid} 与 JSON 不一致列: {diffs}（请重跑 gen_traceability_csv.py）"))


# ── 烧毁式 WARN 基线（附录 H.6，W4-A3 实施）─────────────────────────────────────
# 口径（前台裁决 2026-09-17）：默认 rc=0 是门，--strict 是审计升级。存量 WARN 走
# **烧毁式基线**：①只降不升 —— 基线里"匹配不到当前 WARN"的条目判红
# （BASELINE_STALE），必须随修复同步删除，不允许把基线当只增不减的豁免表；
# ②归零转全量强制 —— 基线为空时所有 WARN 在 --strict 下一律升级 ERROR；
# ③--ignore-baseline = 负例注入面：绕过基线跑，存量 WARN 必须让 --strict 判红。
WARN_BASELINE_REL = "docs/traceability/traceability_warn_baseline.json"


def _load_warn_baseline(root, rel):
    """返回 (sigs, error)。基线缺失 = 空基线（转全量强制），不是错。"""
    p = os.path.join(root, rel)
    if not os.path.isfile(p):
        return set(), None
    try:
        doc = json.load(open(p, encoding="utf-8"))
    except Exception as exc:  # noqa: BLE001
        return set(), f"基线不可解析 {rel}: {exc}"
    sigs = set()
    for ent in doc.get("entries", []):
        sig = ent.get("sig")
        if not sig:
            return set(), f"基线条目缺 sig: {ent!r}"
        sigs.add(sig)
    return sigs, None


def _exit_code(n_errors: int) -> int:
    """退出码：依赖不可用 ⇒ 2（fail-closed，不给结论）；否则按错误数 1/0。"""
    if not tracking_available():
        return 2
    return 1 if n_errors else 0


def _finish(root, args, results):
    base_sigs, base_err = _load_warn_baseline(root, WARN_BASELINE_REL)
    if getattr(args, "ignore_baseline", False):
        base_sigs = set()
    raw_warns = [r for r in results if r["severity"] == "WARN"]
    baselined, unbaselined = [], []
    for r in raw_warns:
        (baselined if r.get("sig") and r["sig"] in base_sigs else unbaselined).append(r)
    matched = {r["sig"] for r in baselined if r.get("sig")}
    stale = sorted(base_sigs - matched)
    extra = []
    if base_err:
        extra.append(err("BASELINE_INVALID", base_err))
    for sig in stale:
        # 只降不升：条目已失效（WARN 被修掉/改名）⇒ 必须同步删条目
        extra.append(err("BASELINE_STALE",
                         f"基线条目在当前扫描面不存在（只降不升，须删除）: {sig}"))
    if args.strict:
        # 注意：--ignore-baseline 在这里**不**豁免升级 —— 它的语义是"假装基线为空"，
        # 于是存量 WARN 全部落进 unbaselined 并在 --strict 下升级为 ERROR
        # （这正是负例注入面要证明的：门本身仍能红，基线只是记账）。
        for r in unbaselined:
            extra.append(err(r["code"], r["detail"] + "（--strict 升级，且不在烧毁式基线内）",
                             r.get("subcode")))
        unbaselined = []
    results = [r for r in results if r["severity"] != "WARN"] + list(extra) + unbaselined
    errors = [r for r in results if r["severity"] == "ERROR"]
    warns = [r for r in results if r["severity"] == "WARN"]
    summary = {
        "checker": "check_traceability_matrix.py",
        "matrix": MATRIX_REL,
        "strict": bool(args.strict),
        "errors": len(errors),
        "warns": len(warns),
        "warn_baseline": {
            "path": WARN_BASELINE_REL,
            "entries": len(base_sigs),
            "baselined": len(baselined),
            "unbaselined": len(unbaselined),
            "stale": len(stale),
            "bypassed": bool(getattr(args, "ignore_baseline", False)),
        },
        "issues": sorted(results, key=lambda r: (r["severity"], r["code"], r["detail"])),
        # git 面留痕（§1「显式降级 + 留痕」）：available=False ⇒ 结论是 UNVERIFIABLE，rc=2
        "git_face": {"available": tracking_available(),
                     "detail": _TRACKING.get("reason", "")},
        "result": ("TRACEABILITY_MATRIX_FAIL" if errors
                   else ("TRACEABILITY_MATRIX_UNVERIFIABLE" if not tracking_available()
                         else "TRACEABILITY_MATRIX_PASS")),
    }
    # 附上输入 hash 与矩阵统计
    full = os.path.join(root, MATRIX_REL)
    try:
        summary["matrix_sha256"] = hashlib.sha256(open(full, "rb").read()).hexdigest()
    except Exception:  # noqa: BLE001
        summary["matrix_sha256"] = None
    text = json.dumps(summary, ensure_ascii=False, indent=1, sort_keys=True)
    if args.json_out:
        try:
            os.makedirs(os.path.dirname(os.path.abspath(args.json_out)) or ".", exist_ok=True)
            with open(args.json_out, "w", encoding="utf-8") as f:
                f.write(text + "\n")
        except Exception as exc:  # noqa: BLE001
            print(f"TOOLING_FAILURE: 写 json-out 失败 {exc}", file=sys.stderr)
    if errors:
        print("TRACEABILITY_MATRIX_FAIL errors=%d" % len(errors))
        for r in errors:
            tag = r["code"] + ("/" + r["subcode"] if r.get("subcode") else "")
            print(f"  [{tag}] {r['detail']}")
        print("WARN 摘要: %d 条（--strict 将升级为 ERROR）" % len(warns))
    elif not tracking_available():
        # 无判据违规但依赖不可用 ⇒ 不得打印 PASS（§1 fail-closed）
        print("TRACEABILITY_MATRIX_UNVERIFIABLE: GIT_UNAVAILABLE %s"
              % _TRACKING.get("reason", ""))
        print("  「受 Git 跟踪」子判据（C1/C7）已显式跳过并留痕；存在性等判据已全部执行；"
              "依赖不可用 ⇒ rc=2，不给出 PASS 结论")
    else:
        rows = 0
        try:
            d = json.load(open(os.path.join(root, MATRIX_REL), encoding="utf-8"))
            rows = len(d.get("modules", []))
        except Exception:  # noqa: BLE001
            pass
        print(f"TRACEABILITY_MATRIX_PASS modules={rows} errors=0"
              + (f" warns={len(warns)}" if warns else ""))
    return len(errors), len(warns)


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except SystemExit:
        raise
    except Exception as exc:  # noqa: BLE001
        print(f"TOOLING_FAILURE: 未捕获异常 {exc!r}", file=sys.stderr)
        raise SystemExit(3)
