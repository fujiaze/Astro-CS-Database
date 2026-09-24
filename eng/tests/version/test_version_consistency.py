#!/usr/bin/env python3
"""VER-001 测试: 版本源合同 + 一致性 checker 的 mutation 试金石。stdlib only。"""
import json, os, re, shutil, subprocess, sys, tempfile, unittest

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
sys.path.insert(0, os.path.join(REPO, "eng", "tools"))
import gen_version  # noqa: E402
import importlib.util

def load_checker():
    spec = importlib.util.spec_from_file_location(
        "cvc", os.path.join(REPO, "eng", "tools", "check_version_consistency.py"))
    m = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(m)
    return m

def validate_schema(obj, schema):
    """最小 draft-07 子集校验: type/required/additionalProperties/const/pattern。"""
    def ok_node(v, s, path):
        t = s.get("type")
        if t == "object":
            if not isinstance(v, dict): return f"{path}: 非对象"
            for k in s.get("required", []):
                if k not in v: return f"{path}: 缺字段 {k}"
            if s.get("additionalProperties") is False:
                extra = set(v) - set(s.get("properties", {}))
                if extra: return f"{path}: 多余字段 {extra}"
            for k, sub in s.get("properties", {}).items():
                if k in v:
                    e = ok_node(v[k], sub, f"{path}.{k}")
                    if e: return e
        elif t == "string":
            if not isinstance(v, str): return f"{path}: 非字符串"
            if "const" in s and v != s["const"]: return f"{path}: const 违例 {v!r}"
            if "pattern" in s and not re.match(s["pattern"], v): return f"{path}: pattern 违例 {v!r}"
        elif t == "boolean":
            if not isinstance(v, bool): return f"{path}: 非布尔"
        return None
    return ok_node(obj, schema, "$")

class TestVersionContract(unittest.TestCase):
    def test_01_base_format_and_report_schema(self):
        base = gen_version.read_base_version()
        self.assertRegex(base, r"^\d+\.\d+\.\d+-alpha\.\d+$")
        schema = json.load(open(os.path.join(REPO, "eng", "contracts", "schemas", "version.schema.json"), encoding="utf-8"))
        rep = gen_version.build_report(commit="0123456789ab" * 3, dirty=False)
        self.assertIsNone(validate_schema(rep, schema), "gen_version 输出必须符合 version.schema.json")
        # 单源语义修正 (V8-CI-012 R6.5): 旧断言硬编码 0.10.0-alpha.2 过期字面量，
        # 与 read_base_version() 单源设计自相矛盾（版本推进即挂）。改由单源派生。
        self.assertTrue(rep["version"].startswith(base + "+g0123456789ab"))
        self.assertNotIn(".dirty", rep["version"])

    def test_02_dirty_suffix(self):
        rep = gen_version.build_report(commit="0123456789ab" * 3, dirty=True)
        self.assertTrue(rep["version"].endswith(".dirty"))
        self.assertEqual(rep["build_id"], "g0123456789ab.dirty")

    def test_03_reject_forbidden_prerelease(self):
        with tempfile.NamedTemporaryFile("w", suffix=".txt", delete=False) as f:
            f.write("1.0.0-rc.1\n"); bad = f.name
        with self.assertRaises(SystemExit):
            gen_version.read_base_version(bad)
        os.unlink(bad)

    def test_04_real_repo_checker_pass(self):
        r = subprocess.run([sys.executable, os.path.join(REPO, "eng", "tools", "check_version_consistency.py")],
                           capture_output=True, text=True, cwd=REPO)
        self.assertEqual(r.returncode, 0, f"真实仓库一致性必须 PASS:\n{r.stdout}{r.stderr}")
        self.assertIn("VERSION_CONSISTENCY_PASS", r.stdout)

    def test_05_mutation_forged_literal_must_fail(self):
        """mutation 合同: 任何一处伪造版本字面量必须被抓。"""
        m = load_checker()
        with tempfile.TemporaryDirectory() as td:
            os.makedirs(os.path.join(td, "docs"))
            forged = os.path.join(td, "docs", "FORGED.md")
            open(forged, "w", encoding="utf-8").write("发布版本: 1.2.3 正式版\n")
            errs = []
            m.check_file(forged, "0.10.0", 1, errs)
            self.assertTrue(any("1.2.3" in e for e in errs), f"伪造版本必须被抓: {errs}")
            # 对照: 当前唯一源不被误报
            ok_doc = os.path.join(td, "docs", "OK.md")
            cur = gen_version.read_base_version()
            base_num, alpha_n = re.match(r"^(\d+\.\d+\.\d+)-alpha\.(\d+)$", cur).groups()
            open(ok_doc, "w", encoding="utf-8").write(f"当前版本 {cur}\n")
            errs2 = []
            m.check_file(ok_doc, base_num, int(alpha_n), errs2)
            self.assertEqual(errs2, [])


