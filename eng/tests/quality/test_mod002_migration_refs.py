#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""MOD-002 迁移后引用刷新：门「能红能绿」双向证据测试（正例 + 负例）。

零仓库副作用：负例在 run/ 下的临时镜像树里执行——镜像树用顶层层级 symlink 指向真实
仓库，只把「被测检查器 + 被注入的生产源」换成真实副本；测试结束整棵镜像树删除。

覆盖：
  T1 历史键自证防线：MODULE_MAP 的 23 条 legacy_paths 都不得等于/落在自己的
     target_dir 之下（迁移清单外的同名历史面除外，见 NOT_MIGRATED）。
  T2 check_module_map 注入探测：把 legacy_paths 写回现存 target_dir ⇒ 必被观测为
     legacy_paths_present（MODULE_MAP.yaml 头注第 21-22 行的「历史键不得自证」），
     且该 finding 是 **NOTE 级**（检查器 FINDING_SEVERITY 表；头注第 16 行「任一 FAIL 级
     finding → exit 1」），故 rc 不得因它变化。断言只认「本用例注入项 + 本用例 rc 差」，
     不认仓库里恰好存在的其它红灯（曾因此长期假绿）。
  T3 eng/tools/check_module_readmes.py 正例 rc=0 / 负例（README 路径指向不存在文件）rc=1。
  T4 eng/tools/check_warning_suppression.py 正例 rc=0 / 负例（生产源注入 -w 抑制）rc=1。
     ⚠ ARCH-001 前该检查器读旧路径全空 ⇒ static_scan 恒空跑（假绿）；本测试钉死
     「注入必红」，防止再次退化成空跑。
  T5 docs/modules、docs/contracts 除 MODULE_MAP 的 legacy_paths/note 历史键外，
     不得再出现迁移前 lib 路径。

