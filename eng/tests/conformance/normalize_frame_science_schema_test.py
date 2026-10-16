#!/usr/bin/env python3
"""frame_science.json schema 符合性（正例/负例）—— eng/tests/conformance 层。

正本: eng/contracts/schemas/normalize_frame_science.schema.json（机器形状）,
     docs/engineering/contracts/NORMALIZE_DATA.md §7/§8（语义）。

判别力来源（expected 不由被测实现反推）:
  * 正例 = 仓库夹具 eng/contracts/data/examples/normalize_frame_science.*.example.json
    （前台定稿, 非本次生成）;
  * 生产 serializer 输出（eng/tests/unit/frame_science_sidecar_test.cpp）必须通过同一
    schema —— 证明 C++ writer 与 schema 同形;
  * 负例 = 对正例做具名形状破坏, 断言 schema 拒绝（无判别力的恒绿门不写）;
  * 物理关系（§8 末段: 引用不悬空/F-V_F-SNR 配对/摘要由有效 controls 得出）由
    C++ reader 侧对拍（见 eng/tests/unit/frame_science_sidecar_test.cpp）。

用法:
    python3 eng/tests/conformance/normalize_frame_science_schema_test.py
退出码恒 0（报告器语义; 红项打印供人读, 不作门禁）。
"""
from __future__ import annotations

import copy
import json
import os
import subprocess
import sys
import tempfile

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))
SCHEMA = os.path.join(REPO, "eng", "contracts", "schemas",
                      "normalize_frame_science.schema.json")
FIX_AVAIL = os.path.join(REPO, "eng", "contracts", "data", "examples",
                         "normalize_frame_science.available.example.json")
FIX_UNAVAIL = os.path.join(REPO, "eng", "contracts", "data", "examples",
                           "normalize_frame_science.unavailable.example.json")
CPP_TEST = os.path.join(REPO, "eng", "tests", "unit",
                        "frame_science_sidecar_test.cpp")

PASS: list[str] = []
FAIL: list[str] = []


def report(ok: bool, name: str, detail: str = "") -> None:
    (PASS if ok else FAIL).append(name)
    print(f"  [{'PASS' if ok else 'FAIL'}] {name}" + (f" — {detail}" if detail else ""))


def load(path: str):
    with open(path, "r", encoding="utf-8") as f:
        return json.load(f)


def validate(schema, instance) -> list[str]:
    import jsonschema  # type: ignore
    v = jsonschema.Draft202012Validator(schema)
    return [f"{'/'.join(str(p) for p in e.path)}: {e.message}"
            for e in v.iter_errors(instance)]