# ── CI-VER-CHK-001 / 裁决 R-08: 标准条款号口径 ────────────────────────────────
CHECKER = os.path.join(REPO, "eng", "tools", "check_version_consistency.py")
CHECKER_REL = os.path.join("eng/tools", "check_version_consistency.py")
TEST_REL = os.path.join("eng", "tests", "version", "test_version_consistency.py")


def _base_alpha():
    """当前唯一源 (基础号, alpha 序号)。"""
    g = re.match(r"^(\d+\.\d+\.\d+)-alpha\.(\d+)$", gen_version.read_base_version())
    return g.group(1), int(g.group(2))


def _tmp_repo(td, version=None):
    """最小假仓库: td/tools/check_version_consistency.py + td/VERSION + td/docs/。

    checker 以自身 __file__ 定位 REPO, 故拷贝到 td/tools/ 下即把扫描根切到 td,
    可在不触碰真实仓库的前提下做端到端 (exit code) 正/负向试金石。
    """
    # 2026-09-21 根目录整合：checker 自身位于 eng/tools/（REPO=dirname^3），
    # 夹具必须同深度，_run_checker 才找得到它。
    os.makedirs(os.path.join(td, "eng", "tools"), exist_ok=True)
    os.makedirs(os.path.join(td, "docs"), exist_ok=True)
    shutil.copyfile(CHECKER, os.path.join(td, "eng", "tools", "check_version_consistency.py"))
    if version is None:
        with open(os.path.join(REPO, "VERSION"), encoding="utf-8") as f:
            version = f.read().strip()
    with open(os.path.join(td, "VERSION"), "w", encoding="utf-8") as f:
        f.write(version + "\n")
    return td


def _run_checker(root):
    """以 CI 同构口径跑检查器 (同一 argv 形态; CI 侧由 eng/ci/run.py 注入 UTF-8 环境)。"""
    return subprocess.run(
        [sys.executable, "-B", os.path.join(root, "eng", "tools", "check_version_consistency.py")],
        capture_output=True, text=True, encoding="utf-8", errors="replace",
        cwd=root, timeout=120)


