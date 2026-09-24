#!/usr/bin/env python3
"""E2E: 正式 CLI normalize → 持久化 → mosaic → 持久化 → export 真链路（CLI-002 重锚）。

数据流(与生产 M42 流程一致: 逐帧 normalize 产品 → mosaic 拼接 → export 投影):
  normalize(light_1) ─┐
  normalize(light_2) ─┴→ mosaic(hips_paths=[p1a,p1b]) ─→ export(读 p2out)
每阶段独立进程、只读上游**持久化 HiPS 产品**（无任何 fixture 顶替）; 断言
rc=0 / status=complete / artifacts 非空且 sha256+size 可独立复算。

CLI-002 重锚依据: CLI-001 唯一命令树 = normalize/mosaic/export（ASTROCS_DESIGN §6.2;
docs/api/CLI_PROTOCOL_V1.md §1 旧 phase1|2|3 run / verify / graph 均为已删除别名 → rc=2）。
旧用例的 verify --json --run-manifest 闭环改为**测试内独立复算** sha256/size（判据不变、
不依赖已删命令）; graph --preset 退役判据保留为负例（rc=2）。

负例矩阵(全非零且不写 complete):
  空 input_lights→2; 缺输入文件→3; 缺 HiPS 输入→3; 缺逐帧 ivar 产品→2;
  无 ivar fixture→2。
正例补充: 含 ivar 的合成 fixture → rc=0 且
manifest.uncertainty_available=true（真实不确定度面可达）。
Phase1 侧同源: light 帧用 --make-noisy（确定性噪声，校准后 σ≈1.8 ADU）⇒ 噪声模型
  有合格 patch（σ>0）⇒ normalize 产出 variance/ivar 子产品 ⇒ normalize→mosaic
  默认逐帧逆方差链可闭合（--make 的常量域帧 σ=0 ⇒ 整帧退化 ⇒ 默认链 fail-closed）。

权重口径（ASTROCS_DESIGN §2.1 + GAP_AUDIT §9.73 裁决 A44「不存在权重模式」）:
  HiPS 里**存**的是**帧级 SNR**（与稀疏控制点上的绝对 SNR）; 权重是阶段二消费 SNR 时
  按覆盖该像素的帧集合**现场算出的派生量**，不是配置键 ⇒ 配置面**不得**出现
  weight_mode / legacy_allow_weight_fallback（CLI 白名单已摘除，出现即 rc=3）。
  生产唯一路径 = 逐帧逆方差; 缺逐帧 ivar 时按 DATA-UNC-001 §30.1 fail-closed
  （禁静默回退等权），故「无 ivar ⇒ rc=2」是本文件的负例判据。
"""
import atexit, hashlib, json, os, re, shutil, signal, subprocess, sys, tempfile, time, unittest

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
# ROOT-008: 唯一产品二进制 build/acsd（旧 build/cli/astrocs 已退役）
EXE = os.environ.get("ASTROCS_CLI_BIN", os.path.join(REPO, "build", "acsd"))


def _pick(*cands):
    for p in cands:
        if os.path.isdir(p):
            return p
    return cands[0]


AIO = _pick(os.path.join(REPO, "lib", "infrastructure", "aio"),
            os.path.join(REPO, "lib", "astro_image_io"))
SHARED = _pick(os.path.join(REPO, "lib", "algorithms", "shared"),
               os.path.join(REPO, "lib", "common"))
HEALPIX_SRC = os.path.join(SHARED, "healpix", "healpix_core.cpp")

# FIX-UTCLI-HYGIENE: 子进程 cwd 统一落 run/（gitignore），见 cli_test_hygiene.py
from cli_test_hygiene import run_cwd  # noqa: E402

SKIP_FITS = r"f77_wrap|drvrgsiftp|drvrsmem|smem|vms|windumpexts|iter_[abc]|" \
            r"cookbook|speed_test|fpack|funpack|fitscopy|listhead|liststruc|" \
            r"imcopy|imarith|tabcompile|sortcol|tabselect"

# 合成 E2E WCS: 64² 帧覆盖 ~32° 天区(0.5°/px), 使 nside=512 的 HiPS 覆盖
# 足够 control 单元(8×8/tile)供 mosaic sampler 取得 >=2 clean frame 观测。
# 绝对天体测量正确性属 Batch 2（WcsTan oracle）; 此处只保证 WCS 自洽可 drizzle。
CD_DEG = 0.5
WCS_EXPLICIT = {"crpix1": 32.5, "crpix2": 32.5, "crval1": 210.0, "crval2": 34.0,
                "cd11": -CD_DEG, "cd12": 0.0, "cd21": 0.0, "cd22": CD_DEG}
DRIZZLE = {"nside": 512, "nested": 1, "pixfrac": 1.0, "precision_mode": 1}


def _cfitsio_objs(tmp):
    objs = []
    cdir = os.path.join(AIO, "third_party", "cfitsio")
    for f in sorted(os.listdir(cdir)):
        if not f.endswith(".c") or re.search(SKIP_FITS, f):
            continue
        o = os.path.join(tmp, f[:-2] + ".o")
        subprocess.run(["gcc", "-O2", "-w", f"-I{cdir}", "-c", os.path.join(cdir, f),
                        "-o", o], check=True, capture_output=True, timeout=300)
        objs.append(o)
    return objs


def _common_incs():
    return [f"-I{os.path.join(REPO, 'lib', 'include')}",
            f"-I{os.path.join(AIO, 'include')}", f"-I{os.path.join(AIO, 'src')}",
            f"-I{os.path.join(AIO, 'third_party', 'cfitsio')}",
            f"-I{SHARED}", f"-I{os.path.dirname(HEALPIX_SRC)}"]


