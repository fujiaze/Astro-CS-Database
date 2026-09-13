#!/usr/bin/env python3
"""CLI-006 集成测试: phase3 run 进程内生产路由(properties→WCS→采样→原子FITS写) — 无子进程/事件/错误映射/取消。"""
import hashlib, json, math, os, re, shutil, subprocess, tempfile, unittest

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
                f"-I{os.path.join(REPO, 'lib', 'common')}",
                f"-I{os.path.join(REPO, 'lib', 'common', 'healpix')}"]
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
            "output_dir": cls.out, "max_tiles": 16,
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
        # B2-A10（宪章 §4.3）: run manifest provenance 必须携带真实值，
        # 禁 p3-node / astrocs-phase3-node 占位与空 manifest hash。
        prov = m.get("provenance")
        self.assertIsInstance(prov, dict, "run manifest 必须含 provenance 节")
        self.assertRegex(prov.get("source_sha", ""), r"^[0-9a-f]{40}$",
                         "source SHA 必须是 40hex 提交号")
        # 形态合同: docs/governance/VERSION_NAMESPACES.md:79/83 `g<commit12>[.dirty]`
        self.assertRegex(prov.get("source_version", ""),
                         r"^0\.11\.0-alpha\.2\+g[0-9a-f]+(\.dirty)?$")
        self.assertTrue(prov.get("algorithm_ids"), "algorithm_ids 不得为空")
        self.assertTrue(prov.get("module_build_ids"), "module_build_ids 不得为空")
        self.assertIn("baseline", prov.get("providers", []))
        in_h = prov.get("input_product_hashes", [])
        self.assertTrue(any(len(x.get("sha256", "")) == 64 for x in in_h),
                        "input_product_hashes 必须含 64hex 输入产品哈希")
        out_h = prov.get("output_product_hashes", [])
        self.assertTrue(any(x.get("role") == "phase3_output" and
                            len(x.get("sha256", "")) == 64 for x in out_h),
                        "output_product_hashes 必须含 phase3_output 哈希")

    def test_02_rejected_projection_returns_2(self):
        """projection≠TAN 显式拒 → exit 2, 无输出 FITS。"""
        cfg = os.path.join(self.tmp, "bad.json")
        d = json.load(open(self.cfg)); d["projection"] = "SIN"
        json.dump(d, open(cfg, "w"))
        r = self._run("phase3", "run", "--config", cfg)
        self.assertEqual(r.returncode, 2, r.stderr[-200:])
        self.assertIn("TAN", r.stderr)

    def test_03_rejected_center_near_pole(self):
        """P3-001 冻结: abs(dec)>85°(距极点<5°) 显式拒 → exit 2。"""
        cfg = os.path.join(self.tmp, "bad.json")
        d = json.load(open(self.cfg)); d["center"]["dec_deg"] = 88.0
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

    def test_07_run_phases3_complete(self):
        """CLI-002: phase3 run 生产编排 → complete manifest + phase3_output artifact。"""
        run_dir = os.path.join(self.tmp, "run7")
        os.makedirs(run_dir, exist_ok=True)
        rcfg = os.path.join(run_dir, "rcfg.json")
        json.dump({
            "schema_version": "1",
            "inputs": {"lights": [], "darks": [], "flats": [], "bias": []},
            "output_dir": run_dir,
            "phase3": {
                "source": {"hips_dir": self.hips},
                "center": {"ra_deg": 210.0, "dec_deg": 34.0},
                "scale_deg_per_px": 0.1,
                "width_px": 40, "height_px": 30,
                "projection": "TAN", "sampler": "nearest",
                "longitude_parity": "east_left", "bitpix": -32,
                "coverage_output": "mask", "max_tiles": 16,
            },
        }, open(rcfg, "w"))
        # CLI-002: 顶层 run --phases 已移除; phase3 run 单相(等价的完整生产 manifest 语义)
        r = self._run("phase3", "run", "--config", rcfg, "--events-jsonl")
        self.assertEqual(r.returncode, 0, r.stderr[-400:])
        mf = [e for e in (json.loads(l) for l in r.stdout.splitlines() if l.strip())
              if e["kind"] == "artifact" and e.get("role") == "run_manifest"][-1]
        m = json.load(open(mf["path"], encoding="utf-8"))
        self.assertEqual(m["status"], "complete")
        self.assertEqual(m["phases"], [3])
        arts = m.get("artifacts", [])
        # FIX-E2E B1-A2: manifest.artifacts 现含全节点产物（部分无 role 键），
        # 断言只看存在 phase3_output 角色且文件在盘（用 get 避免 KeyError）。
        self.assertTrue(any(a.get("role") == "phase3_output" and
                            os.path.isfile(a["path"]) for a in arts),
                        "phase3 run 的 manifest 必须记录 phase3_output artifact")

    def test_08_run_resume_hash_mismatch(self):
        """CLI-002: prior manifest artifact 磁盘 hash 不符 → phase3 run 退 8(不静默跳过验证)。"""
        run_dir = os.path.join(self.tmp, "run8")
        os.makedirs(run_dir, exist_ok=True)
        stable = os.path.join(run_dir, "stable.fits")
        with open(stable, "wb") as f:
            f.write(b"ORIGINAL")
        sha = hashlib.sha256(open(stable, "rb").read()).hexdigest()
        rcfg = os.path.join(run_dir, "rcfg.json")
        # 构造一个 prior complete manifest 记录 stable.fits 的原始 hash
        json.dump({"schema_version": "1", "kind": "astrocs_run_manifest",
                   "run_id": "abc123def456", "astrocs_version": "x",
                   "platform": {"os": "linux", "arch": "amd64"},
                   "config_path": rcfg,
                   "config_sha256": "x", "phases": [3],
                   "artifacts": [{"role": "phase3_output", "path": stable,
                                  "sha256": sha, "size_bytes": 8}],
                   "status": "complete", "started_utc": "", "finished_utc": ""},
                  open(os.path.join(run_dir, "astrocs_run_prior.json"), "w"))
        json.dump({
            "schema_version": "1",
            "inputs": {"lights": [], "darks": [], "flats": [], "bias": []},
            "output_dir": run_dir,
            "phase3": {
                "source": {"hips_dir": self.hips},
                "center": {"ra_deg": 210.0, "dec_deg": 34.0},
                "scale_deg_per_px": 0.1,
                "width_px": 40, "height_px": 30,
                "projection": "TAN", "sampler": "nearest",
                "coverage_output": "mask", "max_tiles": 16,
            },
        }, open(rcfg, "w"))
        # 篡改 stable.fits → 磁盘 hash 与 prior 记录不符
        with open(stable, "wb") as f:
            f.write(b"TAMPERED!")
        r = self._run("phase3", "run", "--config", rcfg, "--events-jsonl")
        self.assertEqual(r.returncode, 8, r.stderr[-300:])
        self.assertIn("hash mismatch", r.stderr)

    def test_09_fits_standard_checksum_and_wcs_provenance(self):
        """B2-A9 + B2-A10: 输出 FITS 通过 astropy checksum=True；RUNID/SWVER/
        manifest= 为真实值（非占位），且 DATASUM/CHECKSUM 为标准字符串。"""
        try:
            from astropy.io import fits  # 独立标准读取器（非项目实现）
        except ImportError:
            self.skipTest("astropy 不可用（独立 Oracle 缺失，不得伪造 PASS）")
        import warnings
        run_dir = os.path.join(self.tmp, "run9")
        os.makedirs(run_dir, exist_ok=True)
        cfg = os.path.join(run_dir, "cfg.json")
        d = json.load(open(self.cfg))
        d["output_dir"] = run_dir
        json.dump(d, open(cfg, "w"))
        r = self._run("phase3", "run", "--config", cfg, "--events-jsonl")
        self.assertEqual(r.returncode, 0, r.stderr[-400:])
        fits_path = os.path.join(run_dir, "output_phase3.fits")
        self.assertTrue(os.path.isfile(fits_path))
        with warnings.catch_warnings(record=True) as w:
            warnings.simplefilter("always")
            hdus = fits.open(fits_path, checksum=True)
            bad = [str(x.message) for x in w
                   if "verification failed" in str(x.message).lower()]
            self.assertEqual(bad, [], "astropy 标准校验必须通过: %r" % bad)
            for x in hdus:
                self.assertIsInstance(x.header.get("DATASUM"), str,
                                      "DATASUM 必须是标准字符串 (%s)" % x.name)
                self.assertIsInstance(x.header.get("CHECKSUM"), str,
                                      "CHECKSUM 必须是标准字符串 (%s)" % x.name)
            h = hdus[0].header
            self.assertEqual(h.get("RUNID"),
                             json.load(open([e for e in (json.loads(l) for l in
                                             r.stdout.splitlines() if l.strip())
                                             if e["kind"] == "artifact" and
                                             e.get("role") == "run_manifest"][-1]["path"],
                                            encoding="utf-8"))["run_id"],
                             "FITS RUNID 必须是本 run 的真实 run_id")
            self.assertNotEqual(h.get("SWVER"), "astrocs-phase3-node")
            # 长 HISTORY 值会被 FITS 68 字符卡片边界拆成多张 HISTORY 卡
            # （cfitsio 惯例）；按读者语义把连续 HISTORY 卡重新拼接后检查。
            hist_parts = [str(c.value) for c in h.cards if c.keyword == "HISTORY"]
            hist_joined = "".join(p.strip().rstrip("&") for p in hist_parts)
            self.assertRegex(hist_joined, r"manifest=[0-9a-f]{64}",
                             "HISTORY 必须携带 64hex 输入产品清单哈希: %r" % hist_parts)
            # B2-A9: BSCALE/BZERO 若存在必须是标准浮点类型（FITS 4.0 §4.4.2.4）;
            # 旧实现以 TINT 写整数 1/0 属类型违规（AUD-COORD F-09）。
            for k in ("BSCALE", "BZERO"):
                if k in h:
                    self.assertIsInstance(h[k], float,
                                          "%s 必须是浮点（FITS 4.0）" % k)
            hdus.close()

    def test_10_coverage_requires_filter_and_nested_ordering(self):
        """B2-A8: Phase2 coverage 对缺 obs_filter / hips_ordering!=NESTED 的
        输入必须 fail-closed（真实 filter/ordering 负例，非"路径不存在"伪负例）。
        负例以 FIFO 生成的真实 HiPS 树构造，直接替换 properties 键。"""
        import glob
        work = os.path.join(self.tmp, "cov_neg")
        os.makedirs(work, exist_ok=True)
        ok = os.path.join(work, "F1.hips")
        if not os.path.isdir(ok):
            shutil.copytree(self.hips, ok)
        copy = os.path.join(work, "F2.hips")
        shutil.copytree(ok, copy)
        props = os.path.join(copy, "signal", "properties")
        self.assertTrue(os.path.isfile(props))
        orig = open(props, encoding="utf-8").readlines()
        self.assertTrue(any(l.startswith("obs_filter") for l in orig),
                        "AIO 写侧必须恒写 obs_filter（B2-A8）")
        # 负例 1: 删除 obs_filter 键 → coverage 必须拒绝
        open(props, "w", encoding="utf-8").write(
            "".join(l for l in orig if not l.startswith("obs_filter")))
        out1 = os.path.join(work, "o1"); os.makedirs(out1)
        cfg1 = os.path.join(work, "c1.json")
        json.dump({"schema_version": "1", "hips_paths": [ok, copy],
                   "output_dir": out1, "weight_mode": 1}, open(cfg1, "w"))
        r = self._run("phase2", "run", "--config", cfg1)
        self.assertNotEqual(r.returncode, 0,
                            "缺 obs_filter 必须 fail-closed（B2-A8）")
        self.assertIn("obs_filter", r.stderr)
        # 负例 2: 显式 hips_ordering=RING → coverage 必须拒绝
        open(props, "w", encoding="utf-8").write(
            "".join(orig) + "hips_ordering=RING\n")
        out2 = os.path.join(work, "o2"); os.makedirs(out2)
        cfg2 = os.path.join(work, "c2.json")
        json.dump({"schema_version": "1", "hips_paths": [ok, copy],
                   "output_dir": out2, "weight_mode": 1}, open(cfg2, "w"))
        r = self._run("phase2", "run", "--config", cfg2)
        self.assertNotEqual(r.returncode, 0,
                            "hips_ordering=RING 必须 fail-closed（B2-A8）")
        self.assertIn("NESTED", r.stderr)
        # 正例: 复原 properties（obs_filter 存在且 NESTED 缺省）→ rc=0
        open(props, "w", encoding="utf-8").write("".join(orig))
        out3 = os.path.join(work, "o3"); os.makedirs(out3)
        cfg3 = os.path.join(work, "c3.json")
        json.dump({"schema_version": "1", "hips_paths": [ok, copy],
                   "output_dir": out3, "weight_mode": 1}, open(cfg3, "w"))
        r = self._run("phase2", "run", "--config", cfg3)
        self.assertEqual(r.returncode, 0, r.stderr[-400:])

if __name__ == "__main__":
    unittest.main(verbosity=2)
