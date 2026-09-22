#!/usr/bin/env python3
"""MON-002 测试: 所有 Phase/kernel 发 stage/resource/backend 事件; summary/raw 分层。
验收(07 §1): resource summary 强制存在(07 §2 必采指标); 资源时序曲线唯一载体 = 磁盘工件
resource_timeseries.csv（summary 事件只内嵌 raw_dir/raw_n 指针）; 无标注 >5s 区间→P1。

CLI-002 重锚(ROOT-008 + CLI-001 命令树 + GATE-FIX-RES R-4 D-14): 运行面由已删的
'phase3 run --config --resource-detail summary|timeseries' 改为 export 会话命令；
「summary|timeseries 分层档」旗标已退役（不在白名单 → rc=2），曲线载体为该 CSV 工件
（lib/infrastructure/cli/resource_events.h:1-9）。判据语义不变, 只换载体。
"""
import json, os, shutil, subprocess, tempfile, unittest

# CTESTFULL-01：fixture 制备统一走 cli_fixture（进程级缓存 + fail-closed 带原因）
import cli_fixture

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
# ROOT-008: CLI 命令层源在 lib/infrastructure/cli/（旧 cli/ 已退役）
CLI = os.path.join(REPO, "lib", "infrastructure", "cli")
# DISPATCH 附录 H（构建隔离）: 被测构建树 = 被测二进制所在目录; ASTROCS_CLI_BIN 覆盖。
EXE = os.environ.get("ASTROCS_CLI_BIN", os.path.join(REPO, "build", "astrocs"))
BUILD = os.path.dirname(os.path.abspath(EXE))
# CTESTFULL-01：fixture 制备已收归 cli_fixture（自带 AIO/SHARED/cfitsio 定位），
# 本文件原有的 _pick/AIO/SHARED 兼容垫片随之退役。

# FIX-UTCLI-HYGIENE: 子进程 cwd 统一落 run/（gitignore），见 cli_test_hygiene.py
from cli_test_hygiene import run_cwd  # noqa: E402


