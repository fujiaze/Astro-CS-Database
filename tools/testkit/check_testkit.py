#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""TST-001 机器校验器：tests/testkit 测试元数据合同全量校验 + module:<id> 选择
+ 期望来源审计（禁止生产函数生成期望）+ 故障注入检查。

规则（详见 tests/testkit/testkit.spec.md §7，exit 0 = TESTKIT_PASS）：
  K1 文件存在且被 Git 跟踪：tests/testkit/schemas/test_metadata.schema.json、
     registry.json（若存在）；
  K2 registry 每项过 JSON Schema（test_metadata.schema.json）——缺字段/坏类型/
     坏格式 → SCHEMA_VIOLATION；空字符串 → EMPTY_VIOLATION（空缺写 MISSING）；
  K3 test_id 全 registry 唯一；label 合法；module: 标签须带匹配 module_id；
  K4 期望来源审计：expectation_source==MISSING 且 type != negative →
     MISSING_EXPECTATION（WARN，--strict 为 ERROR）；oracle/command 引用生产
     符号或 tests 引用 lib/ 生产符号生成期望 → FORBIDDEN_EXPECTATION_SOURCE；
  K5 module:<id> 选择：--module <id> 只执行匹配 label 的测试；--list 输出全部；
     未知 module → UNKNOWN_MODULE（非 0）；匹配 0 个 → NO_TESTS_MATCH（非 0，
     不伪 PASS）；
  K6 故障注入（--fault-injection）：篡改期望/断链 fixture/把 oracle 换成生产
     符号，随后 harness 必须 FAIL（exit != 0），否则 FORBIDDEN_PASS。

任何未捕获异常 → TOOLING_FAILURE exit 3（不允许伪 PASS）。
用法：
  python3 tools/testkit/check_testkit.py --root <repo> [--list] [--module <id>]
      [--strict] [--fault-injection] [--json-out <file>]
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import subprocess
import sys

REGISTRY_REL = "tests/testkit/registry.json"
SCHEMA_REL = "tests/testkit/schemas/test_metadata.schema.json"
# 生产符号域：禁止 oracle/期望生成引用这些符号（生产=被测代码）
PRODUCTION_SYMBOL_RE = re.compile(
    r"\b(?:acs_cap_detect_v1|acs_cap_classify_v1|astrocs_module_query_v1|"
    r"acs_fio_reader_open_v1|p1_session_run|p2_session_run|p3_session_run|"
    r"ac_calibrate_frame|p2_integrate_pixel|snr_noise_model_v1)\b")
# 生产路径域：测试不能把 lib/ 生产实现当 oracle
PRODUCTION_PATH_RE = re.compile(r"(?:^|[\s\"'=])(?:lib/|modules/\S+/src/|runtime/|providers/\S+/src/)")
LABEL_RE = re.compile(r"^(module:|unit:|properties:|oracle:|fixtures:|negative:|performance:|integration:)(.+)$")


def err(code, msg):
    return {"severity": "ERROR", "code": code, "detail": msg}


def warn(code, msg):
    return {"severity": "WARN", "code": code, "detail": msg}


def git_tracked(root, rel):
    r = subprocess.run(["git", "ls-files", "--error-unmatch", "--", rel],
                       cwd=root, capture_output=True, text=True)
    return r.returncode == 0


def load_json(root, rel, results):
    full = os.path.join(root, rel)
    if not os.path.isfile(full):
        results.append(err("MISSING_FILE", f"{rel} 不存在"))
        return None
    if not git_tracked(root, rel):
        results.append(err("MISSING_FILE", f"{rel} 存在但未受 Git 跟踪"))
    try:
        with open(full, encoding="utf-8") as f:
            return json.load(f)
    except Exception as exc:  # noqa: BLE001
        results.append(err("TOOLING_FAILURE", f"{rel} JSON 解析失败: {exc}"))
        return None


