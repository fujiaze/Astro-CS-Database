#!/usr/bin/env python3
"""R-60 声明面同步测试: 变体 DSO 的平台精确声明 ↔ 清单 required_features(逐位)。

三侧口径（R-60 约束 4「清单与声明面同步」的机器化）:
  · 编译许可面: 变体计算面 TU 的旗标（GCC: -mavx512f/-bw/-dq/-vl；MSVC: /arch:AVX512 = F+CD+BW+DQ+VL）；
  · DSO 自陈声明: avx512_backend.cpp 的 ACSD_BACKEND_REQUIRED_FEATURES，按 __AVX512CD__ 平台分支；
  · 清单: eng/tools/gen_provider_manifests.py --compiler <id> 产出的 required_features_bits/names。

判据（本测试即负例面）:
  1) GNU 腿清单 avx512 == 928 == DSO 的 **else 分支**（无 CD）；
  2) MSVC 腿清单 avx512 == 992 == DSO 的 **__AVX512CD__ 分支**（含 CD）；
  3) 两腿除 avx512 的声明集外**逐字节相同**（Linux 腿输出不受本改动影响）。
任一侧漂移（改了分支、改了 GENERATOR 表、改了清单位）即红。
"""
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
import unittest

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
HOST = os.path.join(REPO, "lib", "infrastructure", "benchmark", "backend_host")
INC = os.path.join(REPO, "lib", "include")
GEN = os.path.join(REPO, "eng", "tools", "gen_provider_manifests.py")
# 第二族（CPU provider 变体族 acsd_cpuprov_*）的声明面端到端在独立文件
# eng/tests/backend/test_cpuprov_manifest.py（真 cpuprov DSO + 真 provider 清单生成器）。
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from variant_build import build_variant  # noqa: E402

BITS = {"avx2": 1 << 4, "fma": 1 << 3, "avx512f": 1 << 5, "avx512cd": 1 << 6,
        "avx512bw": 1 << 7, "avx512dq": 1 << 8, "avx512vl": 1 << 9}


def _dsn_declaration_branches():
    """解析 DSO 声明宏的两个平台分支 → (含 CD 分支的位, else 分支的位)。"""
    src = open(os.path.join(HOST, "avx512_backend.cpp"), encoding="utf-8").read()
    m = re.search(r"#if defined\(__AVX512CD__\)(.*?)#else(.*?)#endif", src, re.S)
    assert m, "avx512_backend.cpp 必须保留 __AVX512CD__ 平台分支声明（R-60）"

    GROUP = {"AVX512_PROVIDER_REQUIRED": ("avx512f", "avx512bw", "avx512dq", "avx512vl")}

    def bits_of(text):
        bits = 0
        for name in re.findall(r"ACS_FEAT_(\w+)", text):
            for key in GROUP.get(name, ()) or (name.lower().replace("avx512", "avx512"),):
                bits |= BITS[key]
        return bits

    return bits_of(m.group(1)), bits_of(m.group(2))


class TestManifestIsaDeclaration(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.mkdtemp(prefix="r60_decl_")
        cls.prov = os.path.join(cls.tmp, "providers")
        os.makedirs(cls.prov)
        for variant in ("avx2", "avx512"):
            rc, err = build_variant(HOST, INC, variant,
                                    os.path.join(cls.prov, f"acsd_cpu_{variant}.so"))
            assert rc == 0, err
        cls.manifests = {}
        for leg, compiler in (("gnu", "GNU-14.2.0"), ("msvc", "MSVC-19.38.33130.0")):
            out = os.path.join(cls.tmp, f"{leg}.json")
            r = subprocess.run(["python3", GEN, "--repo", REPO, "--build-dir", cls.tmp,
                                "--providers-dir", cls.prov, "--isa-only", "--out", out,
                                "--compiler", compiler, "--commit", "0" * 40],
                               capture_output=True, text=True, timeout=180)
            assert r.returncode == 0, r.stderr
            cls.manifests[leg] = json.load(open(out, encoding="utf-8"))

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.tmp, ignore_errors=True)

    def _bits(self, leg, backend_id):
        for b in self.manifests[leg]["backends"]:
            if b["backend_id"] == backend_id:
                return b["required_features_bits"], b["required_features_names"]
        self.fail(f"{leg} 清单缺 {backend_id}")

    def test_01_platform_exact_declaration_matches_manifest(self):
        with_cd, without_cd = _dsn_declaration_branches()
        self.assertEqual(without_cd, 928, "无 CD 分支(GCC/Clang 腿)应为 F|BW|DQ|VL")
        self.assertEqual(with_cd, 992, "含 CD 分支(MSVC /arch:AVX512)应为 F|CD|BW|DQ|VL")
        gnu_bits, gnu_names = self._bits("gnu", "avx512")
        msvc_bits, msvc_names = self._bits("msvc", "avx512")
        self.assertEqual((gnu_bits, gnu_names),
                         (without_cd, ["avx512f", "avx512bw", "avx512dq", "avx512vl"]))
        self.assertEqual(msvc_bits, with_cd)
        self.assertIn("avx512cd", msvc_names, "MSVC 腿清单必须与 DSO 的含 CD 声明同步")

    def test_02_linux_leg_output_unaffected(self):
        """除 avx512 的声明集外，两腿清单逐字节相同（Linux 腿输出不受 R-60 影响）。"""
        g, m = self.manifests["gnu"], self.manifests["msvc"]
        for doc in (g, m):
            doc["build"]["compiler"] = "X"
            # 平台旗标按平台不同是**设计**（GNU: -m*；MSVC: /arch:），不是漂移 ⇒ 归一。
            # build.flags 是生成器从旗标站点表推导出的**活字段**（见 gen_provider_manifests.py
            # 的 flags_by_backend），R-60 起不再手抄；它的正确性由 test_cpuprov_manifest.py
            # 与 M6 类交叉判据逐条核。
            doc["build"]["flags"] = "X"
            for b in doc["backends"]:
                if b["backend_id"] == "avx512":
                    for k in ("required_features_bits", "required_features_names",
                              "required_features_bits_detectable"):
                        b[k] = "X"
        self.assertEqual(json.dumps(g, sort_keys=True), json.dumps(m, sort_keys=True))
        self.assertEqual(self._bits("gnu", "avx2")[0], 24, "avx2 两腿都为 AVX2|FMA=24")

    def test_03_disasm_checker_self_test(self):
        chk = os.path.join(REPO, "eng", "tools", "quality", "check_variant_isa_disasm.py")
        r = subprocess.run(["python3", chk, "--self-test"], capture_output=True, text=True,
                           timeout=120)
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
        self.assertIn("SELF_TEST PASS", r.stdout)


if __name__ == "__main__":
    unittest.main(verbosity=2)
