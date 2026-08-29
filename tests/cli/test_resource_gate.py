#!/usr/bin/env python3
"""MON-003 测试: 07 分类和公式 + first-10s 快速失败 + exit 10 + 诊断分类。
验收: 人工 sleep/lock/io/memory/compute fixtures 各判对; 低 CPU compute 必失败(exit 10)。"""
import json, os, re, shutil, subprocess, tempfile, unittest

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
CLI = os.path.join(REPO, "cli")


@unittest.skipUnless(shutil.which("g++"), "需要 g++")
class TestResourceGate(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.mkdtemp(prefix="mon003_")
        src = os.path.join(cls.tmp, "gate.cpp")
        with open(src, "w") as f:
            f.write(r'''
#include "resource_gate.h"
#include <cstdio>
#include <cstdlib>
using namespace astrocs;
static void show(const char* name, GateDiag d){
    std::printf("%s=%s\n", name, gate_diag_name(d));
}
int main(int argc, char** argv){
    // 枚举 K, 构造场景 A/B(每场景两态), 由 --case <id> 选择
    const std::string c = (argc>1)?argv[1]:"";
    GateConfig base; base.available_cpus=4; base.selected_workers=4;
    base.max_active_threads=4; base.avg_equivalent_cores=3.2;
    base.wall_seconds=6.0; base.has_stage_annotation=true; base.cpu_percent=80.0;
    base.iowait_percent=1.0; base.mem_bandwidth_percent=90.0;
    if (c=="compute-ok"){ base.kind=ResKind::Compute; show("r", evaluate_gate(base)); }
    else if (c=="compute-single"){ base.kind=ResKind::Compute; base.selected_workers=1; base.max_active_threads=1; show("r", evaluate_gate(base)); }
    else if (c=="compute-lowcores"){ base.kind=ResKind::Compute; base.avg_equivalent_cores=1.5; show("r", evaluate_gate(base)); }
    else if (c=="compute-alllow"){ base.kind=ResKind::Compute; base.cpu_percent=5.0; base.iowait_percent=1.0; base.mem_bandwidth_percent=5.0; show("r", evaluate_gate(base)); }
    else if (c=="compute-short"){ base.kind=ResKind::Compute; base.wall_seconds=3.0; show("r", evaluate_gate(base)); }
    else if (c=="compute-globallock"){ base.kind=ResKind::Compute; base.one_worker_ns=100.0; base.n_worker_ns=150.0; show("r", evaluate_gate(base)); }
    else if (c=="memory-ok"){ base.kind=ResKind::Memory; base.achieved_memory_bandwidth_frac=0.85; base.required_memory_bandwidth_frac=0.70; show("r", evaluate_gate(base)); }
    else if (c=="memory-low"){ base.kind=ResKind::Memory; base.achieved_memory_bandwidth_frac=0.50; base.required_memory_bandwidth_frac=0.70; show("r", evaluate_gate(base)); }
    else if (c=="memory-unmeasured"){ base.kind=ResKind::Memory; base.achieved_memory_bandwidth_frac=-1.0; base.required_memory_bandwidth_frac=0.70; show("r", evaluate_gate(base)); }
    else if (c=="io-evidence"){ base.kind=ResKind::Io; base.io_bytes=1048576; base.io_ops=100; base.io_await_ms=2.0; show("r", evaluate_gate(base)); }
    else if (c=="io-noevidence"){ base.kind=ResKind::Io; show("r", evaluate_gate(base)); }
    else if (c=="io-shortserial"){ base.kind=ResKind::Io; base.io_is_short_serial=true; show("r", evaluate_gate(base)); }
    else if (c=="mixed-split"){ base.kind=ResKind::Mixed; base.mixed_has_compute_subrange=true; base.mixed_has_io_subrange=true; show("r", evaluate_gate(base)); }
    else if (c=="mixed-unsplit"){ base.kind=ResKind::Mixed; show("r", evaluate_gate(base)); }
    else if (c=="unannotated"){ base.kind=ResKind::Unknown; base.has_stage_annotation=false; base.wall_seconds=6.0; show("r", evaluate_gate(base)); }
    else if (c=="fastfail"){ base.first10s_low_cpu=true; base.first10s_non_io=true; base.first10s_mem_not_saturated=true; show("r", fast_fail_first10s(base)?GateDiag::FastFailFirst10s:GateDiag::Ok); }
    else if (c=="fastfail-no"){ base.first10s_low_cpu=false; base.first10s_non_io=true; base.first10s_mem_not_saturated=true; show("r", fast_fail_first10s(base)?GateDiag::FastFailFirst10s:GateDiag::Ok); }
    else if (c=="diag"){ GateConfig g; g.kind=ResKind::Compute; g.avg_equivalent_cores=1.5; g.selected_workers=4; g.max_active_threads=4; g.available_cpus=4; g.wall_seconds=6.0; g.has_stage_annotation=true; g.cpu_percent=80; g.iowait_percent=1; g.mem_bandwidth_percent=90; show("r", evaluate_gate(g)); std::printf("msg=%s\n", diag_message(evaluate_gate(g), g).c_str()); }
    else { std::printf("unknown-case\n"); return 2; }
    return 0;
}
''')
        cls.exe = os.path.join(cls.tmp, "gate")
        r = subprocess.run(["g++", "-std=c++17", "-O2", f"-I{CLI}",
                            f"-I{os.path.join(REPO, 'third_party')}",
                            src, "-o", cls.exe], capture_output=True, text=True, timeout=180)
        assert r.returncode == 0, r.stderr

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.tmp, ignore_errors=True)

    def _gate(self, case):
        r = subprocess.run([self.exe, case], capture_output=True, text=True, timeout=30)
        self.assertEqual(r.returncode, 0, r.stderr)
        return dict(line.split("=", 1) for line in r.stdout.strip().splitlines() if "=" in line)

    def test_01_compute_ok_and_failures(self):
        """compute ok 通过; 单线程/低核/CPU-io-mem皆低 → 各判对。"""
        self.assertEqual(self._gate("compute-ok")["r"], "ok")
        self.assertEqual(self._gate("compute-single")["r"], "single_threaded")
        self.assertEqual(self._gate("compute-lowcores")["r"], "low_avg_cores")
        self.assertEqual(self._gate("compute-alllow")["r"], "compute_io_mem_all_low")

    def test_02_compute_short_wall_and_globallock(self):
        """短于 5s 豁免; N-worker 无正向加速(全局锁) → fail。"""
        self.assertEqual(self._gate("compute-short")["r"], "ok")
        self.assertEqual(self._gate("compute-globallock")["r"], "global_lock_degradation")

    def test_03_memory_gate(self):
        """memory 达 pre-frozen 比例→ok; 未达→fail; 未测量→fail(禁止不证明)。"""
        self.assertEqual(self._gate("memory-ok")["r"], "ok")
        self.assertEqual(self._gate("memory-low")["r"], "memory_bandwidth_low")
        self.assertEqual(self._gate("memory-unmeasured")["r"], "memory_bandwidth_low")

    def test_04_io_gate(self):
        """io 允许低 CPU 但须证据; 短串行 IO 不作发布阻塞。"""
        self.assertEqual(self._gate("io-evidence")["r"], "ok")
        self.assertEqual(self._gate("io-noevidence")["r"], "io_missing_evidence")
        self.assertEqual(self._gate("io-shortserial")["r"], "ok")

    def test_05_mixed_must_split(self):
        """mixed 未拆份→fail; 拆出 compute/io 子区间→ok。"""
        self.assertEqual(self._gate("mixed-split")["r"], "ok")
        self.assertEqual(self._gate("mixed-unsplit")["r"], "mixed_unsplit")

    def test_06_unannotated_priority_and_diag(self):
        """无标注>5s→P1; 诊断消息含阈值说明。"""
        self.assertEqual(self._gate("unannotated")["r"], "unannotated_priority")
        d = self._gate("diag")
        self.assertEqual(d["r"], "low_avg_cores")
        self.assertIn("0.80", d["msg"])

    def test_07_fast_fail_first10s(self):
        """首 10s 低 CPU+非 IO+非内存饱和 → 快速失败; 否则不触发。"""
        self.assertEqual(self._gate("fastfail")["r"], "fast_fail_first_10s")
        self.assertEqual(self._gate("fastfail-no")["r"], "ok")


if __name__ == "__main__":
    unittest.main(verbosity=2)
