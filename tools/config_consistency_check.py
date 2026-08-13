#!/usr/bin/env python3
# V14 G5：config 默认值一致性校验
# 对照 stage2_common.h（struct 默认）与 stage2_common.cpp（parser 默认），
# 以及 stage2 模板 JSON。返回 config_consistency.json。
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
HDR = ROOT / "lib/phase2/include/astro/phase2/stage2_common.h"
SRC = ROOT / "lib/phase2/src/stage2_common.cpp"
OUT = ROOT / "run/temp/p2_v14/evidence/config_consistency.json"

def struct_defaults(text):
    d = {}
    for m in re.finditer(
        r"^\s*(?:int|double|bool|std::uint64_t|std::string)\s+([a-z_0-9]+)\s*=\s*([^;]+);",
        text, re.M):
        name, val = m.group(1), m.group(2).strip()
        d[name] = val
    return d

def parser_defaults(text):
    d = {}
    # 只取 model 段（mcfg）与顶层；min_samples 区分 model(5)/rejection(2)
    # 按段截取：model 段 = '"model"' 到 '"integration"'
    mstart = text.find('"model"')
    mend = text.find('"integration"')
    seg = text[mstart:mend if mend > mstart else len(text)]
    # 只匹配单 token 默认值：数字、字符串、std::string("...")
    for m in re.finditer(
        r'\.value\(\s*"([a-z_0-9]+)"\s*,\s*'
        r'(?:std::string\("([^"]*)"\)|\(std::uint64_t\)([0-9]+)|'
        r'([-0-9.]+(?:[eE][-+]?[0-9]+)?))', seg):
        name = m.group(1)
        if m.group(2) is not None:
            d[name] = '"' + m.group(2) + '"'
        elif m.group(3) is not None:
            d[name] = m.group(3)
        else:
            d[name] = m.group(4)
    return d

# parser 字符串 → struct 枚举的映射（同一语义的两种表达）
ENUM = {
    "robust_loss": {"huber": "0"},
    "snr_weight_mode": {"snr2_normalized": "0"},
    "weight_mode": {"auto": "0"},
    "precision": {"fp32": "0"},
    "acr_route": {"auto": '"auto"'},
}

sd = struct_defaults(HDR.read_text(encoding="utf-8"))
pd = parser_defaults(SRC.read_text(encoding="utf-8"))

problems = []
for k in sorted(set(sd) & set(pd)):
    a, b = sd[k], pd[k]
    if k in ENUM:
        b = ENUM[k].get(b.strip('"'), b)
    same = a.replace(" ", "") == b.replace(" ", "")
    if not same:
        # 数值等价（如 1e-3 vs 0.001）
        try:
            same = abs(float(a) - float(b)) < 1e-12
        except ValueError:
            same = False
    if not same:
        problems.append({"key": k, "struct_default": a, "parser_default": b})
only_struct = sorted(set(sd) - set(pd))
only_parser = sorted(set(pd) - set(sd))
res = {
    "checked_keys": sorted(set(sd) | set(pd)),
    "mismatches": problems,
    "only_in_struct": only_struct,
    "only_in_parser": only_parser,
    "pass": len(problems) == 0,
}
OUT.parent.mkdir(parents=True, exist_ok=True)
OUT.write_text(json.dumps(res, ensure_ascii=False, indent=2), encoding="utf-8")
print(json.dumps(res, ensure_ascii=False, indent=2))
sys.exit(0 if res["pass"] else 1)
