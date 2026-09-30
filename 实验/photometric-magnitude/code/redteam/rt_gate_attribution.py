"""P1 双边界门的归因盲区反例（只读调用本单元标定与判据实现）。

只 import 本单元的 `scia_calib` / `scia_common` / `scia_sim`，不改任何一行；
输出只到 stdout，**不写 results/**。

检验两件事：
  (A) `sigma_obs_mag` 是 `m(x,y)` **之前**的统计量（`scia_calib.py` 中
      `out.update(..., sigma_obs_mag=2.5*sigma_residual)` 先于 `if m_degree>0`），
      故 m_degree=0 与 m_degree=2 的 `sigma_obs_mag` 逐位相同，而诊断字段
      `delta_after_m` 随 m(x,y) 吸收位置依赖残差而塌缩。
  (B) 未建模的乘性残差把判定从 BELOW_FLOOR 推出下包络的临界幅度。

构型：n=48 星（远超 n≤12.236 的下包络有效域）、星等 12–20、逐星测光噪声取本单元
`noise_sigma_mag` 口径；注入两类乘性残差——亮度依赖 `a·(F/F_max)`（探测器非线性）
与位置依赖 `a·(x−x̄)/σ_x`（乘性平场 / 渐晕）。
"""
import sys
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import scia_calib as pl  # noqa: E402
import scia_common as sc  # noqa: E402
from scia_sim import Instrument  # noqa: E402

N = 48
SIGMA_PIX_E = 18.4
F_MAX_ADU = 2.0e5
BUDGET_SYS = dict(sigma_psfsys=0.0250, sigma_color=0.00574,
                  sigma_gaia=0.002, sigma_flat=0.000881)


def frame(kind, a, seed=11):
    inst = Instrument()
    rng = np.random.default_rng(seed)
    mags = np.linspace(20.0, 12.0, N)
    f_syn = F_MAX_ADU * 10.0 ** (-0.4 * (mags - 12.0))
    x = rng.uniform(0.0, 100.0, N)
    y = rng.uniform(0.0, 100.0, N)
    sig = sc.noise_sigma_mag(f_syn, inst, SIGMA_PIX_E)
    f = f_syn * (1.0 + a * (f_syn / f_syn.max()) ** 1.0) if kind == "bright" \
        else f_syn * (1.0 + a * (x - x.mean()) / x.std())
    f = f * (1.0 + rng.normal(0.0, sig / np.log(10.0) / 2.5, N))
    return inst, f, f_syn, x, y


def sweep(kind, amplitudes):
    inst, _, f_syn, _, _ = frame(kind, 0.0)
    rows = []
    for a in amplitudes:
        _, f, fs, x, y = frame(kind, a)
        c0 = pl.calibrate(f, fs, None, x, y, m_degree=0)
        c2 = pl.calibrate(f, fs, None, x, y, m_degree=2)
        b = pl.budget_from_frame(fs, inst, SIGMA_PIX_E, **BUDGET_SYS)
        b.n = c0["n_inliers"]
        rows.append(dict(kind=kind, a=a,
                         sigma_obs=c0["sigma_obs_mag"],
                         md0_eq_md2=bool(c0["sigma_obs_mag"] == c2["sigma_obs_mag"]),
                         delta_after_m=c2["delta_after_m"],
                         floor=b.sigma_floor, ceiling=b.sigma_ceiling,
                         verdict=sc.gate_verdict(c0["sigma_obs_mag"], b)))
    return rows


def main():
    print("== A: sigma_obs_mag 对 m(x,y) 的免疫性（逐位比较 m_degree=0 / 2）==")
    for kind in ("bright", "pos"):
        for r in sweep(kind, (0.0, 0.02, 0.05)):
            print(f"  {kind:6} a={r['a']:<5} sigma_obs={r['sigma_obs']:.5f} "
                  f"m_degree0==2 -> {r['md0_eq_md2']}  delta_after_m={r['delta_after_m']:.5f}  "
                  f"verdict={r['verdict']}")

    print("\n== B: 下包络被未建模乘性残差推出的临界幅度（n=48 ≫ 12.236）==")
    print(f"  {'kind':6} {'a':>6} {'sigma_obs':>10} {'floor':>9} {'ceiling':>9} {'verdict':>18}")
    for kind in ("bright", "pos"):
        for r in sweep(kind, (0.0, 0.02, 0.03, 0.05, 0.08, 0.12, 0.20, 0.30)):
            print(f"  {kind:6} {r['a']:6} {r['sigma_obs']:10.5f} {r['floor']:9.5f} "
                  f"{r['ceiling']:9.5f} {r['verdict']:>18}")


if __name__ == "__main__":
    main()
