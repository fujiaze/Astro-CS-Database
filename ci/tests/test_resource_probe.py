# -*- coding: utf-8 -*-
"""V8-CI-003 单元测试：tools/monitoring/resource_probe.py（owner=SA-CI-32）。

覆盖（tasks/02_CI_TASKS.md V8-CI-003 验收）：
  - probe() 全字段齐全与取值自洽（核数 >=1、max_workers >=1、quota 不可得时
    effective == affinity）；
  - 解析器函数级测试：parse_cpu_max / parse_cfs_quota / parse_meminfo /
    parse_cpuinfo_logical / parse_cpuinfo_physical；
  - 非硬编码证明：注入不同 affinity（含缩容）时 effective/max_workers 跟随变化；
  - 假 /proc + /sys 树：cgroup v2 相对路径 / 回退默认路径、cgroup v1、
    sysfs topology、cpuinfo fallback、字段缺失不报错。
纯 stdlib；全部探测以注入 proc_root/sys_root/affinity_fn/cpu_count_fn 完成。
"""
from __future__ import annotations

import sys
import tempfile
import unittest
from pathlib import Path

_REPO = Path(__file__).resolve().parents[2]
if str(_REPO) not in sys.path:
    sys.path.insert(0, str(_REPO))

from tools.monitoring import resource_probe as RP  # noqa: E402


def _write(path: Path, text: str) -> Path:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8")
    return path


class TestProbeHost(unittest.TestCase):
    """真宿主 probe()：字段齐全、类型正确、值自洽。"""

    def setUp(self) -> None:
        self.p = RP.probe()

    def test_fields_present(self) -> None:
        for key in ("cpu_affinity", "cpu_affinity_source",
                    "cpu_logical", "cpu_logical_source",
                    "cpu_physical", "cpu_physical_source",
                    "cgroup_quota_cores", "cgroup_quota_source",
                    "mem_total_bytes", "mem_available_bytes", "mem_source",
                    "effective_cpu_cores", "effective_source",
                    "max_workers", "max_workers_source"):
            self.assertIn(key, self.p, f"probe() 缺字段 {key}")

    def test_self_consistent_values(self) -> None:
        p = self.p
        self.assertGreaterEqual(p["cpu_affinity"], 1)
        self.assertGreaterEqual(p["cpu_logical"], 1)
        self.assertGreaterEqual(p["effective_cpu_cores"], 1.0)
        self.assertGreaterEqual(p["max_workers"], 1)
        self.assertGreater(p["mem_total_bytes"], 0)
        # affinity 与 effective 一致性：quota 无约束时 effective 必须等于 affinity
        if p["cgroup_quota_cores"] is None:
            self.assertEqual(p["effective_cpu_cores"], float(p["cpu_affinity"]))
            self.assertEqual(p["effective_source"], "affinity")
        else:
            self.assertLessEqual(p["effective_cpu_cores"],
                                 float(p["cpu_affinity"]))
            self.assertLessEqual(p["effective_cpu_cores"],
                                 p["cgroup_quota_cores"])
        # max_workers 恒由 effective 推导（floor × WORKERS_PER_CORE，下限 1）
        self.assertEqual(p["max_workers"],
                         max(1, int(p["effective_cpu_cores"])))

    def test_sources_annotated(self) -> None:
        p = self.p
        self.assertTrue(p["cpu_affinity_source"])
        self.assertTrue(p["cpu_logical_source"])
        self.assertTrue(p["mem_source"])
        # cpu_physical / quota 允许 "unavailable"（探测不出不报错），但不允许空串
        self.assertTrue(p["cpu_physical_source"])
        self.assertTrue(p["cgroup_quota_source"])


