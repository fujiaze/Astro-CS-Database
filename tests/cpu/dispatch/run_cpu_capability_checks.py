#!/usr/bin/env python3
"""AstroCS CPU-001 能力探测检查编排 — tests/cpu/dispatch/run_cpu_capability_checks.py

验收 (04_CPU_RESOURCE_TASKS.md CPU-001):
  1. 模拟 feature matrix (合成 CPUID/XCR0 证据驱动生产判定) → 全 PASS;
  2. 缺 AVX / 缺 OS state / 缺 AVX-512 子集 / OS 不保存 ZMM → 拒绝 (负测);
  3. Windows/Linux 输出同一 schema — 本机 probe JSON 逐字段校验
     providers/cpu/common/schemas/cpu_capability.schema.json (同一事实源;
     Windows 实机输出由 WIN-* 以同 schema 校验);
  4. 不读取硬编码核心数 — probe 输出无 core 计数/线程字段 (schema 亦无);
     判定只读 CPUID+XCR0 位面。

本 runner:
  - gcc/clang -std=c11 严格 warning 编译 capability_detect.c + 两个测试 main;
  - 运行 matrix 负测 (exit 0);
  - 运行 probe 两次, 校验 JSON 合法 + schema 一致 + 双跑逐字节稳定;
  - 自实现 schema 校验器 (仅本 schema 用到的关键字: type/const/enum/required/
    additionalProperties/properties/uniqueItems/minimum/maximum), 从 schema 文件
    读取 → 与生产 schema 无漂移。

依赖: 仅标准库 (python3.8+); jsonschema 第三方库不要求。
"""
import json
import os
import subprocess
import sys
import tempfile

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(
    os.path.abspath(__file__)))))
INC = os.path.join(REPO, "providers", "cpu", "common", "include")
SRC = os.path.join(REPO, "providers", "cpu", "common", "src", "capability_detect.c")
SCHEMA = os.path.join(REPO, "providers", "cpu", "common", "schemas",
                      "cpu_capability.schema.json")
MATRIX = os.path.join(REPO, "tests", "cpu", "dispatch",
                      "cpu_capability_matrix_test.c")
PROBE = os.path.join(REPO, "tests", "cpu", "dispatch",
                     "cpu_capability_probe_main.c")

FAILURES = []


def log(msg):
    print(msg, flush=True)


def fail(msg):
    FAILURES.append(msg)
    log("FAIL: " + msg)


def run(cmd, cwd=REPO, timeout=180):
    r = subprocess.run(cmd, cwd=cwd, capture_output=True, text=True, timeout=timeout)
    log("$ " + " ".join(cmd) + f"\n  exit={r.returncode}")
    if r.stdout.strip():
        log("  stdout: " + r.stdout.strip()[-2000:])
    if r.stderr.strip():
        log("  stderr: " + r.stderr.strip()[-2000:])
    return r


def cc_compile(cc, src, out, extra=None):
    cmd = [cc, "-std=c11", "-Wall", "-Wextra", "-Wpedantic", "-Wconversion",
           f"-I{INC}", src, SRC, "-o", out]
    if extra:
        cmd.extend(extra)
    return run(cmd)


# ── 最小 JSON-schema 校验器 (本 schema 用到的关键字子集; 源 = schema 文件) ──
def schema_check(node, schema, path, errors):
    if not isinstance(schema, dict):
        return
    t = schema.get("type")
    if t == "object":
        if not isinstance(node, dict):
            errors.append(f"{path}: expect object, got {type(node).__name__}")
            return
        for k in schema.get("required", []):
            if k not in node:
                errors.append(f"{path}: missing required '{k}'")
        props = schema.get("properties", {})
        for k, v in node.items():
            if k not in props:
                if schema.get("additionalProperties") is False:
                    errors.append(f"{path}: unexpected property '{k}'")
                continue
            schema_check(v, props[k], f"{path}.{k}", errors)
        for k, sub in props.items():
            if k in node:
                schema_check(node[k], sub, f"{path}.{k}", errors)
    elif t == "array":
        if not isinstance(node, list):
            errors.append(f"{path}: expect array")
            return
        if schema.get("uniqueItems"):
            seen = []
            for it in node:
                if it in seen:
                    errors.append(f"{path}: duplicate item {it!r}")
                seen.append(it)
        item = schema.get("items", {})
        for i, it in enumerate(node):
            schema_check(it, item, f"{path}[{i}]", errors)
    elif t == "integer":
        if isinstance(node, bool) or not isinstance(node, int):
            errors.append(f"{path}: expect integer")
            return
        if "minimum" in schema and node < schema["minimum"]:
            errors.append(f"{path}: {node} < minimum {schema['minimum']}")
        if "maximum" in schema and node > schema["maximum"]:
            errors.append(f"{path}: {node} > maximum {schema['maximum']}")
    elif t == "boolean":
        if not isinstance(node, bool):
            errors.append(f"{path}: expect boolean")
    elif t == "string":
        if not isinstance(node, str):
            errors.append(f"{path}: expect string")
    if "const" in schema and node != schema["const"]:
        errors.append(f"{path}: const mismatch {node!r} != {schema['const']!r}")
    if "enum" in schema and node not in schema["enum"]:
        errors.append(f"{path}: value {node!r} not in enum")


