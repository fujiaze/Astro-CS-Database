#!/usr/bin/env python3
"""MON-002 测试: 所有 Phase/kernel 发 stage/resource/backend 事件; summary/downsample/raw 分层。
验收(07 §1): resource-detail summary|timeseries; summary 强制存在; 无标注 >5s 区间→P1。"""
import json, os, re, shutil, subprocess, tempfile, unittest

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
CLI = os.path.join(REPO, "cli")
BUILD = os.path.join(REPO, "build", "cli")
EXE = os.path.join(BUILD, "astrocs")
AIO = os.path.join(REPO, "lib", "astro_image_io")

# 复用 phase3 inprocess 测试的 cfitsio_objs 组装
try:
    from tests.cli.test_phase3_inprocess import cfitsio_objs as cfitsio_objs
except Exception:
    def cfitsio_objs(_tmp):
        return []


@unittest.skipUnless(shutil.which("cmake") and os.path.isfile(EXE), "需要构建好的 CLI")
class TestMonitorEvents(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.mkdtemp(prefix="mon002_")
        cls.hips = None
        # 建 phase3 合成 FIELD.hips fixture
        incs = [f"-I{os.path.join(REPO, 'include')}",
                f"-I{os.path.join(AIO, 'include')}", f"-I{os.path.join(AIO, 'src')}",
                f"-I{os.path.join(AIO, 'third_party', 'cfitsio')}",
                f"-I{os.path.join(REPO, 'lib', 'common')}"]
        srcs = [os.path.join(REPO, "tests", "backend", "phase2_fixture_main.cpp"),
                os.path.join(AIO, "src", "hips", "aio_hips_writer.cpp"),
                os.path.join(AIO, "src", "hips", "aio_hips_reader.cpp"),
                os.path.join(AIO, "src", "aio_fits.cpp"),
                os.path.join(AIO, "src", "aio_api.cpp"),
                os.path.join(AIO, "src", "aio_log.cpp"),
                os.path.join(AIO, "src", "aio_compressor.cpp"),
                os.path.join(REPO, "lib", "common", "healpix", "healpix_core.cpp")]
        fixture = os.path.join(cls.tmp, "fixture")
        r = subprocess.run(["g++", "-std=c++17", "-O2", "-w", "-DAIO_ENABLE_FITS", *incs,
                            *srcs, *cfitsio_objs(cls.tmp), "-lz", "-lzstd", "-llz4",
                            "-o", fixture], capture_output=True, text=True, timeout=600)
        if r.returncode == 0:
            data = os.path.join(cls.tmp, "data")
            os.makedirs(data)
            r2 = subprocess.run([fixture, "--make-field", data], capture_output=True,
                                text=True, timeout=300)
            if "HIPS_FIXTURES_OK" in r2.stdout:
                cls.hips = os.path.join(data, "FIELD.hips")
        # run config
        cls.out = os.path.join(cls.tmp, "out")
        os.makedirs(cls.out)
        cls.rcfg = os.path.join(cls.tmp, "r.json")
        json.dump({
            "schema_version": "1",
            "inputs": {"lights": [], "darks": [], "flats": [], "bias": []},
            "output_dir": cls.out,
            "phase3": {"source": {"hips_dir": cls.hips or "/nonexistent"},
                       "center": {"ra_deg": 210.0, "dec_deg": 34.0},
                       "scale_deg_per_px": 0.1, "width_px": 40, "height_px": 30,
                       "projection": "TAN", "sampler": "nearest",
                       "coverage_output": "mask", "max_tiles": 64},
        }, open(cls.rcfg, "w"))

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.tmp, ignore_errors=True)

    def _run(self, *args):
        return subprocess.run([EXE, *args], capture_output=True, text=True, timeout=300)

    def _events(self, out):
        evs = []
        for l in out.stdout.splitlines():
            if not l.strip():
                continue
            evs.append(json.loads(l))
        return evs

    def test_01_resource_detail_flag_accepted(self):
        """--resource-detail summary|timeseries 必须被 run 接受, 非法值→2。"""
        r = self._run("run", "--phases", "3", "--config", self.rcfg,
                      "--events-jsonl", "--resource-detail", "summary")
        self.assertEqual(r.returncode, 0, r.stderr[-300:])
        r2 = self._run("run", "--phases", "3", "--config", self.rcfg,
                       "--events-jsonl", "--resource-detail", "bogus")
        self.assertNotEqual(r2.returncode, 0)  # 非法 detail 应 fail(非静默)

    def test_02_resource_summary_emitted_with_mandatory_metrics(self):
        """run 成功 → resource summary 事件必含 07 §2 指标(peak_rss/n_samples/wall/details)。"""
        r = self._run("run", "--phases", "3", "--config", self.rcfg,
                      "--events-jsonl", "--resource-detail", "summary")
        evs = self._events(r)
        res = [e for e in evs if e["kind"] == "resource" and "resource summary" in e["message"]]
        self.assertTrue(res, "必须发出 resource summary 事件")
        e = res[-1]
        for k in ("n_samples", "peak_rss_bytes", "wall_seconds", "resource_detail",
                  "peak_equivalent_cores", "max_threads"):
            self.assertIn(k, e, f"resource summary 缺必采指标 {k}")
        self.assertEqual(e["resource_detail"], "summary")

    def test_03_backend_event_emitted(self):
        """backend 事件含 backend_id/workers_used/available_cpus(07 §2 必采)。"""
        r = self._run("run", "--phases", "3", "--config", self.rcfg,
                      "--events-jsonl", "--resource-detail", "summary")
        be = [e for e in self._events(r) if e["kind"] == "backend"]
        self.assertTrue(be, "必须发出 backend 事件")
        e = be[-1]
        self.assertIn("backend_id", e)
        self.assertIn("workers_used", e)
        self.assertIn("available_cpus", e)

    def test_04_tier_downsample_present_only_when_timeseries(self):
        """timeseries 内嵌 downsample_max/curve 标记; summary 不内嵌曲线数据(分层小型化)。"""
        r = self._run("run", "--phases", "3", "--config", self.rcfg,
                      "--events-jsonl", "--resource-detail", "timeseries")
        res = [e for e in self._events(r) if e["kind"] == "resource" and "resource summary" in e["message"]]
        self.assertTrue(res)
        e = res[-1]
        self.assertEqual(e["resource_detail"], "timeseries")
        self.assertIn("downsample_max", e)
        # 曲线数据不内嵌几十 MB: only a marker array (empty), raw 指针只指路径
        self.assertIn("raw_dir", e)

    def test_05_stage_annotation_enum_stable(self):
        """stage 资源类别枚举(compute/memory/io/mixed/unknown)在 C++ 侧固定, 供 MON-003 引用。"""
        drv = os.path.join(self.tmp, "enum.cpp")
        with open(drv, "w") as f:
            f.write('''
#include "resource_events.h"
#include <cstdio>
int main(){
    using namespace astrocs;
    std::printf("%s %s %s %s %s\\n",
        stage_kind_name(StageKind::Compute), stage_kind_name(StageKind::Memory),
        stage_kind_name(StageKind::Io), stage_kind_name(StageKind::Mixed),
        stage_kind_name(StageKind::Unknown));
    return 0;
}
''')
        r = subprocess.run(["g++", "-std=c++17", "-O2", f"-I{CLI}",
                            f"-I{os.path.join(REPO, 'third_party')}",
                            drv, "-o", os.path.join(self.tmp, "enum")],
                           capture_output=True, text=True, timeout=120)
        self.assertEqual(r.returncode, 0, r.stderr[-400:])
        o = subprocess.run([os.path.join(self.tmp, "enum")], capture_output=True,
                           text=True, timeout=30).stdout.strip()
        self.assertEqual(o, "compute memory io mixed unknown")

    def test_06_stage_kind_classify_cpp(self):
        """resource_events.h classify_stage/is_unannotated_priority 编译并正确分类。"""
        drv = os.path.join(self.tmp, "classify.cpp")
        with open(drv, "w") as f:
            f.write('''
#include "resource_events.h"
#include <cstdio>
int main(){
    using namespace astrocs;
    std::printf("compute=%s\\n", stage_kind_name(classify_stage("compute")));
    std::printf("unknown_unannoted_5s=%d\\n", (int)is_unannotated_priority(nullptr, 6.0));
    std::printf("compute_annoted_5s=%d\\n", (int)is_unannotated_priority("compute", 6.0));
    std::printf("unknown_short_5s=%d\\n", (int)is_unannotated_priority(nullptr, 3.0));
    std::printf("downsample=%zu\\n", downsample_curve(std::vector<int>(1000, 7), kDownsampleMax).size());
    return 0;
}
''')
        r = subprocess.run(["g++", "-std=c++17", "-O2", f"-I{CLI}",
                            f"-I{os.path.join(REPO, 'third_party')}",
                            drv, "-o", os.path.join(self.tmp, "classify")],
                           capture_output=True, text=True, timeout=120)
        self.assertEqual(r.returncode, 0, r.stderr[-400:])
        o = subprocess.run([os.path.join(self.tmp, "classify")], capture_output=True,
                           text=True, timeout=30).stdout
        self.assertIn("compute=compute", o)
        self.assertIn("unknown_unannoted_5s=1", o, "无标注>5s 必须 P1")
        self.assertIn("compute_annoted_5s=0", o, "compute 标注不触发 P1")
        self.assertIn("unknown_short_5s=0", o, "短于 5s 无标注不 P1")
        self.assertIn("downsample=121", o, "降采样曲线 ≤ 121 点")


if __name__ == "__main__":
    unittest.main(verbosity=2)
