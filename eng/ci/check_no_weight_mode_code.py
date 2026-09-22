#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""CHK-NO-WEIGHT-MODE-CODE：代码面「单一权重口径」门（FZ-WEIGHT-SINGLE-PATH）。

权威依据
  - `ASTROCS_DESIGN.md` §3.1：全程只有 SNR，没有"权重模式"这个概念；权重是 Phase2
    集成时按天球像素对应的输入帧集合**现场计算的派生量**；Phase1 与 Phase3 不产生、
    不消费权重。
  - `docs/science/PSF_SIGNAL_WEIGHT.md` §4（单一权重口径，没有可选择项）：阶段一产
    稀疏 SNR 控制点 → 阶段二重建稠密 SNR 面 → 取逆方差（最优功率）定权 → 叠加；
    `w(x,y) = SNR(x,y)^2 / F_ref^2 = 1 / sigma_F(x,y)^2`。
  - `docs/science/UNIFIED_SCIENCE_MODEL.md` §4.1。
  - `docs/design/UNIFIED_MODEL.md:58`（退役对象 `psfsw_robust_weight` 的显式拒绝面）。
  - `ENGINEERING_SPEC.md` §8（每项检查有正例与负例、能红能绿；fail-closed；锚存活）。

判据（确定性）
  C1  模式选择机制不存在：`lib/**` + `eng/tools/**` 的**代码行**（去注释）不得出现
      模式枚举定义、模式解析/路由函数、模式集合 API 或"生产模式列表"成员。
      （`eng/ci/**` 不在此扫描面：`eng/ci/check_no_weight_mode.py` 自身按名自指。）
  C2  退役对象的**拒绝面**必须存活（收紧不是删除）：`psfsw_robust_weight` / `psfsw`
      仍在禁止权重来源词表里，且显式拒绝 API 与 FZ-MODE-RETIRED 理由在位。
  C3  单一口径的**逆方差链路在位**（非退化判据）：稀疏控制点重建器、逆方差权重链、
      生产集成调用点、以及"稀疏层尚未接入生产数据面"的显式登记必须同时存在；
      链路任一环被删 ⇒ 判红（不是"没有模式即通过"）。
  C4  fail-closed：扫描面缺失、锚文件缺失 ⇒ rc=2；不得把"文件不存在"当"无违规"。
  C5  负例面可执行：`--self-test` 在临时树上证明「注入模式选择 ⇒ 红」「拆除链路 ⇒ 红」
      「干净树 ⇒ 绿」。

用法
  python3 eng/ci/check_no_weight_mode_code.py                 # 扫真实仓库，rc=0 全绿
  python3 eng/ci/check_no_weight_mode_code.py --json-out F    # 机器可读结果（原子写）
  python3 eng/ci/check_no_weight_mode_code.py --self-test     # 临时目录正例/负例
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

SCAN_DIRS = ("lib", "eng/tools")
TEXT_EXT = {".h", ".hpp", ".hh", ".c", ".cc", ".cpp", ".cxx", ".py"}

# ── C1 模式选择机制的代码面形态（模式是**声明**，不是注释里的提及）────────────
# 每条 = (规则名, 正则, 说明)。只在**去注释后的代码文本**上匹配。
MODE_SELECTION_PATTERNS = (
    ("mode-enum-decl",
     re.compile(r"enum\s+class\s+\w*WeightMode\b"),
     "权重口径枚举定义（模式选择机制的载体）"),
    ("mode-enum-arm",
     re.compile(r"\bkSurfaceGls\b|\bkPsfswRobust\b"),
     "权重口径枚举成员（模式选择机制的载体）"),
    # 口径 token 面的路由函数合法名字是 route_phase2_weight_token（只做 fail-closed
    # 判定与 baseline 登记）；下列名字是"模式选择机制"的载体，一律不得复活。
    ("mode-route-api",
     re.compile(r"\broute_phase2_mode\s*\(|\broute_weight_mode\s*\(|"
                r"\bparse_weight_mode\s*\(|\broute_phase2_weight_mode\s*\("),
     "权重口径解析/路由函数（模式选择机制的载体）"),
    ("mode-token-api",
     re.compile(r"\bweight_mode_token\s*\(|\bphase2_type_id\s*\("),
     "权重口径 → 产品身份映射函数"),
    ("mode-gate-api",
     re.compile(r"\bp2_weight_mode_check\b"),
     "权重口径门（coverage 生产门）"),
    ("mode-set-api",
     re.compile(r"\bproduction_weight_modes\s*\(|\bis_production_weight_mode\s*\("),
     "生产权重口径集合 API"),
    ("mode-list-member",
     re.compile(r"\bproduction_modes\b"),
     "记录里的生产模式列表成员"),
)

