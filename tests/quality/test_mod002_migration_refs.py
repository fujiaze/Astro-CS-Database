#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""MOD-002 迁移后引用刷新：门「能红能绿」双向证据测试（正例 + 负例）。

零仓库副作用：负例在 run/ 下的临时镜像树里执行——镜像树用顶层层级 symlink 指向真实
仓库，只把「被测检查器 + 被注入的生产源」换成真实副本；测试结束整棵镜像树删除。

覆盖：
  T1 历史键自证防线：MODULE_MAP 的 23 条 legacy_paths 都不得等于/落在自己的
     target_dir 之下（迁移清单外的同名历史面除外，见 NOT_MIGRATED）。
  T2 check_module_map 负例：把 legacy_paths 写回现存 target_dir ⇒ 必报
     legacy_paths_present；正例：真实仓库 23/23 且真实 FAIL 数 > 0（不为绿放宽）。
  T3 tools/check_module_readmes.py 正例 rc=0 / 负例（README 路径指向不存在文件）rc=1。
  T4 tools/check_warning_suppression.py 正例 rc=0 / 负例（生产源注入 -w 抑制）rc=1。
     ⚠ ARCH-001 前该检查器读旧路径全空 ⇒ static_scan 恒空跑（假绿）；本测试钉死
     「注入必红」，防止再次退化成空跑。
  T5 docs/modules、docs/contracts 除 MODULE_MAP 的 legacy_paths/note 历史键外，
     不得再出现迁移前 lib 路径。

