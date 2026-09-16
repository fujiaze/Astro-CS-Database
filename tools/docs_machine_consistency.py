#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""docs_machine_consistency.py — 文档↔代码机器一致性检查（含同常数多写法门）

检查项（check 名与报告字段向后兼容）:
  1. config_weight_mode_ivar        — CONFIG_SCHEMA weight_mode auto/ivar ↔ stage2_common 解析
  2. frame_id_contract_exact        — DATA-FRAME-ID-001：SHA-256 truncate，无 FNV/路径派生残留
  3. error_taxonomy_exit_codes      — ERROR_MODEL 退出码全集合 ↔ orchestrator.h 枚举
  4. integration_status_full_set    — INTEGRATION_ALGORITHMS ↔ integrate.h（P2_INTEGRATE_*）
  5. rejection_status_full_set      — REJECTION_ALGORITHMS ↔ rejection.h（P2_STATUS_*/P2_REASON_*）
  6. stage_ids_docs_vs_orchestrator — stage ID 面 ↔ orchestrator stage_name_v2
  7. snr_constants                  — NOISE_MODEL/PSF 常数 ↔ noise_model.cpp（前缀锚 + 权威值在场）
  8. product_contracts              — DATA_SEMANTICS 产品 ↔ aio_hips.h 产品位
  9. drizzle_variance_formula       — DRIZZLE.md 方差公式 ↔ drizzle_engine.h
 10. numeric_constants_single_spelling — **新增门**：同一科学常数只允许一种全精度写法；
       截断字面量只能在「≈/简写」语境出现（同给权威值或实测偏差），或落显式登记的非判定面。
 11. source_paths_alive            — **fail-closed / 锚存活**：必需源文件在当前树解析不到即红。

设计约束（ENGINEERING_SPEC §8 五条）:
  * 可执行负例面：--self-test（tempfile mini-repo 正/负例）与 --fault-inject（真仓副本注入必红）。
  * fail-closed：路径缺失 / 扫描面为空 / 必需字面量缺失 一律判红，不崩溃后静默通过。
  * 锚存活：源码路径不再硬编码迁移前目录；由 basename + 期望内容在 tracked 面内解析
    （迁移改址后自动跟随；解析不到即红并打印候选）。
  * 注册表双向一致：本工具尚未登记进 ci/checks.json（该文件归 CI 线独占写者），
    注册行与 docs/ci 说明随 W4-A6 交付清单交前台；未登记不改变本门判据。
  * 裁决 named-ID：判据引用 V12-N-01 / V12-N-02 / V12-N-03 / M3-A-008 与负责人裁决 S-1
    （一处一数值；落位 docs/science/NOISE_MODEL.md §14.2、docs/science/PHOTOMETRY.md §14.2）。

用法:
  python3 tools/docs_machine_consistency.py [--root <repo>] [--json-out <file>] [--quiet]
  python3 tools/docs_machine_consistency.py --self-test
  python3 tools/docs_machine_consistency.py --fault-inject <name> [--root <repo>]