class TestMonitorEvents(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.exe_ok = os.path.isfile(EXE)
        cls.tmp = tempfile.mkdtemp(prefix="mon002_")
        # 建合成 FIELD.hips fixture（export 会话的输入产品）。
        # CTESTFULL-01：旧写法把 g++ / --make-field 的失败**静默折叠**成
        # hips=None，现场只看到「无合成 fixture（setUpClass 未产出 FIELD.hips）」，
        # 真实 rc/stderr 被丢弃 → 不可归因（同批 test_phase2/3 因 assert 带 stderr
        # 而报出了真因：undefined reference to astrocs::crypto::Sha256）。
        # 现在交给 cli_fixture：进程级缓存（整个 suite 只编译一次，不再每个测试
        # 文件重编一份 cfitsio+AIO）+ fail-closed（失败抛 CliFixtureError，
        # 消息含 rc 与 stderr 尾部）。
        cls.hips = cli_fixture.make_field_hips(os.path.join(cls.tmp, "data"),
                                               cwd=run_cwd())
        # export 运行配置（§6.2 命令面 / 模板形态）
        cls.out = os.path.join(cls.tmp, "out")
        os.makedirs(cls.out)
        cls.rcfg = os.path.join(cls.tmp, "r.json")
        with open(cls.rcfg, "w", encoding="utf-8") as fh:
            json.dump({
                "schema_version": "1",
                "source": {"hips_dir": cls.hips or "/nonexistent"},
                "output_dir": cls.out,
                "center": {"ra_deg": 210.0, "dec_deg": 34.0},
                "scale_deg_per_px": 0.1, "width_px": 40, "height_px": 30,
                "projection": "TAN", "sampler": "nearest",
                "coverage_output": "mask",
                # FZ-P3-MODES：phase3 resample 节点要求显式声明 output_mode（缺键即 REJECT）
                "output_mode": "surface_brightness",
            }, fh)

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.tmp, ignore_errors=True)

    def _run(self, *args):
        return subprocess.run([EXE, *args], capture_output=True, text=True, timeout=300,
                              cwd=run_cwd())

    def _events(self, out):
        evs = []
        for line in out.splitlines():
            if line.strip():
                evs.append(json.loads(line))
        return evs

    def _require_exe(self):
        if not self.exe_ok:
            self.skipTest("CLI 二进制缺失(先构建 build/astrocs)")

    def _require_fixture(self):
        self._require_exe()
        self.assertTrue(self.hips, "无合成 fixture（setUpClass 未产出 FIELD.hips）")

    def test_01_resource_detail_flag_retired_and_summary_run_ok(self):
        """曲线档旗标已退役（rc=2 非静默）; 现行运行面 export --events-jsonl → 0。"""
        self._require_fixture()
        r = self._run("export", "--json", self.rcfg, "--events-jsonl", "-y")
        self.assertEqual(r.returncode, 0, r.stderr[-300:])
        # GATE-FIX-RES(R-4 D-14): --resource-detail 不在白名单 → unknown flag rc=2
        for val in ("summary", "timeseries", "bogus"):
            r2 = self._run("export", "--json", self.rcfg, "--resource-detail", val)
            self.assertEqual(r2.returncode, 2, f"--resource-detail {val} 应被拒绝(已退役)")
            self.assertNotIn('"kind"', r2.stdout, "拒绝路径不得吐事件流")

    def test_02_resource_summary_emitted_with_mandatory_metrics(self):
        """run 成功 → resource summary 事件必含 07 §2 指标 + 曲线工件指针。"""
        self._require_fixture()
        r = self._run("export", "--json", self.rcfg, "--events-jsonl", "-y")
        self.assertEqual(r.returncode, 0, r.stderr[-300:])
        res = [e for e in self._events(r.stdout)
               if e["kind"] == "resource" and e["message"] == "resource summary"]
        self.assertTrue(res, "必须发出 resource summary 事件")
        e = res[-1]
        for k in ("n_samples", "peak_rss_bytes", "wall_seconds",
                  "peak_equivalent_cores", "max_threads", "raw_dir", "raw_n"):
            self.assertIn(k, e, f"resource summary 缺必采指标 {k}")
        # D-14: 曲线不再内嵌, 只给磁盘工件指针
        self.assertEqual(e.get("resource_curve_artifact"), "resource_timeseries.csv")
        self.assertNotIn("resource_detail", e, "分层档字段已退役")
        csv = os.path.join(e["raw_dir"], e["resource_curve_artifact"])
        self.assertTrue(os.path.isfile(csv), f"曲线工件缺失: {csv}")
        with open(csv, encoding="utf-8") as fh:
            head = fh.readline().strip()
        self.assertIn("elapsed_seconds", head)
        self.assertIn("active_workers", head)

    def test_03_backend_event_emitted(self):
        """backend 事件含 backend_id/workers_used/available_cpus(07 §2 必采)。"""
        self._require_fixture()
        r = self._run("export", "--json", self.rcfg, "--events-jsonl", "-y")
        be = [e for e in self._events(r.stdout) if e["kind"] == "backend"]
        self.assertTrue(be, "必须发出 backend 事件")
        e = be[-1]
        self.assertIn("backend_id", e)
        self.assertIn("workers_used", e)
        self.assertIn("available_cpus", e)

    def test_04_tier_downsample_present_only_when_timeseries(self):
        """分层小型化: summary 事件不内嵌曲线数据, 降采样曲线仍由 C++ 侧单点构造。"""
        self._require_fixture()
        r = self._run("export", "--json", self.rcfg, "--events-jsonl", "-y")
        res = [e for e in self._events(r.stdout)
               if e["kind"] == "resource" and e["message"] == "resource summary"]
        self.assertTrue(res)
        e = res[-1]
        # 曲线数据不内嵌: 无 curve_points; 只有工件指针 + raw 计数
        self.assertNotIn("curve_points", e, "summary 不得内嵌曲线（D-14 静默空数组信号）")
        self.assertIn("raw_dir", e)
        self.assertIn("raw_n", e)
        self.assertGreaterEqual(int(e["raw_n"]), 1)
        # 退役面: 分层档旗标不得回到命令树白名单（静态锚）
        with open(os.path.join(CLI, "command_tree.h"), encoding="utf-8") as fh:
            tree = fh.read()
        self.assertNotIn("--resource-detail", tree, "曲线档旗标必须保持退役")

    def test_05_stage_annotation_enum_stable(self):
        """stage 资源类别枚举(compute/memory/io/mixed/unknown)在 C++ 侧固定, 供 MON-003 引用。"""
        drv = os.path.join(self.tmp, "enum.cpp")
        with open(drv, "w") as f:
            f.write(r'''
#include "resource_events.h"
#include <cstdio>
int main(){
    using namespace astrocs;
    std::printf("%s %s %s %s %s\n",
        stage_kind_name(StageKind::Compute), stage_kind_name(StageKind::Memory),
        stage_kind_name(StageKind::Io), stage_kind_name(StageKind::Mixed),
        stage_kind_name(StageKind::Unknown));
    return 0;
}
''')
        r = subprocess.run(["g++", "-std=c++17", "-O2", f"-I{CLI}",
                            f"-I{os.path.join(REPO, 'lib', 'third_party')}",
                            drv, "-o", os.path.join(self.tmp, "enum")],
                           capture_output=True, text=True, timeout=120)
        self.assertEqual(r.returncode, 0, r.stderr[-400:])
        o = subprocess.run([os.path.join(self.tmp, "enum")], capture_output=True,
                           text=True, timeout=30, cwd=run_cwd()).stdout.strip()
        self.assertEqual(o, "compute memory io mixed unknown")

    def test_06_stage_kind_classify_cpp(self):
        """resource_events.h classify_stage/is_unannotated_priority 编译并正确分类。"""
        drv = os.path.join(self.tmp, "classify.cpp")
        with open(drv, "w") as f:
            f.write(r'''
#include "resource_events.h"
#include <cstdio>
int main(){
    using namespace astrocs;
    std::printf("compute=%s\n", stage_kind_name(classify_stage("compute")));
    std::printf("unknown_unannoted_5s=%d\n", (int)is_unannotated_priority(nullptr, 6.0));
    std::printf("compute_annoted_5s=%d\n", (int)is_unannotated_priority("compute", 6.0));
    std::printf("unknown_short_5s=%d\n", (int)is_unannotated_priority(nullptr, 3.0));
    return 0;
}
''')
        r = subprocess.run(["g++", "-std=c++17", "-O2", f"-I{CLI}",
                            f"-I{os.path.join(REPO, 'lib', 'third_party')}",
                            drv, "-o", os.path.join(self.tmp, "classify")],
                           capture_output=True, text=True, timeout=120)
        self.assertEqual(r.returncode, 0, r.stderr[-400:])
        o = subprocess.run([os.path.join(self.tmp, "classify")], capture_output=True,
                           text=True, timeout=30, cwd=run_cwd()).stdout
        self.assertIn("compute=compute", o)
        self.assertIn("unknown_unannoted_5s=1", o, "无标注>5s 必须 P1")
        self.assertIn("compute_annoted_5s=0", o, "compute 标注不触发 P1")
        self.assertIn("unknown_short_5s=0", o, "短于 5s 无标注不 P1")
        # GATE-FIX-RES(R-4 D-14/D-15): 内嵌曲线降采样器已删除（曲线唯一载体 =
        # resource_timeseries.csv），退役面静态锚: 不得回流 CLI 事件头。
        for h in ("resource_events.h", "monitor.h"):
            p = os.path.join(CLI, h)
            if os.path.isfile(p):
                with open(p, encoding="utf-8") as fh:
                    self.assertNotIn("downsample_curve", fh.read(),
                                     f"{h} 出现已退役的曲线降采样器")


if __name__ == "__main__":
    unittest.main(verbosity=2)
