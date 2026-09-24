#!/usr/bin/env python3
"""LNX-005 测试: Linux amd64 alpha 发布包 — staging install + 单 exe 白名单 + MANIFEST/SBOM/licenses/hash + 空目录 smoke。
验收(03 L143): 只有一个 user exe; 私有 SO/manifest 完整; 包名 alpha; 解包运行 PASS。

被测对象（输入对象）解析 —— 仓库既有约定，不新造：
  1. 环境变量 ASTROCS_CLI_BIN（全仓 CLI 测试统一覆盖点）；
  2. build/acsd（AGENTS.md §3 唯一根 CMake 产物；ROOT-008 后唯一产品二进制）；
  3. run/ci/build-gcc-release/astrocs（CI 构建目录）。
  先例（逐字同序）：eng/ci/check_algo_wiring.py:150
  `for rel in ("build/acsd", "run/ci/build-gcc-release/astrocs")`；
  另见 eng/tools/check_cli_run_preset.py:285、eng/tools/check_legacy_exit.py:36、
  eng/tests/cli/test_phase123_pipeline.py:34（"唯一产品二进制 build/acsd"）。

原实现硬编码 build/lnx_v5_clean_rel/astrocs —— 该路径**全仓无生产者**（仅
eng/tools/assemble_audit.py:128 的历史审计行提及），于是类级 @skipUnless 恒假、
6 个用例恒 skip：LNX-005 这一条版本条款长期没有可执行载体（AGENTS.md §5
「SKIP 充数算未完成」）。现改为：解析到二进制 ⇒ 6 例真跑；解析不到 ⇒
**具名判红** MISSING_CLI_BINARY（fail-closed，ENGINEERING_SPEC §10），不再静默跳过。
本测试的判据面是**打包工具** eng/tools/make_linux_release.py（包结构/白名单/
manifest/SBOM/hash/解包 smoke），二进制只是输入对象 ⇒ 不要求它来自某个特定
构建目录；"clean release 构建"的溯义不由本测试承担（见 TEST-P3 回执）。
"""
import hashlib, os, re, shutil, subprocess, sys, tarfile, tempfile, unittest

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
MAKER = os.path.join(REPO, "eng", "tools", "make_linux_release.py")


def find_cli_binary():
    """返回 (路径, None) 或 (None, 具名原因)。解析序见模块 docstring。"""
    env = os.environ.get("ASTROCS_CLI_BIN")
    if env:
        if os.path.isfile(env):
            return env, None
        return None, ("ASTROCS_CLI_BIN=%s 指向的文件不存在" % env)
    for rel in ("build/acsd", "run/ci/build-gcc-release/astrocs"):
        cand = os.path.join(REPO, *rel.split("/"))
        if os.path.isfile(cand):
            return cand, None
    return None, ("已探测 ASTROCS_CLI_BIN / build/acsd / "
                  "run/ci/build-gcc-release/astrocs 均不存在")


BUILD, BUILD_MISSING = find_cli_binary()


def sha256_file(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 16), b""):
            h.update(chunk)
    return h.hexdigest()


