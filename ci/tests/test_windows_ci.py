# -*- coding: utf-8 -*-
"""V8-CI-006 单测：GitHub Windows main CI（ci_windows_driver 与 WIN-* 注册）。

覆盖（全程不要求 MSVC/Windows 宿主，Linux 上即可全绿）：
1. 注册表 conformance —— WIN-BUILD-RELEASE / WIN-TEST-UNIT /
   WIN-PACKAGE-CANDIDATE 三项 platform=windows、profiles=[windows-main]、
   waivable=false（CI-001 收紧；owner 裁决 2026-09-11：非重计算面不加
   --gate-required）、outputs 非空；heavy 项 command 自含
   ``ci/resource_monitor.py --timeout N --output run/ci/monitor/<ID>.json --``
   包裹前缀且 requires_monitor=false、heavy=false（owner 裁决
   F-CI-002-04, 2026-09-11：构建/打包/单测非重计算面, R7 conform）、
   prerequisite_tools 含 cmake；
   windows-main plan-only=62（含 3 新 id + CI-REG-002 的 CTEST-REGISTRATION）；
   fast/linux-main/linux-deep 基线 58/88/7（CI-REG-002 后）。
2. 驱动行为 —— --stages 解析（canonical 去重保序/未知阶段拒绝）、
   stage_plan argv 组装（preset/build/install 形状、逐阶段 timeout）、
   run_step 超时 124 与输出捕获、binaryDir 从 CMakePresets.json 推导、
   Windows 形态期望产物由 install-tree contract 推导（无 lib 前缀）。
3. package 组装 —— tmp 伪 install 树真跑 build_candidate：三清单内容
   （BUILD_PROVENANCE 源 SHA/工具链探测字段、SOURCE_MANIFEST git ls-files
   清单、SHA256SUMS 条目数）+ 排除规则（build cache/CTest 日志/源码剔除，
   candidate 内无源文件，SOURCE_MANIFEST 只列清单不复制源码）。
4. hosted 门控 —— 非 Windows 宿主 verify_candidate 全部 hosted-only
   （verdict=None，绝不伪造 PASS）；probe_prerequisite 平台门控
   （linux → platform_mismatch，mock 工具在位 → windows 通过）。
5. runner 集成 —— Linux 真跑 runner --profile windows-main：三项全部
   FAIL(prerequisite)（platform_mismatch；CI-001 收紧后不可 waiver）。
"""
from __future__ import annotations

import importlib.util
import json
import os
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock

_REPO = Path(__file__).resolve().parents[2]
if str(_REPO) not in sys.path:
    sys.path.insert(0, str(_REPO))

from ci.tests import _helpers as H  # noqa: E402
from ci import validate_registry as VR  # noqa: E402

_REG = json.loads((_REPO / "ci" / "checks.json").read_text(encoding="utf-8"))
_WIN_IDS = ("WIN-BUILD-RELEASE", "WIN-TEST-UNIT", "WIN-PACKAGE-CANDIDATE")
_WIN_CHECKS = {c["id"]: c for c in _REG["checks"] if c["id"] in _WIN_IDS}

_spec = importlib.util.spec_from_file_location(
    "ci_windows_driver", _REPO / "tools" / "quality" / "ci_windows_driver.py")
DRV = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(DRV)


def load_driver_module() -> object:
    return DRV


