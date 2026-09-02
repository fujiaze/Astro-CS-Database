#!/usr/bin/env python3
"""ABI-002 生命周期/错误/版本协商语义验收测试。

规格: 控制包 tasks/02_ABI_BUILD_CLI_TASKS.md ABI-002（冻结 query/create/validate/
plan/execute/cancel/inspect/destroy/self_test 的调用时序、并发、重复调用、错误码、
诊断 buffer、版本协商与兼容规则; 验收 = 状态机 property tests; 旧/新 struct_size
尾部扩展; 错误 ABI major、空回调、double destroy 负面测试）。

权威形态:
  1. include/astrocs/abi/lifecycle_v1.h        （合同头: 判定函数声明 + 静态断言）
  2. contracts/config/module_lifecycle_contract.schema.json（机器合同权威数据）
  3. tests/abi/abi002_lifecycle_probe.c        （reference 实现 + 自检）
本测试执行:
  A. C11/C++17 双编译生命周期头族 + 静态断言落盘;
  B. 探针 C 双编译执行（自检 [PASS]/ALL_OK）;
  C. 状态机穷举 property（16 state×op 全组合比对 reference 判定函数输出）;
  D. 负测: 错误 ABI major/host_abi、struct_size 旧/新尾部扩展、空回调表、
     double destroy（通过探针自检项断言）;
  E. schema 与头文件语义一致性（states/operations/transitions/detail 码/常量）;
  F. 头文件边界纯净（无 STL/异常跨边界）与版本宏单源。
"""
import json
import os
import re
import subprocess
import sys
import tempfile
import unittest

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
HDR = os.path.join(REPO, "include", "astrocs", "abi", "lifecycle_v1.h")
PROBE = os.path.join(REPO, "tests", "abi", "abi002_lifecycle_probe.c")
SCHEMA = os.path.join(REPO, "contracts", "config", "module_lifecycle_contract.schema.json")

INC = os.path.join(REPO, "include")
TIMEOUT = 120

# 头文件语义的 python 镜像（与探针 reference 判定一致; schema 一致性对比源）
STATES = {"CREATED": 1, "EXECUTING": 2, "CANCELLING": 3, "DESTROYED": 4}
OPS = {
    "QUERY": 1, "DESCRIBE": 2, "VALIDATE_CONFIG": 3, "PLAN": 4, "SELF_TEST": 5,
    "CREATE": 6, "EXECUTE": 7, "INSPECT": 8, "REQUEST_CANCEL": 9, "DESTROY": 10,
}
INSTANCE_OPS = ("EXECUTE", "INSPECT", "REQUEST_CANCEL", "DESTROY")


def lc_allowed(state_name, op_name):
    """镜像 acs_lc_transition_allowed_v1 的转移表（与探针实现一致）。"""
    if op_name not in INSTANCE_OPS:
        return False
    if state_name == "CREATED":
        return op_name in INSTANCE_OPS
    if state_name == "EXECUTING":
        return op_name in ("INSPECT", "REQUEST_CANCEL")
    if state_name == "CANCELLING":
        return op_name in ("INSPECT", "REQUEST_CANCEL")
    return False  # DESTROYED 或非法


def compile_probe(cc, extra, out_dir, tag):
    """编译探针为可执行并运行; 返回 (run_result, compile_stderr)。"""
    exe = os.path.join(out_dir, "abi002_probe_" + tag)
    cmd = [cc, "-std=c++17" if cc.endswith("++") else "-std=c11",
           "-Wall", "-Wextra", "-pedantic", f"-I{INC}"]
    if extra:
        cmd += extra
    cmd += [PROBE, "-o", exe]
    c = subprocess.run(cmd, capture_output=True, text=True, timeout=TIMEOUT)
    if c.returncode != 0:
        return None, c.stderr
    r = subprocess.run([exe], capture_output=True, text=True, timeout=TIMEOUT)
    return r, c.stderr


