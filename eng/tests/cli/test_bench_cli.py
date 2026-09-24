#!/usr/bin/env python3
"""CPU-003/§6.2 golden: 唯一 exe 的 benchmark（单命令）+ doctor --json。

权威: ASTROCS_DESIGN §6.2（命令树只有 benchmark，无 cpu/--quick/--full/--output/
verify-profile 子面；benchmark 直接生成/更新安装目录 cpu_profile）、§8（cpu_profile
绑定 CPU 特征/provider 哈希）、docs/api/CLI_PROTOCOL_V1.md §1。

退役登记（CLI-001 删除的旧命令面，依据 §6.2 唯一命令树 + CLI-001 rc 矩阵）:
  * test_01_benchmark_quick_profile_v2_valid、test_02_benchmark_full_twelve_kernels、
    test_07_cli_sha256_matches_binary、test_10/test_11:
    旧面 benchmark cpu (--quick|--full) [--output <path>] → 断言语义改新树单命令
    benchmark（profile 落安装目录），保留 v2 schema/profile_id/kernel 规格/verdict/
    binary sha256 全部断言；
  * test_03_benchmark_mode_flag_required → 改断言「旧子命令/旧模式旗标一律 rc=2」；
  * test_04_benchmark_bounded_runtime: quick 模式随 §6.2 删除（无模式旗标），
    运行时长门禁退役（现网 profile 生成是 full 单模式，时长由 CPU-003 域自证）；
  * test_08_verify_profile_negative、test_09_events_jsonl_raw_candidates:
    benchmark verify-profile / --events-jsonl 子面已删除 → 改断言 rc=2；
  * test_05_doctor_json_checks 保留；test_06_hardware_and_doctor_single_json:
    hardware inspect 已删除 → doctor 断言保留 + rc=2 断言。
"""
import functools, hashlib, json, os, subprocess, unittest

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))

# FIX-UTCLI-HYGIENE: 子进程 cwd 统一落 run/（gitignore），见 cli_test_hygiene.py
from cli_test_hygiene import run_cwd  # noqa: E402


def cli_binary():
    env = os.environ.get("ASTROCS_CLI_BIN")
    if env and os.path.isfile(env):
        return env
    for rel in (("build", "acsd"), ("build", "cli", "acsd")):
        cand = os.path.join(REPO, *rel)
        if os.path.isfile(cand):
            return cand
    return os.path.join(REPO, "build", "acsd")


EXE = cli_binary()


def run(*args, timeout=600):
    return subprocess.run([EXE, *args], capture_output=True, text=True, timeout=timeout,
                          cwd=run_cwd())


@functools.lru_cache(maxsize=1)
def benchmark_once():
    """跑一次真 benchmark（§6.2 单命令；profile 落安装目录）并缓存结果。

    返回 (CompletedProcess, profile_path, doc)。供 profile 断言复用，避免重复跑 12 kernel。
    """
    r = run("benchmark")
    # stdout 形如 "<install_dir>/cpu_profile.json PASS|FAIL"；路径可能含空格,
    # 故按最后一个空白切分 verdict，其余整体为路径。
    path = ""
    if r.stdout.strip():
        parts = r.stdout.strip().rsplit(None, 1)
        path = parts[0] if len(parts) == 2 else parts[0]
    doc = None
    if path and os.path.isfile(path):
        with open(path, encoding="utf-8") as fh:
            doc = json.load(fh)
    return r, path, doc


