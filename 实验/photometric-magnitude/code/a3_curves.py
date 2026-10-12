#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""A3 通带声明对照实验：生成四臂配置。

只变**曲线声明**（filter_passband / photometry.fit.filter / photometry.fit.qe_name），
帧集与其余配置逐字不动。帧集从规范副本配置 `configs/p1_rerun_config.json` 的
`input_lights` 枚举（§13.1：不得按目录名/记忆挑帧），再按「同一天区」筛出
T3 望远镜 M1 面板（CRVAL 离散最小的组，见 results/a3_curves/sky_assert.json）。

臂定义：
  ① 历史口径     Baader R            + Ideal QE curve   （QE 必填 ⇒ 用库内理想 QE 等价替代）
  ② 只修滤镜     Astrodon E-series R + Ideal QE curve
  ③ 只修 QE      Baader R            + KAF-16803
  ④ 全修（目标） Astrodon E-series R + KAF-16803

输出：run/PHOT-FIX/a3_curves/arm{1..4}_config.json（工作副本）
      configs/a3/arm{1..4}_config.json（规范副本，逐字节相同）
"""
from __future__ import annotations

import json
import re
import sys
from pathlib import Path

UNIT = Path(__file__).resolve().parents[1]
ROOT = UNIT.parents[1]
CANON = UNIT / "configs/p1_rerun_config.json"
WORK = ROOT / "run/PHOT-FIX/a3_curves"
CANON_OUT = UNIT / "configs/a3"

ARMS = [
    (1, "历史口径", "Baader R", "Ideal QE curve"),
    (2, "只修滤镜", "Astrodon E-series R", "Ideal QE curve"),
    (3, "只修 QE", "Baader R", "KAF-16803"),
    (4, "全修（目标）", "Astrodon E-series R", "KAF-16803"),
]

SUBSET = ("T3", "M1")   # 同一天区：T3 望远镜 M1 面板


def main() -> int:
    cfg = json.loads(CANON.read_text(encoding="utf-8"))
    # 帧集枚举：从规范副本的 input_lights 取，不按目录名挑
    src = None
    for b in cfg["blocks"]:
        if b["name"] == "m42_t3_red":
            src = b
    if src is None:
        print("规范副本里没有 m42_t3_red 块", file=sys.stderr)
        return 2

    tel, panel = SUBSET
    frames = [p for p in src["input_lights"]
              if re.search(r"_%s_%s_flying_dutchman-" % (panel, tel), Path(p).name)]
    frames.sort()
    if len(frames) < 4:
        print("子集帧数不足 4：%d" % len(frames), file=sys.stderr)
        return 2

    WORK.mkdir(parents=True, exist_ok=True)
    CANON_OUT.mkdir(parents=True, exist_ok=True)
    made = []
    for idx, label, filt, qe in ARMS:
        blk = json.loads(json.dumps(src))          # 深拷贝
        blk["name"] = "a3_arm%d_%s_%s" % (idx, tel, panel)
        blk["input_lights"] = frames
        blk["filter_passband"] = filt
        # 曲线库已改为**运行期扫描** <可执行文件目录>/config/response_curves/；
        # `filters_json` / `qe_json` 两键已退役（代码侧读取路径已删除）⇒ 从本臂配置中删去，
        # 只保留名字键（`filter` + `qe_name`）。
        fit = blk["photometry"]["fit"]
        fit.pop("filters_json", None)
        fit.pop("qe_json", None)
        fit["filter"] = filt
        fit["qe_name"] = qe
        blk["output_dir"] = str(WORK / ("arm%d" % idx))
        out = {"schema_version": cfg["schema_version"], "blocks": [blk]}
        wp = WORK / ("arm%d_config.json" % idx)
        cp = CANON_OUT / ("arm%d_config.json" % idx)
        txt = json.dumps(out, indent=2, ensure_ascii=False) + "\n"
        wp.write_text(txt, encoding="utf-8")
        cp.write_text(txt, encoding="utf-8")
        made.append(dict(arm=idx, label=label, filter=filt, qe=qe,
                         n_frames=len(frames), config=str(wp), canon=str(cp)))
        print("arm%d %-12s filter=%-20s qe=%-15s n=%d" % (idx, label, filt, qe, len(frames)))

    (WORK / "arms.json").write_text(
        json.dumps(dict(subset=SUBSET, frames=frames, arms=made),
                   indent=2, ensure_ascii=False), encoding="utf-8")
    print("帧集（%d 帧，%s %s）：" % (len(frames), tel, panel))
    for f in frames:
        print("   ", Path(f).name)
    return 0


if __name__ == "__main__":
    sys.exit(main())