# ── fixture 源清单的权威来源 = 构建图（不手抄文件名）────────────────────────
# 规范依据（AGENTS.md §1.1「先到最高文档确定规范，再动手」）:
#   * 根 CMakeLists.txt:4-5「显式源列表, 禁 GLOB (QA-002)」⇒ 每个 add_library 的
#     显式清单**就是**该目标的构建图闭包，不是「惯例」;
#   * 根 CMakeLists.txt:378-381 `add_library(astrocs_common STATIC ...)` ——
#     crypto/sha256.cpp + healpix/healpix_core.cpp 的唯一权威源清单;
#   * 根 CMakeLists.txt:449-459 `add_library(astrocs_aio STATIC ...)`;
#   * 根 CMakeLists.txt:480-491 `add_library(astrocs_hips STATIC ...)`;
#   * lib/algorithms/drizzle/hips/CMakeLists.txt:44-54 逐字登记 aio_hips_writer.cpp
#     的**最小链接闭包必须含 lib/algorithms/shared/crypto/sha256.cpp** —— 链路是
#     aio_hips_writer.cpp:22 `#include "aio_sparse_punch.h"` →
#     aio_sparse_punch.h:37 `#include "aio_file_io.h"` →
#     aio_file_io.h:240 inline `aio_file::sha256_hex` 调 `astrocs::crypto::Sha256`
#     （唯一实现 TU = crypto/sha256.cpp）。漏它的表现是**链接期**
#     `undefined reference to astrocs::crypto::Sha256::...`（CHK-FIX406-SIGTERM
#     现场 2026-09-22），而链接器一次只报第一条、且不告诉你该补哪个 .cpp。
_ROOT_CMAKE = os.path.join(REPO, "CMakeLists.txt")
_FIXTURE_GRAPH_TARGETS = ("astrocs_common", "astrocs_aio", "astrocs_hips")

# fixture 需要的源 —— 本表只回答「要谁」（**文件名**），不回答「在哪」；
# 路径一律由上面的构建图解析，因此目录搬迁/改名不会让清单静默失效（改错即判红，
# 见 TestAioFixtureLinkClosure::test_01）。
_AIO_SEED = ("aio_hips_writer.cpp", "aio_hips_reader.cpp", "aio_fits.cpp",
             "aio_api.cpp", "aio_log.cpp", "aio_compressor.cpp",
             "healpix_core.cpp", "sha256.cpp")


def _cmake_target_sources(cmake_file, target):
    """解析 CMakeLists.txt 里 add_library/add_executable(<target> ...) 的显式源清单。

    返回已 resolve 的绝对路径（按清单出现顺序）。构建图变了（目标改名/清单为空）
    直接抛错判红 —— 不允许测试静默退回手抄。
    """
    with open(cmake_file, encoding="utf-8") as fh:
        text = fh.read()
    m = re.search(r"add_(?:library|executable)\(\s*" + re.escape(target) + r"\b", text)
    if m is None:
        raise AssertionError(
            "构建图已变：%s 里找不到目标 %s 的显式源清单" % (cmake_file, target))
    open_at = text.index("(", m.start())
    depth = 0
    close_at = -1
    for k in range(open_at, len(text)):
        ch = text[k]
        if ch == "(":
            depth += 1
        elif ch == ")":
            depth -= 1
            if depth == 0:
                close_at = k
                break
    if close_at < 0:
        raise AssertionError("%s: 目标 %s 的源清单括号不闭合" % (cmake_file, target))
    block = "\n".join(ln.split("#")[0]
                      for ln in text[open_at + 1:close_at].splitlines())
    # 仓库路径含空格：直接对整块做路径正则会被空格截断 ⇒ 先把
    # CMake 变量 CMAKE_CURRENT_SOURCE_DIR 换成无空格占位符，拼回绝对路径后再还原。
    var = "@CMAKE_CURRENT_SOURCE_DIR@"
    block = block.replace("$" + "{CMAKE_CURRENT_SOURCE_DIR}", var)
    root = os.path.dirname(cmake_file)
    srcs = []
    for token in re.findall(r"([\w@./-]+\.cpp)", block):
        token = token.replace(var, root)
        srcs.append(os.path.normpath(token if os.path.isabs(token)
                                     else os.path.join(root, token)))
    if not srcs:
        raise AssertionError("%s: 目标 %s 的源清单为空" % (cmake_file, target))
    return srcs


def _fixture_graph_sources():
    """构建图解析出的 fixture 候选源清单（跨目标去重、保序）: [(name, abspath), ...]"""
    ordered, seen = [], set()
    for tgt in _FIXTURE_GRAPH_TARGETS:
        for src in _cmake_target_sources(_ROOT_CMAKE, tgt):
            name = os.path.basename(src)
            if name not in seen:
                seen.add(name)
                ordered.append((name, src))
    return ordered


def _fixture_graph_by_name():
    return dict(_fixture_graph_sources())


def _aio_srcs():
    """phase2 fixture 的 AIO/C++ 源清单 —— 路径从构建图解析（不手抄）。

    _AIO_SEED 里每个名字必须仍在权威清单里，否则判红（构建图已变，需同步本表）。
    """
    by_name = _fixture_graph_by_name()
    missing = [n for n in _AIO_SEED if n not in by_name]
    if missing:
        raise AssertionError(
            "fixture 源清单已不在构建图里（根 CMakeLists.txt 目标 %s 的权威清单已变，"
            "需同步 _AIO_SEED）: %s"
            % ("/".join(_FIXTURE_GRAPH_TARGETS), ", ".join(missing)))
    return [by_name[n] for n in _AIO_SEED]