class TestStandardClauseExclusion(unittest.TestCase):
    """CI-VER-CHK-001 / 裁决 R-08: 标准条款号 (§a.b.c) 不得判为版本字面量;
    同时真实产品版本漂移仍须 FAIL —— 口径只准更精确、不准更宽松。

    覆盖形态:
      N1 § 前缀条款号 (§2.1.1 / §4.2.1 / §4.4.1 / §6.3.1) — 19 条误报的实测形态;
      N2 枚举续项无 § 前缀 (§4.1/4.2.1/4.4.1/6.3.1);
      N3 标准名后紧跟的裸条款号 (Paper I 2.1.1 / HiPS 4.2.1);
      P1 真实版本漂移 (VERSION 基础号 != 文档字面量) 必须 FAIL;
      P2 故障注入: 条款号同行/邻近行出现伪造版本必须 FAIL;
      P3 alpha 漂移不得被条款豁免掩盖 (alpha 判定仍跑在原始行上)。
    """

    CLAUSE_LINES = [
        # docs/standards/STANDARDS_REGISTRY.md:41 实测行 (Paper I/II + SIP)
        "- CLAUSES: Paper I §2.1.1（CRPIX 1-based）/§3（CD/CTYPE）；Paper II §2.1（旋转/LONPOLE）/§5 Table 1（TAN/SIN/CAR/AIT）",
        # docs/standards/STANDARDS_REGISTRY.md:86 实测行 (IVOA HiPS)
        "- CLAUSES: HiPS 1.0 §3（层级索引与目录结构）/§4.1（tile）/§4.2.1（properties）/§4.4.1（all-sky map）/§6.3.1（客户端绘制）",
        # docs/standards/STANDARDS_REGISTRY.md:95 / :239 实测行
        "| §4.2.1（properties 必需键集） | `hips_version/hips_order` 必需且自洽 | PARTIAL | docs/algorithms/HIPS_WRITER.md |",
        "| STD-F1 | spherical-projection | Paper I §2.1.1（CRPIX 1-based 参考像素） | 第 1 行 | OPEN | STD-F1-ADJ |",
        # N2 + N3: 枚举续项与标准名裸条款号
        "条款面: §4.1/4.2.1/4.4.1/6.3.1；Paper II 5.1.1 与 HiPS 4.2.1 见标准正文",
    ]

    def test_06_pre_fix_regex_would_flag_clause_numbers(self):
        """先红证据: 旧口径 (BASE_RE 直扫原始行) 必抓条款号 —— 修复前 19 条即此形态。"""
        m = load_checker()
        for line in self.CLAUSE_LINES:
            self.assertTrue(m.BASE_RE.findall(line),
                            f"样本必须能被旧口径命中, 否则用例失去回归意义: {line}")
        for line in self.CLAUSE_LINES:
            self.assertEqual(m.BASE_RE.findall(m.mask_standard_clause_numbers(line)), [],
                             f"修复后条款号不得再被判为版本字面量: {line}")

    def test_07_real_registry_zero_false_positive_and_anchors_intact(self):
        """负例: 真实注册表零误报; 且标准条款号锚必须原样保留 (R-08 禁改写)。"""
        registry = os.path.join(REPO, "docs", "standards", "STANDARDS_REGISTRY.md")
        with open(registry, encoding="utf-8") as f:
            text = f.read()
        for anchor in ("§2.1.1", "§4.2.1", "§4.4.1", "§6.3.1"):
            self.assertIn(anchor, text, f"标准条款号是可追溯锚, 不得改写或删除: {anchor}")
        errs = []
        load_checker().check_file(registry, *_base_alpha(), errs)
        self.assertEqual(errs, [], f"标准注册表条款号必须零误报: {errs}")
        # 同一文件内注入真实版本漂移: 条款豁免不得把整份注册表变成"免检区"
        with tempfile.TemporaryDirectory() as td:
            d = os.path.join(td, "docs", "standards")
            os.makedirs(d)
            with open(os.path.join(d, "STANDARDS_REGISTRY.md"), "w", encoding="utf-8") as f:
                f.write(text + "\n| 产品版本 | 1.2.3 |\n")
            errs_drift = []
            load_checker().check_file(os.path.join(d, "STANDARDS_REGISTRY.md"),
                                      *_base_alpha(), errs_drift)
            self.assertTrue(any("1.2.3" in e2 for e2 in errs_drift),
                            f"注册表内的真实版本漂移必须 FAIL: {errs_drift}")

    def test_08_tmp_repo_clause_only_is_pass(self):
        """负例端到端: 只含标准条款号的假仓库必须 PASS (exit 0)。"""
        with tempfile.TemporaryDirectory() as td:
            _tmp_repo(td)
            with open(os.path.join(td, "docs", "STANDARDS.md"), "w", encoding="utf-8") as f:
                f.write("\n".join(self.CLAUSE_LINES) + "\n")
            r = _run_checker(td)
            self.assertEqual(r.returncode, 0, f"条款号不得判 FAIL:\n{r.stdout}\n{r.stderr}")
            self.assertIn("VERSION_CONSISTENCY_PASS", r.stdout)

    def test_09_real_version_drift_still_fails(self):
        """正例端到端: 真实产品版本漂移 (文档字面量 != 根 VERSION) 必须 FAIL。"""
        with tempfile.TemporaryDirectory() as td:
            _tmp_repo(td)
            with open(os.path.join(td, "docs", "RELEASE.md"), "w", encoding="utf-8") as f:
                f.write("当前产品版本 1.2.3（与根 VERSION 不一致）\n")
            r = _run_checker(td)
            self.assertNotEqual(r.returncode, 0, "真实版本漂移必须 FAIL (R-08 正向守卫)")
            self.assertIn("VERSION_CONSISTENCY_FAIL", r.stdout)
            self.assertIn("1.2.3", r.stdout)
            # 对照: 同一假仓库去掉漂移行后必须回到 PASS (证明红灯来自漂移本身)
            with open(os.path.join(td, "docs", "RELEASE.md"), "w", encoding="utf-8") as f:
                f.write("当前产品版本取自根 VERSION 唯一源\n")
            self.assertEqual(_run_checker(td).returncode, 0)

    def test_10_forged_version_on_clause_line_still_fails(self):
        """故障注入必败: 条款豁免不得顺带放行同行/相邻的伪造版本。"""
        m = load_checker()
        with tempfile.TemporaryDirectory() as td:
            os.makedirs(os.path.join(td, "docs"))
            p = os.path.join(td, "docs", "MIXED.md")
            with open(p, "w", encoding="utf-8") as f:
                f.write("| §4.2.1（properties） | 版本 1.2.3 漂移 |\n")
            errs = []
            m.check_file(p, *_base_alpha(), errs)
            self.assertTrue(any("1.2.3" in e for e in errs),
                            f"条款豁免不得放行同行伪造版本: {errs}")

    def test_11_bare_clause_in_standard_context_and_guard(self):
        """N3 负例 + 正向守卫: 仅"标准名紧跟数字"才是条款号形态。"""
        m = load_checker()
        b, a = _base_alpha()
        with tempfile.TemporaryDirectory() as td:
            os.makedirs(os.path.join(td, "docs"))
            ok = os.path.join(td, "docs", "STD.md")
            with open(ok, "w", encoding="utf-8") as f:
                f.write("Paper I 2.1.1 与 HiPS 4.2.1 条款\n")
            errs_ok = []
            m.check_file(ok, b, a, errs_ok)
            self.assertEqual(errs_ok, [], f"标准名后紧跟的裸条款号必须豁免: {errs_ok}")
            bad = os.path.join(td, "docs", "STD_GUARD.md")
            with open(bad, "w", encoding="utf-8") as f:
                f.write("Paper I 发布版本 1.2.3\n")
            errs_bad = []
            m.check_file(bad, b, a, errs_bad)
            self.assertTrue(any("1.2.3" in e for e in errs_bad),
                            f"非条款号形态的版本字面量仍须 FAIL: {errs_bad}")

    def test_12_alpha_drift_not_masked_by_clause_exemption(self):
        """P3 守卫: alpha 漂移判定仍跑在原始行上, 条款豁免不得掩盖。"""
        m = load_checker()
        b, a = _base_alpha()
        drift = f"{b}-alpha.{a + 1}"
        with tempfile.TemporaryDirectory() as td:
            os.makedirs(os.path.join(td, "docs"))
            p = os.path.join(td, "docs", "ALPHA_DRIFT.md")
            with open(p, "w", encoding="utf-8") as f:
                f.write(f"| §4.2.1（properties） | 当前版本 {drift} |\n")
            errs = []
            m.check_file(p, b, a, errs)
            self.assertTrue(any("alpha 版本漂移" in e for e in errs),
                            f"条款豁免不得掩盖 alpha 漂移: {errs}")

    def test_13_checker_output_bitwise_deterministic(self):
        """确定性: 同输入两次运行 rc 与 stdout 必须逐字节一致 (无时间戳/无随机序)。"""
        runs = [subprocess.run([sys.executable, "-B", CHECKER], capture_output=True,
                               cwd=REPO, timeout=120) for _ in range(2)]
        self.assertEqual(runs[0].returncode, runs[1].returncode)
        self.assertEqual(runs[0].stdout, runs[1].stdout, "stdout 必须逐字节一致")
        self.assertEqual(runs[0].returncode, 0)