def validate_against_schema(instance, schema_path):
    with open(schema_path, encoding="utf-8") as f:
        schema = json.load(f)
    errors = []
    schema_check(instance, schema, "$", errors)
    return errors


def main():
    tmp = tempfile.mkdtemp(prefix="cpu001_")
    matrix_bin = os.path.join(tmp, "cap_matrix")
    probe_bin = os.path.join(tmp, "cap_probe")

    log(f"repo={REPO}")
    log(f"matrix={MATRIX}\nsrc={SRC}\nschema={SCHEMA}")

    # 1) 编译 (gcc 与 clang 双编译器; 严格 warning 即语法/格式门)
    for cc in ("gcc", "clang"):
        rc = cc_compile(cc, MATRIX, matrix_bin + ("_g" if cc == "gcc" else "_c"))
        if rc.returncode != 0:
            fail(f"{cc} matrix compile")
    rc = cc_compile("gcc", PROBE, probe_bin)
    if rc.returncode != 0:
        fail("gcc probe compile")
    rc = cc_compile("clang", PROBE, probe_bin + "_c")
    if rc.returncode != 0:
        fail("clang probe compile")

    # 2) feature matrix 负测 (生产判定函数)
    r = run([matrix_bin + "_g"])
    if r.returncode != 0:
        fail("feature matrix (gcc) exit != 0")
    r = run([matrix_bin + "_c"])
    if r.returncode != 0:
        fail("feature matrix (clang) exit != 0")

    # 3) 本机 probe: JSON + schema + 双跑稳定
    outs = []
    for i in range(2):
        r = run([probe_bin], timeout=120)
        if r.returncode != 0:
            fail(f"probe run {i} exit != 0")
            continue
        text = r.stdout.strip()
        if not text:
            fail(f"probe run {i}: empty stdout")
            continue
        try:
            obj = json.loads(text)
        except json.JSONDecodeError as e:
            fail(f"probe run {i}: stdout 非合法 JSON: {e}")
            continue
        outs.append(text)
        errs = validate_against_schema(obj, SCHEMA)
        if errs:
            fail(f"probe run {i}: schema 校验失败: {'; '.join(errs[:10])}")
            continue
        # 语义: os_safe ⊆ hw; 位掩码与数组一致
        hw = obj["hw_features_bitmask"]
        osf = obj["os_safe_features_bitmask"]
        if (hw & osf) != osf:
            fail(f"probe run {i}: os_safe 位不在 hw 内 (hw=0x{hw:x} os=0x{osf:x})")
        def bit_of(name):
            names = ["sse2", "sse4_1", "sse4_2", "avx", "avx2", "fma", "bmi1",
                     "bmi2", "avx512f", "avx512cd", "avx512bw", "avx512dq",
                     "avx512vl"]
            return 1 << names.index(name)
        hw_names = sum(bit_of(n) for n in obj["features"]["hw"])
        os_names = sum(bit_of(n) for n in obj["features"]["os_safe"])
        if hw_names != hw or os_names != osf:
            fail(f"probe run {i}: feature 数组与位掩码不一致")
        log(f"probe run {i}: schema OK; hw=0x{hw:x} os_safe=0x{osf:x} "
            f"(sse2={bool(osf & 1)})")
    if len(outs) == 2 and outs[0] != outs[1]:
        fail("probe 双跑输出不一致 (schema 不稳定)")

    # 4) 不读取硬编码核心数: schema 无 core/thread/worker 字段; probe JSON 键集
    #    不包含逻辑核计数; 判定只读 CPUID/XCR0 (代码审查 + 本检查)
    with open(SCHEMA, encoding="utf-8") as f:
        schema = json.load(f)
    banned = {"core_count", "logical_cpus", "affinity", "threads", "workers",
              "num_cpus", "cores"}
    schema_keys = set(schema.get("required", [])) | set(schema.get("properties", {}))
    hit = banned & schema_keys
    if hit:
        fail(f"schema 含被禁核心计数键: {sorted(hit)}")

    # 5) JSON schema 文件本身合法 JSON
    with open(SCHEMA, encoding="utf-8") as f:
        json.load(f)
    log("schema file parses")

    if FAILURES:
        log(f"\nCPU-001 CHECKS FAIL ({len(FAILURES)})")
        return 1
    log("\nCPU-001 CHECKS PASS (matrix negative tests, live probe JSON schema, "
        "stability, no hardcoded core count)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
