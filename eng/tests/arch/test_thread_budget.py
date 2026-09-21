#!/usr/bin/env python3
"""ARCH-004 测试: thread budget 静态 checker 试金石。"""
import importlib.util, os, sys, tempfile, unittest

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
spec = importlib.util.spec_from_file_location("ctb", os.path.join(REPO, "eng", "tools", "arch", "check_thread_budget.py"))
ctb = importlib.util.module_from_spec(spec)
spec.loader.exec_module(ctb)

class TestThreadBudget(unittest.TestCase):
    def test_01_real_repo_passes(self):
        import subprocess
        r = subprocess.run([sys.executable, os.path.join(REPO, "eng", "tools", "arch", "check_thread_budget.py")],
                           capture_output=True, text=True, cwd=REPO, timeout=300)
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
        self.assertIn("未登记线程创建=0 硬编码线程数=0", r.stdout)
        # RT-001 (P0 M5a-G-005): 非空转自证 —— 空转（扫 0 文件/0 登记命中）不得算通过
        self.assertIn("扫描文件数=", r.stdout)
        scanned = int(r.stdout.split("扫描文件数=")[1].split()[0])
        self.assertGreater(scanned, 100, "扫描面为空转: %d 个文件" % scanned)
        self.assertGreater(int(r.stdout.split("已登记命中=")[1].split()[0]), 0,
                           "登记命中为 0 => 登记面与扫描面脱钩")

    def test_02_unregistered_thread_must_fail(self):
        with tempfile.TemporaryDirectory() as td:
            os.makedirs(os.path.join(td, "lib", "x"))
            open(os.path.join(td, "lib", "x", "rogue.cpp"), "w").write(
                "void f(){ std::thread t([](){}); t.join(); }\n")
            old = ctb.SCAN_ROOTS
            ctb.SCAN_ROOTS = [td]
            try:
                errors, reg, _ = ctb.scan()
            finally:
                ctb.SCAN_ROOTS = old
            self.assertTrue(any("rogue.cpp" in e and "未登记" in e for e in errors))

    def test_03_omp_set_unregistered_must_fail(self):
        with tempfile.TemporaryDirectory() as td:
            os.makedirs(os.path.join(td, "lib", "y"))
            open(os.path.join(td, "lib", "y", "bad.cpp"), "w").write(
                "#include <omp.h>\nvoid g(){ omp_set_num_threads(8); }\n")
            old = ctb.SCAN_ROOTS
            ctb.SCAN_ROOTS = [td]
            try:
                errors, reg, _ = ctb.scan()
            finally:
                ctb.SCAN_ROOTS = old
            self.assertTrue(any("bad.cpp" in e and "omp_set_num_threads" in e for e in errors))

    def test_04_registered_rows_carry_annotation(self):
        for k, v in ctb.REGISTERED.items():
            self.assertTrue(v.strip(), f"登记 {k} 必须带整改/豁免注记")

    def test_05_hardcoded_literal_always_fails(self):
        with tempfile.TemporaryDirectory() as td:
            os.makedirs(os.path.join(td, "lib", "z"))
            open(os.path.join(td, "lib", "z", "lit.cpp"), "w").write(
                "#pragma omp parallel for num_threads(16)\nint main(){}\n")
            old = ctb.SCAN_ROOTS
            ctb.SCAN_ROOTS = [td]
            try:
                errors, reg, _ = ctb.scan()
            finally:
                ctb.SCAN_ROOTS = old
            self.assertTrue(any("lit.cpp" in e and "字面量线程数" in e for e in errors))

    # ── ARCH-TB-001 / R-10: 登记「预算注入形态」的负向注入 ──

    MODULE_ENTRY = (
        "lib/algorithms/calibration/src/module_entry.cpp",
        "lib/algorithms/drizzle/src/module_entry.cpp",
        "lib/algorithms/cosmetic/src/module_entry.cpp",
    )

    def test_06_registered_file_still_fails_on_hardcoded_literal(self):
        """负向注入: 已登记文件内出现 num_threads(<数字>) 仍必须 FAIL。

        登记只豁免 omp_set_num_threads 的「未登记」项; hardcoded_num_threads 独立扫描
        不得被登记放宽 (R-10 / 宪章 §10.4)。
        """
        with tempfile.TemporaryDirectory() as td:
            d = os.path.join(td, "lib", "algorithms", "calibration", "src")
            os.makedirs(d)
            open(os.path.join(d, "module_entry.cpp"), "w").write(
                "#include <omp.h>\n"
                "#define CAL_OMP_SET(n) omp_set_num_threads((n))\n"
                "void f(){ CAL_OMP_SET(4); }\n"
                "#pragma omp parallel for num_threads(4)\nvoid g(){}\n")
            old = ctb.SCAN_ROOTS
            ctb.SCAN_ROOTS = [td]
            try:
                errors, reg, _ = ctb.scan()
            finally:
                ctb.SCAN_ROOTS = old
            self.assertFalse(
                [e for e in errors if "omp_set_num_threads 未登记" in e],
                "已登记文件不应再报 omp_set_num_threads 未登记: %s" % errors)
            self.assertTrue(
                any("module_entry.cpp" in e and "字面量线程数" in e for e in errors),
                "硬编码字面量必须独立 FAIL, 不被登记豁免: %s" % errors)
            self.assertTrue(reg, "已登记行必须出现在 registered 输出中")

    def test_07_registration_matches_windows_backslash_relpath(self):
        """登记键必须对 Windows 反斜杠 relpath 同样命中 (HOSTFIX-23③ 同型根因)。"""
        for posix in self.MODULE_ENTRY:
            win = posix.replace("/", "\\")
            note_posix = ctb.registered_annotation(posix)
            note_win = ctb.registered_annotation(win)
            self.assertIsNotNone(note_posix, "%s 必须命中登记" % posix)
            self.assertIsNotNone(note_win, "%s 必须命中登记" % win)
            self.assertEqual(note_posix, note_win, "两种分隔符必须命中同一注记")

    def test_08_registration_annotation_records_thread_source(self):
        """R-10: 注记必须写明线程数来源 = host budget 租约 (非编译期字面量)。"""
        tokens = ("acquire", "release", "租约", "租借", "budget")
        for posix in self.MODULE_ENTRY:
            note = ctb.registered_annotation(posix)
            self.assertIsNotNone(note, "%s 必须命中登记" % posix)
            self.assertTrue(any(t in note for t in tokens),
                            "%s 注记未写明线程数来源: %s" % (posix, note))

    def test_09_registration_is_path_scoped_not_basename(self):
        """登记必须限定到这三条路径, 不得用 basename 放行其它 module_entry.cpp。"""
        for other in ("lib/algorithms/drizzle/hips/src/module_entry.cpp",
                      "lib/algorithms/noise_snr/src/module_entry.cpp"):
            self.assertIsNone(ctb.registered_annotation(other),
                              "%s 不得被这三条预算注入登记放行" % other)

    # ── RT-001 (P0 M5a-G-005): 私有线程池可见性 + 登记悬空 + 行级豁免封堵 ──

    def _scan_tmp(self, rel_parts, body):
        with tempfile.TemporaryDirectory() as td:
            d = os.path.join(td, *rel_parts)
            os.makedirs(d, exist_ok=True)
            with open(os.path.join(d, rel_parts[-1] + ".cpp"), "w") as fh:
                fh.write(body)
            old = ctb.SCAN_ROOTS
            ctb.SCAN_ROOTS = [td]
            try:
                return ctb.scan()
            finally:
                ctb.SCAN_ROOTS = old

    def test_10_unregistered_thread_pool_must_fail(self):
        """负例(必红): 未登记的 std::vector<std::thread> 私有池必须 FAIL。

        改前: 正则不含该形态 => 池声明对 check 完全不可见(全仓 12+ 处)。
        """
        errors, reg, _ = self._scan_tmp(
            ("lib", "x"), "void f(){ std::vector<std::thread> pool; pool.emplace_back([]{ }); }\n")
        self.assertTrue(any("未登记私有线程池" in e and "x.cpp" in e for e in errors),
                        "未登记私有线程池必须 FAIL: %s" % errors)

    def test_11_registered_thread_pool_passes_with_annotation(self):
        """正例(可绿): 登记面覆盖后同形态不再报错(登记须带注记)。"""
        key = "lib/x/x.cpp"
        old = dict(ctb.REGISTERED)
        ctb.REGISTERED[key] = "测试注记: 线程数来自 ThreadBudget 租约"
        try:
            errors, reg, _ = self._scan_tmp(
                ("lib", "x"), "void f(){ std::vector<std::thread> pool; }\n")
            self.assertFalse([e for e in errors if "未登记私有线程池" in e], errors)
            self.assertTrue(any("x.cpp" in r for r in reg), reg)
        finally:
            ctb.REGISTERED.clear(); ctb.REGISTERED.update(old)

    def test_12_dangling_registration_key_must_fail(self):
        """负例(必红): 登记键指向不存在的路径 => 悬空登记 FAIL。

        改前: ARCH-001 迁移(ARCH-001: lib/core -> lib/infrastructure/scheduler)后
        旧路径键静默失配, check 由绿转红/由红转绿均无门可查。
        """
        old = dict(ctb.REGISTERED)
        ctb.REGISTERED["lib/does/not/exist/rogue.cpp"] = "悬空登记(负例注入)"
        try:
            errs = ctb.validate_registry()
            self.assertTrue(any("登记悬空" in e and "rogue.cpp" in e for e in errs), errs)
        finally:
            ctb.REGISTERED.clear(); ctb.REGISTERED.update(old)
        self.assertEqual(ctb.validate_registry(), [], "真实树不得有悬空登记")

    def test_13_row_level_watchdog_exemption_removed(self):
        """负例(必红): 行内含 "watchdog" 不再是放行理由(行级豁免已删除)。"""
        errors, reg, _ = self._scan_tmp(
            ("lib", "w"), "std::thread watchdog_thread([]{ });\n")
        self.assertTrue(any("w.cpp" in e and "未登记线程创建" in e for e in errors),
                        "行级 watchdog 逃逸口必须已封堵: %s" % errors)


if __name__ == "__main__":
    unittest.main(verbosity=2)