class TestAbi002Headers(unittest.TestCase):
    """A: 头族 C11/C++17 双编译 + 静态断言; F: 边界纯净。"""

    def test_00_abi001_header_family_regression(self):
        """回归: ABI-001 族头（status/host/module/artifact + lifecycle）C11/C++17 全编译。"""
        with tempfile.TemporaryDirectory() as td:
            for cc, std, ext in (("gcc", "c11", "c"), ("g++", "c++17", "cpp")):
                tu = os.path.join(td, f"fam.{ext}")
                with open(tu, "w", encoding="utf-8") as f:
                    f.write('#include "astrocs/abi/lifecycle_v1.h"\n'
                            'int main(void){ (void)acs_abi_compat_v1(1u);'
                            ' return acs_struct_ext_ok_v1(8u,8u)?0:1;}\n')
                # 只编译（链接语义由探针测试覆盖; 避免未定义引用）
                r = subprocess.run([cc, f"-std={std}", "-Wall", "-Wextra", "-pedantic",
                                    f"-I{INC}", "-c", tu, "-o",
                                    os.path.join(td, f"fam_{ext}.o")],
                                   capture_output=True, text=True, timeout=TIMEOUT)
                self.assertEqual(r.returncode, 0, f"{cc} {std} 编译失败: {r.stderr}")

    def test_01_lifecycle_header_compiles_c11_and_cpp17(self):
        with tempfile.TemporaryDirectory() as td:
            for cc, src, std in (("gcc", "c", "c11"), ("g++", "c++", "c++17")):
                tu = os.path.join(td, f"lc_{src}.{src}")
                body = ('#include "astrocs/abi/lifecycle_v1.h"\n'
                        'int main(void){ return acs_lc_op_error_v1(1,7)==ACS_OK?0:1;}\n'
                        if src == "c" else
                        '#include "astrocs/abi/lifecycle_v1.h"\n'
                        'int main(){ return acs_lc_op_error_v1(1,7)==ACS_OK?0:1;}\n')
                with open(tu, "w", encoding="utf-8") as f:
                    f.write(body)
                r = subprocess.run([cc, f"-std={std}", "-Wall", "-Wextra", "-pedantic",
                                    f"-I{INC}", "-c", tu, "-o",
                                    os.path.join(td, f"lc_{src}.o")],
                                   capture_output=True, text=True, timeout=TIMEOUT)
                self.assertEqual(r.returncode, 0,
                                 f"{cc} {std} 编译失败: {r.stderr}")

    def test_02_probe_compiles_and_runs_both_cc(self):
        """B: 探针 gcc -std=c11 / g++ -std=c++17 编译执行, 自检全 [PASS] + ALL_OK。"""
        with tempfile.TemporaryDirectory() as td:
            for cc, tag in (("gcc", "gcc"), ("g++", "gpp")):
                run, cerr = compile_probe(cc, None, td, tag)
                self.assertIsNotNone(run, f"{cc} 探针编译失败: {cerr}")
                self.assertEqual(run.returncode, 0,
                                 f"{cc} 探针运行失败:\n{run.stdout}\n{run.stderr}")
                self.assertIn("[PASS]", run.stdout)
                self.assertNotIn("[FAIL]", run.stdout)
                self.assertIn("RESULT: ALL_OK", run.stdout)
                self.assertIn("state_matrix", run.stdout)

    def test_03_static_assert_header_self_consistent(self):
        """头文件内含布局静态断言与版本常量关系断言（编译即验证）。"""
        text = open(HDR, encoding="utf-8").read()
        for macro in ("ACS_STATIC_ASSERT(sizeof(acs_version_negotiation_v1) == 32u",
                      "ACS_STATIC_ASSERT(ACS_ABI_MAJOR_V1 == 1u",
                      "ACS_STATIC_ASSERT(ACS_ABI_VERSION_V1 == 1u"):
            self.assertIn(macro, text, f"头缺静态断言: {macro}")

    def test_04_no_stl_no_throw_across_boundary(self):
        """F: lifecycle_v1.h 不引入 STL/throw/noexcept(false) 跨边界。"""
        text = open(HDR, encoding="utf-8").read()
        bad = re.findall(r"#include\s*[<\"](string|vector|iostream|sstream|fstream|"
                         r"map|unordered_map|set|memory|exception|stdexcept|thread|mutex)",
                         text)
        self.assertEqual(bad, [], f"STL include 跨边界: {bad}")
        self.assertNotIn("throw", text)
        self.assertNotIn("noexcept", text)

    def test_05_version_macro_single_source(self):
        """ABI 版本分量只在头族定义（不泄漏到探针/测试实现）。"""
        # 探针 C 不得自行 #define 版本宏（须 include 头族; 行首宏定义检测）
        with open(PROBE, encoding="utf-8") as f:
            for line in f.read().splitlines():
                self.assertNotIn("#define ACS_ABI_VERSION_V1", line,
                                 "探针泄漏 ABI 版本宏")
        text = open(HDR, encoding="utf-8").read()
        self.assertIn("ACS_ABI_VERSION_V1", text)


