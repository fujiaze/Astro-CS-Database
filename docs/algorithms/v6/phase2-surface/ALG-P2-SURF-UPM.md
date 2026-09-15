# ALG-P2-SURF-UPM — UPM 乘法/加性分离 / gauge / 秩 / 条件数 / 参数协方差

- 文档 ID：`ALG-P2-SURF-UPM`（`ALG-P2-SURF-001` 子文档）
- 上位：`DESIGN-P2-001 §4`；宪章 §6.3；`UNIFIED §6`；`PROJECT_SPEC §3/§8`；`ADJ-OBS-01` / `ADJ-F-OBS-04` / `ADJ-F-OBS-05`
- 既有实现级合同：`docs/algorithms/UPM_SOLVER.md`（`ALG-UPM-001`）、`docs/algorithms/PHASE2_UPM_IMPL.md`（`ALG-P2-UPM-IMPL-001`）——本文件是**目标态乘加分离规格**，不复制其逐行锚；冲突以 `DESIGN-P2 §4` 为准。
- 本任务条款：`ALG-P2S-UPM.1..8`
- 文献锚：Padmanabhan et al. 2008 ApJ 674,1217（重叠观测联合相对光度标定、gauge/连通性）；Bertin 2010 SCAMP ASPC 442,435（多帧联合校准实践）

## 1 目标模型（`ALG-P2S-UPM.1`）

对重叠区域控制点 `p` 的每帧观测 `y_k(p)`（校准后背景/场估计，单位 ADU）：

```text
y_k(p) = g_k * s(p) + b_k + eps_k(p)
```

- `g_k`：乘法响应（光度尺度/透明度/零点，相对参考帧无量纲）；每帧一个，或空间模型的尺度场 `g_k(x)`；
- `b_k`：加性背景（帧级常数 + 空间梯度/场 `b_k(x)`）；单位 ADU；
- `s(p)`：潜在真实场（参考帧口径），单位 ADU；
- `eps_k(p)`：随机项，`Var(eps)=control_variance=k_corr*(pi/2)*sigma_bg^2/N_retained`（`FZ-PROV-KCORR`，见 §6）。

**分离不变量**（宪章 §6.3）：乘法光度比例 `g_k` 与加性背景 `b_k` 必须**分别估计**，**不得互相代替**，尤其**禁止把乘法光度比例隐藏在加性梯度曲面 `b_k(x)` 中**。加性校正场 `C_i(p)`（现行 `UPM_SOLVER.md F4` 的 centered 双线性 8×8）只承载空间变化背景/梯度，不承载全局尺度。

## 2 gauge（`ALG-P2S-UPM.2`）

每个连通分量独立 gauge（沿用 `UPM_SOLVER.md F5` 的 min-frame_id 参考帧约定）：

```text
scale gauge:  g_ref = 1         （参考帧 = 分量内最小 frame_id）
level gauge:  b_ref = 0         （参考帧零加性背景）
```

- 乘法尺度与加性背景各消耗 1 个 gauge/分量（共 `2 × n_components`）；
- 断开分量各自独立；无观测几何节点不参与数据图（sentinel，`UPM_SOLVER.md §11`）；
- `s(p)` 由参考帧口径定义（`g_ref=1, b_ref=0` 时 `s(p) = y_ref(p)`）。

## 3 秩与可辨识性（`ALG-P2S-UPM.3`）

设 `n_p` 控制点数、`F` 帧数、`n_components` 连通分量数。未知数 = `n_p + 2F`，gauge = `2*n_components`。

```text
可辨识性判据:  rank(J) == n_p + 2F - 2*n_components    （J = 观测模型 Jacobian，于解处求值）
奇异值秩判据:   sigma_i / sigma_max > FZ-AP2S-RANK-RTOL (=1e-10)
```

- **最少帧数**：`FZ-AP2S-UPM-MINFRAMES = 2`——乘法尺度 `g` 与加性背景 `b` 的可辨识需每分量至少 2 帧；**单帧区禁止拟合 `g`**，须显式声明 additive-only 降级（`g=1` 固定、`b` 无法分离时标 `unavailable`）或 harmonic continuation 延拓（见 `UPM_SOLVER.md F5` 单帧区）。
- **杠杆臂**：`g` 与 `b` 可分离还需 `s(p)` 非恒常（存在空间变化）；若 `s` 恒常，`g*s+b` 退化为 `(g*S0)+b` 的一维家族，g/b 不可辨识 → 该分量 fail-closed。
- 秩亏 / 断图 / 显著模型失配 → 显式失败或分组件（`DESIGN-P2 §4`）。

## 4 条件数（`ALG-P2S-UPM.4`）

```text
kappa = cond_2( D^-1 A^T W A D^-1 ),   W = C_in^-1,  D = diag(列范数)  （列均衡消除单位尺度伪病态）
```

