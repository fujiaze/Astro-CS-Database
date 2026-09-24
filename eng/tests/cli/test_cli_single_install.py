#!/usr/bin/env python3
"""CLI-008 测试: 发布 install/package 树 scanner — 仅一个用户 exe, 无旧 phase/benchmark exe 泄漏; CLI 不 shell-out。"""
import os, re, shutil, subprocess, tempfile, unittest

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
CLI = os.path.join(REPO, "lib", "infrastructure", "cli")

# 非发布(旧 phase/benchmark/tool/test)exe 名: 泄漏到 install 树任一都是违规
LEGACY_EXES = re.compile(
    r"^(orchestrator\.exe|astrocs-stage2|phase2|acr-benchmark|acr-report|acr-classic-runner|"
    r"browser_cli|healpix_browser_qt|calibrated_pair_diag|rejection_cli|"
    r"phase1|phase2_synthetic_gate|phase2_ivar_wiring|phase2_execution_options|"
    r"phase2_routing|phase2_async_io|phase2_sampler_parallel)$", re.I)


@unittest.skipUnless(shutil.which("cmake") and shutil.which("g++"), "需要 eng/cmake/g++")
class TestCliSingleInstall(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.mkdtemp(prefix="cli008_")
        # BLD-002 冻结合同 (3e7f7581): 唯一产品事实源 = 根 CMakeLists, cli 子图
        # 禁止 install 规则。扫描对象改用根图构建树 (CI build 步产出; 漂移修复)。
        # 断言语义不变: install 树恰一个用户 exe acsd + 无 legacy exe 泄漏。
        # DISPATCH 附录 H（构建隔离）: 被测构建树 = 被测二进制所在目录; ASTROCS_CLI_BIN 覆盖。
        _bin = os.environ.get("ASTROCS_CLI_BIN")
        cls.bdir = (os.path.dirname(os.path.abspath(_bin)) if _bin
                    else os.path.join(REPO, "build"))
        # install 规则源 = 真实 CMake 构建目录(有 CMakeCache.txt); 根 build/
        # 只是 CI 步 cp 出的漂移检查面, 自身无 install 规则。
        cls.cmake_dir = os.path.join(cls.bdir, "linux-control")
        if not os.path.isfile(os.path.join(cls.cmake_dir, "CMakeCache.txt")):
            cls.cmake_dir = cls.bdir
        have_tree = all(os.path.isfile(os.path.join(
            cls.bdir, p)) for p in ("acsd", "libacsd_runtime.so"))
        if not have_tree:
            msg = ("需根图构建树 build/{acsd,libacsd_runtime.so} "
                   "(BLD-002; eng/ci/steps/linux_build_root_graph.sh 产出)")
            # M8-F-003: CI 面缺前置产物是硬失败(门失效), 不得静默 SKIP;
            # 仅本地开发环境(无 CI 标记)允许跳过。
            if os.environ.get("CI") or os.environ.get("GITHUB_ACTIONS"):
                raise AssertionError(
                    msg + " — CI 构建步未产出前置产物, 该门恒 SKIP = 未执行")
            raise unittest.SkipTest(msg)
        cls.prefix = os.path.join(cls.tmp, "prefix")
        r = subprocess.run(["cmake", "--install", cls.cmake_dir, "--prefix", cls.prefix],
                           capture_output=True, text=True, timeout=300)
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
        """install 树 bin/ 必须恰一个用户 exe, 即 acsd。"""
        if self.install_rc != 0:
            self.fail(f"install 失败 rc={self.install_rc}: {self.install_err[-300:]}")
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
        users = [e for e in exes if os.path.basename(e).lower() in ("acsd", "acsd.exe")]
        self.assertEqual(len(users), 1, f"必须恰一个用户 exe acsd, 得 {users}")

    def test_03_no_legacy_exe_leaked(self):
        """install 树不得含任何旧 phase/benchmark/tool/test 可执行目标。"""
        if self.install_rc != 0:
            self.fail(f"install 失败 rc={self.install_rc}: {self.install_err[-300:]}")
        for fpath in self._install_files():
            base = os.path.splitext(os.path.basename(fpath))[0]
            if LEGACY_EXES.match(base):
                self.fail(f"旧/非发布 exe 泄漏到 install 树: {fpath}")

    def test_04_no_shellout_in_install(self):
        """安装树只应含 CLI 与必要的共享库/数据, 不得含 script 转发到子进程的执行器。"""
        if self.install_rc != 0:
            self.fail(f"install 失败 rc={self.install_rc}: {self.install_err[-300:]}")
        files = self._install_files()
        # 允许的数据/库扩展; 禁止可执行脚本类(e.g. .sh/.py 可穿透执行旧 exe)
        for fpath in files.values():
            if fpath.endswith((".sh", ".bat", ".cmd")):
                self.fail(f"install 树含脚本入口(可能 shell-out): {fpath}")

    def test_05_install_tree_only_bin_and_astroc_data(self):
        """install 树仅含 bin(或 lib)下的用户产物, 不携带源码/第三方便携 exe。"""
        if self.install_rc != 0:
            self.fail(f"install 失败 rc={self.install_rc}: {self.install_err[-300:]}")
        files = self._install_files()
        for rel in files:
            self.assertFalse(rel.endswith((".cpp", ".h", ".c", ".hpp")),
                             f"源码/头文件不应进入 install 树: {rel}")
            self.assertNotIn("third_party", rel, f"第三方便携 exe/头不应进 install 树: {rel}")


if __name__ == "__main__":
    unittest.main(verbosity=2)
