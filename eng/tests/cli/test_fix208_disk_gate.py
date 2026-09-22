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

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
sys.path.insert(0, REPO)

from cli_test_hygiene import run_cwd  # noqa: E402


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
                            "-I" + os.path.join(REPO, "lib", "third_party"),
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


class TestFrameLevelAttributionUnit(unittest.TestCase):
    """磁盘满归因必须**帧级**：并发帧各自的判定互不覆盖、互不抢占（能红能绿）。

    权威：docs/contracts/LOG_AND_ERROR_CONTRACT.md §5「IO | 7（IO）| I/O 失败；
    **失败节点 manifest** 的 error_kind==disk_full 时改判 10」——判定属于**失败的
    那一帧/那个节点**，不是"进程里发生过一次磁盘满"的运行级事实。

    被测面 = 真实产品头 lib/infrastructure/aio/src/aio_disk_full.h（header-only）。
    旧实现（进程级单比特 + aio_hips_product_begin 里的 reset() + exchange 语义的
    consume()）在并发帧下互相覆盖 ⇒ 失败帧丢 error_kind ⇒ exit 7 的 fail-open
    （FLAKE-01 §A4：单帧 12/12 正确、双帧 58 次里 5 次 rc=7）。本单元把该竞态钉成
    **确定性**判据：不依赖调度、不依赖 tmpfs、不依赖 CLI。

      T1 串行对照：帧 A 的失败分类置位后，帧 A 自己的窗口必须 failed()；
      T2 不抹除  ：帧 A 判定已置位后帧 B 开窗（旧实现在此处 reset）⇒ A 仍 failed()；
      T3 不抢占  ：两帧**真并发**各自失败 ⇒ 两个窗口**都** failed()；
      T4 阴性对照：非空间类失败（EACCES）不得让任何窗口 failed()。

    负例注入实证：把 aio_disk_full.h 临时退回"开窗即复位全局 + 读后即清除"的旧语义
    ⇒ T2/T3 必判红（证据 run/AIOD-FIX-01/logs/neg_*）。
    """

    @classmethod
    def setUpClass(cls):
        if not shutil.which("g++"):
            raise unittest.SkipTest("需要 g++")
        cls.tmp = tempfile.mkdtemp(prefix="fix208_frame_attr_")
        src = os.path.join(cls.tmp, "frame_attr_probe.cpp")
        with open(src, "w", encoding="utf-8") as fh:
            fh.write(r"""
#include "aio_disk_full.h"
#include <cerrno>
#include <cstdio>
#include <thread>
int main() {
    // T1/T2: 串行两帧（帧 B 开窗 = 旧实现里 aio_hips_product_begin 的 reset 位点）
    aio_disk::FailureEpoch ep_a;
    const bool a_noted = aio_disk::note_failure("/nonexistent/astrocs/probe_a", ENOSPC);
    const bool a_before = ep_a.failed();
    aio_disk::FailureEpoch ep_b;
    const bool a_after = ep_a.failed();
    const bool b_failed = ep_b.failed();
    std::printf("a_noted=%d\n", a_noted ? 1 : 0);
    std::printf("a_before_b=%d\n", a_before ? 1 : 0);
    std::printf("a_after_b=%d\n", a_after ? 1 : 0);
    std::printf("b_failed=%d\n", b_failed ? 1 : 0);
    // T3: 两帧真并发各自失败（各自持有自己的归因窗口）
    int ca = 0, cb = 0;
    std::thread ta([&] {
        aio_disk::FailureEpoch e;
        aio_disk::note_failure("/nonexistent/astrocs/probe_ta", ENOSPC);
        ca = e.failed() ? 1 : 0;
    });
    std::thread tb([&] {
        aio_disk::FailureEpoch e;
        aio_disk::note_failure("/nonexistent/astrocs/probe_tb", ENOSPC);
        cb = e.failed() ? 1 : 0;
    });
    ta.join();
    tb.join();
    std::printf("conc_a=%d\n", ca);
    std::printf("conc_b=%d\n", cb);
    // T4: 阴性对照（文件系统仍有空间的 I/O 失败不得冒充磁盘满）
    aio_disk::FailureEpoch ep_c;
    const bool c_noted = aio_disk::note_failure("/nonexistent/astrocs/probe_c", EACCES);
    std::printf("c_noted=%d\n", c_noted ? 1 : 0);
    std::printf("c_failed=%d\n", ep_c.failed() ? 1 : 0);
    return 0;
}
""")
        cls.exe = os.path.join(cls.tmp, "frame_attr_probe")
        r = subprocess.run(["g++", "-std=c++17", "-O1", "-w", "-pthread",
                            "-I" + os.path.join(REPO, "lib", "infrastructure", "aio", "src"),
                            src, "-o", cls.exe],
                           capture_output=True, text=True, timeout=300)
        assert r.returncode == 0, r.stderr[-1200:]

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.tmp, ignore_errors=True)

    def test_01_frame_level_attribution_not_cross_covered(self):
        r = subprocess.run([self.exe], capture_output=True, text=True, timeout=120)
        self.assertEqual(r.returncode, 0, r.stderr[-400:])
        out = dict(l.split("=", 1) for l in r.stdout.splitlines() if "=" in l)
        self.assertEqual(out["a_noted"], "1", "ENOSPC 必须被真实判据分类")
        self.assertEqual(out["a_before_b"], "1", "T1: 帧 A 自己的窗口必须看到分类")
        self.assertEqual(out["a_after_b"], "1",
                         "T2: 帧 B 开窗不得抹掉帧 A 已置位的判定（旧实现必红）")
        self.assertEqual(out["b_failed"], "0", "T2: 帧 B 没有失败，不得被误判为 disk_full")
        self.assertEqual(out["conc_a"], "1",
                         "T3: 并发帧 A 的判定不得被抢占（旧实现必红）")
        self.assertEqual(out["conc_b"], "1", "T3: 并发帧 B 的判定不得被抢占")
        self.assertEqual(out["c_noted"], "0", "T4: EACCES 不是磁盘满")
        self.assertEqual(out["c_failed"], "0",
                         "T4: 非空间类失败不得被判为 disk_full（否则 exit 7 被误升为 10）")

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
            from test_cli004_process_protocol import build_fixture
            fixture = build_fixture(cls.tmp)
            cls.data = os.path.join(cls.tmp, "data")
            os.makedirs(cls.data, exist_ok=True)
            r = subprocess.run([fixture, "--make", cls.data], capture_output=True,
                               text=True, timeout=300, cwd=run_cwd())
            assert "FIXTURES_OK" in r.stdout, r.stderr
        # 并发帧数：判据从 2 帧提到 **4 帧**（FLAKE-01 §A7 建议）——该竞态是
        # 帧间调度交错引起的，帧数 ↑ ⇒ 交错窗口 ↑ ⇒ 检出率 ↑。fixture 只产
        # light_1/light_2，这里按字节复制出 light_3/light_4（加性：不改共享
        # fixture，也不依赖 fixture 缓存目录可写）。
        cls.lights4 = []
        lights_dir = os.path.join(cls.tmp, "lights4")
        os.makedirs(lights_dir, exist_ok=True)
        src2 = [os.path.join(cls.data, "light_1.fits"),
                os.path.join(cls.data, "light_2.fits")]
        for i, name in enumerate(("light_1.fits", "light_2.fits",
                                  "light_3.fits", "light_4.fits")):
            dst = os.path.join(lights_dir, name)
            shutil.copyfile(src2[i % 2], dst)
            cls.lights4.append(dst)
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

    @staticmethod
    def _classify_not_exit10(rc, events, err_s):
        """rc≠10 的**显式归因**（FLAKE-01：把"偶发 7 != 10"变成可读的真判据失败）。

        背景（一手证据 run/FLAKE-01/）：本用例在满负载（jobs=8）下曾偶发判红而单跑必绿，
        容易被当成噪声。实测根因**不是**判据不稳（场景是私有挂载命名空间里的 1 MiB
        tmpfs，与宿主全盘可用空间无关，也不受无关进程写盘影响），而是产品在**并发帧**
        下丢失磁盘满分类：lib/infrastructure/aio/src/aio_disk_full.h 的进程级粘滞标志
        在失败发生处 note_full() 置位后，被另一帧的 aio_hips_product_begin() 里的
        aio_disk::reset() 抹掉，失败收尾的 consume() 取不到 ⇒ 失败节点 manifest 无
        error_kind="disk_full" ⇒ CLI 落 exit 7(IO) 而非 exit 10(RESOURCE)。
        确定性复现：run/FLAKE-01/evidence/aio_disk_protocol_demo.cpp（真实头文件）；
        剂量-反应：单帧 12/12 正确、双帧 58 次中 5 次 rc=7 且无 failure_kind 事件
        （run/FLAKE-01/logs/a_dose.log、a_f2_40.log；宿主 8 个 CPU 自旋时 20/20 正确
        ⇒ 触发条件是帧间调度交错，不是宿主压力）。
        ⇒ 判据保留（ASTROCS_DESIGN §3.5/§6.3 要求 fail-closed 为 exit 10），
        但失败必须**自解释**，不得被当作 flake 忽略。

        该竞态已修：磁盘满分类改为**帧级归因窗口**（lib/infrastructure/aio/src/
        aio_disk_full.h 的 FailureEpoch = 执行流局部的单调计数快照；product_begin
        不再 reset、失败收尾不再用 exchange 语义抢占）⇒ 并发帧各自的判定互不覆盖。
        以下分支保留为**回归判据**：若再次走到这里，说明帧级归因失效（不是 flake）。
        """
        disk_ev = [e for e in events if e.get("severity") == "error"
                   and e.get("failure_kind") in ("disk_full", "write_failed")]
        if disk_ev:
            return ("事件流已给出 failure_kind=%s，但退出码未归并为 10 ⇒ CLI 归并路径缺陷"
                    % disk_ev[0].get("failure_kind"))
        if rc == 7 and "aio_hips_write_signal_support_tile" in err_s:
            return ("磁盘满分类在失败收尾处**丢失**（事件流无 failure_kind 事件、rc=7=IO，"
                    "而 stderr 显示真实写失败 aio_hips_write_signal_support_tile rc=-4）"
                    "⇒ 产品并发缺陷（粘滞标志被并发的 aio_hips_product_begin 的 reset() "
                    "抹掉），**不是假红**，不得按 flake 处理")
        return "事件流无磁盘门判定字段（rc=%d）⇒ 需按 stderr 逐条定位" % rc

    def _dump_failure_evidence(self, tag, out_s, err_s):
        """失败时把 CLI 事件流/stderr 落盘（tmp 会被 tearDownClass 删除）。"""
        root = os.environ.get("ASTROCS_CI_OUT_ROOT") or os.path.join(REPO, "run")
        d = os.path.join(root, "fix208_disk_gate_evidence")
        try:
            os.makedirs(d, exist_ok=True)
            for suffix, body in ((".out", out_s), (".err", err_s)):
                with open(os.path.join(d, tag + suffix), "w", encoding="utf-8") as fh:
                    fh.write(body)
        except OSError as exc:      # 证据落盘失败不影响判定本身（判据照常判红）
            sys.stderr.write("fix208_disk_gate: 证据落盘失败（不影响判定）：%s\n" % exc)
            return None
        return d

    @unittest.skipUnless(UNSHARE, "需要 unshare -Ur -m（无 root 的用户命名空间挂载）")
    def test_01_runtime_disk_full_is_error_and_exit10(self):
        """**4 帧并发**运行中写盘失败/磁盘满 ⇒ error（fail-closed）+ exit 10。

        并发帧数是本判据的检出率来源：FLAKE-01 §A4 的剂量-反应显示同一场景单帧
        12/12 正确、双帧 58 次里 5 次 rc=7（≈9%）；根因是磁盘满分类被并发帧抹掉
        （aio_disk_full.h 头注「归因粒度=帧级」）。故本用例跑 **4 帧**。
        """
        out_dir = os.path.join(self.mnt, "out_full")
        cfg = self._cfg("cfg_full.json", self.lights4, out_dir)
        rc, out_s, err_s = self._run_in_tiny_tmpfs(cfg, "full")
        events = [json.loads(l) for l in out_s.splitlines() if l.strip()]
        if rc != 10:
            diag = self._classify_not_exit10(rc, events, err_s)
            ev = self._dump_failure_evidence("disk_full_rc%d" % rc, out_s, err_s)
            self.fail("磁盘满必须 fail-closed 为 exit 10（实得 %d）。%s%s"
                      % (rc, diag, ("；证据已落 %s" % ev) if ev else ""))
        errs = [e for e in events if e.get("severity") == "error"]
        self.assertTrue(errs, "必须发 error 事件（运行中写盘失败不得静默）")
        disk = [e for e in errs if e.get("failure_kind") in ("disk_full", "write_failed")]
        self.assertTrue(disk, "error 事件必须带磁盘门判定字段 failure_kind: %s" % errs[:1])
        self.assertEqual(disk[0]["kind"], "resource")
        # (a) 失败**节点 manifest** 的归因必须落在磁盘满上。
        #     LOG_AND_ERROR_CONTRACT §5：「IO | 7（IO）| I/O 失败；失败节点 manifest
        #     的 error_kind==disk_full 时改判 10」。失败节点 manifest 是 CLI 进程内
        #     对象（runtime_client.cpp::g_manifests，无落盘出口），其**唯一**外部可观测
        #     代理 = CLI 自报的这条 resource 事件：rrc==RESOURCE 只可能来自
        #     pipeline_exit_code_from_error 读到某失败节点 manifest 的
        #     error_kind=="disk_full"（commands.cpp 的 rc==RESOURCE 分支逐字写出该归因）。
        #     断言它即断言「CLI 读到的失败节点 manifest 带 error_kind=disk_full」。
        self.assertEqual(disk[0].get("diag"), "disk_write_failure:disk_full")
        self.assertIn("node manifest error_kind=disk_full", disk[0].get("detail", ""),
                      "失败节点 manifest 必须带 error_kind=disk_full（否则是 exit 7 的 "
                      "fail-open）: %s" % disk[0])
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
        """--strict-resource-gate 保留接受但不再 enforce（登记现状，无 rc=10 路径）。

        场景必须是**资源判据命中违规**：只有违规被记录时才会出现 `resource_gate` 事件，
        判据也才有观测对象。这里用 MON-002 测试钩子（非用户接口，见
        lib/infrastructure/cli/runtime_client.cpp::run_pipeline）造低 CPU 假 workload
        ⇒ active 窗口 >10s、avg 等效核 ≈0 ⇒ evaluate_gate 判 low_avg_cores
        （或 first-10s 快速失败诊断）⇒ 事件必然出现。旧写法（不造违规）在快机器上
        事件根本不出现 ⇒ 断言空转（恒绿假判据），在慢机器上事件出现 ⇒ 断言恒红
        （RELEASE-05 D-14「未定位的瞬时红」的根因）。

        判据面 = `resource_gate` 事件的**合同冻结扩展字段**（逐字；两处机器面同面：
        lib/infrastructure/cli/protocol.h::missing_required_extension_v1 ↔
        eng/contracts/schemas/jsonl_event_v1.schema.json 的 then.required 与
        x-astrocs-event-kind-registry.kinds.resource_gate。人类可读合同
        docs/api/CLI_PROTOCOL_V1.md §4 只列了 5 类 kind 的扩展字段，未列本 kind）：
          diag / enforcement / strict / enforced / work_core_seconds /
          workload_floor_core_seconds / workload_floor_reached
        该 kind **没有** `resource_gate_mode` 字段（全仓 docs/ 零处规定该名字；
        旧断言把 `resource` 事件上的实现私有键搬到了 `resource_gate` 事件上 ⇒
        事件一出现必红）。record-only 处置的合同可观测量 = `enforcement`：取值来自
        lib/infrastructure/cli/resource_gate.h::gate_enforcement()（§9.74 裁决 10 恒
        RecordOnly）⇒ "record_only"；它是**派生**值，比硬编码字面量更强——若有人把
        enforce 路径接回来，它会变成 "enforced"，而硬编码字面量不会。
        """
        out_dir = os.path.join(self.tmp, "out_strict")
        os.makedirs(out_dir, exist_ok=True)
        cfg = self._cfg("cfg_strict.json",
                        [os.path.join(self.data, "light_1.fits"),
                         os.path.join(self.data, "light_2.fits")], out_dir)
        env = dict(os.environ)
        env["ASTROCS_TEST_PIPELINE_SLEEP_MS"] = "12000"   # 造违规（判据的观测对象）
        r = subprocess.run([EXE, "normalize", "--json", cfg, "-y", "--strict-resource-gate"],
                           capture_output=True, text=True, timeout=600, cwd=run_cwd(), env=env)
        self.assertEqual(r.returncode, 0, r.stderr[-400:])
        events = [json.loads(l) for l in r.stdout.splitlines() if l.strip()]
        self.assertNotIn(10, [e.get("exit_code") for e in events],
                         "不得再出现资源判据产生的 exit 10")
        gate_ev = [e for e in events if e.get("kind") == "resource_gate"]
        self.assertTrue(gate_ev,
                        "资源判据命中违规必须记录 resource_gate 事件（否则本判据空转）")
        for e in gate_ev:
            self.assertEqual(e.get("severity"), "warning")
            self.assertFalse(e.get("enforced"))
            self.assertEqual(e.get("enforcement"), "record_only",
                             "§9.74 裁决 10: 恒 record-only（合同冻结字段 enforcement）")
            self.assertTrue(e.get("strict"),
                            "--strict-resource-gate 必须如实入事件（合同冻结字段 strict）"
                            "，但不再改变裁决")


    def test_04_concurrent_frames_without_disk_full_succeed(self):
        """阴性对照（防假红）：**4 帧并发**、磁盘余量充足 ⇒ 必须成功 rc=0。

        修法（帧级归因窗口）不得把"并发"本身变成失败：没有磁盘满时任何一帧都不得被
        判为 disk_full（否则 exit 7 被误升为 10）。场景与 test_01 只差"有没有写满"
        （test_01 = 1 MiB tmpfs，本用例 = 宿主普通目录）。
        """
        out_dir = os.path.join(self.tmp, "out_nofull")
        os.makedirs(out_dir, exist_ok=True)
        cfg = self._cfg("cfg_nofull.json", self.lights4, out_dir)
        r = subprocess.run([EXE, "normalize", "--json", cfg, "-y"],
                           capture_output=True, text=True, timeout=600, cwd=run_cwd())
        self.assertEqual(r.returncode, 0,
                         "并发 4 帧、磁盘充足必须成功（防假红）: %s" % r.stderr[-800:])
        events = [json.loads(l) for l in r.stdout.splitlines() if l.strip()]
        self.assertTrue(events, "必须有事件流")
        fin = events[-1]
        self.assertEqual((fin["kind"], fin["exit_code"]), ("final", 0),
                         "成功运行 final 必须 exit_code=0")
        disk = [e for e in events
                if e.get("failure_kind") in ("disk_full", "write_failed")]
        self.assertFalse(disk, "无磁盘满时不得出现磁盘门判定事件: %s" % disk[:1])


if __name__ == "__main__":
    unittest.main(verbosity=2)