class TestLifecycleBoundary(unittest.TestCase):
    """W4-A3: 合同生命周期边界值不得被当产品版本，但产品版本声明照旧必须 FAIL。

    事由: DATA-001 的 `retire_after: "0.12.0"`(前向退出边界) 使检查器在真仓恒
    FAIL(49 条)，遮蔽 UT-VERSION test_04/test_13；口径只准更精确, 不准更宽松。
    """

    def test_16_lifecycle_boundary_value_is_not_product_version(self):
        m = load_checker()
        b, a = _base_alpha()
        with tempfile.TemporaryDirectory() as td:
            os.makedirs(os.path.join(td, "docs", "contracts"))
            p = os.path.join(td, "docs", "contracts", "registry.json")
            with open(p, "w", encoding="utf-8") as f:
                f.write('{"object": "signal", "retire_after": "9.9.9"}\n')
            errs = []
            m.check_file(p, b, a, errs)
            self.assertEqual(errs, [], f"前向生命周期边界不得被判为产品版本: {errs}")

    def test_17_product_version_claim_in_same_object_still_fails(self):
        """负例守卫: 同一行/同一对象里的产品版本声明不得被生命周期豁免放行。"""
        m = load_checker()
        b, a = _base_alpha()
        with tempfile.TemporaryDirectory() as td:
            os.makedirs(os.path.join(td, "docs", "contracts"))
            p = os.path.join(td, "docs", "contracts", "registry.json")
            # 同一 JSON 对象: 生命周期边界(放行) + 产品版本声明(必须 FAIL)
            with open(p, "w", encoding="utf-8") as f:
                f.write('{"product_version": "0.12.0", "retire_after": "0.12.0"}\n')
            errs = []
            m.check_file(p, b, a, errs)
            self.assertTrue(any("0.12.0" in e for e in errs),
                            f"product_version 声明的同一字面量必须仍然 FAIL: {errs}")

    def test_18_lifecycle_table_column_is_fail_closed(self):
        """契约文档"退役窗口"列: 只放行**显式登记**的边界值; 其它值必须判红。"""
        m = load_checker()
        b, a = _base_alpha()
        with tempfile.TemporaryDirectory() as td:
            os.makedirs(os.path.join(td, "docs", "contracts"))
            md = os.path.join(td, "docs", "contracts", "UNIFIED.md")
            head = "| 旧路径 | 新路径 | 退役窗口 |\n|---|---|---|\n"
            with open(md, "w", encoding="utf-8") as f:
                f.write(head + "| a.json | b.json | 0.12.0 |\n")
            errs_ok = []
            m.check_file(md, b, a, errs_ok)
            self.assertEqual(errs_ok, [], f"已登记的退役窗口值不得判红: {errs_ok}")
            with open(md, "w", encoding="utf-8") as f:
                f.write(head + "| a.json | b.json | 8.8.8 |\n")
            errs_bad = []
            m.check_file(md, b, a, errs_bad)
            self.assertTrue(any("8.8.8" in e for e in errs_bad),
                            f"未登记的生命周期边界值必须 FAIL(fail-closed): {errs_bad}")