class TestRegistryConformance(unittest.TestCase):
    """注册表结构：新 id 字段与规格一致（V8-CI-006 checks.json 契约）。"""

    def test_windows_checks_registered_with_contract_fields(self):
        for cid in _WIN_IDS:
            self.assertIn(cid, _WIN_CHECKS, cid)
            c = _WIN_CHECKS[cid]
            self.assertEqual(c["platform"], "windows", cid)
            self.assertEqual(c["profiles"], ["windows-main"], cid)
            # CI-001 收紧：WIN-BUILD/TEST/PACKAGE 不可 waiver（fail-closed）
            self.assertFalse(c["waivable"], cid)
            self.assertTrue(c["outputs"], cid)
            self.assertTrue(c["changed_paths"], cid)
            self.assertIn("cmake", c.get("prerequisite_tools", []), cid)
            self.assertIn("--stages", c["command"], cid)
            # owner 裁决（CI-001 复核, 2026-09-11）：WIN-* 为构建/打包/单测
            # 非重计算面（§10.5/§18.2 资源门针对重计算区间），不加
            # --gate-required；监控包装保留（采样留证），waivable=false 维持。
            self.assertNotIn("--gate-required", c["command"], cid)
            stages = c["command"][c["command"].index("--stages") + 1].split(",")
            for s in stages:
                self.assertIn(s, DRV.STAGE_ORDER, cid)
        # 覆盖全部五阶段
        covered = set()
        for c in _WIN_CHECKS.values():
            covered |= set(c["command"][c["command"].index("--stages") + 1].split(","))
        self.assertEqual(covered, set(DRV.STAGE_ORDER))

    def test_windows_heavy_monitor_wrapper_prefix(self):
        for cid in _WIN_IDS:
            c = _WIN_CHECKS[cid]
            # owner 裁决（F-CI-002-04, 2026-09-11）：WIN-* 非重计算面 →
            # requires_monitor=false 解除 monitor_gate_missing 硬失败面；
            # R7（heavy→monitor）conform 同步 heavy=false。监控包装与
            # waivable=false 维持（采样留证 + 不可 waiver）。
            self.assertFalse(c["heavy"], cid)
            self.assertFalse(c["requires_monitor"], cid)
            self.assertEqual(c["command"][:2],
                             ["python3", "ci/resource_monitor.py"], cid)
            i = c["command"].index("--")
            self.assertEqual(c["command"][i + 1 : i + 3],
                             ["python3", "tools/quality/ci_windows_driver.py"], cid)
            j = c["command"].index("--output")
            self.assertEqual(c["command"][j + 1],
                             f"run/ci/monitor/{cid}.json", cid)
            self.assertIn("--timeout", c["command"], cid)

    def test_registry_strict_pass_97(self):
        # CI-REG-002 注册 17 项 CTEST-* 后 80 → 97。
        errors, n = VR.validate(_REPO / "ci" / "checks.json", strict=True)
        self.assertEqual(errors, [])
        self.assertEqual(n, 97)


class TestProfilePlans(unittest.TestCase):
    """只读主仓库 plan-only：windows-main 增量与既有 profile 基线不变。"""

    @classmethod
    def plan(cls, profile: str) -> dict:
        proc = H.sh([sys.executable, str(H.RUNNER), "--profile", profile,
                     "--plan-only"], cwd=_REPO, timeout=120)
        assert proc.returncode == 0, proc.stderr[-400:]
        return json.loads(proc.stdout)

    def test_windows_main_61_contains_new_ids(self):
        plan = self.plan("windows-main")
        ids = {c["id"] for c in plan["checks"]}
        self.assertEqual(plan["selected_count"], 62)
        self.assertTrue(set(_WIN_IDS) <= ids)
        self.assertEqual({c["platform"] for c in plan["checks"]
                          if c["id"] in _WIN_IDS}, {"windows"})

    def test_other_profiles_unchanged_57_71_7(self):
        for profile, count in (("fast", 58), ("linux-main", 88), ("linux-deep", 7)):
            plan = self.plan(profile)
            ids = {c["id"] for c in plan["checks"]}
            self.assertEqual(plan["selected_count"], count, profile)
            self.assertFalse(ids & set(_WIN_IDS), profile)