依据：ENGINEERING_SPEC.md §8（每项检查有正例与负例，能红能绿）；MOD-002 任务卡
「检查器/测试改动必须给正例 + 负例（能红能绿）」。
"""
from __future__ import annotations

import importlib.util
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

REPO = pathlib.Path(__file__).resolve().parents[3]
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
    """在 run/... 下造一棵镜像树（真目录 + 文件级 symlink；eng/tools/ 为真实副本）。"""
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
    shutil.rmtree(root / "eng" / "tools")
    shutil.copytree(REPO / "eng" / "tools", root / "eng" / "tools", symlinks=False)
    return root


class TestModuleMapLegacyPathsCannotSelfCertify(unittest.TestCase):
    """T1/T2：legacy_paths 必须是历史键，不得指向现存 target_dir。"""

    @classmethod
    def setUpClass(cls):
        cls.doc = yaml.safe_load((REPO / "docs/modules/MODULE_MAP.yaml").read_text(encoding="utf-8"))

    @staticmethod
    def _fixture_module():
        """按路径加载检查器的合成 fixture 构造器（不依赖包结构/__init__.py）。"""
        path = REPO / "eng/tools/quality/fixtures/module_map_fixture.py"
        spec = importlib.util.spec_from_file_location("mod002_module_map_fixture", path)
        mod = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(mod)
        return mod

    @staticmethod
    def _build_fixture(root: pathlib.Path) -> pathlib.Path:
        """合成仓库：正例（7 项面全部物化、缺口表为空）。

        用合成树而不是真实仓库做注入，是为了让「本用例的判据」与仓库里其它面的红绿
        完全解耦——这正是本文件此前假绿的根因（见 t02 docstring）。
        """
        return TestModuleMapLegacyPathsCannotSelfCertify._fixture_module().build_repo(root)

    @staticmethod
    def _run_checker(root: pathlib.Path, out: pathlib.Path):
        """跑检查器，返回 (rc, json)；json 是「本用例专属」的可观测量来源。"""
        proc = run([PY, "eng/tools/quality/check_module_map.py",
                    "--repo-root", str(root), "--json-out", str(out), "--quiet"])
        return proc.returncode, json.loads(out.read_text(encoding="utf-8"))

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

    def test_t02_self_referential_legacy_path_is_observed_but_never_fails(self):
        """注入：legacy_paths 写回现存 target_dir ⇒ 必须被观测（NOTE），且不得抬 rc。

        守的是「历史键不得退化成自证」这条**可观测性**不变量 + 「该 finding 属提示级、
        不是失败级」这条**严重度**不变量（MODULE_MAP.yaml 头注第 16/21-22 行；
        check_module_map.py 的 FINDING_SEVERITY 把 legacy_paths_present 与
        not_verified / schema_link_glob_unverifiable 同列为 NOTE）。

        为什么不再断言 rc==1：rc 是**聚合量**——任何与本注入无关的 FAIL 级 finding 都能
        把它抬到 1，于是「rc==1」既会被无关红灯假绿（曾长期如此），也无法证明本注入被看见。
        这里只认两个本用例专属的可观测量：注入项本身、以及注入前后的 rc / FAIL 级条数差。
        """
        with tempfile.TemporaryDirectory(prefix="mod002-selfref-") as td:
            tmp = pathlib.Path(td)
            root = self._build_fixture(tmp / "base")
            base_rc, base = self._run_checker(root, tmp / "base.json")
            self.assertNotIn("legacy_paths_present",
                             {f["code"] for f in base["findings"]},
                             "夹具基线不得已带自指 legacy 登记")

            root2 = self._build_fixture(tmp / "inject")
            mp = root2 / "docs/modules/MODULE_MAP.yaml"
            doc = yaml.safe_load(mp.read_text(encoding="utf-8"))
            m0 = doc["modules"][0]
            self.assertIn("legacy_paths", m0, "注入锚失效：映射表首行无 legacy_paths 键")
            m0["legacy_paths"] = [str(m0["target_dir"])]   # 自指：历史键退化为自证
            mp.write_text(yaml.safe_dump(doc, allow_unicode=True, sort_keys=False),
                          encoding="utf-8")
            inj_rc, injected = self._run_checker(root2, tmp / "inject.json")

            hits = [f for f in injected["findings"] if f["code"] == "legacy_paths_present"]
            self.assertEqual([(h["module"], h["severity"]) for h in hits],
                             [(str(m0["id"]), "NOTE")],
                             "把现存 target_dir 写进 legacy_paths 必须被观测为 NOTE 级 finding；"
                             "rc=%s" % inj_rc)
            self.assertEqual(inj_rc, base_rc,
                             "legacy_paths_present 是 NOTE 级（不参与 fail-closed），"
                             "注入不得改变 rc")
            self.assertEqual(injected["summary"]["fail_findings"],
                             base["summary"]["fail_findings"],
                             "注入自指 legacy 路径不得新增 FAIL 级 finding")

    def test_t03_real_ledger_and_checker_stay_honest(self):
        """真实台账 + 检查器必须仍然「诚实且判据活着」——不许为绿放宽，也不靠别人的红灯。

        原断言是 fail_findings > 0（「真实仓库仍应是红灯」）：那不是不变量而是**状态快照**，
        任何一处合法修复都会让它失效；更糟的是它把「与本用例无关的其它红灯」当护栏
        （那条陈旧登记一旦合法清除，本用例立刻转红——而它守的东西从来没被验证过）。

        本用例改守四条**与仓库红绿无关**的不变量，外加两条判别力负例（注入必红）：
          A 合成正例可绿（判据可达，不是靠放宽换来的）；
          B 注入「未登记假路径」/「陈旧能力登记」⇒ 必须判红并报出对应 FAIL（判别力证明）；
          C 真实台账棘轮：declared_absent_paths 登记的缺口今天必须仍然缺失、
            且 legacy_paths 一律不在树中（历史键不得退化成自证）；
          D 机器报告计数自洽：summary 各计数 == findings 数组实际分布，
            台账条数 == registered_gaps == GAP 级条数，且 GAP 条目全部来自两份台账；
          E 真实仓库无假路径、无陈旧登记（有则检查器必然判红，不存在「静默转绿」）。
        """
        with tempfile.TemporaryDirectory(prefix="mod002-map-real-") as td:
            tmp = pathlib.Path(td)
            real_rc, data = self._run_checker(REPO, tmp / "real.json")

            # --- A 合成正例可绿（判据可达） ---
            pos_rc, pos = self._run_checker(self._build_fixture(tmp / "pos"), tmp / "pos.json")
            self.assertEqual(pos_rc, 0,
                             "合成正例必须绿（判据可达性）：%s" % pos["summary"])
            self.assertEqual(pos["summary"]["fail_findings"], 0)

            # --- B 判别力负例：注入未登记的假路径 ⇒ 必红 ---
            root = self._build_fixture(tmp / "fakepath")
            mp = root / "docs/modules/MODULE_MAP.yaml"
            doc = yaml.safe_load(mp.read_text(encoding="utf-8"))
            m0 = doc["modules"][0]
            self.assertIn("readme", m0, "注入锚失效：映射表首行无 readme 键")
            m0["readme"] = str(m0["target_dir"]) + "/README_GONE_FOR_NEGATIVE_TEST.md"
            mp.write_text(yaml.safe_dump(doc, allow_unicode=True, sort_keys=False),
                          encoding="utf-8")
            bad_rc, bad = self._run_checker(root, tmp / "fakepath.json")
            self.assertEqual(bad_rc, 1,
                             "未登记的假路径必须判红（判据被放宽 = 假绿）")
            self.assertEqual(
                [(f["module"], f["code"]) for f in bad["findings"] if f["severity"] == "FAIL"],
                [(str(m0["id"]), "fake_path")],
                "未登记假路径必须报 FAIL(fake_path)")

            # --- B2 判别力负例：陈旧能力登记（登记了却不再出现）⇒ 必红 ---
            stale = self._fixture_module().build_repo(tmp / "capstale",
                                                      mutation="capability_stale")
            stale_rc, stale_data = self._run_checker(stale, tmp / "capstale.json")
            self.assertEqual(stale_rc, 1, "陈旧缺口登记必须判红（棘轮反向自证）")
            self.assertIn("stale_declared_absent_capability",
                          {f["code"] for f in stale_data["findings"] if f["severity"] == "FAIL"},
                          "陈旧能力登记必须报 FAIL(stale_declared_absent_capability)")

            # --- C 真实台账棘轮（不依赖仓库其它面的红绿） ---
            cmake = "\n".join(
                p.read_text(encoding="utf-8", errors="ignore")
                for p in REPO.rglob("CMakeLists.txt") if "build" not in p.parts)
            for it in (self.doc.get("declared_absent_paths") or {}).get("items") or []:
                if it["key"] == "target":
                    self.assertNotRegex(
                        cmake, r"add_(?:library|executable)\s*\(\s*%s\b" % re.escape(str(it["path"])),
                        "target %s 已定义：缺口落地后必须删登记（棘轮）" % it["path"])
                else:
                    self.assertFalse((REPO / str(it["path"])).exists(),
                                     "%s 已存在：缺口落地后必须删登记（棘轮）" % it["path"])
            self.assertEqual(
                [(m["id"], p) for m in self.doc["modules"]
                 for p in (m.get("legacy_paths") or []) if (REPO / str(p)).exists()],
                [], "legacy_paths 里出现树中现存路径：历史键退化为自证")

            # --- D 机器报告计数自洽（检查器不得静默丢 finding 或错报计数） ---
            summary = data["summary"]
            self.assertEqual(summary["modules_total"], 23)
            self.assertEqual(summary["ids_unique"], 23)
            self.assertEqual(summary["index_ids_total"], 23)
            self.assertEqual(summary["modules_total"], len(data["modules"]))
            fails = [f for f in data["findings"] if f["severity"] == "FAIL"]
            gaps = [f for f in data["findings"] if f["severity"] == "GAP"]
            notes = [f for f in data["findings"] if f["severity"] == "NOTE"]
            self.assertEqual(summary["fail_findings"], len(fails))
            self.assertEqual(summary["gap_findings"], len(gaps))
            self.assertEqual(summary["note_findings"], len(notes))
            self.assertEqual(summary["fail_findings"] + summary["gap_findings"]
                             + summary["note_findings"], len(data["findings"]))
            self.assertEqual(summary["verdict"], "FAIL" if fails else "PASS")
            self.assertEqual(real_rc, 1 if fails else 0, "rc 必须与 FAIL 级 finding 一致")
            for f in data["findings"]:
                for key in ("module", "code", "detail", "severity"):
                    self.assertIn(key, f, "finding 缺字段 %s：%r" % (key, f))
            self.assertEqual(
                summary["fake_paths"],
                len([f for f in fails if f["code"] in ("fake_path", "fake_target")]))
            paths = (self.doc.get("declared_absent_paths") or {}).get("items") or []
            self.assertEqual(len(paths), data["gaps"]["registered"])
            self.assertEqual(summary["registered_gaps"], len(paths))
            # GAP 级 finding 只允许来自两份台账登记，且每条都能追到台账条目
            # （能力缺口可能同时命中「该模块实报」与「登记了但未命中」两条，故用
            #  ≥ 而非 ==，并要求逐条可追踪，避免把实现细节当不变量锁死）。
            caps = (self.doc.get("declared_absent_capabilities") or {}).get("items") or []
            cap_codes = {str(i["code"]) for i in caps}
            cap_keys = {(str(i["module"]), str(i["code"])) for i in caps}
            for f in gaps:
                code = str(f["code"])
                if code == "declared_absent_registered":
                    continue      # 路径/target 缺口（已登记）
                if code == "declared_absent_capability_registered":
                    continue      # 登记了但本次未命中的能力缺口（可见计数）
                self.assertIn(code, cap_codes,
                              "GAP 级 finding 的 code 不在能力缺口台账中：%r" % f)
                self.assertIn((str(f["module"]), code), cap_keys,
                              "GAP 级 finding 未逐条命中能力缺口台账：%r" % f)

            # --- E 真实仓库无假路径、无陈旧登记（诚实性红线） ---
            self.assertEqual(summary["fake_paths"], 0, "真实仓库不得有未登记的假路径")
            self.assertEqual(
                [f["code"] for f in fails
                 if f["code"] in ("stale_declared_absent", "stale_declared_absent_capability")],
                [], "真实台账不得有陈旧缺口登记（登记必须随落地同步删除）")


class TestModuleReadmeCheckerCanRedAndGreen(unittest.TestCase):
    """T3：迁移路径订正后，README 门在正例 rc=0、缺文件负例 rc=1。"""

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(PROBE_ROOT, ignore_errors=True)

    def test_t10_positive_real_repo_green(self):
        proc = run([PY, "eng/tools/check_module_readmes.py"])
        self.assertEqual(proc.returncode, 0, proc.stdout + proc.stderr)
        self.assertIn("DOC-003_PASS", proc.stdout)

    def test_t11_negative_missing_readme_is_red(self):
        mirror = make_mirror("readme")
        p = mirror / "eng/tools/check_module_readmes.py"
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

    # B（`wrapper_phase1/noise_model.cpp`）已按 EXP-206 定案退役、文件在 HEAD 已删除
    # （GAP_AUDIT §5.2「生产唯一实现 = A」）⇒ 负例注入目标改用**同目录仍存活**的生产源；
    # 退役文件另行断言「不存在」（原断言「该文件存在」已失真）。
    RETIRED = "lib/algorithms/noise_snr/wrapper_phase1/noise_model.cpp"
    TARGET = "lib/algorithms/noise_snr/wrapper_phase1/snr_frame_science.cpp"

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(PROBE_ROOT, ignore_errors=True)

    def test_t20_positive_real_repo_green(self):
        proc = run([PY, "eng/tools/check_warning_suppression.py"])
        self.assertEqual(proc.returncode, 0, proc.stdout + proc.stderr)

    def test_t21_negative_injected_suppression_is_red(self):
        """镜像树里把生产源换成含 -w 的副本（真实仓库零改动）。"""
        mirror = make_mirror("warn")
        src_real = REPO / self.TARGET
        # B 已按 EXP-206 定案退役：该路径**不得再被 Git 跟踪**（HEAD 1fc88989 已删）。
        # 不变量取「未被跟踪」而非「工作树不存在」——untracked 的 0 字节同名文件由域外
        # `eng/tools/check_warning_suppression.py:104` 的 `rel_src.touch()` 产生（DOC-205 回执已登记）。
        tracked = subprocess.run(["git", "ls-files", "--error-unmatch", self.RETIRED],
                                 cwd=str(REPO), capture_output=True, text=True)
        self.assertNotEqual(tracked.returncode, 0,
                            "B 已按 EXP-206 退役，不得再被 Git 跟踪：%s" % self.RETIRED)
        self.assertTrue(src_real.is_file(), "负例目标不存在：%s" % self.TARGET)
        target = mirror / self.TARGET
        target.unlink()                                  # 只断开镜像树里的文件级 symlink
        self.assertFalse(target.exists())
        target.write_text("// MOD-002 negative probe -w" + chr(10)
                          + src_real.read_text(encoding="utf-8"), encoding="utf-8")
        self.assertTrue(src_real.is_file(), "真实仓库源文件必须仍在（负例不得写穿镜像树）")
        checker = mirror / "eng/tools/check_warning_suppression.py"
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