# ── BLD-401 R3: 第三方工具版本口径 ───────────────────────────────────────────
class TestExternalToolVersionExemption(unittest.TestCase):
    """docs/research/*RESEARCH_PACK.md 的**第三方工具版本**不得被判为产品版本;
    同时真产品版本漂移仍须 FAIL —— 口径只准更精确、不准更宽松。

    事由: 研究包「开源对照」表把 SWarp 2.41.5 / DeepSkyStacker-DSS 6.2.2 /
    SExtractor 2.28.2 写在表里, 旧口径当"未知产品版本字面量" ⇒ 真仓恒 FAIL(7 条),
    遮蔽 test_04/test_13。这些是外部工具/文献版本(溯源锚, DOC-404 逐字校验),
    不是本项目版本声明 —— 与 R-08 / W4-A3 同款: 修口径, 不改研究包。

    覆盖形态（均为研究包实测写法）:
      P 表格"工具名单元格 | 版本单元格"与单元格内"工具名 版本";
      N1 真项目版本号写错必须 FAIL;
      N2 工具名与字面量之间出现产品版本语境词 ⇒ 必须 FAIL;
      N3 同行"工具版本 + 产品版本"里的产品版本必须 FAIL;
      N4 alpha 漂移不得被工具豁免掩盖;
      N5 间隔超出上限的字面量不得被吸附豁免。
    """

    EXTERNAL_LINES = [
        # docs/research/PHOTOMETRY_RESEARCH_PACK.md:134 实测行
        "| O7 | **SWarp**（GPL-3.0） | 2.41.5 | `src/coadd.c:292`（`coadd_fields()`：逐像素组合主入口） |",
        # docs/research/SNR_WEIGHT_RESEARCH_PACK.md:172 实测行
        "| **SWarp**（GPL-3.0） | 2.41.5 | `src/coadd.c:292`（`coadd_fields`）；`src/back.c:413`（`backstat`） |",
        # docs/research/SNR_WEIGHT_RESEARCH_PACK.md:198-199 实测行
        "| SWarp `coadd.c:1279-1311` | SWarp 2.41.5 | **有效**：`COADD_WEIGHTED` 分支的逐像素逆方差组合循环 |",
        # docs/research/SNR_WEIGHT_RESEARCH_PACK.md:200-201 实测行
        "| DeepSkyStacker `RegisterEngine.cpp:86-118` | DeepSkyStacker/DSS 6.2.2 | **有效**：`:86` 为 `CRegisteredFrame::ComputeScore` |",
        # docs/research/SNR_WEIGHT_RESEARCH_PACK.md:202 实测行
        "| SExtractor `analyse.c:200-203,304-310` | SExtractor 2.28.2 | **有效**：`:200-203` 为 FLUXERR 方差累加 |",
    ]

    def test_19_external_tool_versions_not_flagged(self):
        """P: 修复前必被抓（先红证据）+ 修复后零误报。"""
        m = load_checker()
        b, a = _base_alpha()
        for line in self.EXTERNAL_LINES:
            self.assertTrue(m.BASE_RE.findall(line),
                            f"样本必须能被旧口径命中, 否则用例失去回归意义: {line}")
        with tempfile.TemporaryDirectory() as td:
            os.makedirs(os.path.join(td, "docs"))
            p = os.path.join(td, "docs", "PACK.md")
            with open(p, "w", encoding="utf-8") as f:
                f.write("\n".join(self.EXTERNAL_LINES) + "\n")
            errs = []
            m.check_file(p, b, a, errs)
            self.assertEqual(errs, [], f"第三方工具版本不得判为产品版本: {errs}")

    def test_20_product_version_drift_still_fails(self):
        """N1/N2/N3: 真产品版本漂移必须 FAIL —— 豁免不得把研究包变成"免检区"。"""
        m = load_checker()
        b, a = _base_alpha()
        with tempfile.TemporaryDirectory() as td:
            os.makedirs(os.path.join(td, "docs"))
            p = os.path.join(td, "docs", "MIXED.md")
            cases = [
                "发布版本: 1.2.3 正式版",
                "SWarp 对照：本项目版本 9.9.9",
                "| SWarp | 2.41.5 | 产品版本 9.9.9 |",
            ]
            for line in cases:
                with open(p, "w", encoding="utf-8") as f:
                    f.write(line + "\n")
                errs = []
                m.check_file(p, b, a, errs)
                self.assertTrue(any("9.9.9" in e or "1.2.3" in e for e in errs),
                                f"产品版本漂移必须 FAIL: {line!r} -> {errs}")
            # 对照: 同一文件只留第三方工具版本行时必须回到零误报
            with open(p, "w", encoding="utf-8") as f:
                f.write("| SWarp | 2.41.5 |\n")
            errs_ok = []
            m.check_file(p, b, a, errs_ok)
            self.assertEqual(errs_ok, [], f"对照行必须零误报: {errs_ok}")

    def test_21_alpha_drift_and_gap_limit_not_masked(self):
        """N4/N5: alpha 漂移与超距字面量都不得被工具名豁免吸附。"""
        m = load_checker()
        b, a = _base_alpha()
        with tempfile.TemporaryDirectory() as td:
            os.makedirs(os.path.join(td, "docs"))
            p = os.path.join(td, "docs", "GUARD.md")
            with open(p, "w", encoding="utf-8") as f:
                f.write("| SWarp 2.41.5 | 当前版本 %s-alpha.%d |\n" % (b, a + 1))
            errs = []
            m.check_file(p, b, a, errs)
            self.assertTrue(any("alpha 版本漂移" in e for e in errs),
                            f"工具豁免不得掩盖 alpha 漂移: {errs}")
            with open(p, "w", encoding="utf-8") as f:
                f.write("SWarp " + ("x" * (m.EXTERNAL_TOOL_GAP_MAX + 10)) + " 9.9.9\n")
            errs2 = []
            m.check_file(p, b, a, errs2)
            self.assertTrue(any("9.9.9" in e for e in errs2),
                            f"超出间隔上限的字面量不得被吸附豁免: {errs2}")

    def test_22_real_research_packs_zero_false_positive(self):
        """负例: 真实研究包零误报, 且第三方版本锚必须原样保留（DOC-404 禁改写）。"""
        m = load_checker()
        b, a = _base_alpha()
        packs = {
            os.path.join("docs", "research", "PHOTOMETRY_RESEARCH_PACK.md"):
                ("**SWarp**（GPL-3.0） | 2.41.5",),
            os.path.join("docs", "research", "SNR_WEIGHT_RESEARCH_PACK.md"):
                ("SWarp 2.41.5", "DeepSkyStacker/DSS 6.2.2", "SExtractor 2.28.2"),
        }
        for rel, anchors in packs.items():
            path = os.path.join(REPO, rel)
            with open(path, encoding="utf-8") as f:
                text = f.read()
            for anchor in anchors:
                self.assertIn(anchor, text, f"第三方版本锚是溯源证据, 不得改写: {rel} {anchor}")
            errs = []
            m.check_file(path, b, a, errs)
            self.assertEqual(errs, [], f"研究包第三方工具版本必须零误报: {rel} {errs}")

    def test_23_checker_self_test_cli_passes(self):
        """--self-test（可执行负例面）必须 rc=0, 且与 CI 同 argv 形态。"""
        r = subprocess.run([sys.executable, "-B", CHECKER, "--self-test"],
                           capture_output=True, text=True, encoding="utf-8",
                           errors="replace", cwd=REPO, timeout=120)
        self.assertEqual(r.returncode, 0, f"检查器 --self-test 必须全绿:\n{r.stdout}\n{r.stderr}")
        self.assertIn("SELF_TEST PASS", r.stdout)