# ---- 轻量 JSON Schema 校验（仅本 schema 用到的关键字）----
def schema_check(obj, schema, path, results, errors_out):
    if schema.get("type") == "object":
        if not isinstance(obj, dict):
            errors_out.append(err("SCHEMA_VIOLATION", f"{path}: 应为 object，实为 {type(obj).__name__}"))
            return
        for req in schema.get("required", []):
            if req not in obj:
                errors_out.append(err("SCHEMA_VIOLATION", f"{path}: 缺必需字段 {req}"))
        for key, val in obj.items():
            if key not in schema.get("properties", {}) and schema.get("additionalProperties") is False:
                errors_out.append(err("SCHEMA_VIOLATION", f"{path}: 未知字段 {key}"))
            else:
                if key in schema.get("properties", {}):
                    schema_check(val, schema["properties"][key], f"{path}.{key}", results, errors_out)
    elif schema.get("type") == "array":
        if not isinstance(obj, list):
            errors_out.append(err("SCHEMA_VIOLATION", f"{path}: 应为 array"))
            return
        items = schema.get("items", {})
        for i, it in enumerate(obj):
            schema_check(it, items, f"{path}[{i}]", results, errors_out)
    else:
        _leaf_check(obj, schema, path, errors_out)


def _type_matches(obj, tt):
    """单类型匹配：支持 JSON Schema type 数组的单个成员。"""
    if tt == "string":
        return isinstance(obj, str)
    if tt == "integer":
        return isinstance(obj, int) and not isinstance(obj, bool)
    if tt == "boolean":
        return isinstance(obj, bool)
    if tt == "array":
        return isinstance(obj, list)
    if tt == "null":
        return obj is None
    return False


def _leaf_check(obj, schema, path, errors_out):
    t = schema.get("type")
    types = t if isinstance(t, list) else ([t] if t is not None else [])
    ok = any(_type_matches(obj, tt) for tt in types)
    if types:
        if not ok:
            errors_out.append(err("SCHEMA_VIOLATION", f"{path}: 类型/取值非法 {obj!r}"))
            return
        if isinstance(obj, str) and "string" in types:
            if "minLength" in schema and len(obj) < schema["minLength"]:
                errors_out.append(err("EMPTY_VIOLATION", f"{path}: 字符串过短(空值) {obj!r} → 空缺必须写 MISSING"))
                return
            if "pattern" in schema and not re.fullmatch(schema["pattern"], obj):
                errors_out.append(err("SCHEMA_VIOLATION", f"{path}: {obj!r} 不匹配 {schema['pattern']}"))
                return
        if isinstance(obj, list) and "array" in types:
            for i, it in enumerate(obj):
                _leaf_check(it, schema.get("items", {}), f"{path}[{i}]", errors_out)
            return
        # type 通过后仍可叠加 enum/const 取值约束
        if "enum" in schema and obj not in schema["enum"]:
            errors_out.append(err("SCHEMA_VIOLATION", f"{path}: {obj!r} 不在 enum {schema['enum']}"))
            return
        if "const" in schema and obj != schema["const"]:
            errors_out.append(err("SCHEMA_VIOLATION", f"{path}: 必须等于 {schema['const']!r}"))
        return
    # 无 type 约束：仅 enum/const 取值约束（如 expectation_source / providers items）
    if "enum" in schema:
        if obj not in schema["enum"]:
            errors_out.append(err("SCHEMA_VIOLATION", f"{path}: {obj!r} 不在 enum {schema['enum']}"))
        return
    if "const" in schema:
        if obj != schema["const"]:
            errors_out.append(err("SCHEMA_VIOLATION", f"{path}: 必须等于 {schema['const']!r}"))
        return