# ── C2 退役对象拒绝面（必须存活）──────────────────────────────────────────────
REJECTION_ANCHORS = (
    ("lib/algorithms/coverage/src/coverage.cpp",
     ("psfsw_robust_weight", "FZ-MODE-RETIRED"),
     "禁止权重来源词表里的退役对象 token + 显式拒绝理由"),
    ("lib/algorithms/integration/v6/src/phase2_integrate.cpp",
     ("is_retired_weight_mode_token", "FZ-MODE-RETIRED"),
     "产品校验面的退役对象识别 + 显式拒绝理由"),
    ("lib/algorithms/photometry/cpp/src/psfsw.cpp",
     ("is_retired_weight_mode_token", "retired_weight_mode_reject_reason"),
     "psfsw 记录族的退役对象识别 + 迁移提示"),
)

# ── C3 单一口径的逆方差链路（必须同时存活）────────────────────────────────────
CHAIN_ANCHORS = (
    ("lib/algorithms/integration/v6/include/astrocs/v6/weight_chain.h",
     ("SparseSnrReconstructor", "reconstruct_sparse_snr",
      "compute_inverse_variance_weights", "weight_from_snr"),
     "稀疏控制点重建器 + 逆方差权重链 API"),
    ("lib/algorithms/integration/v6/src/weight_chain.cpp",
     ("SparseSnrReconstructor::eval", "compute_inverse_variance_weights",
      "weight_from_snr"),
     "重建器与逆方差权重链实现"),
    ("lib/infrastructure/scheduler/src/module_adapters.cpp",
     ("compute_inverse_variance_weights", "AIO_HIPS_RD_IVAR",
      "稀疏 SNR 层尚未接入生产数据面"),
     "生产集成调用点：逐样本 ivar 优先，缺 ivar 时走 SNR → 逆方差链"),
    ("lib/algorithms/integration/v6/src/phase2_integrate.cpp",
     ("p2_weight_source_token_reject", "FZ-WEIGHT-SINGLE-PATH"),
     "集成层的权重来源门 + 单一权重口径锚"),
)


def strip_comments(text: str) -> str:
    """去掉行注释与块注释，只留代码面（避免把注释里的历史符号名当活代码）。"""
    text = re.sub(r"/\*.*?\*/", " ", text, flags=re.S)
    out = []
    for line in text.splitlines():
        s = line.lstrip()
        if s.startswith("//") or s.startswith("#"):
            continue
        out.append(line)
    return "\n".join(out)


def iter_files(root):
    files = []
    for d in SCAN_DIRS:
        base = os.path.join(root, d)
        if not os.path.isdir(base):
            return None
        for dirpath, dirnames, filenames in os.walk(base):
            dirnames[:] = sorted(x for x in dirnames
                                 if x not in {".git", "build", "__pycache__",
                                              "node_modules", "third_party"})
            for name in sorted(filenames):
                if os.path.splitext(name)[1].lower() in TEXT_EXT:
                    files.append(os.path.join(dirpath, name))
    return files


def scan_code_face(root):
    """C1：返回 (findings, scanned_files)。findings 为稳定排序的 dict 列表。"""
    files = iter_files(root)
    if files is None:
        return None, 0
    findings = []
    for path in files:
        try:
            with open(path, encoding="utf-8", errors="replace") as fh:
                raw = fh.read()
        except OSError:
            continue
        code = strip_comments(raw)
        for rule, rx, why in MODE_SELECTION_PATTERNS:
            for m in rx.finditer(code):
                line_no = code.count("\n", 0, m.start()) + 1
                findings.append({"rule": rule, "path": os.path.relpath(path, root),
                                 "line": line_no, "token": m.group(0), "why": why})
    findings.sort(key=lambda f: (f["path"], f["line"], f["rule"]))
    return findings, len(files)


