#!/usr/bin/env python3
"""R-60 产品级回归: CPU provider 变体族（acsd_cpuprov_*）的 TU 级 ISA 隔离。

配方（与第一族 backend 变体同因同法，见 eng/tools/quality/isa_sites.json 的
tu_isolation cpuprov-avx2 / cpuprov-avx512）:
  门面 TU（{v}_provider.cpp: acsd_provider_query_v1 / acsd_cpu_*_cap_gate /
  *_self_test / kernel 注册表）**零 ISA 旗标**
  + 计算面 TU（{v}_kernels.cpp: 热点 kernel 数值循环）**唯一**带该族旗标,
  两者只经唯一跨 TU 桥 acsd_cpuprov_kernel_range_v1 相连。

为什么必须有本测试（三层判据之外的角色）:
  · 构建输入层（check_isa_same_source.py S1/S2/S3/S7/S8）证明"旗标挂在哪个 target"，
    但证明不了"旗标最终落到了哪些函数体" —— 那要看产物;
  · 产物层能判，但**要有人把判据指向本族的符号**;
    第一族由 eng/tests/backend/test_isa_variants.py 承担，本族此前无人承担;
  · 判据只能判它被喂到的东西: 若没人用"门面整 TU 带旗标"的形态跑一遍同一套断言，
    判据会退化成"永远绿"。故本测试自带**注入负例**（N1/N2/N3）。

本测试即负例面（每条都能红）:
  N3 负例: 门面 TU 源码的代码行里出现 ISA 旗标字面量 ⇒ 文本禁令判红;
  S1 静态: 门面 TU 必须走跨 TU 桥（include 桥头 + 调用桥入口）且不复制 kernel 实现;
  S2 静态: 产物导出面 = provider ABI 白名单（acsd_provider_query_v1 +
           acsd_cpu_<v>_cap_gate + acsd_cap_* 探测面），**跨 TU 桥不进动态符号表**
           （hidden visibility）⇒ ABI 导出面逐条不变;
  S3 声明面: 声明位 = 站点旗标推导位（eng/tools/isa_sites.py），两族两平台逐组核对。

复现: python3 -m unittest discover -s eng/tests/backend -t eng/tests/backend -k cpuprov
依赖: g++/objdump (binutils)。退出码 0 = 全 PASS。
"""
import json
import os
import re
import subprocess
import sys
import tempfile
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(os.path.dirname(HERE)))
sys.path.insert(0, HERE)
from variant_build import (  # noqa: E402
    BRIDGE_SYMBOL, CAPSRC, CPUPROV_VARIANTS, cpuprov_face_src, cpuprov_isa_flags,
    cpuprov_kernels_src, build_cpuprov_variant,
)

sys.path.insert(0, os.path.join(REPO, "eng", "tools"))
import isa_feature_bits  # noqa: E402
import isa_sites  # noqa: E402

FB = isa_feature_bits.FeatureBits.load(repo_root=REPO)
# 原 FACE_CLEAN_SYMS / ISA 两常量随 G08-01 移除：其唯一读者 P1/N1/N2 是
# check_variant_isa_disasm.py 的执行体，已随门禁删除，留存即死常量。
SITE_BY_VARIANT = {"avx2": "product-cpuprov-avx2", "avx512": "product-cpuprov-avx512"}
# 声明位全集（由站点旗标推导，测试不手抄位表）。
COMPILERS = ("GNU-14.2.0", "MSVC-19.38.33130.0")


def _read(path):
    with open(path, encoding="utf-8") as f:
        return f.read()


def _cap_obj(tmp):
    """capability_detect.c → 无任何 -mavx* 旗标的独立对象（纯 C）。
    CPU-001 契约: 探测自身只用 SSE2 可执行指令（C 编译避免 -mavx* 经 g++ 向量化）。"""
    obj = os.path.join(tmp, "capability_detect.o")
    inc = ["-I" + os.path.join(REPO, "lib", "include"),
           "-I" + os.path.join(REPO, "lib", "infrastructure", "benchmark", "cpu",
                               "common", "include")]
    r = subprocess.run(["gcc", "-std=c11", "-O2", "-DNDEBUG", "-fPIC", "-c",
                        "-Wall", "-Wextra", "-Wpedantic", *inc, CAPSRC, "-o", obj],
                       capture_output=True, text=True, timeout=300)
    assert r.returncode == 0, r.stderr
    return obj