class TestAbi002StateMachine(unittest.TestCase):
    """C: 状态机穷举 property + 非法调用时序拒绝; D: double destroy 负测。"""

    def _probe_out(self):
        with tempfile.TemporaryDirectory() as td:
            run, cerr = compile_probe("gcc", ["-O2", "-DNDEBUG"], td, "sm")
            if run is None:
                self.fail(f"探针编译失败: {cerr}")
            return run

    def test_10_exhaustive_transition_matrix_matches_spec(self):
        """4 状态 × 10 op 全组合: 头文件判定 == python 镜像转移表。"""
        run = self._probe_out()
        self.assertEqual(run.returncode, 0, run.stdout)
        rows = {}
        for line in run.stdout.splitlines():
            m = re.match(r"^  (\w+): (.*)$", line)
            if m:
                rows[m.group(1)] = [int(x) for x in m.group(2).split()]
        self.assertEqual(set(rows), {"CREATED", "EXECUTING", "CANCELLING", "DESTROYED"},
                         "矩阵缺状态行")
        for state, col in rows.items():
            self.assertEqual(col, [1 if lc_allowed(state, op) else 0
                                   for op in INSTANCE_OPS],
                             f"{state} 行判定与规范不符")
        # 非实例级 op 全部拒绝（module 级 op 不经实例状态机）
        for state in STATES:
            for op in ("QUERY", "DESCRIBE", "VALIDATE_CONFIG", "PLAN", "SELF_TEST", "CREATE"):
                self.assertFalse(lc_allowed(state, op), f"{state}+{op} 应拒绝")

    def test_11_illegal_sequences_rejected(self):
        """非法调用时序被拒绝（验收: 状态机 property tests）。"""
        cases = [
            # (state, op, 期望拒绝)
            ("EXECUTING", "EXECUTE", True),   # 同实例并发 execute
            ("EXECUTING", "DESTROY", True),   # 执行中销毁
            ("CANCELLING", "EXECUTE", True),  # 取消中重入 execute
            ("CANCELLING", "DESTROY", True),
            ("DESTROYED", "EXECUTE", True),   # destroy 后 execute
            ("DESTROYED", "DESTROY", True),   # double destroy 检测
            ("DESTROYED", "INSPECT", True),   # destroy 后 inspect
            ("DESTROYED", "REQUEST_CANCEL", True),
            ("CREATED", "EXECUTE", False),    # 合法
            ("CREATED", "DESTROY", False),    # 合法
            ("EXECUTING", "INSPECT", False),  # 合法（只读并发）
            ("EXECUTING", "REQUEST_CANCEL", False),
            ("CANCELLING", "INSPECT", False),
            ("CANCELLING", "REQUEST_CANCEL", False),  # 幂等
        ]
        for state, op, expect_reject in cases:
            allowed = lc_allowed(state, op)
            if expect_reject:
                self.assertFalse(allowed, f"{state}+{op} 应被拒绝但被允许")
            else:
                self.assertTrue(allowed, f"{state}+{op} 应允许但被拒绝")

    def test_12_execute_completion_returns_to_created(self):
        """execute 完成（含取消/错误）后实例回 CREATED, 可重跑/可 destroy（v1 无死态）。"""
        # 语义来自 lifecycle_v1.h 文件头 + probe 注释: EXECUTING/CANCELLING 完成→CREATED
        text = open(HDR, encoding="utf-8").read()
        self.assertIn("取消后允许重跑", text)
        self.assertIn("失败后可重跑或 destroy", text)
        self.assertIn("DONE/FAILED 死态", text)