class TestBenchCli(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        assert os.path.isfile(EXE), "先构建 CLI（cmake -S . -B build && ninja -C build acsd）"

    # ── 1. 单命令 profile 有效性（v2 schema / kernel 规格） ──
    def test_01_benchmark_profile_v2_valid(self):
        r, path, d = benchmark_once()
        self.assertIn(r.returncode, (0, 4),
                      "benchmark 退出码单源: PASS→0 / FAIL→SCIENCE(4); 实测 %s: %s"
                      % (r.returncode, r.stderr[-300:]))
        self.assertTrue(path and os.path.isfile(path), "benchmark 必须落安装目录 profile: %r" % path)
        self.assertIsNotNone(d)
        self.assertEqual(d["schema"], "astrocs.cpu-profile/v2")
        self.assertRegex(d["profile_id"], r"^sha256:[0-9a-f]{64}$")
        for k in ("created_utc", "host", "build", "memory_bandwidth",
                  "raw_samples_sha256", "kernels", "verdict"):
            self.assertIn(k, d)
        self.assertGreaterEqual(len(d["kernels"]), 1)
        for kid, kp in d["kernels"].items():
            self.assertIn("workload_class", kp, kid)
            self.assertIn(kp["provider"], ("baseline", "avx2", "avx512"), kid)
            self.assertGreaterEqual(kp["workers"], 1, kid)
            self.assertGreaterEqual(kp["block"], 1, kid)
            self.assertRegex(kp["self_test_sha256"], r"^[0-9a-f]{64}$", kid)
            if kp.get("correctness_test") == "oracle:pass":
                self.assertGreater(kp["median"], 0, kid)
                self.assertGreaterEqual(kp["mad"], 0, kid)

    # ── 2. verdict 单源 + 退出码语义（B8-P1-2） ──
    def test_02_verdict_single_source_and_exit_semantics(self):
        r, _path, d = benchmark_once()
        self.assertIsNotNone(d)
        all_pass = bool(d["kernels"]) and all(
            k.get("correctness_test") == "oracle:pass" for k in d["kernels"].values())
        self.assertEqual(d["verdict"], "PASS" if all_pass else "FAIL",
                         "verdict 必须与全部 kernel oracle 结果单源一致")
        self.assertEqual(r.returncode, 0 if d["verdict"] == "PASS" else 4,
                         "exit 语义: PASS→0 / FAIL→SCIENCE(4)")
        self.assertEqual(r.stdout.strip().split()[-1], d["verdict"],
                         "stdout 尾 token 必须与 profile verdict 一致")
        for kid, kp in d["kernels"].items():
            if kp.get("correctness_test") == "oracle:fail":
                self.assertEqual(kp.get("median"), 0.0, "%s: 错误候选不计时(CPU-003)" % kid)
                self.assertIn(kp.get("fallback_reason"),
                              ("no passing provider", "oracle_failed"),
                              "%s: fallback 证据缺失" % kid)

    # ── 3. 旧子命令/旧模式旗标一律 rc=2（§6.2 唯一命令树） ──
    def test_03_deleted_subcommands_and_mode_flags_exit_2(self):
        tmp = run_cwd()
        out = os.path.join(tmp, "bench_should_not_exist.json")
        cases = [("benchmark", "cpu"),
                 ("benchmark", "cpu", "--quick"),
                 ("benchmark", "cpu", "--full", "--output", out),
                 ("benchmark", "verify-profile", out),
                 ("benchmark", "--quick"),
                 ("benchmark", "--full"),
                 ("benchmark", "--output", out),
                 ("hardware", "inspect", "--json")]
        for case in cases:
            r = run(*case, timeout=60)
            self.assertEqual(r.returncode, 2,
                             "旧命令/旗标必须 rc=2: acsd %s → %s" % (" ".join(case),
                                                                        r.returncode))
            self.assertEqual(r.stdout, "", "参数错误 stdout 不得有输出")
            self.assertFalse(os.path.isfile(out), "被拒命令不得产出 profile 文件")

    # ── 4. doctor --json（§6.2 环境自检） ──
    def test_04_doctor_json_checks(self):
        r = run("doctor", "--json", timeout=120)
        self.assertEqual(r.returncode, 0, r.stderr)
        d = json.loads(r.stdout)
        self.assertEqual(d["kind"], "astrocs_doctor")
        self.assertEqual(d["verdict"], "PASS")
        names = [c["name"] for c in d["checks"]]
        self.assertIn("baseline_selftest", names)
        self.assertIn("hardware_sanity", names)
        for c in d["checks"]:
            self.assertIn(c["status"], ("pass", "skipped"), "%s 应通过或跳过" % c["name"])

    def test_05_doctor_single_json_document(self):
        r = run("doctor", "--json", timeout=120)
        lines = [l for l in r.stdout.splitlines() if l.strip()]
        self.assertGreaterEqual(len(lines), 1)
        json.loads(r.stdout)  # 整体恰一 JSON 文档
        self.assertNotIn("CRASH", r.stderr)
        # doctor 无 --json → 2（§6.2 只登记 doctor [--json]）
        r2 = run("doctor", timeout=60)
        self.assertEqual(r2.returncode, 2)
        self.assertEqual(r2.stdout, "")

    # ── 5. profile 内 binary sha256 = 可执行文件实测 hash（08 §4） ──
    def test_06_cli_sha256_matches_binary(self):
        _r, _path, d = benchmark_once()
        self.assertIsNotNone(d)
        cli_sha = d["build"]["benchmark_binary_sha256"]
        sha = hashlib.sha256(open(os.path.realpath(EXE), "rb").read()).hexdigest()
        self.assertEqual(cli_sha, sha,
                         "benchmark_binary_sha256 必须是可执行文件实测 hash")


if __name__ == "__main__":
    unittest.main(verbosity=2)
