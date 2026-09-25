#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""W2 追加：反向可解析性三层对照 + module_id 正则对照 + authority 锚存活。"""
import json
import os
import re
import sys
from pathlib import Path

sys.stdout.reconfigure(encoding="utf-8")
REPO = Path(r"F:/Astro dev/Astro CS Normalization Database")
MAT = json.loads((REPO / "docs/traceability/TRACEABILITY_MATRIX.json").read_text(encoding="utf-8"))
rows = MAT["modules"]

CONTRACT_LAYERS = {  # spec §3 明列的"合同层"
    "SCI": "science_id", "ALG": "algorithm_id", "DATA": "data_id",
    "API": "api_id", "ARCH": "arch_id", "TEST": "test_id",
}
STATUS = {"SCI": "science_status", "ALG": "algorithm_status", "DATA": "data_status",
          "API": "api_status", "ARCH": "arch_status", "TEST": "test_status",
          "SRC": "src_status", "EVID": "evidence_status"}
ALLCOL = dict(CONTRACT_LAYERS, SRC="src_id", EVID="evidence_id")

idx_txt = (REPO / "docs/contracts/INDEX.yaml").read_text(encoding="utf-8")
idx_ids = set(re.findall(r"^\s*-\s*id:\s*([A-Za-z0-9_.:-]+)", idx_txt, re.M))
api_csv = (REPO / "docs/contracts/API_CONTRACTS.csv").read_text(encoding="utf-8")

AUTH = {
    "SCI": ["docs/science"],
    "ALG": ["docs/algorithms", "docs/science"],
    "DATA": ["docs/contracts", "docs/interfaces/data", "docs/api"],
    "API": ["docs/api", "docs/contracts", "docs/interfaces/io", "docs/architecture/cpu",
            "docs/modules/registry", "eng/tests/conformance", "lib/infrastructure/benchmark/cpu"],
    "ARCH": ["docs/architecture", "docs/architecture/cpu", "docs/contracts", "docs/api"],
    "MOD": ["eng/tests/conformance", "lib/infrastructure", "docs/modules/registry"],
    "SRC": ["lib"],
    "TEST": ["eng/tests", "lib", "docs/interfaces", "docs/contracts", "docs/modules/registry"],
    "EVID": ["artifacts/evidence"],
}


TRACKED = [l.strip().replace("\\", "/") for l in
           open(Path(__file__).with_name("tracked_files.txt"), encoding="utf-8")]


def tracked_files(d):
    pref = d.rstrip("/") + "/"
    out = []
    for rel in TRACKED:
        if rel.startswith(pref) and rel.endswith((".md", ".yaml", ".yml", ".json", ".csv",
                                                  ".txt", ".py", ".cpp", ".h", ".c", ".hpp", ".cc")):
            out.append(REPO / rel)
    return out


def dir_tracked(d):
    pref = d.rstrip("/") + "/"
    return sum(1 for rel in TRACKED if rel.startswith(pref))


dirlive = {d: (REPO / d).is_dir() for lst in AUTH.values() for d in lst}
print("== authority 锚目录存活（跟踪面判定） ==")
for d, v in sorted(dirlive.items()):
    print(f"   {d:42s} dir_on_disk={v} tracked_files={dir_tracked(d)}")

blobs = {}
for layer, dirs in AUTH.items():
    txt = []
    for d in dirs:
        for f in tracked_files(d):
            try:
                txt.append(f.read_text(encoding="utf-8", errors="replace"))
            except OSError:
                pass
    blobs[layer] = "\n".join(txt)

print("\n== 每层 VERIFIED id 的三种反向判定 ==")
tot_v = tot_idx = tot_dir = tot_both = 0
detail = {}
for layer, col in ALLCOL.items():
    ids = set()
    for r in rows:
        if r.get(STATUS[layer]) != "VERIFIED":
            continue
        for t in re.split(r"[;,]\s*", str(r.get(col, ""))):
            t = t.strip()
            if t and not t.endswith("-MISSING"):
                ids.add(t)
    no_idx = {i for i in ids if i not in idx_ids}
    no_dir = {i for i in ids if i not in blobs[layer]}
    detail[layer] = sorted(no_idx)
    print(f"  {layer:5s} VERIFIED 去重={len(ids):3d}  INDEX.yaml 零命中={len(no_idx):3d} "
          f"  checker authority 目录零命中={len(no_dir):3d}")
    tot_v += len(ids); tot_idx += len(no_idx); tot_dir += len(no_dir)
    if layer in CONTRACT_LAYERS and no_idx:
        print(f"        INDEX 零命中值: {sorted(no_idx)[:12]}")
print(f"  合计（8 个 id 列） VERIFIED 去重={tot_v} INDEX 零命中={tot_idx}")

cl = [k for k in CONTRACT_LAYERS]
cv = sum(len({t for r in rows if r.get(STATUS[k]) == "VERIFIED"
              for t in re.split(r"[;,]\s*", str(r.get(CONTRACT_LAYERS[k], ""))) if t and not t.endswith("-MISSING")}) for k in cl)
cn = sum(len(detail[k]) for k in cl)
print(f"  仅 spec §3 列举的合同层 {cl}: VERIFIED 去重={cv}  INDEX.yaml 零命中={cn}")

print("\n== API 层另判：spec §4 规则 2 指定 API_CONTRACTS.csv ==")
api_ids = {t for r in rows if r.get("api_status") == "VERIFIED"
           for t in re.split(r"[;,]\s*", str(r.get("api_id", ""))) if t and not t.endswith("-MISSING")}
print("   api_id VERIFIED 去重 =", len(api_ids),
      "| 不在 API_CONTRACTS.csv =", len([a for a in api_ids if a not in api_csv]),
      sorted(a for a in api_ids if a not in api_csv)[:12])

print("\n== module_id 对 spec §3 MOD 正则 与 检查器 MOD 正则 ==")
spec_mod = re.compile(r"^MOD-[A-Z0-9]+(-[A-Z0-9]+)*$")
tool_mod = re.compile(r"^MOD-[A-Za-z0-9]+(-[A-Za-z0-9]+)*$")
mids = [r["module_id"] for r in rows]
print("   行数 =", len(mids))
print("   不匹配 spec MOD 正则 =", len([m for m in mids if not spec_mod.match(m)]))
print("   不匹配 checker MOD 正则 =", len([m for m in mids if not tool_mod.match(m)]))
print("   样例 =", mids[:4])

print("\n== spec §3 承诺'第 5 节给出两表关系' —— §5 实际内容 ==")
spec = idx_txt and (REPO / "docs/traceability/TRACEABILITY_SPEC.md").read_text(encoding="utf-8")
sec = re.search(r"## 5\..*?(?=\n## |\Z)", spec, re.S)
body = sec.group(0)
print("   §5 标题:", body.splitlines()[0])
print("   §5 提及 TRACEABILITY.csv 的次数 =", len(re.findall(r"TRACEABILITY\.csv", body)))
print("   §5 提及 两表/关系/连接键/requirement_id 的次数 =",
      len(re.findall(r"两表|关系|连接键|requirement_id", body)))
print("   spec 全文提及 requirement_id 次数 =", len(re.findall(r"requirement_id", spec)))
print("   spec 全文提及 TRACEABILITY.csv 次数 =", len(re.findall(r"TRACEABILITY\.csv", spec)))
json.dump(detail, open(Path(__file__).with_name("W2_zero_hits_by_layer.json"), "w",
                       encoding="utf-8"), ensure_ascii=False, indent=1)
