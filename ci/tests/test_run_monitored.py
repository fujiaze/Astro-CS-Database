# -*- coding: utf-8 -*-
"""V8-CI-003 单元测试：tools/monitoring/run_monitored.py（owner=SA-CI-32）。

覆盖（tasks/02_CI_TASKS.md V8-CI-003 验收）：
  ① busy-loop 子进程 → 树级平均 CPU% 高（>50%）；
  ② sleep 子进程 → CPU% 近零（<20%）且 evaluate(min_cpu_percent=...) 标记违规；
  ③ memory-growth 子进程 → peak RSS 显著高于基线（>32MB）且
     evaluate(max_rss_growth_kb=...) 标记泄漏违规；
  ④ 多线程子进程 → threads_max >= 3；
  ⑤ PROGRESS: d/t（stdout）与 progress-file 两种进度途径 → progress 字段解析；
  ⑥ timeout：sleep 30 + timeout 2s → timed_out=true、子进程被杀、总时长受控；
  ⑦ --output JSON 文件可解析且字段完整。
所有被监控命令均以 argv 数组传入（永不 shell=True）；每个 run_monitored 调用
都带 timeout；全部时长秒级，两模块合计 <<120s。
"""
from __future__ import annotations

import json
import sys
import tempfile
import time
import unittest
from pathlib import Path

_REPO = Path(__file__).resolve().parents[2]
if str(_REPO) not in sys.path:
    sys.path.insert(0, str(_REPO))

from tools.monitoring import run_monitored as RM  # noqa: E402

BUSY_SECONDS = 2.5   # busy-loop 运行时长
IDLE_SECONDS = 2.0   # sleep 运行时长
MEM_SECONDS = 2.0    # memory-growth 运行时长
PROC = "python3"


def _py(code: str) -> list[str]:
    return [PROC, "-c", code]


class TestBusyLoop(unittest.TestCase):
    """① busy-loop：进程树平均 CPU% 高于 50%（单核满载应接近 100%）。"""

    def test_busy_loop_high_cpu(self) -> None:
        result = RM.run_monitored(
            _py(f"import time\nt=time.monotonic()\nwhile time.monotonic()-t < {BUSY_SECONDS}:\n    pass\n"),
            timeout=30, poll_interval=0.1)
        self.assertEqual(result["exit_code"], 0)
        self.assertFalse(result["timed_out"])
        self.assertIsNotNone(result["cpu_percent_avg"])
        self.assertGreater(result["cpu_percent_avg"], 50.0,
                           f"busy-loop 平均 CPU% 过低: {result['cpu_percent_avg']}")
        self.assertGreater(result["samples"], 3)
        # 阈值判定：min_cpu_percent=50 不应产生低利用违规
        self.assertEqual([v for v in RM.evaluate(result, min_cpu_percent=50)
                          if v.startswith("low_cpu")], [])


class TestSleepIdle(unittest.TestCase):
    """② sleep：CPU% 近零；evaluate(min_cpu_percent) 能抓低利用率。"""

    def test_sleep_low_cpu_flagged(self) -> None:
        result = RM.run_monitored(
            _py(f"import time\ntime.sleep({IDLE_SECONDS})\n"),
            timeout=30, poll_interval=0.1)
        self.assertEqual(result["exit_code"], 0)
        self.assertLess(result["cpu_percent_avg"], 20.0,
                        f"sleep 平均 CPU% 过高: {result['cpu_percent_avg']}")
        violations = RM.evaluate(result, min_cpu_percent=50)
        self.assertTrue(any(v.startswith("low_cpu_utilization")
                            for v in violations),
                        f"未标记低利用率: {violations}")


