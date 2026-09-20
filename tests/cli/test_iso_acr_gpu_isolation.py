#!/usr/bin/env python3
"""ISO-001 测试: 静态+运行证明 CLI/Phase/manifest 不引用 ACR/GPU/Mixed; 发行包扫描。
验收: production route 0 触达(纯 CPU, 不触 ACR/GPU); 配置请求 ACR 后端明确拒绝(exit 3, 非静默
fallback); 发行包不含 ACR/GPU/CUDA 标识。仅 Linux amd64。

CLI-002 重锚(ROOT-008 + CLI-001 命令树): CLI 源在 lib/infrastructure/cli/, 唯一产品二进制
build/astrocs; 旧 'phase3 run --config' 已删除 → 运行面改用 export 会话命令(§6.2)。判据语义
不变(拒绝面/清单面/发行面), 只换载体。源扫描用例不依赖二进制, 不再被 EXE 门槛整体跳过。
"""
import json, os, re, shutil, subprocess, tempfile, unittest

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
# ROOT-008: CLI 命令层源在 lib/infrastructure/cli/（旧 cli/ 已退役）
CLI = os.path.join(REPO, "lib", "infrastructure", "cli")
# DISPATCH 附录 H（构建隔离）: 被测构建树 = 被测二进制所在目录; ASTROCS_CLI_BIN 覆盖。
EXE = os.environ.get("ASTROCS_CLI_BIN", os.path.join(REPO, "build", "astrocs"))
BUILD = os.path.dirname(os.path.abspath(EXE))
def _pick(*cands):
    for p in cands:
        if os.path.isdir(p):
            return p
    return cands[0]


# ROOT-008: AIO 迁 lib/infrastructure/aio/（旧 lib/astro_image_io 已退役）
AIO = _pick(os.path.join(REPO, "lib", "infrastructure", "aio"),
            os.path.join(REPO, "lib", "astro_image_io"))
SHARED = _pick(os.path.join(REPO, "lib", "algorithms", "shared"),
               os.path.join(REPO, "lib", "common"))
HEALPIX_SRC = os.path.join(SHARED, "healpix", "healpix_core.cpp")

# FIX-UTCLI-HYGIENE: 子进程 cwd 统一落 run/（gitignore），见 cli_test_hygiene.py
from tests.cli.cli_test_hygiene import run_cwd  # noqa: E402

# ACR/GPU/Mixed 关联标识(禁词); "mixed" 作为资源类别在 gate/events 中合法, 但不得作为生产后端选路。
ACR_GPU_TERMS = re.compile(
    r'\b(acr|cuda|gpu_route|cuda_bridge|device_executor|kernel_registry|mosaic_reject_cuda|'
    r'kOpMosaicReject|register_phase2_acr_kernels|dynamic_plugin|CpuExecutor)\b',
    re.IGNORECASE)

# 生产源码: CLI + 三个 phase session + 事件/manifest 头 + 生产 kernel(upm)。不含 lib/infrastructure/acr/(未接入引擎);
# 不含 stage2_common.cpp(其为 ACR 边界"拒绝层"/legacy tool parser; 本身校验 acr_route 只允许 auto/cpu,
# 属 ACR 隔离防线, 不属生产选路)。
PRODUCTION_SOURCES = [
    "lib/infrastructure/cli/main.cpp",
    "lib/infrastructure/cli/jsonl.h",
    "lib/phase1_session/p1_session.cpp",
    "lib/phase2_session/p2_session.cpp",
    "lib/phase3_session/p3_session.cpp",
    "lib/algorithms/coverage/src/upm.cpp",
]

try:
    from tests.cli.test_phase3_inprocess import cfitsio_objs as cfitsio_objs
except Exception:
    def cfitsio_objs(_tmp):
        return []


