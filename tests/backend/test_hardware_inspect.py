#!/usr/bin/env python3
"""BENCH-001 测试: 硬件画像 schema/实机比对/affinity 约束(fixture=taskset 单 CPU vs 实机)。"""
import json, os, platform, re, shutil, subprocess, sys, tempfile, unittest

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(REPO, "tools"))
import gen_version  # noqa: E402
SCHEMA = json.load(open(os.path.join(REPO, "contracts", "schemas", "hardware_inspect.schema.json"),
                        encoding="utf-8"))
# V6.1 布局路径修复 (V8-CI-012 R7): 旧 built() 经 V6.1 独立 cli 构建
# (cmake -S cli -B build/cli) 在 BLD-002 根构建布局下编译失败；改为复用
# 根构建产品 build/astrocs（与 test_cpu_profile 同型），缺失时按根 CMake 构建。
BUILD = os.path.join(REPO, "build")
EXE = os.path.join(BUILD, "astrocs")


def built():
    if not os.path.isfile(EXE):
        subprocess.run(["cmake", "-S", REPO, "-B", BUILD],
                       check=True, capture_output=True, timeout=300)
        subprocess.run(["cmake", "--build", BUILD, "-j2"],
                       check=True, capture_output=True, timeout=1800)
    return EXE


def inspect():
    r = subprocess.run([built(), "hardware", "inspect", "--json"],
                       capture_output=True, text=True, timeout=60)
    assert r.returncode == 0, r.stderr
    return json.loads(r.stdout)


class TestHardwareInspect(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        built()
        cls.tmp = tempfile.mkdtemp(prefix="hw_")

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.tmp, ignore_errors=True)

    def test_01_schema_valid(self):
        """schema 验证: required 全齐+additionalProperties=false(最小校验器, 免依赖)。"""
        d = inspect()
        for k in SCHEMA["required"]:
            self.assertIn(k, d, f"缺 required {k}")
        props = SCHEMA["properties"]
        for k, v in d.items():
            self.assertIn(k, props, f"字段 {k} 不在 schema(additionalProperties=false)")
            rule = props[k]
            if "const" in rule:
                self.assertEqual(v, rule["const"], k)
            if rule.get("type") == "integer" and isinstance(v, int) and "minimum" in rule:
                self.assertGreaterEqual(v, rule["minimum"], k)

    def test_02_ground_truth_affinity_constrained(self):
        """available_cpus 受 affinity 约束(06 §2 硬性; ≠机器总数)。"""
        d = inspect()
        aff = sorted(os.sched_getaffinity(0))
        self.assertEqual(d["affinity"], aff, "affinity 数组与 sched_getaffinity 一致")
        self.assertEqual(d["affinity_count"], len(aff))
        self.assertEqual(d["available_logical_cpus"], len(aff), "可用 CPU=affinity 实测")
        self.assertEqual(d["logical_cpus_configured"], os.cpu_count())

    def test_03_cpu_identity_matches_proc(self):
        d = inspect()
        cpuinfo = open("/proc/cpuinfo", encoding="utf-8").read()
        m = re.search(r"vendor_id\s*:\s*(\S+)", cpuinfo)
        if m:
            self.assertEqual(d["vendor"], m.group(1))
        self.assertGreater(d["feature_bits"], 0, "feature_bits 实测>0")
        self.assertIn("sse2", d["feature_names"])
        self.assertGreater(d["ram_bytes"], 0)
        self.assertEqual(d["page_size"], os.sysconf("SC_PAGESIZE"))
        self.assertIn(platform.machine(), ("x86_64",))
        # 单源语义 (V8-CI-012 R7): 版本字面量改由 VERSION 单源派生（旧硬编码
        # alpha 版本字面量随 VER-001 推进即挂），与 build 前缀 +12 位短哈希核对。
        self.assertRegex(d["astrocs_build"],
                         re.escape(gen_version.read_base_version()) + r"\+g[0-9a-f]{12}")

    def test_04_affinity_one_cpu_fixture(self):
        """fixture: taskset 单 CPU 下 available_cpus 必须降为 1(mock 环境 vs 实机比对)。"""
        if len(os.sched_getaffinity(0)) < 2:
            self.skipTest("宿主本身 1 CPU")
        p = subprocess.run(["taskset", "-c", "0", built(), "hardware", "inspect", "--json"],
                           capture_output=True, text=True, timeout=60)
        self.assertEqual(p.returncode, 0, p.stderr)
        d = json.loads(p.stdout)
        self.assertEqual(d["available_logical_cpus"], 1, "affinity=1 → 可用=1")
        self.assertEqual(d["affinity"], [0])

    def test_05_stdout_single_json_document(self):
        r = subprocess.run([built(), "hardware", "inspect", "--json"],
                           capture_output=True, text=True, timeout=60)
        docs = [l for l in r.stdout.splitlines() if l.strip()]
        json.loads(r.stdout)  # 整体恰一 JSON 文档
        self.assertNotIn("astrocs:", r.stderr or "", "正常路径 stderr 无报错")

if __name__ == "__main__":
    unittest.main(verbosity=2)
