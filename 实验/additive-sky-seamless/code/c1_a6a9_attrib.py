#!/usr/bin/env python3
# 实验/additive-sky-seamless/code/c1_a6a9_attrib.py
#
# A6_subset_invariance / A9_final_gauge_near_noop 判词翻转的最小判别实验（归因脚本）。
# 本文件是 results/rerun_1f6d0a64/c1_a6a9_attrib.py 的**同源副本**，收进 code/ 以随单元
# 一并保留（AGENTS.md §13.2「实验代码一并保留」；results/ 在 .gitignore 内、不会入库）。
#
# 用途：关掉相对容差（切回绝对口径）并扫 tolerance，判别 A6/A9 的判词翻转是
#   (b) 判据口径需更新 还是 (a) 实现缺陷。
#
# 待判假设：翻转由生产提交 d77fd11f(SCI-502 FIX-1) 把相对容差分母由
#   max(scale_M/scale_C,1) 改为 max(scale_obs,1) 引起 —— 同一 CFG_FULL
#   (tolerance=1e-3, tolerance_relative=1) 的**有效绝对容差**因此变松约 scale_obs 倍，
#   求解更早停，C 场更不收敛 ⇒ A6/A9 的**绝对**阈值不再对应同一收敛态。
#
# 判别量：把 full_4elem 臂的 tolerance_relative 关掉（绝对口径），扫 tolerance，
#   看 A6/A9 是否在「足够紧」的绝对容差下回到基线量级。
#   回到 ⇒ (b) 判据口径需更新；不回到 ⇒ 指向 (a) 实现缺陷。
#
# 与副本的唯一差别：只读助手目录的定位方式改为**向上搜索**（使脚本在 code/ 与
#   results/rerun_1f6d0a64/ 两处都可运行），输出文件名加 `_codecopy` 后缀以免覆盖既有读数。
#   判据、阈值、扫描配置与测量逻辑**与本单元判据面完全一致，未作任何改动**。
#   只读复用 code/ 的助手（sci_c_common、c1_additive），不新建判据、不改阈值。
#
# 输出：默认 <单元>/results/c1_a6a9_attrib_codecopy.json；可用环境变量 C1_A6A9_OUT 覆盖。
import json
import os
import sys
from pathlib import Path

import numpy as np


def _find_code_dir():
    """向上搜索含 sci_c_common.py 的目录（兼容 code/ 与 results/<rerun>/ 两种位置）。"""
    here = Path(__file__).resolve().parent
    for cand in [here] + list(here.parents):
        if (cand / "sci_c_common.py").is_file():
            return cand
        if (cand / "code" / "sci_c_common.py").is_file():
            return cand / "code"
    raise RuntimeError("找不到含 sci_c_common.py 的 code/ 目录")


CODE = _find_code_dir()
UNIT = CODE.parent
sys.path.insert(0, str(CODE))
import sci_c_common as S                                   # noqa: E402
from c1_additive import CFG_FULL, scenario, subset_dev     # noqa: E402

OUT = Path(os.environ.get("C1_A6A9_OUT",
                          str(UNIT / "results" / "c1_a6a9_attrib_codecopy.json")))
world = S.build_world(seed_tag="c1")

# A6 阈值与 A9 阈值：直接引 c1_additive 的原句（0.05 e- / 0.01 e-）
TOL_A6, TOL_A9 = 0.05, 0.01


def measure(cfg, tag):
    o, corr, _ = S.run_upm_probe(scenario(world, cfg), tag)
    sub = subset_dev(world, corr)["max_dev"]
    _, c0, _ = S.run_upm_probe(scenario(world, dict(cfg, final_gauge=0)), tag + "_fg0")
    dev_fg = float(np.max(np.abs(c0 - corr)))
    return dict(tag=tag, converged=o.get("converged"), iterations=o.get("iterations"),
                max_abs_C=o.get("max_abs_C"),
                A6_subset_max_dev=float(sub), A6_ok=bool(sub < TOL_A6),
                A9_fg_max_abs_diff=dev_fg, A9_ok=bool(dev_fg < TOL_A9))


rows = []
# 1) 现行 CFG_FULL（tolerance=1e-3, tolerance_relative=1）—— 复现翻转
rows.append(dict(kind="CFG_FULL（现行，相对口径）", **measure(CFG_FULL, "a6a9_cur")))
# 2) 同 cfg 但切回绝对口径，扫 tolerance：紧到与旧收敛深度可比
for tol in (1e-3, 1e-4, 1e-5, 1e-6, 1e-7):
    cfg = dict(CFG_FULL, tolerance=tol, tolerance_relative=0)
    rows.append(dict(kind="绝对口径 tolerance=%g" % tol, **measure(cfg, "a6a9_abs_%g" % tol)))
# 3) 相对口径下把 tolerance 收紧 scale_obs 倍（等价于旧的有效绝对容差）
for tol in (1e-5, 1e-6):
    cfg = dict(CFG_FULL, tolerance=tol, tolerance_relative=1)
    rows.append(dict(kind="相对口径 tolerance=%g" % tol, **measure(cfg, "a6a9_rel_%g" % tol)))

out = dict(tol_A6=TOL_A6, tol_A9=TOL_A9, rows=rows,
           note="A6/A9 的阈值原句取自 code/c1_additive.py（A6: max_dev < 0.05 e-；"
                "A9: |Δ(C+G)| < 0.01 e-）。本脚本只测量，不改判据。")
OUT.parent.mkdir(parents=True, exist_ok=True)
OUT.write_text(json.dumps(out, indent=2, ensure_ascii=False, default=float),
               encoding="utf-8")

print("== A6/A9 翻转的最小判别实验（阈值 A6<%.2f  A9<%.2f）==" % (TOL_A6, TOL_A9))
print("  %-30s %-5s %-6s %-11s %-11s %-6s %-11s %s"
      % ("配置", "conv", "iters", "A6 max_dev", "A6 ok", "A9 diff", "A9 ok", "max_abs_C"))
for r in rows:
    print("  %-30s %-5s %-6s %-11.6g %-11s %-11.6g %-6s %.6g"
          % (r["kind"], r["converged"], r["iterations"], r["A6_subset_max_dev"],
             r["A6_ok"], r["A9_fg_max_abs_diff"], r["A9_ok"], r["max_abs_C"]))
