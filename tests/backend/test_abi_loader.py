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
