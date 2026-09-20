#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""P2-001 | Phase2 old Stage2 步骤映射 + ACR 禁止迁移依赖校验（审计 CONFORM-SWEEP-3-017）。

根因（原实现为何死掉）
  原 MAP = docs/refactor/P2_SYMBOL_MAP.md，SRC = lib/phase2/src —— 两条路径均已失效：
    - docs/refactor/ 整目录被 GOV-002（b7b2dea7「docs(governance): GOV-002 归档非当前
      工程文档」）移入 docs/archive/refactor/，文件本身带 ARCHIVED_NON_NORMATIVE 头注；
    - lib/phase2/ 模块按 AGENTS.md §6 并联放置搬迁为 lib/algorithms/coverage/。
  后果：工具裸抛 FileNotFoundError，且从未在 ci/checks.json 注册，映射面长期无门。

判据（任一 R 违规 ⇒ exit 1；输入缺失 ⇒ exit 2，fail-closed）
  R1 anchor_alive       MAP / SRC / INDEX 三条硬编码路径必须存在，失效时报
                        ANCHOR_STALE: <常量名> <路径> 并点名（不 traceback、不静默）。
  R2 map_not_empty      映射表必须解析出 >= 8 条映射行（防扫描面塌缩判绿）。
  R3 mapped_exists      映射表点名的每个 .cpp 必须仍存在于 SRC（防悬空映射）。
  R4 source_classified  SRC 下每个 .cpp 必须「在冻结映射表内」或「在
                        POST_ARCHIVE_ADDITIONS 登记面内（owner/evidence 必填）」。
  R5 acr_forbidden      ACR 相关文件必须在映射表中标「禁止迁移」，且
                        「禁止迁移依赖」「禁止作为迁移依赖」两条声明必须存在。
  R6 sci_registered     映射表引用的每个 SCI-* 必须登记在 docs/contracts/INDEX.yaml。

不覆盖（如实声明）
  归档表是 ARCHIVED_NON_NORMATIVE 历史记录，其内容正确性不再由本门保证；现行模块
  映射由 tools/quality/check_module_map.py + docs/architecture/MODULE_MAP.md 承担。
  本门只保证：历史映射不悬空、ACR 禁止迁移声明不丢失、SCI 引用不悬空、归档后新增
  源必须显式登记。

用法
  python3 tools/check_p2_symbol_map.py [--root .] [--json-out F]
  python3 tools/check_p2_symbol_map.py --self-test
exit 0 = PASS；1 = 判据违规；2 = 输入不可用（fail-closed）。

