# 插件文档：sampling（控制采样）

> 上游：ASTROCS_DESIGN.md §5.2（固定科学流程）

## 1. 职责与边界

- **职责**：为 UPM 拟合提供两类稀疏采样——① 光度控制点（定每帧**加性**校正场；本期 `g_k ≡ 1`，不做乘性响应）；② **天光背景采样点**（定加性天光面 `C_k(x)`）；同时生成星点掩膜，保证模型可辨识。
- **不是**：不做模型拟合（upm）；采样点的控制权重（`control_ivar`）不是最终科学叠加权重（最终权重在 integration 由逆方差产生）；不做排异推断（rejection 可辅助但独立）。

## 2. 权威依据

- 最高设计 `ASTROCS_DESIGN.md` §5.2（固定科学流程：控制采样）、§5.4（天光平面与统一相对模型）
- `docs/design/PHASE2_DETAILED_DESIGN.md` §4（UPM 控制点要求）
- `docs/plugins/algorithms_phase1/07_noise_snr.md`（帧级/帧内 SNR）

## 3. 输入/输出数据合同

- **输入**：Phase1 产品组、coverage、validity、检测目录、帧级 SNR 与（若启用）稀疏帧内 SNR 层、配置。
- **输出**：
  - `star_mask`：星点/高结构掩膜（天球坐标，随控制点集分发）；
  - `phot_control_points`：光度控制点集（坐标、值、噪声、mask）；
  - `sky_samples`：**天光背景采样点集**（每帧一批；坐标（帧像素 + WCS 天球）、背景估计值、variance、该点 SNR、参与标记）；
  - 覆盖与统计（每帧点数、空间分布、SNR 分布）。
- 参考：`eng/contracts/schemas/control_points.schema.json`。

## 4. 算法与公式要点

### 4.1 星点掩膜

- 掩膜排除：检测目录星点（按 PSF 半径膨胀）、饱和与溢出区、坏点/宇宙线修正区、亮星光晕、高结构区域（星云边缘、星系）；
- 掩膜同时用于光度控制点与天光采样点；移动源/瞬变源区域打标记供 rejection 参考。

### 4.2 天光背景采样点（稀疏）

```mermaid
flowchart LR
    F["每帧校准图像"] --> M["套用星点掩膜"]
    M --> GRID["空间分层网格"]
    GRID --> P["每格取稀疏背景采样点<br/>（局部稳健背景 + 方差）"]
    P --> W["每点赋 control_ivar 权重<br/>w_ki = control_ivar_ki = N_retained/(k_corr·(π/2)·σ_bg²)"]
    W --> OUT["sky_samples 稀疏点表"]
```

- **稀疏而非稠密**：按空间分层网格在每帧掩膜外取一批采样点（数量由天光面自由度决定，远少于像素数）；不生成逐像素背景栅格；
- 每点在格内做局部稳健背景估计（如 σ-clipping/中位数小窗），记录值与 variance；
- 每点携带该位置的 SNR：有稀疏绝对 SNR 层时由控制点重建得到 `SNR(x,y)`，无稀疏层时用帧级标量；
- **采样点权重 = 逆方差**：`w_ki = control_ivar_ki = N_retained/(k_corr·(π/2)·σ_bg²)`（= `1/control_variance`，冻结式 `control_variance = k_corr·(π/2)·σ_bg²/N_retained` 见 `docs/modules/phase2_samp.md` §6 / `docs/contracts/DATA_SEMANTICS.md` §23）——低 SNR 帧、光污染帧的采样点权重自然变小，无法把正常帧的天光面异常拉高；**为什么不是 `SNR²`**：`w = 1/σ² = SNR²/F_ref² ∝ SNR²` 的 `∝` 以**固定参考通量** `F_ref` 为前提，而天光控制点的**被估量本身在变**（估的是天光面/背景电平，不是固定源通量）⇒ `SNR²` **不是**有效逆方差代理；控制点权重一律取 `control_ivar`，SNR 只作 veto/质量门（推导与依据见 `docs/science/CONTROL_WEIGHT_SNR.md` 与 `docs/modules/phase2_samp.md` §6）。
- 采样点经 WCS 映射到天球坐标，供跨帧联合拟合。
- **公共面与逐帧梯度的分工**：采样点用于**全部帧联合**拟合公共天光面 `B_ref(x)`；每帧只在其上拟合平缓梯度 `δ_k(x)`，归一施加量为 `δ_k`（**保留 `B_ref`**）；`raw − C_k`（全减，含 `B_ref`）不是默认路径（详见 `11_upm.md` §4.1/§5）。

### 4.3 光度控制点

- 控制点避开源（检测目录）、饱和、坏点、高结构区域；
- 空间均匀 + 按背景/噪声分层，保证加性场与天光面可辨识（本期 `g_k ≡ 1`，不估计乘性响应）；
- 记录每个控制点的 signal、variance、validity、是否参与拟合。

### 4.4 数量与失败条件

- 天光采样点与光度控制点的数量下限由 UPM 自由度（样条节点数/阶数）决定；
- 有效点不足、空间连通性不支持样条拟合 → 明确失败，由 upm fail-closed；
- **可辨识性判决只有一条口径**（欠定与病态是同一条不等式的两种读法）：判在**未正则化**的列均衡数据信息矩阵上，唯一相对阈值 `τ = rank_rtol`（地板 `max(m,n)·eps`）。**设门位置 = 这一个矩阵**（绝对条件数上限、正则化后求解矩阵均属另一口径：正则化后条件数有上界 ⇒ 恒真门）。判决位与全部读数（`identifiable` / `rank_eff` / `n_unidentified` / `rank_rtol_effective` / `kappa` / `dof_eff` / `chi2_red`）见 `11_upm.md` §4.7。

