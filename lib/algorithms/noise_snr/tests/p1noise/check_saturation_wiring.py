#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""SAT-001 生产接线门：饱和过滤必须在默认配置下**生效或显式声明**（claim SC-008）。

判据（全部可失败；负例注入见 run/PROJECT-GOVERNANCE-01/SAT-001/自证摘要.md §4）:
  W1 eng/packaging/config/defaults.json 登记 noise.saturation_level，source_ref 指向 SCI「饱和域」行，
     且 constraint 含「未提供」语义（禁止把 0 登记成「禁用」的默认值）
  W2 SCI NOISE_MODEL 目标行含「饱和域」与 NOISE_SATURATION_FILTER（显式声明契约）
  W3 生产接线 orchestrator.cpp：解析 SATURATE/DATAMAX → ncfg.saturation_level，
     并写 NOISE_SATURATION_LEVEL/FILTER/SOURCE 显式声明
  W4 模块文档/头文件不得再把 saturation_level=0 描述为「禁用」（消除静默关闭语义）

Exit: 0 PASS, 1 FAIL, 2 env error
"""
import argparse
import json
import pathlib
import re
import sys


def find_repo(start: pathlib.Path) -> pathlib.Path:
    for p in [start, *start.parents]:
        # BLD-401: 路径随 2026-09-21 根目录整合订正（config/ → eng/packaging/config/）。
        # 旧锚点在现行树恒不命中 ⇒ find_repo 回退到脚本自身目录 ⇒ W1/W2/W3 全部
        # 以"文件不存在"失败（判据未真正执行）。
        if (p / "eng" / "packaging" / "config" / "defaults.json").exists() \
                and (p / "docs" / "science").is_dir():
            return p
    return start


_ap = argparse.ArgumentParser(add_help=True)
_ap.add_argument("--root", default=None,
                 help="仓库根（默认自动定位；负例注入时指向隔离副本）")
_args = _ap.parse_args()
REPO = pathlib.Path(_args.root).resolve() if _args.root else find_repo(
    pathlib.Path(__file__).resolve().parent)
findings = []


def fail(cid, msg):
    findings.append((cid, msg))


# ---- W1: defaults.json 登记 ----
try:
    # 2026-09-21 根目录整合：config/ → eng/packaging/config/。
    defaults = json.loads((REPO / "eng" / "packaging" / "config"
                           / "defaults.json").read_text(encoding="utf-8"))
    fields = {f["key"]: f for f in defaults["fields"]}
    sat = fields.get("noise.saturation_level")
    if sat is None:
        fail("W1-MISSING", "eng/packaging/config/defaults.json 未登记 noise.saturation_level")
    else:
        constraint = sat.get("constraint", "")
        if ("未提供" not in constraint) and ("unset" not in constraint):
            fail("W1-SEMANTICS", "noise.saturation_level constraint 未声明 0=未提供(unset) 语义")
        if "NOISE_SATURATION_FILTER" not in constraint:
            fail("W1-DECL", "noise.saturation_level constraint 未要求显式降级声明 NOISE_SATURATION_FILTER")
        ref = sat.get("source_ref") or {}
        ref_path = ref.get("path")
        ref_line = ref.get("line")
        if not ref_path or not ref_line:
            fail("W1-REF", "noise.saturation_level 缺 source_ref（禁止无权威数值/语义）")
        else:
            doc = REPO / ref_path
            if not doc.exists():
                fail("W1-REF-FILE", f"source_ref 文件不存在: {ref_path}")
            else:
                lines = doc.read_text(encoding="utf-8").splitlines()
                if not (1 <= ref_line <= len(lines)) or "饱和域" not in lines[ref_line - 1]:
                    fail("W1-REF-LINE", f"source_ref {ref_path}:{ref_line} 未落在 SCI「饱和域」条款行")
except Exception as exc:  # noqa: BLE001
    fail("W1-EXC", f"defaults.json 读取/解析失败: {exc}")

# ---- W2: SCI 显式声明契约 ----
try:
    sci = (REPO / "docs" / "science" / "NOISE_MODEL.md").read_text(encoding="utf-8")
    if "NOISE_SATURATION_FILTER" not in sci:
        fail("W2-DECL", "SCI NOISE_MODEL 未定义饱和过滤状态的显式声明键 NOISE_SATURATION_FILTER")
    if "饱和域" not in sci:
        fail("W2-DOMAIN", "SCI NOISE_MODEL 缺「饱和域」输入有效域条款")
    if "未提供" not in sci:
        fail("W2-UNSET", "SCI NOISE_MODEL 未定义 saturation_level=0 的「未提供电平」语义")
except Exception as exc:  # noqa: BLE001
    fail("W2-EXC", f"SCI 读取失败: {exc}")

# ---- W3: 生产接线 ----
try:
    orch = (REPO / "lib/infrastructure/pipeline/orchestrator/cpp/src/orchestrator.cpp").read_text(
        encoding="utf-8", errors="ignore")
    for token, cid in (('fn_kv_get(frame_, "header", "SATURATE")', "W3-SATURATE"),
                       ('fn_kv_get(frame_, "header", "DATAMAX")', "W3-DATAMAX"),
                       ("NOISE_SATURATION_FILTER", "W3-DECL"),
                       ("NOISE_SATURATION_LEVEL", "W3-LEVEL")):
        if token not in orch:
            fail(cid, f"orchestrator.cpp 缺饱和接线要素: {token}")
    # 关键：解析结果必须**真的赋给 cfg**（仅出现函数名不算接线 —— 死接线必须红）
    if not re.search(r"ncfg\.saturation_level\s*=\s*astrocs::noise::resolve_effective_saturation\s*\(",
                     orch):
        fail("W3-ASSIGN",
             "orchestrator.cpp 未把 resolve_effective_saturation 的结果赋给 ncfg.saturation_level"
             "（恒定 0 / 死接线 = 静默关闭饱和过滤）")
    # 显式声明必须由**同一个实测电平**驱动（不得写死字面量状态串）
    if not re.search(r"saturation_filter_state\s*\(\s*ncfg\.saturation_level\s*\)", orch):
        fail("W3-STATE",
             "orchestrator.cpp 的过滤状态未由 ncfg.saturation_level 驱动"
             "（saturation_filter_state(ncfg.saturation_level) 缺失）")
    if "saturation_policy.h" not in orch:
        fail("W3-INCLUDE", "orchestrator.cpp 未包含 astrocs/noise/saturation_policy.h")
except Exception as exc:  # noqa: BLE001
    fail("W3-EXC", f"orchestrator.cpp 读取失败: {exc}")

# ---- W4: 消除「0=禁用」静默语义 ----
try:
    legacy = re.compile(r"saturation_level[^\n]{0,40}0\s*=\s*禁用")
    for rel in ("lib/algorithms/noise_snr/cpp/include/snr_estimator.h",
                "lib/algorithms/noise_snr/README.md"):
        p = REPO / rel
        if not p.exists():
            continue
        for i, line in enumerate(p.read_text(encoding="utf-8").splitlines(), 1):
            if legacy.search(line):
                fail("W4-LEGACY", f"{rel}:{i} 仍把 saturation_level=0 描述为「禁用」（应写「未提供电平 unset」）")
except Exception as exc:  # noqa: BLE001
    fail("W4-EXC", f"模块文档扫描失败: {exc}")

if findings:
    print("SATURATION_WIRING_GATE_FAIL")
    for cid, msg in findings:
        print(f"  [{cid}] {msg}")
    sys.exit(1)
print("SATURATION_WIRING_GATE_PASS (W1..W4)")
sys.exit(0)
