# -*- coding: utf-8 -*-
"""
V4 独立复算：把"登记的配置键 — 实际硬编码位置 — 权威回链行号"三件逐条对齐。

我自己重写判据（不 import 仓库 Python）：
  P1 value-on-line ：source_ref 那一行是否真的含有该 value
  P2 CFG002 口径   ：仓库门 check_09_defaults_anchor_survival 用的"hint token 松匹配"
                     （我按它的规则重写：整串命中 / len>=3 时前 2 字命中 / CJK 前 2 字命中）
  P3 CFG001 口径   ：test 里 KEY_ANCHORS 的"严格 token in line"
  实际陈述行       ：该 value 在被引文档里真实出现的行号（含数值的行）
"""
import io
import json
import os
import re
import subprocess
import sys

sys.stdout.reconfigure(encoding="utf-8")
ROOT = r"F:\Astro dev\Astro CS Normalization Database"
OUT = os.path.dirname(os.path.abspath(__file__))

PHOT_KEYS = ["photometry.mag_tolerance", "photometry.tukey_c", "photometry.irls_max_iterations",
             "photometry.irls_tolerance_dex", "photometry.min_reference_stars",
             "photometry.min_inlier_stars"]

# 仓库测试里硬写的期望锚（KEY_ANCHORS），我逐字抄来只做口径对齐用
KEY_ANCHORS = [
    ("detection.threshold_sigma", "docs/science/STAR_DETECTION.md", 21, "5.0"),
    ("psf.default_model", "docs/science/PSF.md", 9, "Moffat4"),
    ("psf.moffat_beta", "docs/science/PSF.md", 94, "4"),
    ("noise.source_mask_radius_px", "docs/science/NOISE_MODEL.md", 86, "rmax"),
    ("noise.variance_floor", "docs/science/NOISE_MODEL.md", 23, "1e-12"),
    ("rejection.sigma.lower_sigma", "docs/science/REJECTION.md", 68, "4.0/3.0/8"),
    ("photometry.mag_tolerance", "docs/science/PHOTOMETRY.md", 30, "3.0 mag"),
    ("precision.default", "docs/science/SCIENCE_SCOPE.md", 56, "FP64"),
    ("upm.k_corr", "docs/science/PHASE2_UPM.md", 24, "1.4"),
    ("hips.tile_width", "docs/science/PHASE3_HIPS_TO_FITS.md", 41, "512"),
    ("drizzle.pixfrac", "docs/science/DRIZZLE.md", 35, "pixfrac"),
]

# 权威锚曾经是否成立：按 §2a 插入前那个提交的内容复核（HEAD~ 的历史版本）
PREV_DOC = "92ede644^"


def read(path):
    return io.open(os.path.join(ROOT, path.replace("/", os.sep)), encoding="utf-8").read()


def doc_at(ref, path):
    r = subprocess.run(["git", "-C", ROOT, "show", f"{ref}:{path}"],
                       capture_output=True, text=True, encoding="utf-8")
    return r.stdout.splitlines() if r.returncode == 0 else None


def value_tokens(v):
    if isinstance(v, bool):
        return [str(v).lower(), "true" if v else "false"]
    if isinstance(v, (int, float)):
        s = repr(v)
        cand = {s, str(v), ("%g" % v), s.replace("e-0", "e-")}
        if isinstance(v, float) and v == int(v):
            cand.add(str(int(v)))
        return sorted(cand)
    if isinstance(v, str):
        return [v]
    if isinstance(v, list):
        out = []
        for x in v:
            out += value_tokens(x)
        return out
    return []


def hint_of(source):
    m = re.search(r":\d+（([^）]*)）", source or "")
    return m.group(1) if m else ""


def loose_hint_ok(txt, hint):
    """仓库 check_09 的口径（按其规则重写）：>=2 字 token，任一整串命中，
    或 len>=3 时前 2 字命中，或含中文时前 2 字命中。"""
    toks = [t for t in hint.split() if len(t) >= 2 and not re.fullmatch(r"§\d+", t)]
    for t in toks:
        if t in txt:
            return True, t
        if len(t) >= 3 and t[:2] in txt:
            return True, t + "(前2字)"
        if re.search(r"[一-鿿]", t) and t[:2] in txt:
            return True, t + "(前2字/CJK)"
    return False, None


def strict_token(txt, token):
    return token in txt


def first_value_line(lines, vtoks):
    hits = []
    for i, l in enumerate(lines, 1):
        if any(t in l for t in vtoks):
            hits.append(i)
    return hits


