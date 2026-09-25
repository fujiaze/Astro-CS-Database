#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""AUDIT-06 第②层独立复核 W2：追溯映射反向断链复算（只读）。

复算四件事：
  J1 矩阵每行 VERIFIED 层 ID 是否在 docs/contracts/INDEX.yaml 里命中（反向）
  J2 docs/TRACEABILITY.csv（细粒度）与 TRACEABILITY_MATRIX.json（模块矩阵）的可连接键
  J3 TRACEABILITY_SPEC.md §3 自设 ID 正则 vs 它自己的示例 vs 矩阵实际取值
  J4 "必填层数" 在 spec 与 LAYERS.csv / schema / 矩阵列 里的各处计数
"""
import json
import re
import sys
from pathlib import Path

sys.stdout.reconfigure(encoding="utf-8")
REPO = Path(r"F:/Astro dev/Astro CS Normalization Database")
MAT = json.loads((REPO / "docs/traceability/TRACEABILITY_MATRIX.json").read_text(encoding="utf-8"))

# ---- 矩阵形态探测 -------------------------------------------------
print("== 矩阵顶层类型 ==", type(MAT).__name__,
      list(MAT)[:12] if isinstance(MAT, dict) else len(MAT))
if isinstance(MAT, dict):
    for k, v in MAT.items():
        if isinstance(v, list):
            print(f"   list 键 {k!r} len={len(v)}")
rows = MAT["modules"] if isinstance(MAT, dict) and "modules" in MAT else MAT
if isinstance(rows, dict):
    rows = [dict(module_id=k, **v) for k, v in rows.items()]
print("   rows =", len(rows))
COLS = [c for c in rows[0]]
print("   列 =", COLS)

LAYER_ID_COL = {"SCI": "science_id", "ALG": "algorithm_id", "DATA": "data_id",
                "API": "api_id", "ARCH": "arch_id", "SRC": "src_id",
                "TEST": "test_id", "EVIDENCE": "evidence_id"}
LAYER_ST_COL = {k: (v.replace("_id", "_status") if k != "MOD" else None)
                for k, v in LAYER_ID_COL.items()}
LAYER_ST_COL["MOD"] = None

# ---- J1 VERIFIED 层 ID 是否在合同索引里 ---------------------------
idx_txt = (REPO / "docs/contracts/INDEX.yaml").read_text(encoding="utf-8")
idx_ids = set(re.findall(r"^\s*-\s*id:\s*([A-Za-z0-9_.:-]+)", idx_txt, re.M))
print("\n== J1 == INDEX.yaml 注册 id 数 =", len(idx_ids))

miss = {}
ver_total = {}
for layer, col in LAYER_ID_COL.items():
    st = LAYER_ST_COL[layer]
    ver_ids, absent = set(), set()
    for r in rows:
        if st and r.get(st) != "VERIFIED":
            continue
        raw = str(r.get(col, ""))
        for tok in re.split(r"[;,]\s*", raw):
            tok = tok.strip()
            if not tok or tok.endswith("-MISSING") or tok == "MISSING":
                continue
            ver_ids.add(tok)
    absent = {t for t in ver_ids if t not in idx_ids}
    ver_total[layer] = len(ver_ids)
    miss[layer] = sorted(absent)
    print(f"  {layer:8s} VERIFIED 去重 id={len(ver_ids):3d}  在 INDEX.yaml 零命中={len(absent):3d}")
allv = sum(ver_total.values())
alla = sum(len(v) for v in miss.values())
print(f"  合计 VERIFIED 去重 id={allv}  零命中={alla}")
for layer, v in miss.items():
    if v:
        print(f"    {layer} 零命中样例: {v[:6]}{' ...' if len(v) > 6 else ''}")
json.dump({k: v for k, v in miss.items()},
          open(Path(__file__).with_name("W2_J1_zero_hits.json"), "w", encoding="utf-8"),
          ensure_ascii=False, indent=1)

# ---- J2 两表可连接键 ----------------------------------------------
csv_txt = (REPO / "docs/TRACEABILITY.csv").read_text(encoding="utf-8").splitlines()
hdr = csv_txt[0].split(",")
print("\n== J2 == TRACEABILITY.csv 列 =", hdr, " 数据行 =", len(csv_txt) - 1)


def csv_col(name):
    i = hdr.index(name)
    out = set()
    for ln in csv_txt[1:]:
        # 粗解析：本表 notes 在最后一列，前面的列无逗号内嵌引号
        parts = ln.split(",")
        if len(parts) > i:
            out.add(parts[i].strip())
    return out


fine_req = csv_col("requirement_id")
fine_doc = csv_col("authority_doc")
fine_mod = csv_col("module")
fine_alg = csv_col("algorithm_id")
mat_mod = {r.get("module_id") for r in rows}
reg_anchor = {r.get("module_anchor") for r in rows}
mat_sci = {t for r in rows for t in re.split(r"[;,]\s*", str(r.get("science_id", ""))) if t}
mat_alg = {t for r in rows for t in re.split(r"[;,]\s*", str(r.get("algorithm_id", ""))) if t}
print("  csv.requirement_id 去重 =", len(fine_req))
print("  csv.authority_doc  去重 =", len(fine_doc))
print("  csv.module         去重 =", len(fine_mod), sorted(x for x in fine_mod if x)[:8])
print("  matrix.module_id   去重 =", len(mat_mod))
print("  matrix.module_anchor 去重 =", len(reg_anchor))
print("  键相交 requirement_id ∩ matrix 任一 id 列 =",
      len(fine_req & (mat_sci | mat_alg)))
for nm, s in (("authority_doc", fine_doc), ("module", fine_mod)):
    inter = s & (reg_anchor | mat_mod)
    print(f"  csv.{nm} ∩ (matrix.module_id ∪ matrix.module_anchor) = {len(inter)}")
print("  csv.requirement_id 样例:", sorted(fine_req)[:5])
print("  matrix.science_id  样例:", sorted(x for x in mat_sci if x)[:5])

# ---- J3 ID 正则 vs 示例 vs 矩阵取值 --------------------------------
spec = (REPO / "docs/traceability/TRACEABILITY_SPEC.md").read_text(encoding="utf-8")
rx = {}
for ln in spec.splitlines():
    m = re.match(r"^(SCI|ALG|DATA|API|ARCH|MOD|TEST|EVID)\s+(\^\S+\$)\s+(.*)$", ln.strip())
    if m:
        rx[m.group(1)] = (re.compile(m.group(2)), m.group(3))
print("\n== J3 == spec §3 正则条数 =", len(rx))
viol_ex = []
for k, (p, ex) in rx.items():
    for tok in re.findall(r"[A-Z]{2,6}-[A-Za-z0-9._-]+", ex):
        ok = bool(p.match(tok))
        print(f"   示例 {tok:38s} 匹配 {k} 正则? {ok}")
        if not ok:
            viol_ex.append((k, tok))
print("   示例与自身正则互斥条数 =", len(viol_ex))

bad = []
for r in rows:
    for layer, col in LAYER_ID_COL.items():
        st = LAYER_ST_COL[layer]
        key = "EVID" if layer == "EVIDENCE" else layer
        if key not in rx:
            continue
        for tok in re.split(r"[;,]\s*", str(r.get(col, ""))):
            tok = tok.strip()
            if not tok:
                continue
            if not rx[key][0].match(tok):
                bad.append((r.get("module_id"), layer, col, tok))
print(f"   矩阵 id 单元格不匹配自身正则 = {len(bad)} 处，去重值 =",
      len({b[3] for b in bad}))
from collections import Counter
print("   按层分布:", Counter(b[1] for b in bad))
print("   不合规样例:", sorted({b[3] for b in bad})[:10])

# ---- J4 必填层数 ---------------------------------------------------
print("\n== J4 == 必填层数口径")
lay = (REPO / "docs/traceability/TRACEABILITY_LAYERS.csv").read_text(encoding="utf-8").strip().splitlines()
print("   LAYERS.csv 层行数 =", len(lay) - 1, [l.split(',')[0] for l in lay[1:]])
print("   LAYERS.csv required_for_module 全为 every_module 的行数 =",
      sum(1 for l in lay[1:] if l.split(",")[4] == "every_module"))
sch = json.loads((REPO / "eng/contracts/schemas/traceability_matrix.schema.json").read_text(encoding="utf-8"))
print("   schema 顶层键 =", list(sch))
req = sch.get("required") or (sch.get("items") or {}).get("required")
print("   schema required =", req, "len =", len(req) if req else None)
print("   spec 文本里出现的层数断言:")
for ln in spec.splitlines():
    if re.search(r"[八89] ?个?(必填|必填层|层)|必填层|每行 8 层|8 层|9 层", ln):
        print("     ", ln.strip()[:120])
print("   矩阵 CSV 表头列数 =", len((REPO / 'docs/traceability/TRACEABILITY_MATRIX.csv').read_text(encoding='utf-8').splitlines()[0].split(',')))
print("   矩阵 JSON 每行列数 =", len(COLS))