class TestDriverStages(unittest.TestCase):
    """驱动层：--stages 解析 / stage_plan argv / run_step 超时与捕获 / 推导。"""

    def test_stage_parsing_canonical_dedupe_and_reject(self):
        self.assertEqual(DRV.parse_stages("build,configure,build"),
                         ["configure", "build"])
        self.assertEqual(DRV.parse_stages("install,package"),
                         ["install", "package"])
        with self.assertRaises(ValueError):
            DRV.parse_stages("bogus")
        with self.assertRaises(ValueError):
            DRV.parse_stages("")
        # 空段宽容（"configure,,," 不炸，等价 "configure"）
        self.assertEqual(DRV.parse_stages("configure,,,"),
                         ["configure"])

    def test_stage_plan_argv_shape_and_timeouts(self):
        plan = {s["name"]: s for s in DRV.stage_plan(
            ["configure", "build", "test", "install", "package"])}
        self.assertEqual(plan["configure"]["argv"],
                         ["cmake", "--preset", DRV.DEFAULT_PRESET])
        self.assertEqual(plan["configure"]["timeout"], DRV.STAGE_TIMEOUTS["configure"])
        build = plan["build"]["argv"]
        self.assertEqual(build[:2], ["cmake", "--build"])
        self.assertIn("--config", build)
        self.assertIn(DRV.BUILD_CONFIG, build)
        self.assertIn("--parallel", build)  # 并行度由 host 探测注入
        self.assertEqual(plan["test"]["argv"][:2], ["ctest", "--preset"])
        self.assertEqual(plan["test"]["argv"][-2:],
                         ["--output-junit", DRV.DEFAULT_JUNIT])
        inst = plan["install"]["argv"]
        self.assertEqual(inst[:2], ["cmake", "--install"])
        self.assertIn(DRV.DEFAULT_BUILD_DIR, inst)
        self.assertEqual(plan["package"].get("kind"), "python-internal")
        for name, s in plan.items():
            self.assertEqual(s["timeout"], DRV.STAGE_TIMEOUTS[name], name)

    def test_run_step_timeout_kills_to_124(self):
        res = DRV.run_step([sys.executable, "-c", "import time; time.sleep(30)"],
                           timeout=1)
        self.assertEqual(res["exit_code"], 124)
        self.assertTrue(res["timed_out"])

    def test_run_step_captures_output_and_exit_code(self):
        res = DRV.run_step([sys.executable, "-c",
                            "print('hello-driver'); raise SystemExit(3)"],
                           timeout=30)
        self.assertEqual(res["exit_code"], 3)
        self.assertFalse(res["timed_out"])
        self.assertIn("hello-driver", res["output_tail"])
        # F-R4-01：失败但无错误关键词时 error_lines 恒为空列表（schema 兼容）
        self.assertEqual(res["error_lines"], [])

    def test_run_step_success_has_empty_error_lines(self):
        """F-R4-01：成功（exit 0）时 error_lines 恒为 []。"""
        res = DRV.run_step([sys.executable, "-c", "print('ok')"], timeout=30)
        self.assertEqual(res["exit_code"], 0)
        self.assertEqual(res["error_lines"], [])

    def test_run_step_collects_error_lines_on_failure(self):
        """F-R4-01：失败时按编译/链接错误关键词过滤收集，warning 不收、保序。"""
        script = (
            "import sys\n"
            "print('foo.cpp(12): warning C4996: deprecation')\n"
            "print('foo.cpp(30): error C2065: undeclared identifier')\n"
            "print('  Link: warning LNK4098 clash')\n"
            "print('bar.obj : error LNK2019: unresolved external symbol')\n"
            "print('C:/x/include/z.h(5): fatal error C1083: cannot open file')\n"
            "print('MSBUILD : error MSB1009: project file missing')\n"
            "print('LINK : fatal error LNK1104: cannot open file')\n"
            "print('plain stderr noise line')\n"
            "raise SystemExit(1)\n"
        )
        res = DRV.run_step([sys.executable, "-c", script], timeout=30)
        self.assertEqual(res["exit_code"], 1)
        errs = res["error_lines"]
        self.assertEqual(len(errs), 5, errs)
        self.assertIn("error C2065", errs[0])
        self.assertIn("error LNK2019", errs[1])
        self.assertIn("fatal error C1083", errs[2])
        self.assertIn("error MSB1009", errs[3])
        self.assertIn("LINK : fatal error LNK1104", errs[4])
        # warning 行与噪声行不进入 error_lines
        self.assertFalse(any("warning" in ln and "error" not in ln.lower()
                             for ln in errs), errs)
        self.assertFalse(any("plain stderr noise" in ln for ln in errs))
        # output_tail 语义不变：仍保留最后 25 行（含全部 9 行输出）
        self.assertIn("warning C4996", res["output_tail"])
        self.assertIn("plain stderr noise line", res["output_tail"])

    def test_run_step_error_lines_cap_50(self):
        """F-R4-01：error 行超 50 行时截断至 cap，保序取前 50 行。"""
        n = 60
        script = "\n".join(f"print('e{i}: error C2001: boom keep-order {i}')"
                           for i in range(n))
        res = DRV.run_step([sys.executable, "-c",
                            script + "\nraise SystemExit(2)"], timeout=30)
        self.assertEqual(res["exit_code"], 2)
        self.assertEqual(len(res["error_lines"]), DRV.ERROR_LINES_CAP)
        self.assertIn("keep-order 0", res["error_lines"][0])
        self.assertIn("keep-order 49", res["error_lines"][-1])

    def test_collect_error_lines_pure_function(self):
        """collect_error_lines 纯函数：保序过滤、cap、空输入。"""
        self.assertEqual(DRV.collect_error_lines([]), [])
        lines = ["x", "a : error C1234: bad", "y", "b : fatal error nope"]
        self.assertEqual(DRV.collect_error_lines(lines), lines[1::2])

    def test_build_dir_from_preset_single_source(self):
        self.assertEqual(DRV.build_dir_from_preset("win-msvc-17.14.39-x64"),
                         "build/win-msvc-17.14.39-x64")
        # 未知/不可读 preset 回退默认，不抛异常
        self.assertEqual(DRV.build_dir_from_preset("no-such-preset"),
                         DRV.DEFAULT_BUILD_DIR)

    def test_expected_artifacts_derived_from_contract_no_lib_prefix(self):
        arts = DRV.expected_windows_artifacts()
        for rel in ("astrocs.exe", "astrocs_runtime.dll", "astrocs_io.dll",
                    "modules/astrocs_noop.dll",
                    "providers/astrocs_cpu_baseline.dll"):
            self.assertIn(rel, arts)
        self.assertFalse([a for a in arts if a.startswith("lib")],
                         "Windows DLL 不得带 lib 前缀")
        self.assertIn("astrocs.product.json", arts)