def check_anchors(root, anchors):
    """C2/C3：锚存活检查。缺文件或缺符号 ⇒ finding（fail-closed）。"""
    findings = []
    for rel, needles, why in anchors:
        path = os.path.join(root, rel)
        if not os.path.isfile(path):
            findings.append({"rule": "ANCHOR-MISSING", "path": rel, "token": "-",
                             "why": "锚文件不存在（fail-closed）：" + why})
            continue
        with open(path, encoding="utf-8", errors="replace") as fh:
            text = fh.read()
        for n in needles:
            if n not in text:
                findings.append({"rule": "ANCHOR-STALE", "path": rel, "token": n,
                                 "why": "锚符号缺失（fail-closed）：" + why})
    return findings


def evaluate(root):
    code_findings, n_files = scan_code_face(root)
    if code_findings is None:
        return None
    findings = list(code_findings)
    findings += check_anchors(root, REJECTION_ANCHORS)
    findings += check_anchors(root, CHAIN_ANCHORS)
    return findings, n_files


# ────────────────────────────────────────────────────────── self-test（能红能绿）────
_SELFTEST_LIB = {
    "lib/algorithms/integration/v6/include/astrocs/v6/weight_chain.h":
        "struct SparseSnrReconstructor { double eval(double, double, double*, void*, void*); };\n"
        "bool reconstruct_sparse_snr(const void*, double, double, double*, void*, void*);\n"
        "void compute_inverse_variance_weights();\n"
        "bool weight_from_snr(double, double, double*, void*);\n",
    "lib/algorithms/integration/v6/src/weight_chain.cpp":
        "double SparseSnrReconstructor::eval(double a, double b, double* c, void* d, void* e) { return 0; }\n"
        "void compute_inverse_variance_weights() {}\n"
        "bool weight_from_snr(double, double, double*, void*) { return true; }\n",
    "lib/infrastructure/scheduler/src/module_adapters.cpp":
        "// compute_inverse_variance_weights\n"
        "void f() { compute_inverse_variance_weights(); }\n"
        "const char* k = \"AIO_HIPS_RD_IVAR\";\n"
        "// 稀疏 SNR 层尚未接入生产数据面\n",
    "lib/algorithms/integration/v6/src/phase2_integrate.cpp":
        "void p2_weight_source_token_reject();\n"
        "const char* a = \"FZ-WEIGHT-SINGLE-PATH\";\n"
        "bool is_retired_weight_mode_token(const char*) { return false; }\n"
        "const char* b = \"FZ-MODE-RETIRED\";\n",
    "lib/algorithms/coverage/src/coverage.cpp":
        "const char* t[] = {\"psfsw_robust_weight\", \"psfsw\"};\n"
        "const char* r = \"FZ-MODE-RETIRED\";\n",
    "lib/algorithms/photometry/cpp/src/psfsw.cpp":
        "bool is_retired_weight_mode_token(const char*) { return false; }\n"
        "const char* retired_weight_mode_reject_reason(const char*) { return \"\"; }\n",
}


def _write_tree(root, lib_files):
    for rel, text in lib_files.items():
        path = os.path.join(root, rel)
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, "w", encoding="utf-8") as fh:
            fh.write(text)
    # 扫描面必须存在（C4 fail-closed 的正例面）
    os.makedirs(os.path.join(root, "eng", "tools"), exist_ok=True)
    with open(os.path.join(root, "eng", "tools", "placeholder.py"), "w",
              encoding="utf-8") as fh:
        fh.write("# placeholder\n")