def main():
    doc = json.loads(read("eng/packaging/config/defaults.json"))
    fields = {f["key"]: f for f in doc["fields"]}
    res = {"field_count_declared": doc["field_count"], "field_count_actual": len(doc["fields"]),
           "photometry": {}, "all_fields_summary": {}}

    # ---- 六条测光常数逐条对齐
    for k in PHOT_KEYS:
        f = fields[k]
        ref = f.get("source_ref") or {}
        path, line = ref.get("path"), ref.get("line")
        lines = read(path).splitlines()
        txt = lines[line - 1] if line and 1 <= line <= len(lines) else ""
        vtoks = value_tokens(f["value"])
        hits = first_value_line(lines, vtoks)
        # 该键在"插入 §2a 之前"的同一行是否成立
        prev = doc_at(PREV_DOC, path)
        prev_line_txt = prev[line - 1] if (prev and line and line <= len(prev)) else None
        prev_ok = (prev_line_txt is not None and
                   any(t in prev_line_txt for t in vtoks))
        hint = hint_of(f.get("source"))
        loose_ok, loose_tok = loose_hint_ok(txt, hint) if hint else (None, None)
        ka = [a for a in KEY_ANCHORS if a[0] == k]
        res["photometry"][k] = {
            "value": f["value"],
            "source_text": f.get("source"),
            "source_ref": f"{path}:{line}",
            "line_content_at_HEAD": txt[:110],
            "P1_value_on_line": any(t in txt for t in vtoks),
            "P2_cfg002_loose_hint": loose_ok,
            "P2_matched_token": loose_tok,
            "P3_cfg001_key_anchor_registered": (bool(ka) and
                                                strict_token(txt, ka[0][3]) if ka else None),
            "actual_value_lines_in_doc": hits[:8],
            "offset_lines": (hits[0] - line) if hits else None,
            "anchor_was_true_before_section2a": prev_ok,
            "anchor_note": f.get("note", "")[:80],
        }

    # ---- 全表 59 条的 P1 统计（看"大面积"是不是测光独有）
    p1_bad, p1_ok, no_ref, nodoc = [], [], [], []
    for f in doc["fields"]:
        ref = f.get("source_ref")
        if not ref:
            no_ref.append(f["key"])
            continue
        try:
            lines = read(ref["path"]).splitlines()
        except Exception:
            nodoc.append(f["key"])
            continue
        ln = ref.get("line")
        if not isinstance(ln, int) or not (1 <= ln <= len(lines)):
            p1_bad.append((f["key"], "越界"))
            continue
        vt = value_tokens(f["value"])
        if f.get("authority_status") == "pending_authority" or not vt:
            continue
        if any(t in lines[ln - 1] for t in vt):
            p1_ok.append(f["key"])
        else:
            p1_bad.append((f["key"], f"{ref['path']}:{ln}"))
    res["all_fields_summary"] = {
        "value_on_line_OK": len(p1_ok), "value_on_line_FAIL": len(p1_bad),
        "no_source_ref": len(no_ref), "fail_list": p1_bad[:30],
        "photometry_in_fail": [k for k, _ in p1_bad if k.startswith("photometry.")]}

    # ---- 值实际落在哪里：源码硬编码点
    src_hits = {}
    for pat, label in ((r"_TUKEY_C = 4\.685", "star_matcher 冻结常数"),
                       (r"_IRLS_MAX_ITER = 50", "IRLS 迭代上限"),
                       (r"_IRLS_CONVERGE = 1e-6", "IRLS 收敛门"),
                       (r"3\.0,\s*//\s*mag_tolerance", "mag_tolerance 实参"),
                       (r"kTukeyC = 4\.685", "spatial_gain 重复常数")):
        r = subprocess.run(["git", "-C", ROOT, "grep", "-n", "-I", "-E", pat, "--", "lib/"],
                           capture_output=True, text=True, encoding="utf-8")
        src_hits[label] = [l.split(":", 2)[:3] for l in r.stdout.splitlines()]
    res["hardcoded_locations"] = src_hits

    # ---- defaults.json 是否被运行时读取（键是否只是文档性登记）
    r = subprocess.run(["git", "-C", ROOT, "grep", "-l", "-I", "defaults.json", "--",
                        "lib/", "eng/cmake/", "CMakeLists.txt"],
                       capture_output=True, text=True, encoding="utf-8")
    res["defaults_json_read_by_runtime"] = r.stdout.splitlines()

    txt = json.dumps(res, indent=2, ensure_ascii=False)
    io.open(os.path.join(OUT, "v4_provenance_alignment.json"), "w", encoding="utf-8").write(txt)
    print(txt)


if __name__ == "__main__":
    main()
