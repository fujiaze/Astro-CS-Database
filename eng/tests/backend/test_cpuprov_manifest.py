#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""R-60 声明面（第二族）: CPU provider 变体族的清单位值 = 站点旗标推导位。

判据链: 根 CMakeLists.txt 的旗标站点（eng/tools/quality/isa_sites.json）
  → eng/tools/isa_sites.py 推导位面
  → eng/tools/gen_provider_manifests.py 拒绝出不一致的清单（rc=5）
  → 本测试用**真 DSO + 真生成器**逐位核对（表对表的自证不够: 表错则一起绿）。

复现: python3 -m unittest discover -s eng/tests/backend -t eng/tests/backend -k cpuprov_manifest
"""
import json
import os
import shutil
import subprocess
import sys
import tempfile
import unittest

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
GEN = os.path.join(REPO, "eng", "tools", "gen_provider_manifests.py")
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from variant_build import build_cpuprov_variant  # noqa: E402

sys.path.insert(0, os.path.join(REPO, "eng", "tools"))
import isa_feature_bits  # noqa: E402
import isa_sites  # noqa: E402

FB = isa_feature_bits.FeatureBits.load(repo_root=REPO)
COMPILERS = {"gnu": "GNU-14.2.0", "msvc": "MSVC-19.38.33130.0"}


class TestProviderFamilyManifest(unittest.TestCase):
    """第二族（CPU provider 变体族 astrocs_cpuprov_*）的**声明面**端到端。

    为什么单独成文件（不并进 test_manifest_isa_declaration.py）:
      那个文件是「第一族 backend 变体 + 清单↔产物交叉判据（M1-M6）」的正本，另一条线正在
      同文件上在途改动。声明面第二族是**另一族变体**，口径相同、对象不同，各自成文件可避免
      并发写互相抹掉（实测踩过：整类被对方的整体重写覆盖过一次）。

    覆盖: 真 cpuprov DSO（门面 TU 零 ISA 旗标 / 计算面 TU 唯一带旗标，走与发行同形的
    eng/tests/backend/variant_build.py 配方）+ 真 eng/tools/gen_provider_manifests.py
    --family provider，两条平台腿（GNU / MSVC）逐位核对。

    判据（负例也在这里）:
      1) 两腿 provider 清单位值必须等于**站点旗标推导位**（avx2=24 / avx512=992）;
      2) build.flags 必须是该平台**适用**的旗标（GNU: -m*；MSVC: /arch:），不是手抄串;
      3) provider 家族带 baseline 条目（skip_baseline_when_isa_only=False），其声明位为空
         是**设计**（amd64 SSE2 基线不声明任何附加位），不得因此判红;
      4) 负例: 站点表里删掉 provider 变体 ⇒ 生成器 fail-closed（rc=2），不得凭空出清单;
      5) 负例: 站点旗标少一位而声明表没跟着少 ⇒ rc=5「声明了但没编进去」（虚假能力声明）。
    """

    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.mkdtemp(prefix="r60_prov_manifest_")
        cls.prov = os.path.join(cls.tmp, "providers")
        os.makedirs(cls.prov)
        cpu = os.path.join(REPO, "lib", "infrastructure", "benchmark", "cpu")
        cls.cap = os.path.join(cls.tmp, "capability_detect.o")
        r = subprocess.run(["gcc", "-std=c11", "-O2", "-DNDEBUG", "-fPIC", "-c",
                            "-Wall", "-Wextra", "-Wpedantic",
                            "-I" + os.path.join(REPO, "lib", "include"),
                            "-I" + os.path.join(cpu, "common", "include"),
                            os.path.join(cpu, "common", "src", "capability_detect.c"),
                            "-o", cls.cap], capture_output=True, text=True, timeout=300)
        assert r.returncode == 0, r.stderr
        # baseline（无 ISA 旗标的门面 TU，本族 baseline 就是基线 ISA 面）
        r = subprocess.run(["g++", "-std=c++17", "-O2", "-DNDEBUG", "-fPIC", "-shared",
                            "-Wall", "-Wextra", "-Wpedantic",
                            "-I" + os.path.join(REPO, "lib", "include"),
                            "-I" + os.path.join(cpu, "common", "include"),
                            "-I" + os.path.join(cpu, "baseline", "include"),
                            os.path.join(cpu, "baseline", "src", "baseline_provider.cpp"),
                            cls.cap, "-o", os.path.join(cls.prov, "astrocs_cpuprov_baseline.so"),
                            "-lpthread"], capture_output=True, text=True, timeout=300)
        assert r.returncode == 0, r.stderr
        for variant in ("avx2", "avx512"):
            rc, err = build_cpuprov_variant(
                variant, os.path.join(cls.prov, f"astrocs_cpuprov_{variant}.so"),
                extra_link_inputs=[cls.cap], ld_extra=["-lpthread"])
            assert rc == 0, err
        cls.manifests = {}
        for leg, compiler in COMPILERS.items():
            out = os.path.join(cls.tmp, f"provider_{leg}.json")
            r = subprocess.run(["python3", GEN, "--repo", REPO, "--build-dir", cls.tmp,
                                "--providers-dir", cls.prov, "--family", "provider",
                                "--out", out, "--compiler", compiler, "--commit", "0" * 40],
                               capture_output=True, text=True, timeout=300)
            assert r.returncode == 0, r.stderr
            with open(out, encoding="utf-8") as f:
                cls.manifests[leg] = json.load(f)

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.tmp, ignore_errors=True)

    def _rec(self, leg, backend_id):
        for b in self.manifests[leg]["backends"]:
            if b["backend_id"] == backend_id:
                return b
        self.fail(f"{leg} provider 清单缺 {backend_id}")

    def test_01_provider_manifest_equals_registry_both_legs(self):
        for leg, compiler in COMPILERS.items():
            self.assertEqual(self._rec(leg, "baseline")["required_features_bits"], 0,
                             "baseline 不声明任何附加位（amd64 SSE2 基线）")
            for backend_id, site_id in (("avx2", "product-cpuprov-avx2"),
                                        ("avx512", "product-cpuprov-avx512")):
                reg = isa_sites.load()
                site = isa_sites.site_of(reg, site_id)
                self.assertIsNotNone(site, f"站点 {site_id} 未登记")
                flags, names, bits = isa_sites.permitted(
                    reg, site, isa_sites.platform_of(compiler), FB)
                rec = self._rec(leg, backend_id)
                self.assertEqual(rec["required_features_names"], list(names),
                                 f"{leg} 腿 {backend_id} 清单位名 != 站点旗标推导位")
                self.assertEqual(rec["required_features_bits"], bits,
                                 f"{leg} 腿 {backend_id} 清单位值 != 站点旗标推导位")
                self.assertEqual(self.manifests[leg]["build"]["flags"][backend_id],
                                 " ".join(flags),
                                 f"{leg} 腿 build.flags 必须是该平台适用旗标（不得手抄）")
        # 该族位值是**已知且合法**的: avx2 = AVX2|FMA = 24; avx512 = F|CD|BW|DQ|VL = 992
        # （本族确实带 -mavx512cd 编译 ⇒ 声明含 CD; 与第一族 backend 变体的 928 是合法
        #   差异 —— 判据只要求各自同源）。
        self.assertEqual(self._rec("gnu", "avx2")["required_features_bits"], 24)
        self.assertEqual(self._rec("gnu", "avx512")["required_features_bits"], 992)
        self.assertEqual(self._rec("msvc", "avx512")["required_features_bits"], 992)

    def test_02_negative_registry_and_flag_injection(self):
        """负例: 站点缺失 ⇒ rc=2; 站点旗标少一位而声明表没少 ⇒ rc=5。"""
        with open(isa_sites.DEFAULT_SITES, encoding="utf-8") as f:
            reg = json.load(f)
        out = os.path.join(self.tmp, "neg.json")
        base = ["python3", GEN, "--repo", REPO, "--build-dir", self.tmp,
                "--providers-dir", self.prov, "--family", "provider", "--out", out,
                "--commit", "0" * 40]

        # (a) 站点表里删掉 provider 变体条目 ⇒ 站点未登记不得凭空出清单（fail-closed）
        no_site = os.path.join(self.tmp, "isa_sites_nosite.json")
        trimmed = json.loads(json.dumps(reg))
        trimmed["sites"] = [s for s in trimmed["sites"]
                            if not s["id"].startswith("product-cpuprov-")]
        with open(no_site, "w", encoding="utf-8") as f:
            json.dump(trimmed, f)
        r = subprocess.run(base + ["--compiler", COMPILERS["gnu"], "--sites-file", no_site],
                           capture_output=True, text=True, timeout=300)
        self.assertEqual(r.returncode, 2, "站点未登记必须 fail-closed rc=2:\n" + r.stderr)
        self.assertIn("没有站点", r.stderr)

        # (b) 站点旗标少一位（-mavx512cd）而声明表仍含 CD ⇒ 声明 ⊄ 编译（虚假能力声明）rc=5
        fewer = os.path.join(self.tmp, "isa_sites_fewer.json")
        trimmed = json.loads(json.dumps(reg))
        for s in trimmed["sites"]:
            if s["id"] == "product-cpuprov-avx512":
                s["flags"] = [f for f in s["flags"] if f != "-mavx512cd"]
        with open(fewer, "w", encoding="utf-8") as f:
            json.dump(trimmed, f)
        r = subprocess.run(base + ["--compiler", COMPILERS["gnu"], "--sites-file", fewer],
                           capture_output=True, text=True, timeout=300)
        self.assertEqual(r.returncode, 5, "旗标少一位必须判红 rc=5:\n" + r.stdout)
        self.assertIn("声明了但没编进去", r.stderr)
        # 同一张注入表对 **MSVC 腿**不判红（平台旗标是 /arch:AVX512，不含 -mavx512cd）
        r = subprocess.run(base + ["--compiler", COMPILERS["msvc"], "--sites-file", fewer],
                           capture_output=True, text=True, timeout=300)
        self.assertEqual(r.returncode, 0, "注入只影响 GNU 腿:\n" + r.stdout + r.stderr)


if __name__ == "__main__":
    unittest.main(verbosity=2)