## 5. 配置项

| 字段 | 默认 | 单位 | 说明 |
|---|---|---|---|
| `spacing` | —— | px/deg | 空间采样间隔（光度控制点） |
| `sky_sample_spacing` | —— | px/deg | 天光采样点网格间距（粗于像素网格；须与**由输入几何导出**的样条节点间距相容——每个节点邻域内有足够采样点） |
| `bright_star_mask` | —— | —— | 亮星排除半径（按 PSF 倍数） |
| `min_control_points` | —— | —— | 光度控制点数量下限 |
| `min_sky_samples` | —— | —— | 每帧天光采样点数量下限 |
| `local_estimator` | `robust_median` | —— | 局部背景估计器（robust_median/trimmed_mean） |

采样/天光面的**施加侧**配置（`additive_mode`、`sky_plane.enabled`）登记在 `11_upm.md` §5（采样模块只产点表，不施加归一化）。

## 6. 接口/ABI

- entrypoint：产品组+coverage+validity+检测目录+SNR → star_mask + 光度控制点集 + 天光采样点集；
- 输出被 upm 消费；采样点表是稀疏小对象，随 Phase2 中间产品落盘。

## 7. 错误与边界

- 控制点/采样点不足、连通性断裂 → fail-closed（UPM 欠定/不可辨识），判据口径见 §4.4（唯一判据，绝对条件数上限属另一口径）；
- 高结构区域误入 → 标记，不进拟合；
- 帧内大片掩膜（如星云占满视场）导致采样点空间分布退化 → 报告覆盖缺口，降阶或分组件处理；
- 移动源区域标记（供 rejection 参考）。

## 8. 测试与 Oracle

- 构造已知背景梯度/已知加性天光面 `C_k(x)` 场景 → 采样点估计无偏、覆盖与统计符合预期；
- 亮星/坏点/星云边缘排除验证；
- **control_ivar 加权验证**：注入低 SNR/光污染帧，联合天光面不被拉高（与等权拟合对照，偏差显著减小）；
- 稀疏性验证：采样点数量级远低于像素数，内存占用随点数而非像素数增长；
- 欠定检测（点数不足/连通性断裂时报错）；
- 1 worker vs N worker 一致。

---

## 9 SCI-C 实测结论（RELEASE-04 / SCI-403）

实验单元 `实验/additive-sky-seamless/`（报告 `README.md`、结果 `results/*.json`、复跑 `code/run_all.sh`）。

1. **`control_ivar` 是三臂中最优的采样权重**：伪影漏入 0.0245 e⁻，比 uniform（0.0651）小 2.7×、
   比 `SNR²`（0.3203）小 **13×**；真值加权 RMS 亦最小（1.532 < uniform 1.674 < SNR² 2.069 e⁻）。
   **边界（必须同引）**：相对 uniform 的 RMS 优势 8.5% 小于 NMC=20 的 MC 误差（std 0.698 e⁻），
   ⇒ 在**噪声项**上与等权不可分辨；决定性优势在**偏差漏入**。
2. **完整链路实测**：1374 采样点 / 4 帧（每帧 ≥4），星点掩膜覆盖最亮 0.1% 像素 100%，
   掩膜面积占比 1.19%，联合天光面 49 节点、`identifiable = 1`（`r_eff == n_params`）、
   χ²_red 0.771、δ_k 非零。
3. **稀疏性实测**：稀疏模型 14,001 B vs 稠密栅格 8,389,129 B（**0.167%**）；
   节点/像素 = 1.87e-4；按需求值 64² 块峰值 RSS 11,688 kB < 稠密物化 512² 的 14,568 kB；
   子集现场求值与全网格求值**逐位相同**（δ_k、b_k 均为 0.0）。
4. **真实数据**（M42 M1 T3 Red 4 帧）：生产天光面 rc=0、`identifiable = 1`（`r_eff == n_params`，
   `n_params` = 判据矩阵的阶）、χ²_red 1.004；同一次求解的 `κ(H_red) = 3.16e7` 是**诊断读数**，
   **它的用途 = 诊断**（合格判定只看 `r_eff == n_params` ⟺ `κ < 1/τ`，τ = `rank_rtol`）。
   真实帧间背景乘性斜率中位 0.995（0.806–1.323，分块动态范围仅 ~10 ADU ⇒ 不确定度大）。
5. **收敛状态与容差**：`converged` 是状态枚举 `0=max_iter / 1=converged / 2=stalled / 3=invalid`
   （`p2_upm_convergence` 与 `p2_upm_model.json#identifiability.converged` 同源）。
   绝对容差 `tolerance=1e-6` 在 ~300 e⁻ 尺度下 300 次迭代仍不收敛（判据与适用域见
   `docs/science/PHASE2_UPM.md` §16.3）；**不收敛不阻塞**——产品照出、构建 rc 不变，
   但 `warning_codes` 必须含 `P2-UPM-NOT-CONVERGED`（见 `11_upm.md` §4.6）。

