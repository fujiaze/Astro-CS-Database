#!/usr/bin/env python3
"""CLI 集成测试: export 进程内生产路由(properties→WCS→采样→原子FITS写)。

按 CLI-001 §6.2 新树同步（原文件用 phase3 run --config）:
  * 命令: export --json <cfg> [--events-jsonl] [-y]（§6.2 / §6.3）;
  * 二进制: 唯一 exe build/acsd（ASTROCS_CLI_BIN 可覆盖）;
  * fixture 源码路径: ARCH-001 迁移后布局（lib/infrastructure/aio +
    lib/algorithms/shared/healpix）, 旧路径回退。

**export 缺陷（显式红, 归属 CLI-002; 依据 ASTROCS_DESIGN §3.5/§6.1 + §6.2）**:
  新树 export 的预检(lib/infrastructure/cli/subcommand.h precheck_config)只把 source 当
  array/string 计数, 而 phase3 会话(lib/phase3_session/p3_session.cpp)要求
  source.hips_dir 对象 ⇒ **没有任何 source 形态能同时过两层**:
    - source={"hips_dir":...} → 预检报 "[error] source 为空" → rc=2, 永不进入会话;
    - source="<path>"        → 预检放行, 会话报 "missing source.hips_dir" → rc=2;
  且 export --template 产出 "source": "" / "center": [ra,dec] 与会话合同不一致
  （§6.1 要求模板输出可直接运行）。因此本文件所有 export 成功/会话级负例断言
  （test_01/02/03/04/05/07/08/09）在 CLI-002 修复前**必然红**, 这是缺陷的显式标记,
  不得用 -force、skip 或放宽断言变绿（TEST-CLI-SYNC 裁决 (a)）。
  修复判据（CLI-002 验收门）: export --json <--template 产物> 不加 -force 直接 rc=0。
  test_06（stdout 纪律）与 test_10（mosaic coverage 负例/正例, 不依赖 export）不受影响。
"""
import hashlib, json, os, re, shutil, subprocess, tempfile, unittest

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))


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

# FIX-UTCLI-HYGIENE: 子进程 cwd 统一落 run/（gitignore），见 cli_test_hygiene.py
from cli_test_hygiene import run_cwd  # noqa: E402


def _pick(*cands):
    for p in cands:
        if os.path.isdir(p):
            return p
    return cands[0]


AIO = _pick(os.path.join(REPO, "lib", "infrastructure", "aio"),
            os.path.join(REPO, "lib", "astro_image_io"))
SHARED = _pick(os.path.join(REPO, "lib", "algorithms", "shared"),
               os.path.join(REPO, "lib", "common"))
HEALPIX_SRC = _pick(os.path.join(SHARED, "healpix", "healpix_core.cpp"),
                    os.path.join(REPO, "lib", "common", "healpix", "healpix_core.cpp"))


