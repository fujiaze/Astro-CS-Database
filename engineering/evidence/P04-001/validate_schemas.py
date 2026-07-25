#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
P04-001 JSON Schema 验证脚本

验证内容:
1. cli_request_schema.json 本身是合法的 JSON Schema
2. effective_config_schema.json 本身是合法的 JSON Schema
3. 一组合法 request 样例通过 cli_request_schema 校验
4. 一组非法 request 样例被 cli_request_schema 拒绝
5. orchestrator inspect 实际输出的 effective_config 通过 effective_config_schema 校验

输出: 标准 stdout 输出 PASS/FAIL 行, 退出码 0=全部通过, 非0=有失败
"""
import json
import os
import subprocess
import sys
import re
from pathlib import Path

try:
    import jsonschema
    from jsonschema import Draft202012Validator
except ImportError:
    print("[FATAL] jsonschema 未安装, 请运行: pip install jsonschema")
    sys.exit(2)


# ----------------------------------------------------------------------------
# 路径配置
# ----------------------------------------------------------------------------
REPO_ROOT = Path(r"f:\Astro dev\Astro CS Normalization Database")
SCHEMA_DIR = REPO_ROOT / "engineering" / "contracts"
EVIDENCE_DIR = REPO_ROOT / "engineering" / "evidence" / "P04-001"
ORCH_EXE = REPO_ROOT / "lib" / "orchestrator" / "cpp" / "orchestrator.exe"

REQUEST_SCHEMA_PATH = SCHEMA_DIR / "cli_request_schema.json"
EFFECTIVE_CONFIG_SCHEMA_PATH = SCHEMA_DIR / "effective_config_schema.json"


# ----------------------------------------------------------------------------
# 测试计数
# ----------------------------------------------------------------------------
PASS_COUNT = 0
FAIL_COUNT = 0


def check(cond, msg):
    """断言函数, 累计 PASS/FAIL 计数"""
    global PASS_COUNT, FAIL_COUNT
    if cond:
        print(f"  [PASS] {msg}")
        PASS_COUNT += 1
    else:
        print(f"  [FAIL] {msg}")
        FAIL_COUNT += 1


# ----------------------------------------------------------------------------
# 加载 schema
# ----------------------------------------------------------------------------
def load_schema(path):
    with open(path, "r", encoding="utf-8") as f:
        return json.load(f)


# ----------------------------------------------------------------------------
# 测试 1: schema 文件本身可解析为合法 JSON Schema
# ----------------------------------------------------------------------------
def test_schema_files_valid():
    print("\n[Section] 测试 1: schema 文件本身可解析为合法 JSON Schema")

    # cli_request_schema.json
    try:
        request_schema = load_schema(REQUEST_SCHEMA_PATH)
        check(True, f"cli_request_schema.json 可解析 ({REQUEST_SCHEMA_PATH.name})")
    except Exception as e:
        check(False, f"cli_request_schema.json 解析失败: {e}")
        return None

    try:
        Draft202012Validator.check_schema(request_schema)
        check(True, "cli_request_schema.json 符合 JSON Schema Draft 2020-12 规范")
    except jsonschema.SchemaError as e:
        check(False, f"cli_request_schema.json 不是合法 schema: {e.message}")
        return None

    # effective_config_schema.json
    try:
        ec_schema = load_schema(EFFECTIVE_CONFIG_SCHEMA_PATH)
        check(True, f"effective_config_schema.json 可解析 ({EFFECTIVE_CONFIG_SCHEMA_PATH.name})")
    except Exception as e:
        check(False, f"effective_config_schema.json 解析失败: {e}")
        return None

    try:
        Draft202012Validator.check_schema(ec_schema)
        check(True, "effective_config_schema.json 符合 JSON Schema Draft 2020-12 规范")
    except jsonschema.SchemaError as e:
        check(False, f"effective_config_schema.json 不是合法 schema: {e.message}")
        return None

    return request_schema, ec_schema


# ----------------------------------------------------------------------------
# 测试 2: 合法 request 样例通过 cli_request_schema
# ----------------------------------------------------------------------------
VALID_REQUESTS = [
    {
        "name": "stage1 最小合法 request",
        "data": {
            "schema_version": 1,
            "command": "stage1",
            "frame": "testdata/T4 calibration files/Light_1.fits",
            "output": "output/result.hiss"
        }
    },
    {
        "name": "stage2 with frame",
        "data": {
            "schema_version": 1,
            "command": "stage2",
            "frame": "output/hiss_dir",
            "output": "output/result.hcsd"
        }
    },
    {
        "name": "stage2 with inputs 数组",
        "data": {
            "schema_version": 1,
            "command": "stage2",
            "inputs": ["output/a.hiss", "output/b.hiss"],
            "output": "output/result.hcsd"
        }
    },
    {
        "name": "stage1 + job_id + config(inline) + overrides",
        "data": {
            "schema_version": 1,
            "command": "stage1",
            "job_id": "job_abc123",
            "frame": "x.fits",
            "output": "y.hiss",
            "config": {"log_level": "INFO", "threads": 4},
            "overrides": {"threads": 8},
            "timeouts": {"stage1": 600}
        }
    },
    {
        "name": "inspect command",
        "data": {
            "schema_version": 1,
            "command": "inspect",
            "output": "out.hiss"
        }
    },
    {
        "name": "capabilities command",
        "data": {
            "schema_version": 1,
            "command": "capabilities",
            "output": "out.hiss"
        }
    },
    {
        "name": "config 字段为文件路径字符串",
        "data": {
            "schema_version": 1,
            "command": "stage1",
            "frame": "x.fits",
            "output": "y.hiss",
            "config": "config/stage1_config.json"
        }
    }
]


def test_valid_requests(request_schema):
    print("\n[Section] 测试 2: 合法 request 样例通过 cli_request_schema")
    validator = Draft202012Validator(request_schema)
    for case in VALID_REQUESTS:
        try:
            validator.validate(case["data"])
            check(True, f"合法: {case['name']}")
        except jsonschema.ValidationError as e:
            check(False, f"合法但被拒绝: {case['name']} - {e.message} (path={list(e.absolute_path)})")


# ----------------------------------------------------------------------------
# 测试 3: 非法 request 样例被 cli_request_schema 拒绝
# ----------------------------------------------------------------------------
INVALID_REQUESTS = [
    {
        "name": "缺少 schema_version",
        "data": {"command": "stage1", "frame": "x.fits", "output": "y.hiss"},
        "expected_error_contains": "schema_version"
    },
    {
        "name": "缺少 command",
        "data": {"schema_version": 1, "frame": "x.fits", "output": "y.hiss"},
        "expected_error_contains": "command"
    },
    {
        "name": "缺少 output",
        "data": {"schema_version": 1, "command": "stage1", "frame": "x.fits"},
        "expected_error_contains": "output"
    },
    {
        "name": "command 不在 enum 中",
        "data": {"schema_version": 1, "command": "invalid_cmd", "output": "y.hiss"},
        "expected_error_contains": "invalid_cmd"
    },
    {
        "name": "schema_version 非 1",
        "data": {"schema_version": 2, "command": "inspect", "output": "y.hiss"},
        "expected_error_contains": "schema_version"
    },
    {
        "name": "stage1 缺少 frame (allOf 条件)",
        "data": {"schema_version": 1, "command": "stage1", "output": "y.hiss"},
        "expected_error_contains": "frame"
    },
    {
        "name": "stage2 缺少 frame 和 inputs",
        "data": {"schema_version": 1, "command": "stage2", "output": "y.hiss"},
        "expected_error_contains": ""
    },
    {
        "name": "inputs 为空数组 (minItems=1)",
        "data": {"schema_version": 1, "command": "stage2", "inputs": [], "output": "y.hiss"},
        "expected_error_contains": "inputs"
    },
    {
        "name": "额外字段 (additionalProperties=false)",
        "data": {"schema_version": 1, "command": "inspect", "output": "y.hiss", "unknown_field": "x"},
        "expected_error_contains": "unknown_field"
    },
    {
        "name": "timeouts 值为 0 (exclusiveMinimum=0)",
        "data": {"schema_version": 1, "command": "inspect", "output": "y.hiss",
                 "timeouts": {"stage1": 0}},
        "expected_error_contains": "timeouts"
    }
]


def test_invalid_requests(request_schema):
    print("\n[Section] 测试 3: 非法 request 样例被 cli_request_schema 拒绝")
    validator = Draft202012Validator(request_schema)
    for case in INVALID_REQUESTS:
        try:
            validator.validate(case["data"])
            check(False, f"非法但被接受: {case['name']}")
        except jsonschema.ValidationError as e:
            # 验证错误信息符合预期
            err_msg = e.message.lower()
            expected = case["expected_error_contains"].lower()
            if expected == "" or expected in err_msg or expected in str(e.absolute_path).lower():
                check(True, f"非法被正确拒绝: {case['name']}")
            else:
                check(False, f"非法被拒绝但错误不符预期: {case['name']} "
                             f"(expected contains '{expected}', got '{e.message}', "
                             f"path={list(e.absolute_path)})")


# ----------------------------------------------------------------------------
# 测试 4: orchestrator inspect 输出的 effective_config 通过 schema
# ----------------------------------------------------------------------------
def test_orchestrator_effective_config(ec_schema):
    print("\n[Section] 测试 4: orchestrator inspect 实际输出通过 effective_config_schema")

    if not ORCH_EXE.exists():
        check(False, f"orchestrator.exe 不存在: {ORCH_EXE}")
        return

    # 创建测试 request 文件
    tmp_request = EVIDENCE_DIR / "_test_request_for_schema.json"
    request_data = {
        "schema_version": 1,
        "command": "stage1",
        "job_id": "schema_test_job",
        "frame": "nonexistent.fits",
        "output": "output/out.hiss",
        "config": {
            "log_level": "INFO",
            "threads": 4,
            "platesolve": {"max_stars": 1500}
        },
        "overrides": {
            "threads": 8,
            "log_level": "WARN"
        }
    }
    with open(tmp_request, "w", encoding="utf-8") as f:
        json.dump(request_data, f, indent=2)

    # 运行 orchestrator inspect
    cmd = [str(ORCH_EXE), "inspect", "--request", str(tmp_request)]
    try:
        result = subprocess.run(cmd, capture_output=True, text=True, timeout=30, encoding="utf-8")
    except Exception as e:
        check(False, f"运行 orchestrator inspect 失败: {e}")
        return

    if result.returncode != 0:
        check(False, f"orchestrator inspect 退出码非 0: {result.returncode}, stderr={result.stderr[:200]}")
        return

    # 解析 stdout 为 JSON
    try:
        ec_data = json.loads(result.stdout)
    except json.JSONDecodeError as e:
        check(False, f"inspect stdout 不是合法 JSON: {e}")
        return

    # 验证 effective_config_schema
    validator = Draft202012Validator(ec_schema)
    try:
        validator.validate(ec_data)
        check(True, "inspect 输出通过 effective_config_schema 校验")
    except jsonschema.ValidationError as e:
        check(False, f"inspect 输出不符合 schema: {e.message} (path={list(e.absolute_path)})")
        return

    # 验证关键字段
    check(ec_data.get("schema_version") == 1, "effective_config.schema_version == 1")
    check(ec_data.get("command") == "stage1", "effective_config.command == stage1")
    check(ec_data.get("job_id") == "schema_test_job", "effective_config.job_id 一致")
    check(isinstance(ec_data.get("config"), dict), "effective_config.config 是对象")
    check(isinstance(ec_data.get("sources"), dict), "effective_config.sources 是对象")

    # 验证 hash 格式 (64 位小写十六进制)
    hash_value = ec_data.get("effective_config_hash", "")
    hash_ok = isinstance(hash_value, str) and bool(re.match(r"^[0-9a-f]{64}$", hash_value))
    check(hash_ok, f"effective_config_hash 符合 ^[0-9a-f]{{64}}$ 格式 (前12位: {hash_value[:12]}...)")

    # 验证 sources 中的值都在 enum 中
    valid_sources = {"cli", "overrides", "config", "default"}
    sources = ec_data.get("sources", {})
    all_valid = all(v in valid_sources for v in sources.values())
    check(all_valid, f"sources 所有值都在 {valid_sources} 中")

    # 验证 created_at 字段存在
    check(bool(ec_data.get("created_at")), "effective_config.created_at 非空")

    # 保存实际输出作为证据
    out_path = EVIDENCE_DIR / "cli_request_effective_config.json"
    with open(out_path, "w", encoding="utf-8") as f:
        json.dump(ec_data, f, indent=2, ensure_ascii=False)
    print(f"  [INFO] 实际输出已保存: {out_path}")

    # 清理临时文件
    try:
        tmp_request.unlink()
    except Exception:
        pass


# ----------------------------------------------------------------------------
# 测试 5: 配置优先级语义验证 (深度检查)
# ----------------------------------------------------------------------------
def test_config_priority_semantics(ec_schema):
    print("\n[Section] 测试 5: 配置优先级语义深度验证 (default < config < overrides < cli)")

    if not ORCH_EXE.exists():
        check(False, f"orchestrator.exe 不存在: {ORCH_EXE}")
        return

    tmp_request = EVIDENCE_DIR / "_test_priority.json"
    request_data = {
        "schema_version": 1,
        "command": "inspect",
        "job_id": "priority_test",
        "output": "out.hiss",
        "config": {
            "threads": 4,
            "log_level": "INFO",
            "gaia_data_dir": "config_specified_gaia_dir"
        },
        "overrides": {
            "threads": 8,
            "log_level": "WARN"
        }
    }
    with open(tmp_request, "w", encoding="utf-8") as f:
        json.dump(request_data, f, indent=2)

    cmd = [str(ORCH_EXE), "inspect", "--request", str(tmp_request)]
    try:
        result = subprocess.run(cmd, capture_output=True, text=True, timeout=30, encoding="utf-8")
    except Exception as e:
        check(False, f"运行 orchestrator inspect 失败: {e}")
        return

    if result.returncode != 0:
        check(False, f"inspect 退出码非 0: {result.returncode}")
        return

    try:
        ec_data = json.loads(result.stdout)
    except json.JSONDecodeError as e:
        check(False, f"inspect stdout 不是合法 JSON: {e}")
        return

    config = ec_data.get("config", {})
    sources = ec_data.get("sources", {})

    # threads: default=0 -> config=4 -> overrides=8, 期望 8 (overrides 覆盖)
    check(config.get("threads") == 8, f"threads=8 (overrides 覆盖 config=4), 实际={config.get('threads')}")
    check(sources.get("threads") == "overrides", f"sources.threads=overrides, 实际={sources.get('threads')}")

    # log_level: default=INFO -> config=INFO -> overrides=WARN, 期望 WARN
    check(config.get("log_level") == "WARN", f"log_level=WARN (overrides), 实际={config.get('log_level')}")
    check(sources.get("log_level") == "overrides", f"sources.log_level=overrides, 实际={sources.get('log_level')}")

    # gaia_data_dir: default=GaiaDR3SP -> config=config_specified_gaia_dir (无 overrides), 期望 config 值
    check(config.get("gaia_data_dir") == "config_specified_gaia_dir",
          f"gaia_data_dir=config_specified (config 覆盖 default), 实际={config.get('gaia_data_dir')}")
    check(sources.get("gaia_data_dir") == "config", f"sources.gaia_data_dir=config, 实际={sources.get('gaia_data_dir')}")

    # calibration_dir: 无 config/overrides, 期望 default 值
    check(config.get("calibration_dir") == "testdata/T4 calibration files",
          f"calibration_dir=default 值, 实际={config.get('calibration_dir')}")
    check(sources.get("calibration_dir") == "default", f"sources.calibration_dir=default, 实际={sources.get('calibration_dir')}")

    # 清理
    try:
        tmp_request.unlink()
    except Exception:
        pass


# ----------------------------------------------------------------------------
# 测试 6: hash 一致性 (相同输入产生相同 hash)
# ----------------------------------------------------------------------------
def test_hash_consistency():
    print("\n[Section] 测试 6: SHA-256 hash 一致性 (相同输入产生相同 hash)")

    if not ORCH_EXE.exists():
        check(False, f"orchestrator.exe 不存在: {ORCH_EXE}")
        return

    tmp_request = EVIDENCE_DIR / "_test_hash_consistency.json"
    request_data = {
        "schema_version": 1,
        "command": "inspect",
        "job_id": "hash_consistency_test",
        "output": "out.hiss",
        "config": {"threads": 2, "log_level": "DEBUG"}
    }
    with open(tmp_request, "w", encoding="utf-8") as f:
        json.dump(request_data, f, indent=2)

    cmd = [str(ORCH_EXE), "inspect", "--request", str(tmp_request)]
    hashes = []
    for i in range(3):
        try:
            result = subprocess.run(cmd, capture_output=True, text=True, timeout=30, encoding="utf-8")
            if result.returncode != 0:
                check(False, f"第 {i+1} 次 inspect 失败: exit={result.returncode}")
                return
            ec_data = json.loads(result.stdout)
            hashes.append(ec_data.get("effective_config_hash"))
        except Exception as e:
            check(False, f"第 {i+1} 次 inspect 异常: {e}")
            return

    check(len(set(hashes)) == 1, f"3 次 inspect hash 全部一致: {hashes[0]}")
    check(len(hashes[0]) == 64, f"hash 长度 64: {len(hashes[0])}")

    # 清理
    try:
        tmp_request.unlink()
    except Exception:
        pass


# ----------------------------------------------------------------------------
# 测试 7: stdout/stderr 严格分离
# ----------------------------------------------------------------------------
def test_stdout_stderr_separation():
    print("\n[Section] 测试 7: stdout/stderr 严格分离 (stdout=JSON, stderr=日志)")

    if not ORCH_EXE.exists():
        check(False, f"orchestrator.exe 不存在: {ORCH_EXE}")
        return

    tmp_request = EVIDENCE_DIR / "_test_separation.json"
    request_data = {
        "schema_version": 1,
        "command": "inspect",
        "job_id": "separation_test",
        "output": "out.hiss",
        "config": {"log_level": "INFO"}
    }
    with open(tmp_request, "w", encoding="utf-8") as f:
        json.dump(request_data, f, indent=2)

    cmd = [str(ORCH_EXE), "inspect", "--request", str(tmp_request)]
    try:
        result = subprocess.run(cmd, capture_output=True, text=True, timeout=30, encoding="utf-8")
    except Exception as e:
        check(False, f"运行 inspect 失败: {e}")
        return

    check(result.returncode == 0, f"inspect 退出码 0, 实际={result.returncode}")

    # stdout 应为可解析 JSON
    try:
        ec_data = json.loads(result.stdout)
        check(True, "stdout 可解析为 JSON (机器可读)")
    except json.JSONDecodeError as e:
        check(False, f"stdout 不是合法 JSON: {e}")

    # stderr 应非空 (LOG_INFO 输出)
    check(bool(result.stderr), "stderr 非空 (日志输出)")
    # stderr 应包含日志格式 (含 cli 模块名)
    check("cli" in result.stderr or "inspect" in result.stderr,
          f"stderr 包含模块名 (cli/inspect), 实际前 200 字符: {result.stderr[:200]!r}")

    # stdout 不应包含日志格式行 (不含 [INFO] 等)
    has_log_in_stdout = ("[INFO]" in result.stdout or "[WARN]" in result.stdout or
                          "[ERROR]" in result.stdout or "[DEBUG]" in result.stdout)
    check(not has_log_in_stdout, "stdout 不含日志格式标记 ([INFO]/[WARN]/[ERROR]/[DEBUG])")

    # 保存分离证据
    sep_evidence = {
        "stdout_first_line": result.stdout.split("\n")[0] if result.stdout else "",
        "stdout_is_json": True,
        "stderr_first_line": result.stderr.split("\n")[0] if result.stderr else "",
        "stderr_line_count": len(result.stderr.split("\n")) if result.stderr else 0
    }
    sep_path = EVIDENCE_DIR / "stdout_stderr_separation.json"
    with open(sep_path, "w", encoding="utf-8") as f:
        json.dump(sep_evidence, f, indent=2, ensure_ascii=False)
    print(f"  [INFO] 分离证据已保存: {sep_path}")

    # 清理
    try:
        tmp_request.unlink()
    except Exception:
        pass


# ----------------------------------------------------------------------------
# 主函数
# ----------------------------------------------------------------------------
def main():
    print("=" * 60)
    print("P04-001 JSON Schema 验证 + 端到端语义测试")
    print("=" * 60)

    schemas = test_schema_files_valid()
    if schemas is None:
        print("\n[FATAL] schema 文件加载失败, 终止测试")
        sys.exit(2)

    request_schema, ec_schema = schemas
    test_valid_requests(request_schema)
    test_invalid_requests(request_schema)
    test_orchestrator_effective_config(ec_schema)
    test_config_priority_semantics(ec_schema)
    test_hash_consistency()
    test_stdout_stderr_separation()

    print("\n" + "=" * 60)
    print(f"测试汇总: {PASS_COUNT} 通过, {FAIL_COUNT} 失败")
    print("=" * 60)

    sys.exit(0 if FAIL_COUNT == 0 else 1)


if __name__ == "__main__":
    main()
