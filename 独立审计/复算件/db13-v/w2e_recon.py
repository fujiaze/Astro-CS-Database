#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""W2 口径对账：复算成稿的 137/53 与我的 140/75 是不是同一口径。"""
import json
import re
import sys
from pathlib import Path

sys.stdout.reconfigure(encoding="utf-8")
REPO = Path(r"F:/Astro dev/Astro CS Normalization Database")
rows = json.loads((REPO / "docs/traceability/TRACEABILITY_MATRIX.json").read_text(encoding="utf-8"))["modules"]
idx_txt = (REPO / "docs/contracts/INDEX.yaml").read_text(encoding="utf-8")
idx_ids = set(re.findall(r"^\s*-\s*id:\s*([A-Za-z0-9_.:-]+)", idx_txt, re.M))
PH = re.compile(r"^[A-Z]+-MISSING$")
IDCOLS = ["science_id", "algorithm_id", "data_id", "api_id", "arch_id", "src_id", "test_id", "evidence_id"]
STAT = {"science_id": "science_status", "algorithm_id": "algorithm_status", "data_id": "data_status",
        "api_id": "api_status", "arch_id": "arch_status", "src_id": "src_status",
        "test_id": "test_status", "evidence_id": "evidence_status"}


def toks(v):
    return [t.strip() for t in re.split(r"[;,]\s*", str(v)) if t.strip()]


# 口径 A：按"单元格 token"计（含行内多值），去重
all_tok = set()
ver_tok = set()
for r in rows:
    for c in IDCOLS:
        for t in toks(r.get(c, "")):
            if PH.match(t):
                continue
            all_tok.add(t)
            if r.get(STAT[c]) == "VERIFIED":
                ver_tok.add(t)
# 口径 B：按 token 出现次数（非去重）
all_occ = sum(len([t for t in toks(r.get(c, "")) if not PH.match(t)]) for r in rows for c in IDCOLS)
ver_occ = sum(len([t for t in toks(r.get(c, "")) if not PH.match(t)])
              for r in rows for c in IDCOLS if r.get(STAT[c]) == "VERIFIED")
print("去重 全状态非占位 id =", len(all_tok), " 其中 VERIFIED 状态的去重 id =", len(ver_tok))
print("出现次数 全状态 =", all_occ, " VERIFIED =", ver_occ)
print("VERIFIED 去重中 INDEX.yaml 零命中 =", len([t for t in ver_tok if t not in idx_ids]))
print("VERIFIED 去重中 INDEX.yaml 零命中，且属 SRC/EVID 之外的合同层 =",
      len([t for t in ver_tok if t not in idx_ids and not t.startswith(("SRC-", "EVID-"))]))
# 成稿口径猜测：全部非占位 token 去重 = ?  零命中且 VERIFIED = ?
print("全状态去重中 INDEX 零命中 =", len([t for t in all_tok if t not in idx_ids]))

# csv.algorithm_id 与矩阵
csvl = (REPO / "docs/TRACEABILITY.csv").read_text(encoding="utf-8").splitlines()
hdr = csvl[0].split(",")
ai = hdr.index("algorithm_id")
cAlg = {ln.split(",")[ai].strip() for ln in csvl[1:] if len(ln.split(",")) > ai} - {""}
mAlg = set()
for r in rows:
    mAlg |= set(toks(r.get("algorithm_id", "")))
print("\ncsv.algorithm_id 非空去重 =", len(cAlg), sorted(cAlg))
print("   ∩ 矩阵 algorithm_id =", len(cAlg & mAlg), "   ∩ 矩阵全部 id 列 =", len(cAlg & all_tok))
mi = {r["module_id"] for r in rows} | {r["module_anchor"] for r in rows}
cm = hdr.index("module")
cmod = {ln.split(",")[cm].strip() for ln in csvl[1:] if len(ln.split(",")) > cm} - {""}
print("   csv.module 加 MOD- 前缀后 ∩ 矩阵 =", len({"MOD-" + x for x in cmod} & mi))
print("   csv.module ∩ 矩阵 module_id(去 MOD-astrocs- 前缀) =",
      len(cmod & {m.replace("MOD-astrocs-", "") for m in mi if m.startswith("MOD-astrocs-")}))