class TestMemoryGrowth(unittest.TestCase):
    """③ memory-growth：峰值 RSS 高出基线 >32MB；evaluate 能抓泄漏。"""

    def test_memory_growth_leak_flagged(self) -> None:
        code = (
            "import time\n"
            "chunks = []\n"
            f"for _ in range(16):\n"
            "    chunks.append(bytearray(4 * 1024 * 1024))\n"
            f"    time.sleep({MEM_SECONDS / 16:.3f})\n"
            "print('len', len(chunks))\n"
        )
        result = RM.run_monitored(_py(code), timeout=30, poll_interval=0.1)
        self.assertEqual(result["exit_code"], 0)
        self.assertIsNotNone(result["peak_rss_kb"])
        self.assertIsNotNone(result["rss_start_kb"])
        growth = result["peak_rss_kb"] - result["rss_start_kb"]
        # 64MB 分配至少应反映 ~32MB RSS 增长（留余量抗页回收抖动）
        self.assertGreater(growth, 32 * 1024,
                           f"RSS 增长 {growth} KB 未达 32MB 预期")
        violations = RM.evaluate(result, max_rss_growth_kb=32 * 1024)
        self.assertTrue(any(v.startswith("memory_leak_suspected")
                            for v in violations),
                        f"未标记内存泄漏: {violations}")
        # 收紧到 1MB 上限同样违规；峰值本身远高于 32MB
        self.assertGreater(result["peak_rss_kb"], 32 * 1024)


class TestThreads(unittest.TestCase):
    """④ 多线程子进程：threads_max >= 3。"""

    def test_threads_max_counted(self) -> None:
        code = ("import threading, time\n"
                "def work():\n"
                "    time.sleep(2.0)\n"
                "threads = [threading.Thread(target=work) for _ in range(3)]\n"
                "for t in threads:\n"
                "    t.start()\n"
                "time.sleep(0.8)\n")
        result = RM.run_monitored(_py(code), timeout=30, poll_interval=0.1)
        self.assertEqual(result["exit_code"], 0)
        self.assertGreaterEqual(result["threads_max"], 3,
                                f"threads_max={result['threads_max']} 未观测到多线程")


class TestProgress(unittest.TestCase):
    """⑤ 进度解析：stdout PROGRESS 行 + progress-file 途径。"""

    def test_progress_from_stdout(self) -> None:
        code = ("print('PROGRESS: 3/10', flush=True)\n"
                "import time\n"
                "time.sleep(0.6)\n")
        result = RM.run_monitored(_py(code), timeout=30)
        self.assertEqual(result["progress"]["done"], 3)
        self.assertEqual(result["progress"]["total"], 10)
        self.assertEqual(result["progress"]["source"], "stdout")

    def test_progress_from_progress_file(self) -> None:
        with tempfile.TemporaryDirectory() as td:
            pfile = Path(td) / "progress.txt"
            code = ("import os\n"
                    "path = os.environ['ASTROCS_PROGRESS_FILE']\n"
                    "open(path, 'a').write('7/12\\n')\n"
                    "import time\n"
                    "time.sleep(0.4)\n")
            result = RM.run_monitored(_py(code), timeout=30,
                                      progress_file=pfile)
            self.assertEqual(result["progress"]["done"], 7)
            self.assertEqual(result["progress"]["total"], 12)
            self.assertEqual(result["progress"]["source"], "progress_file")

    def test_progress_file_overrides_stdout(self) -> None:
        """两途径并存 → 以 progress-file 为准（文档化约定）。"""
        with tempfile.TemporaryDirectory() as td:
            pfile = Path(td) / "progress.txt"
            code = ("import os, time\n"
                    "print('PROGRESS: 1/10', flush=True)\n"
                    "open(os.environ['ASTROCS_PROGRESS_FILE'], 'a').write('9/10\\n')\n"
                    "time.sleep(0.4)\n")
            result = RM.run_monitored(_py(code), timeout=30, progress_file=pfile)
            self.assertEqual(result["progress"]["done"], 9)
            self.assertEqual(result["progress"]["total"], 10)
            self.assertEqual(result["progress"]["source"], "progress_file")

    def test_progress_none_when_absent(self) -> None:
        result = RM.run_monitored(
            _py("import time; time.sleep(0.3)"), timeout=30)
        self.assertIsNone(result["progress"])

    def test_evaluate_min_progress(self) -> None:
        result = {"progress": {"done": 2, "total": 10}}
        self.assertTrue(any(v.startswith("progress_below_threshold")
                            for v in RM.evaluate(result, min_progress=5)))
        self.assertEqual(RM.evaluate({"progress": None}, min_progress=5),
                         ["no_progress: 未观测到进度（要求 done >= 5）"])
        self.assertEqual(RM.evaluate({"progress": {"done": 5, "total": 5}},
                                     min_progress=5), [])

    def test_parse_progress_line(self) -> None:
        self.assertEqual(RM.parse_progress_line("PROGRESS: 1/4"),
                         {"done": 1, "total": 4})
        self.assertEqual(RM.parse_progress_line("  PROGRESS:  10 / 20  "),
                         {"done": 10, "total": 20})
        self.assertEqual(RM.parse_progress_line("3/5"), {"done": 3, "total": 5})
        self.assertIsNone(RM.parse_progress_line("PROGRESS: 1/"))
        self.assertIsNone(RM.parse_progress_line("nothing"))
        self.assertIsNone(RM.parse_progress_line(""))


