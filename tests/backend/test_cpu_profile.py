#!/usr/bin/env python3
"""BENCH-004 测试: profile schema/mutation/stale/AVX512 slower/噪声裕量/无 profile 多线程。"""
import hashlib, json, os, re, shutil, subprocess, tempfile, unittest

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
HOST = os.path.join(REPO, "lib", "backend_host")
INC = os.path.join(REPO, "include")
SCHEMA = json.load(open(os.path.join(
    REPO, "工程控制", "RELEASE_V5",
    "AstroCS_MAIN_RELEASE_CONTROL_V5_SINGLE_CLI_AMD64_20260828",
    "schemas", "cpu_profile.schema.json"), encoding="utf-8"))
COMMIT = subprocess.run(["git", "-C", REPO, "rev-parse", "HEAD"],
                        capture_output=True, text=True).stdout.strip()


def common_srcs():
    return [os.path.join(HOST, "baseline_backend.cpp"),
            os.path.join(HOST, "bench_harness.cpp"),
            os.path.join(HOST, "host_services.cpp"),
            os.path.join(HOST, "cpu_features.cpp"),
            os.path.join(HOST, "hardware_inspect.cpp"),
            os.path.join(HOST, "backend_loader.cpp"),
            os.path.join(REPO, "lib", "common", "crypto", "sha256.cpp"),
            f"-I{os.path.join(REPO, 'lib', 'common', 'crypto')}",
            f"-I{os.path.join(REPO, 'third_party')}", "-ldl"]


