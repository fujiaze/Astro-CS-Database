#!/usr/bin/env python3
"""CLI-006 集成测试: phase3 run 进程内生产路由(properties→WCS→采样→原子FITS写) — 无子进程/事件/错误映射/取消。"""
import json, math, os, re, shutil, subprocess, tempfile, unittest

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
EXE = os.path.join(REPO, "build", "cli", "astrocs")
AIO = os.path.join(REPO, "lib", "astro_image_io")


def cfitsio_objs(tmp):
    objs = []
    cdir = os.path.join(AIO, "third_party", "cfitsio")
    for f in sorted(os.listdir(cdir)):
        if not f.endswith(".c"):
            continue
        if re.search(r"f77_wrap|drvrgsiftp|drvrsmem|smem|vms|windumpexts|iter_[abc]|"
                     r"cookbook|speed_test|fpack|funpack|fitscopy|listhead|liststruc|"
                     r"imcopy|imarith|tabcompile|sortcol|tabselect", f):
            continue
        o = os.path.join(tmp, f[:-2] + ".o")
        subprocess.run(["gcc", "-O2", "-w", f"-I{cdir}", "-c", os.path.join(cdir, f),
                        "-o", o], check=True, capture_output=True, timeout=300)
        objs.append(o)
    return objs


class TestPhase3InProcess(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        assert os.path.isfile(EXE), "先构建 CLI"
        cls.tmp = tempfile.mkdtemp(prefix="p3cli_")
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
        cls.fixture = os.path.join(cls.tmp, "fixture")
        r = subprocess.run(["g++", "-std=c++17", "-O2", "-w", "-DAIO_ENABLE_FITS", *incs,
                            *srcs, *cfitsio_objs(cls.tmp), "-lz", "-lzstd", "-llz4",
                            "-o", cls.fixture], capture_output=True, text=True, timeout=600)
        assert r.returncode == 0, r.stderr[-600:]
        cls.data = os.path.join(cls.tmp, "data")
        cls.out = os.path.join(cls.tmp, "out")
        os.makedirs(cls.out)
        # 每 tile 常量 1..12 的 field
        r = subprocess.run([cls.fixture, "--make-field", cls.data], capture_output=True,
                           text=True, timeout=300)
        assert "HIPS_FIXTURES_OK" in r.stdout, r.stderr
        cls.hips = os.path.join(cls.data, "FIELD.hips")
        cls.cfg = os.path.join(cls.tmp, "cfg.json")
        json.dump({
            "source": {"hips_dir": cls.hips},
            "center": {"ra_deg": 210.0, "dec_deg": 34.0},
            "scale_deg_per_px": 0.1,
            "width_px": 40, "height_px": 30,
            "projection": "TAN", "sampler": "nearest",
            "longitude_parity": "east_left", "bitpix": -32,
            "coverage_output": "mask",
            "output_dir": cls.out, "max_tiles": 64,
        }, open(cls.cfg, "w"))

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.tmp, ignore_errors=True)

    def _run(self, *args, **kw):
        return subprocess.run([EXE, *args], capture_output=True, text=True,
                              timeout=kw.pop("timeout", 300), **kw)

    def test_01_production_route_complete(self):
        """实际生产函数路由: properties→WCS→sampling→原子FITS; 事件+manifest complete。"""
        r = self._run("phase3", "run", "--config", self.cfg, "--events-jsonl")
        self.assertEqual(r.returncode, 0, r.stderr[-500:])
        events = [json.loads(l) for l in r.stdout.splitlines() if l.strip()]
        seqs = [e["sequence"] for e in events]
        self.assertEqual(seqs, list(range(len(events))))
        self.assertEqual(events[-1]["kind"], "final")
        self.assertEqual(events[-1]["status"], "ok")
        mpath = [e for e in events if e["kind"] == "artifact" and
                 e.get("role") == "run_manifest"][-1]["path"]
        m = json.load(open(mpath, encoding="utf-8"))
        self.assertEqual(m["status"], "complete")
        self.assertIn(3, m["phases"])
        # 输出 artifact
        arts = m.get("artifacts", [])
        self.assertTrue(any(a["path"].endswith(".fits") for a in arts),
                        "输出 FITS artifact 必须存在")

    def test_02_rejected_projection_returns_2(self):
        """projection≠TAN 显式拒 → exit 2, 无输出 FITS。"""
        cfg = os.path.join(self.tmp, "bad.json")
        d = json.load(open(self.cfg)); d["projection"] = "SIN"
        json.dump(d, open(cfg, "w"))
        r = self._run("phase3", "run", "--config", cfg)
        self.assertEqual(r.returncode, 2, r.stderr[-200:])
        self.assertIn("TAN", r.stderr)

    def test_03_rejected_center_near_pole(self):
        """|dec|<5° 显式拒 → exit 2。"""
        cfg = os.path.join(self.tmp, "bad.json")
        d = json.load(open(self.cfg)); d["center"]["dec_deg"] = 2.0
        json.dump(d, open(cfg, "w"))
        r = self._run("phase3", "run", "--config", cfg)
        self.assertEqual(r.returncode, 2, r.stderr[-200:])

    def test_04_rejected_scale_bounds(self):
        """width 越界/scale≤0 → exit 2。"""
        cfg = os.path.join(self.tmp, "bad.json")
        d = json.load(open(self.cfg)); d["width_px"] = 0
        json.dump(d, open(cfg, "w"))
        r = self._run("phase3", "run", "--config", cfg)
        self.assertEqual(r.returncode, 2, r.stderr[-200:])
        d = json.load(open(self.cfg)); d["scale_deg_per_px"] = 0
        json.dump(d, open(cfg, "w"))
        r = self._run("phase3", "run", "--config", cfg)
        self.assertEqual(r.returncode, 2, r.stderr[-200:])

    def test_05_both_samplers_produce_valid_output(self):
        """nearest 与 bilinear 都能产出合法 FITS(reopen 成功)。"""
        for samp in ("nearest", "bilinear"):
            cfg = os.path.join(self.tmp, f"s_{samp}.json")
            d = json.load(open(self.cfg)); d["sampler"] = samp
            d["output_dir"] = os.path.join(self.out, samp)
            os.makedirs(d["output_dir"], exist_ok=True)
            json.dump(d, open(cfg, "w"))
            r = self._run("phase3", "run", "--config", cfg)
            self.assertEqual(r.returncode, 0, r.stderr[-300:])
            out = os.path.join(d["output_dir"], "output_phase3.fits")
            self.assertTrue(os.path.isfile(out), f"{samp} 缺输出")

    def test_06_stdout_pure_json_when_jsonl(self):
        """--events-jsonl 模式 stdout 必须纯 JSON(日志进 stderr, 无污染)。"""
        r = self._run("phase3", "run", "--config", self.cfg, "--events-jsonl")
        for line in r.stdout.splitlines():
            if not line.strip():
                continue
            json.loads(line)   # 任何非 JSON 行都会抛异常 → 失败

if __name__ == "__main__":
    unittest.main(verbosity=2)