class TestLinuxRelease(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.mkdtemp(prefix="lnx005_")
        cls.out = os.path.join(cls.tmp, "out")
        cls.unpack = os.path.join(cls.tmp, "unpack")
        cls.pkg = None
        cls.rc = None
        cls.outtxt = ""
        if BUILD is None:
            return                      # 每个用例由 _require_bin 具名判红
        r = subprocess.run([sys.executable, MAKER, "--bin", BUILD, "--out", cls.out],
                           capture_output=True, text=True, cwd=REPO, timeout=120)
        cls.rc = r.returncode
        cls.outtxt = r.stdout
        taps = [f for f in os.listdir(cls.out) if f.endswith(".tar.zst") or f.endswith(".tar.gz")]
        cls.pkg = os.path.join(cls.out, taps[0]) if taps else None
        # 解包到独立目录(供各用例)
        os.makedirs(cls.unpack)
        if cls.pkg:
            if cls.pkg.endswith(".tar.zst"):
                subprocess.run(["tar", "--zstd", "-xf", cls.pkg, "-C", cls.unpack], check=True, timeout=60)
            else:
                subprocess.run(["tar", "-xzf", cls.pkg, "-C", cls.unpack], check=True, timeout=60)

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.tmp, ignore_errors=True)

    def _require_bin(self):
        """缺输入对象 ⇒ 具名判红，**不得**静默 skip（AGENTS.md §5 / ENGINEERING_SPEC §10）。

        门禁判据：LNX-005 是版本条款的可执行载体，SKIP 等于该条款没有载体。
        """
        if BUILD is None:
            self.fail("MISSING_CLI_BINARY: 未找到被测 acsd CLI 产物（%s）。"
                      "构建: cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release && "
                      "ninja -C build；或用 ASTROCS_CLI_BIN=<path> 指定。"
                      "本用例不得以 SKIP 通过。" % BUILD_MISSING)

    def test_01_package_created(self):
        self._require_bin()
        self.assertEqual(self.rc, 0, self.outtxt)
        self.assertTrue(self.pkg and os.path.isfile(self.pkg), "必须生成 tar 包")
        self.assertIn("ACSD-Linux-amd64-", os.path.basename(self.pkg))

    def test_02_package_name_is_alpha(self):
        self._require_bin()
        base = os.path.basename(self.pkg)
        # 版本单源: 读取 VERSION
        ver = open(os.path.join(REPO, "VERSION"), encoding="utf-8").read().strip()
        m = re.search(r"ACSD-Linux-amd64-(.+)\.tar\.", base)
        self.assertTrue(m)
        self.assertEqual(m.group(1), ver, "包名版本必须来自 VERSION 源")
        # 版本号单源 = 根 VERSION（VERSION-CONSISTENCY 门，见 docs/ci/01_CHECKS.md）
        # ⇒ 这里只锁「alpha 形态」，不得写死任何基础号（写死即随唯一源推进而恒假，
        # 并被 @skipUnless 的恒 skip 掩盖 —— EMPTYASSERT-01 实测：门禁对象一就位即判红）。
        self.assertRegex(ver, r"^\d+\.\d+\.\d+-alpha\.\d+$", "包名必须 alpha")
        for bad in ("stable", "rc", "release"):
            self.assertNotIn(bad, base, f"禁止 {bad} 标记")
        # 稳定版标记（两段式：无补丁段、无预发布段）必须按**版本段**判定，不得按裸子串：
        # 子串判定会被基础号本身误伤（基础号含该子串 ⇒ 断言在现行 VERSION 下恒假，
        # 并被 @skipUnless 的恒 skip 掩盖 —— 与 EMPTYASSERT-01 同型）。
        # 注：本注释刻意不写任何 X.Y.Z 字面量 —— VER-001（docs/VERSIONING.md §4）
        # 的扫描面含 eng/tests/**，写死历史版本号会随唯一源推进而恒红。
        ver_field = m.group(1)
        self.assertNotRegex(ver_field, r"^\d+\.\d+$", "禁止两段式稳定版标记")
        self.assertIn("-alpha.", ver_field, "禁止缺预发布段的稳定版标记")

    def test_03_single_user_exe_and_tree(self):
        self._require_bin()
        root = os.path.join(self.unpack, "acsd")
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
        self._require_bin()
        root = os.path.join(self.unpack, "acsd")
        for req in ["MANIFEST.json", "SBOM.spdx.json", "VERSION", "SHA256SUMS",
                    "backends.manifest.json", "LICENSES/NOTICE.txt"]:
            self.assertTrue(os.path.isfile(os.path.join(root, req)), f"缺 {req}")

    def test_05_manifest_entries_match_files(self):
        self._require_bin()
        root = os.path.join(self.unpack, "acsd")
        man = json_load(os.path.join(root, "MANIFEST.json"))
        for e in man["files"]:
            p = os.path.join(root, e["path"])
            self.assertTrue(os.path.isfile(p), f"manifest 指向不存在 {e['path']}")
            self.assertEqual(sha256_file(p), e["sha256"], f"hash 不符 {e['path']}")
        sums = os.path.join(root, "SHA256SUMS")
        r = subprocess.run(["sha256sum", "-c", sums], capture_output=True, text=True, cwd=self.unpack)
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)

    def test_06_extracted_run_doctor_passes(self):
        self._require_bin()
        exe = os.path.join(self.unpack, "acsd", "bin", "acsd")
        r = subprocess.run([exe, "doctor", "--json"], capture_output=True, text=True, timeout=60)
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
        self.assertIn('"verdict": "PASS"', r.stdout, "解包后 doctor must PASS")
        v = subprocess.run([exe, "--version"], capture_output=True, text=True, timeout=30)
        with open(os.path.join(REPO, "VERSION"), encoding="utf-8") as vf:
            base_num = vf.read().strip().split("-alpha.")[0]  # 单源: 根 VERSION 基础号
        self.assertIn("acsd " + base_num, v.stdout, "解包后 --version 可运行")


def json_load(path):
    import json
    with open(path, encoding="utf-8") as f:
        return json.load(f)


if __name__ == "__main__":
    unittest.main(verbosity=2)