依据：ENGINEERING_SPEC.md §8（每项检查有正例与负例，能红能绿）；MOD-002 任务卡
「检查器/测试改动必须给正例 + 负例（能红能绿）」。
"""
from __future__ import annotations

import json
import os
import pathlib
import re
import shutil
import subprocess
import sys
import tempfile
import unittest

import yaml

REPO = pathlib.Path(__file__).resolve().parents[2]
PY = sys.executable
PROBE_ROOT = REPO / "run" / "PROJECT-GOVERNANCE-01" / "MOD-002" / "negprobe"

OLD_LIB_DIRS = [
    "lib/calibration", "lib/cosmetic", "lib/star_detector", "lib/dynamic_psf",
    "lib/plate_solve", "lib/photometric_calib", "lib/snr_estimator",
    "lib/phase2_rej", "lib/phase2_samp", "lib/phase2_upm", "lib/phase2_int",
    "lib/phase3_proj", "lib/phase3_rsmp", "lib/phase3_fits",
    "lib/astro_image_io", "lib/orchestrator", "lib/backend_host",
    "lib/gaia_xpsd_client", "lib/common", "lib/drizzle", "lib/hips",
    "lib/phase2", "lib/acr", "lib/hips_p2", "lib/core", "lib/healpix_db",
]
# 不在 ARCH-001 迁移清单内、树中仍存在的历史面（不得要求其消失/改名）
NOT_MIGRATED = {"cli", "lib/infrastructure/gaia_xpsd_client"}
BOUND = r"(?![A-Za-z0-9_])"
SKIP_DIRS = {"build", "run", ".git", "artifacts", "evidence", "logs"}


def run(cmd, cwd=REPO, timeout=300):
    return subprocess.run(cmd, cwd=str(cwd), capture_output=True, text=True, timeout=timeout)


def _link_tree(src: pathlib.Path, dst: pathlib.Path) -> None:
    """递归造层级镜像：文件用 symlink 指向真实文件，目录是真目录。

    必须逐层建真目录而不是整目录 symlink——否则往镜像树里写文件会顺着 symlink
    落在真实仓库（曾把 lib/.../noise_model.cpp 覆写/删除）。文件级 symlink 只在
    镜像树内被替换时才断开，绝不写穿到真实仓库。
    """
    dst.mkdir(parents=True, exist_ok=True)
    for entry in sorted(src.iterdir()):
        if entry.name in SKIP_DIRS:
            continue
        target = dst / entry.name
        if entry.is_dir():
            _link_tree(entry, target)
        else:
            os.symlink(entry, target)


def make_mirror(name: str) -> pathlib.Path:
    """在 run/... 下造一棵镜像树（真目录 + 文件级 symlink；tools/ 为真实副本）。"""
    root = PROBE_ROOT / name
    if root.exists():
        shutil.rmtree(root)
    root.mkdir(parents=True)
    for entry in sorted(REPO.iterdir()):
        if entry.name in SKIP_DIRS:
            continue
        dst = root / entry.name
        if entry.is_dir():
            _link_tree(entry, dst)
        else:
            os.symlink(entry, dst)
    shutil.rmtree(root / "tools")
    shutil.copytree(REPO / "tools", root / "tools", symlinks=False)
    return root


class TestModuleMapLegacyPathsCannotSelfCertify(unittest.TestCase):
    """T1/T2：legacy_paths 必须是历史键，不得指向现存 target_dir。"""

    @classmethod
    def setUpClass(cls):
        cls.doc = yaml.safe_load((REPO / "docs/modules/MODULE_MAP.yaml").read_text(encoding="utf-8"))

    def test_t01_legacy_paths_never_inside_own_target_dir(self):
        self.assertEqual(len(self.doc["modules"]), 23)
        bad = []
        for m in self.doc["modules"]:
            td = str(m["target_dir"]).rstrip("/")
            for p in (m.get("legacy_paths") or []):
                p = str(p).rstrip("/")
                if p in NOT_MIGRATED:
                    continue
                if p == td or p.startswith(td + "/"):
                    bad.append((m["id"], p, td))
        self.assertEqual(bad, [], "legacy_paths 落在自身 target_dir 内（历史键退化为自证）：%s" % bad)

    def test_t01b_legacy_paths_are_pre_migration_paths(self):
        """迁移清单内的 legacy 旧目录必须已不在树中；清单外的历史面允许仍存在。"""
        bad = []
        for m in self.doc["modules"]:
            for p in (m.get("legacy_paths") or []):
                if str(p) in NOT_MIGRATED:
                    continue
                if (REPO / str(p)).exists():
                    bad.append((m["id"], p))
        self.assertEqual(bad, [], "迁移清单内的 legacy 路径仍存在（应已迁走）：%s" % bad)

    def test_t02_restoring_self_referential_legacy_path_is_reported(self):
        """负例：legacy_paths 写回现存 target_dir ⇒ 门必须报 legacy_paths_present。"""
        with tempfile.TemporaryDirectory(prefix="mod002-map-") as td:
            tmp = pathlib.Path(td)
            src = (REPO / "docs/modules/MODULE_MAP.yaml").read_text(encoding="utf-8")
            mutated = src.replace("    legacy_paths: [lib/calibration]",
                                  "    legacy_paths: [lib/algorithms/calibration]", 1)
            self.assertNotEqual(src, mutated, "变异未生效：映射表缺 calibration legacy_paths 锚")
            bad_map = tmp / "MODULE_MAP.yaml"
            bad_map.write_text(mutated, encoding="utf-8")
            out = tmp / "bad.json"
            proc = run([PY, "tools/quality/check_module_map.py",
                        "--repo-root", str(REPO), "--map", str(bad_map),
                        "--json-out", str(out), "--quiet"])
            data = json.loads(out.read_text(encoding="utf-8"))
            codes = {f["code"] for f in data["findings"] if f["module"] == "calibration"}
            self.assertEqual(proc.returncode, 1, proc.stdout + proc.stderr)
            self.assertIn("legacy_paths_present", codes,
                          "把现存 target_dir 写进 legacy_paths 必须被报出")

    def test_t03_real_repo_map_still_reports_honestly(self):
        """正例：真实仓库 + 真实映射表可跑通，23/23 与真实 FAIL 数如实给出。"""
        with tempfile.TemporaryDirectory(prefix="mod002-map-real-") as td:
            out = pathlib.Path(td) / "real.json"
            proc = run([PY, "tools/quality/check_module_map.py",
                        "--json-out", str(out), "--quiet"])
            data = json.loads(out.read_text(encoding="utf-8"))
            self.assertEqual(data["summary"]["modules_total"], 23)
            self.assertEqual(data["summary"]["ids_unique"], 23)
            self.assertGreater(data["summary"]["fail_findings"], 0,
                               "真实仓库仍应是红灯（不得为绿放宽判据）")
            legacy_notes = [f for f in data["findings"] if f["code"] == "legacy_paths_present"]
            self.assertLessEqual(len(legacy_notes), 2)
            self.assertEqual(proc.returncode, 1, proc.stdout + proc.stderr)


class TestModuleReadmeCheckerCanRedAndGreen(unittest.TestCase):
    """T3：迁移路径订正后，README 门在正例 rc=0、缺文件负例 rc=1。"""

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(PROBE_ROOT, ignore_errors=True)

    def test_t10_positive_real_repo_green(self):
        proc = run([PY, "tools/check_module_readmes.py"])
        self.assertEqual(proc.returncode, 0, proc.stdout + proc.stderr)
        self.assertIn("DOC-003_PASS", proc.stdout)

    def test_t11_negative_missing_readme_is_red(self):
        mirror = make_mirror("readme")
        p = mirror / "tools/check_module_readmes.py"
        src = p.read_text(encoding="utf-8")
        mutated = src.replace(
            "lib/algorithms/noise_snr/wrapper_phase1/README.md",
            "lib/algorithms/noise_snr/wrapper_phase1/README_MISSING_FOR_NEGATIVE_TEST.md", 1)
        self.assertNotEqual(src, mutated, "变异未生效：检查器缺该路径锚")
        p.write_text(mutated, encoding="utf-8")
        proc = run([PY, str(p)], cwd=mirror)
        shutil.rmtree(mirror, ignore_errors=True)
        self.assertEqual(proc.returncode, 1, proc.stdout + proc.stderr)
        self.assertIn("missing", proc.stdout)


class TestWarningSuppressionCheckerCanRedAndGreen(unittest.TestCase):
    """T4：生产源注入 -w 抑制 ⇒ rc=1；真实仓库 rc=0。"""

    TARGET = "lib/algorithms/noise_snr/wrapper_phase1/noise_model.cpp"

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(PROBE_ROOT, ignore_errors=True)

    def test_t20_positive_real_repo_green(self):
        proc = run([PY, "tools/check_warning_suppression.py"])
        self.assertEqual(proc.returncode, 0, proc.stdout + proc.stderr)

    def test_t21_negative_injected_suppression_is_red(self):
        """镜像树里把生产源换成含 -w 的副本（真实仓库零改动）。"""
        mirror = make_mirror("warn")
        src_real = REPO / self.TARGET
        self.assertTrue(src_real.is_file(), "负例目标不存在：%s" % self.TARGET)
        target = mirror / self.TARGET
        target.unlink()                                  # 只断开镜像树里的文件级 symlink
        self.assertFalse(target.exists())
        target.write_text("// MOD-002 negative probe -w" + chr(10)
                          + src_real.read_text(encoding="utf-8"), encoding="utf-8")
        self.assertTrue(src_real.is_file(), "真实仓库源文件必须仍在（负例不得写穿镜像树）")
        checker = mirror / "tools/check_warning_suppression.py"
        proc = run([PY, str(checker)], cwd=mirror)
        shutil.rmtree(mirror, ignore_errors=True)
        self.assertEqual(proc.returncode, 1, proc.stdout + proc.stderr)
        self.assertIn("QA-001_WARN_VIOLATION", proc.stdout)


class TestNoStaleOldPathsInDocOwnedSurface(unittest.TestCase):
    """T5：docs/modules、docs/contracts 不得再有迁移前 lib 路径（MODULE_MAP 历史键除外）。"""

    ROOTS = ("docs/modules", "docs/contracts")
    EXEMPT_FILES = {"docs/modules/MODULE_MAP.yaml"}

    def test_t30_no_old_lib_paths(self):
        hits = []
        for root in self.ROOTS:
            for f in (REPO / root).rglob("*"):
                if not f.is_file() or f.suffix.lower() not in (".md", ".yaml", ".yml", ".json"):
                    continue
                rel = str(f.relative_to(REPO)).replace(os.sep, "/")
                if rel in self.EXEMPT_FILES:
                    continue
                txt = f.read_text(encoding="utf-8", errors="replace")
                for i, ln in enumerate(txt.split(chr(10)), 1):
                    for old in OLD_LIB_DIRS:
                        if re.search(re.escape(old) + BOUND, ln):
                            hits.append("%s:%d [%s]" % (rel, i, old))
                            break
        self.assertEqual(hits, [], "docs/modules|docs/contracts 仍有迁移前 lib 路径：%s" % hits[:5])


if __name__ == "__main__":
    unittest.main()