# ── 故障注入守卫 (裁决 R-08 自证: 新用例必须"注入必败"、未注入必绿) ──────────
# 本类把上面的条款号用例当作被试对象做 meta 测试: 对检查器施加"过宽/过窄"两类
# 故障注入, 断言子进程里的用例必须变红; 未注入时必须全绿。
# 递归保护: 子进程带 CI_VER_CHK_MUTATION_GUARD=1, 本类自行 skip。
MUTATION_GUARD_ENV = "CI_VER_CHK_MUTATION_GUARD"
# W4-A3: 检查器的未知字面量扫描行现为"条款号 + 生命周期边界"双层挖空，
# 注入锚点随判据更新（锚必须逐字命中现行源码，否则守卫本身失效 ——
# test_14 的 assertIn 就是这条要求的机器判据）。
MASK_CALL = "for m in BASE_RE.finditer(scan_line):"
# BLD-401 R3: 第三方工具版本豁免在 scan_line 组装链上（锚必须逐字命中现行源码）
EXT_MASK_CALL = ("scan_line = mask_external_tool_versions("
                 "mask_lifecycle_boundaries(mask_standard_clause_numbers(line)))")
MUTATIONS = {
    # 过窄: 取消条款号豁免 → §2.1.1 等再现 19 条误报
    "masking_disabled": (MASK_CALL, "for m in BASE_RE.finditer(line):"),
    # 过宽 1: 整行含 § 即挖空 → 同行真实版本字面量被放行
    "mask_whole_line_on_section_sign": (
        MASK_CALL,
        "for m in BASE_RE.finditer(' ' * len(line) if '\u00a7' in line else line):"),
    # 过宽 2: 行内出现标准名即挖空 → 同行/邻近漂移被放行
    "mask_any_standard_name_line": (
        MASK_CALL,
        "for m in BASE_RE.finditer(mask_standard_clause_numbers(line) "
        "if not any(k in line for k in ('Paper', 'HiPS', 'SIP')) else ' ' * len(line)):"),
    # 过宽 3: 挖掉所有三元组 → 检查器整体失效
    "mask_every_triple": (
        MASK_CALL,
        "for m in BASE_RE.finditer(BASE_RE.sub(lambda x: ' ' * len(x.group(0)), line)):"),
    # 过窄: 丢掉"标准名后裸条款号"豁免
    "bare_standard_name_clause_not_masked": (
        "spans.extend(m.span(1) for m in STANDARD_NAME_CLAUSE_RE.finditer(line))",
        "pass  # 注入: 去掉标准名后裸条款号豁免"),
    # 过宽 4: 过宽豁免提前到 alpha 判定之前 → alpha 漂移被掩盖
    "over_broad_masking_before_alpha_check": (
        "    for i, line in enumerate(lines, 1):",
        "    for i, _raw in enumerate(lines, 1):\n"
        "        line = ' ' * len(_raw) if '\u00a7' in _raw else _raw"),
    # BLD-401 R3 第三方工具版本口径的双向守卫:
    # 过窄: 取消工具版本豁免 → 研究包 7 条误报再现 (test_19/test_22 必败)
    "external_tool_mask_disabled": (
        EXT_MASK_CALL,
        "scan_line = mask_lifecycle_boundaries(mask_standard_clause_numbers(line))"),
    # 过宽: 行内出现工具名即挖空整行 → 同行产品版本漂移被放行 (test_20/test_21 必败)
    "external_tool_mask_whole_line": (
        EXT_MASK_CALL,
        "scan_line = (' ' * len(line)) if EXTERNAL_TOOL_NAME_RE.search(line) "
        "else mask_lifecycle_boundaries(mask_standard_clause_numbers(line))"),
}
CHILD_TESTS = [
    "test_version_consistency.TestStandardClauseExclusion." + n for n in (
        "test_06_pre_fix_regex_would_flag_clause_numbers",
        "test_08_tmp_repo_clause_only_is_pass",
        "test_10_forged_version_on_clause_line_still_fails",
        "test_11_bare_clause_in_standard_context_and_guard",
        "test_12_alpha_drift_not_masked_by_clause_exemption",
    )] + [
    "test_version_consistency.TestLifecycleBoundary." + n for n in (
        "test_16_lifecycle_boundary_value_is_not_product_version",
        "test_17_product_version_claim_in_same_object_still_fails",
        "test_18_lifecycle_table_column_is_fail_closed",
    )] + [
    "test_version_consistency.TestExternalToolVersionExemption." + n for n in (
        "test_19_external_tool_versions_not_flagged",
        "test_20_product_version_drift_still_fails",
        "test_21_alpha_drift_and_gap_limit_not_masked",
    )]


