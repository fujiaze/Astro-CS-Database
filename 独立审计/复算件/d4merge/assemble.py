# -*- coding: utf-8 -*-
"""D4 装配：判读层(354) + 链路报告条目(60) + 机械层对照 → 总台账主表 + 层间冲突表。"""
import csv, io, os, re, sys, json, collections

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from merge_lib import (BASE, OUT, load, extract_positions, disp_norm, classify,
                      find_cases, CONFLICT_KW, conflict_excerpt)
from chain_a import CHAIN
from chain_b import CHAIN2
from chain_c import CHAIN3
from redcases import RED_BY_POS, RED_BY_SYM

COLS = ["符号或键", "类别", "位置集合（全部侧，规范化全路径:行）", "现行值", "单位",
        "坐标系或归一化", "精度要求", "有效有限域", "来源现状", "出处", "处置",
        "适用域", "是否已复核", "冲突标记", "来源成稿与条目", "多侧默认值预筛（占位·预筛在途）"]

# 链路条目 → 判读层符号 的同一参数别名（合并，不重复成行）
ALIAS = {
    "photometry.tukey_c / _TUKEY_C": "photometry.tukey_c",
    "photometry.mag_tolerance": "photometry.mag_tolerance",
    "photometry.min_reference_stars": "photometry.min_reference_stars",
    "photometry.min_inlier_stars": "photometry.min_inlier_stars",
    "sparse_snr.spacing_px（Δ）的「够密」论证": "sparse_snr.spacing_px",
    "snr.path / snr_path_effective": "snr.path",
}

MECH = load("raw/AUD-402-常数台账.csv")[1]
MECH_IX = {(r[0], r[1]): r for r in MECH}

JQ = [("AUD-402-判读-A1", "判读-A1"), ("AUD-402-判读-A2", "判读-A2"),
      ("AUD-402-判读-A3", "判读-A3"), ("AUD-402-判读-BD1", "判读-BD1")]

entries = collections.OrderedDict()   # key -> row dict
layer = []                            # 层间冲突
jud_stats = collections.Counter()

for fname, tag in JQ:
    h, b = load("raw/%s.csv" % fname)
    for r in b:
        sym, pos = r[0], r[1]
        m = MECH_IX.get((sym, pos))
        key = sym if sym.strip() != "(无名)" else "(无名)｜" + pos
        e = entries.get(key)
        ps = [pos] + extract_positions(r[8], r[11])
        if e is None:
            label = sym if sym.strip() != "(无名)" else "(无名)·%s" % pos
            e = {"sym": label, "rows": [], "pos": [], "vals": [], "src": "", "ref": "",
                 "unit": r[3], "coord": r[4], "prec": r[5], "dom": r[6],
                 "source_state": r[7], "disp_raw": r[9], "app": r[10], "note": r[11],
                 "cases": set(), "chenggao": set(), "reviewed": "判读层已核（未过第②层）",
                 "mech_disp": set(), "mech_src": set()}
            entries[key] = e
        e["rows"].append((fname, r))
        e["pos"] += ps
        e["vals"].append(r[2])
        e["mech_disp"].add(m[9] if m else "")
        e["mech_src"].add((m[7] if m else "")[:26])
        for s in (r[8], r[11], r[9]):
            e["cases"] |= set(find_cases(s))
        e["chenggao"].add(fname)
        # 层间不一致
        if m is not None:
            mech_none = m[7].startswith("无来源")
            jud_has_src = bool(r[8].strip()) and not r[8].strip().startswith(("无", "—", "不适用", "缺"))
            if mech_none and jud_has_src:
                layer.append(["机械无来源→判读给出处", sym, pos, m[7], r[8][:150],
                              "以判读层为准"])
                jud_stats["A"] += 1
            elif mech_none and r[9].startswith("不适用") and not jud_has_src:
                jud_stats["B"] += 1
            elif (not mech_none) and (not jud_has_src) and r[7].startswith("无"):
                layer.append(["机械有来源→判读判无出处", sym, pos, m[7][:60], r[7],
                              "以判读层为准"])
                jud_stats["C"] += 1
            if m[9].strip() != r[9].strip():
                jud_stats["D"] += 1

