#!/usr/bin/env python3
"""C10 -- 05_正向规格.md 锚与常数核查 (只读仓库取证, 不 import 仓库 Python).

对 05_正向规格.md 声称的"来源档/锚"逐个在仓库检索, 判定 FOUND / NOT_FOUND.
输出 JSON 表 (供 report.md 引用). 只读; 目录遍历跳过 run/testdata/gaia 等大目录.
"""
import json
import os
import re
from pathlib import Path

ROOT = Path(os.path.dirname(os.path.abspath(__file__)), "..", "..", "..", "..", "..").resolve()
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "results", "c10_anchor_forensics.json")

CHECKS = [
    ("05: '11_upm.md §4.2 = 节点密度约束'", "docs/plugins/algorithms_phase2/11_upm.md", r"### 4\.2 [^\n]*"),
    ("05: sky_spline.cpp::refine_nodes_adaptive 存在", "lib/algorithms/coverage/src", r"refine_nodes_adaptive"),
    ("05: PHASE2_UPM.md §6 = lambda_bend 弯曲能 [0.01,0.1]", "docs/science/PHASE2_UPM.md", r"lambda_bend|弯曲能"),
    ("05: PHASE2_UPM_IMPL.md §6.3 数值稳定性", "docs/algorithms/PHASE2_UPM_IMPL.md", r"^### 6\.3|^## 6 "),
    ("05: REJECTION.md §5.2 留痕图景", "docs/science/REJECTION.md", r"^### 5\.2"),
    ("05: ASTROCS_DESIGN.md §2.3 平滑性要求", "ASTROCS_DESIGN.md", r"^### 2\.3 [^\n]*"),
    ("05: sky_corrections.jsonl 产品 schema", "docs", r"sky_corrections\.jsonl"),
    ("05: TilingResult / run_summary.json", "docs", r"TilingResult|run_summary\.json"),
    ("05: min_cluster_size = 3", "lib", r"min_cluster_size"),
    ("05: quality_factor 初始 0.5 基准", "lib/algorithms/coverage/src/upm.cpp", r"quality_factor"),
    ("05: sigma_bg_floor = 1.0 ADU", "lib", r"sigma_bg_floor"),
    ("05: support_min = 0.2", "lib/algorithms/coverage", r"support_threshold"),
    ("05: condition_number_max = 1e12", "docs", r"condition_number_max"),
    ("05: alpha=0.7/beta=2.0/h_absolute_min=0.01 节点密度", "docs/plugins/algorithms_phase2", r"0\.7|h_absolute_min"),
    ("05: rel_step_max = 0.1 (接缝门 10%)", "docs/science/PHASE2_UPM.md", r"rel_step"),
    ("正本: min_samples 默认 5 (sampler.h)", "lib/algorithms/coverage/include/astro/phase2/sampler.h", r"min_samples"),
    ("正本: rank_rtol 判据与 max(m,n)*eps 地板", "lib/algorithms/coverage/include/astro/phase2/identifiability.h", r"rank_rtol"),
    ("正本: sigma_floor = 1e-3 (upm.cpp sigma_eff)", "lib/algorithms/coverage/src/upm.cpp", r"sigma_floor"),
    ("正本: 接缝门 1e-2 (PHASE2_UPM §17)", "docs/science/PHASE2_UPM.md", r"rel_step"),
    ("正本: 方差比对电平阶跃失明 (11_upm §4.1)", "docs/plugins/algorithms_phase2/11_upm.md", r"方差比"),
    ("正本: control_variance = k_corr*(pi/2)*sigma^2/N", "docs/science/PHASE2_UPM.md", r"pi/2|π/2"),
    ("正本: k_corr 冻结默认 1.4, MC 1.3883", "docs/science/PHASE2_UPM.md", r"1\.3883|1\.4"),
    ("正本: Huber delta=1.345", "docs/science/PHASE2_UPM.md", r"1\.345|[Hh]uber"),
    ("正本: scale_obs=5.26e13 / 绝对 1e-6 容差", "docs/science/PHASE2_UPM.md", r"5\.26|1e-6"),
    ("正本: rtol 1e-12 跨 worker / 2.22e-15", "docs/science/PHASE2_UPM.md", r"1e-12|2\.22e-15|2\.22"),
]

SKIP_DIRS = {"run", "testdata", "gaia", "build", ".git", "artifacts", "node_modules", "旧稿存档"}


def iter_texts(base):
    if base.is_file():
        yield base
        return
    stack = [base]
    while stack:
        d = stack.pop()
        try:
            entries = sorted(d.iterdir())
        except OSError:
            continue
        for q in entries:
            if q.is_dir():
                if q.name not in SKIP_DIRS:
                    stack.append(q)
            elif q.suffix in (".h", ".cpp", ".md", ".json", ".py", ".txt", ".yml", ".yaml"):
                yield q


def grep_file(path, pattern):
    p = ROOT / path
    if not p.exists():
        return {"error": "target missing"}
    rx = re.compile(pattern)
    hits = []
    for q in iter_texts(p):
        try:
            t = q.read_text(errors="ignore")
        except OSError:
            continue
        for i, line in enumerate(t.splitlines(), 1):
            if rx.search(line):
                hits.append({"file": str(q.relative_to(ROOT)), "line": i,
                             "text": line.strip()[:160]})
                if len(hits) >= 5:
                    return hits
    return hits


def main():
    out = {"repo_root": str(ROOT), "checks": []}
    for claim, target, pattern in CHECKS:
        hits = grep_file(target, pattern)
        st = "FOUND" if hits else "NOT_FOUND"
        out["checks"].append({"claim": claim, "target": target, "pattern": pattern,
                              "hits": hits, "status": st})
        w = "-"
        if isinstance(hits, list) and hits:
            w = hits[0]["file"] + ":" + str(hits[0]["line"])
        print(f"[{st:9s}] {claim[:52]:52s} {w}")
    with open(OUT, "w") as f:
        json.dump(out, f, indent=2, ensure_ascii=False)


if __name__ == "__main__":
    main()
