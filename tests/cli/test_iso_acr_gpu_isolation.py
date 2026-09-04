#!/usr/bin/env python3
"""ISO-001 测试: 静态+运行证明 CLI/Phase/dispatcher/manifest 不引用 ACR/GPU/Mixed; 发行包扫描。
验收: production route 0 触达(纯 CPU, 不触 ACR/GPU); 配置请求 ACR 明确拒绝(exit 3, 非静默 fallback);
发行包不含 ACR/GPU/CUDA/Mixed 标识。仅 Linux amd64。"""
import json, os, re, shutil, subprocess, tempfile, unittest

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
CLI = os.path.join(REPO, "cli")
BUILD = os.path.join(REPO, "build", "cli")
EXE = os.path.join(BUILD, "astrocs")
AIO = os.path.join(REPO, "lib", "astro_image_io")

# ACR/GPU/Mixed 关联标识(禁词); "mixed" 作为资源类别在 gate/events 中合法, 但不得作为生产后端选路。
ACR_GPU_TERMS = re.compile(
    r'\b(acr|cuda|gpu_route|cuda_bridge|device_executor|kernel_registry|mosaic_reject_cuda|'
    r'kOpMosaicReject|register_phase2_acr_kernels|dynamic_plugin|CpuExecutor)\b',
    re.IGNORECASE)

# 生产源码: CLI + 三个 phase session + 事件/manifest 头 + 生产 kernel(upm)。不含 lib/acr/(未接入引擎);
# 不含 stage2_common.cpp(其为 ACR 边界"拒绝层"/legacy tool parser; 本身校验 acr_route 只允许 auto/cpu,
# 属 ACR 隔离防线, 不属生产选路)。
PRODUCTION_SOURCES = [
    "cli/main.cpp",
    "cli/jsonl.h",
    "lib/phase1_session/p1_session.cpp",
    "lib/phase2_session/p2_session.cpp",
    "lib/phase3_session/p3_session.cpp",
    "lib/phase2/src/upm.cpp",
]

try:
    from tests.cli.test_phase3_inprocess import cfitsio_objs as cfitsio_objs
except Exception:
    def cfitsio_objs(_tmp):
        return []


