#!/usr/bin/env python3
"""ABI-002 测试: manifest/hash/ABI/ISA/路径注入 全拒绝且无 illegal instruction。"""
import ctypes, json, os, shutil, subprocess, tempfile, unittest

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
HOST = os.path.join(REPO, "lib", "backend_host")
INC = os.path.join(REPO, "include")

FIXTURE_SRC = os.path.join(REPO, "tests", "backend", "fixture_backend.cpp")


class TestBackendLoader(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.mkdtemp(prefix="abi_loader_")
        cls.backends = os.path.join(cls.tmp, "backends")
        os.makedirs(cls.backends)
        # 合法 fixture DSO
        cls.so = os.path.join(cls.backends, "fixture.so")
        r = subprocess.run(["g++", "-std=c++17", "-shared", "-fPIC", f"-I{INC}",
                            FIXTURE_SRC, os.path.join(HOST, "host_services.cpp"),
                            "-o", cls.so], capture_output=True, text=True, timeout=120)
        assert r.returncode == 0, r.stderr
        # 合法 manifest(实测 sha256)
        import hashlib
        cls.good_sha = hashlib.sha256(open(cls.so, "rb").read()).hexdigest()
        cls.manifest = os.path.join(cls.tmp, "backends.manifest.json")

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.tmp, ignore_errors=True)

    def _write_manifest(self, entry):
        doc = {"schema_version": "1", "kind": "astrocs_backends_manifest", "backends": [entry]}
        with open(self.manifest, "w", encoding="utf-8") as f:
            json.dump(doc, f)
        return self.manifest

    def _load(self, manifest_path):
        """编译+运行 loader 测试 TU; 返回 (rc, stdout)。"""
        exe = os.path.join(self.tmp, "loader_tu")
        src = os.path.join(REPO, "tests", "backend", "loader_probe_main.cpp")
        r = subprocess.run(["g++", "-std=c++17", f"-I{INC}", f"-I{HOST}",
                            f"-I{os.path.join(REPO, 'lib', 'common', 'crypto')}",
                            src, os.path.join(HOST, "backend_loader.cpp"),
                            os.path.join(HOST, "host_services.cpp"),
                            os.path.join(HOST, "cpu_features.cpp"),
                            os.path.join(REPO, "lib", "common", "crypto", "sha256.cpp"),
                            "-ldl", "-o", exe], capture_output=True, text=True, timeout=180)
        assert r.returncode == 0, r.stderr
        run = subprocess.run([exe, self.backends, manifest_path],
                             capture_output=True, text=True, timeout=60)
        return run

    def test_01_cpu_features_and_affinity(self):
        exe = os.path.join(self.tmp, "cpu_probe")
        r = subprocess.run(["g++", "-std=c++17", f"-I{HOST}", "-x", "c++", "-",
                            os.path.join(HOST, "cpu_features.cpp"), "-o", exe],
                           input="#include \"cpu_features.h\"\n"
                                 "#include <cstdio>\n"
                                 "int main(){ printf(\"feat=%llu aff=%u\\n\", "
                                 "(unsigned long long)astrocs_cpu_detect_features_v1(), "
                                 "astrocs_cpu_affinity_count_v1()); return 0; }",
                           capture_output=True, text=True, timeout=60)
        self.assertEqual(r.returncode, 0, r.stderr)
        run = subprocess.run([exe], capture_output=True, text=True, timeout=30)
        self.assertEqual(run.returncode, 0)
        self.assertIn("aff=", run.stdout)
        aff = int(run.stdout.split("aff=")[1])
        self.assertGreaterEqual(aff, 1, "affinity 可用 CPU ≥1")

    def test_02_happy_path_load(self):
        self._write_manifest({"file": "fixture.so", "backend_id": "fixture",
                              "sha256": self.good_sha, "abi_version": 1,
                              "required_features_bits": 0})
        run = self._load(self.manifest)
        self.assertEqual(run.returncode, 0, f"{run.stdout}{run.stderr}")
        self.assertIn("LOADED backend_id=fixture", run.stdout)
        self.assertIn("SELFTEST_OK", run.stdout)

    def test_03_fake_hash_rejected(self):
        self._write_manifest({"file": "fixture.so", "backend_id": "fixture",
                              "sha256": "0" * 64, "abi_version": 1,
                              "required_features_bits": 0})
        run = self._load(self.manifest)
        self.assertEqual(run.returncode, 0, "回退不是崩溃")
        self.assertIn("FALLBACK hash mismatch", run.stdout)

    def test_04_fake_abi_rejected(self):
        self._write_manifest({"file": "fixture.so", "backend_id": "fixture",
                              "sha256": self.good_sha, "abi_version": 2,
                              "required_features_bits": 0})
        run = self._load(self.manifest)
        self.assertIn("FALLBACK", run.stdout)
        self.assertNotIn("LOADED", run.stdout)

    def test_05_unsupported_isa_rejected_without_execution(self):
        self._write_manifest({"file": "fixture.so", "backend_id": "fixture",
                              "sha256": self.good_sha, "abi_version": 1,
                              "required_features_bits": 1 << 62})  # 未定义位: 恒不在 detected
        run = self._load(self.manifest)
        self.assertEqual(run.returncode, 0, "预检拒绝后进程存活(无 illegal instruction)")
        self.assertIn("FALLBACK unsupported ISA", run.stdout)
        # 若本机实测含 AVX512F 则可用真实位再验一次; 合成位路径已覆盖机制
        self.assertNotIn("LOADED", run.stdout)

    def test_06_malformed_manifest_rejected(self):
        with open(self.manifest, "w", encoding="utf-8") as f:
            f.write("{not json at all")
        run = self._load(self.manifest)
        self.assertIn("FALLBACK malformed", run.stdout)

    def test_06a_missing_required_features_bits_rejected(self):
        """P0(bug 狩猎 R5): required_features_bits 字段缺失 → 硬失败拒载。

        此前 b.value("required_features_bits", 0ull) 缺失静默默认 0 →
        ISA 预检门恒真 → AVX/AVX512 DSO 可在无 ISA 机器加载、首调 SIGILL。
        生成器对每个条目(含 baseline bits=0)显式写该字段, 缺失即结构非法
        (backend_loader.h: "结构非法→err 非空, 不猜")。"""
        self._write_manifest({"file": "fixture.so", "backend_id": "fixture",
                              "sha256": self.good_sha, "abi_version": 1})
        run = self._load(self.manifest)
        self.assertEqual(run.returncode, 0, "拒载不是崩溃")
        self.assertIn("FALLBACK malformed", run.stdout)
        self.assertIn("required_features_bits", run.stdout)
        self.assertNotIn("LOADED", run.stdout)

    def test_06b_wrong_type_required_features_bits_rejected(self):
        """P0(bug 狩猎 R5): required_features_bits 类型非法(字符串) → 硬失败。

        严格类型白名单: 仅无符号整数; 负数/浮点/字符串/布尔均拒, 杜绝
        隐式数值转换把恶意值洗成 0 或近似值。"""
        for evil in ("\"0\"", "-1", "0.5", "true", "null"):
            doc = {"schema_version": "1", "kind": "astrocs_backends_manifest",
                   "backends": [{"file": "fixture.so", "backend_id": "fixture",
                                 "sha256": self.good_sha, "abi_version": 1,
                                 "required_features_bits": json.loads(evil)}]}
            with open(self.manifest, "w", encoding="utf-8") as f:
                json.dump(doc, f)
            run = self._load(self.manifest)
            self.assertIn("FALLBACK malformed", run.stdout,
                          f"required_features_bits={evil} 必须拒载")
            self.assertNotIn("LOADED", run.stdout)

    def test_06c_explicit_zero_bits_still_loads(self):
        """P0(bug 狩猎 R5): baseline 显式 required_features_bits=0 仍正常加载
        (防矫枉过正: 硬失败只针对字段缺失/类型非法, 不针对合法显式 0)。"""
        self._write_manifest({"file": "fixture.so", "backend_id": "fixture",
                              "sha256": self.good_sha, "abi_version": 1,
                              "required_features_bits": 0})
        run = self._load(self.manifest)
        self.assertIn("LOADED backend_id=fixture", run.stdout)
        self.assertIn("SELFTEST_OK", run.stdout)

    def test_07_path_injection_rejected(self):
        for evil in ("/tmp/evil.so", "../evil.so", "sub/dir.so", "..\\evil.so",
                     "C:/evil.so", ".hidden.so", ".."):
            self._write_manifest({"file": evil, "backend_id": "fixture",
                                  "sha256": self.good_sha, "abi_version": 1,
                                  "required_features_bits": 0})
            run = self._load(self.manifest)
            self.assertIn(f"REJECT_SECURITY", run.stdout, f"{evil} 必须安全拒绝")
            self.assertNotIn("LOADED", run.stdout)

    def test_08_missing_file_rejected(self):
        self._write_manifest({"file": "ghost.so", "backend_id": "ghost",
                              "sha256": "1" * 64, "abi_version": 1,
                              "required_features_bits": 0})
        run = self._load(self.manifest)
        self.assertIn("FALLBACK backend file missing", run.stdout)

    def test_09_gen_tool_roundtrip(self):
        out = os.path.join(self.tmp, "gen.json")
        r = subprocess.run(["python3", os.path.join(REPO, "tools", "gen_backends_manifest.py"),
                            self.so, "--out", out, "--feat", "sse2",
                            "--compiler", "g++-14.2"],
                           capture_output=True, text=True, timeout=60)
        self.assertEqual(r.returncode, 0, r.stderr)
        doc = json.load(open(out, encoding="utf-8"))
        self.assertEqual(doc["kind"], "astrocs_backends_manifest")
        e = doc["backends"][0]
        self.assertEqual(e["file"], "fixture.so")
        self.assertEqual(e["sha256"], self.good_sha)
        self.assertEqual(e["required_features_bits"], 1)

if __name__ == "__main__":
    unittest.main(verbosity=2)
