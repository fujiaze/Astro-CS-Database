#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""test_public_header_layering.py — LAYER-PUBHDR-001 公共头分层判据。

判据（确定性、可机器复跑）
--------------------------
  公共 include 面 = lib/include ∪ lib/**/include ∪ lib/**/cpp
  （与 DOC-004/AST-API 门的 clang 独立解析面同一定义，见
   eng/tools/check_ast_api.py::include_flags）。
  规则: 落在公共 include 面的头（*.h / *.hpp）不得 #include "..." 一个位于
  任意 src/ 目录下的头。src/ 是本仓"实现面"的固定约定
  （ASTROCS_DESIGN.md §8.4「lib/include/ 公共头」、§8.5「每个可独立调度模块
  具备…版本化公开头」）; aio 的机制原语头
  lib/infrastructure/aio/src/aio_atomic_file.h 即属基建层内部头。

为什么需要本判据（本次 AST-API 红灯的根因）
-------------------------------------------
  CLEAN-403（ASTROCS_DESIGN.md §10「aio 是文件级唯一 I/O 边界」）把
  ipv/include/ipv_log.h 的日志落盘从 std::ofstream 改为 aio 机制原语，却把
  #include "aio_atomic_file.h"（aio/src 内部头）留在了公共头里 ⇒ AST-API
  （DOC-004）门在公共 include 面下 clang 独立解析 ipv_log.h 及其 5 个下游公共头
  （ipv_distortion.h / ipv_select.h / ipv_sip.h / ipv_solver.h / ipv_wcs.h）全部
  fatal error: aio_atomic_file.h file not found（6/45 头判红）。
  DOC-004 只在 docs/contracts/API_CONTRACTS.csv 登记的头族上、以"clang 解析失败"
  的间接形式表达这条规则; 本判据把它写成直接判据并覆盖全部公共头，使"公共头
  依赖基建层内部头"在公共 include 面之外也立刻判红。

能红能绿（ENGINEERING_SPEC §8）
-------------------------------
  test_01 = 真实仓库扫描必须绿（且带覆盖计数，禁止空扫描假绿）;
  test_02 = 合成树上注入"公共头 → 另一模块 src/ 头"负例，必须被检出;
  test_03 = 复刻本次 CLEAN-403 的真实缺陷形态（ipv_log.h → aio/src）必须被检出，
            同时"公共头 → 另一模块公共头"的正例不得误报（判据非恒真）。
