#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""W2 追加 2：忠实复刻检查器 C8（REF_OUT_OF_SCOPE）判据 + 两表重合键 + 在册命令面。"""
import json
import os
import re
import sys
from pathlib import Path

sys.stdout.reconfigure(encoding="utf-8")
REPO = Path(r"F:/Astro dev/Astro CS Normalization Database")
TRACKED = set(l.strip().replace("\\", "/") for l in
              open(Path(__file__).with_name("tracked_files.txt"), encoding="utf-8"))
MAT = json.loads((REPO / "docs/traceability/TRACEABILITY_MATRIX.json").read_text(encoding="utf-8"))
rows = MAT["modules"]
LAYERS = [
    ("SCI", "science_id", "science_status", "science_doc"),
    ("ALG", "algorithm_id", "algorithm_status", "algorithm_doc"),
    ("DATA", "data_id", "data_status", None),
    ("API", "api_id", "api_status", None),
    ("ARCH", "arch_id", "arch_status", None),
    ("EVID", "evidence_id", "evidence_status", None),
]
AUTH = {"SCI": ["docs/science"],
        "ALG": ["docs/algorithms", "docs/science"],
        "DATA": ["docs/contracts", "docs/interfaces/data", "docs/api"],
        "API": ["docs/api", "docs/contracts", "docs/interfaces/io", "docs/architecture/cpu",
                "docs/modules/registry", "eng/tests/conformance", "lib/infrastructure/benchmark/cpu"],
        "ARCH": ["docs/architecture", "docs/architecture/cpu", "docs/contracts", "docs/api"],
        "EVID": ["artifacts/evidence"]}
EXT = (".md", ".csv", ".yaml", ".yml", ".json", ".c", ".h", ".cpp")
PLACEHOLDER = re.compile(r"^[A-Z]+-MISSING$")

cache = {}


def files_of(layer):
    if layer not in cache:
        cache[layer] = [(rel, (REPO / rel).read_text(encoding="utf-8", errors="ignore"))
                        for rel in sorted(TRACKED)
                        if any(rel.startswith(d.rstrip("/") + "/") for d in AUTH[layer])
                        and rel.endswith(EXT)]
    return cache[layer]


def has_authority(layer, ident, pathhint, tracked_only=True):
    if pathhint and pathhint != "MISSING":
        base = pathhint.split("::", 1)[0]
        if (tracked_only is False or base in TRACKED) and (REPO / base).is_file():
            try:
                if re.search(r"\b" + re.escape(ident) + r"\b",
                             (REPO / base).read_text(encoding="utf-8", errors="ignore")):
                    return True
            except OSError:
                pass
    pat = re.compile(r"\b" + re.escape(ident) + r"\b")
    return any(pat.search(t) for _r, t in files_of(layer))


warns = []
for r in rows:
    for layer, idk, stk, pathk in LAYERS:
        if r.get(stk) != "VERIFIED":
            continue
        ident = str(r.get(idk, ""))
        if not ident or PLACEHOLDER.fullmatch(ident):
            continue
        if has_authority(layer, ident, r.get(pathk) if pathk else None):
            continue
        warns.append(f"REF_OUT_OF_SCOPE|{r['module_id']}|{layer}|{ident}")

base = json.loads((REPO / "docs/traceability/traceability_warn_baseline.json").read_text(encoding="utf-8"))
bsigs = [e["sig"] for e in base["entries"]]
print("== 忠实复刻 C8（仅跟踪集，扫描面=os.walk 等价） ==")
print("   复刻出的 WARN 条数 =", len(warns), "  基线条目 =", len(bsigs))
print("   基线中匹配不到 WARN 的条目（→ BASELINE_STALE ERROR） =",
      [s for s in bsigs if s not in warns])
print("   超出基线的 WARN（默认模式仍 WARN，--strict 才升级 ERROR） 条数 =",
      len([w for w in warns if w not in bsigs]))
for w in sorted(set(warns)):
    print("     ", w)

print("\n== 两表事实重合键（requirement_id ∩ 矩阵任一层 id） ==")
csvl = (REPO / "docs/TRACEABILITY.csv").read_text(encoding="utf-8").splitlines()
hdr = csvl[0].split(",")
ri = hdr.index("requirement_id")
req = {ln.split(",")[ri].strip() for ln in csvl[1:] if len(ln.split(",")) > ri}
allmid = set()
for r in rows:
    for k in ("science_id", "algorithm_id", "data_id", "api_id", "arch_id",
              "test_id", "evidence_id", "src_id"):
        for t in re.split(r"[;,]\s*", str(r.get(k, ""))):
            if t.strip():
                allmid.add(t.strip())
inter = sorted(req & allmid)
print("   重合 id 数 =", len(inter), inter)
print("   docs/TRACEABILITY.csv 数据行数 =", len(csvl) - 1,
      "| 矩阵行数 =", len(rows))
print("   csv 中无 module_id 列 =", "module_id" not in hdr,
      "| 矩阵中无 requirement_id 列 =", "requirement_id" not in [c for c in rows[0]])