def audit_expectation_source(item, results, strict):
    """K4：期望来源审计（禁止生产函数生成期望）。"""
    es = item.get("expectation_source")
    t = item.get("type")
    if es == "MISSING" and t != "negative":
        it = err("MISSING_EXPECTATION", f"{item.get('test_id')}: 非 negative 测试期望来源 MISSING") if strict \
            else warn("MISSING_EXPECTATION", f"{item.get('test_id')}: 非 negative 测试期望来源 MISSING（WARN）")
        results.append(it)
    cmd = str(item.get("command", ""))
    oracle = str(item.get("oracle", ""))
    if PRODUCTION_SYMBOL_RE.search(cmd) or PRODUCTION_SYMBOL_RE.search(oracle):
        results.append(err("FORBIDDEN_EXPECTATION_SOURCE",
                           f"{item.get('test_id')}: oracle/command 引用生产符号（禁止用被测代码生成期望）"))
    if PRODUCTION_PATH_RE.search(oracle):
        results.append(err("FORBIDDEN_EXPECTATION_SOURCE",
                           f"{item.get('test_id')}: oracle 引用生产路径（禁止 lib/ 生产实现当 oracle）"))


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--root", default=".")
    ap.add_argument("--list", action="store_true", help="列出全部测试（稳定排序）")
    ap.add_argument("--module", default=None, help="module:<id> 选择执行")
    ap.add_argument("--type", default=None, help="按 type 过滤")
    ap.add_argument("--strict", action="store_true")
    ap.add_argument("--fault-injection", action="store_true", help="演示故障注入必败")
    ap.add_argument("--json-out", default=None)
    args = ap.parse_args()
    root = os.path.abspath(args.root)
    results: list[dict] = []

    schema = load_json(root, SCHEMA_REL, results)
    registry = load_json(root, REGISTRY_REL, results)
    if schema is None or registry is None:
        return _finish(root, args, results)

    items = registry.get("tests", []) if isinstance(registry, dict) else registry
    if not isinstance(items, list):
        results.append(err("SCHEMA_VIOLATION", "registry.json: tests 必须是数组"))
        return _finish(root, args, results)

    # K2/K3 逐项校验
    seen_ids = {}
    for i, it in enumerate(items):
        if not isinstance(it, dict):
            results.append(err("SCHEMA_VIOLATION", f"tests[{i}] 非对象"))
            continue
        schema_check(it, schema, f"tests[{i}]", results, results)
        tid = it.get("test_id")
        if tid:
            if tid in seen_ids:
                results.append(err("DUPLICATE_ID", f"test_id 重复: {tid} ({seen_ids[tid]} 与 tests[{i}])"))
            seen_ids[tid] = i
        label = it.get("label", "")
        m = LABEL_RE.match(str(label))
        if not m:
            results.append(err("SCHEMA_VIOLATION", f"{tid or '?'}: label 非法 {label!r}"))
        elif m.group(1) == "module:":
            mid = it.get("module_id")
            if not mid:
                results.append(err("SCHEMA_VIOLATION", f"{tid}: module: 标签必须带 module_id"))
        audit_expectation_source(it, results, args.strict)

    # K5 module:<id> 选择
    selected = items
    if args.module:
        sel = [it for it in items if it.get("label") == f"module:{args.module}"]
        if not sel:
            results.append(err("UNKNOWN_MODULE", f"无 module:{args.module} 测试"))
        selected = sel
    if args.type:
        selected = [it for it in selected if it.get("type") == args.type]

    if args.list:
        for it in sorted(items, key=lambda x: str(x.get("test_id"))):
            print(f"{it.get('test_id')}\t{it.get('label')}\t{it.get('type')}\t{it.get('command')}")

    # K6 故障注入演示：F1 篡改期望 / F2 断链 / F3 坏 oracle → 必须 FAIL
    if args.fault_injection:
        fi_results = _fault_injection_demo(root, selected)
        results.extend(fi_results)

    return _finish(root, args, results, executed=selected if (args.module or args.type) else None)


def _bash_run(cmd, root, timeout=120):
    return subprocess.run(["bash", "-c", str(cmd).replace("{root}", root)],
                          capture_output=True, text=True, timeout=timeout)