# ---------------- 链路报告条目 ----------------
for d in CHAIN + CHAIN2 + CHAIN3:
    key = ALIAS.get(d["sym"], d["sym"])
    e = entries.get(key)
    if e is not None and key != d["sym"]:
        # 并入判读层已有行：补位置、出处、冲突、成稿、复核态
        e["pos"] += d["pos"]
        e["extra_ref"] = (e.get("extra_ref", "") + "；" + d["ref"]).strip("；")
        if d["conf"]:
            e.setdefault("xconf", []).append(d["conf"])
        e["chenggao"].add(d["src2"].split("；")[0].strip())
        if d["rev"].startswith("是"):
            e["reviewed"] = "判读层已核＋第②层独立复核"
        continue
    entries[key] = {
        "sym": d["sym"], "rows": [], "pos": list(d["pos"]), "vals": [d["val"]],
        "unit": d["unit"], "coord": d["coord"], "prec": d["prec"], "dom": d["dom"],
        "source_state": d["src"], "ref": d["ref"], "disp_raw": d["disp"], "app": d["app"],
        "note": "", "cases": set(find_cases(d["conf"] + " " + d["src2"])),
        "chenggao": {d["src2"]}, "reviewed": d["rev"], "cat_fix": d["cat"],
        "xconf": [d["conf"]] if d["conf"] else [], "mech_disp": set(), "mech_src": set()}

# ---------------- 出表 ----------------
def dedup_pos(ps):
    seen, res = set(), []
    for p in ps:
        p = p.strip().rstrip("，。,;；")
        if not p or p in seen:
            continue
        seen.add(p)
        res.append(p)
    return res


def fmt_pos(ps, cap=14):
    u = dedup_pos(ps)
    out = "；".join(u[:cap])
    if len(u) > cap:
        out += "；…（另有 %d 处同簇位点，见判读成稿）" % (len(u) - cap)
    return out, len(u)


rows_out = []
for key, e in entries.items():
    vals = [v for v in e["vals"] if v]
    uniq = []
    for v in vals:
        if v not in uniq:
            uniq.append(v)
    val = uniq[0] if len(uniq) <= 1 else " ｜ ".join(uniq)
    if len(uniq) > 1:
        # 多行同符号：位置侧的取值不等
        pairs = "；".join(sorted(set("%s=%s" % (
            os.path.basename(x[1][1].split(":")[0]) + ":" + x[1][1].rsplit(":", 1)[-1],
            x[1][2]) for x in e["rows"])))
        if not any(x.startswith("红") for x in e.get("xconf", [])):
            e.setdefault("xconf", []).append(
                "红：判读层把同一符号按位点分行登记，取值不等（%s）" % pairs[:300])
    conf = " ∥ ".join(e.get("xconf", [])) if e.get("xconf") else ""
    # 1) 显式立案登记表（每条红/黄都能指回成稿里的立案编号）优先
    reg = []
    if e["sym"] in RED_BY_SYM:
        reg.append(RED_BY_SYM[e["sym"]])
    for p in e["pos"]:
        pp = p.replace("\\", "/")
        cands = {pp}
        if ":" in pp:
            pre, rest = pp.split(":", 1)
            cands.add(pre + ":" + rest.split("-")[0].split(",")[0])
            cands.add(pre)
        for c in cands:
            if c in RED_BY_POS:
                reg.append(RED_BY_POS[c])
                break
    if reg:
        parts = []
        for sev, cid, quote, srcdoc in reg:
            parts.append("%s（判读层立案 %s｜%s）：%s" % (sev, cid, srcdoc, quote))
        conf = " ∥ ".join(parts)
    if not conf:
        note = e.get("note", "")
        hits = [k for k in CONFLICT_KW if k in note or k in e.get("ref", "")
                or k in e.get("source_state", "")]
        multi = (len(e["rows"]) > 1 or "多侧" in note or "两侧" in note or "三侧" in note
                 or "四侧" in note or "六侧" in note or "五侧" in note)
        if hits and multi:
            cs = "、".join(sorted(e["cases"])[:6]) or "逐侧并列见本行出处与备注引文"
            ex = conflict_excerpt(note, hits) or conflict_excerpt(e.get("ref", ""), hits)
            conf = "红（判读层立案 %s）：%s" % (cs, ex)
    cat = e.get("cat_fix") or classify(e["sym"], val, e.get("unit"), e.get("coord"),
                                       e.get("note"), e["disp_raw"])
    pset, npos = fmt_pos(e["pos"])
    disp = disp_norm(e["disp_raw"])
    refs = sorted(set(x[1][8] for x in e["rows"] if x[1][8].strip()))
    refj = e.get("ref", "").strip()
    if refj and refj not in refs:
        refs.append(refj)
    extra = (e.get("extra_ref") or "").strip("；")
    if extra:
        refs.append("链路报告侧：" + extra)
    cg = sorted(x for x in e["chenggao"] if x)
    cases = sorted(e["cases"])[:12]
    rev_raw = e.get("reviewed", "")
    if rev_raw.startswith("判读层已核＋"):
        rev = "是（第②层独立复核）"
    elif rev_raw.startswith("判读层已核"):
        rev = "否（判读层逐行已判，未过第②层复核）"
    elif rev_raw.startswith("否"):
        rev = "否（仅链路成稿，未过第②层复核）"
    elif any(k in rev_raw for k in ("改判", "推翻", "降级", "订正", "替换", "定案", "升为")):
        rev = "是（第②层复核改判）"
    else:
        rev = "是（第②层独立复核）"
    if rev != "否（判读层逐行已判，未过第②层复核）" and rev_raw:
        cg = cg + ["复核注记：" + rev_raw]
    rows_out.append({
        "符号或键": e["sym"], "类别": cat, "位置集合（全部侧，规范化全路径:行）": pset,
        "现行值": val, "单位": e.get("unit", ""), "坐标系或归一化": e.get("coord", ""),
        "精度要求": e.get("prec", ""), "有效有限域": e.get("dom", ""),
        "来源现状": e.get("source_state", ""), "出处": "；".join(refs)[:1600],
        "处置": disp, "适用域": e.get("app", ""),
        "是否已复核": rev, "冲突标记": conf,
        "来源成稿与条目": "；".join(cg + cases)[:900],
        "多侧默认值预筛（占位·预筛在途）": "",
        "_npos": npos, "_disp_raw": e["disp_raw"], "_mech": sorted(e["mech_disp"])})