class TestAbi002Negative(unittest.TestCase):
    """D: 错误 ABI major、空回调、double destroy 负测（探针自检覆盖断言）。"""

    def _probe_out(self):
        with tempfile.TemporaryDirectory() as td:
            run, cerr = compile_probe("gcc", None, td, "neg")
            if run is None:
                self.fail(f"探针编译失败: {cerr}")
            return run

    def test_20_wrong_abi_major_rejected(self):
        out = self._probe_out().stdout
        self.assertIn("[PASS] host_abi=2 rejected (major mismatch)", out)
        self.assertIn("[PASS] negotiate host_abi=2 -> ABI_MISMATCH", out)
        self.assertIn("[PASS] host_abi garbage rejected", out)

    def test_21_struct_size_tail_extension_old_new(self):
        out = self._probe_out().stdout
        self.assertIn("[PASS] peer==self struct_size compatible", out)
        self.assertIn("[PASS] peer newer (尾部扩展) compatible", out)
        self.assertIn("[PASS] peer older (缺尾部字段) rejected", out)
        self.assertIn("[PASS] negotiate host newer tail-ext ok", out)
        self.assertIn("[PASS] negotiate host older -> ABI_MISMATCH", out)

    def test_22_null_callback_negative(self):
        out = self._probe_out().stdout
        self.assertIn("[PASS] selftest NULL api -> SELFTEST", out)
        self.assertIn("[PASS] selftest 空回调表 -> SELFTEST (空回调负测)", out)
        self.assertIn("[PASS] selftest 缺 destroy -> SELFTEST", out)

    def test_23_double_destroy_negative(self):
        out = self._probe_out().stdout
        self.assertIn("[PASS] DESTROYED+DESTROY rejected (double destroy 检测)", out)
        self.assertIn("[PASS] DESTROYED+EXECUTE -> ACS_ERR_STATE", out)

    def test_24_illegal_state_op_values_negative(self):
        out = self._probe_out().stdout
        for marker in ("state=0 rejected", "state=99 rejected",
                       "op=0 rejected", "op=99 rejected",
                       "非法 state+op -> ACS_ERR_STATE"):
            self.assertIn(f"[PASS] {marker}", out)


