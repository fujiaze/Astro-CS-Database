#!/usr/bin/env python3
"""CPU-003 golden: hardware/benchmark/verify-profile CLI + quick/full + v2 profile 有效性。"""
import hashlib, json, os, re, shutil, subprocess, tempfile, unittest

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
CLI = os.path.join(REPO, "cli")
BUILD = os.path.join(REPO, "build", "cli")
EXE = os.path.join(BUILD, "astrocs")
VALIDATOR = os.path.join(REPO, "tools", "validate_cpu_profile.py")
def binary_commit_full():
    """二进制内嵌构建 commit(40hex): --version 的 +g<hash12> → git 全哈希。"""
    r = subprocess.run([EXE, "--version"], capture_output=True, text=True, timeout=30)
    m = re.search(r"\+g([0-9a-f]{12})", r.stdout)
    assert m, r.stdout
    full = subprocess.run(["git", "-C", REPO, "rev-parse", m.group(1)],
                          capture_output=True, text=True).stdout.strip()
    return full


def run(*args, timeout=240):
    return subprocess.run([EXE, *args], capture_output=True, text=True, timeout=timeout)


def binary_commit_prefix():
    """二进制内嵌构建 commit(12hex): 来自 --version 输出 +g<hash12>。"""
    r = subprocess.run([EXE, "--version"], capture_output=True, text=True, timeout=30)
    m = re.search(r"\+g([0-9a-f]{12})", r.stdout)
    assert m, r.stdout
    return m.group(1)