def jsonl_lines(text):
    return [json.loads(l) for l in text.splitlines() if l.strip()]


def manifest_event(events):
    return [e for e in events if e["kind"] == "artifact" and
            e.get("role") == "run_manifest"][-1]


def sha256_file(path):
    h = hashlib.sha256()
    with open(path, "rb") as fh:
        for chunk in iter(lambda: fh.read(1 << 16), b""):
            h.update(chunk)
    return h.hexdigest()


class TestPhase123Pipeline(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        assert os.path.isfile(EXE), "先构建 CLI（cmake -S . -B build && ninja -C build acsd）"
        cls.tmp = tempfile.mkdtemp(prefix="syn009_")
        objs = _cfitsio_objs(cls.tmp)
        incs = _common_incs()
        cls.p1 = os.path.join(cls.tmp, "p1fx")
        r = subprocess.run(["g++", "-std=c++17", "-O2", "-w", "-DAIO_ENABLE_FITS", *incs,
                            os.path.join(REPO, "eng", "tests", "backend", "phase1_fixture_main.cpp"),
                            os.path.join(AIO, "src", "aio_fits.cpp"),
                            os.path.join(AIO, "src", "aio_api.cpp"),
                            os.path.join(AIO, "src", "aio_log.cpp"),
                            os.path.join(AIO, "src", "aio_compressor.cpp"),
                            *objs, "-lz", "-lzstd", "-llz4", "-o", cls.p1],
                           capture_output=True, text=True, timeout=900)
        assert r.returncode == 0, r.stderr[-800:]
        cls.p2 = os.path.join(cls.tmp, "p2fx")
        r = subprocess.run(["g++", "-std=c++17", "-O2", "-w", "-DAIO_ENABLE_FITS", *incs,
                            os.path.join(REPO, "eng", "tests", "backend", "phase2_fixture_main.cpp"),
                            *_aio_srcs(), *objs, "-lz", "-lzstd", "-llz4", "-o", cls.p2],
                           capture_output=True, text=True, timeout=900)
        assert r.returncode == 0, r.stderr[-800:]
        cls.p1data = os.path.join(cls.tmp, "p1data")
        os.makedirs(cls.p1data)
        # --make-noisy（非 --make）：常量域 light 帧 σ=0 ⇒ 噪声模型整帧退化 ⇒
        # Phase1 产品无 variance/ivar、无帧级 SNR ⇒ A44 后的默认（唯一）逐帧逆方差
        # 权重链按 DATA-UNC-001 §30.1 fail-closed（mosaic rc=2，禁静默等权）。
        # §2.1 要求 HiPS 存帧级 SNR，故端到端正例必须喂非退化噪声面。
        r = subprocess.run([cls.p1, "--make-noisy", cls.p1data], capture_output=True,
                           text=True, timeout=120, cwd=run_cwd())
        assert "FIXTURES_OK" in r.stdout, r.stderr
        # 含 variance/ivar 的 Phase2 fixture (B1-A5 真实不确定度面正例)
        cls.hips = os.path.join(cls.tmp, "hips")
        os.makedirs(cls.hips)
        for m in ("--make", "--make-field", "--make-nan"):
            subprocess.run([cls.p2, m, cls.hips], capture_output=True, text=True, timeout=120,
                           cwd=run_cwd())
        # 无 ivar 的 Phase2 fixture (默认逐帧逆方差负例; §2.1 / §9.73 A44)
        cls.noivar = os.path.join(cls.tmp, "noivar")
        os.makedirs(cls.noivar)
        subprocess.run([cls.p2, "--make-noivar", cls.noivar], capture_output=True,
                       text=True, timeout=120, cwd=run_cwd())
        # 逐帧 normalize 持久化产品目录
        cls.p1a = os.path.join(cls.tmp, "p1_light1")
        os.makedirs(cls.p1a)
        cls.p1b = os.path.join(cls.tmp, "p1_light2")
        os.makedirs(cls.p1b)
        cls.p2out = os.path.join(cls.tmp, "p2out")
        os.makedirs(cls.p2out)

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.tmp, ignore_errors=True)

    # ── config helpers（§6.2 命令面配置形态；与 --template 同源）──
    def _write(self, name, doc):
        p = os.path.join(self.tmp, name)
        with open(p, "w", encoding="utf-8") as fh:
            json.dump(doc, fh)
        return p

    def _p1_cfg(self, out, lights, with_chain=True):
        doc = {"schema_version": "1", "input_lights": list(lights),
               "master_bias": os.path.join(self.p1data, "bias.fits"),
               "master_dark": os.path.join(self.p1data, "dark.fits"),
               "master_flat": os.path.join(self.p1data, "flat.fits"),
               # BIAS-001: 夹具 dark(含 bias) ⇒ 显式声明兼容式（否则默认标准式给 (200-100-150)/1.25=-40）。
               "dark_optimization": True,
               "output_dir": out}
        if with_chain:
            doc["wcs"] = dict(WCS_EXPLICIT)
            doc["drizzle"] = dict(DRIZZLE)
        return self._write("p1_%s.json" % os.path.basename(out), doc)

    def _p2_cfg(self, out, hips, extra=None):
        doc = {"schema_version": "1", "hips_paths": list(hips), "output_dir": out}
        if extra:
            doc.update(extra)
        return self._write("p2_%s.json" % os.path.basename(out), doc)

    def _p3_cfg(self, out, hips_dir):
        return self._write("p3_%s.json" % os.path.basename(out), {
            "schema_version": "1",
            "source": {"hips_dir": hips_dir},
            "output_dir": out,
            "center": {"ra_deg": 210.0, "dec_deg": 34.0},
            "scale_deg_per_px": 0.5, "width_px": 20, "height_px": 20,
            "sampler": "bilinear", "projection": "TAN",
            "coverage_output": "mask",
            # FZ-P3-MODES（FROZEN；docs/contracts/DATA_SEMANTICS.md §31）：
            # phase3 resample 节点要求显式声明 output_mode（缺键即 REJECT）。
            "output_mode": "surface_brightness"})

    def _run(self, session, cfg, timeout=600, extra=None):
        return subprocess.run([EXE, session, "--json", cfg, "--events-jsonl", "-y",
                               *(extra or [])],
                              capture_output=True, text=True, timeout=timeout, cwd=run_cwd())

    @staticmethod
    def _artifact_names(man):
        return {os.path.basename(a["path"]) for a in man["artifacts"]}

    @staticmethod
    def _complete_manifests(d):
        out = []
        for f in os.listdir(d):
            if f.startswith("astrocs_run_") and f.endswith(".json"):
                try:
                    with open(os.path.join(d, f), encoding="utf-8") as fh:
                        m = json.load(fh)
                    if m.get("status") == "complete":
                        out.append(m)
                except Exception:  # noqa: BLE001
                    pass
        return out

    def _assert_manifest_closed(self, r, out, phase, min_art=1):
        """闭环判据: final ok + 唯一 complete manifest + 逐 artifact 独立复算 sha256/size。"""
        events = jsonl_lines(r.stdout)
        self.assertEqual(events[-1]["kind"], "final")
        self.assertEqual(events[-1]["status"], "ok")
        mans = self._complete_manifests(out)
        self.assertEqual(len(mans), 1, "应有且仅有一个 complete manifest")
        man = mans[0]
        self.assertEqual(man["phases"], [phase])
        self.assertGreaterEqual(len(man["artifacts"]), min_art)
        for a in man["artifacts"]:
            self.assertTrue(os.path.isfile(a["path"]), "artifact 必须在盘: " + a["path"])
            self.assertGreater(a["size_bytes"], 0, a["path"])
            # CLI-002: 旧 verify --run-manifest 已删 → 测试内独立复算（判据不变）
            self.assertEqual(sha256_file(a["path"]), a["sha256"],
                             "artifact sha256 与 manifest 不符: " + a["path"])
            self.assertEqual(os.path.getsize(a["path"]), a["size_bytes"], a["path"])
        mf = manifest_event(events)["path"]
        with open(mf, encoding="utf-8") as fh:
            self.assertEqual(json.load(fh)["status"], "complete")
        return man

    # ── normalize: 逐帧持久化 + 校准数值 Oracle ──
    def test_01_normalize_products_persist_hips(self):
        for out, idx in ((self.p1a, 1), (self.p1b, 2)):
            cfg = self._p1_cfg(out, [os.path.join(self.p1data, "light_%d.fits" % idx)])
            r = self._run("normalize", cfg)
            self.assertEqual(r.returncode, 0, r.stderr[-500:])
            events = jsonl_lines(r.stdout)
            self.assertEqual([e["sequence"] for e in events], list(range(len(events))))
            self.assertEqual(events[-1]["kind"], "final")
            self.assertEqual(events[-1]["status"], "ok")
            kinds = [e["kind"] for e in events]
            self.assertIn("resource", kinds)
            self.assertIn("backend", kinds)
            man = self._assert_manifest_closed(r, out, 1, min_art=8)
            names = self._artifact_names(man)
            self.assertIn("calibrated_light_%d.fits" % idx, names)
            # 末端直写标准 HiPS（signal/properties + Moc/metadata）
            for want in ("p1_sources.json", "p1_psf.json", "p1_wcs.json", "p1_flux.json",
                         "p1_snr.json", "p1_final.json"):
                self.assertIn(want, names, "缺产物 " + want)
            # P0-21 §3.4: 每帧一个 HiPS 产品目录 output_dir/<frame_key>/（1 帧配置）。
            self.assertTrue(
                os.path.isfile(os.path.join(out, "light_%d" % idx, "signal", "properties")),
                "normalize 必须为每帧持久化 IVOA HiPS signal/properties")
            with open(os.path.join(out, "p1_products.json"), encoding="utf-8") as fh:
                prods = json.load(fh)
            self.assertEqual(prods["n_products"], 1)
            self.assertEqual(prods["n_frames"], 1)
            self.assertEqual(prods["hips_paths"],
                             [os.path.join(out, "light_%d" % idx)])
            # 数值 Oracle 归 eng/tests/cli/test_phase1_inprocess.py::test_02
            # （校准公式订正面 = calibration/BIAS-001 域；本用例只判跨命令链路:
            #  normalize 产物能否被 mosaic 只读消费、mosaic 产物能否被 export 只读消费）
            self.assertGreater(os.path.getsize(
                os.path.join(out, "calibrated_light_%d.fits" % idx)), 0)

    # ── mosaic: 只读两个 normalize 持久化产品（独立进程），默认逐帧逆方差 ──
    def test_02_mosaic_consumes_normalize_products(self):
        # P0-21 §3.4: normalize 产品 = 逐帧目录；mosaic 直接消费 p1_products.json
        # 的 hips_paths（可串行衔接）。
        # §2.1 / §9.73 裁决 A44: **无 weight_mode 配置键** —— 权重是阶段二消费帧级
        # SNR 时按覆盖该像素的帧集合现场算出的派生量；生产路径恒为逐帧逆方差
        # （normalize 产品已含 variance/ivar）。
        cfg = self._p2_cfg(self.p2out,
                           [os.path.join(self.p1a, "light_1"),
                            os.path.join(self.p1b, "light_2")])
        r = self._run("mosaic", cfg)
        self.assertEqual(r.returncode, 0, r.stderr[-500:])
        # 真链路必须产生重叠控制点（>=2 clean frame/UPM 几何前提）
        with open(os.path.join(self.p2out, "p2_samples.json"), encoding="utf-8") as fh:
            smp = json.load(fh)
        self.assertGreater(smp.get("stats", {}).get("overlap_controls", 0), 0,
                           "mosaic 必须在两个 normalize 产品的重叠区取得控制点")
        man = self._assert_manifest_closed(r, self.p2out, 2, min_art=5)
        # B1-A5 + DATA-UNC-001 §30.1: 默认（唯一）生产路径 = 逐帧逆方差 ⇒
        # 不确定度面必须真实可达（强于旧的「显式等权 ⇒ false」判据）
        self.assertIs(man.get("uncertainty_available"), True)
        self.assertTrue(os.path.isfile(os.path.join(self.p2out, "signal", "properties")),
                        "mosaic 必须持久化 mosaic HiPS")

    # ── export: 只读 mosaic 持久化产品（独立进程） ──
    def test_03_export_consumes_mosaic_product(self):
        out = os.path.join(self.tmp, "p3out")
        os.makedirs(out, exist_ok=True)
        cfg = self._p3_cfg(out, self.p2out)
        r = self._run("export", cfg)
        self.assertEqual(r.returncode, 0, r.stderr[-500:])
        man = self._assert_manifest_closed(r, out, 3, min_art=2)
        self.assertTrue(any(a.get("role") == "phase3_output" for a in man["artifacts"]))
        self.assertTrue(os.path.isfile(os.path.join(out, "output_phase3.fits")))

    # ── 负例矩阵 ──
    def test_04_negative_matrix(self):
        D = self.tmp
        # a) normalize 空 input_lights → 2（预检结构门, 不可 -force 越）, 不写 complete
        d = os.path.join(D, "neg_empty")
        os.makedirs(d, exist_ok=True)
        r = self._run("normalize", self._p1_cfg(d, []))
        self.assertEqual(r.returncode, 2, r.stderr[-300:])
        self.assertIn("input_lights", r.stderr)
        self.assertEqual(self._complete_manifests(d), [], "空输入不得写 complete")
        # b) normalize 缺输入文件 → 3
        d = os.path.join(D, "neg_missing")
        os.makedirs(d, exist_ok=True)
        r = self._run("normalize", self._p1_cfg(d, [os.path.join(D, "nope.fits")]))
        self.assertEqual(r.returncode, 3, r.stderr[-300:])
        # c) mosaic 缺 HiPS 输入 → 3 (B1-A7 映射; SMOKE-001 D11)
        d = os.path.join(D, "neg_nohips")
        os.makedirs(d, exist_ok=True)
        r = self._run("mosaic", self._p2_cfg(d, ["/nonexistent/does_not_exist.hips"]))
        self.assertEqual(r.returncode, 3, r.stderr[-400:])
        # 红必须是「缺输入」，不得是配置键被 CLI 白名单摘除（§9.73 A44）
        self.assertNotIn("unknown key", r.stderr)
        # d) mosaic 默认逐帧逆方差对无 ivar 的产品 → 2, 不写 complete
        #    定案 2（逐像素方差接入）后 normalize 产品**已含** variance/ivar ⇒
        #    本负例改为消费「剥掉 variance/ivar 的副本」：判据与意图（缺 ivar ⇒
        #    fail-closed，不写 complete）逐字不变，只是不再依赖「生产不产方差」这一
        #    已被修复的缺陷。
        d = os.path.join(D, "neg_ivar")
        os.makedirs(d, exist_ok=True)
        novar = []
        for src in (os.path.join(self.p1a, "light_1"), os.path.join(self.p1b, "light_2")):
            dst = os.path.join(D, "neg_ivar_frames", os.path.basename(src) + "_noivar")
            if not os.path.isdir(dst):
                shutil.copytree(src, dst)
                for prod in ("variance", "ivar"):
                    shutil.rmtree(os.path.join(dst, prod), ignore_errors=True)
            novar.append(dst)
        r = self._run("mosaic", self._p2_cfg(d, novar))
        self.assertEqual(r.returncode, 2, r.stderr[-400:])
        self.assertIn("ivar", r.stderr)
        self.assertEqual(self._complete_manifests(d), [], "缺 ivar 不得写 complete")
        # e) 无 ivar fixture + 默认逐帧逆方差 → 2
        d = os.path.join(D, "neg_fxnoivar")
        os.makedirs(d, exist_ok=True)
        r = self._run("mosaic", self._p2_cfg(d, [os.path.join(self.noivar, "F1.hips"),
                                                 os.path.join(self.noivar, "F2.hips")]))
        self.assertEqual(r.returncode, 2, r.stderr[-400:])
        # f) 含 ivar fixture + 默认逐帧逆方差 → 0 且 uncertainty_available=true
        d = os.path.join(D, "pos_ivar")
        os.makedirs(d, exist_ok=True)
        r = self._run("mosaic", self._p2_cfg(d, [os.path.join(self.hips, "F1.hips"),
                                                 os.path.join(self.hips, "F2.hips")]))
        self.assertEqual(r.returncode, 0, r.stderr[-400:])
        man = self._assert_manifest_closed(r, d, 2, min_art=5)
        self.assertIs(man.get("uncertainty_available"), True)

    # ── mosaic/export 各自独立进程两次运行 ──
    def test_05_mosaic_export_isolated_commands(self):
        o2 = os.path.join(self.tmp, "iso2")
        os.makedirs(o2)
        o3 = os.path.join(self.tmp, "iso3")
        os.makedirs(o3)
        cfg2 = self._p2_cfg(o2, [os.path.join(self.hips, "F1.hips"),
                                 os.path.join(self.hips, "F2.hips")])
        cfg3 = self._p3_cfg(o3, os.path.join(self.hips, "FIELD.hips"))
        r2 = self._run("mosaic", cfg2)
        self.assertEqual(r2.returncode, 0, r2.stderr[-400:])
        r3 = self._run("export", cfg3)
        self.assertEqual(r3.returncode, 0, r3.stderr[-400:])
        ev2 = jsonl_lines(r2.stdout)
        ev3 = jsonl_lines(r3.stdout)
        self.assertEqual(ev2[-1]["status"], "ok")
        self.assertEqual(ev3[-1]["status"], "ok")
        rid2 = {e["run_id"] for e in ev2}
        rid3 = {e["run_id"] for e in ev3}
        self.assertEqual(len(rid2), 1)
        self.assertEqual(len(rid3), 1)
        self.assertNotEqual(rid2, rid3, "两次运行必须不同 run_id(进程隔离)")
        with open(manifest_event(ev2)["path"], encoding="utf-8") as fh:
            man2 = json.load(fh)
        with open(manifest_event(ev3)["path"], encoding="utf-8") as fh:
            man3 = json.load(fh)
        self.assertEqual(man2["status"], "complete")
        self.assertEqual(man3["status"], "complete")
        self.assertEqual(man2["phases"], [2])
        self.assertEqual(man3["phases"], [3])

    # ── 中断 → exit 9 + incomplete manifest ──
    def test_06_cancel_interrupt(self):
        """运行期取消 → rc=9 + final status=cancelled + 不留 complete manifest。

        CLI-002: 新命令树有两个同语义测试钩子（ASTROCS_TEST_SLEEP_MS）——
        CLI 入口等待（lib/infrastructure/cli/subcommand.h:156, 只回 rc=9 不产事件）
        与会话内等待（lib/infrastructure/cli/commands.cpp cmd_session1_run, 产
        incomplete manifest + final cancelled）。入口窗先耗尽，故 SIGINT 必须落在
        会话窗（入口窗之后）才测到**运行期**取消面（判据与原 phase3 用例一致）。
        """
        out = os.path.join(self.tmp, "outcancel")
        os.makedirs(out)
        cfg = self._p1_cfg(out, [os.path.join(self.p1data, "light_1.fits")])
        env = dict(os.environ, ASTROCS_TEST_SLEEP_MS="6000")
        p = subprocess.Popen([EXE, "normalize", "--json", cfg, "--events-jsonl", "-y"],
                             stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, env=env,
                             cwd=run_cwd())
        time.sleep(8.0)          # 越过入口等待窗(0~6s), 落在会话等待窗(6~12s)
        p.send_signal(signal.SIGINT)
        p.wait(timeout=60)
        self.assertEqual(p.returncode, 9, "SIGINT → exit 9")
        ev = [json.loads(l) for l in p.stdout.read().decode().splitlines() if l.strip()]
        self.assertTrue(ev, "运行期取消必须发事件流（final=cancelled）")
        self.assertEqual(ev[-1]["kind"], "final")
        self.assertEqual(ev[-1]["status"], "cancelled")
        self.assertEqual(self._complete_manifests(out), [],
                         "取消不得留下看似完整的 run manifest")

    # ── resume/hash mismatch → exit 8 + incomplete ──
    def test_07_resume_hash_mismatch(self):
        out = os.path.join(self.tmp, "outhm")
        os.makedirs(out)
        cfg = self._p3_cfg(out, os.path.join(self.hips, "FIELD.hips"))
        r1 = self._run("export", cfg)
        self.assertEqual(r1.returncode, 0, r1.stderr[-400:])
        with open(manifest_event(jsonl_lines(r1.stdout))["path"], encoding="utf-8") as fh:
            man = json.load(fh)
        art = [a for a in man["artifacts"] if a.get("role") == "phase3_output"][0]
        with open(art["path"], "ab") as fh:
            fh.write(b"TAMPER")
        r2 = self._run("export", cfg)
        self.assertEqual(r2.returncode, 8, r2.stdout[-300:] + r2.stderr[-300:])
        ev = [e for e in jsonl_lines(r2.stdout) if e["kind"] == "final"]
        self.assertEqual(ev[-1]["status"], "resume_hash_mismatch")

    # ── RT-009: 运行图产物 ──
    def test_08_run_graphs(self):
        out = os.path.join(self.tmp, "outg")
        os.makedirs(out)
        cfg = self._p3_cfg(out, os.path.join(self.hips, "FIELD.hips"))
        r = self._run("export", cfg)
        self.assertEqual(r.returncode, 0, r.stderr[-400:])
        gdir = os.path.join(out, "graph")
        for name in ("static_graph.json", "observed_trace.json", "graph_sidecar.json",
                     "static_graph.dot", "observed_graph.dot", "static_graph.svg",
                     "observed_graph.svg", "l0_graph.json", "l0_graph.dot"):
            self.assertTrue(os.path.isfile(os.path.join(gdir, name)), name)
        with open(os.path.join(gdir, "observed_trace.json"), encoding="utf-8") as fh:
            tr = json.load(fh)
        self.assertEqual(tr["schema"], "astrocs.observed-trace/v1")
        nodes = {n["node_id"]: n for n in tr["nodes"]}
        self.assertIn("properties", nodes)
        self.assertEqual(nodes["properties"]["status"], "COMPLETED")
        with open(os.path.join(gdir, "observed_trace.json"), encoding="utf-8") as fh:
            raw = fh.read()
        self.assertNotIn(REPO, raw)
        self.assertNotIn("/home/", raw)
        with open(os.path.join(gdir, "graph_sidecar.json"), encoding="utf-8") as fh:
            side = json.load(fh)
        self.assertEqual(side["schema"], "astrocs.graph-sidecar/v1")
        mods = os.path.join(self.tmp, "mods.json")
        with open(mods, "w", encoding="utf-8") as fh:
            json.dump({"astrocs.phase3.resample": {"module_id": "astrocs.phase3.resample",
                                                   "module_version": "1.x"}}, fh)
        c = subprocess.run([sys.executable, os.path.join(REPO, "eng", "tools", "quality",
                            "check_pipeline_graph.py"),
                            "--ir", os.path.join(gdir, "static_graph.json"),
                            "--module-index", mods,
                            "--trace", os.path.join(gdir, "observed_trace.json")],
                           capture_output=True, text=True, timeout=120, cwd=run_cwd())
        self.assertEqual(c.returncode, 0, c.stderr[-400:])
        self.assertIn("PIPELINE_GRAPH_PASS", c.stdout)
        # 退役面负例: graph 用户命令已删（CLI_PROTOCOL_V1 §1）→ rc=2 unknown command
        g = subprocess.run([EXE, "graph", "--preset", "1,2,3", "--config", cfg,
                            "--output", os.path.join(self.tmp, "gstatic")],
                           capture_output=True, text=True, timeout=120, cwd=run_cwd())
        self.assertEqual(g.returncode, 2, g.stderr[-200:])
        self.assertIn("unknown command", g.stderr)