def _child_env():
    """子进程强制 UTF-8: 与 CI (eng/ci/run.py PYTHONUTF8=1) 同口径, 且免疫宿主 cp1252。"""
    env = dict(os.environ)
    env["PYTHONUTF8"] = "1"
    env["PYTHONIOENCODING"] = "utf-8"
    env[MUTATION_GUARD_ENV] = "1"
    return env


def _child_tree(td, checker_source):
    """只放被试用例真正需要的文件 (不需要真实 docs/, 便于 Windows 无符号链接权限时可用)。"""
    os.makedirs(os.path.join(td, "eng", "tools"))
    os.makedirs(os.path.join(td, "eng", "tests", "version"))
    with open(os.path.join(td, CHECKER_REL), "w", encoding="utf-8") as f:
        f.write(checker_source)
    shutil.copyfile(os.path.join(REPO, "eng", "tools", "gen_version.py"),
                    os.path.join(td, "eng", "tools", "gen_version.py"))
    shutil.copyfile(os.path.join(REPO, "VERSION"), os.path.join(td, "VERSION"))
    shutil.copyfile(os.path.join(REPO, TEST_REL), os.path.join(td, TEST_REL))
    shutil.copyfile(os.path.join(REPO, "eng", "tests", "version", "__init__.py"),
                    os.path.join(td, "eng", "tests", "version", "__init__.py"))
    return os.path.join(td, "eng", "tests", "version")