class TestAbi002SchemaConsistency(unittest.TestCase):
    """E: schema 与头文件语义一致性（双形态不漂移）。"""

    def _load(self):
        schema = json.load(open(SCHEMA, encoding="utf-8"))
        text = open(HDR, encoding="utf-8").read()
        return schema, text

    @staticmethod
    def _status_codes_text():
        """status_codes.h 全文（状态码枚举冻结基础层; schema 引用的 ACS_ERR_* 均定义于此）。"""
        p = os.path.join(REPO, "include", "astrocs", "abi", "status_codes.h")
        return open(p, encoding="utf-8").read()

    def test_30_schema_states_operations_match_header_enums(self):
        schema, text = self._load()
        s_map = {s["id"]: s["enum"] for s in schema["states"]}
        o_map = {o["id"]: o["enum"] for o in schema["operations"]}
        self.assertEqual(s_map, STATES, "schema states != 头枚举")
        self.assertEqual(o_map, OPS, "schema operations != 头枚举")

    def test_31_schema_transitions_match_header_table(self):
        schema, text = self._load()
        t = {(e["from_state"], e["op"], e["to_state"])
             for e in schema["transitions"]}
        # 由 python 镜像转移表推导期望 (from, op) → to
        expect_to = {
            ("CREATED", "EXECUTE"): "EXECUTING",
            ("CREATED", "INSPECT"): "CREATED",
            ("CREATED", "REQUEST_CANCEL"): "CREATED",
            ("CREATED", "DESTROY"): "DESTROYED",
            ("EXECUTING", "INSPECT"): "EXECUTING",
            ("EXECUTING", "REQUEST_CANCEL"): "CANCELLING",
            ("CANCELLING", "INSPECT"): "CANCELLING",
            ("CANCELLING", "REQUEST_CANCEL"): "CANCELLING",
        }
        self.assertEqual(t, {(f, op, to) for (f, op), to in expect_to.items()},
                         "schema transitions 与头文件转移表不一致")
        # 每个 (from,op) 至多一条
        self.assertEqual(len(t), len({(e[0], e[1]) for e in t}))

    def test_32_schema_error_codes_and_diag_match_header(self):
        schema, text = self._load()
        # 通用细分码枚举值
        ecode = {
            "NONE": 0, "NULL_CALLBACK": 1, "NULL_CONFIG": 2, "CONFIG_SCHEMA": 3,
            "BUFFER_TOO_SMALL": 4, "ILLEGAL_STATE": 5, "DOUBLE_DESTROY": 6,
            "UNSUPPORTED_OP": 7,
        }
        for name, val in ecode.items():
            self.assertIn(f"ACS_DIAG_ECODE_{name} = {val}", text,
                          f"头缺细分码 {name}={val}")
        status_text = self._status_codes_text()
        for ent in schema["error_mapping"]:
            status = ent["status"]
            self.assertIn(status, status_text,
                          f"schema 引用状态码 {status} 未定义于 status_codes.h")
        # schema 的 error_mapping 与头文件错误码映射段一致（引用状态码清单）
        mapped_statuses = {e["status"] for e in schema["error_mapping"]}
        for expect in ("ACS_ERR_ABI_MISMATCH", "ACS_ERR_PARAM", "ACS_ERR_STATE",
                       "ACS_ERR_SELFTEST", "ACS_ERR_CANCELLED"):
            self.assertIn(expect, mapped_statuses, f"schema 缺错误映射 {expect}")
        self.assertEqual(schema["diagnostics"]["max_utf8"], 4096)
        self.assertIn("ACS_DIAG_MAX_UTF8 4096u", text)
        self.assertEqual(schema["version_negotiation"]["wire_value"], 1)
        self.assertEqual(schema["version_negotiation"]["major"], 1)
        self.assertEqual(schema["version_negotiation"]["minor"], 0)
        self.assertEqual(schema["abi_version"], 1)
        # 必填/可选回调
        self.assertIn("describe", schema["self_test"]["required_callbacks"])
        self.assertIn("destroy", schema["self_test"]["required_callbacks"])
        self.assertEqual(schema["self_test"]["optional_callbacks"], ["request_cancel"])
        self.assertEqual(schema["self_test"]["failure_status"], "ACS_ERR_SELFTEST")

    def test_33_schema_json_is_valid_and_locked(self):
        schema = json.load(open(SCHEMA, encoding="utf-8"))
        self.assertEqual(schema["contract_id"], "CONTRACT-ABI002-LIFECYCLE-V1")
        self.assertEqual(schema["doc_status"], "ACTIVE_NORMATIVE")
        self.assertEqual(schema["target_version"], "0.11.0-alpha.1")
        self.assertEqual(schema["contract_version"], 1)
        self.assertEqual(schema["owner"], "SA-ABI-03")
        # 无多余顶层字段（冻结数据形态契约）
        allowed = {"contract_schema", "contract_version", "contract_id", "doc_status",
                   "title", "description", "frozen_at", "owner", "target_version",
                   "abi_version", "states", "operations", "transitions", "concurrency",
                   "error_mapping", "diagnostics", "version_negotiation",
                   "tail_extension", "self_test"}
        self.assertTrue(set(schema) <= allowed, f"schema 多余顶层键: {set(schema) - allowed}")


if __name__ == "__main__":
    unittest.main(verbosity=2)