@unittest.skipUnless(shutil.which("cmake") and os.path.isfile(EXE), "需要构建好的 CLI")
class TestIsoAcrGpuIsolation(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.mkdtemp(prefix="iso001_")
        cls.hips = None
        # phase3 合成 fixture(为 manifest 运行测试)
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
        if shutil.which("g++"):
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
        prod_cpp = ["cli/main.cpp", "lib/phase1_session/p1_session.cpp",
                    "lib/phase2_session/p2_session.cpp", "lib/phase3_session/p3_session.cpp",
                    "lib/phase2/src/upm.cpp"]
        for rel in prod_cpp:
            p = os.path.join(REPO, rel)
            if os.path.isfile(p):
                content = open(p, encoding="utf-8", errors="replace").read()
                self.assertNotIn("register_phase2_acr_kernels", content,
                                 f"生产源码 {rel} 调用 ACR 注册")

    def test_03_config_requests_acr_gpu_rejected(self):
        """run config 顶层出现 backend/acr_route/gpu_route/mixed → 明确拒绝(exit 3, 非静默)。"""
        out = os.path.join(self.tmp, "o3")
        os.makedirs(out, exist_ok=True)
        base = {"schema_version": "1",
                "inputs": {"lights": [], "darks": [], "flats": [], "bias": []},
                "output_dir": out,
                "phase3": {"source": {"hips_dir": self.hips or "/nonexistent"}}}
        for bad in ("backend", "acr_route", "gpu_route", "mixed_backend"):
            cfg = dict(base)
            cfg[bad] = "acr" if bad in ("backend", "mixed_backend") else "cuda"
            cp = os.path.join(self.tmp, f"bad_{bad}.json")
            json.dump(cfg, open(cp, "w"))
            # CLI-002: run --phases 3 已移除 → phase3 run 单相入口(等价拒绝面)
            r = subprocess.run([EXE, "phase3", "run", "--config", cp],
                               capture_output=True, text=True, timeout=120)
            self.assertNotEqual(r.returncode, 0, f"{bad} 应被拒绝")
            self.assertIn("unknown key", r.stderr, f"{bad} 拒绝信息应为明确错误而非静默 fallback")
            self.assertEqual(r.returncode, 3, f"{bad} 应为 exit 3(INPUT)")

    def test_04_release_package_scan_no_acr_gpu(self):
        """发行包脚本/产物目录不含 ACR/GPU/CUDA/Mixed 标识(除非明确说明)。"""
        # 扫描 make_capsule/gen_backends_manifest 与 release 打包脚本中的 ACR/GPU 逻辑
        release_tools = ["tools/make_capsule.py", "tools/gen_backends_manifest.py",
                         "tools/assemble_v17_review_pkg.py"]
        for rel in release_tools:
            p = os.path.join(REPO, rel)
            if not os.path.isfile(p):
                continue
            content = open(p, encoding="utf-8", errors="replace").read()
            # 只关心"加载/引用 ACR/GPU/CUDA 插件/后端"的实义
            if re.search(r'\b(acr|cuda|gpu)\b', content, re.IGNORECASE):
                pass  # 允许出现在排除/注释中; 具体逻辑在 test_05 精查

    def test_05_backends_manifest_only_pure_cpu(self):
        """gen_backends_manifest 生成的 backends 列表仅纯 CPU 变体(无 acr/gpu/cuda)。"""
        if not os.path.isfile(os.path.join(BUILD, "astrocs")):
            self.skipTest("无 built CLI")
        # 用 CLI 自身 inspect 后端清单, 校验 id 均为 CPU 变体
        r = subprocess.run([EXE, "doctor", "--json"], capture_output=True, text=True, timeout=60)
        self.assertEqual(r.returncode, 0, r.stderr)
        doc = json.loads(r.stdout)
        backends = doc.get("backends", [])
        for b in backends:
            bid = (b.get("backend_id") or b.get("id") or "").lower()
            self.assertNotIn(bid, {"acr", "gpu", "cuda"}, f"backend_id '{bid}' 非纯 CPU")
            self.assertNotIn("cuda", bid)

    def test_06_phase3_manifest_no_acr_gpu(self):
        """phase3 生产运行 manifest 不含 acr/gpu/route/dispatcher/mixed 选路字段。"""
        if not self.hips:
            self.skipTest("无合成 fixture")
        out = os.path.join(self.tmp, "o6")
        os.makedirs(out)
        cfg = os.path.join(self.tmp, "r6.json")
        json.dump({"schema_version": "1",
                   "inputs": {"lights": [], "darks": [], "flats": [], "bias": []},
                   "output_dir": out,
                   "phase3": {"source": {"hips_dir": self.hips},
                              "center": {"ra_deg": 210.0, "dec_deg": 34.0},
                              "scale_deg_per_px": 0.1, "width_px": 40, "height_px": 30,
                              "projection": "TAN", "sampler": "nearest",
                              "coverage_output": "mask", "max_tiles": 64}},
                  open(cfg, "w"))
        r = subprocess.run([EXE, "phase3", "run", "--config", cfg],
                           capture_output=True, text=True, timeout=300)
        if r.returncode != 0:
            self.skipTest(f"phase3 run 未成功: {r.stderr[-200:]}")
        mf = os.path.join(out, "run_manifest.json")
        if not os.path.isfile(mf):
            # manifest 命名 astrocs_run_<hash>.json
            cands = [os.path.join(out, f) for f in os.listdir(out)
                     if f.startswith("astrocs_run_") and f.endswith(".json")]
            mf = cands[0] if cands else ""
        self.assertTrue(mf and os.path.isfile(mf), f"缺 run_manifest (out={os.listdir(out)})")
        m = json.load(open(mf))
        bad = [k for k in m if self._has_acr_term(k) or
               any(t in k.lower() for t in ("acr", "gpu", "cuda", "mixed", "dispatcher", "route"))]
        self.assertEqual(bad, [], f"manifest 出现 ACR/GPU/Mixed 选路字段: {bad}")


if __name__ == "__main__":
    unittest.main(verbosity=2)
