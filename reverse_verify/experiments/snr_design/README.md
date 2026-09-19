# SNR-DESIGN 数值实验

支撑 `reverse_verify/docs/snr-propagation-design.md` 的 5 个独立数值实验。
全部为 Python（numpy/scipy/astropy），**无需构建**。

## 复跑

```bash
cd reverse_verify/experiments/snr_design
TMPDIR=/dev/shm/astrocs_snrd ./run_all.sh
```

结果落 `run/reverse_verify/snr_design/`（日志 + JSON），并在本目录留一份小 JSON 作为证据。

## 实验清单

| 脚本 | 真值来源 | 验证的论断 | 关键判据（先写后跑） |
|---|---|---|---|
| `exp1_weight_penalty.py` | 解析式 + Monte Carlo | 权重误差的效率惩罚 `Var_approx/Var_opt = 1 + Var_p(δ)`；共模误差免费；残差制造者方差 `(N+1)/(N−1)` | 解析式与 MC 必须 5 位小数一致；共模 ratio 必须恰为 1.0 |
| `exp2_sparse_snr_reconstruction.py` | 真实帧 + 变差函数拟合 | 局部 SNR 场的相关长度；5 种间距 × 5 种重建算子的 RMSE | 打乱控制点后 RMSE 必须显著变差（负例） |
| `exp3_multiframe_weight_penalty.py` | 真实 4 帧重叠数据 | 帧标量 σ 一致性；**真实数据的权重效率惩罚** | 帧标量 σ 比值阈值 1.05；稀疏层需按 SP-0 判据才启用 |
| `exp4_kriging_scaling.py` | 已知核的平稳高斯场 | kriging vs 双线性的误差随 Δ/ℓ 的标度律；网格不可分辨功率占比 | nugget 必须物理（`1e-10` 会让预测方差恒 0） |
| `exp5_error_budget.py` | 解析 + 实测输入 | 叠加增益（√N 律）；成品 σ 的误差预算合成 | 每一项必须有证据来源，无来源即红 |

## 输入数据（只读）

- `run/RELEASE-02/L4-rebuild/norm/<tile>/calibrated_*.fts` — 真实校准帧（4096²，`>f4`）
- `run/RELEASE-02/L4-rebuild/norm/<tile>/p1_snr.json` — 生产 Phase-1 SNR 产品
- `run/RELEASE-02/L4-rebuild/norm/<tile>/p1_sources.json` — 生产 Phase-1 源表

**注意**：`p1_sources.json` 里 `file` 字段是 `cleaned_*`，而校准帧文件名是 `calibrated_*`（**去掉 `cleaned_` 前缀**）。
直接拼 `calibrated_ + cleaned_*` 会找不到文件——EXP-3 首轮因此静默失败（`need at least one array to stack`）。

## 已知陷阱（写进实现判据）

1. **kriging nugget 必须物理**：取控制点测量方差 `sigma_value²`。用 `1e-10·I` 会使 `1 − kᵀw ≈ 0`，预测方差恒为 0。
2. **评价窗口必须是网格内点**：stride=64 的网格跨度为 64 < 80 时窗口不内点，必须显式跳过而不是让广播失败。
3. **文件句柄**：解析 ~100 MB 的 JSON 时逐个 `close`，否则多 tile 循环会 OOM。
