# A5（SMOOTH-LAMBDA）判据预登记（**先写判据，再看结果**）

> 依据 `reverse_verify/README.md` §3.3「判据先行：先写判据与阈值，再看结果；不得事后放宽」。
> 本文件在 λs 扫描结果产生**之前**写入。若最终判据需修订，修订必须**另立条目并说明理由**，不得就地放宽。

## 0 量定义（全部**尺度无关**，无量纲或相对量）

| 量 | 定义 |
|---|---|
| `removal_frac` | `1 - median_k std_f(z_fk) / median_k std_f(y_fk)`，`z` = 校正后。1 = 逐帧天光差被完全去除；0 = 完全没去除 |
| `resid_over_noise` | `median_k std_f(z_fk) / median_k sqrt(mean_f unc_fk^2)`。1 = 残差已到测量噪声底；>1 = 还有未去除的帧间结构 |
| `resid_over_sky` | `median_k std_f(z_fk) / median_k std_f(B_fk)`（真值在场时）。1 = 天光一点没扣；0 = 扣净 |
| `step_ratio` | 覆盖子集突变边 `|dStack|` 中位 / 同子集内部边中位（Q2 §6.2 口径，与 FIX-P2a 的 0.96 同口径） |
| `dim_bright_med` | `median_{k in 亮区前10%} (M_est(k) - M_target(k))/M_target(k)`，`M_est` = 单位权叠加产品，`M_target` = 逐帧真值面的覆盖均值 |
| `dim_peak_med` | 同上，亮区前 1%（M42/M16 核心式突兀峰值） |
| `dim_faint_diff` | `dim_bright_med - dim_faint_med`（差分压暗，抵消全局零点） |
| `c_grad_rms` | C 场同 tile 相邻 control 差的 RMS（过平滑直接观测量） |

> **单位声明（A5 不依赖物理单位）**：所有判据只用**相对量 / 比值**。合成场景的增益/读出/暗电流
> 是**正向模拟参数**，不是从 FITS 头或数据反推的物理量；生产数据一律称「流水线标定单位」，
> 不赋予物理量纲。λs 的最优值对绝对标度不变（见判据 C7）。

## 1 判据

- **C1 天光去除有效性**：推荐 λs 处 `removal_frac >= 0.80` 且 `resid_over_noise <= 3`（场景 A_base、B_prod）。
- **C2 台阶消除**：推荐 λs 处 `step_ratio <= 1.2`（真实 L4 real49 与 A_base）。
- **C3 亮区保真**：推荐 λs 处 `|dim_peak_med| <= 1%` 且 `|dim_bright_med| <= 0.5%` 且 `|dim_faint_diff| <= 1%`（A_base、B_prod、A_vary）。
- **C4 负例 1（真值无天光）**：A_nosky / B_nosky 处 `removal_frac <= 0.05` 且 `median|C| <= 0.2 x median|C|(对应有天光场景同 λs)`。
- **C5 负例 2（真值无亮结构）**：A_nocore 处 `|dim_peak_med| <= 0.2%` 且 `|dim_bright_med| <= 0.2%`。
- **C6 红例（能红）**：λs=0 必须 `step_ratio >= 1.5`（C2 红）；λs >= 1000 必须 `removal_frac <= 0.5`（C1 红，过平滑 ⇒ 天光没扣掉）。
- **C7 尺度不变性**：A_base 与 A_scale（x1e9）的 λs 曲线逐点相对差 <= 1e-6（判据对绝对标度不敏感）。
- **C8 负责人观察**：A_base（亮峰各帧一致）与 A_vary（亮峰帧间抖动 10%）对比 —— 若两者 `|dim_peak_med|` 都 <=1% ⇒ 「各帧都亮 ⇒ 不构成大问题」**成立**（且过平滑不压暗）；若仅 A_vary 显著 ⇒ 观察**成立且必要**（帧间一致是缓解因素）。

## 2 推荐区间规则

推荐区间 = 同时满足 C1 且 C2 且 C3 的 λs 集合。若集合为空 ⇒ 判「冲突」，**不得**为迎合「λs>0 一定好」而放宽任一判据；须给出冲突的量化边界与「待定 + 判定方法」。