def _run_child(cwd):
    return subprocess.run(
        [sys.executable, "-B", "-m", "unittest"] + CHILD_TESTS,
        capture_output=True, text=True, encoding="utf-8", errors="replace",
        cwd=cwd, env=_child_env(), timeout=180)


@unittest.skipIf(os.environ.get(MUTATION_GUARD_ENV) == "1",
                 "递归保护: 本进程已是故障注入子进程")
class TestCheckerMutationGuard(unittest.TestCase):
    """meta 测试: 条款号用例在故障注入下必败、未注入时必绿 (R-08 反向必败要求)。"""

    def _fixed_source(self):
        with open(CHECKER, encoding="utf-8") as f:
            return f.read()

    def test_14_fault_injections_must_be_caught(self):
        src = self._fixed_source()
        for name, (old, new) in MUTATIONS.items():
            self.assertIn(old, src, f"{name}: 注入锚点未命中, 守卫失效")
            with self.subTest(mutation=name):
                with tempfile.TemporaryDirectory() as td:
                    cwd = _child_tree(td, src.replace(old, new))
                    r = _run_child(cwd)
                self.assertNotEqual(
                    r.returncode, 0,
                    f"故障注入 {name} 必须被新用例抓住 (不得放水):\n{r.stdout}\n{r.stderr}")

    def test_15_unmutated_checker_passes_control(self):
        """对照组: 未注入时必须全绿 —— 证明 test_14 的红灯来自注入而非夹具本身。"""
        with tempfile.TemporaryDirectory() as td:
            cwd = _child_tree(td, self._fixed_source())
            r = _run_child(cwd)
        self.assertEqual(r.returncode, 0,
                         f"未注入的检查器 + 被试用例必须全绿:\n{r.stdout}\n{r.stderr}")


if __name__ == "__main__":
    unittest.main(verbosity=2)