class TestParsers(unittest.TestCase):
    """解析器函数级测试（含文档要求的样本值）。"""

    def test_parse_cpu_max(self) -> None:
        self.assertEqual(RP.parse_cpu_max("200000 100000"), 2.0)
        self.assertEqual(RP.parse_cpu_max("150000 100000"), 1.5)
        self.assertEqual(RP.parse_cpu_max("200000"), 2.0)  # 缺周期 → 默认 100000
        self.assertIsNone(RP.parse_cpu_max("max 100000"))   # v2 无限制
        self.assertIsNone(RP.parse_cpu_max("MAX 100000"))
        self.assertIsNone(RP.parse_cpu_max(""))
        self.assertIsNone(RP.parse_cpu_max(None))
        self.assertIsNone(RP.parse_cpu_max("garbage"))
        self.assertIsNone(RP.parse_cpu_max("-1 100000"))    # 负 quota
        self.assertIsNone(RP.parse_cpu_max("200000 0"))     # 坏周期
        self.assertIsNone(RP.parse_cpu_max("200000 nope"))  # 坏周期 token

    def test_parse_cfs_quota(self) -> None:
        self.assertEqual(RP.parse_cfs_quota("200000", "100000"), 2.0)
        self.assertEqual(RP.parse_cfs_quota("50000", "100000"), 0.5)
        self.assertIsNone(RP.parse_cfs_quota("-1", "100000"))  # 无限制
        self.assertIsNone(RP.parse_cfs_quota("", "100000"))
        self.assertIsNone(RP.parse_cfs_quota(None, None))
        self.assertEqual(RP.parse_cfs_quota("200000", None), 2.0)  # 缺周期默认

    def test_parse_cpuinfo_logical(self) -> None:
        self.assertEqual(RP.parse_cpuinfo_logical("processor\t: 0\n"
                                                  "processor\t: 1\n"
                                                  "processor\t: 2\n"), 3)
        self.assertEqual(RP.parse_cpuinfo_logical(""), 0)
        self.assertEqual(RP.parse_cpuinfo_logical(None), 0)

    def test_parse_cpuinfo_physical(self) -> None:
        x86 = (
            "processor\t: 0\nphysical id\t: 0\ncore id\t: 0\n"
            "processor\t: 1\nphysical id\t: 0\ncore id\t: 1\n"
            "processor\t: 2\nphysical id\t: 1\ncore id\t: 0\n"
            "processor\t: 3\nphysical id\t: 1\ncore id\t: 1\n"
        )
        self.assertEqual(RP.parse_cpuinfo_physical(x86), 4)
        # 超线程共享 (physical id, core id)：4 线程 → 2 物理核
        ht = (
            "processor\t: 0\nphysical id\t: 0\ncore id\t: 0\n"
            "processor\t: 1\nphysical id\t: 0\ncore id\t: 1\n"
            "processor\t: 2\nphysical id\t: 0\ncore id\t: 0\n"
            "processor\t: 3\nphysical id\t: 0\ncore id\t: 1\n"
        )
        self.assertEqual(RP.parse_cpuinfo_physical(ht), 2)
        # 同一物理核的两个超线程 → 去重后 1
        single = (
            "processor\t: 0\nphysical id\t: 0\ncore id\t: 0\n"
            "processor\t: 1\nphysical id\t: 0\ncore id\t: 0\n"
        )
        self.assertEqual(RP.parse_cpuinfo_physical(single), 1)
        # 无 core id 字段（部分 ARM）→ None，不报错
        self.assertIsNone(RP.parse_cpuinfo_physical("processor\t: 0\n"))
        self.assertIsNone(RP.parse_cpuinfo_physical(None))
        self.assertIsNone(RP.parse_cpuinfo_physical(""))

    def test_parse_meminfo(self) -> None:
        sample = ("MemTotal:       16374520 kB\n"
                  "MemFree:         1234567 kB\n"
                  "MemAvailable:   13922720 kB\n"
                  "SwapTotal:             0 kB\n")
        out = RP.parse_meminfo(sample)
        self.assertEqual(out["mem_total_bytes"], 16374520 * 1024)
        self.assertEqual(out["mem_available_bytes"], 13922720 * 1024)
        no_avail = RP.parse_meminfo("MemTotal:   1000 kB\n")
        self.assertEqual(no_avail["mem_total_bytes"], 1000 * 1024)
        self.assertIsNone(no_avail["mem_available_bytes"])
        self.assertEqual(RP.parse_meminfo(None),
                         {"mem_total_bytes": None, "mem_available_bytes": None})
        self.assertEqual(RP.parse_meminfo("garbage line\n")["mem_total_bytes"],
                         None)