exit 0 = 全部 PASS；exit 1 = 有检查判红（含 fail-closed）；exit 2 = 用法/内部错误。
"""

from __future__ import annotations

import argparse
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile

BT = chr(96)  # 反引号：本文件多处需要在文档文本里核对带反引号的常量写法

# ---------------------------------------------------------------------------
# 常数族单一权威值（family -> 权威全精度写法；截断写法只能在「≈」语境出现）
# ---------------------------------------------------------------------------
CONST_FAMILIES = {
    "mad_to_sigma": {
        "full": ["1.482602218505602"],
        "truncated": [
            (re.compile(r"(?<![\d.])1\.4826(?!02218505602)(?![\d])"), "1.4826"),
            (re.compile(r"(?<![\d.])1\.4826022185(?![\d])"), "1.4826022185"),
        ],
        "authority": "docs/science/NOISE_MODEL.md §14.2 / 负责人裁决 S-1（一处一数值）",
        "decisions": ["V12-N-01", "V12-N-03", "M3-A-008"],
    },
    "sigma_to_mad": {
        "full": ["0.6744897501960817"],
        "truncated": [
            (re.compile(r"(?<![\d.])0\.6745(?![\d])"), "0.6745"),
        ],
        "authority": "docs/science/PHOTOMETRY.md §14.2 = 1/1.482602218505602（与 NOISE_MODEL §14.2 同值）",
        "decisions": ["V12-N-03", "M3-A-008"],
    },
    "trimmean_to_sigma": {
        "full": ["0.7316727929211932"],
        "truncated": [
            (re.compile(r"(?<![\d.])0\.7316728(?![\d])"), "0.7316728"),
            (re.compile(r"(?<![\d.])0\.73167(?!27929211932)(?![\d])"), "0.73167"),
        ],
        "authority": "docs/science/PSF.md §14.3 / lib/algorithms/noise_snr kTrimMeanToSigma",
        "decisions": ["V12-N-02", "V12-N-04"],
    },
}

# 「≈/简写」语境标记（且同行须给出权威值或实测偏差数字）
APPROX_MARKERS = ("≈", "~", "约等于", "简写", "截断", "相对差", "绝对差", "不得互换",
                  "不可互换", "只允许", "容许", "旧 4 位", "缺陷", "对照", "归档",
                  "历史写法", "不得再作")

# 显式登记的非判定面：命中的截断字面量只登记不判红（每条必须给理由，不许空豁免）
BENIGN_SURFACES = [
    (re.compile(r"^docs/archive/"), "历史归档（GOV-002 归档面，非活动规范）"),
    (re.compile(r"^docs/references/PHOTOMETRY_LITERATURE_REVIEW_ARCHIVE\.md$"),
     "文献评审归档副本（历史冻结，非活动规范）"),
    (re.compile(r"^lib/infrastructure/aio/healpix_db/archive/legacy/"),
     "legacy 归档实现（不在根 CMake 交付图；W4-A6 不改归档码）"),
    (re.compile(r"^lib/infrastructure/hips_browser/"),
     "HiPS 浏览器显示工具（可视化拉伸，非科学量计算）"),
    (re.compile(r"^tools/quality/frame_qc_grid\.py$"),
     "纯展示工具：九宫格视觉拉伸（任务卡点名不得强改；−1.50e-6 偏差对 8bit 显示无影响）"),
    (re.compile(r"^lib/infrastructure/benchmark/"),
     "独立实现：benchmark baseline provider（口径独立于科学交付面，任务卡点名）"),
    (re.compile(r"^lib/infrastructure/acr/backends/cuda/"), "ACR dormant 通道（最高设计：ACR dormant）"),
    (re.compile(r"^lib/algorithms/[^/]+/memory\.md$"), "模块历史记忆文件（非规范文本）"),
    (re.compile(r"^lib/algorithms/photometry/tests/p1phot/.*\.(cpp|hpp)$"),
     "回归锁自述文本（被验条目原文引用，数值本身即断言对象）"),
    (re.compile(r"^lib/algorithms/star_detection/tests/p1star/p1star_mad_check\.cpp$"),
     "缺陷对照夹具：4 位截断值是「位数纪律判别力」的被测对象，故意保留"),
    (re.compile(r"^lib/algorithms/star_detection/tests/p1star/CMakeLists\.txt$"), "上条夹具的构建注释"),
    (re.compile(r"^tests/api/|^tests/backend/|^tests/cpu/"),
     "Python 侧回归 oracle（交付清单 C-2 报告，本任务未改）"),
    (re.compile(r"^lib/algorithms/photometry/cpp/test/"),
     "Python 侧显示/回归脚本（非 ABI 交付面）"),
    (re.compile(r"^lib/algorithms/coverage/tools/"), "覆盖域诊断工具（非交付面）"),
    (re.compile(r"^reports/"), "审计报告与历史证据（非活动规范文本）"),
    (re.compile(r"^工程控制/"), "控制包任务卡与账本（过程记录，非活动规范文本）"),
    (re.compile(r"^tests/unit/v6_p2_rej/oracle_expected\.inc$"),
     "数值 oracle 期望表（数据数组，非常数写法；W4-A6 / M7-T-102 归其域）"),
    (re.compile(r"^tools/docs_machine_consistency\.py$"),
     "本门自身的判据正则与负例夹具定义处（含截断字面量是判据的一部分，自扫描会造成自指假红）"),
]

TEXT_EXT = {".cpp", ".h", ".hpp", ".c", ".cc", ".cu", ".inc", ".py", ".md", ".yaml",
            ".yml", ".json", ".jsonl", ".txt", ".csv", ".psv", ".cmake", ".sh"}
SKIP_PATH_PARTS = (".git/", "build/", "run/", "third_party/", "node_modules/",
                   "__pycache__/", ".pytest_cache/", "logs/")


# ---------------------------------------------------------------------------
# 路径解析（锚存活 + fail-closed）
# ---------------------------------------------------------------------------
class Resolver:
    """把逻辑名解析到当前树中的真实路径；解析不到即 fail-closed。"""

    def __init__(self, root: str):
        self.root = root
        self._by_base = None
        self._tracked_cache = None
        self.missing = []

    def _tracked(self):
        if self._tracked_cache is not None:
            return self._tracked_cache
        try:
            out = subprocess.run(["git", "ls-files"], cwd=self.root, capture_output=True,
                                 text=True, timeout=300)
            if out.returncode == 0 and out.stdout.strip():
                self._tracked_cache = [p for p in out.stdout.splitlines() if p.strip()]
                return self._tracked_cache
        except Exception:
            pass
        found = []
        for dp, dns, fns in os.walk(self.root):
            dns[:] = [d for d in dns if d not in (".git", "build", "third_party", "node_modules")]
            for fn in fns:
                found.append(os.path.relpath(os.path.join(dp, fn), self.root))
        self._tracked_cache = found
        return found

    def index(self):
        if self._by_base is None:
            idx = {}
            for rel in self._tracked():
                if any(s in rel for s in SKIP_PATH_PARTS):
                    continue
                idx.setdefault(os.path.basename(rel), []).append(rel)
            self._by_base = idx
        return self._by_base

    def _read(self, rel: str) -> str:
        try:
            with open(os.path.join(self.root, rel), encoding="utf-8", errors="replace") as f:
                return f.read()
        except OSError:
            return ""

    def find(self, base: str, must_contain, hint: str = "") -> str:
        cands = [c for c in self.index().get(base, []) if "archive" not in c]
        ok = [c for c in cands if all(m in self._read(c) for m in must_contain)]
        if len(ok) == 1:
            return ok[0]
        self.missing.append("%s (basename=%s 迁移前=%s 候选=%s 内容匹配=%s)"
                            % (must_contain[0], base, hint, cands, ok))
        return ""

    def read(self, rel: str) -> str:
        return self._read(rel) if rel else ""


SOURCES = [
    ("config_schema", "CONFIG_SCHEMA.md", ("weight_mode",), "docs/development/CONFIG_SCHEMA.md"),
    ("stage2_common", "stage2_common.cpp", ('wm == "auto" || wm == "ivar"',),
     "lib/phase2/src/stage2_common.cpp"),
    ("sampler_h", "sampler.h", ("truncated-64",), "lib/phase2/include/astro/phase2/sampler.h"),
    ("data_semantics", "DATA_SEMANTICS.md", ("frame_id",), "docs/contracts/DATA_SEMANTICS.md"),
    ("upm_doc", "PHASE2_UPM.md", ("control cell",), "docs/science/PHASE2_UPM.md"),
    ("error_model", "ERROR_MODEL.md", ("AstroCsExitCode",), "docs/architecture/ERROR_MODEL.md"),
    ("orchestrator_h", "orchestrator.h", ("namespace AstroCsExitCode",),
     "lib/orchestrator/cpp/include/orchestrator.h"),
    ("orchestrator_cpp", "orchestrator.cpp", ("stage_name_v2",),
     "lib/orchestrator/cpp/src/orchestrator.cpp"),
    ("integration_algorithms", "INTEGRATION_ALGORITHMS.md", ("状态枚举取值",),
     "docs/algorithms/INTEGRATION_ALGORITHMS.md"),
    ("integrate_h", "integrate.h", ("P2_INTEGRATE",), "lib/phase2/include/astro/phase2/integrate.h"),
    ("rejection_algorithms", "REJECTION_ALGORITHMS.md", ("状态/原因枚举取值",),
     "docs/algorithms/REJECTION_ALGORITHMS.md"),
    ("rejection_h", "rejection.h", ("P2_STATUS",), "lib/phase2/include/astro/phase2/rejection.h"),
    ("noise_model_doc", "NOISE_MODEL.md", ("1.482602218505602",), "docs/science/NOISE_MODEL.md"),
    ("psf_doc", "PSF.md", ("robust_residual_sigma",), "docs/science/PSF.md"),
    ("noise_model_cpp", "noise_model.cpp", ("kTrimMeanToSigma",),
     "lib/snr_estimator/cpp/src/noise_model.cpp"),
    ("aio_hips_h", "aio_hips.h", ("AIO_HIPS_PRODUCT_VARIANCE",),
     "lib/astro_image_io/include/aio_hips.h"),
    ("drizzle_doc", "DRIZZLE.md", ("sumVarNum",), "docs/science/DRIZZLE.md"),
    ("drizzle_engine_h", "drizzle_engine.h", ("sumVarNum",),
     "lib/healpix_db/healpix_drizzle/drizzle_engine.h"),
]


# ---------------------------------------------------------------------------
# 通用工具
# ---------------------------------------------------------------------------
def check(name: str, ok: bool, detail: str) -> dict:
    return {"check": name, "pass": bool(ok), "detail": detail}


def extract_table(txt: str) -> dict:
    """解析 Markdown 「| 名称 | 值 |」枚举表（docs/algorithms 的取值表形态）。"""
    out = {}
    for line in txt.splitlines():
        m = re.match(r"^\|\s*([A-Z][A-Z0-9_]{2,})\s*\|\s*(\d+)\s*\|\s*$", line.strip())
        if m:
            out[m.group(1)] = int(m.group(2))
    return out


def extract_enum(txt: str, block_start=None) -> dict:
    out = {}
    if block_start:
        i = txt.find(block_start)
        if i < 0:
            return {}
        j = txt.find("}", i)
        txt = txt[i:j]
    for m in re.finditer(r"([A-Z][A-Z0-9_]{2,})\s*=\s*(\d+)", txt):
        out[m.group(1)] = int(m.group(2))
    return out


def is_benign(rel: str) -> str:
    for rx, why in BENIGN_SURFACES:
        if rx.search(rel):
            return why
    return ""


def line_is_approx(line: str, full_values) -> bool:
    """允许语境：同行含权威值，或含显式「≈/简写」标记并给出偏差数字。"""
    if any(v in line for v in full_values):
        return True
    if any(mk in line for mk in APPROX_MARKERS):
        return bool(re.search(r"e[-+]?\d\d|[0-9]\.[0-9]{2,}e|相对差|绝对差|简写|截断|旧 4 位|原 4 位", line))
    return False


def scan_constants(resolver: Resolver) -> dict:
    violations, benign, approx = [], [], []
    files = [rel for rel in resolver._tracked()
             if os.path.splitext(rel)[1].lower() in TEXT_EXT
             and not any(s in rel for s in SKIP_PATH_PARTS)]
    scanned = 0
    for rel in files:
        txt = resolver.read(rel)
        if not txt:
            continue
        scanned += 1
        why = is_benign(rel)
        for fam, spec in CONST_FAMILIES.items():
            for rx, shown in spec["truncated"]:
                for i, line in enumerate(txt.splitlines(), 1):
                    if not rx.search(line):
                        continue
                    item = {"family": fam, "file": rel, "line": i,
                            "literal": shown, "text": line.strip()[:180]}
                    if why:
                        benign.append(dict(item, reason=why))
                    elif line_is_approx(line, spec["full"]):
                        approx.append(item)
                    else:
                        violations.append(dict(item, authority=spec["authority"],
                                               decisions=spec["decisions"]))
    return {"violations": violations, "benign": benign, "approx": approx,
            "scanned_files": scanned, "tracked_files": len(files)}


# ---------------------------------------------------------------------------
# 全部检查
# ---------------------------------------------------------------------------
def run_checks(root: str) -> dict:
    r = Resolver(root)
    p = {}
    for key, base, must, legacy in SOURCES:
        p[key] = r.find(base, must, hint=legacy)

    results = []

    cfg_doc = r.read(p["config_schema"])
    cfg_code = r.read(p["stage2_common"])
    results.append(check(
        "config_weight_mode_ivar",
        "weight_mode(auto)" in cfg_doc and 'wm == "auto" || wm == "ivar"' in cfg_code,
        "CONFIG_SCHEMA weight_mode auto/ivar <-> stage2_common parse"))

    sampler_h = r.read(p["sampler_h"])
    data_sem = r.read(p["data_semantics"])
    upm_doc = r.read(p["upm_doc"])
    frame_id_ok = (
        "FNV-1a 64" not in sampler_h and "FNV-1a 64" not in data_sem and
        "由输入路径派生" not in sampler_h and "由输入路径派生" not in data_sem and
        "truncated-64" in sampler_h and "SHA-256" in data_sem and
        "DATA-FRAME-ID-001" in upm_doc)
    results.append(check(
        "frame_id_contract_exact", frame_id_ok,
        "DATA-FRAME-ID-001：SHA-256 truncate；无 FNV/路径派生残留"))

    tax = r.read(p["error_model"])
    orc_h = r.read(p["orchestrator_h"])
    doc_exit = extract_enum(tax)
    code_exit = extract_enum(orc_h, "namespace AstroCsExitCode")
    orc = r.read(p["orchestrator_cpp"])
    results.append(check(
        "error_taxonomy_exit_codes",
        "AstroCsExitCode" in tax and bool(code_exit) and doc_exit == code_exit,
        "ERROR_MODEL 全集合 == orchestrator.h 退出码 (doc=%d code=%d)"
        % (len(doc_exit), len(code_exit))))

    int_h = r.read(p["integrate_h"])
    rej_h = r.read(p["rejection_h"])
    int_doc = extract_table(r.read(p["integration_algorithms"]))
    int_code = {k: v for k, v in extract_enum(int_h).items() if k.startswith("P2_INTEGRATE")}
    results.append(check(
        "integration_status_full_set", bool(int_code) and int_doc == int_code,
        "integration status 全集合 (doc=%d code=%d)" % (len(int_doc), len(int_code))))
    rej_doc = extract_table(r.read(p["rejection_algorithms"]))
    rej_code = {k: v for k, v in extract_enum(rej_h).items()
                if k.startswith("P2_STATUS") or k.startswith("P2_REASON")}
    results.append(check(
        "rejection_status_full_set", bool(rej_code) and rej_doc == rej_code,
        "rejection status 全集合 (doc=%d code=%d)" % (len(rej_doc), len(rej_code))))

    stages = ["P1.READ", "P1.CALIBRATE", "P1.PLATESOLVE", "P1.PSF",
              "P1.PHOTOMETRIC", "P1.NOISE", "P1.DRIZZLE", "P1.HIPS_WRITE",
              "P2.INTEGRATE", "P2.HIPS_WRITE"]
    results.append(check(
        "stage_ids_docs_vs_orchestrator",
        all(s in tax for s in stages) and "stage_name_v2" in orc,
        "stage IDs in ERROR_TAXONOMY <-> orchestrator stage_name_v2"))

    snr_doc = r.read(p["noise_model_doc"])
    psf_doc = r.read(p["psf_doc"])
    nm = r.read(p["noise_model_cpp"])
    snr_ok = ("1.482602218505602" in snr_doc and "1.4826022185" in snr_doc and
              "0.7316727929211932" in psf_doc and
              "0.7316727929211932" in nm and "1.482602218505602" in nm)
    results.append(check(
        "snr_constants", snr_ok,
        "NOISE_MODEL/PSF 权威常数 <-> noise_model.cpp（前缀锚 + 全精度在场）"))

    aio = r.read(p["aio_hips_h"])
    prod_ok = (all(x in data_sem for x in ["signal", "support", "variance", "ivar"]) and
               "AIO_HIPS_PRODUCT_VARIANCE" in aio and "AIO_HIPS_PRODUCT_IVAR" in aio)
    results.append(check(
        "product_contracts", prod_ok,
        "DATA_CONTRACTS products <-> aio_hips.h product flags"))

    drz_doc = r.read(p["drizzle_doc"])
    eng = r.read(p["drizzle_engine_h"])
    results.append(check(
        "drizzle_variance_formula",
        "sumVarNum" in drz_doc and "sumVarNum" in eng and
        "variance_p" in drz_doc and "ivar_p" in drz_doc,
        "DRIZZLE.md variance formula <-> drizzle_engine.h"))

    # ---- 门 10/11：锚存活（fail-closed）+ 同常数多写法 ----
    if r.missing:
        results.append(check(
            "source_paths_alive", False,
            "fail-closed：%d 个必需源文件在当前树解析不到 → %s"
            % (len(r.missing), " | ".join(r.missing))))
    else:
        results.append(check(
            "source_paths_alive", True,
            "%d 个必需源文件全部解析到位（原迁移前路径已在 SOURCES 中登记）" % len(SOURCES)))

    scan = scan_constants(r)
    scan_ok = (not scan["violations"]) and scan["scanned_files"] > 0
    results.append(check(
        "numeric_constants_single_spelling", scan_ok,
        "同常数多写法：判定面违规=%d（扫描 %d 文件）；登记非判定面=%d；≈语境=%d；%s"
        % (len(scan["violations"]), scan["scanned_files"], len(scan["benign"]),
           len(scan["approx"]),
           "扫描面为空 ⇒ fail-closed 判红" if scan["scanned_files"] == 0 else
           "; ".join("%s:%d[%s]=%s" % (v["file"], v["line"], v["family"], v["literal"])
                     for v in scan["violations"][:12]))))

    return {"results": results, "scan": scan, "paths": p, "missing": r.missing}


# ---------------------------------------------------------------------------
# 可执行负例面 1：--self-test（tempfile mini-repo，单向判据自证）
# ---------------------------------------------------------------------------
# self-test 夹具的枚举取值唯一源（表与代码同源生成，避免夹具自身漂移）
MINI_INTEGRATE = [("P2_INTEGRATE_OK", 0), ("P2_INTEGRATE_NO_CANDIDATES", 1),
                  ("P2_INTEGRATE_ALL_REJECTED", 2), ("P2_INTEGRATE_ZERO_VALID_WEIGHT", 3),
                  ("P2_INTEGRATE_INVALID_INPUT", 4)]
MINI_REASON = [("P2_REASON_ACCEPTED", 0), ("P2_REASON_REJECTED_LOW", 1),
               ("P2_REASON_REJECTED_HIGH", 2), ("P2_REASON_UNDERDETERMINED", 3)]
MINI_STATUS = [("P2_STATUS_OK", 0), ("P2_STATUS_MIN_SAMPLES", 1),
               ("P2_STATUS_ALL_REJECTED", 2), ("P2_STATUS_INVALID_INPUT", 3),
               ("P2_STATUS_UNDERDETERMINED", 4), ("P2_STATUS_INVALID_CONFIGURATION", 5),
               ("P2_STATUS_INVALID_METHOD", 6), ("P2_STATUS_INTERNAL_ERROR", 7)]


def _mini_table(rows, title="状态枚举取值") -> str:
    head = ("## 1a %s（唯一事实源）\n\n| 名称 | 值 |\n|---|---|\n" % title)
    return head + "".join("| %s | %d |\n" % r for r in rows)


def _mini_enum(name, rows) -> str:
    return ("enum %s {\n" % name) + "".join("    %s = %d,\n" % r for r in rows) + "};\n"


MINI_FILES = {
    "lib/algorithms/coverage/src/stage2_common.cpp":
        'if (wm == "auto" || wm == "ivar") { }\n',
    "lib/algorithms/coverage/include/astro/phase2/sampler.h":
        "// frame_id = truncated-64 SHA-256\n",
    "lib/algorithms/coverage/include/astro/phase2/integrate.h":
        _mini_enum("P2IntegrateStatus", MINI_INTEGRATE),
    "lib/algorithms/coverage/include/astro/phase2/rejection.h":
        _mini_enum("P2RejectReason", MINI_REASON) + _mini_enum("P2RejectStatus", MINI_STATUS),
    "lib/infrastructure/pipeline/orchestrator/cpp/include/orchestrator.h":
        "namespace AstroCsExitCode {\n constexpr int SUCCESS = 0;\n"
        " constexpr int GENERIC_ERROR = 1;\n}\n",
    "lib/infrastructure/pipeline/orchestrator/cpp/src/orchestrator.cpp":
        'const char* stage_name_v2(int) { return "P1.READ"; }\n',
    "lib/infrastructure/aio/include/aio_hips.h":
        "#define AIO_HIPS_PRODUCT_VARIANCE 1\n#define AIO_HIPS_PRODUCT_IVAR 2\n",
    "lib/algorithms/drizzle/healpix_drizzle/drizzle_engine.h": "// sumVarNum accumulator\n",
    "lib/algorithms/noise_snr/cpp/src/noise_model.cpp":
        "constexpr double kTrimMeanToSigma = 0.7316727929211932;\n"
        "constexpr double kMadToSigma = 1.482602218505602;\n",
    "docs/development/CONFIG_SCHEMA.md": "# weight_mode(auto)\n",
    "docs/contracts/DATA_SEMANTICS.md": "frame_id SHA-256\n| signal | support | variance | ivar |\n",
    "docs/science/PHASE2_UPM.md": "DATA-FRAME-ID-001 control cell\n",
    "docs/architecture/ERROR_MODEL.md":
        "AstroCsExitCode\n SUCCESS=0  GENERIC_ERROR=1\n"
        "P1.READ P1.CALIBRATE P1.PLATESOLVE P1.PSF P1.PHOTOMETRIC P1.NOISE "
        "P1.DRIZZLE P1.HIPS_WRITE P2.INTEGRATE P2.HIPS_WRITE\n",
    "docs/algorithms/INTEGRATION_ALGORITHMS.md": _mini_table(MINI_INTEGRATE),
    "docs/algorithms/REJECTION_ALGORITHMS.md":
        _mini_table(MINI_REASON + MINI_STATUS, title="状态/原因枚举取值"),
    "docs/science/NOISE_MODEL.md":
        "MAD 常数 1.482602218505602（11 位简写 1.4826022185 只能在约等于语境，"
        "与之相对差 3.779e-12）\n",
    "docs/science/PSF.md": "| robust_residual_sigma | residual_scale/0.7316727929211932 |\n",
    "docs/science/DRIZZLE.md": "variance_p ivar_p sumVarNum\n",
    "lib/algorithms/coverage/src/sampler.cpp": "return 1.482602218505602 * median_of(v);\n",
}
SAMPLER = "lib/algorithms/coverage/src/sampler.cpp"


def _mini_repo(base: str, sampler_body=None) -> str:
    for rel, body in MINI_FILES.items():
        fp = os.path.join(base, rel)
        os.makedirs(os.path.dirname(fp), exist_ok=True)
        with open(fp, "w", encoding="utf-8") as f:
            f.write(body)
    if sampler_body is not None:
        with open(os.path.join(base, SAMPLER), "w", encoding="utf-8") as f:
            f.write(sampler_body)
    subprocess.run(["git", "init", "-q"], cwd=base, capture_output=True, timeout=120)
    subprocess.run(["git", "add", "-A"], cwd=base, capture_output=True, timeout=300)
    return base


def _reds(res):
    return [x["check"] for x in res["results"] if not x["pass"]]


def self_test() -> int:
    cases = []
    tmp = tempfile.mkdtemp(prefix="w4a6_selftest_")
    try:
        # A 正例：迁移后布局（coverage/、infrastructure/pipeline）+ 全精度 ⇒ 全绿
        res = run_checks(_mini_repo(os.path.join(tmp, "ok")))
        cases.append(("A_positive_migrated_layout_green", not _reds(res), _reds(res)))

        # B 负例：截断值裸写 ⇒ 同常数门必红
        res = run_checks(_mini_repo(os.path.join(tmp, "bad"),
                                    "return 1.4826 * median_of(v);\n"))
        cases.append(("B_truncated_literal_red",
                      "numeric_constants_single_spelling" in _reds(res), _reds(res)))

        # C 负例：必需源文件缺失 ⇒ fail-closed 判红（不崩溃静默）
        repo = _mini_repo(os.path.join(tmp, "gone"))
        os.remove(os.path.join(repo, "lib/algorithms/noise_snr/cpp/src/noise_model.cpp"))
        try:
            res = run_checks(repo)
            cases.append(("C_missing_source_red", "source_paths_alive" in _reds(res), _reds(res)))
        except Exception as exc:
            cases.append(("C_missing_source_red", False, "raised %r" % (exc,)))

        # D 负例：扫描面为空 ⇒ fail-closed 判红
        empty = os.path.join(tmp, "empty")
        os.makedirs(empty, exist_ok=True)
        try:
            res = run_checks(empty)
            cases.append(("D_empty_scan_red", bool(_reds(res)), _reds(res)))
        except Exception as exc:
            cases.append(("D_empty_scan_red", False, "raised %r" % (exc,)))

        # E 负例：≈/简写语境（截断 + 权威值 + 偏差）不得误红
        res = run_checks(_mini_repo(
            os.path.join(tmp, "approx"),
            "// 旧 4 位截断 1.4826 vs 权威 1.482602218505602 相对差 -1.50e-6\n"
            "return 1.482602218505602 * median_of(v);\n"))
        cases.append(("E_approx_context_green",
                      "numeric_constants_single_spelling" not in _reds(res), _reds(res)))

        # F 负例：登记的非判定面（归档）不得判红
        repo = _mini_repo(os.path.join(tmp, "benign"))
        os.makedirs(os.path.join(repo, "docs/archive"), exist_ok=True)
        with open(os.path.join(repo, "docs/archive/old.md"), "w", encoding="utf-8") as f:
            f.write("sigma = 1.4826 * MAD\n")
        subprocess.run(["git", "add", "-A"], cwd=repo, capture_output=True, timeout=120)
        res = run_checks(repo)
        cases.append(("F_benign_surface_green",
                      "numeric_constants_single_spelling" not in _reds(res), _reds(res)))

        # G 负例：三族截断全覆盖（0.6745 / 0.7316728 / 11 位简写）
        res = run_checks(_mini_repo(
            os.path.join(tmp, "fam"),
            "sigma = 0.6745 * mad;\npsf = 0.7316728 * rs;\nbg = 1.4826022185 * mad2;\n"))
        fams = sorted({v["family"] for v in res["scan"]["violations"]})
        cases.append(("G_all_families_red",
                      "numeric_constants_single_spelling" in _reds(res) and
                      fams == ["mad_to_sigma", "sigma_to_mad", "trimmean_to_sigma"],
                      {"reds": _reds(res), "families": fams}))
    finally:
        shutil.rmtree(tmp, ignore_errors=True)

    failed = [c for c in cases if not c[1]]
    print(json.dumps({"tool": "docs_machine_consistency", "mode": "self-test",
                      "cases": [{"case": c, "pass": ok, "detail": d} for c, ok, d in cases],
                      "pass": not failed}, ensure_ascii=False, indent=1))
    return 0 if not failed else 1


# ---------------------------------------------------------------------------
# 可执行负例面 2：--fault-inject（真仓副本注入，必须判红）
# ---------------------------------------------------------------------------
INJECTIONS = {
    "truncate-mad-to-sigma": (
        "lib/algorithms/photometry/cpp/src/star_matcher.cpp",
        "static constexpr double _MAD_SCALE = 0.6744897501960817;",
        "static constexpr double _MAD_SCALE = 0.6745;"),
    "truncate-inv-mad": (
        "lib/algorithms/coverage/src/sampler.cpp",
        "return 1.482602218505602 * median_of(std::move(v));",
        "return 1.4826 * median_of(std::move(v));"),
    "truncate-psf-trimmean": (
        "lib/algorithms/noise_snr/cpp/src/noise_model.cpp",
        "constexpr double kTrimMeanToSigma = 0.7316727929211932;",
        "constexpr double kTrimMeanToSigma = 0.7316728;"),
    "drop-source-file": ("lib/infrastructure/aio/include/aio_hips.h", None, None),
}


def _stage_copy(src: str, dst: str) -> str:
    """只物化 **tracked** 文件（git archive），既快又不把 run/ 与 build/ 拖进判据面。"""
    os.makedirs(dst, exist_ok=True)
    r = subprocess.run(["git", "archive", "--format=tar", "HEAD"], cwd=src,
                       stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=600)
    if r.returncode != 0 or not r.stdout:
        raise RuntimeError("git archive 失败: %s" % r.stderr.decode("utf-8", "replace")[:400])
    subprocess.run(["tar", "-xf", "-", "-C", dst], input=r.stdout, timeout=900, check=True)
    # 工作区里未提交的改动也要带上（本门要判当前工作区，而不是 HEAD）
    st = subprocess.run(["git", "status", "--porcelain"], cwd=src, capture_output=True,
                        text=True, timeout=300)
    for line in st.stdout.splitlines():
        if len(line) < 4:
            continue
        code, rel = line[:2], line[3:].strip().strip('"')
        if code.strip() == "D":
            fp = os.path.join(dst, rel)
            if os.path.exists(fp):
                os.remove(fp)
            continue
        if code.strip() not in ("M", "A", "R", "??", "AM", "MM"):
            continue
        src_fp = os.path.join(src, rel.split(" -> ")[-1])
        dst_fp = os.path.join(dst, rel.split(" -> ")[-1])
        if os.path.isfile(src_fp):
            os.makedirs(os.path.dirname(dst_fp), exist_ok=True)
            shutil.copy2(src_fp, dst_fp)
    subprocess.run(["git", "init", "-q"], cwd=dst, capture_output=True, timeout=120)
    subprocess.run(["git", "add", "-A"], cwd=dst, capture_output=True, timeout=600)
    return dst


def fault_inject(name: str, root: str) -> int:
    if name not in INJECTIONS:
        print(json.dumps({"error": "unknown fault-inject name", "known": sorted(INJECTIONS)},
                         ensure_ascii=False))
        return 2
    rel, old, new = INJECTIONS[name]
    tmp = tempfile.mkdtemp(prefix="w4a6_faultinject_")
    repo = os.path.join(tmp, "repo")
    try:
        _stage_copy(root, repo)
        target = os.path.join(repo, rel)
        if old is None:
            if os.path.exists(target):
                os.remove(target)
        else:
            with open(target, encoding="utf-8") as f:
                txt = f.read()
            if txt.count(old) != 1:
                print(json.dumps({"error": "injection anchor not unique", "file": rel,
                                  "count": txt.count(old)}, ensure_ascii=False))
                return 2
            with open(target, "w", encoding="utf-8") as f:
                f.write(txt.replace(old, new, 1))
        res = run_checks(repo)
        reds = _reds(res)
        print(json.dumps({"tool": "docs_machine_consistency", "mode": "fault-inject",
                          "injection": name, "file": rel,
                          "injected": ("deleted" if old is None else new),
                          "red_checks": reds, "pass": bool(reds)},
                         ensure_ascii=False, indent=1))
        return 0 if reds else 1
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


# ---------------------------------------------------------------------------
def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description="docs <-> code machine consistency")
    ap.add_argument("--root", default=os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
                    help="仓库根（默认本文件上一级）")
    ap.add_argument("--json-out", default="",
                    help="报告落盘路径（默认不写文件：本工具不再无条件覆盖已跟踪证据件）")
    ap.add_argument("--quiet", action="store_true", help="只打印汇总行")
    ap.add_argument("--self-test", action="store_true", help="mini-repo 正/负例自证")
    ap.add_argument("--fault-inject", default="", metavar="NAME",
                    help="真仓副本注入后必须判红：%s" % ", ".join(sorted(INJECTIONS)))
    args = ap.parse_args(argv)

    if args.self_test:
        return self_test()
    root = os.path.abspath(args.root)
    if args.fault_inject:
        return fault_inject(args.fault_inject, root)
    if not (os.path.isdir(os.path.join(root, "docs")) and os.path.isdir(os.path.join(root, "lib"))):
        print(json.dumps({"tool": "docs_machine_consistency", "pass": False,
                          "error": "root 不像仓库（缺 docs/ 或 lib/）", "root": root},
                         ensure_ascii=False))
        return 1

    out = run_checks(root)
    results = out["results"]
    scan = out["scan"]
    report = {
        "tool": "docs_machine_consistency",
        "version": "2.0.0",
        "root": root,
        "paths": out["paths"],
        "checks": results,
        "numeric_constants": {
            "violations": scan["violations"],
            "benign_surfaces": scan["benign"],
            "approx_context": scan["approx"],
            "scanned_files": scan["scanned_files"],
        },
        "pass": all(x["pass"] for x in results),
    }
    if args.json_out:
        d = os.path.dirname(os.path.abspath(args.json_out))
        if d:
            os.makedirs(d, exist_ok=True)
        with open(args.json_out, "w", encoding="utf-8") as f:
            json.dump(report, f, ensure_ascii=False, indent=1)
        print("REPORT_WRITTEN %s" % args.json_out)
    if args.quiet:
        bad = [x["check"] for x in results if not x["pass"]]
        print("DOCS_MACHINE_CONSISTENCY %s checks=%d failed=%d %s"
              % ("PASS" if report["pass"] else "FAIL", len(results), len(bad), ",".join(bad)))
    else:
        print(json.dumps(report, ensure_ascii=False, indent=1))
    return 0 if report["pass"] else 1


if __name__ == "__main__":
    sys.exit(main())