class TestBenchCli(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        assert os.path.isfile(EXE), "先构建 CLI"
        cls.tmp = tempfile.mkdtemp(prefix="bench_cli_")

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.tmp, ignore_errors=True)

    def test_01_benchmark_quick_profile_v2_valid(self):
        out = os.path.join(self.tmp, "profile_quick.json")
        r = run("benchmark", "cpu", "--quick", "--output", out)
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertEqual(r.stdout.strip(), f"{out} PASS")
        d = json.load(open(out, encoding="utf-8"))
        # v2 schema 顶层
        self.assertEqual(d["schema"], "astrocs.cpu-profile/v2")
        self.assertRegex(d["profile_id"], r"^sha256:[0-9a-f]{64}$")
        for k in ("created_utc", "host", "build", "memory_bandwidth",
                  "raw_samples_sha256", "kernels"):
            self.assertIn(k, d)
        # 至少 1 kernel, 每 kernel 规格字段
        self.assertGreaterEqual(len(d["kernels"]), 1)
        for kid, kp in d["kernels"].items():
            self.assertIn("workload_class", kp)
            self.assertIn(kp["provider"], ("baseline", "avx2", "avx512"))
            self.assertGreaterEqual(kp["workers"], 1)
            self.assertGreaterEqual(kp["block"], 1)
            self.assertRegex(kp["self_test_sha256"], r"^[0-9a-f]{64}$")
            self.assertGreater(kp["median"], 0)
            self.assertGreaterEqual(kp["mad"], 0)
        # verify-profile 复读通过
        v = run("benchmark", "verify-profile", out)
        self.assertEqual(v.returncode, 0, v.stderr)
        vd = json.loads(v.stdout)
        self.assertEqual(vd["verdict"], "PASS")

    def test_02_benchmark_full_twelve_kernels(self):
        out = os.path.join(self.tmp, "profile_full.json")
        r = run("benchmark", "cpu", "--full", "--output", out, timeout=300)
        self.assertEqual(r.returncode, 0, r.stderr)
        d = json.load(open(out, encoding="utf-8"))
        self.assertEqual(d["schema"], "astrocs.cpu-profile/v2")
        self.assertEqual(len(d["kernels"]), 12, "full=全 12 注册 kernel(06 §7)")
        # 无 fallback(2 核 VM 上全部 Oracle pass)
        for kid, kp in d["kernels"].items():
            self.assertIsNotNone(kp.get("median"), f"{kid} median 缺失")
        v = run("benchmark", "verify-profile", out)
        self.assertEqual(v.returncode, 0, v.stderr)

    def test_03_benchmark_mode_flag_required(self):
        r = run("benchmark", "cpu", "--output", os.path.join(self.tmp, "x.json"))
        self.assertEqual(r.returncode, 2)
        r2 = run("benchmark", "cpu", "--quick", "--full")
        self.assertEqual(r2.returncode, 2)

    def test_04_benchmark_bounded_runtime(self):
        """timeout golden: quick 模式在受限时间内完成(单调钟, 无阻塞)。"""
        import time
        out = os.path.join(self.tmp, "bounded.json")
        t0 = time.monotonic()
        r = run("benchmark", "cpu", "--quick", "--output", out, timeout=60)
        elapsed = time.monotonic() - t0
        self.assertEqual(r.returncode, 0)
        self.assertLess(elapsed, 30, f"quick 应 <30s, 实测 {elapsed:.1f}s")

    def test_05_doctor_json_checks(self):
        r = run("doctor", "--json")
        self.assertEqual(r.returncode, 0, r.stderr)
        d = json.loads(r.stdout)
        self.assertEqual(d["kind"], "astrocs_doctor")
        self.assertEqual(d["verdict"], "PASS")
        names = [c["name"] for c in d["checks"]]
        self.assertIn("baseline_selftest", names)
        self.assertIn("hardware_sanity", names)
        for c in d["checks"]:
            self.assertIn(c["status"], ("pass", "skipped"), f"{c['name']} 应通过或跳过")

    def test_06_hardware_and_doctor_single_json(self):
        for args in (("hardware", "inspect", "--json"), ("doctor", "--json")):
            r = run(*args)
            json.loads(r.stdout)  # 恰一文档
            self.assertNotIn("CRASH", r.stderr, f"{args} 异常")
            self.assertNotIn("unknown command", r.stderr, f"{args} 参数误判")

    def test_07_cli_sha256_matches_binary(self):
        """profile 内 benchmark_binary_sha256 = 运行中可执行文件 hash(08 §4)。"""
        out = os.path.join(self.tmp, "p.json")
        run("benchmark", "cpu", "--quick", "--output", out)
        d = json.load(open(out, encoding="utf-8"))
        cli_sha = d["build"]["benchmark_binary_sha256"]
        bin_path = os.path.realpath(EXE)
        sha = hashlib.sha256(open(bin_path, "rb").read()).hexdigest()
        self.assertEqual(cli_sha, sha, "benchmark_binary_sha256 必须是可执行文件实测 hash")

    def test_08_verify_profile_negative(self):
        """verify-profile 负例: 篡改 schema/workers/commit 必须 FAIL(exit=8)。"""
        good = os.path.join(self.tmp, "good.json")
        run("benchmark", "cpu", "--quick", "--output", good)
        # schema 篡改
        d = json.load(open(good, encoding="utf-8"))
        d["schema"] = "astrocs.cpu-profile/v1"
        bad1 = os.path.join(self.tmp, "bad_schema.json")
        json.dump(d, open(bad1, "w"))
        r = run("benchmark", "verify-profile", bad1)
        self.assertEqual(r.returncode, 8, r.stderr)
        # workers=0
        d = json.load(open(good, encoding="utf-8"))
        first = next(iter(d["kernels"]))
        d["kernels"][first]["workers"] = 0
        bad2 = os.path.join(self.tmp, "bad_workers.json")
        json.dump(d, open(bad2, "w"))
        r = run("benchmark", "verify-profile", bad2)
        self.assertEqual(r.returncode, 8, r.stderr)
        # commit 不匹配(二进制内嵌 commit ≠ 篡改值)
        d = json.load(open(good, encoding="utf-8"))
        d["build"]["source_commit"] = "0" * 40
        bad3 = os.path.join(self.tmp, "bad_commit.json")
        json.dump(d, open(bad3, "w"))
        r = run("benchmark", "verify-profile", bad3)
        self.assertEqual(r.returncode, 8, r.stderr)

    def test_09_events_jsonl_raw_candidates(self):
        """--events-jsonl 输出全部原始候选事件(CPU-003 规格: 保存全部原始候选)。"""
        out = os.path.join(self.tmp, "ev.json")
        evl = os.path.join(self.tmp, "ev.jsonl")
        with open(evl, "w") as f:
            r = subprocess.run([EXE, "benchmark", "cpu", "--quick", "--output", out,
                                "--events-jsonl"], stdout=f, stderr=subprocess.PIPE,
                               text=True, timeout=120)
        self.assertEqual(r.returncode, 0, r.stderr)
        lines = [json.loads(l) for l in open(evl) if l.strip()]
        raw = [e for e in lines if e.get("message") == "raw candidate"]
        self.assertGreaterEqual(len(raw), 2, "quick 应至少 2 个原始候选(1 kernel × ≥2 workers)")
        for e in raw:
            self.assertIn("provider", e)
            self.assertIn("workers", e)
            self.assertIn("median_ns", e)
            self.assertIn("oracle_pass", e)
        written = [e for e in lines if e.get("message") == "cpu profile written"]
        self.assertEqual(len(written), 1)
        self.assertIn("profile_id", written[0])

    # ── B8-P1-2: verdict 字段 + FAIL 退出码语义 ──

    def test_10_benchmark_profile_has_top_level_verdict(self):
        """v2 profile 输出 JSON 顶层 verdict 字段登记(B8-P1-2)。"""
        out = os.path.join(self.tmp, "verdict.json")
        r = run("benchmark", "cpu", "--quick", "--output", out)
        self.assertEqual(r.returncode, 0, r.stderr)
        d = json.load(open(out, encoding="utf-8"))
        self.assertIn("verdict", d, "顶层 verdict 字段必须登记")
        self.assertIn(d["verdict"], ("PASS", "FAIL"))
        # 机器一致性: verdict = PASS ⇔ 全部 kernel oracle:pass(实现单源推导)
        all_pass = bool(d["kernels"]) and all(
            k.get("correctness_test") == "oracle:pass"
            for k in d["kernels"].values())
        self.assertEqual(d["verdict"], "PASS" if all_pass else "FAIL")
        self.assertEqual(r.stdout.strip().split()[-1], d["verdict"])

    def test_11_verdict_fail_exit_nonzero_semantics(self):
        """verdict 推导与退出码语义一致性(B8-P1-2)。

        全 fail fixture → verdict=FAIL + exit=SCIENCE(4) 的黄金断言在
        tests/unit/cli_bench_verdict_test.cpp 以 dispatch 同源函数
        benchmark_profile_verdict 固化(ctest cli_bench_verdict)。
        本端到端断言绑定可观测合同:
        1) 生成侧: verdict=PASS ⇒ exit 0, stdout 尾 token 与 verdict 一致;
        2) 分层口径: verify-profile 是结构/机器一致性复读(oracle:fail 结构
           合法, rc=0); oracle:fail 的执行资格(baseline 回退)属 CPU-005
           select 路由(lib/backend_host, tests/cpu dispatch 域 T7/T8 已覆盖),
           show-effective 的 kernel_routes 是 CPU-004 展示摘要不执行资格 ——
           CLI 域不重复断言 lib 层行为。
        """
        out = os.path.join(self.tmp, "g2.json")
        r = run("benchmark", "cpu", "--quick", "--output", out)
        self.assertEqual(r.returncode, 0, r.stderr)
        d = json.load(open(out, encoding="utf-8"))
        self.assertEqual(d["verdict"], "PASS")
        self.assertEqual(r.stdout.strip().split()[-1], "PASS")
        # 全 kernel 篡改为 oracle:fail → verify-profile 结构合法(不拒收)
        d["kernels"] = {kid: {**kp, "correctness_test": "oracle:fail"}
                        for kid, kp in d["kernels"].items()}
        bad = os.path.join(self.tmp, "all_fail.json")
        json.dump(d, open(bad, "w", encoding="utf-8"))
        v = run("benchmark", "verify-profile", bad)
        self.assertEqual(v.returncode, 0, "verify-profile 仅结构校验(oracle:fail 合法)")


if __name__ == "__main__":
    unittest.main(verbosity=2)