class TestComputeSemantics(unittest.TestCase):
    """effective / max_workers 语义与非硬编码证明。"""

    def test_compute_effective(self) -> None:
        # quota 不可得/无限制 → effective=affinity
        self.assertEqual(RP.compute_effective(16, None), (16.0, "affinity"))
        self.assertEqual(RP.compute_effective(16, -1.0), (16.0, "affinity"))
        self.assertEqual(RP.compute_effective(16, 0), (16.0, "affinity"))
        # quota 生效 → effective=min(affinity, quota)
        self.assertEqual(RP.compute_effective(16, 4.0), (4.0, "quota"))
        self.assertEqual(RP.compute_effective(2, 8.0), (2.0, "affinity"))
        # affinity 边界收缩到 1
        self.assertEqual(RP.compute_effective(0, None), (1.0, "affinity"))

    def test_derive_max_workers(self) -> None:
        self.assertEqual(RP.derive_max_workers(16.0), 16)
        self.assertEqual(RP.derive_max_workers(4.0), 4)
        self.assertEqual(RP.derive_max_workers(1.5), 1)  # floor，下限 1
        self.assertEqual(RP.derive_max_workers(0.2), 1)
        self.assertEqual(RP.derive_max_workers(-3.0), 1)

    def test_shrinking_affinity_changes_effective_and_workers(self) -> None:
        """注入缩容 affinity：effective/max_workers 必须跟随变化（非硬编码核数）。"""
        def probe_with(n: int) -> dict:
            return RP.probe(affinity_fn=lambda: set(range(n)),
                            cpu_count_fn=lambda: 64)

        wide = probe_with(8)
        narrow = probe_with(2)
        self.assertEqual(wide["cpu_affinity"], 8)
        self.assertEqual(wide["max_workers"], 8)
        self.assertEqual(narrow["cpu_affinity"], 2)
        self.assertEqual(narrow["max_workers"], 2)
        self.assertLess(narrow["max_workers"], wide["max_workers"])
        self.assertEqual(wide["effective_cpu_cores"], 8.0)
        self.assertEqual(narrow["effective_cpu_cores"], 2.0)

    def test_affinity_fallback_when_unavailable(self) -> None:
        """sched_getaffinity 不可得 → fallback os.cpu_count 注入值。"""
        def broken() -> object:
            raise OSError("no affinity here")
        p = RP.probe(affinity_fn=broken, cpu_count_fn=lambda: 7)
        self.assertEqual(p["cpu_affinity"], 7)
        self.assertEqual(p["cpu_affinity_source"], "fallback:cpu_count")
        self.assertEqual(p["max_workers"], 7)


