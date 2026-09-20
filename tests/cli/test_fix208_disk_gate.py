#!/usr/bin/env python3
"""FIX-208 验收 4/5/6：资源门收窄为**磁盘门**；内存/CPU/线程不设门；exit 10 收窄。

权威（逐条）：
  * ASTROCS_DESIGN.md §3.5「资源门只管磁盘：运行前磁盘余量不足 ⇒ 报 warn（不阻断）；
    运行中写盘失败/磁盘满 ⇒ 报错（fail-closed）；内存 / CPU / 线程不设门」；
  * ASTROCS_DESIGN.md §6.3 退出码表「10 = 磁盘写满 / 写盘失败（一般性资源超限门已取消）」；
  * GAP_AUDIT.md(RELEASE-02) §9.74 裁决 10（负责人逐字：「不应该有资源超限（除非存储不足）。
    那个问题直接在跑前报 warn，写入磁盘满了报错……只考虑磁盘写满这一个问题」）。

方法：
  * 单元：编译共址 C++ 探针直接调 lib/infrastructure/cli/disk_gate.h 与 resource_gate.h
    （正/负例：ENOSPC→disk_full、EIO→io_failure、余量足→无 warn、余量不足→warn、
    gate_enforcement 恒 RecordOnly）；
  * 端到端：真实小 tmpfs（unshare -Ur -m + mount -t tmpfs -o size=1M，无需 root；
    权限不足时 skip 并给出原因）⇒ 运行中磁盘满必须 rc=10 + error 事件（fail-closed）；
    跑前余量不足必须 warn 且**不阻断**（运行确实开始）。
"""
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
import textwrap
import unittest

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, REPO)

from tests.cli.cli_test_hygiene import run_cwd  # noqa: E402


def cli_binary():
    env = os.environ.get("ASTROCS_CLI_BIN")
    if env and os.path.isfile(env):
        return env
    return os.path.join(REPO, "build", "astrocs")


EXE = cli_binary()


def unshare_available():
    if os.name != "posix" or not shutil.which("unshare"):
        return False
    r = subprocess.run(["unshare", "-Ur", "-m", "--propagation", "private", "true"],
                       capture_output=True, text=True, timeout=60)
    return r.returncode == 0


UNSHARE = unshare_available()