- `FZ-AP2S-KAPPA-MAX = 1e6`：超限 → 分组件或 `unavailable`，**禁止**静默欠定解（`DESIGN-P2 §4`）。
- 判据来源（登记）：FP64 相对精度 `≈1e-16`，`kappa=1e6` 约损失 6 位有效数字，参数协方差相对误差 `≈1e-10`，仍远小于统计/科学容差（`1e-6` 收敛门量级）；`kappa>1e6` 表示秩亏正在发展，应 fail-closed。
- 收敛与阈值沿用 `UPM_SOLVER.md`：Huber `δ=1.345`、`max_iter=100`、`tol=1e-6`、`sigma_floor=1e-3`、`zero_anchor=1e-3`（**不重新定义**）。

## 5 参数协方差与传播（`ALG-P2S-UPM.5`；`ALG-P2S-UPM.6`）

```text
C_theta = (J^T W J)^-1                     （gauge 消除后的可辨识子空间）
C_out   = C_stat + J_out C_theta J_out^T   （传播到输出；J_out = partial(output)/partial(theta)）
```

- UPM 光度尺度/背景参数协方差 `C_theta` **必须**传播到最终 covariance（`DESIGN-P2 §4`；`SCI-P2-001-COVARIANCE-EPSF §4`）。
- 共享系统项进低秩 `L L^T`/相关核/共同 master ID+强度参数，**禁止**伪装独立随机 ivar（`ADJ-OBS-01`；`ADJ-F-OBS-04`，`SC-ADJ-F04.1..3`）。
- 验证门：含/不含 UPM 项的方差比 > 1 必须可检出（`SCI-P2-001 Oracle C9` 判据，实测比值 2.0）。

## 6 k_corr 与 control_variance（`ALG-P2S-UPM.7`）

`control_variance = k_corr * (pi/2) * sigma_bg^2 / N_retained`；`control_ivar = 1/control_variance`（`UPM_SOLVER.md F1`；`ALG-UPM-CONTROL-IVAR-001` 承接）。
`k_corr` 定义与适用域**原样继承** `FZ-PROV-KCORR`（`ADJ-F-OBS-05`）：

```text
k_corr = Var(median) / [ pi * sigma_bg^2 / (2 * N_retained) ]
```

- 适用域（几何/pixfrac/control patch 尺寸/稳健估计器版本/是否球面 Drizzle 输出）显式；标定脚本 + 固定种子 MC 纳入可复跑证据；
- 可复跑标定落地前，只允许在**已声明适用域内**取 `k_corr=1.4`，**禁止外推/内插**；
- 改变 pixfrac/几何而不更新 provenance → 门变红（`ADJ-F-OBS-05` gate：`kcorr_ignores_correlation -> k_corr=1 rc!=0`）。

## 7 fail-closed（`ALG-P2S-UPM.8`）

| 条件 | 行为 |
|---|---|
| `control_ivar<=0`/非有限 | rc=2 显式失败（`UPM_SOLVER.md §4`；`PHASE2_UPM_IMPL §10`） |
| 秩亏（§3） | 分组件 / 显式失败 |
| `kappa > FZ-AP2S-KAPPA-MAX` | 分组件 / `unavailable` |
| 单帧分量拟合 `g` | 禁止；additive-only 降级须显式声明 |
| 恒常 `s` 场（g/b 退化） | 该分量 fail-closed |
| 缺 `control_ivar`（production） | rc=2，禁静默回退 legacy |
| 共享系统项按独立处理 | REJECT（`ADJ-OBS-01`） |
| `unavailable` 无原因 | REJECT（`FZ-PROV-MINIMAL-SET`） |

## 8 provenance（`ALG-P2S-UPM.8` 续）

必须记录：`gauge_mode`、每分量 `ref_frame_id`、`rank`、`rank_rtol`、`kappa`、`kappa_max`、`C_theta` 摘要、`k_corr` 值 + 适用域、`flux_conservation_factor`（若涉面亮度/通量换算）、`model_hash`、`min_frames`、`any_fail_closed_reason`（`FZ-PROV-MINIMAL-SET`；`ADJ-GEN-03`）。

## 9 与现行实现的关系（登记，不改码）

现行 `PHASE2_UPM_IMPL.md` 的模型是 `M_k`（latent reference）+ `C_i(p)`（加性校正场），**仅加性**；本文件规定 V6 目标态新增乘法 `g_k` 并强制乘加分离。实施责任：`IMPL-P2-UPM-001`（W5）；数据面/schema 归 `DATA-DESIGN-001`（W3）/`SCHEMA-INTEGRATE-001`（W6）。
本文件**不裁决**实现细节（CG/稀疏持久化/并行归约沿用既有合同），只冻结科学语义。