# ── nm -C 链接闭包判据（对真实 .o 求「未定义 astrocs:: 符号 − 已定义符号」）────
# 判据依赖真实编译（nm 需要目标文件），不是源码文本猜测 —— 本缺陷的典型形态
# aio_file::sha256_hex 是 aio_file_io.h:240 的 **inline** 函数，纯文本扫不出
# 「谁在调 crypto::Sha256」，只有编成 .o 才现形。

_AIO_CLOSURE = {}


def _compile_unit(src, out_dir, *, defs=("-DAIO_ENABLE_FITS",)):
    """编译单个 TU → .o（与 fixture 同款 flags: -std=c++17 -O2 -w + 同一 include 面）。

    -O2 必须与 fixture 一致：inline 函数在 -O2 下被内联展开，才会把
    astrocs::crypto::Sha256::* 变成**未定义外部符号**；换成 -O0 判据就与真实
    链接面不符（inline 体不展开，符号面不同）。
    """
    obj = os.path.join(out_dir, os.path.basename(src) + ".o")
    cmd = ["g++", "-std=c++17", "-O2", "-w", *defs, *_common_incs(), "-c", src, "-o", obj]
    r = subprocess.run(cmd, capture_output=True, text=True, timeout=900)
    if r.returncode != 0:
        raise RuntimeError("compile %s failed:\n%s"
                           % (os.path.basename(src), r.stderr[-2000:]))
    return obj