class TestFakeProcTree(unittest.TestCase):
    """假 /proc + /sys 树：各来源路径与 fallback。"""

    def test_cgroup_v2_relative_path(self) -> None:
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            proc = root / "proc"
            _write(proc / "self" / "cgroup", "0::/system.slice/foo.service\n")
            _write(root / "sys" / "system.slice" / "foo.service" / "cpu.max",
                   "300000 100000\n")
            p = RP.probe(proc_root=proc, sys_root=root / "sys",
                         affinity_fn=lambda: set(range(16)),
                         cpu_count_fn=lambda: 16)
            self.assertEqual(p["cgroup_quota_cores"], 3.0)
            self.assertEqual(p["cgroup_quota_source"], "cgroup2")
            self.assertEqual(p["effective_cpu_cores"], 3.0)
            self.assertEqual(p["effective_source"], "quota")
            self.assertEqual(p["max_workers"], 3)

    def test_cgroup_v2_unlimited_falls_back_to_root(self) -> None:
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            proc = root / "proc"
            _write(proc / "self" / "cgroup", "0::/slice/foo.service\n")
            _write(root / "sys" / "slice" / "foo.service" / "cpu.max",
                   "max 100000\n")   # 本层无限制 → 回退 root
            _write(root / "sys" / "cpu.max", "120000 100000\n")
            p = RP.probe(proc_root=proc, sys_root=root / "sys",
                         affinity_fn=lambda: set(range(16)),
                         cpu_count_fn=lambda: 16)
            self.assertEqual(p["cgroup_quota_cores"], 1.2)
            self.assertEqual(p["cgroup_quota_source"], "cgroup2")

    def test_cgroup_v1(self) -> None:
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            proc = root / "proc"
            _write(proc / "self" / "cgroup",
                   "12:cpu,cpuacct:/machine.slice/app\n")
            v1 = root / "sys" / "cpu" / "machine.slice" / "app"
            _write(v1 / "cpu.cfs_quota_us", "250000\n")
            _write(v1 / "cpu.cfs_period_us", "100000\n")
            p = RP.probe(proc_root=proc, sys_root=root / "sys",
                         affinity_fn=lambda: set(range(16)),
                         cpu_count_fn=lambda: 16)
            self.assertEqual(p["cgroup_quota_cores"], 2.5)
            self.assertEqual(p["cgroup_quota_source"], "cgroup1")

    def test_no_cgroup_quota_is_none(self) -> None:
        """无 v2/v1 文件（本宿主实况）→ quota=None 不报错，effective=affinity。"""
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            p = RP.probe(proc_root=root / "proc", sys_root=root / "sys",
                         affinity_fn=lambda: set(range(5)),
                         cpu_count_fn=lambda: 5)
            self.assertIsNone(p["cgroup_quota_cores"])
            self.assertEqual(p["cgroup_quota_source"], "unavailable")
            self.assertEqual(p["effective_cpu_cores"], 5.0)
            self.assertEqual(p["effective_source"], "affinity")

    def test_quota_below_affinity_caps_workers(self) -> None:
        """quota < affinity：effective 取 quota（交集语义），worker 被封顶。"""
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            _write(root / "sys" / "cpu.max", "150000 100000\n")  # 1.5 核
            p = RP.probe(proc_root=root / "proc", sys_root=root / "sys",
                         affinity_fn=lambda: set(range(16)),
                         cpu_count_fn=lambda: 16)
            self.assertEqual(p["effective_cpu_cores"], 1.5)
            self.assertEqual(p["max_workers"], 1)
            self.assertEqual(p["effective_source"], "quota")

    def test_logical_falls_back_to_cpuinfo(self) -> None:
        """os.cpu_count 不可得 → /proc/cpuinfo processor 计数。"""
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            _write(root / "proc" / "cpuinfo",
                   "processor\t: 0\nmodel name\t: x\nprocessor\t: 1\n"
                   "processor\t: 2\n")
            p = RP.probe(proc_root=root / "proc", sys_root=root / "sys",
                         affinity_fn=lambda: set(range(3)),
                         cpu_count_fn=lambda: None)
            self.assertEqual(p["cpu_logical"], 3)
            self.assertEqual(p["cpu_logical_source"], "procfs")

    def test_logical_falls_back_to_affinity(self) -> None:
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            p = RP.probe(proc_root=root / "proc", sys_root=root / "sys",
                         affinity_fn=lambda: set(range(6)),
                         cpu_count_fn=lambda: None)
            self.assertEqual(p["cpu_logical"], 6)
            self.assertEqual(p["cpu_logical_source"], "fallback:affinity")

    def test_physical_from_cpuinfo_and_sysfs(self) -> None:
        # 路径 A：/proc/cpuinfo (physical id, core id) 去重
        with tempfile.TemporaryDirectory() as td_a:
            root = Path(td_a)
            _write(root / "proc" / "cpuinfo",
                   "processor\t: 0\nphysical id\t: 0\ncore id\t: 0\n"
                   "processor\t: 1\nphysical id\t: 0\ncore id\t: 1\n")
            p = RP.probe(proc_root=root / "proc", sys_root=root / "sys",
                         affinity_fn=lambda: set(range(2)),
                         cpu_count_fn=lambda: 2)
            self.assertEqual(p["cpu_physical"], 2)
            self.assertEqual(p["cpu_physical_source"], "procfs")

        # 路径 B：无 cpuinfo → sysfs topology fallback
        with tempfile.TemporaryDirectory() as td_b:
            root = Path(td_b)
            topo = root / "sys" / "devices" / "system" / "cpu"
            _write(topo / "cpu0" / "topology" / "physical_package_id", "0\n")
            _write(topo / "cpu0" / "topology" / "core_id", "0\n")
            _write(topo / "cpu1" / "topology" / "physical_package_id", "0\n")
            _write(topo / "cpu1" / "topology" / "core_id", "1\n")
            p = RP.probe(proc_root=root / "proc", sys_root=root / "sys",
                         affinity_fn=lambda: set(range(2)),
                         cpu_count_fn=lambda: 2)
            self.assertEqual(p["cpu_physical"], 2)
            self.assertEqual(p["cpu_physical_source"], "sysfs")

        # 路径 C：两者都没有 → None，不报错
        with tempfile.TemporaryDirectory() as td_c:
            root = Path(td_c)
            p = RP.probe(proc_root=root / "proc", sys_root=root / "sys",
                         affinity_fn=lambda: set(range(2)),
                         cpu_count_fn=lambda: 2)
            self.assertIsNone(p["cpu_physical"])
            self.assertEqual(p["cpu_physical_source"], "unavailable")

    def test_meminfo_bytes(self) -> None:
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            _write(root / "proc" / "meminfo",
                   "MemTotal:   2000000 kB\nMemAvailable: 1500000 kB\n")
            p = RP.probe(proc_root=root / "proc", sys_root=root / "sys",
                         affinity_fn=lambda: set(range(4)),
                         cpu_count_fn=lambda: 4)
            self.assertEqual(p["mem_total_bytes"], 2000000 * 1024)
            self.assertEqual(p["mem_available_bytes"], 1500000 * 1024)
            self.assertEqual(p["mem_source"], "procfs")

        # meminfo 缺失 → None 不报错（独立 tempdir，避免残留文件串扰）
        with tempfile.TemporaryDirectory() as td2:
            root = Path(td2)
            p = RP.probe(proc_root=root / "proc", sys_root=root / "sys",
                         affinity_fn=lambda: set(range(4)),
                         cpu_count_fn=lambda: 4)
            self.assertIsNone(p["mem_total_bytes"])
            self.assertIsNone(p["mem_available_bytes"])
            self.assertEqual(p["mem_source"], "unavailable")


if __name__ == "__main__":
    unittest.main()
