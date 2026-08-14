#!/usr/bin/env python3
# V15 G6：config 默认值一致性校验（单语义单默认）
# 对照 stage2_common.h（struct 默认）与 stage2_common.cpp（parser 默认）
# 以及 工程控制/configs/stage2.template.json（JSON schema/template）。
# 覆盖 model 段 + integration.rejection 段（V15 method-specific typed 参数）。
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
HDR = ROOT / "lib/phase2/include/astro/phase2/stage2_common.h"
SRC = ROOT / "lib/phase2/src/stage2_common.cpp"
SCHEMA = ROOT / "工程控制/schemas/stage2.schema.json"
TEMPLATE = ROOT / "工程控制/configs/stage2.template.json"
OUT = ROOT / "run/temp/p2_v15/evidence/config_consistency.json"


def struct_defaults(text):
    d = {}
    for m in re.finditer(
        r"^\s*(?:int|double|bool|std::uint64_t|std::string|std::uint32_t)"
        r"\s+([a-z_0-9]+)\s*=\s*([^;]+);",
        text, re.M):
        d[m.group(1)] = m.group(2).strip()
    return d


def extract_values(seg):
    """提取 seg 内所有 .value("key", default) 的 (key, default)。"""
    out = {}
    for m in re.finditer(
        r'\.value\(\s*"([a-z_0-9]+)"\s*,\s*'
        r'(?:std::string\("([^"]*)"\)|\(std::uint64_t\)([0-9]+)|'
        r'\(std::uint32_t\)([0-9]+)|'
        r'([-0-9.]+(?:[eE][-+]?[0-9]+)?)|(true|false))', seg):
        key = m.group(1)
        if m.group(2) is not None:
            val = '"' + m.group(2) + '"'
        elif m.group(3) is not None:
            val = m.group(3)
        elif m.group(4) is not None:
            val = m.group(4)
        elif m.group(5) is not None:
            val = m.group(5)
        else:
            val = m.group(6)
        out.setdefault(key, val)  # 每 key 只取首个默认（同节内唯一）
    return out


def parser_defaults(text):
    d = {}
    # model 段（"model" → "integration"）
    mstart = text.find('"model"')
    mend = text.find('"integration"')
    model_seg = text[mstart:mend if mend > mstart else len(text)]
    d.update(extract_values(model_seg))
    # rejection 段（"rejection" → 该 if 块结束：取到 "weight_mode" 前）
    rstart = text.find('"rejection"')
    wstart = text.find('"weight_mode"', rstart)
    rej_seg = text[rstart:wstart if wstart > rstart else len(text)]
    d.update(extract_values(rej_seg))
    # method-specific typed 子段（robust_mad_clip 等）
    for block in re.finditer(
            r'if\s*\(\s*rj\.contains\("([a-z_0-9]+)"\)\s*\)\s*\{'
            r'(.*?)\n\s*\}', rej_seg, re.S):
        name = block.group(1)
        sub = extract_values(block.group(2))
        for k, v in sub.items():
            d[f"{name}.{k}"] = v
    # large_scale 子段（V17）：括号计数到匹配的闭括号（含嵌套 if）
    ls_start = rej_seg.find('rj.contains("large_scale")')
    if ls_start >= 0:
        brace = rej_seg.find('{', ls_start)
        depth = 0
        end = brace
        while end < len(rej_seg):
            if rej_seg[end] == '{':
                depth += 1
            elif rej_seg[end] == '}':
                depth -= 1
                if depth == 0:
                    break
            end += 1
        ls_block = rej_seg[brace:end + 1]
        for k, v in extract_values(ls_block).items():
            d[f"large_scale.{k}"] = v
    return d


def norm(v):
    return v.replace(" ", "")


