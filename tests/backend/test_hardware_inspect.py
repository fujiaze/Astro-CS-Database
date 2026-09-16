#!/usr/bin/env python3
"""BENCH-001 硬件画像: CLI 域断言 + 宿主探针（ASTROCS_DESIGN §6.2 新树同步）。

CLI-001 删除 hardware inspect 用户命令（§6.2 唯一命令树）后本文件的处置（TEST-CLI-SYNC）:
  * CLI 域断言保留并改新树: hardware inspect → rc=2（命令不存在是**正确行为**）;
    §6.2 的宿主自检载体是 doctor [--json]（本节保留其 JSON 合同断言）;
  * schema/affinity/CPU identity 断言 → **迁移到宿主探针**（不依赖 CLI）: 测试在
    临时目录写一个极小 main 调 lib/infrastructure/benchmark/backend_host/
    hardware_inspect.h::hardware_inspect_json_v1（与生产同实现）, 编译运行后对
    contracts/schemas/hardware_inspect.schema.json 断言。探针源码路径按 ARCH-001
    迁移后布局（lib/infrastructure/benchmark/backend_host + lib/algorithms/shared/crypto）,
    旧路径回退;
  * 退役登记: 原 test_02/test_03/test_04/test_05 经 CLI hardware inspect 取画像的
    断言形态退役（命令不存在）; 其**语义**（affinity 约束/CPU identity/单 JSON 文档）
    在本文件宿主探针路径上继续断言, 能力不静默丢失;
  * 缺口登记（归 CPU-001）: doctor --json 只输出 baseline_selftest/hardware_sanity/
    backends_manifest 三项检查, **不承载** hardware profile 字段; 即删除 hardware
    inspect 后, 硬件画像没有 tracked 的 CLI 报告面（唯一出口是 benchmark 写安装目录
    cpu_profile.json 的 host 子集, 缺 affinity/ram_bytes/page_size/feature_bits）。
"""
import json, os, platform, re, shutil, subprocess, sys, tempfile, unittest

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
SCHEMA = json.load(open(os.path.join(REPO, "contracts", "schemas", "hardware_inspect.schema.json"),
                        encoding="utf-8"))
sys.path.insert(0, os.path.join(REPO, "tools"))
import gen_version  # noqa: E402


def cli_binary():
    env = os.environ.get("ASTROCS_CLI_BIN")
    if env and os.path.isfile(env):
        return env
    for rel in (("build", "astrocs"), ("build", "cli", "astrocs")):
        cand = os.path.join(REPO, *rel)
        if os.path.isfile(cand):
            return cand
    return os.path.join(REPO, "build", "astrocs")


EXE = cli_binary()


def _pick_dir(*cands):
    for p in cands:
        if os.path.isdir(p):
            return p
    return cands[0]


HOST = _pick_dir(os.path.join(REPO, "lib", "infrastructure", "benchmark", "backend_host"),
                 os.path.join(REPO, "lib", "backend_host"))
CRYPTO = _pick_dir(os.path.join(REPO, "lib", "algorithms", "shared", "crypto"),
                   os.path.join(REPO, "lib", "common", "crypto"))

PROBE_MAIN = r"""
#include <cstdio>
#include <string>
#include "hardware_inspect.h"
int main(int argc, char** argv) {
    const std::string build = argc > 1 ? argv[1] : std::string("0.0.0-alpha.0+g000000000000");
    const std::string s = astrocs::backend_host::hardware_inspect_json_v1(build);
    std::fputs(s.c_str(), stdout);
    std::fputc('\n', stdout);
    return 0;
}
"""


class TestHardwareInspectCliDomain(unittest.TestCase):
    """CLI 域: 旧命令不存在（rc=2）+ doctor --json 是 §6.2 的宿主自检面。"""

    @classmethod
    def setUpClass(cls):
        assert os.path.isfile(EXE), "先构建 CLI（cmake -S . -B build && ninja -C build astrocs）"

    def test_01_hardware_inspect_command_removed_rc2(self):
        r = subprocess.run([EXE, "hardware", "inspect", "--json"], capture_output=True,
                           text=True, timeout=60)
        self.assertEqual(r.returncode, 2, "旧命令必须 rc=2（§6.2 唯一命令树）")
        self.assertIn("unknown command", r.stderr)
        self.assertEqual(r.stdout, "")

    def test_02_doctor_json_is_the_self_check_surface(self):
        r = subprocess.run([EXE, "doctor", "--json"], capture_output=True, text=True, timeout=120)
        self.assertEqual(r.returncode, 0, r.stderr)
        doc = json.loads(r.stdout)   # 整体恰一 JSON 文档
        self.assertEqual(doc["kind"], "astrocs_doctor")
        self.assertEqual(doc["verdict"], "PASS")
        names = [c["name"] for c in doc["checks"]]
        self.assertIn("hardware_sanity", names)
        # 缺口登记（CPU-001）: doctor 不承载 hardware profile 字段
        for k in ("affinity", "feature_bits", "ram_bytes", "page_size", "vendor"):
            self.assertNotIn(k, doc, "doctor 若承载硬件画像字段, 请把 CPU-001 缺口标记移除")

    def test_03_doctor_requires_json(self):
        r = subprocess.run([EXE, "doctor"], capture_output=True, text=True, timeout=60)
        self.assertEqual(r.returncode, 2)
        self.assertEqual(r.stdout, "")


