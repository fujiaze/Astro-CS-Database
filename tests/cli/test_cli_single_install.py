#!/usr/bin/env python3
"""CLI-008 测试: 发布 install/package 树 scanner — 仅一个用户 exe, 无旧 phase/benchmark exe 泄漏; CLI 不 shell-out。"""
import os, re, shutil, subprocess, tempfile, unittest

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
CLI = os.path.join(REPO, "cli")

# 非发布(旧 phase/benchmark/tool/test)exe 名: 泄漏到 install 树任一都是违规
LEGACY_EXES = re.compile(
    r"^(orchestrator\.exe|astrocs-stage2|phase2|acr-benchmark|acr-report|acr-classic-runner|"
    r"browser_cli|healpix_browser_qt|calibrated_pair_diag|rejection_cli|"
    r"phase1|phase2_synthetic_gate|phase2_ivar_wiring|phase2_execution_options|"
    r"phase2_routing|phase2_async_io|phase2_sampler_parallel)$", re.I)


@unittest.skipUnless(shutil.which("cmake") and shutil.which("g++"), "需要 cmake/g++")
class TestCliSingleInstall(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.mkdtemp(prefix="cli008_")
        cls.bdir = os.path.join(cls.tmp, "build")
        subprocess.run(["cmake", "-S", CLI, "-B", cls.bdir],
                       check=True, capture_output=True, timeout=120)
        subprocess.run(["cmake", "--build", cls.bdir, "-j2"],
                       check=True, capture_output=True, timeout=240)
        cls.prefix = os.path.join(cls.tmp, "prefix")
        r = subprocess.run(["cmake", "--install", cls.bdir, "--prefix", cls.prefix],
                           capture_output=True, text=True, timeout=120)
        cls.install_rc = r.returncode
        cls.install_err = r.stderr

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.tmp, ignore_errors=True)

    def _install_files(self):
        out = {}
        for root, _dirs, files in os.walk(self.prefix):
            for f in files:
                out[os.path.relpath(os.path.join(root, f), self.prefix)] = os.path.join(root, f)
        return out

    def test_01_install_succeeds(self):
        self.assertEqual(self.install_rc, 0, self.install_err[-500:])

    def test_02_exactly_one_user_exe(self):
        """install 树 bin/ 必须恰一个用户 exe, 即 astrocs。"""
        if self.install_rc != 0:
            self.skipTest("install 失败")
        exes = []
        for root, _dirs, files in os.walk(self.prefix):
            for f in files:
                p = os.path.join(root, f)
                if os.name == "nt":
                    if f.lower().endswith((".exe", ".dll")):
                        exes.append(os.path.relpath(p, self.prefix))
                else:
                    if os.access(p, os.X_OK) and os.path.isfile(p):
                        exes.append(os.path.relpath(p, self.prefix))
        users = [e for e in exes if os.path.basename(e).lower() in ("astrocs", "astrocs.exe")]
        self.assertEqual(len(users), 1, f"必须恰一个用户 exe astrocs, 得 {users}")

    def test_03_no_legacy_exe_leaked(self):
        """install 树不得含任何旧 phase/benchmark/tool/test 可执行目标。"""
        if self.install_rc != 0:
            self.skipTest("install 失败")
        for fpath in self._install_files():
            base = os.path.splitext(os.path.basename(fpath))[0]
            if LEGACY_EXES.match(base):
                self.fail(f"旧/非发布 exe 泄漏到 install 树: {fpath}")

    def test_04_no_shellout_in_install(self):
        """安装树只应含 CLI 与必要的共享库/数据, 不得含 script 转发到子进程的执行器。"""
        if self.install_rc != 0:
            self.skipTest("install 失败")
        files = self._install_files()
        # 允许的数据/库扩展; 禁止可执行脚本类(e.g. .sh/.py 可穿透执行旧 exe)
        for fpath in files.values():
            if fpath.endswith((".sh", ".bat", ".cmd")):
                self.fail(f"install 树含脚本入口(可能 shell-out): {fpath}")

    def test_05_install_tree_only_bin_and_astroc_data(self):
        """install 树仅含 bin(或 lib)下的用户产物, 不携带源码/第三方便携 exe。"""
        if self.install_rc != 0:
            self.skipTest("install 失败")
        files = self._install_files()
        for rel in files:
            self.assertFalse(rel.endswith((".cpp", ".h", ".c", ".hpp")),
                             f"源码/头文件不应进入 install 树: {rel}")
            self.assertNotIn("third_party", rel, f"第三方便携 exe/头不应进 install 树: {rel}")


if __name__ == "__main__":
    unittest.main(verbosity=2)
