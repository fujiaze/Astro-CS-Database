#!/usr/bin/env python3
"""DOCCHK-001 机器化文档检查: 核对 docs/api/*_V1.md 函数名/签名/schema/命令/退出码
   与真实头文件/源码/schema; 删除/改名/签名 mutation 均使 checker fail。
验收(03 L132 + PHASE1_API_V1 §4): 解析 headers/source/schema/help; 核对文档函数名/签名/
   字段/退出码; 删除/改名/签名 mutation 均使 checker fail。
"""
import json, os, re, shutil, subprocess, tempfile, unittest

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
CHECKER = os.path.join(REPO, "tools", "check_api_docs.py")


def run_check(repo, docs_dir=None, exit_codes_h=None):
    cmd = ["python3", CHECKER, "--repo", repo]
    if docs_dir:
        cmd += ["--docs-dir", docs_dir]
    if exit_codes_h:
        cmd += ["--exit-codes-h", exit_codes_h]
    r = subprocess.run(cmd, capture_output=True, text=True, timeout=180)
    return r.returncode, r.stdout, r.stderr


class TestDocMachineCheck(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.clean_rc, cls.clean_out, cls.clean_err = run_check(REPO)
        cls.tmp = tempfile.mkdtemp(prefix="docchk_")

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.tmp, ignore_errors=True)

    # ── 基线: 干净仓库 PASS ──
    def test_00_clean_repo_passes(self):
        self.assertEqual(self.clean_rc, 0,
                         f"干净仓库应 PASS, rc={self.clean_rc}\n{self.clean_err}")

    # ── 复制 docs/api → temp, 做 mutation, 每次须 FAIL ──
    def _mut_repo(self, fn):
        dst = os.path.join(self.tmp, "docs", "api")
        if os.path.isdir(dst):
            shutil.rmtree(dst)
        shutil.copytree(os.path.join(REPO, "docs", "api"), dst)
        fn(os.path.join(dst, "PHASE1_API_V1.md"))
        return dst

    def _expect_fail(self, dst, label, operator, extra=None):
        rc, out, err = run_check(REPO, docs_dir=dst, exit_codes_h=extra)
        self.assertNotEqual(rc, 0, f"{label}: mutation 应使 checker FAIL")

    def test_01_rename_function_fails(self):
        """改名: 把 p1_session_run 改名为 p1_session_do_the_work → FAIL。"""
        def fn(p):
            t = open(p, encoding="utf-8").read()
            t = t.replace("p1_session_run(", "p1_session_do_the_work(")
            open(p, "w", encoding="utf-8").write(t)
        self._expect_fail(self._mut_repo(fn), "rename p1_session_run", None)

    def test_02_delete_registration_row_fails(self):
        """删除 §2 登记行(如 ac_calibrate_frame) → FAIL(§2 无登记行/符号缺失)。"""
        def fn(p):
            t = open(p, encoding="utf-8").read()
            # 删除包含 ac_calibrate_frame 的行
            lines = [l for l in t.splitlines() if "ac_calibrate_frame" not in l]
            open(p, "w", encoding="utf-8").write("\n".join(lines) + "\n")
        self._expect_fail(self._mut_repo(fn), "delete ac_calibrate_frame row", None)

    def test_03_signature_change_fails(self):
        """签名 mutation: p1_session_run 增加参数(int extra) → 参数数 doc≠code → FAIL。"""
        def fn(p):
            t = open(p, encoding="utf-8").read()
            t = t.replace("int async_io_depth);", "int async_io_depth, int extra);")
            open(p, "w", encoding="utf-8").write(t)
        self._expect_fail(self._mut_repo(fn), "signature p1_session_run +param", None)

    def test_04_exit_code_remove_fails(self):
        """退出码: 把 exit_codes.h 中 `8` 的定义删除 → 文档 §2 提到的 `8` 在唯一源缺失 → FAIL。"""
        tmp_dir = os.path.join(self.tmp, "inc_tmp")
        os.makedirs(tmp_dir, exist_ok=True)
        src = os.path.join(REPO, "cli", "exit_codes.h")
        tmp_h = os.path.join(tmp_dir, "exit_codes.h")
        t = open(src, encoding="utf-8").read()
        # 删除所有赋值 =8 的行(退出码 8 缺失)
        t = "\n".join(l for l in t.splitlines() if not re.search(r"=\s*8\b", l)) + "\n"
        open(tmp_h, "w", encoding="utf-8").write(t)
        rc, out, err = run_check(REPO, exit_codes_h=tmp_h)
        self.assertNotEqual(rc, 0, "exit code 8 缺失应使 checker FAIL")

    def test_05_command_tree_mutation_fails(self):
        """命令树: 删掉文档 CLI §1 一行命令 → 该命令在 doc 缺失(help 有) — 需反向:
        把 doc 的 run 行改成帮助中不存在的文本 → 命令树 doc vs help 不一致 → FAIL。"""
        dst = os.path.join(self.tmp, "docs", "api")
        shutil.rmtree(dst, ignore_errors=True)
        shutil.copytree(os.path.join(REPO, "docs", "api"), dst)
        p = os.path.join(dst, "CLI_PROTOCOL_V1.md")
        t = open(p, encoding="utf-8").read()
        t = t.replace("astrocs run --phases <1|2|3|1,2|1,2,3>",
                      "astrocs run --phases <1|2|3|1,2|1,2,3,9>")   # 加入非法 phase 9
        open(p, "w", encoding="utf-8").write(t)
        self._expect_fail(dst, "command tree run-phase mutation", None)

    def test_06_schema_field_mutation_fails(self):
        """Phase3 schema: 在临时 schema 删字段 → 引用字段未提及 → 需反向。用文档侧:
        把 PHASE3 §2 提到的一个 schema 字段改名 → schema 字段不在文档 → FAIL。
        但 schema 来自真实 schemas/; 若 schema 字段未在文档出现即 FAIL。本测试删除文档对
        'output_dir' 的提及(若存在)不直接; 简化: 在临时 docs 的 PHASE3 文档中把
        'scale_deg_per_px' 改名 → schema 有 scale_deg_per_px 但文档不再提及 → FAIL。"""
        dst = os.path.join(self.tmp, "docs", "api")
        shutil.rmtree(dst, ignore_errors=True)
        shutil.copytree(os.path.join(REPO, "docs", "api"), dst)
        p = os.path.join(dst, "PHASE3_API_V1.md")
        t = open(p, encoding="utf-8").read()
        # 把 schema 实际字段 scale_deg_per_px 在文档中改名(若 schema 含该字段)
        t = t.replace("scale_deg_per_px", "scale_deg_per_pixel")
        open(p, "w", encoding="utf-8").write(t)
        self._expect_fail(dst, "phase3 schema field mutation", None)


if __name__ == "__main__":
    unittest.main(verbosity=2)