class TestHardwareInspectProbe(unittest.TestCase):
    """宿主探针（不经 CLI）: schema/affinity/CPU identity — BENCH-001 语义保留。"""

    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.mkdtemp(prefix="hw_")
        main = os.path.join(cls.tmp, "hw_probe_main.cpp")
        with open(main, "w", encoding="utf-8") as fh:
            fh.write(PROBE_MAIN)
        cls.probe = os.path.join(cls.tmp, "hw_probe")
        srcs = [os.path.join(HOST, "hardware_inspect.cpp"),
                os.path.join(HOST, "cpu_features.cpp"),
                os.path.join(HOST, "host_services.cpp"),
                os.path.join(HOST, "backend_loader.cpp"),   # file_sha256_hex
                os.path.join(CRYPTO, "sha256.cpp")]
        r = subprocess.run(["g++", "-std=c++17", "-O2", "-Wno-format-truncation",
                            f"-I{os.path.join(REPO, 'include')}", f"-I{HOST}",
                            f"-I{CRYPTO}", f"-I{os.path.join(REPO, 'third_party')}",
                            main, *srcs, "-ldl", "-o", cls.probe],
                           capture_output=True, text=True, timeout=600)
        assert r.returncode == 0, "硬件探针编译失败（lib 路径迁移中间态?）:\n" + r.stderr[-800:]

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.tmp, ignore_errors=True)

    def _inspect(self, *prefix):
        build = gen_version.read_base_version() + "+g0123456789ab"
        r = subprocess.run([*prefix, self.probe, build], capture_output=True, text=True,
                           timeout=120)
        self.assertEqual(r.returncode, 0, r.stderr)
        return json.loads(r.stdout)

    def test_01_schema_valid(self):
        """schema 验证: required 全齐+additionalProperties=false(最小校验器, 免依赖)。"""
        d = self._inspect()
        for k in SCHEMA["required"]:
            self.assertIn(k, d, "缺 required %s" % k)
        props = SCHEMA["properties"]
        for k, v in d.items():
            self.assertIn(k, props, "字段 %s 不在 schema(additionalProperties=false)" % k)
            rule = props[k]
            if "const" in rule:
                self.assertEqual(v, rule["const"], k)
            if rule.get("type") == "integer" and isinstance(v, int) and "minimum" in rule:
                self.assertGreaterEqual(v, rule["minimum"], k)

    def test_02_ground_truth_affinity_constrained(self):
        """available_cpus 受 affinity 约束(禁 hardware_concurrency 单源)。"""
        d = self._inspect()
        aff = sorted(os.sched_getaffinity(0))
        self.assertEqual(d["affinity"], aff, "affinity 数组与 sched_getaffinity 一致")
        self.assertEqual(d["affinity_count"], len(aff))
        self.assertEqual(d["available_logical_cpus"], len(aff), "可用 CPU=affinity 实测")
        self.assertEqual(d["logical_cpus_configured"], os.cpu_count())

    def test_03_cpu_identity_matches_proc(self):
        d = self._inspect()
        with open("/proc/cpuinfo", encoding="utf-8") as fh:
            cpuinfo = fh.read()
        m = re.search(r"vendor_id\s*:\s*(\S+)", cpuinfo)
        if m:
            self.assertEqual(d["vendor"], m.group(1))
        self.assertGreater(d["feature_bits"], 0, "feature_bits 实测>0")
        self.assertIn("sse2", d["feature_names"])
        self.assertGreater(d["ram_bytes"], 0)
        self.assertEqual(d["page_size"], os.sysconf("SC_PAGESIZE"))
        self.assertIn(platform.machine(), ("x86_64",))
        self.assertRegex(d["astrocs_build"], r"^" + re.escape(gen_version.read_base_version())
                         + r"\+g[0-9a-f]{12}")

    def test_04_affinity_one_cpu_fixture(self):
        """fixture: taskset 单 CPU 下 available_cpus 必须降为 1(mock 环境 vs 实机比对)。"""
        if not shutil.which("taskset"):
            self.skipTest("taskset 不可用")
        if len(os.sched_getaffinity(0)) < 2:
            self.skipTest("宿主本身 1 CPU")
        d = self._inspect("taskset", "-c", "0")
        self.assertEqual(d["available_logical_cpus"], 1, "affinity=1 → 可用=1")
        self.assertEqual(d["affinity"], [0])

    def test_05_stdout_single_json_document(self):
        build = gen_version.read_base_version() + "+g0123456789ab"
        r = subprocess.run([self.probe, build], capture_output=True, text=True, timeout=120)
        json.loads(r.stdout)  # 整体恰一 JSON 文档
        self.assertNotIn("astrocs:", r.stderr or "", "正常路径 stderr 无报错")


if __name__ == "__main__":
    unittest.main(verbosity=2)