class TestPackageAssembly(unittest.TestCase):
    """package 组装：tmp 伪 install 树真跑 build_candidate（三清单+排除规则）。"""

    @classmethod
    def setUpClass(cls):
        cls._tmp = tempfile.TemporaryDirectory()
        root = Path(cls._tmp.name)
        cls.repo = H.make_repo(root / "repo")   # git 仓库：ls-files / HEAD 可用
        cand = root / "candidate"
        for rel in DRV.expected_windows_artifacts():
            p = cand / rel
            p.parent.mkdir(parents=True, exist_ok=True)
            p.write_bytes(b"\x00ASTROCS-STUB\n")
        # 违禁物：build cache / CTest 日志 / 源码 / 测试数据目录
        (cand / "CMakeCache.txt").write_text("cache\n")
        (cand / "CMakeFiles" / "x.dir" / "bar.obj").parent.mkdir(parents=True)
        (cand / "CMakeFiles" / "x.dir" / "bar.obj").write_bytes(b"obj")
        (cand / "Testing" / "last.log").parent.mkdir(parents=True)
        (cand / "Testing" / "last.log").write_text("ctest log\n")
        (cand / "leaked.c").write_text("int f(void){return 0;}\n")
        (cand / "leaked.h").write_text("#pragma once\n")
        (cand / "helper.py").write_text("print('no')\n")
        (cand / "testdata" / "case1.bin").parent.mkdir(parents=True)
        (cand / "testdata" / "case1.bin").write_bytes(b"\x01")
        cls.cand = cand
        cls.result = DRV.build_candidate(cand, source_repo=cls.repo,
                                         cmake_ver="4.1.1-test")
        cls.verification = cls.result["verification"]

    @classmethod
    def tearDownClass(cls):
        cls._tmp.cleanup()

    def test_three_manifests_written_with_expected_content(self):
        prov = json.loads((self.cand / "BUILD_PROVENANCE.json").read_text(encoding="utf-8"))
        head = H.sh(["git", "rev-parse", "HEAD"], cwd=self.repo).stdout.strip()
        self.assertEqual(prov["source_sha"], head)
        self.assertEqual(prov["preset"]["configure"], DRV.DEFAULT_PRESET)
        self.assertEqual(prov["toolchain"]["cmake_version"], "4.1.1-test")
        self.assertIn("host", prov)          # host OS 探测（非硬编码）
        self.assertIn("built_utc", prov)
        self.assertFalse(prov["acr_enabled"])
        src = json.loads((self.cand / "SOURCE_MANIFEST.json").read_text(encoding="utf-8"))
        listed = {e["path"] for e in src["files"]}
        self.assertEqual(listed, {"A.txt", "B.txt"})   # git ls-files 全集
        self.assertEqual(src["file_count"], 2)
        self.assertTrue(all(len(e["sha256"]) == 64 for e in src["files"]))
        sums = (self.cand / "SHA256SUMS").read_text(encoding="utf-8").splitlines()
        files = [p.relative_to(self.cand).as_posix()
                 for p in self.cand.rglob("*") if p.is_file()]
        self.assertEqual(len(sums), len(files) - 1)    # 不含 SHA256SUMS 自身
        for line in sums:
            digest, rel = line.split("  ", 1)
            self.assertRegex(digest, r"^[0-9a-f]{64}$")
            self.assertIn(rel, files)

    def test_exclude_rules_remove_cache_sources_testdata(self):
        pruned = set(self.result["pruned_by_exclude_rules"])
        for bad in ("CMakeCache.txt", "leaked.c", "leaked.h", "helper.py",
                    "Testing/last.log", "testdata/case1.bin"):
            self.assertIn(bad, pruned, bad)
            self.assertFalse((self.cand / bad).exists(), bad)
        self.assertFalse((self.cand / "CMakeFiles" / "x.dir").exists())
        leftovers = [p.relative_to(self.cand).as_posix()
                     for p in self.cand.rglob("*")
                     if p.is_file() and p.suffix in (".c", ".h", ".py")]
        self.assertEqual(leftovers, [], "candidate 内不得残留源文件")

    def test_source_manifest_lists_only_never_copies(self):
        listed = {e["path"] for e in json.loads(
            (self.cand / "SOURCE_MANIFEST.json").read_text(encoding="utf-8"))["files"]}
        for rel in listed:
            self.assertFalse((self.cand / rel).exists(),
                             f"源文件被复制进 candidate：{rel}")

    def test_verify_candidate_hosted_only_on_linux(self):
        self.assertTrue(self.verification["executed"] is False or
                        sys.platform.startswith("win"))
        if sys.platform.startswith("win"):
            self.skipTest("仅非 Windows 宿主断言 hosted-only 语义")
        for chk in self.verification["checks"]:
            self.assertIsNone(chk["verdict"], chk["item"])
            self.assertFalse(chk["executed"], chk["item"])
        blob = json.dumps(self.verification, ensure_ascii=False)
        self.assertIn("hosted", blob)