def _nm_symbols(objs, kind):
    """nm -C 提取目标文件集合的符号名（kind='defined' | 'undefined'）。

    注意 nm -C 的**名字里含空格**（模板/参数表），故按列切分时用 maxsplit，
    不能对整行 split() —— 否则会截出半个符号名，差集判据静默失效。
    """
    r = subprocess.run(["nm", "-C", "--%s-only" % kind, *[str(o) for o in objs]],
                       capture_output=True, text=True, timeout=300)
    if r.returncode != 0:
        raise RuntimeError("nm --%s-only failed:\n%s" % (kind, r.stderr[-2000:]))
    out = set()
    for line in r.stdout.splitlines():
        line = line.strip()
        if not line:
            continue
        if kind == "defined":            # "<addr> <type> <name...>"
            parts = line.split(None, 2)
            name = parts[2] if len(parts) == 3 else ""
        else:                            # "<blank> U <name...>"
            parts = line.split(None, 1)
            name = parts[1] if len(parts) == 2 else ""
        if name:
            out.add(name)
    return out


def _symbol_leaf(sym):
    """取符号名的叶子标识符（函数名 / 类名），供源码定义正则兜底用。"""
    s = sym
    for pre in ("vtable for ", "VTT for ", "typeinfo for ", "typeinfo name for ",
                "construction vtable for "):
        if s.startswith(pre):
            s = s[len(pre):]
            break
    s = s.split("(")[0]                  # 去参数表
    s = s.split("<")[0]                  # 去模板实参
    return s.rsplit("::", 1)[-1].strip()