"""
import os
import re
import shutil
import tempfile
import unittest

REPO = os.path.dirname(os.path.dirname(os.path.dirname(
    os.path.dirname(os.path.abspath(__file__)))))

INC_RE = re.compile(r'^\s*#\s*include\s+"([^"]+)"', re.M)
PUBLIC_ROOT_NAMES = ("include", "cpp")
# 非源码面（构建产物 / 运行产物 / 外部只读数据集 / vendored 第三方）不参与扫描
SKIP_DIR_NAMES = {".git", "build", "run", "artifacts", "gaia", "testdata",
                  "third_party", "__pycache__"}
HEADER_EXT = (".h", ".hpp")


def public_roots(root):
    """公共 include 面根目录（与 check_ast_api.include_flags 同定义）。"""
    out = []
    for dirpath, dirnames, _files in os.walk(os.path.join(root, "lib")):
        dirnames[:] = [d for d in dirnames if d not in SKIP_DIR_NAMES]
        if os.path.basename(dirpath) in PUBLIC_ROOT_NAMES:
            out.append(dirpath)
    return sorted(set(out))


def public_headers(root):
    """公共头清单：路径中含 include 目录分量的 *.h / *.hpp。"""
    out = []
    for dirpath, dirnames, files in os.walk(os.path.join(root, "lib")):
        dirnames[:] = [d for d in dirnames if d not in SKIP_DIR_NAMES]
        parts = dirpath.replace(os.sep, "/").split("/")
        if "include" not in parts:
            continue
        for f in files:
            if f.endswith(HEADER_EXT):
                out.append(os.path.join(dirpath, f))
    return sorted(out)


def _index_by_basename(dirs):
    idx = {}
    for d in dirs:
        for dirpath, _dirnames, files in os.walk(d):
            for f in files:
                idx.setdefault(f, []).append(os.path.join(dirpath, f))
    return idx


def scan(root):
    """扫描公共头的 quoted include。

    返回 (公共头数, 已解析 include 数, 违规列表)。
    违规 = include 解析落点位于任意 src/ 目录（实现面内部头）。
    """
    roots = public_roots(root)
    pub_by_base = _index_by_basename(roots)
    all_by_base = _index_by_basename([os.path.join(root, "lib")])
    headers = public_headers(root)

    def resolve_public(inc, hdr_dir):
        for r in [hdr_dir] + roots:
            cand = os.path.join(r, inc)
            if os.path.isfile(cand):
                return cand
        if "/" not in inc and inc in pub_by_base:
            return pub_by_base[inc][0]
        return None

    def resolve_anywhere(inc):
        if "/" not in inc:
            hits = all_by_base.get(inc)
            return hits[0] if hits else None
        for base in (os.path.join(root, "lib"), root):
            cand = os.path.join(base, inc)
            if os.path.isfile(cand):
                return cand
        return None

    violations = []
    resolved_n = 0
    for hdr in headers:
        rel_hdr = os.path.relpath(hdr, root).replace(os.sep, "/")
        with open(hdr, encoding="utf-8", errors="replace") as fh:
            text = fh.read()
        for m in INC_RE.finditer(text):
            inc = m.group(1)
            target = resolve_public(inc, os.path.dirname(hdr)) or resolve_anywhere(inc)
            if target is None:
                continue
            resolved_n += 1
            rel_t = os.path.relpath(target, root).replace(os.sep, "/")
            if "/src/" in rel_t:
                violations.append((rel_hdr, inc, rel_t))
    return len(headers), resolved_n, violations


def _write(path, text):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", encoding="utf-8") as fh:
        fh.write(text)


class TestPublicHeaderLayering(unittest.TestCase):
    """公共头不得依赖实现面（src/）内部头。"""

    def test_01_real_repo_public_headers_clean(self):
        """真实仓库: 公共头 → src/ 依赖必须为 0（带覆盖计数，禁空扫描假绿）。"""
        n_headers, n_includes, violations = scan(REPO)
        # 覆盖计数（ENGINEERING_SPEC §8: PASS 文案不得虚报覆盖；空扫描不得绿）
        # 实测基线（2026-09-23）: 122 个公共头 / 120 条已解析 quoted include。
        # 下限取 ~80% 作为棘轮，防"扫描面塌缩 ⇒ 空扫描假绿"。
        self.assertGreaterEqual(n_headers, 100,
                                "公共头扫描面过小（%d），判据可能已退化" % n_headers)
        self.assertGreaterEqual(n_includes, 90,
                                "已解析 include 过少（%d），判据可能已退化" % n_includes)
        self.assertEqual(
            violations, [],
            "公共头依赖了实现面（src/）内部头 —— 公共 include 面 = "
            "lib/include ∪ lib/**/include ∪ lib/**/cpp; 实现面依赖必须下沉到 TU "
            "(见 lib/algorithms/star_detection/src/sdet_log.cpp 先例):\n  "
            + "\n  ".join("%s -> %s [%s]" % v for v in violations))

    def test_02_selftest_detects_public_to_src_dependency(self):
        """负例: 合成树里公共头 include 另一模块 src/ 头 → 必红; 干净树 → 必绿。"""
        tmp = tempfile.mkdtemp(prefix="pubhdr_layer_")
        try:
            _write(os.path.join(tmp, "lib", "algorithms", "demo", "include", "demo_pub.h"),
                   '#include "demo_priv.h"\nint demo_api(void);\n')
            _write(os.path.join(tmp, "lib", "algorithms", "demo", "src", "demo_priv.h"),
                   "int demo_priv(void);\n")
            _n, _i, viol = scan(tmp)
            self.assertEqual(len(viol), 1, "负例未被检出: %s" % (viol,))
            self.assertEqual(viol[0][0], "lib/algorithms/demo/include/demo_pub.h")
            self.assertEqual(viol[0][2], "lib/algorithms/demo/src/demo_priv.h")

            # 正例: 同一公共 include 面内的公共头互引不是违规
            _write(os.path.join(tmp, "lib", "algorithms", "demo", "include", "demo_pub.h"),
                   '#include "demo_other.h"\nint demo_api(void);\n')
            _write(os.path.join(tmp, "lib", "algorithms", "other", "include", "demo_other.h"),
                   "int demo_other(void);\n")
            _n2, _i2, viol2 = scan(tmp)
            self.assertEqual(viol2, [], "公共头互引被误报: %s" % (viol2,))
        finally:
            shutil.rmtree(tmp, ignore_errors=True)

    def test_03_selftest_detects_clean_403_regression_shape(self):
        """复刻本次缺陷形态: 公共头 include 基建层 src 内部头（无公共副本）→ 必红。"""
        tmp = tempfile.mkdtemp(prefix="pubhdr_clean403_")
        try:
            _write(os.path.join(tmp, "lib", "infrastructure", "aio", "src",
                                "aio_atomic_file.h"), "namespace aio_atomic {}\n")
            _write(os.path.join(tmp, "lib", "algorithms", "platesolve", "cpp", "ipv",
                                "include", "ipv_log.h"),
                   '#include "aio_atomic_file.h"\nnamespace ipv { class Logger {}; }\n')
            _n, _i, viol = scan(tmp)
            self.assertEqual(len(viol), 1, "CLEAN-403 形态未被检出: %s" % (viol,))
            self.assertEqual(viol[0][1], "aio_atomic_file.h")
            self.assertEqual(viol[0][2], "lib/infrastructure/aio/src/aio_atomic_file.h")
        finally:
            shutil.rmtree(tmp, ignore_errors=True)


if __name__ == "__main__":
    unittest.main(verbosity=2)
