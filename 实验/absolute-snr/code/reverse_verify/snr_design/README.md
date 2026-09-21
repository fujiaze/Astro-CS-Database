# SNR-DESIGN 数值实验

支撑 `实验/SCI-B/docs/snr-propagation-design.md` 的 5 个独立数值实验。
全部为 Python（numpy/scipy/astropy），**无需构建**。

> **复核分片 SNR-EXP-AUDIT**：本目录下的 `audit/` 是对上述 5 个实验的**方法论复核**
> （`GAP_AUDIT §9.41` 合成测试必须模拟真实物理实现；`§9.42` 禁用物理闭合反推；`§9.46` SP-0 降为诊断量）。
> 结论与订正见设计文档 §13。**原实验脚本与 JSON 一律保留不改**，复核意见以「原结论 / 复核后」并列。

## 复跑

```bash
cd 实验/SCI-B/code/reverse_verify/snr_design
TMPDIR=/dev/shm/astrocs_snrd ./run_all.sh                     # 原 5 个实验
TMPDIR=/dev/shm/astrocs_snraudit ./audit/run_all_audit.sh     # 复核分片（4 脚本，约 6 分钟）
```

结果落 `run/reverse_verify/snr_design/`（原实验）与 `run/reverse_verify/snr_design/audit/`（复核），
并在本目录 / `audit/` 留一份小 JSON 作为证据。

## 实验清单

| 脚本 | 真值来源 | 验证的论断 | 关键判据（先写后跑） |
|---|---|---|---|
| `exp1_weight_penalty.py` | 解析式 + Monte Carlo | 权重误差的效率惩罚 `Var_approx/Var_opt = 1 + Var_p(δ)`；共模误差免费；残差制造者方差 `(N+1)/(N−1)` | 解析式与 MC 必须 5 位小数一致；共模 ratio 必须恰为 1.0 |
| `exp2_sparse_snr_reconstruction.py` | 真实帧 + 变差函数拟合 | 局部 SNR 场的相关长度；5 种间距 × 5 种重建算子的 RMSE | 打乱控制点后 RMSE 必须显著变差（负例） |
| `exp3_multiframe_weight_penalty.py` | 真实 4 帧重叠数据 | 帧标量 σ 一致性；**真实数据的权重效率惩罚** | 帧标量 σ 比值阈值 1.05；稀疏层需按 SP-0 判据才启用 |
| `exp4_kriging_scaling.py` | 已知核的平稳高斯场 | kriging vs 双线性的误差随 Δ/ℓ 的标度律；网格不可分辨功率占比 | nugget 必须物理（`1e-10` 会让预测方差恒 0） |
| `exp5_error_budget.py` | 解析 + 实测输入 | 叠加增益（√N 律）；成品 σ 的误差预算合成 | 每一项必须有证据来源，无来源即红 |

## 复核分片清单（audit/）

| 脚本 | 输出 | 内容 |
|---|---|---|
| `audit/physnoise.py` | — | 物理前向噪声模型库（真实帧作底 + Poisson/Gaussian/量化/平场/天空梯度） |
| `audit/audit_sim_validation.py` | `audit_sim_validation.json` | V1–V4 自校验；无量纲噪声亲和度；**天光红线**（算术加常数 = 0.0000%） |
| `audit/audit_sp0.py` | `audit_sp0.json` | SP-0 精确式重推 + 物理噪声验证 + 负例 + 两条失效模式 |
| `audit/audit_exp3_physical.py` | `audit_exp3_physical.json` | EXP-3 结论物理重算；0.063% 的窗口敏感性；负例套件 |
| `audit/audit_exp1245.py` | `audit_exp1245.json` | EXP-1 算子核对 / EXP-2 变差函数重拟合 / EXP-4 逐格复算 / EXP-5 预算复核 |
| `audit/audit_mosaic_shape.py` | `audit_mosaic_shape.json` | **真实跨板块**对照：同指向 vs 不同指向下 SP-0 的适用域 |

## 输入数据（只读）

- `run/RELEASE-02/L4-rebuild/norm/<tile>/calibrated_*.fts` — 真实校准帧（4096²，`>f4`）
- `run/RELEASE-02/L4-rebuild/norm/<tile>/p1_snr.json` — 生产 Phase-1 SNR 产品
- `run/RELEASE-02/L4-rebuild/norm/<tile>/p1_sources.json` — 生产 Phase-1 源表

**注意**：`p1_sources.json` 里 `file` 字段是 `cleaned_*`，而校准帧文件名是 `calibrated_*`（**去掉 `cleaned_` 前缀**）。
直接拼 `calibrated_ + cleaned_*` 会找不到文件——EXP-3 首轮因此静默失败（`need at least one array to stack`）。

## 已知陷阱（写进实现判据）

1. **kriging nugget 必须物理**：取控制点测量方差 `sigma_value²`。用 `1e-10·I` 会使 `1 − kᵀw ≈ 0`，预测方差恒为 0。
   （**例外**：EXP-4 的控制点是**场的真值、无测量噪声**，那里 `1e-10` 只是条件化抖动，是**正确**的——见设计文档 §13.4.3。）
2. **评价窗口必须是网格内点**：stride=64 的网格跨度为 64 < 80 时窗口不内点，必须显式跳过而不是让广播失败。
3. **文件句柄**：解析 ~100 MB 的 JSON 时逐个 `close`，否则多 tile 循环会 OOM。
4. **物理仿真**：`template_noise_model()` 返回的是**方差**，传给 `simulate_frame(template_sigma_adu=...)` 前必须开方；
   patch 网格降采样要平均**方差**再开方，不能平均 σ 再开方（否则真值 σ 会差一个量级）。
5. **GAP_AUDIT §9.42**：不得用任何物理闭合式反推增益/口径/曝光；`G`/`RN` 是**声明参数**，
   结论必须给出尺度不变性扫描。
