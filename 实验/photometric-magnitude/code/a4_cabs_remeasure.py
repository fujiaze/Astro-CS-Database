#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""A4/E1：在**文档化参考域**（16 帧 T2 参考集）上用 `Astrodon E-series R` 重测 `C_abs`。

正本依据（`docs/detail/registry/acsd.phase1.photometry.md`）：
  · `zp_inst = C_abs − 2.5·log10(k_photo)`（:119）；唯一帧相关输入是 `k_photo`（:122-123）；
  · `z^R_i = magG_i + 2.5·log10 F_syn^R,i = C^R + (G−R)_i`，`median(z^R) = C^R + median(G−R)`（:127-129）；
  · `C_abs` **分通带**（:136）、**冻结为在文档化参考域上测定的值**（:143）；
  · **参考域 = 16 帧 T2 参考集**，其 `median(z^R)` 的**块中位**取自已发布基线产物（:147）；
  · **参考通带 = Baader R**，数值 −14.276811281840642 mag（:148-149）；
  · 要支持其它通带 ⇒ **按通带登记常数：为每条具名通带各做一次参考域测量并登记**（:185）。

⇒ 测量口径：`C_abs^P = blockmedian_{16 帧 T2}( synthetic_colour_median_mag 在通带 P 下 )`。
   该口径已用历史产物**逐位复现**旧值（见 results/a4_cabs/cabs_remeasure.json 的 reproduce 段）。

本脚本只生成配置；运行由外层 shell 调用 acsd。
"""
from __future__ import annotations

import json
import sys
from pathlib import Path

UNIT = Path(__file__).resolve().parents[1]
ROOT = UNIT.parents[1]
CANON = UNIT / "configs/p1_rerun_config.json"
WORK = ROOT / "run/PHOT-FIX/a4_cabs"

# 参考域 = 16 帧 T2 参考集（正本 :147）⇒ 取规范副本的 m42_t2_red 块，帧集逐字不动
REF_BLOCK = "m42_t2_red"


def build(filt: str, qe: str, out_name: str) -> Path:
    cfg = json.loads(CANON.read_text(encoding="utf-8"))
    src = next(b for b in cfg["blocks"] if b["name"] == REF_BLOCK)
    blk = json.loads(json.dumps(src))
    blk["name"] = "a4_%s" % out_name
    blk["filter_passband"] = filt
    fit = blk["photometry"]["fit"]
    fit.pop("filters_json", None)      # 已退役键（运行期扫描替代）
    fit.pop("qe_json", None)
    fit["filter"] = filt
    fit["qe_name"] = qe
    blk["output_dir"] = str(WORK / out_name)
    WORK.mkdir(parents=True, exist_ok=True)
    p = WORK / ("%s_config.json" % out_name)
    p.write_text(json.dumps({"schema_version": cfg["schema_version"], "blocks": [blk]},
                            indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    return p


def main() -> int:
    which = sys.argv[1] if len(sys.argv) > 1 else "all"
    if which in ("all", "astro"):
        p = build("Astrodon E-series R", "KAF-16803", "astro")
        print(p)
    if which in ("all", "baader"):
        p = build("Baader R", "Ideal QE curve", "baader")
        print(p)
    return 0


if __name__ == "__main__":
    sys.exit(main())