def repo_version():
    with open(os.path.join(REPO, "VERSION"), encoding="utf-8") as fh:
        return fh.read().strip()


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
        assert os.path.isfile(EXE), "先构建 CLI（cmake -S . -B build && ninja -C build acsd）"
        cls.tmp = tempfile.mkdtemp(prefix="p3cli_")
        incs = [f"-I{os.path.join(REPO, 'lib', 'include')}",
                f"-I{os.path.join(AIO, 'include')}", f"-I{os.path.join(AIO, 'src')}",
                f"-I{os.path.join(AIO, 'third_party', 'cfitsio')}",
                f"-I{SHARED}",
                f"-I{os.path.dirname(HEALPIX_SRC)}"]
        srcs = [os.path.join(REPO, "eng", "tests", "backend", "phase2_fixture_main.cpp"),
                os.path.join(AIO, "src", "hips", "aio_hips_writer.cpp"),
                os.path.join(AIO, "src", "hips", "aio_hips_reader.cpp"),
                os.path.join(AIO, "src", "aio_fits.cpp"),
                os.path.join(AIO, "src", "aio_api.cpp"),
                os.path.join(AIO, "src", "aio_log.cpp"),
                os.path.join(AIO, "src", "aio_compressor.cpp"),
                HEALPIX_SRC,
                # CTESTFULL-01 根因修复：同 test_phase2_inprocess ——
                # aio_file_io.h 的 inline sha256_hex 依赖 astrocs::crypto::Sha256。
                os.path.join(SHARED, "crypto", "sha256.cpp")]
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
                           text=True, timeout=300, cwd=run_cwd())
        assert "HIPS_FIXTURES_OK" in r.stdout, r.stderr
        cls.hips = os.path.join(cls.data, "FIELD.hips")
        cls.cfg = os.path.join(cls.tmp, "cfg.json")
        with open(cls.cfg, "w", encoding="utf-8") as fh:
            json.dump({
                "source": {"hips_dir": cls.hips},
                "center": {"ra_deg": 210.0, "dec_deg": 34.0},
                "scale_deg_per_px": 0.1,
                "width_px": 40, "height_px": 30,
                "projection": "TAN", "sampler": "nearest",
                "longitude_parity": "east_left", "bitpix": -32,
                "coverage_output": "mask",
                # FZ-P3-MODES：phase3 resample 节点要求显式声明 output_mode（缺键即 REJECT）
                "output_mode": "surface_brightness",
                "output_dir": cls.out, "max_tiles": 16,
            }, fh)

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.tmp, ignore_errors=True)

    def _run(self, *args, **kw):
        # -y: §6.3 跳过运行确认（本文件全部运行在非交互环境）
        return subprocess.run([EXE, *args, "-y"], capture_output=True, text=True,
                              timeout=kw.pop("timeout", 300), cwd=run_cwd(), **kw)

    def _cfg_variant(self, name, **over):
        cfg = os.path.join(self.tmp, name)
        with open(self.cfg, encoding="utf-8") as fh:
            d = json.load(fh)
        d.update(over)
        with open(cfg, "w", encoding="utf-8") as fh:
            json.dump(d, fh)
        return cfg

    def test_01_production_route_complete(self):
        """实际生产函数路由: properties→WCS→sampling→原子FITS; 事件+manifest complete。"""
        r = self._run("export", "--json", self.cfg, "--events-jsonl")
        self.assertEqual(r.returncode, 0, r.stderr[-500:])
        events = [json.loads(l) for l in r.stdout.splitlines() if l.strip()]
        seqs = [e["sequence"] for e in events]
        self.assertEqual(seqs, list(range(len(events))))
        self.assertEqual(events[-1]["kind"], "final")
        self.assertEqual(events[-1]["status"], "ok")
        mpath = [e for e in events if e["kind"] == "artifact" and
                 e.get("role") == "run_manifest"][-1]["path"]
        with open(mpath, encoding="utf-8") as fh:
            m = json.load(fh)
        self.assertEqual(m["status"], "complete")
        self.assertIn(3, m["phases"])
        arts = m.get("artifacts", [])
        self.assertTrue(any(a["path"].endswith(".fits") for a in arts),
                        "输出 FITS artifact 必须存在")
        # B2-A10（宪章 §4.3）: run manifest provenance 必须携带真实值，
        # 禁 p3-node / astrocs-phase3-node 占位与空 manifest hash。
        prov = m.get("provenance")
        self.assertIsInstance(prov, dict, "run manifest 必须含 provenance 节")
        self.assertRegex(prov.get("source_sha", ""), r"^[0-9a-f]{40}$",
                         "source SHA 必须是 40hex 提交号")
        # 形态合同: docs/owner/RELEASE_STATUS.md §2 g<commit>[.dirty]
        # 版本串由根 VERSION 单源派生（禁硬编码 alpha 版本字面量, §12）
        self.assertRegex(prov.get("source_version", ""),
                         r"^" + re.escape(repo_version()) + r"\+g[0-9a-f]+(\.dirty)?$")
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
        """projection≠TAN 显式拒 → exit 2, 无输出 FITS。（会话级诊断）"""
        cfg = self._cfg_variant("bad_proj.json", projection="SIN")
        r = self._run("export", "--json", cfg)
        self.assertEqual(r.returncode, 2, r.stderr[-200:])
        self.assertIn("TAN", r.stderr, "必须是会话级 projection 拒绝诊断")

    def test_03_rejected_center_near_pole(self):
        """P3-001 冻结: abs(dec)>85°(距极点<5°) 显式拒 → exit 2。"""
        cfg = self._cfg_variant("bad_dec.json",
                                center={"ra_deg": 210.0, "dec_deg": 88.0})
        r = self._run("export", "--json", cfg)
        self.assertEqual(r.returncode, 2, r.stderr[-200:])
        self.assertNotIn("source 为空", r.stderr, "不得被预检 source 口径提前拦截")

    def test_04_rejected_scale_bounds(self):
        """width 越界/scale≤0 → exit 2。"""
        cfg = self._cfg_variant("bad_w.json", width_px=0)
        r = self._run("export", "--json", cfg)
        self.assertEqual(r.returncode, 2, r.stderr[-200:])
        self.assertNotIn("source 为空", r.stderr, "不得被预检 source 口径提前拦截")
        cfg = self._cfg_variant("bad_scale.json", scale_deg_per_px=0)
        r = self._run("export", "--json", cfg)
        self.assertEqual(r.returncode, 2, r.stderr[-200:])
        self.assertNotIn("source 为空", r.stderr, "不得被预检 source 口径提前拦截")

    def test_05_both_samplers_produce_valid_output(self):
        """nearest 与 bilinear 都能产出合法 FITS(reopen 成功)。"""
        for samp in ("nearest", "bilinear"):
            out = os.path.join(self.out, samp)
            os.makedirs(out, exist_ok=True)
            cfg = self._cfg_variant("s_%s.json" % samp, sampler=samp, output_dir=out)
            r = self._run("export", "--json", cfg)
            self.assertEqual(r.returncode, 0, r.stderr[-300:])
            self.assertTrue(os.path.isfile(os.path.join(out, "output_phase3.fits")),
                            "%s 缺输出" % samp)

    def test_06_stdout_pure_json_when_jsonl(self):
        """--events-jsonl 模式 stdout 必须纯 JSON(日志进 stderr, 无污染)。

        GATE-502 空断言普查：原实现只在循环里 json.loads(line)，stdout **为空**时
        循环体一次都不执行 ⇒ 空跑也通过（恒真面）。现补：返回码 + 非空 + 逐行可解析
        + 事件流含终态 final（非退化判据）。
        """
        r = self._run("export", "--json", self.cfg, "--events-jsonl")
        self.assertEqual(r.returncode, 0, r.stderr[-400:])
        lines = [l for l in r.stdout.splitlines() if l.strip()]
        self.assertGreater(len(lines), 0, "事件流不得为空（空 stdout 不是通过证据）")
        events = []
        for i, line in enumerate(lines):
            try:
                events.append(json.loads(line))       # 任何非 JSON 行都会抛异常 → 失败
            except json.JSONDecodeError as e:
                self.fail("stdout 第 %d 行不是纯 JSON（日志污染）: %s | %r" % (i + 1, e, line[:120]))
        kinds = [e.get("kind") for e in events]
        self.assertIn("final", kinds, "事件流必须含终态 final 事件")

    def test_07_run_phase3_complete_manifest(self):
        """export 生产编排 → complete manifest(phases==[3]) + phase3_output artifact。"""
        run_dir = os.path.join(self.tmp, "run7")
        os.makedirs(run_dir, exist_ok=True)
        rcfg = self._cfg_variant("run7/cfg7.json", output_dir=run_dir)
        r = self._run("export", "--json", rcfg, "--events-jsonl")
        self.assertEqual(r.returncode, 0, r.stderr[-400:])
        mf = [e for e in (json.loads(l) for l in r.stdout.splitlines() if l.strip())
              if e["kind"] == "artifact" and e.get("role") == "run_manifest"][-1]
        with open(mf["path"], encoding="utf-8") as fh:
            m = json.load(fh)
        self.assertEqual(m["status"], "complete")
        self.assertEqual(m["phases"], [3])
        arts = m.get("artifacts", [])
        self.assertTrue(any(a.get("role") == "phase3_output" and
                            os.path.isfile(a["path"]) for a in arts),
                        "export run 的 manifest 必须记录 phase3_output artifact")

    def test_08_run_resume_hash_mismatch(self):
        """prior manifest artifact 磁盘 hash 不符 → export 退 8(不静默跳过验证)。"""
        run_dir = os.path.join(self.tmp, "run8")
        os.makedirs(run_dir, exist_ok=True)
        stable = os.path.join(run_dir, "stable.fits")
        with open(stable, "wb") as f:
            f.write(b"ORIGINAL")
        with open(stable, "rb") as f:
            sha = hashlib.sha256(f.read()).hexdigest()
        rcfg = self._cfg_variant("run8/cfg8.json", output_dir=run_dir)
        # 构造一个 prior complete manifest 记录 stable.fits 的原始 hash
        with open(os.path.join(run_dir, "astrocs_run_abc123def456.json"), "w",
                  encoding="utf-8") as fh:
            json.dump({"schema_version": "1", "kind": "astrocs_run_manifest",
                       "run_id": "abc123def456", "astrocs_version": "x",
                       "platform": {"os": "linux", "arch": "amd64"},
                       "config_path": rcfg,
                       "config_sha256": "x", "phases": [3],
                       "artifacts": [{"role": "phase3_output", "path": stable,
                                      "sha256": sha, "size_bytes": 8}],
                       "status": "complete", "started_utc": "", "finished_utc": ""}, fh)
        # 篡改 stable.fits → 磁盘 hash 与 prior 记录不符
        with open(stable, "wb") as f:
            f.write(b"TAMPERED!")
        r = self._run("export", "--json", rcfg, "--events-jsonl")
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
        cfg = self._cfg_variant("run9/cfg9.json", output_dir=run_dir)
        r = self._run("export", "--json", cfg, "--events-jsonl")
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
            mf = [e for e in (json.loads(l) for l in r.stdout.splitlines() if l.strip())
                  if e["kind"] == "artifact" and e.get("role") == "run_manifest"][-1]
            with open(mf["path"], encoding="utf-8") as fh:
                self.assertEqual(h.get("RUNID"), json.load(fh)["run_id"],
                                 "FITS RUNID 必须是本 run 的真实 run_id")
            self.assertNotEqual(h.get("SWVER"), "astrocs-phase3-node")
            # 长 HISTORY 值会被 FITS 68 字符卡片边界拆成多张 HISTORY 卡
            # （cfitsio 惯例）；按读者语义把连续 HISTORY 卡重新拼接后检查。
            hist_parts = [str(c.value) for c in h.cards if c.keyword == "HISTORY"]
            hist_joined = "".join(p.strip().rstrip("&") for p in hist_parts)
            self.assertRegex(hist_joined, r"manifest=[0-9a-f]{64}",
                             "HISTORY 必须携带 64hex 输入产品清单哈希: %r" % hist_parts)
            # B2-A9: BSCALE/BZERO 若存在必须是标准浮点类型（FITS 4.0 §4.4.2.4）
            for k in ("BSCALE", "BZERO"):
                if k in h:
                    self.assertIsInstance(h[k], float, "%s 必须是浮点（FITS 4.0）" % k)
            hdus.close()

    def test_11_template_runnable_without_force(self):
        """CLI-002 验收门（§6.1/§6.3 + GAP-034 D3）: export --template 产出的配置
        **结构可直接运行** —— 只填路径（不改结构）后不加 -force 即 rc=0。

        模板的占位值（空 hips_dir / output_dir "."）本身不是可运行输入（与
        normalize 模板的空 input_lights 同款），因此判据是「结构同源 + 只改路径可跑」。
        """
        tpl_path = os.path.join(self.tmp, "tpl", "e.json")
        os.makedirs(os.path.dirname(tpl_path), exist_ok=True)
        r = self._run("export", "--template", "-o", tpl_path)
        self.assertEqual(r.returncode, 0, r.stderr)
        with open(tpl_path, encoding="utf-8") as fh:
            doc = json.load(fh)
        # 与运行期会话同源的对象形态（source.hips_dir / center.ra_deg|dec_deg）
        self.assertIsInstance(doc["source"], dict, "模板 source 必须是对象")
        self.assertIn("hips_dir", doc["source"])
        self.assertIsInstance(doc["center"], dict, "模板 center 必须是对象")
        self.assertIn("ra_deg", doc["center"])
        self.assertIn("dec_deg", doc["center"])
        out = os.path.join(self.tmp, "tpl_out")
        os.makedirs(out, exist_ok=True)
        doc["source"]["hips_dir"] = self.hips      # 只填路径
        doc["output_dir"] = out
        cfg = os.path.join(self.tmp, "tpl_run.json")
        with open(cfg, "w", encoding="utf-8") as fh:
            json.dump(doc, fh)
        r2 = self._run("export", "--json", cfg)
        self.assertEqual(r2.returncode, 0, r2.stderr[-400:])
        self.assertTrue(os.path.isfile(os.path.join(out, "output_phase3.fits")))

    def test_10_coverage_requires_filter_and_nested_ordering(self):
        """B2-A8: coverage 对缺 obs_filter / hips_ordering!=NESTED 的输入必须 fail-closed。
        三条臂共用同一份 FIFO 生成的真实 HiPS 树（properties 键直接替换）。（mosaic 域, 不经 export）

        夹具选择（本用例单独生成，不动 setUpClass 的 --make-field）：
        正例臂要求 mosaic 整链 rc=0，而默认权重是**逐帧逆方差**（权重 = Phase2 消费
        帧级 SNR 时现场派生量，ASTROCS_DESIGN §2.1 / §9.73 裁决 A44），缺逐帧
        variance/ivar 时按 DATA-UNC-001 §30.1 **禁止静默回退等权** ⇒ 用只有
        SIGNAL|SUPPORT 的 --make-field 夹具时正例臂必然 rc=2（实测
        "2/2 frames missing ivar ... weight chain NOT closed"）。
        故此处用 --make（B1-A5：等权合成帧 σ=0.1 ⇒ 写 variance/ivar 子产品，
        默认链可闭合），负例两臂同用该夹具以保持单一夹具口径。"""
        work = os.path.join(self.tmp, "cov_neg")
        os.makedirs(work, exist_ok=True)
        fx = os.path.join(self.tmp, "cov_fx")
        os.makedirs(fx, exist_ok=True)
        r = subprocess.run([self.fixture, "--make", fx], capture_output=True, text=True,
                           timeout=300, cwd=run_cwd())
        assert "HIPS_FIXTURES_OK" in r.stdout, r.stderr
        ok = os.path.join(work, "F1.hips")
        if not os.path.isdir(ok):
            shutil.copytree(os.path.join(fx, "F1.hips"), ok)
        copy = os.path.join(work, "F2.hips")
        shutil.copytree(os.path.join(fx, "F2.hips"), copy)
        props = os.path.join(copy, "signal", "properties")
        self.assertTrue(os.path.isfile(props))
        with open(props, encoding="utf-8") as fh:
            orig = fh.readlines()
        self.assertTrue(any(l.startswith("obs_filter") for l in orig),
                        "AIO 写侧必须恒写 obs_filter（B2-A8）")
        # 负例 1: 删除 obs_filter 键 → coverage 必须拒绝
        with open(props, "w", encoding="utf-8") as fh:
            fh.write("".join(l for l in orig if not l.startswith("obs_filter")))
        out1 = os.path.join(work, "o1"); os.makedirs(out1)
        cfg1 = os.path.join(work, "c1.json")
        with open(cfg1, "w", encoding="utf-8") as fh:
            json.dump({"schema_version": "1", "hips_paths": [ok, copy],
                       "output_dir": out1}, fh)
        r = self._run("mosaic", "--json", cfg1)
        self.assertNotEqual(r.returncode, 0, "缺 obs_filter 必须 fail-closed（B2-A8）")
        self.assertIn("obs_filter", r.stderr)
        # 负例 2: 显式 hips_ordering=RING → coverage 必须拒绝
        with open(props, "w", encoding="utf-8") as fh:
            fh.write("".join(orig) + "hips_ordering=RING\n")
        out2 = os.path.join(work, "o2"); os.makedirs(out2)
        cfg2 = os.path.join(work, "c2.json")
        with open(cfg2, "w", encoding="utf-8") as fh:
            json.dump({"schema_version": "1", "hips_paths": [ok, copy],
                       "output_dir": out2}, fh)
        r = self._run("mosaic", "--json", cfg2)
        self.assertNotEqual(r.returncode, 0, "hips_ordering=RING 必须 fail-closed（B2-A8）")
        self.assertIn("NESTED", r.stderr)
        # 正例: 复原 properties（obs_filter 存在且 NESTED 缺省）→ rc=0
        with open(props, "w", encoding="utf-8") as fh:
            fh.write("".join(orig))
        out3 = os.path.join(work, "o3"); os.makedirs(out3)
        cfg3 = os.path.join(work, "c3.json")
        with open(cfg3, "w", encoding="utf-8") as fh:
            json.dump({"schema_version": "1", "hips_paths": [ok, copy],
                       "output_dir": out3}, fh)
        r = self._run("mosaic", "--json", cfg3)
        self.assertEqual(r.returncode, 0, r.stderr[-400:])


if __name__ == "__main__":
    unittest.main(verbosity=2)
