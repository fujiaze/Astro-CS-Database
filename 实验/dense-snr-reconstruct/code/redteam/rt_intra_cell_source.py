"""胞内点源在生产默认重建算子下的失效域反例（胞内源 → 常数平台 → 权重乐观高估）。

复用本单元盲复算脚本 `rt06_reconstruction_redteam.py` 里已自检过的可分离自然边界
双三次样条（含 `[min(node), max(node)]` 值域钳制，与 `weight_chain.cpp` 的钳制语义
逐条对齐），只新增本反例；不改 `rt06_*` 的任何既有算子、判据或自检。
输出只到 stdout，**不写 results/**。

构型：Δ=64 px、4×4 = 16 控制点、背景为常数 SNR 场、**控制点取真值**（零估计量
噪声），在胞内（距最近控制点 0.30·Δ）注入 FWHM 可变点源，重建 256² 场。
比较真峰、`max(node)`、重建峰，并给 `SNR_rec/true` 与权重高估倍数
`1/(rec/true)^2`；另给钳制 ON/OFF 对照，判断钳制是不是机制。
"""
import sys
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parent))
from rt06_reconstruction_redteam import _moffat_at, spline_clip_2d  # noqa: E402

DELTA = 64
N_NODES = 4
SIZE = 256
BACKGROUND = 1.0


def run(peak, fwhm, clip=True):
    nodes = np.array([i * DELTA + (DELTA - 1) / 2 for i in range(N_NODES)])
    xs, ys = np.meshgrid(np.arange(SIZE), np.arange(SIZE), indexing="ij")
    xq = xs.astype(float)
    yq = ys.astype(float)
    src = _moffat_at(xq, yq, 0.30 * DELTA, 0.30 * DELTA, fwhm, peak)
    truth = BACKGROUND + src
    node_vals = BACKGROUND + _moffat_at(nodes.astype(float), nodes.astype(float),
                                        0.30 * DELTA, 0.30 * DELTA, fwhm, peak)
    grid = np.outer(node_vals, np.ones(N_NODES)).reshape(N_NODES, N_NODES)
    rec = spline_clip_2d(grid, nodes, nodes, xq.ravel(), yq.ravel(),
                         clip=clip).reshape(SIZE, SIZE)
    node_max = float(node_vals.max())
    return dict(true_peak=float(truth.max()), node_max=node_max,
                rec_peak=float(rec.max()),
                eq_node_max=float((np.abs(rec - node_max) <= 1e-12 * max(1.0, abs(node_max))).mean()))


def main():
    print("== 胞内点源（Δ=64 px、控制点取真值、背景常数）==")
    print(f"  {'peak':>7} {'fwhm':>6} {'true_peak':>10} {'max(node)':>11} {'rec_peak':>10} "
          f"{'rec/true':>9} {'weight_over':>12} {'eq_max_frac':>12}")
    for peak, fwhm in ((0.5, 3.0), (2.0, 3.0), (8.0, 3.0), (32.0, 3.0),
                       (32.0, 8.0), (128.0, 3.0), (32.0, 20.0)):
        r = run(peak, fwhm)
        ratio = r["rec_peak"] / r["true_peak"]
        print(f"  {peak:7} {fwhm:6} {r['true_peak']:10.2f} {r['node_max']:11.2f} "
              f"{r['rec_peak']:10.2f} {ratio:9.4f} {1.0 / ratio ** 2:12.1f} "
              f"{r['eq_node_max']:12.3f}")

    on = run(32.0, 3.0, clip=True)["rec_peak"]
    off = run(32.0, 3.0, clip=False)["rec_peak"]
    print(f"\n  钳制 ON/OFF 的重建峰：{on:.6f} / {off:.6f} "
          f"（相对差 {abs(on - off) / off:.2e}）⇒ 钳制不是把重建峰钉在 max(node) 的机制")


if __name__ == "__main__":
    main()