# 排序：红 → 黄 → 其余；组内按处置、符号
order = {"文献值": 0, "公式导出": 1, "实验标定": 2, "待确认": 3, "不适用": 4}


def sev(r):
    c = r["冲突标记"]
    return 0 if c.startswith("红") else (1 if c.startswith("黄") else 2)


rows_out.sort(key=lambda r: (sev(r), order.get(r["处置"], 9), r["符号或键"]))

# ---------------- 写 CSV ----------------
dst = os.path.join(BASE, r"独立审计/02_科学\常数公式算法总台账.csv")
os.makedirs(os.path.dirname(dst), exist_ok=True)
PROJ = len(rows_out)


def wcsv(chunk, mode):
    with io.open(dst, mode, encoding="utf-8-sig", newline="") as f:
        wr = csv.writer(f)
        if mode == "w":
            wr.writerow(COLS)
        for r in chunk:
            wr.writerow([r[c] for c in COLS])


wcsv(rows_out[:200], "w")
rest = rows_out[200:]
i = 0
while rest:
    wcsv(rest[i:i + 200], "a")
    i += 200
    if i >= len(rest):
        break
with io.open(dst, "a", encoding="utf-8-sig", newline="") as f:
    f.write("<!-- PROGRESS: 已装配 %d 行 / 预计 %d 行 -->\n" % (len(rows_out), PROJ))

# ---------------- 层间冲突表 ----------------
with io.open(os.path.join(OUT, "layer_conflicts.csv"), "w", encoding="utf-8-sig", newline="") as f:
    wr = csv.writer(f)
    wr.writerow(["不一致类型", "符号", "位置", "机械层", "判读层", "裁定"])
    for x in layer:
        wr.writerow(x)

st = {"total_rows": len(rows_out), "judged_mech_rows": 354,
      "chain_entries": len(CHAIN) + len(CHAIN2) + len(CHAIN3),
      "layer_A": jud_stats["A"], "layer_B": jud_stats["B"], "layer_C": jud_stats["C"],
      "layer_D": jud_stats["D"], "layer_rows": len(layer),
      "disp": dict(collections.Counter(r["处置"] for r in rows_out)),
      "cat": dict(collections.Counter(r["类别"] for r in rows_out)),
      "red": sum(1 for r in rows_out if r["冲突标记"].startswith("红")),
      "yellow": sum(1 for r in rows_out if r["冲突标记"].startswith("黄")),
      "reviewed": dict(collections.Counter(r["是否已复核"] for r in rows_out)),
      "cat_x_disp": {"%s|%s" % k: v for k, v in
                     sorted(collections.Counter((r["类别"], r["处置"]) for r in rows_out).items())},
      "disp_x_family": {}}


def fam(pset):
    first = pset.split("；")[0] if pset else ""
    p = first.split(":")[0]
    parts = p.split("/")
    return "/".join(parts[:2]) if len(parts) > 2 else (parts[0] if parts and parts[0] else "（无路径）")


dx = collections.Counter((fam(r["位置集合（全部侧，规范化全路径:行）"]), r["处置"]) for r in rows_out)
for k, v in dx.most_common():
    st["disp_x_family"][k[0] + "|" + k[1]] = v
st["family_total"] = dict(collections.Counter(
    fam(r["位置集合（全部侧，规范化全路径:行）"]) for r in rows_out).most_common())
json.dump(st, io.open(os.path.join(OUT, "stats.json"), "w", encoding="utf-8"),
          ensure_ascii=False, indent=1)
import sys as _s; _s.stdout.reconfigure(encoding="utf-8", errors="replace")