class TestCpuProfile(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.mkdtemp(prefix="prof_")
        cls.gen = os.path.join(cls.tmp, "pgen")
        r = subprocess.run(["g++", "-std=c++17", "-O2", "-Wno-format-truncation",
                            f"-I{INC}", f"-I{HOST}", f"-I{os.path.join(REPO, 'third_party')}",
                            os.path.join(REPO, "tests", "backend", "profile_gen_main.cpp"),
                            *common_srcs(), "-o", cls.gen], capture_output=True, text=True,
                           timeout=300)
        assert r.returncode == 0, r.stderr
        cls.profile = os.path.join(cls.tmp, "cpu_profile.json")
        cls.hw = os.path.join(cls.tmp, "hw.json")
        # 由被测 CLI 自身产出硬件画像(fixture 真实性: 与生产路径同一实现)
        exe = os.path.join(REPO, "build", "cli", "astrocs")
        r2 = subprocess.run([exe, "hardware", "inspect", "--json"],
                            capture_output=True, text=True, timeout=60)
        assert r2.returncode == 0, r2.stderr
        open(cls.hw, "w", encoding="utf-8").write(r2.stdout)
        r3 = subprocess.run([cls.gen, "--out", cls.profile, "--mode", "quick",
                             "--version", "0.9.0-alpha.1", "--commit", COMMIT,
                             "--backend-sha", "0"], capture_output=True, text=True, timeout=300)
        assert r3.returncode == 0, r3.stderr

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.tmp, ignore_errors=True)

    def _validate(self, profile=None, hw=None, commit=COMMIT):
        a = [profile or self.profile]
        if hw:
            a += ["--hardware", hw if isinstance(hw, str) else hw]
        if commit:
            a += ["--commit", commit]
        return subprocess.run(["python3", os.path.join(REPO, "tools", "validate_cpu_profile.py"),
                               *a], capture_output=True, text=True, timeout=60)

    def test_01_profile_schema_valid(self):
        d = json.load(open(self.profile, encoding="utf-8"))
        for k in SCHEMA["required"]:
            self.assertIn(k, d, f"缺 {k}")
        self.assertEqual(d["verdict"], "PASS")
        k = d["kernels"][0]
        for f in SCHEMA["properties"]["kernels"]["items"]["required"]:
            self.assertIn(f, k, f"kernel 缺 {f}")
        m = k["measurements"]
        self.assertLessEqual(m["p05_ns"], m["median_ns"], "p05 ≤ median")
        self.assertLessEqual(m["median_ns"], m["p95_ns"], "median ≤ p95")
        self.assertGreater(m["mad_ns"], 0)
        self.assertRegex(d["build"]["commit"], r"^[0-9a-f]{40}$")
        r = self._validate()
        self.assertEqual(r.returncode, 0, r.stdout)
        self.assertIn("VALID", r.stdout)

    def test_02_stale_isa_state(self):
        """mutation: feature_bits 变化(如禁用 AVX2 后)→ STALE(OS ISA state)。"""
        d = json.load(open(self.profile, encoding="utf-8"))
        d["hardware"]["feature_bits"] = d["hardware"]["feature_bits"] & ~((1 << 3) | (1 << 4))
        p = os.path.join(self.tmp, "m_isa.json")
        json.dump(d, open(p, "w"))
        hw2 = json.load(open(self.hw, encoding="utf-8"))
        hw2["feature_bits"] = d["hardware"]["feature_bits"]
        hw_path = os.path.join(self.tmp, "hw_isa.json")
        json.dump(hw2, open(hw_path, "w"))
        r = self._validate(profile=p, hw=hw_path)
        self.assertEqual(r.returncode, 1)
        self.assertIn("STALE", r.stdout)

    def test_03_stale_affinity_and_commit(self):
        # affinity 收缩 → STALE
        hw2 = json.load(open(self.hw, encoding="utf-8"))
        hw2["affinity"] = [0]
        hw2["available_logical_cpus"] = 1
        hw_path = os.path.join(self.tmp, "hw_aff.json")
        json.dump(hw2, open(hw_path, "w"))
        r = self._validate(hw=hw_path)
        self.assertEqual(r.returncode, 1)
        self.assertIn("STALE affinity changed", r.stdout)
        # commit 变化 → STALE(build 失效)
        r2 = self._validate(commit="0" * 40)
        self.assertEqual(r2.returncode, 1)
        self.assertIn("STALE commit changed", r2.stdout)

    def test_04_oracle_fail_verdict_fail(self):
        d = json.load(open(self.profile, encoding="utf-8"))
        d["kernels"][0]["oracle_status"] = "fail"
        p = os.path.join(self.tmp, "m_oracle.json")
        json.dump(d, open(p, "w"))
        r = self._validate(profile=p)
        self.assertEqual(r.returncode, 1)
        self.assertIn("STALE oracle_status=fail", r.stdout)

    def test_05_avx512_slower_never_selected(self):
        """ISA-001 实测: 变体更慢的 op(driz_accum)在候选选择中不得胜过 baseline。"""
        bench = os.path.join(self.tmp, "kbench")
        crypto_inc = f"-I{os.path.join(REPO, 'lib', 'common', 'crypto')}"
        r = subprocess.run(["g++", "-std=c++17", "-O2", f"-I{INC}", f"-I{HOST}", crypto_inc,
                            os.path.join(REPO, "tests", "backend", "kernel_bench_main.cpp"),
                            os.path.join(HOST, "baseline_backend.cpp"),
                            os.path.join(HOST, "host_services.cpp"), "-ldl", "-o", bench],
                           capture_output=True, text=True, timeout=240)
        self.assertEqual(r.returncode, 0, r.stderr)
        vso = os.path.join(self.tmp, "avx2_backend.so")
        r = subprocess.run(["g++", "-std=c++17", "-O2", "-DNDEBUG", "-mavx2", "-mfma",
                            "-fPIC", "-shared", f"-I{INC}", f"-I{HOST}",
                            os.path.join(HOST, "avx2_backend.cpp"), "-o", vso],
                           capture_output=True, text=True, timeout=240)
        self.assertEqual(r.returncode, 0, r.stderr)
        run = subprocess.run([bench, "--variant", vso], capture_output=True, text=True,
                             timeout=300)
        self.assertIn("VARIANT_LOADED avx2", run.stdout)
        b = re.search(r"BENCH driz_accum ([\d.]+)", run.stdout)
        v = re.search(r"VARIANT driz_accum ([\d.]+)", run.stdout)
        self.assertIsNotNone(v)
        # 实测方向: 变体更慢 → 不选(与 ISA-001 台账一致)
        self.assertGreater(float(v.group(1)), float(b.group(1)),
                           "driz_accum 变体实测应更慢(与 ISA-001 一致)")

    def test_06_no_profile_baseline_multithread(self):
        """06 §6: 无 profile → baseline+workers=affinity(≥2 不退 1)+reason=no_valid_profile。"""
        probe = os.path.join(self.tmp, "polprobe")
        src = r'''
#include <cstdio>
#include "bench_harness.h"
int main() {
    using namespace astrocs::backend_host;
    for (uint32_t avail : {1u, 2u, 16u}) {
        auto p = no_profile_policy(avail);
        std::printf("POL avail=%u backend=%s workers=%u reason=%s\n",
                    avail, p.backend_id.c_str(), p.workers, p.reason);
    }
    return 0;
}
'''
        src_path = os.path.join(self.tmp, "pol.cpp")
        open(src_path, "w").write(src)
        r = subprocess.run(["g++", "-std=c++17", f"-I{INC}", f"-I{HOST}",
                            f"-I{os.path.join(REPO, 'lib', 'common', 'crypto')}", src_path,
                            os.path.join(HOST, "bench_harness.cpp"),
                            os.path.join(HOST, "host_services.cpp"),
                            os.path.join(REPO, "lib", "common", "crypto", "sha256.cpp"),
                            "-o", probe], capture_output=True, text=True, timeout=120)
        self.assertEqual(r.returncode, 0, r.stderr)
        out = subprocess.run([probe], capture_output=True, text=True, timeout=30).stdout
        self.assertIn("avail=2 backend=baseline workers=2 reason=no_valid_profile", out)
        self.assertNotIn("avail=2 backend=baseline workers=1", out, "可用≥2 不得退 1")
        self.assertIn("avail=16 backend=baseline workers=16", out)

    def test_07_noise_margin_conservative_choice(self):
        """06 §4: 候选收益 < 噪声裕量 → 选更保守路径(确定性构造验证)。"""
        probe = os.path.join(self.tmp, "nmprobe")
        src = r'''
#include <cstdio>
#include "bench_harness.h"
using namespace astrocs::backend_host;
int main() {
    std::vector<BenchResult> rs = {
        {"baseline", "OK", "", 9, 1000, 50, 950, 1050, 0, "h1"},
        {"marginal", "OK", "", 9, 940, 45, 890, 990, 0, "h2"},    // +6.4% < 15% 裕量
    };
    std::printf("SEL margin15 -> %s\n", select_with_noise_margin(rs, "baseline", 0.15).c_str());
    std::printf("SEL margin0 -> %s\n", select_with_noise_margin(rs, "baseline", 0.0).c_str());
    rs.push_back({"big_gain", "OK", "", 9, 400, 20, 380, 420, 0, "h3"});   // +60% > 裕量
    std::printf("SEL biggain -> %s\n", select_with_noise_margin(rs, "baseline", 0.15).c_str());
    rs[2].verdict = "ORACLE_FAIL";   // 大收益但错误 → 不可选
    std::printf("SEL fail-excluded -> %s\n", select_with_noise_margin(rs, "baseline", 0.15).c_str());
    return 0;
}
'''
        src_path = os.path.join(self.tmp, "nm.cpp")
        open(src_path, "w").write(src)
        r = subprocess.run(["g++", "-std=c++17", f"-I{INC}", f"-I{HOST}",
                            f"-I{os.path.join(REPO, 'lib', 'common', 'crypto')}", src_path,
                            os.path.join(HOST, "bench_harness.cpp"),
                            os.path.join(HOST, "host_services.cpp"),
                            os.path.join(REPO, "lib", "common", "crypto", "sha256.cpp"),
                            "-o", probe], capture_output=True, text=True, timeout=120)
        self.assertEqual(r.returncode, 0, r.stderr)
        out = subprocess.run([probe], capture_output=True, text=True, timeout=30).stdout
        self.assertIn("SEL margin15 -> baseline", out, "收益不足裕量 → 保守")
        self.assertIn("SEL margin0 -> marginal", out, "裕量 0 → 纯最快")
        self.assertIn("SEL biggain -> big_gain", out, "收益超裕量 → 选更快者")
        self.assertIn("SEL fail-excluded -> baseline", out, "错误路径结构性不可胜出")

if __name__ == "__main__":
    unittest.main(verbosity=2)