_TEXT_CACHE = {}


def _read_text(path):
    if path not in _TEXT_CACHE:
        with open(path, encoding="utf-8", errors="replace") as fh:
            _TEXT_CACHE[path] = fh.read()
    return _TEXT_CACHE[path]


def _source_defines_symbol(text, sym):
    """源码文本里是否存在 sym 的**定义**（而非调用/声明）——候选无法独立编译时的兜底。"""
    leaf = _symbol_leaf(sym)
    if not leaf:
        return False
    for m in re.finditer(r"(?m)^[^\n;#{}]*\b" + re.escape(leaf) + r"\s*\(", text):
        head = m.group(0)[:m.group(0).rindex(leaf)]
        if "=" in head:
            continue                     # 赋值/初始化里的调用，不是定义
        if re.search(r"\b(return|throw|case|sizeof|delete|new|if|while|for|switch|else)\b",
                     head):
            continue
        return True
    return False


def aio_fixture_closure_build():
    """编译 fixture 的 AIO 源 + fixture 入口 → .o，并求链接闭包差集（进程内缓存）。

    返回 dict: {tmp, objs, main_obj, undefined, defined, missing, error}
    编译/失败不抛异常，把诊断写进 error，由测试判红。
    """
    if _AIO_CLOSURE.get("done"):
        return _AIO_CLOSURE
    tmp = tempfile.mkdtemp(prefix="aio_closure_")
    atexit.register(shutil.rmtree, tmp, ignore_errors=True)
    _AIO_CLOSURE.update({"done": True, "tmp": tmp, "objs": [], "main_obj": None,
                         "undefined": set(), "defined": set(), "missing": [],
                         "error": None})
    try:
        objs = [_compile_unit(s, tmp) for s in _aio_srcs()]
        main_obj = _compile_unit(
            os.path.join(REPO, "eng", "tests", "backend", "phase2_fixture_main.cpp"), tmp)
        both = objs + [main_obj]
        undefined = {s for s in _nm_symbols(both, "undefined") if "astrocs::" in s}
        defined = _nm_symbols(both, "defined")
        _AIO_CLOSURE.update({"objs": objs, "main_obj": main_obj,
                             "undefined": undefined, "defined": defined,
                             "missing": sorted(undefined - defined)})
    except Exception as exc:             # noqa: BLE001  诊断自身失败不得掩盖原始错误
        _AIO_CLOSURE["error"] = repr(exc)
    return _AIO_CLOSURE