class TestDiskGateUnit(unittest.TestCase):
    """disk_gate.h + resource_gate.h 判定单元（正/负例，能红能绿）。"""

    @classmethod
    def setUpClass(cls):
        if not shutil.which("g++"):
            raise unittest.SkipTest("需要 g++")
        cls.tmp = tempfile.mkdtemp(prefix="fix208_disk_")
        src = os.path.join(cls.tmp, "probe.cpp")
        with open(src, "w", encoding="utf-8") as fh:
            fh.write(r"""
#include "disk_gate.h"
#include "resource_gate.h"
#include <cerrno>
#include <cstdio>
#include <string>
int main(int argc, char** argv) {
    using namespace astrocs;
    const std::string mode = (argc > 1) ? argv[1] : "";
    if (mode == "classify") {
        std::printf("enospc=%s\n", write_failure_kind_name(classify_write_failure(ENOSPC)));
        std::printf("edquot=%s\n", write_failure_kind_name(classify_write_failure(EDQUOT)));
        std::printf("efbig=%s\n", write_failure_kind_name(classify_write_failure(EFBIG)));
        std::printf("eio=%s\n", write_failure_kind_name(classify_write_failure(EIO)));
        std::printf("zero=%s\n", write_failure_kind_name(classify_write_failure(0)));
        std::printf("exit_diskfull=%d\n", write_failure_is_resource_exit(WriteFailureKind::DiskFull) ? 1 : 0);
        std::printf("exit_writefailed=%d\n", write_failure_is_resource_exit(WriteFailureKind::WriteFailed) ? 1 : 0);
        std::printf("exit_io=%d\n", write_failure_is_resource_exit(WriteFailureKind::IoFailure) ? 1 : 0);
        std::printf("exit_none=%d\n", write_failure_is_resource_exit(WriteFailureKind::None) ? 1 : 0);
        return 0;
    }
    if (mode == "precheck") {
        DiskEstimate est; est.input_bytes = 2ull * 1024 * 1024; est.files = 3;
        DiskSpace low; low.probed = true; low.free_bytes = 1ull * 1024 * 1024;
        DiskSpace high; high.probed = true; high.free_bytes = 4ull * 1024 * 1024;
        DiskSpace unknown; unknown.probed = false; unknown.err = EACCES;
        std::printf("low=%s\n", disk_precheck_warning(low, est, "/out").empty() ? "none" : "warn");
        std::printf("high=%s\n", disk_precheck_warning(high, est, "/out").empty() ? "none" : "warn");
        std::printf("unknown=%s\n", disk_precheck_warning(unknown, est, "/out").empty() ? "none" : "warn");
        std::printf("line=%s\n", disk_estimate_line(high, est, "/out").c_str());
        return 0;
    }
    if (mode == "space") {
        DiskSpace sp = disk_space_of(argv[2]);
        std::printf("probed=%d\n", sp.probed ? 1 : 0);
        std::printf("free_gt0=%d\n", sp.free_bytes > 0 ? 1 : 0);
        DiskSpace nested = disk_space_of(std::string(argv[2]) + "/a/b/c/not_created_yet");
        std::printf("nested_probed=%d\n", nested.probed ? 1 : 0);
        return 0;
    }
    if (mode == "scopes") {
        const std::string text = argv[2];
        auto doc = nlohmann::json::parse(text);
        auto flat = disk_scopes(doc, "input_lights", "", true);
        std::printf("flat_n=%zu\n", flat.size());
        std::printf("flat_bytes_gt0=%d\n", (!flat.empty() && flat[0].est.input_bytes > 0) ? 1 : 0);
        std::printf("flat_out=%s\n", flat.empty() ? "" : flat[0].output_dir.c_str());
        auto blocks = disk_scopes(doc, "input_lights", "", true);
        std::printf("blocks_n=%zu\n", blocks.size());
        return 0;
    }
    if (mode == "enforcement") {
        // §9.74 裁决 10: 一般性资源超限门已取消 ⇒ 恒 RecordOnly（含 strict + 违规）
        std::printf("strict_violation=%s\n",
                    gate_enforcement_name(gate_enforcement(true, GateDiag::LowAvgCores)));
        std::printf("strict_memory=%s\n",
                    gate_enforcement_name(gate_enforcement(true, GateDiag::MemoryGrowth)));
        std::printf("default_violation=%s\n",
                    gate_enforcement_name(gate_enforcement(false, GateDiag::CpuMeanLow)));
        std::printf("strict_ok=%s\n",
                    gate_enforcement_name(gate_enforcement(true, GateDiag::Ok)));
        return 0;
    }
    if (mode == "probe") {
        WriteProbe pr = probe_writable(argv[2]);
        std::printf("kind=%s\n", write_failure_kind_name(pr.kind));
        std::printf("err=%d\n", pr.err);
        return 0;
    }
    std::printf("unknown-mode\n");
    return 2;
}
""")
        cls.exe = os.path.join(cls.tmp, "probe")
        r = subprocess.run(["g++", "-std=c++17", "-O1", "-w",
                            "-I" + os.path.join(REPO, "lib", "infrastructure", "cli"),
                            "-I" + os.path.join(REPO, "third_party"),
                            "-I" + os.path.join(REPO, "build"),
                            src, "-o", cls.exe], capture_output=True, text=True, timeout=300)
        assert r.returncode == 0, r.stderr[-1200:]

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.tmp, ignore_errors=True)

    def _run(self, *args):
        r = subprocess.run([self.exe] + list(args), capture_output=True, text=True, timeout=120)
        self.assertEqual(r.returncode, 0, r.stderr[-400:])
        return dict(l.split("=", 1) for l in r.stdout.splitlines() if "=" in l)

    def test_01_write_failure_classification(self):
        """写失败归类：ENOSPC/EDQUOT → disk_full；EFBIG → write_failed；EIO → io_failure。"""
        out = self._run("classify")
        self.assertEqual(out["enospc"], "disk_full")
        self.assertEqual(out["edquot"], "disk_full")
        self.assertEqual(out["efbig"], "write_failed")
        self.assertEqual(out["eio"], "io_failure")          # 阴性对照: 非磁盘满不冒充
        self.assertEqual(out["zero"], "none")
        self.assertEqual(out["exit_diskfull"], "1")
        self.assertEqual(out["exit_writefailed"], "1")
        self.assertEqual(out["exit_io"], "0", "一般 I/O 失败不得落 exit 10")
        self.assertEqual(out["exit_none"], "0")

    def test_02_precheck_warn_is_warn_not_error(self):
        """跑前余量不足 ⇒ warn（非阻断）；余量足 ⇒ 无 warn（阴性对照）。"""
        out = self._run("precheck")
        self.assertEqual(out["low"], "warn")
        self.assertEqual(out["high"], "none", "余量充足不得报 warn")
        self.assertEqual(out["unknown"], "warn", "无法探测 ⇒ warn（不阻断，不静默放过）")
        self.assertIn("MiB", out["line"])

    def test_03_disk_space_and_scopes(self):
        out = self._run("space", self.tmp)
        self.assertEqual(out["probed"], "1")
        self.assertEqual(out["free_gt0"], "1")
        self.assertEqual(out["nested_probed"], "1", "output_dir 未创建时必须向上探测")
        cfg = json.dumps({"schema_version": "1", "input_lights": ["/etc/hostname"],
                          "output_dir": "/tmp/out"})
        out2 = self._run("scopes", cfg)
        self.assertEqual(out2["flat_n"], "1")
        self.assertEqual(out2["flat_bytes_gt0"], "1")
        self.assertEqual(out2["flat_out"], "/tmp/out")

    def test_04_no_general_resource_gate_enforcement(self):
        """§9.74 裁决 10: strict + 违规也不得 enforce（内存/CPU/线程不设门）。"""
        out = self._run("enforcement")
        self.assertEqual(out["strict_violation"], "record_only")
        self.assertEqual(out["strict_memory"], "record_only")
        self.assertEqual(out["default_violation"], "record_only")
        self.assertEqual(out["strict_ok"], "record_only")