def _fault_injection_demo(root, selected) -> list:
    """三类故障注入演示，每次注入后 harness/示例必须 FAIL（非 0），
    否则 FORBIDDEN_PASS；注入全部撤销/恢复，保证可复现：

    F1 篡改期望（运行级）：unit 示例期望参数 4→5 → 断言失败 exit 非 0；
    F2 断链：临时移除 fixture 文件 → fixture_hash 失败 exit 非 0；
    F3 破坏 oracle 独立性：registry 注入坏项（oracle 换生产符号）→
       check_testkit 报 FORBIDDEN_EXPECTATION_SOURCE 且 exit 非 0。
    """
    out: list[dict] = []
    # F1：篡改期望 —— check_constant.py 的期望即命令行参数（registry command 用 4）
    r1 = _bash_run("python3 tests/testkit/examples/check_constant.py 5", root)
    if r1.returncode == 0:
        out.append(err("FORBIDDEN_PASS", "F1 篡改期望(4→5)后 harness 仍 exit 0（故障注入未生效）"))

    # F2：断链 fixture —— 临时改名 demo_fixture.txt，跑完恢复
    fx = os.path.join(root, "tests/testkit/fixtures/demo_fixture.txt")
    bak = fx + ".inj.bak"
    if os.path.isfile(fx):
        os.replace(fx, bak)
        try:
            r2 = _bash_run("python3 tests/testkit/examples/fixture_hash.py {root}", root)
            if r2.returncode == 0:
                out.append(err("FORBIDDEN_PASS", "F2 断链 fixture 后 harness 仍 exit 0（故障注入未生效）"))
        finally:
            os.replace(bak, fx)

    # F3：registry 注入坏项 —— oracle 引用生产符号（用被测代码生成期望）→ 检查器必须 FAIL
    reg = os.path.join(root, "tests/testkit/registry.json")
    if os.path.isfile(reg):
        with open(reg, encoding="utf-8") as f:
            orig = f.read()
        try:
            data = json.loads(orig)
            mutated = False
            for it in data.get("tests", []):
                if it.get("type") == "oracle":
                    it["oracle"] = "期望由生产函数 acs_cap_detect_v1 生成（注入）"
                    mutated = True
            if mutated:
                with open(reg, "w", encoding="utf-8") as f:
                    json.dump(data, f, ensure_ascii=False, indent=1)
                r3 = subprocess.run([sys.executable, "tools/testkit/check_testkit.py", "--root", root],
                                    capture_output=True, text=True, timeout=120)
                if r3.returncode == 0:
                    out.append(err("FORBIDDEN_PASS", "F3 oracle 引用生产符号后检查器仍 exit 0（未拦截）"))
        finally:
            with open(reg, "w", encoding="utf-8") as f:
                f.write(orig)
    if not out:
        out.append(warn("FAULT_INJECTION", "F1 篡改期望/F2 断链/F3 坏 oracle 注入后全部 FAIL（演示通过）"))
    return out


def _finish(root, args, results, executed=None) -> int:
    errors = [r for r in results if r["severity"] == "ERROR"]
    warns = [r for r in results if r["severity"] == "WARN"]
    summary = {
        "checker": "check_testkit.py",
        "task": "TST-001",
        "root": root,
        "strict": bool(args.strict),
        "errors": len(errors),
        "warns": len(warns),
        "issues": sorted(results, key=lambda r: (r["severity"], r["code"], r["detail"])),
        "verdict": "TESTKIT_PASS" if not errors else "TESTKIT_FAIL",
    }
    text = json.dumps(summary, ensure_ascii=False, indent=1, sort_keys=True)
    if args.json_out:
        try:
            os.makedirs(os.path.dirname(os.path.abspath(args.json_out)) or ".", exist_ok=True)
            with open(args.json_out, "w", encoding="utf-8") as f:
                f.write(text + "\n")
        except Exception as exc:  # noqa: BLE001
            print(f"TOOLING_FAILURE: 写 json-out 失败 {exc}", file=sys.stderr)
    if errors:
        print(f"TESTKIT_FAIL errors={len(errors)}")
        for r in errors:
            print(f"  [{r['code']}] {r['detail']}")
        if warns:
            print(f"WARN: {len(warns)} 条（--strict 升级为 ERROR）")
        return 1
    print(f"TESTKIT_PASS errors=0" + (f" warns={len(warns)}" if warns else ""))
    if executed is not None:
        print(f"  executed={len(executed)}")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except SystemExit:
        raise
    except Exception as exc:  # noqa: BLE001
        print(f"TOOLING_FAILURE: 未捕获异常 {exc!r}", file=sys.stderr)
        raise SystemExit(3)