class TestIsoAcrGpuIsolation(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.exe_ok = os.path.isfile(EXE)
        cls.tmp = tempfile.mkdtemp(prefix="iso001_")
        cls.hips = None
        # phase2 合成 fixture(为 export/manifest 运行测试提供带 variance 的 HiPS 产品)
        incs = [f"-I{os.path.join(REPO, 'include')}",
                f"-I{os.path.join(AIO, 'include')}", f"-I{os.path.join(AIO, 'src')}",
                f"-I{os.path.join(AIO, 'third_party', 'cfitsio')}",
                f"-I{SHARED}",
                f"-I{os.path.dirname(HEALPIX_SRC)}"]
        srcs = [os.path.join(REPO, "tests", "backend", "phase2_fixture_main.cpp"),
                os.path.join(AIO, "src", "hips", "aio_hips_writer.cpp"),
                os.path.join(AIO, "src", "hips", "aio_hips_reader.cpp"),
                os.path.join(AIO, "src", "aio_fits.cpp"),
                os.path.join(AIO, "src", "aio_api.cpp"),
                os.path.join(AIO, "src", "aio_log.cpp"),
                os.path.join(AIO, "src", "aio_compressor.cpp"),
                HEALPIX_SRC]
        fixture = os.path.join(cls.tmp, "fixture")
        if shutil.which("g++"):
            r = subprocess.run(["g++", "-std=c++17", "-O2", "-w", "-DAIO_ENABLE_FITS", *incs,
                                *srcs, *cfitsio_objs(cls.tmp), "-lz", "-lzstd", "-llz4",
                                "-o", fixture], capture_output=True, text=True, timeout=600)
            if r.returncode == 0:
                data = os.path.join(cls.tmp, "data")
                os.makedirs(data)
                r2 = subprocess.run([fixture, "--make-field", data], capture_output=True,
                                    text=True, timeout=300, cwd=run_cwd())
                if "HIPS_FIXTURES_OK" in r2.stdout:
                    cls.hips = os.path.join(data, "FIELD.hips")

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.tmp, ignore_errors=True)

    def _has_acr_term(self, text):
        return bool(ACR_GPU_TERMS.search(text))

    def test_01_production_sources_no_acr_gpu(self):
        """生产源码(CLI/phase session/upm)不引用 ACR/GPU/CUDA/Mixed 后端标识。"""
        violations = []
        for rel in PRODUCTION_SOURCES:
            p = os.path.join(REPO, rel)
            if not os.path.isfile(p):
                continue
            with open(p, encoding="utf-8", errors="replace") as f:
                for i, line in enumerate(f, 1):
                    if self._has_acr_term(line):
                        # mixed 作为资源类别门禁注释/func 允许; 只报后端选路禁词
                        violations.append(f"{rel}:{i}: {line.strip()[:80]}")
        # 只检查真正指向 ACR/GPU 后端运行的符号, 排除纯注释说明
        self.assertEqual(violations, [], "生产源码出现 ACR/GPU 引用:\n" + "\n".join(violations))

    def test_02_production_has_no_acr_register_call(self):
        """生产路径不调用 register_phase2_acr_kernels(仅 tests/tools 调)。"""
        prod_cpp = ["lib/infrastructure/cli/main.cpp", "lib/phase1_session/p1_session.cpp",
                    "lib/phase2_session/p2_session.cpp", "lib/phase3_session/p3_session.cpp",
                    "lib/algorithms/coverage/src/upm.cpp"]
        for rel in prod_cpp:
            p = os.path.join(REPO, rel)
            if os.path.isfile(p):
                with open(p, encoding="utf-8", errors="replace") as fh:
                    content = fh.read()
                self.assertNotIn("register_phase2_acr_kernels", content,
                                 f"生产源码 {rel} 调用 ACR 注册")

    def test_03_config_requests_acr_gpu_rejected(self):
        """config 顶层出现 backend/acr_route/gpu_route/mixed_backend → 明确拒绝(exit 3, 非静默)。"""
        if not self.exe_ok:
            self.skipTest("CLI 二进制缺失(先构建 build/astrocs)")
        out = os.path.join(self.tmp, "o3")
        os.makedirs(out, exist_ok=True)
        base = {"schema_version": "1",
                "source": {"hips_dir": self.hips or "/nonexistent"},
                "output_dir": out,
                "center": {"ra_deg": 210.0, "dec_deg": 34.0},
                "width_px": 40, "height_px": 30, "scale_deg_per_px": 0.1}
        for bad in ("backend", "acr_route", "gpu_route", "mixed_backend"):
            cfg = dict(base)
            cfg[bad] = "acr" if bad in ("backend", "mixed_backend") else "cuda"
            cp = os.path.join(self.tmp, f"bad_{bad}.json")
            with open(cp, "w", encoding="utf-8") as fh:
                json.dump(cfg, fh)
            r = subprocess.run([EXE, "export", "--json", cp, "-y"],
                               capture_output=True, text=True, timeout=120, cwd=run_cwd())
            self.assertNotEqual(r.returncode, 0, f"{bad} 应被拒绝")
            self.assertIn("unknown key", r.stderr, f"{bad} 拒绝信息应为明确错误而非静默 fallback")
            self.assertEqual(r.returncode, 3, f"{bad} 应为 exit 3(INPUT)")

    def test_04_release_package_scan_no_acr_gpu(self):
        """发行包工具不得引入 ACR/GPU/CUDA 后端; ACR 目录必须被显式排除。"""
        pure = ["tools/make_capsule.py", "tools/gen_backends_manifest.py"]
        for rel in pure:
            p = os.path.join(REPO, rel)
            if not os.path.isfile(p):
                continue
            with open(p, encoding="utf-8", errors="replace") as fh:
                content = fh.read()
            hits = [l for l in content.splitlines()
                    if re.search(r'\b(acr|cuda|gpu)\b', l, re.IGNORECASE)]
            self.assertEqual(hits, [], f"{rel} 含 ACR/GPU/CUDA 引用: {hits[:3]}")
        pkg = os.path.join(REPO, "tools", "assemble_v17_review_pkg.py")
        if os.path.isfile(pkg):
            with open(pkg, encoding="utf-8", errors="replace") as fh:
                content = fh.read()
            self.assertIn('"lib/acr"', content,
                          "发行包脚本必须显式排除 lib/acr（ACR dormant, 不入发行包）")
            self.assertEqual([l for l in content.splitlines()
                              if re.search(r'\b(cuda|gpu)\b', l, re.IGNORECASE)], [],
                             "发行包脚本出现 CUDA/GPU 引用（ACR/GPU 路线 dormant）")

    def test_05_backends_manifest_only_pure_cpu(self):
        """doctor --json 的后端面仅纯 CPU 变体(无 acr/gpu/cuda)。"""
        if not self.exe_ok:
            self.skipTest("CLI 二进制缺失(先构建 build/astrocs)")
        r = subprocess.run([EXE, "doctor", "--json"], capture_output=True, text=True,
                           timeout=120, cwd=run_cwd())
        self.assertEqual(r.returncode, 0, r.stderr[-300:])
        doc = json.loads(r.stdout)
        self.assertEqual(doc.get("verdict"), "PASS", doc)
        checks = doc.get("checks", [])
        pre = [c for c in checks if str(c.get("name", "")).startswith("backend_preflight:")]
        for c in pre:
            bid = str(c["name"]).split(":", 1)[1].lower()
            self.assertNotIn(bid, {"acr", "gpu", "cuda"}, f"backend_id '{bid}' 非纯 CPU")
            self.assertNotIn("cuda", bid)
            self.assertIn(c.get("status"), ("pass", "skipped"), c)
        if not pre:
            # 无随包 DSO: doctor 必须显式声明内建 CPU 基线（不得静默无后端面）
            bm = [c for c in checks if c.get("name") == "backends_manifest"]
            self.assertTrue(bm, f"doctor 缺后端面检查: {checks}")
            self.assertEqual(bm[0]["status"], "pass", bm[0])
            self.assertNotIn("acr", str(bm[0].get("detail", "")).lower())

    def test_06_export_manifest_no_acr_gpu(self):
        """export 生产运行 manifest 不含 acr/gpu/route/dispatcher/mixed 选路字段。"""
        if not self.exe_ok:
            self.skipTest("CLI 二进制缺失(先构建 build/astrocs)")
        self.assertTrue(self.hips, "无合成 fixture（setUpClass 未产出 FIELD.hips）")
        out = os.path.join(self.tmp, "o6")
        os.makedirs(out, exist_ok=True)
        cfg = os.path.join(self.tmp, "r6.json")
        with open(cfg, "w", encoding="utf-8") as fh:
            json.dump({"schema_version": "1",
                       "source": {"hips_dir": self.hips},
                       "output_dir": out,
                       "center": {"ra_deg": 210.0, "dec_deg": 34.0},
                       "scale_deg_per_px": 0.1, "width_px": 40, "height_px": 30,
                       "projection": "TAN", "sampler": "nearest",
                       # FZ-P3-MODES：phase3 resample 节点要求显式声明 output_mode（缺键即 REJECT）
                       "coverage_output": "mask", "output_mode": "surface_brightness"}, fh)
        r = subprocess.run([EXE, "export", "--json", cfg, "-y"],
                           capture_output=True, text=True, timeout=300, cwd=run_cwd())
        self.assertEqual(r.returncode, 0, r.stderr[-300:])
        cands = [os.path.join(out, f) for f in os.listdir(out)
                 if f.startswith("astrocs_run_") and f.endswith(".json")]
        self.assertTrue(cands, f"缺 run manifest (out={os.listdir(out)})")
        with open(cands[0], encoding="utf-8") as fh:
            m = json.load(fh)
        self.assertEqual(m.get("status"), "complete", m.get("status"))
        bad = [k for k in m if self._has_acr_term(k) or
               any(t in k.lower() for t in ("acr", "gpu", "cuda", "mixed", "dispatcher", "route"))]
        self.assertEqual(bad, [], f"manifest 出现 ACR/GPU/Mixed 选路字段: {bad}")


if __name__ == "__main__":
    unittest.main(verbosity=2)