class TestDiskGateEndToEnd(unittest.TestCase):
    """真实小 tmpfs（unshare + mount，无需 root）⇒ 磁盘门两条臂。"""

    @classmethod
    def setUpClass(cls):
        assert os.path.isfile(EXE), "先构建 CLI（ninja -C build astrocs）"
        cls.tmp = tempfile.mkdtemp(prefix="fix208_disk_e2e_")
        cache = os.environ.get("ASTROCS_FIX208_FIXTURE_DIR")
        if cache and os.path.isfile(os.path.join(cache, "fixture")) and \
                os.path.isdir(os.path.join(cache, "data")):
            cls.data = os.path.join(cache, "data")
        else:
            from tests.cli.test_cli004_process_protocol import build_fixture
            fixture = build_fixture(cls.tmp)
            cls.data = os.path.join(cls.tmp, "data")
            os.makedirs(cls.data, exist_ok=True)
            r = subprocess.run([fixture, "--make", cls.data], capture_output=True,
                               text=True, timeout=300, cwd=run_cwd())
            assert "FIXTURES_OK" in r.stdout, r.stderr
        cls.mnt = os.path.join(cls.tmp, "tiny")
        os.makedirs(cls.mnt, exist_ok=True)
        # 大输入（稀疏 4 MiB，可读）用于触发「跑前余量不足」：预估需求下限 = 输入字节和
        cls.big = os.path.join(cls.tmp, "big_light.fits")
        with open(cls.big, "wb") as fh:
            fh.truncate(4 * 1024 * 1024)

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.tmp, ignore_errors=True)

    def _cfg(self, name, lights, out_dir):
        path = os.path.join(self.tmp, name)
        with open(path, "w", encoding="utf-8") as fh:
            json.dump({"schema_version": "1",
                       "input_lights": lights,
                       "master_bias": os.path.join(self.data, "bias.fits"),
                       "master_dark": os.path.join(self.data, "dark.fits"),
                       "master_flat": os.path.join(self.data, "flat.fits"),
                       "dark_optimization": True,
                       "output_dir": out_dir,
                       "wcs": {"crpix1": 32.5, "crpix2": 32.5, "crval1": 210.0,
                               "crval2": 34.0, "cd11": -2.7777777777777776e-4,
                               "cd12": 0.0, "cd21": 0.0, "cd22": 2.7777777777777776e-4},
                       "drizzle": {"nside": 512, "nested": 1, "pixfrac": 1.0,
                                   "precision_mode": 0}}, fh)
        return path

    def _run_in_tiny_tmpfs(self, cfg, tag, size="1M", extra_flags=()):
        """在 unshare 挂载命名空间内挂 size 的 tmpfs 并跑 CLI；返回 (rc, stdout, stderr)。"""
        out_log = os.path.join(self.tmp, tag + ".out")
        err_log = os.path.join(self.tmp, tag + ".err")
        flags = " ".join(extra_flags)
        script = textwrap.dedent("""
            set -e
            mkdir -p "{mnt}"
            mount -t tmpfs -o size={size} tmpfs "{mnt}"
            cd "{repo}"
            set +e
            "{exe}" normalize --json "{cfg}" -y {flags} > "{out_log}" 2> "{err_log}"
            echo CLI_RC=$?
        """).format(mnt=self.mnt, size=size, repo=REPO, exe=EXE, cfg=cfg,
                    flags=flags, out_log=out_log, err_log=err_log)
        r = subprocess.run(["unshare", "-Ur", "-m", "--propagation", "private",
                            "bash", "-c", script], capture_output=True, text=True, timeout=600,
                           cwd=run_cwd())
        self.assertIn("CLI_RC=", r.stdout, "unshare 运行失败: %s / %s" % (r.stdout, r.stderr[-400:]))
        rc = int(re.search(r"CLI_RC=(\d+)", r.stdout).group(1))
        with open(out_log, encoding="utf-8") as fh:
            out = fh.read()
        with open(err_log, encoding="utf-8") as fh:
            err = fh.read()
        return rc, out, err

    @unittest.skipUnless(UNSHARE, "需要 unshare -Ur -m（无 root 的用户命名空间挂载）")
    def test_01_runtime_disk_full_is_error_and_exit10(self):
        """运行中写盘失败/磁盘满 ⇒ error（fail-closed）+ exit 10。"""
        out_dir = os.path.join(self.mnt, "out_full")
        cfg = self._cfg("cfg_full.json",
                        [os.path.join(self.data, "light_1.fits"),
                         os.path.join(self.data, "light_2.fits")], out_dir)
        rc, out_s, err_s = self._run_in_tiny_tmpfs(cfg, "full")
        events = [json.loads(l) for l in out_s.splitlines() if l.strip()]
        self.assertEqual(rc, 10, "磁盘满必须 fail-closed 为 exit 10（实得 %d）" % rc)
        errs = [e for e in events if e.get("severity") == "error"]
        self.assertTrue(errs, "必须发 error 事件（运行中写盘失败不得静默）")
        disk = [e for e in errs if e.get("failure_kind") in ("disk_full", "write_failed")]
        self.assertTrue(disk, "error 事件必须带磁盘门判定字段 failure_kind: %s" % errs[:1])
        self.assertEqual(disk[0]["kind"], "resource")
        fin = events[-1]
        self.assertEqual((fin["kind"], fin["exit_code"]), ("final", 10),
                         "final 事件必须如实回填 exit_code=10")
        # 失败 run 不得留下看似完整的 manifest（不得是 0 字节/截断的成功形）
        man = [e for e in events if e.get("role") == "run_manifest"]
        if man:
            self.assertNotEqual(man[-1].get("size_bytes"), 0,
                                "manifest 不得在磁盘满时留 0 字节且仍报 written")

    @unittest.skipUnless(UNSHARE, "需要 unshare -Ur -m（无 root 的用户命名空间挂载）")
    def test_02_precheck_disk_warn_does_not_block(self):
        """跑前余量不足 ⇒ warn 且**不阻断**（运行确实开始）。"""
        out_dir = os.path.join(self.mnt, "out_warn")
        cfg = self._cfg("cfg_warn.json",
                        [os.path.join(self.data, "light_1.fits"), self.big], out_dir)
        rc, out_s, err_s = self._run_in_tiny_tmpfs(cfg, "warn")
        self.assertIn("[warn]", err_s, "余量不足必须报 warn（预检页）")
        self.assertIn("磁盘余量不足", err_s)
        self.assertNotIn("[error]", err_s.split("astrocs: disk")[0],
                         "磁盘 warn 不得升级为预检 error")
        events = [json.loads(l) for l in out_s.splitlines() if l.strip()]
        kinds = [e.get("kind") for e in events]
        self.assertTrue(events, "warn 不得阻断：运行必须已开始（有事件流）")
        self.assertIn("stage_start", kinds, "运行必须已进入会话（未被预检阻断）")
        self.assertIn("progress", kinds, "运行必须已启动管线（未被预检阻断）")
        self.assertNotIn(rc, (2,), "不得因磁盘 warn 落预检阻断码 2")

    def test_03_strict_resource_flag_no_longer_enforces(self):
        """--strict-resource-gate 保留接受但不再 enforce（登记现状，无 rc=10 路径）。"""
        out_dir = os.path.join(self.tmp, "out_strict")
        os.makedirs(out_dir, exist_ok=True)
        cfg = self._cfg("cfg_strict.json",
                        [os.path.join(self.data, "light_1.fits"),
                         os.path.join(self.data, "light_2.fits")], out_dir)
        r = subprocess.run([EXE, "normalize", "--json", cfg, "-y", "--strict-resource-gate"],
                           capture_output=True, text=True, timeout=600, cwd=run_cwd())
        self.assertEqual(r.returncode, 0, r.stderr[-400:])
        events = [json.loads(l) for l in r.stdout.splitlines() if l.strip()]
        self.assertNotIn(10, [e.get("exit_code") for e in events],
                         "不得再出现资源判据产生的 exit 10")
        for e in events:
            if e.get("kind") == "resource_gate":
                self.assertEqual(e.get("severity"), "warning")
                self.assertFalse(e.get("enforced"))
                self.assertEqual(e.get("resource_gate_mode"), "record_only")


if __name__ == "__main__":
    unittest.main(verbosity=2)