只读；仅 stdlib；输出稳定排序（无时间戳）。
"""
from __future__ import annotations

import argparse
import json
import os
import re
import shutil
import sys
import tempfile

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

MAP_REL = "docs/archive/refactor/P2_SYMBOL_MAP.md"
SRC_REL = "lib/algorithms/coverage/src"
INDEX_REL = "docs/contracts/INDEX.yaml"

ACR_FILES = ("acr_kernels.cpp", "cuda_bridge_stub.cpp")
ACR_TOKENS = ("acr_kernels.cpp", "cuda_bridge_stub.cpp", "p2_acr_block_eligible")
FORBIDDEN_MARK = "禁止迁移"
FORBIDDEN_DECLS = ("禁止迁移依赖", "禁止作为迁移依赖")
MIN_MAP_ROWS = 8

# 归档后新增源（GOV-002 归档之后进入 lib/algorithms/coverage/src 的文件）：
# 必须显式登记 owner + evidence，否则 R4 判红（禁止静默漏映射）。
POST_ARCHIVE_ADDITIONS = {
    "sky_plane.cpp": {
        "owner": "RELEASE-02 FIX-A（稀疏天光面链）",
        "evidence": ["lib/algorithms/coverage/src/sky_plane.cpp",
                     "lib/algorithms/coverage/include/astro/phase2/sky_plane.h",
                     "docs/architecture/MODULE_MAP.md"],
        "note": "GOV-002 归档（2026-09-02）之后新增；归档映射表为历史记录，不回填，"
                "改由本登记面承接分类责任。",
    },
}

_ROW = re.compile(r"^\|.+\|\s*$")
_SRC_FILE = re.compile(r"`([A-Za-z0-9_]+\.[a-z]{1,4})`")
_SCI = re.compile(r"\b(SCI-[A-Z0-9-]+)\b")


def fail(code, detail):
    return {"code": code, "detail": detail}


def read_text(path):
    with open(path, encoding="utf-8") as fh:
        return fh.read()


def check_root(root, additions=None):
    additions = POST_ARCHIVE_ADDITIONS if additions is None else additions
    root = os.path.abspath(root)
    errors = []
    map_path = os.path.join(root, MAP_REL)
    src_path = os.path.join(root, SRC_REL)
    index_path = os.path.join(root, INDEX_REL)

    # R1 锚存活（fail-closed，逐条点名）
    for const, rel, path in (("MAP_REL", MAP_REL, map_path),
                             ("SRC_REL", SRC_REL, src_path),
                             ("INDEX_REL", INDEX_REL, index_path)):
        if not os.path.exists(path):
            errors.append(fail("R1_anchor_alive", "ANCHOR_STALE: %s %s" % (const, rel)))
    if errors:
        return {"verdict": "FAIL", "errors": errors, "rows": 0, "sources": 0}

    map_text = read_text(map_path)
    index_text = read_text(index_path)
    src_files = sorted(f for f in os.listdir(src_path) if f.endswith(".cpp"))
    rows = [l for l in map_text.splitlines() if _ROW.match(l)]
    mapped_files = set(_SRC_FILE.findall(map_text))

    # R2 映射面非空
    if len(rows) < MIN_MAP_ROWS:
        errors.append(fail("R2_map_not_empty",
                           "映射表仅解析出 %d 行（< %d），扫描面塌缩，禁止判绿"
                           % (len(rows), MIN_MAP_ROWS)))

    # R3 映射目标仍存在（防悬空映射）
    for name in sorted(mapped_files):
        if name.endswith(".cpp") and name not in src_files:
            errors.append(fail("R3_mapped_exists",
                               "映射表点名 %s，但 %s 下已不存在" % (name, SRC_REL)))

    # R4 现行源分类闭合
    for name in src_files:
        if name in mapped_files:
            continue
        if name in additions:
            meta = additions[name]
            missing = [k for k in ("owner", "evidence") if not meta.get(k)]
            if missing:
                errors.append(fail("R4_source_classified",
                                   "登记面 %s 缺字段 %s" % (name, missing)))
            continue
        errors.append(fail("R4_source_classified",
                           "unclassified phase2 source: %s（既不在归档映射表，也不在 "
                           "POST_ARCHIVE_ADDITIONS 登记面）" % name))

    # R5 ACR 禁止迁移声明
    for token in ACR_TOKENS:
        if token not in map_text:
            errors.append(fail("R5_acr_forbidden",
                               "forbidden-migration token missing: %s" % token))
    for name in ACR_FILES:
        idx = map_text.find(name)
        if idx < 0:
            errors.append(fail("R5_acr_forbidden", "ACR file absent from map: %s" % name))
            continue
        line = map_text[map_text.rfind("\n", 0, idx) + 1:map_text.find("\n", idx)]
        if FORBIDDEN_MARK not in line:
            errors.append(fail("R5_acr_forbidden",
                               "ACR file not marked %s on its row: %s"
                               % (FORBIDDEN_MARK, name)))
    for decl in FORBIDDEN_DECLS:
        if decl not in map_text:
            errors.append(fail("R5_acr_forbidden",
                               "missing forbidden-migration declaration: %s" % decl))

    # R6 SCI 登记
    for sci in sorted(set(_SCI.findall(map_text))):
        if sci not in index_text:
            errors.append(fail("R6_sci_registered", "SCI not in INDEX: %s" % sci))

    errors.sort(key=lambda e: (e["code"], e["detail"]))
    return {"verdict": "PASS" if not errors else "FAIL", "errors": errors,
            "rows": len(rows), "sources": len(src_files),
            "mapped_files": sorted(mapped_files),
            "post_archive_additions": sorted(additions)}


def emit(result, json_out):
    by_code = {}
    for e in result["errors"]:
        by_code[e["code"]] = by_code.get(e["code"], 0) + 1
    report = {
        "schema": "astrocs/p2-symbol-map/v1",
        "task": "GUARD-TOOLS-FIX / CONFORM-SWEEP-3-017",
        "fact_sources": {"map": MAP_REL, "src": SRC_REL, "index": INDEX_REL},
        "verdict": result["verdict"],
        "map_rows": result.get("rows", 0),
        "sources": result.get("sources", 0),
        "by_code": dict(sorted(by_code.items())),
        "errors": result["errors"],
    }
    if json_out:
        parent = os.path.dirname(os.path.abspath(json_out))
        if parent:
            os.makedirs(parent, exist_ok=True)
        with open(json_out, "w", encoding="utf-8") as fh:
            json.dump(report, fh, ensure_ascii=False, indent=1, sort_keys=True)
            fh.write("\n")
    if result["verdict"] != "PASS":
        print("P2-001_MAP_VIOLATION: %d findings (by_code=%s)"
              % (len(result["errors"]),
                 ",".join("%s:%d" % kv for kv in sorted(by_code.items()))))
        for e in result["errors"]:
            print("  [%s] %s" % (e["code"], e["detail"]))
        return 1
    print("P2-001_PASS: %d 映射行 / %d 现行源，ACR 禁止迁移声明完整，SCI 全部登记"
          % (report["map_rows"], report["sources"]))
    return 0


# ------------------------------------------------------------------ self-test
BT = chr(96)
_FIX_MAP = ("# P2-001: Phase2 old Stage2 步骤映射表\n\n"
            "| old Stage2 步骤/符号 | 目标模块 | 输入 → 输出 | SCI | ALG | 状态 |\n"
            "|---|---|---|---|---|---|\n"
            + "".join("| " + BT + n + BT + " | " + BT + "astrocs.phase2.m" + str(i) + BT
                      + " | x → y | SCI-AAA-001 | ALG-A-00" + str(i) + " | 已映射 |\n"
                      for i, n in enumerate(["alpha.cpp", "beta.cpp", "gamma.cpp",
                                             "delta.cpp", "epsilon.cpp", "zeta.cpp",
                                             "eta.cpp", "theta.cpp"], 1))
            + "| **" + BT + "acr_kernels.cpp" + BT + "** | **禁止迁移依赖** | — | — | — "
              "| ACR call 禁止迁移 |\n"
              "| " + BT + "cuda_bridge_stub.cpp" + BT + " | 禁止迁移依赖 | — | — | — "
              "| stub 仅编译占位 |\n"
              "| " + BT + "p2_acr_block_eligible" + BT + " | 禁止迁移依赖 | — | — | — "
              "| ACR 路径不进生产 |\n"
              "| " + BT + "beta.cpp" + BT + " | " + BT + "astrocs.phase2.b2" + BT
              + " | x → y | SCI-BBB-001 | ALG-B-001 | 已映射 |\n"
            "\n## 禁止迁移依赖声明\n"
            "- " + BT + "acr_kernels.cpp" + BT + " — **禁止作为迁移依赖**;\n")
_FIX_SRCS = ["alpha.cpp", "beta.cpp", "gamma.cpp", "delta.cpp", "epsilon.cpp",
             "zeta.cpp", "eta.cpp", "theta.cpp", "acr_kernels.cpp",
             "cuda_bridge_stub.cpp"]
_FIX_INDEX = "SCI-AAA-001: a\nSCI-BBB-001: b\n"
_FIX_ACR_ROW = ("| **" + BT + "acr_kernels.cpp" + BT + "** | **禁止迁移依赖** | — | — | — "
               "| ACR call 禁止迁移 |\n")
_FIX_MAP_ACR_UNMARKED = _FIX_MAP.replace(
    _FIX_ACR_ROW,
    "| **" + BT + "acr_kernels.cpp" + BT + "** | " + BT + "astrocs.phase2.acr" + BT
    + " | — | — | — | 已映射 |\n")


def _write(root, rel, text):
    p = os.path.join(root, rel)
    os.makedirs(os.path.dirname(p), exist_ok=True)
    with open(p, "w", encoding="utf-8") as fh:
        fh.write(text)


def _mk_fixture():
    root = tempfile.mkdtemp(prefix="p2map_selftest_")
    _write(root, MAP_REL, _FIX_MAP)
    _write(root, INDEX_REL, _FIX_INDEX)
    for name in _FIX_SRCS:
        _write(root, os.path.join(SRC_REL, name), "// fixture\n")
    return root


def self_test():
    fails = []
    checks = []

    def expect(name, root, want, code=None, additions=None):
        res = check_root(root, additions if additions is not None else {})
        codes = {e["code"] for e in res["errors"]}
        ok = res["verdict"] == want and (code is None or code in codes)
        checks.append((name, res["verdict"], sorted(codes), ok))
        if not ok:
            fails.append("%s: 期望 %s/%s 实得 %s/%s"
                         % (name, want, code, res["verdict"], sorted(codes)))

    root = _mk_fixture()
    try:
        expect("N0 正例（映射闭合 + ACR 声明完整）", root, "PASS")
        _write(root, os.path.join(SRC_REL, "newmod.cpp"), "// new\n")
        expect("N1 新增源未登记", root, "FAIL", "R4_source_classified")
        expect("N1' 登记后回绿", root, "PASS", None,
               {"newmod.cpp": {"owner": "selftest", "evidence": ["fixture"]}})
        os.remove(os.path.join(root, SRC_REL, "newmod.cpp"))
        _write(root, MAP_REL, _FIX_MAP_ACR_UNMARKED)
        expect("N2 ACR 未标禁止迁移", root, "FAIL", "R5_acr_forbidden")
        _write(root, MAP_REL, _FIX_MAP)
        _write(root, MAP_REL, _FIX_MAP.replace("禁止作为迁移依赖", "可迁移"))
        expect("N3 禁止迁移声明缺失", root, "FAIL", "R5_acr_forbidden")
        _write(root, MAP_REL, _FIX_MAP)
        _write(root, MAP_REL, _FIX_MAP.replace("SCI-BBB-001", "SCI-ZZZ-999"))
        expect("N4 SCI 未登记", root, "FAIL", "R6_sci_registered")
        _write(root, MAP_REL, _FIX_MAP)
        _write(root, MAP_REL, "# 空映射表\n无内容\n")
        expect("N5 映射面塌缩", root, "FAIL", "R2_map_not_empty")
        _write(root, MAP_REL, _FIX_MAP)
        os.remove(os.path.join(root, MAP_REL))
        expect("N6 MAP 锚失效", root, "FAIL", "R1_anchor_alive")
        _write(root, MAP_REL, _FIX_MAP)
        _write(root, MAP_REL, _FIX_MAP.replace(BT + "theta.cpp" + BT,
                                               BT + "ghost.cpp" + BT))
        expect("N7 映射悬空", root, "FAIL", "R3_mapped_exists")
        _write(root, MAP_REL, _FIX_MAP)
        expect("N7' 恢复后回绿", root, "PASS")
    finally:
        shutil.rmtree(root, ignore_errors=True)

    for name, verdict, codes, ok in checks:
        print("  %-30s verdict=%-4s codes=%-34s %s"
              % (name, verdict, ",".join(codes) or "-", "OK" if ok else "**FAIL**"))
    if fails:
        print("P2_SYMBOL_MAP_SELFTEST_FAIL:")
        for f in fails:
            print("  " + f)
        return 1
    print("P2_SYMBOL_MAP_SELFTEST_PASS: %d 组（正例 1 + 负例/恢复 7 类）全部符合预期"
          % len(checks))
    return 0


def main(argv=None):
    ap = argparse.ArgumentParser(description="P2-001 symbol map / ACR migration gate")
    ap.add_argument("--root", default=REPO)
    ap.add_argument("--json-out", default=None)
    ap.add_argument("--self-test", action="store_true")
    args = ap.parse_args(argv)
    if args.self_test:
        return self_test()
    if not os.path.isdir(args.root):
        print("P2-001_MAP_VIOLATION: ANCHOR_STALE: --root %s" % args.root)
        return 2
    return emit(check_root(args.root), args.json_out)


if __name__ == "__main__":
    sys.exit(main())