class TestPlatformGating(unittest.TestCase):
    """platform 门控：probe 语义与 CI-001 收紧后的 FAIL(prerequisite)（不可 waiver）。"""

    def test_probe_prerequisite_platform_mismatch_and_windows_pass(self):
        from ci import run as ci_run
        check = _WIN_CHECKS["WIN-BUILD-RELEASE"]
        # 工具在位时：platform 门控是唯一失败原因（platform_mismatch）
        with mock.patch.object(ci_run.shutil, "which", return_value="/usr/bin/fake"):
            ok, reason = ci_run.probe_prerequisite(check, _REPO, "linux")
            self.assertFalse(ok)
            self.assertIn("platform_mismatch", reason)
            ok, reason = ci_run.probe_prerequisite(check, _REPO, "windows")
        self.assertTrue(ok, reason)
        self.assertIsNone(reason)

    def test_runner_skips_windows_checks_on_linux(self):
        if sys.platform.startswith("win"):
            self.skipTest("仅非 Windows 宿主验证平台跳过")
        # CI-001 收紧：WIN-* 不可 waiver → platform_mismatch 不再 SKIPPED，
        # 而是 FAIL(prerequisite)（fail-closed；跨平台漏跑必须显性失败）。
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            repo = H.make_repo(root / "repo")
            (repo / "ci").mkdir(exist_ok=True)
            (repo / "tools" / "quality").mkdir(parents=True, exist_ok=True)
            # probe 需要的仓库内脚本副本（不执行，仅存在性探测）
            shutil.copyfile(_REPO / "ci" / "resource_monitor.py",
                            repo / "ci" / "resource_monitor.py")
            shutil.copyfile(_REPO / "tools" / "quality" / "ci_windows_driver.py",
                            repo / "tools" / "quality" / "ci_windows_driver.py")
            H.write_registry(repo, [_WIN_CHECKS[c] for c in _WIN_IDS])
            H.write_ci_result_schema(repo)
            # 假 cmake 进入 PATH：prerequisite_tools 通过，probe 唯一失败原因
            # 收敛为 platform_mismatch（which 只探测存在性，假脚本不会被执行）
            fake_bin = root / "bin"
            fake_bin.mkdir()
            # F-R2-06 接入侧：windows shutil.which 依赖 PATHEXT，无扩展名
            # 假脚本对 which("cmake") 不可见 → prerequisite 误判缺失；按
            # 平台写 <tool>.exe（posix 维持无扩展名 stub，行为不变）
            for tool in ("cmake", "dumpbin"):
                fake = fake_bin / (f"{tool}.exe" if os.name == "nt" else tool)
                fake.write_text("#!/bin/sh\nexit 1\n", encoding="utf-8")
                fake.chmod(0o755)
            env = dict(os.environ, PATH=f"{fake_bin}{os.pathsep}{os.environ['PATH']}")
            out = root / "out"
            proc = subprocess.run(
                [sys.executable, str(H.RUNNER), "--repo-root", str(repo),
                 "--profile", "windows-main", "--output-root", str(out)],
                cwd=str(repo), capture_output=True, text=True,
                encoding="utf-8", errors="replace",  # F-R2-06 接入侧：cp1252 不脆断
                timeout=150, env=env)
            per = [H.load_check_result(out, cid) for cid in _WIN_IDS]
            for entry in per:
                self.assertEqual(entry["verdict"], "FAIL(prerequisite)", entry["id"])
                self.assertIn("platform_mismatch", entry.get("reason", ""), entry["id"])
                self.assertFalse(entry["waivable"], entry["id"])
            # runner 汇总：非 waivable 硬失败 → verdict=FAIL、exit 1
            ci = H.load_ci_result(out)
            self.assertEqual(ci["verdict"], "FAIL")
            self.assertEqual(ci["summary"]["fail_detail"].get("FAIL(prerequisite)"), 3)
            self.assertEqual(proc.returncode, 1)
            self.assertEqual(ci["summary"]["skipped_waivable"], 0,
                             "CI-001 收紧后 platform_mismatch 不再计入 skipped_waivable")


if __name__ == "__main__":
    unittest.main()