def main():
    sd = struct_defaults(HDR.read_text(encoding="utf-8"))
    pd = parser_defaults(SRC.read_text(encoding="utf-8"))
    schema = json.loads(SCHEMA.read_text(encoding="utf-8"))
    template = json.loads(TEMPLATE.read_text(encoding="utf-8"))

    # 解析器 key → struct field 映射（含枚举表达映射）
    ALIAS = {
        "method": ("reject_method", {"\"auto\"": "10"}),
        "profile": ("reject_profile",
                    {"\"wbpp_current\"": "\"wbpp_2_9_1\""}),
        "normalization": ("reject_normalization",
                          {"\"median_center\"": "\"astrocs_median_center_v1\"",
                           "\"median_scale\"": "\"astrocs_median_scale_v1\""}),
        "underdetermined_n": ("reject_underdetermined_n", {}),
        "robust_mad_clip.lower_sigma": ("sigma_lower", {}),
        "robust_mad_clip.upper_sigma": ("sigma_upper", {}),
        "robust_mad_clip.max_iterations": ("sigma_max_iterations", {}),
        "winsorized_sigma.lower_sigma": ("winsor_lower", {}),
        "winsorized_sigma.upper_sigma": ("winsor_upper", {}),
        "winsorized_sigma.max_iterations": ("winsor_max_iterations", {}),
        "averaged_sigma.lower_sigma": ("avg_lower", {}),
        "averaged_sigma.upper_sigma": ("avg_upper", {}),
        "averaged_sigma.max_iterations": ("avg_max_iterations", {}),
        "linear_fit.lower": ("linfit_lower", {}),
        "linear_fit.upper": ("linfit_upper", {}),
        "linear_fit.max_iterations": ("linfit_max_iterations", {}),
        "generalized_esd.alpha": ("esd_alpha", {}),
        "generalized_esd.max_outliers": ("esd_max_outliers", {}),
        "percentile.low_fraction": ("pct_low_fraction", {}),
        "percentile.high_fraction": ("pct_high_fraction", {}),
        "median_sigma.lower_sigma": ("medsig_lower", {}),
        "median_sigma.upper_sigma": ("medsig_upper", {}),
        "median_sigma.max_iterations": ("medsig_max_iterations", {}),
        "minmax.reject_low_count": ("minmax_low_count", {}),
        "minmax.reject_high_count": ("minmax_high_count", {}),
        "minmax.min_kept": ("minmax_min_kept", {}),
        "large_scale.enabled": ("large_scale_enabled", {}),
        "large_scale.min_structure_pixels":
            ("large_scale_min_structure_pixels", {}),
        "large_scale.low_grow_radius_pixels":
            ("large_scale_low_grow_pixels", {}),
        "large_scale.high_grow_radius_pixels":
            ("large_scale_high_grow_pixels", {}),
    }

    problems = []
    checked = []
    for pk, (field, enum_map) in ALIAS.items():
        if pk not in pd:
            continue
        if field not in sd:
            problems.append({"key": pk, "issue": "struct 无此字段",
                             "parser_default": pd[pk]})
            continue
        a, b = norm(sd[field]), norm(pd[pk])
        if pk == "low" and b and b[0] != "-":
            b = "-" + b  # 解析器 low → -fabs(low) 存 sigma_low
        if pk == "method" and b == "\"auto\"":
            b = "P2_REJECT_AUTO"
        b = enum_map.get(b, b)
        same = a == b
        if not same:
            try:
                same = abs(float(a) - float(b)) < 1e-12
            except ValueError:
                same = False
        if not same:
            problems.append({"key": pk, "struct_field": field,
                             "struct_default": sd[field],
                             "parser_default": pd[pk]})
        checked.append(pk)

    # template 必须与 schema 默认一致（抽查关键项）
    tj = template["integration"]["rejection"]
    sj = schema["properties"]["integration"]["properties"]["rejection"]
    tmethod = tj.get("method", sj["properties"]["method"].get("default"))
    smethod = sj["properties"]["method"].get("default")
    if tmethod != smethod:
        problems.append({"key": "template.method", "template": tmethod,
                         "schema_default": smethod})
    tprofile = tj.get("profile", sj["properties"]["profile"].get("default"))
    if tprofile != sj["properties"]["profile"].get("default"):
        problems.append({"key": "template.profile", "template": tprofile})
    for mname in ["robust_mad_clip", "winsorized_sigma", "averaged_sigma",
                  "linear_fit", "generalized_esd", "percentile",
                  "median_sigma", "minmax"]:
        tsub = tj.get(mname, {})
        sprops = sj["properties"][mname]["properties"]
        for k, v in tsub.items():
            dfl = sprops[k].get("default")
            if dfl is not None and v != dfl:
                problems.append({"key": f"template.{mname}.{k}",
                                 "template": v, "schema_default": dfl})
    # large_scale：template ↔ schema 默认一致
    tls = tj.get("large_scale", {})
    sls = sj["properties"]["large_scale"]["properties"]
    for k, v in tls.items():
        dfl = sls[k].get("default")
        if dfl is not None and v != dfl:
            problems.append({"key": f"template.large_scale.{k}",
                             "template": v, "schema_default": dfl})

    res = {
        "checked_keys": sorted(checked),
        "mismatches": problems,
        "pass": len(problems) == 0,
    }
    OUT.parent.mkdir(parents=True, exist_ok=True)
    OUT.write_text(json.dumps(res, ensure_ascii=False, indent=2),
                   encoding="utf-8")
    print(json.dumps(res, ensure_ascii=False, indent=2))
    sys.exit(0 if res["pass"] else 1)


if __name__ == "__main__":
    main()