def _disasm(so, tmp, tag):
    out = os.path.join(tmp, tag + ".dis")
    r = subprocess.run(["objdump", "-d", so], capture_output=True, text=True, timeout=300)
    assert r.returncode == 0, r.stderr
    with open(out, "w", encoding="utf-8") as f:
        f.write(r.stdout)
    return out


class TestCpuProviderIsaIsolation(unittest.TestCase):
    """R-60: CPU provider 变体族的 TU 级 ISA 隔离（产物级 + 源码级 + 声明级）。"""

    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.mkdtemp(prefix="r60_cpuprov_")
        cls.cap = _cap_obj(cls.tmp)
        cls.so = {}
        for v in ("avx2", "avx512"):
            so = os.path.join(cls.tmp, "acsd_cpuprov_%s.so" % v)
            rc, err = build_cpuprov_variant(v, so, extra_link_inputs=[cls.cap],
                                            ld_extra=["-lpthread"])
            assert rc == 0, err
            cls.so[v] = so

    def _declared_names(self, variant, compiler):
        reg = isa_sites.load()
        site = isa_sites.site_of(reg, SITE_BY_VARIANT[variant])
        self.assertIsNotNone(site, f"站点 {SITE_BY_VARIANT[variant]} 未登记")
        _flags, names, _bits = isa_sites.permitted(reg, site,
                                                  isa_sites.platform_of(compiler), FB)
        return names

    def test_N3_negative_face_source_has_no_isa_flag_literal(self):
        """负例 3: 门面 TU 的**代码行**里不得出现 ISA 旗标字面量（注释可以且必须）。"""
        for v in ("avx2", "avx512"):
            src = _read(cpuprov_face_src(v))
            for line in src.splitlines():
                s = line.strip()
                if s.startswith("//") or s.startswith("*") or s.startswith("/*"):
                    continue
                for flag in cpuprov_isa_flags(v):
                    self.assertNotIn(flag, s, f"{v} 门面 TU 的代码行带 {flag}: {s[:70]}")
        # 注入: 把旗标写进一行**代码** ⇒ 同一禁令必须判红（证明禁令不是恒真）。
        src = cpuprov_face_src("avx2")
        text = _read(src)
        injected = text.replace('#include "acsd/cpu/cpuprov_kernels_v1.h"',
                                '#include "acsd/cpu/cpuprov_kernels_v1.h"\n'
                                '// c\nconst char* kBad = "-mavx2";', 1)
        self.assertNotEqual(injected, text, "注入未生效（源码形态已变，禁令失真）")
        bad = []
        for line in injected.splitlines():
            s = line.strip()
            if s.startswith("//") or s.startswith("*") or s.startswith("/*"):
                continue
            bad += [f for f in cpuprov_isa_flags("avx2") if f in s]
        self.assertTrue(bad, "注入的负例未被禁令抓住（禁令恒真）")

    def test_S1_facade_uses_bridge_and_does_not_copy_kernels(self):
        """静态: 门面走唯一跨 TU 桥、计算面含实现 ⇒ 共享合同（同一实现源）不漂移。"""
        for v in ("avx2", "avx512"):
            face = _read(cpuprov_face_src(v))
            kern = _read(cpuprov_kernels_src(v))
            self.assertIn('#include "acsd/cpu/cpuprov_kernels_v1.h"', face,
                          f"{v} 门面 TU 必须走跨 TU 桥")
            self.assertIn(BRIDGE_SYMBOL, face, f"{v} 门面 TU 必须调用桥入口")
            self.assertIn(BRIDGE_SYMBOL, kern, f"{v} 计算面 TU 必须定义桥入口")
            # 门面不得自带数值实现（kernel 循环留在计算面 TU）
            self.assertNotIn("KIDX_HIPS_BULK) {", face,
                             f"{v} 门面 TU 仍含 kernel 数值实现（未按 TU 隔离）")
            self.assertNotIn("kernel_pixel_range", face)
            # 计算面必须**只**经这一个符号与门面相连（唯一跨 TU 桥）
            bridge_defs = [ln for ln in kern.splitlines()
                           if BRIDGE_SYMBOL in ln and not ln.strip().startswith(("//", "*", "/*"))]
            self.assertEqual(len(bridge_defs), 1, f"{v} 计算面 TU 桥入口定义数 != 1")

    def test_S2_abi_export_surface_unchanged_bridge_hidden(self):
        """静态: 产物导出面 = provider ABI 白名单; 跨 TU 桥**不进**动态符号表。"""
        expect = {
            "avx2": {"acsd_cap_classify_v1", "acsd_cap_detect_v1", "acsd_cap_feature_name_v1",
                     "acsd_cap_hw_satisfies_v1", "acsd_cap_os_safe_satisfies_v1",
                     "acsd_cap_os_saves_avx512_state_v1", "acsd_cap_serialize_json_v1",
                     "acsd_cpu_avx2_cap_gate", "acsd_provider_query_v1"},
            "avx512": {"acsd_cap_classify_v1", "acsd_cap_detect_v1", "acsd_cap_feature_name_v1",
                       "acsd_cap_hw_satisfies_v1", "acsd_cap_os_safe_satisfies_v1",
                       "acsd_cap_os_saves_avx512_state_v1", "acsd_cap_serialize_json_v1",
                       "acsd_cpu_avx512_cap_gate", "acsd_provider_query_v1"},
        }
        for v in ("avx2", "avx512"):
            r = subprocess.run(["nm", "-D", "--defined-only", self.so[v]],
                               capture_output=True, text=True, timeout=120)
            self.assertEqual(r.returncode, 0, r.stderr)
            got = {ln.split()[-1] for ln in r.stdout.splitlines() if ln.strip()
                   and not ln.split()[-1].startswith("_Z")}
            self.assertEqual(got, expect[v],
                             f"{v} 导出面漂移（provider ABI 白名单逐条比对）")
            self.assertNotIn(BRIDGE_SYMBOL, r.stdout,
                             f"{v} 跨 TU 桥泄漏进动态符号表（hidden visibility 失效）")

    def test_S3_declared_bits_equal_registry_derived_both_platforms(self):
        """声明面: 清单声明位必须由站点旗标推导（两族 × 两平台），不得手抄。"""
        for v in ("avx2", "avx512"):
            for compiler in COMPILERS:
                names = self._declared_names(v, compiler)
                bits = FB.bits_of(names)
                self.assertTrue(bits > 0, f"{v}@{compiler} 推导位为 0（站点未登记旗标）")
                # 声明宏必须就在门面 TU 里被 cap_gate 真正使用（否则声明旁路）
                face = _read(cpuprov_face_src(v))
                macro = ("ACS_CPU_AVX2_REQUIRED_FEATURES" if v == "avx2"
                         else "ACS_CPU_AVX512_REQUIRED_FEATURES")
                self.assertIn(macro, face, f"{v} 门面 TU 未使用声明宏 {macro}")
        # 负例: 站点旗标被摘掉一位 ⇒ 推导位必须变小（证明推导真的随站点变，不是恒定表）
        reg = isa_sites.load()
        site = dict(isa_sites.site_of(reg, SITE_BY_VARIANT["avx2"]))
        site["flags"] = [f for f in site["flags"] if f != "-mfma"]
        full = isa_sites.permitted(reg, isa_sites.site_of(reg, "product-cpuprov-avx2"),
                                   isa_sites.GNU_PLATFORM, FB)[2]
        less = isa_sites.permitted(reg, site, isa_sites.GNU_PLATFORM, FB)[2]
        self.assertLess(less, full, "摘掉 -mfma 后推导位未变（推导未读站点旗标）")
        self.assertNotEqual(less, full)


if __name__ == "__main__":
    unittest.main(verbosity=2)