def main() -> int:
    print("=" * 78)
    print("frame_science.json schema 符合性 · 报告器（不设门禁）")
    print("=" * 78)
    try:
        import jsonschema  # noqa: F401
    except ImportError:
        print("SKIP: jsonschema 未安装（pip install --break-system-packages jsonschema）")
        return 0

    schema = load(SCHEMA)

    # 1. schema 自身可加载
    try:
        jsonschema.Draft202012Validator.check_schema(schema)
        report(True, "schema itself loads (Draft 2020-12)")
    except Exception as e:  # noqa: BLE001
        report(False, "schema itself loads (Draft 2020-12)", str(e))
        return 0

    # 2. 前台夹具正例
    for name, path in (("available fixture", FIX_AVAIL), ("unavailable fixture", FIX_UNAVAIL)):
        errs = validate(schema, load(path))
        report(not errs, f"fixture {name} passes schema", "; ".join(errs[:2]))

    # 3. 生产 serializer 输出必须同形（C++ 单元测试写出真实文件）
    with tempfile.TemporaryDirectory() as td:
        # 直接编译单测（与 eng/tests/unit 相同的编译面; 链接 sha256 + aio）
        inc = [
            os.path.join(REPO, "lib", "include"),
            os.path.join(REPO, "lib", "algorithms", "noise_snr", "include"),
            os.path.join(REPO, "lib", "algorithms", "integration",
                         "normalize_science", "include"),
            os.path.join(REPO, "lib", "third_party"),
            os.path.join(REPO, "lib", "infrastructure", "aio", "src"),
            os.path.join(REPO, "lib", "infrastructure", "aio", "include"),
            os.path.join(REPO, "lib", "algorithms", "shared"),
        ]
        exe = os.path.join(td, "fs_sidecar_test")
        cc = ["g++", "-std=c++17", "-O1"]
        for i in inc:
            cc += ["-I", i]
        aio = os.path.join(REPO, "build", "linux-sci", "libacsd_aio.a")
        cmd = cc + [CPP_TEST,
                    os.path.join(REPO, "lib", "algorithms", "integration",
                                 "normalize_science", "src", "normalize_science.cpp"),
                    os.path.join(REPO, "lib", "algorithms", "noise_snr", "cpp",
                                 "src", "sparse_reference.cpp"),
                    os.path.join(REPO, "lib", "algorithms", "shared", "crypto",
                                 "sha256.cpp"),
                    aio, "-lz", "-lpthread", "-o", exe]
        r = subprocess.run(cmd, capture_output=True, text=True)
        if r.returncode != 0:
            report(False, "build sidecar unit test", r.stderr[-300:])
        else:
            out = os.path.join(td, "fsout")
            env = dict(os.environ, FRAME_SCIENCE_OUT=out)
            rr = subprocess.run([exe], capture_output=True, text=True, env=env)
            cpp_all_pass = "ALL PASS" in rr.stdout
            report(cpp_all_pass, "sidecar C++ unit test ALL PASS",
                   "" if cpp_all_pass else rr.stdout[-400:])
            produced = os.path.join(out, "frame_science.json")
            if os.path.exists(produced):
                errs = validate(schema, load(produced))
                report(not errs, "production serializer output passes schema",
                       "; ".join(errs[:3]))
                # §7: JSON 禁 NaN/Inf（schema 的 number 不查 NaN ⇒ 文本层扫描）
                with open(produced, "r", encoding="utf-8") as f:
                    text = f.read()
                report("NaN" not in text and "Infinity" not in text,
                       "no NaN/Infinity tokens in produced sidecar")
                # leaf_ipix 必须为十进制字符串（防 double 丢 64-bit 索引）
                doc = load(produced)
                ctrl = doc["precision"]["controls"][0]
                report(isinstance(ctrl["leaf_ipix"], str) and
                       ctrl["leaf_ipix"] == "1152921504606846976",
                       "leaf_ipix serialized as decimal string (>2^53 preserved)")
            else:
                report(False, "production serializer output written", produced)

    # 4. 负例: 具名形状破坏必须被拒（否则该门恒绿, 无证据资格）
    base = load(FIX_AVAIL)
    negatives = {
        "missing required top-level key": lambda d: d.pop("frame_id"),
        "wrong enum token (signal.scale_state)": lambda d: d["signal"].update(
            scale_state="whatever"),
        "wrong type (leaf_order as string)": lambda d: d["signal"].update(
            leaf_order="9"),
        "control sky not [ra,dec] pair": lambda d: d["precision"]["controls"][0].update(
            sky={"ra_deg": 10.0}),
        "leaf_ipix as number (must be decimal string)": lambda d: d["precision"]
            ["controls"][0].update(leaf_ipix=1000),
        "frame_summary method wrong token": lambda d: d["precision"]["frame_summary"]
            .update(method="plain_mean"),
        "drizzle.normalization wrong token": lambda d: d["drizzle"].update(
            normalization="whatever"),
        "calibration.uncertainty wrong token": lambda d: d["calibration"].update(
            uncertainty="magic"),
        "control without status": lambda d: d["precision"]["controls"][0].pop("status"),
        "wcs.source wrong token (ipv|explicit_config only)": lambda d: d["wcs"].update(
            source="plate_solve_external"),
    }
    for name, mutate in negatives.items():
        d = copy.deepcopy(base)
        mutate(d)
        errs = validate(schema, d)
        report(bool(errs), f"negative: {name} rejected", "; ".join(errs[:1]))

    # 5. 附加: unknown key must be rejected (additionalProperties: false)
    d = copy.deepcopy(base)
    d["unknown_top_level"] = 1
    errs = validate(schema, d)
    report(bool(errs), "negative: unknown top-level key rejected", "; ".join(errs[:1]))

    print("-" * 78)
    print(f"PASS {len(PASS)} / FAIL {len(FAIL)}（报告器, 退出码恒 0）")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