class TestTimeoutKill(unittest.TestCase):
    """⑥ timeout：超时 SIGKILL 进程组、timed_out=true、时长受控。"""

    def test_timeout_kills_child(self) -> None:
        t0 = time.monotonic()
        result = RM.run_monitored(_py("import time; time.sleep(30)"),
                                  timeout=2.0, poll_interval=0.1)
        elapsed = time.monotonic() - t0
        self.assertTrue(result["timed_out"])
        self.assertTrue(result["killed"])
        self.assertIsNotNone(result["exit_code"])
        self.assertLess(elapsed, 5.0,
                        f"timeout 2s 的用例跑了 {elapsed:.1f}s")
        self.assertLess(result["duration_seconds"], 5.0)
        self.assertGreaterEqual(result["duration_seconds"], 2.0)


class TestOutputJson(unittest.TestCase):
    """⑦ output JSON 文件可解析且字段完整。"""

    def test_output_file_complete_fields(self) -> None:
        with tempfile.TemporaryDirectory() as td:
            out = Path(td) / "evidence.json"
            result = RM.run_monitored(
                _py("import time\nprint('hi', flush=True)\ntime.sleep(0.5)\n"),
                timeout=30, poll_interval=0.1, output=out)
            loaded = json.loads(out.read_text(encoding="utf-8"))
            for key in ("exit_code", "timed_out", "duration_seconds",
                        "cpu_samples", "peak_rss_kb", "peak_pss_kb",
                        "io_read_bytes", "io_write_bytes", "threads_max",
                        "progress", "samples", "rss_start_kb",
                        "cpu_percent_avg", "cpu_percent_median",
                        "started_utc", "finished_utc", "host_probe"):
                self.assertIn(key, loaded, f"证据 JSON 缺字段 {key}")
            self.assertEqual(loaded["command"], result["command"])
            self.assertIsInstance(loaded["cpu_samples"], list)
            self.assertGreater(loaded["samples"], 0)
            # host_probe 内嵌探测（重算并发时的环境证据）
            self.assertIn("effective_cpu_cores", loaded["host_probe"])
            self.assertIn("max_workers", loaded["host_probe"])


class TestEvaluateMisc(unittest.TestCase):
    """evaluate 其余分支：RSS 不可得 fail-closed、无阈值不产生违规。"""

    def test_no_thresholds_no_violations(self) -> None:
        self.assertEqual(RM.evaluate({}), [])

    def test_rss_unavailable_fails_closed(self) -> None:
        violations = RM.evaluate({"peak_rss_kb": None, "rss_start_kb": None},
                                 max_rss_growth_kb=1024)
        self.assertTrue(any(v.startswith("rss_unavailable") for v in violations),
                        f"RSS 不可得未 fail-closed: {violations}")

    def test_low_cpu_unavailable_fails_closed(self) -> None:
        violations = RM.evaluate({"cpu_percent_avg": None}, min_cpu_percent=10)
        self.assertTrue(any(v.startswith("low_cpu_unavailable")
                            for v in violations))


if __name__ == "__main__":
    unittest.main()