def self_test():
    cases = []
    problems = []
    tmp = tempfile.mkdtemp(prefix="chk-no-weight-mode-code-")
    try:
        # 正例（绿）：链路在位、无模式选择机制
        ok_root = os.path.join(tmp, "ok")
        _write_tree(ok_root, _SELFTEST_LIB)
        res = evaluate(ok_root)
        cases.append(("clean-tree", 0 if res and not res[0] else 1, 0))

        # 负例 1（红）：注入模式选择机制
        bad_root = os.path.join(tmp, "bad_enum")
        files = dict(_SELFTEST_LIB)
        files["lib/algorithms/integration/v6/src/phase2_integrate.cpp"] += (
            "enum class WeightMode : int { kPointInformation = 0, kSurfaceGls = 1 };\n"
            "int route_weight_mode(const char*, void*, char*, unsigned long) { return 0; }\n"
            "int p2_weight_mode_check(const char*, char*, unsigned long) { return 0; }\n"
            "const void& production_weight_modes();\n"
            "struct R { int production_modes; };\n")
        _write_tree(bad_root, files)
        res = evaluate(bad_root)
        cases.append(("inject-mode-selection", 1 if res and res[0] else 0, 1))

        # 负例 2（红）：拆除逆方差链路（重建器被删）
        broken_root = os.path.join(tmp, "bad_chain")
        files = dict(_SELFTEST_LIB)
        files["lib/algorithms/integration/v6/src/weight_chain.cpp"] = (
            "void compute_inverse_variance_weights() {}\n"
            "bool weight_from_snr(double, double, double*, void*) { return true; }\n")
        _write_tree(broken_root, files)
        res = evaluate(broken_root)
        cases.append(("remove-inverse-variance-chain", 1 if res and res[0] else 0, 1))

        # 负例 3（红）：退役对象拒绝面被删
        noret_root = os.path.join(tmp, "bad_reject")
        files = dict(_SELFTEST_LIB)
        files["lib/algorithms/coverage/src/coverage.cpp"] = "int x = 0;\n"
        _write_tree(noret_root, files)
        res = evaluate(noret_root)
        cases.append(("remove-retired-reject-surface", 1 if res and res[0] else 0, 1))

        # 负例 4（红）：扫描面缺失 ⇒ fail-closed（rc=2）
        missing_root = os.path.join(tmp, "missing")
        os.makedirs(missing_root, exist_ok=True)
        cases.append(("missing-scan-face", 2 if evaluate(missing_root) is None else 0, 2))
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


def run(root, json_out):
    res = evaluate(root)
    if res is None:
        msg = ("CHK-NO-WEIGHT-MODE-CODE_FAIL: 扫描面不可用（缺 %s）—— fail-closed"
               % " 或 ".join(SCAN_DIRS))
        print(msg, file=sys.stderr)
        if json_out:
            _emit(json_out, {"tool": "check_no_weight_mode_code", "ok": False,
                             "fail_closed": True, "reason": msg})
        return 2
    findings, n_files = res
    if json_out:
        _emit(json_out, {"tool": "check_no_weight_mode_code", "ok": not findings,
                         "scanned_files": n_files, "findings": findings})
    if findings:
        print("CHK-NO-WEIGHT-MODE-CODE_FAIL: %d 处违规（FZ-WEIGHT-SINGLE-PATH）"
              % len(findings), file=sys.stderr)
        for f in findings[:40]:
            print("  %s:%s [%s] %s — %s"
                  % (f["path"], f.get("line", "-"), f["rule"], f["token"], f["why"]),
                  file=sys.stderr)
        return 1
    print("CHK-NO-WEIGHT-MODE-CODE_PASS: files=%d 无模式选择机制、逆方差链路在位"
          % n_files)
    return 0


def _emit(path, doc):
    os.makedirs(os.path.dirname(os.path.abspath(path)), exist_ok=True)
    tmp = path + ".tmp"
    with open(tmp, "w", encoding="utf-8") as fh:
        json.dump(doc, fh, ensure_ascii=False, indent=1, sort_keys=True)
        fh.write("\n")
    os.replace(tmp, path)


def main(argv=None):
    ap = argparse.ArgumentParser(
        description="FZ-WEIGHT-SINGLE-PATH 代码面门（无模式选择机制 + 逆方差链路在位）")
    ap.add_argument("--root", default=".", help="仓库根（默认 .）")
    ap.add_argument("--json-out", default=None, help="机器可读结果输出路径（原子写）")
    ap.add_argument("--self-test", action="store_true", help="临时目录正例/负例自检")
    args = ap.parse_args(argv)
    if args.self_test:
        return self_test()
    return run(args.root, args.json_out)


if __name__ == "__main__":
    sys.exit(main())
