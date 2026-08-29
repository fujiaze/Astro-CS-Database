#!/usr/bin/env python3
"""LNX-005 测试: Linux amd64 alpha 发布包 — staging install + 单 exe 白名单 + MANIFEST/SBOM/licenses/hash + 空目录 smoke。
验收(03 L143): 只有一个 user exe; 私有 SO/manifest 完整; 包名 alpha; 解包运行 PASS。
"""
import hashlib, os, re, shutil, subprocess, sys, tarfile, tempfile, unittest

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
MAKER = os.path.join(REPO, "tools", "make_linux_release.py")
BUILD = os.path.join(REPO, "build", "lnx_v5_clean_rel", "astrocs")


def sha256_file(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 16), b""):
            h.update(chunk)
    return h.hexdigest()


@unittest.skipUnless(os.path.isfile(BUILD), "需要先建 lnx_v5_clean_rel/astrocs")
class TestLinuxRelease(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.mkdtemp(prefix="lnx005_")
        cls.out = os.path.join(cls.tmp, "out")
        r = subprocess.run([sys.executable, MAKER, "--bin", BUILD, "--out", cls.out],
                           capture_output=True, text=True, cwd=REPO, timeout=120)
        cls.rc = r.returncode
        cls.outtxt = r.stdout
        taps = [f for f in os.listdir(cls.out) if f.endswith(".tar.zst") or f.endswith(".tar.gz")]
        cls.pkg = os.path.join(cls.out, taps[0]) if taps else None
        # 解包到独立目录(供各用例)
        cls.unpack = os.path.join(cls.tmp, "unpack")
        os.makedirs(cls.unpack)
        if cls.pkg:
            if cls.pkg.endswith(".tar.zst"):
                subprocess.run(["tar", "--zstd", "-xf", cls.pkg, "-C", cls.unpack], check=True, timeout=60)
            else:
                subprocess.run(["tar", "-xzf", cls.pkg, "-C", cls.unpack], check=True, timeout=60)

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.tmp, ignore_errors=True)

    def test_01_package_created(self):
        self.assertEqual(self.rc, 0, self.outtxt)
        self.assertTrue(self.pkg and os.path.isfile(self.pkg), "必须生成 tar 包")
        self.assertIn("AstroCS-Linux-amd64-", os.path.basename(self.pkg))

    def test_02_package_name_is_alpha(self):
        base = os.path.basename(self.pkg)
        # 版本单源: 读取 VERSION
        ver = open(os.path.join(REPO, "VERSION"), encoding="utf-8").read().strip()
        m = re.search(r"AstroCS-Linux-amd64-(.+)\.tar\.", base)
        self.assertTrue(m)
        self.assertEqual(m.group(1), ver, "包名版本必须来自 VERSION 源")
        self.assertRegex(ver, r"^0\.9\.0-alpha\.\d+$", "包名必须 alpha")
        for bad in ("stable", "rc", "release", "1.0"):
            self.assertNotIn(bad, base, f"禁止 {bad} 标记")

    def test_03_single_user_exe_and_tree(self):
        root = os.path.join(self.unpack, "astrocs")
        exes = []
        for dp, _dn, files in os.walk(root):
            for fn in files:
                p = os.path.join(dp, fn)
                if os.access(p, os.X_OK):
                    exes.append(os.path.relpath(p, root))
        bin_exes = [e for e in exes if e.startswith("bin/")]
        self.assertEqual(len(bin_exes), 1, f"bin/ 必须恰一个 user exe, got {bin_exes}")
        self.assertEqual(bin_exes[0].replace(os.sep, "/"), "bin/astrocs")

    def test_04_manifest_sbom_licenses_hash_present(self):
        root = os.path.join(self.unpack, "astrocs")
        for req in ["MANIFEST.json", "SBOM.spdx.json", "VERSION", "SHA256SUMS",
                    "backends.manifest.json", "LICENSES/NOTICE.txt"]:
            self.assertTrue(os.path.isfile(os.path.join(root, req)), f"缺 {req}")

    def test_05_manifest_entries_match_files(self):
        root = os.path.join(self.unpack, "astrocs")
        man = json_load(os.path.join(root, "MANIFEST.json"))
        for e in man["files"]:
            p = os.path.join(root, e["path"])
            self.assertTrue(os.path.isfile(p), f"manifest 指向不存在 {e['path']}")
            self.assertEqual(sha256_file(p), e["sha256"], f"hash 不符 {e['path']}")
        sums = os.path.join(root, "SHA256SUMS")
        r = subprocess.run(["sha256sum", "-c", sums], capture_output=True, text=True, cwd=self.unpack)
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)

    def test_06_extracted_run_doctor_passes(self):
        exe = os.path.join(self.unpack, "astrocs", "bin", "astrocs")
        r = subprocess.run([exe, "doctor", "--json"], capture_output=True, text=True, timeout=60)
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
        self.assertIn('"verdict": "PASS"', r.stdout, "解包后 doctor must PASS")
        v = subprocess.run([exe, "--version"], capture_output=True, text=True, timeout=30)
        self.assertIn("astrocs 0.9.0", v.stdout, "解包后 --version 可运行")


def json_load(path):
    import json
    with open(path, encoding="utf-8") as f:
        return json.load(f)


if __name__ == "__main__":
    unittest.main(verbosity=2)
