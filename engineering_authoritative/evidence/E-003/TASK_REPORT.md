# E-003 TASK REPORT — 全局加性共识曲面稀疏求解器

- Gate: E
- 状态: PASS
- 依赖: E-002
- 实现文件: `lib/healpix_db/healpix_stack/python/e_chain/e_solver.py`

## 1. 任务目标

将三片控制点组成全局方程组, 求解全局加性曲面 (每帧一个偏移量), 约束: 全局零均值, 只加性, 用 scipy.sparse 求解。

## 2. 数学模型

观测方程 (每个控制点 i 属于帧 f(i)):
```
signal_i = Σ_m c_m × B_m(ra_i, dec_i) + off_{f(i)} + noise_i
```

参数向量 x = [c_0..c_5, off_0, off_1, off_2] (M=6 曲面系数, F=3 帧偏移)

曲面基底: [1, ra', dec', ra'×dec', ra'², dec'²] (中心化, D=2)

约束: Σ off_f = 0 (零均值, 大权重等式行 1e6×max_data_w)

加权最小二乘: min Σ w_i × (signal_i - A_i×x)² + λ×(Σ off_f)²

求解: scipy.sparse.linalg.lsqr (稀疏, 稳健, 处理秩亏)

## 3. 求解结果

### 系统规模
- N=2400 控制点 (仅 valid), M=6 曲面系数, F=3 帧偏移
- 总参数: 9, 总方程: 2401 (含零均值约束行)
- 中心: (ra0=272.8954, dec0=-18.2009)

### 曲面系数 (中心化基底)
| 系数 | 值 | 含义 |
|------|------|------|
| c0 | 27269.38 | 常数项 |
| c1 (a_ra) | 197.69 | RA 方向线性梯度 |
| c2 (b_dec) | -267.25 | Dec 方向线性梯度 |
| c3 | 12.94 | ra×dec 交叉项 |
| c4 | -64.73 | ra² 项 |
| c5 | -25.76 | dec² 项 |

### 帧偏移 (零均值约束)
| Panel | Offset |
|-------|--------|
| panel1 | +33.93 |
| panel2 | +222.19 |
| panel3 | -256.12 |
| **Σ** | **-5.68e-13** (≈0, 零均值满足) |

### 残差统计
| 指标 | 值 |
|------|------|
| lsqr istop | 2 (收敛) |
| lsqr iters | 14 |
| residual WRMS | 4686.70 |
| residual RMS | 5363.58 |
| residual max | 27526.48 |
| zero_mean_error | 5.68e-13 |

## 4. 契约合规

| 契约 | 状态 | 说明 |
|------|------|------|
| 不得使用无权重梯度 | PASS | W^(1/2)A x = W^(1/2)b, 加权最小二乘 |
| 不得选单一参考帧 | PASS | 全局共识曲面, 非差异拟合 |
| 不得加入乘性项 | PASS | 参数只有 surface_coeffs + offsets, 无 scale |
| 全局零均值规范 | PASS | Σ off = -5.68e-13 ≈ 0 |
| 只加性 | PASS | signal = surface + offset, 无乘性 |

## 5. 证据文件

- `e003_result.json` — 结构化结果
- `pipeline_summary.json` — 全 pipeline 汇总

## 6. 运行命令

```
python lib/healpix_db/healpix_stack/python/e_chain/run_e_pipeline.py
```

elapsed: 0.003s
