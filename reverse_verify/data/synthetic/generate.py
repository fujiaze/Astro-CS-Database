#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""DATA-TYPE-MATRIX —— 合成数据集生成入口（可复跑）。

读 data/synthetic/datasets.json，逐个场景调用 synthetic/render.py 的 render_dataset()，
产物落 run/reverse_verify/data_matrix/<outdir>/（gitignore；不入库）。

复跑（推荐先设 TMPDIR 到 /dev/shm）::

    export TMPDIR=/dev/shm/astrocs_dtm
    python3 reverse_verify/data/synthetic/generate.py            # 全部
    python3 reverse_verify/data/synthetic/generate.py --only nebula_core_analytic,sweep_sky
    python3 reverse_verify/data/synthetic/generate.py --list

判据与示范实验在 reverse_verify/experiments/data_matrix/。

STACKN32-001（A4 前台裁决，2026-09-20）：M16 场景的 `stack_n` 全局统一为 **32**
（真实 drz 是 NDRIZIM=32 的子曝光 drizzle 合成品，交付帧曝光 = 真实 drz 的**总**曝光
⇒ 叠加等效读出噪声 sqrt(32)*3.1 = 17.536 e-）。本入口**不含**任何 stack_n 硬编码：
默认值由场景 JSON（`reverse_verify/synthetic/scenes/m16_*.json` 的 `stack_n`）
与渲染器 `m16_scene.M16_DEFAULTS["stack_n"]` 两级给出，两者现均为 32。
非 M16 场景（`renderer="render"`）是无叠加语义的解析玩具仪器，不使用 stack_n。
"""

from __future__ import annotations

import argparse
import json
import sys
import time
from pathlib import Path

HERE = Path(__file__).resolve()
RV = HERE.parents[2]                     # reverse_verify/  (data/synthetic/generate.py)
ROOT = RV.parent                        # 仓库根
sys.path.insert(0, str(RV / "synthetic"))
import render as R  # noqa: E402
import m16_scene as M16  # noqa: E402  M16 场景模板前向渲染器（renderer="m16_scene"）

# renderer 键 -> 渲染函数（按场景配方的 renderer 字段派发；缺省 = DATA-TYPE-MATRIX 通用渲染器）
RENDERERS = {
    "render": R.render_dataset,
    "m16_scene": M16.render_m16_dataset,
}

REGISTRY = HERE.parent / "datasets.json"


def main() -> int:
    ap = argparse.ArgumentParser(description="生成 DATA-TYPE-MATRIX 合成数据集")
    ap.add_argument("--only", type=str, default=None, help="逗号分隔的 dataset id")
    ap.add_argument("--list", action="store_true")
    ap.add_argument("--seed", type=int, default=None, help="覆盖基础种子（默认用场景内 seed）")
    a = ap.parse_args()

    reg = json.loads(REGISTRY.read_text(encoding="utf-8"))
    root = ROOT / reg["outdir_root"]
    items = reg["datasets"]
    if a.list:
        for d in items:
            print("%-32s %-58s %s" % (d["id"], d["scene"], ",".join(d.get("cells", []))))
        return 0
    want = set(a.only.split(",")) if a.only else None
    manifest = {"generated_at": time.strftime("%Y-%m-%dT%H:%M:%S"),
                "registry": str(REGISTRY.relative_to(ROOT)), "datasets": []}
    rc = 0
    for d in items:
        if want is not None and d["id"] not in want:
            continue
        scene = RV / d["scene"]
        out = root / d["outdir"]
        rk = str(d.get("renderer", "render"))
        if rk not in RENDERERS:
            print("  [FAIL] unknown renderer %r" % rk)
            manifest["datasets"].append({"id": d["id"], "status": "FAIL",
                                         "reason": "unknown renderer %r" % rk})
            rc = 1
            continue
        print("=== %s  (%s, renderer=%s)" % (d["id"], d["scene"], rk))
        try:
            man = RENDERERS[rk](scene, out, seed=a.seed)
        except Exception as exc:                     # noqa: BLE001 - 登记而非掩盖
            print("  [FAIL] %s: %s" % (type(exc).__name__, exc))
            manifest["datasets"].append({"id": d["id"], "status": "FAIL",
                                         "reason": "%s: %s" % (type(exc).__name__, exc)})
            rc = 1
            continue
        n_ok = sum(1 for f in man["frames"] if f.get("status") == "OK")
        n_un = sum(1 for f in man["frames"] if f.get("status") == "UNAVAILABLE")
        manifest["datasets"].append({"id": d["id"], "status": "OK", "renderer": rk,
                                     "outdir": str(out),
                                     "scene_id": man["scene_id"], "kind": man["kind"],
                                     "n_frames": len(man["frames"]), "n_ok": n_ok,
                                     "n_unavailable": n_un,
                                     "cells": d.get("cells", []),
                                     "criteria": d.get("criteria", [])})
    (root / "generate_manifest.json").parent.mkdir(parents=True, exist_ok=True)
    with open(root / "generate_manifest.json", "w", encoding="utf-8") as fh:
        json.dump(manifest, fh, indent=1, ensure_ascii=False)
    print("[generate] manifest -> %s" % (root / "generate_manifest.json"))
    return rc


if __name__ == "__main__":
    raise SystemExit(main())