def _resolve_missing_symbols(missing, tmp, compiled):
    """把未解析符号解析回构建图里的 .cpp —— 即「该补哪个文件」。

    精确优先：对**源码文本里提到该符号叶子名**的候选 TU（构建图清单里尚未编译的）
    编成 .o，用 nm 精确匹配符号定义；候选无法独立编译（需 CMake 生成头 / PRIVATE
    include 面）时退回源码定义正则。
    """
    pool = [p for _n, p in _fixture_graph_sources()]
    compiled = {os.path.normpath(str(c)) for c in compiled}
    leaves = {_symbol_leaf(s) for s in missing}
    exact, fallback = {}, []
    for src in pool:
        if src in compiled:
            continue                     # 已在清单里 ⇒ 不可能定义未解析符号
        text = _read_text(src)
        if not any(leaf and leaf in text for leaf in leaves):
            continue                     # 便宜预筛：文本里根本没提到就不编
        try:
            obj = _compile_unit(src, tmp)
        except RuntimeError:
            fallback.append(src)
            continue
        exact[src] = _nm_symbols([obj], "defined")
    report = []
    for sym in sorted(missing):
        hits = [os.path.relpath(s, REPO) for s, defs in exact.items() if sym in defs]
        if not hits:
            hits = [os.path.relpath(s, REPO) for s in fallback
                    if _source_defines_symbol(_read_text(s), sym)]
        report.append((sym, sorted(hits)))
    return report


def aio_fixture_closure_diagnostics():
    """返回 (missing, detail) —— 供本文件与 CHK-FIX406-SIGTERM 的判据共用。"""
    b = aio_fixture_closure_build()
    if b["error"]:
        return ["<compile-error>"], (
            "fixture 源清单未编译出目标文件，无法做闭包判定：\n" + b["error"])
    if not b["missing"]:
        return [], ""
    report = _resolve_missing_symbols(b["missing"], b["tmp"], b["objs"] + [b["main_obj"]])
    detail = "\n".join(
        "  %s\n      ← 定义在 %s" % (sym, "、".join(hits) if hits
                                    else "（构建图清单内无定义者，可能来自其他库）")
        for sym, hits in report)
    return b["missing"], detail


@unittest.skipUnless(shutil.which("g++") and shutil.which("nm"), "需要 g++ 与 nm")
class TestAioFixtureLinkClosure(unittest.TestCase):
    """机器判据：fixture 源清单必须由构建图推出且覆盖链接闭包。

    防的缺陷类（CHK-FIX406-SIGTERM 现场 2026-09-22）：aio_hips_writer.cpp 因
    aio_sparse_punch.h → aio_file_io.h 的 inline sha256_hex 引入
    astrocs::crypto::Sha256::* 外部符号后，手抄源清单要到**链接期**才炸，而链接器
    只报「undefined reference to <符号>」——不告诉你该补哪个 .cpp，一次只报第一条。
    本类用两条判据把这一类缺陷变成可判、可点名的红：

      ① test_01_seed_sources_come_from_build_graph：_AIO_SEED 每个名字必须仍在
         根 CMakeLists.txt 的 astrocs_common/astrocs_aio/astrocs_hips 权威清单里
         （防改名/搬目录后清单静默失效）；
      ② test_02_link_closure_complete：nm -C 对真实 .o 求
         「未定义 astrocs:: 符号 − 已定义符号」差集，非空即判红，并把每个未解析
         符号解析回构建图里定义它的 .cpp（直接点名该补谁）。
    """

    def test_01_seed_sources_come_from_build_graph(self):
        graph = _fixture_graph_sources()
        self.assertTrue(graph, "构建图里没解析到任何 fixture 候选源（目标改名？）")
        by_name = dict(graph)
        missing = [n for n in _AIO_SEED if n not in by_name]
        self.assertEqual(missing, [],
                         "fixture 种子源已不在构建图权威清单里（需同步 _AIO_SEED）: "
                         + ", ".join(missing))
        for src in _aio_srcs():
            self.assertTrue(os.path.isfile(src), "构建图解析出的源不存在: " + src)

    def test_02_link_closure_complete(self):
        missing, detail = aio_fixture_closure_diagnostics()
        if not missing:
            return
        self.fail("fixture 源清单未覆盖链接闭包（未定义 astrocs:: 符号差集非空）：\n"
                  + detail + "\n  修法：把上面点名的 .cpp 加入 _AIO_SEED。")


if __name__ == "__main__":
    unittest.main(verbosity=2)
