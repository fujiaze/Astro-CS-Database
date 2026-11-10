# B0｜五份 foundation 正向推导审查与修订（2026-11-11）

> 受检基线：`3d8d5eb01`。方法：只读五份 foundation 正本，按 `science/04-FOUNDATIONS-AND-PROOFS.md` 的七条主线逐项核对；不以当前 C++ 运行结果反推公式。修订只改 `docs/science/foundation/`，未改代码/容差/真值。

## 1. 七条主线的覆盖表

| 主线 | 现有正本位置 | 裁决 |
|---|---|---|
| 世界中的量：I、f、μ、gain/面积/QE 可辨识 | `OBSERVATION_COORDINATES` §1 式(1)(2)、§1.1（本轮新增） | **已闭合**：乘积不可分证明、母版 shared root、单位/标度边界 |
| 真实观测：Poisson+read+母版+ADU/x | `OBSERVATION_COORDINATES` §1；`STATISTICS_SKY_COADD` §1 式(1) | **已闭合**：扣 sky 后 shot noise 仍在；unknown 不填零；gain 未测不能造 shot noise |
| 坐标：PixelCenter0/Edge0→FITS1、TAN/SIP/CD/LONPOLE/极点/逆域 | `OBSERVATION_COORDINATES` §2–§3.3、式(3)(5)(6) | **已闭合**：一次 ±1、SIP 在 CD 前、LONPOLE 折进 CD 的前提限定为天顶投影、单射证书 |
| 点源/响应：像元 PSF 积分、有限窗尾、XP、α(u) | `GAIA_RESPONSE` §1–§3、§3.1（本轮新增）；`OBSERVATION_COORDINATES` 式(2) | **已闭合**：PSF 积分≠中心采样；尾通量不重归一；α 逐像元；rank/gauge 明确 |
| 几何：Ω/A/a、w、F/N/R、支持、B 界、同一 R | `SPHERICAL_DRIZZLE` §1、§1.1（本轮新增）、§5、式(9)–(14) | **已闭合**：同一 R 作用于信号/响应/噪声；R D_α 非交换反例；支持状态与盒界 |
| 统计/sky/coadd：V_F/V_B/V_ctrl、H、P、Γ/BLUE/PSD/QP、选择 coverage | `STATISTICS_SKY_COADD` §1–§5、§2.3（本轮新增） | **已闭合**：P=I−EKT 逐行推导、负交叉项反例、选择后条件 coverage 与 K_eff 边界 |
| 导出：Q、支持均值/积分、NaN/负/零、HiPS、恢复边界 | `OBSERVATION_COORDINATES` §5；`SPHERICAL_DRIZZLE` §5.2；`GAIA_RESPONSE` §4 | **部分**：Q 与支持积分有定义；B5 的“导出值不是宇宙真场/恢复一致性”仍需在 Export 正本补齐（见 §4） |

## 2. 本轮修订

1. `OBSERVATION_COORDINATES` §1.1：新增 \(A_0TQ E\) 乘积不可分证明（\(A_0\to cA_0,Q\to Q/c\) 不变）、gain 与电子数同乘不可分、母版 shared root 不得按帧独立。
2. `SPHERICAL_DRIZZLE` §1.1：显式写出 \(R_{pj}=w_{jp}/N_p\)、\(\sum_j R_{pj}\Omega_j=1\)，并证明同一 R 同时作用于信号、响应、噪声；给出 `R D_α x=1.5` vs `D_α R x=1` 的非交换反例。
3. `STATISTICS_SKY_COADD` §2.3：从 \(\hat\beta=Kc\)、\(z=y-E\hat\beta\) 推出 \(P=I-EKT\)；展开 \(P C_y P^T\) 的两个负交叉项；两帧共同均值反例给出 \(\mathrm{Var}(z_1)=\sigma^2/2,\mathrm{Cov}=-\sigma^2/2\)；明确选择后 coverage 不等于固定权公式。
4. `GAIA_RESPONSE` §3.1：给出常数颜色项与整体标度的 gauge、秩亏方向、每星 XP 潜尺度退化，以及 \(\alpha(u)\) 必须逐像元进入 PSF 积分。

## 3. 未推断边界（每条都有反例或构造）

1. 高 SNR 单星可定标 ≠ 物理 gain/未知星云窄线 SED：`GAIA_RESPONSE` §4 的窄线反例。
2. WCS 正逆 round-trip≈0 ≠ 与 ICRS/外部原点一致：`OBSERVATION_COORDINATES` §3.3 已限定小残差只是内部自洽。
3. 常数场守恒/面积总和 ≠ 每叶成员正确：`SPHERICAL_DRIZZLE` §1、§5 的 `Σa=A` 与 `D_p` 区分。
4. `R C Rᵀ` 可算 ≠ 未给完整 C 时已知真实方差：`STATISTICS_SKY_COADD` §1 unknown 语义。
5. 数值 rank 阈值 ≠ 已证精确物理零空间：`STATISTICS_SKY_COADD` §2 PSD 约束段；`GAIA_RESPONSE` §3.1 秩亏。
6. sky 残差小 ≠ 保留星云/暗云；固定权公式 ≠ 无条件 coverage：`STATISTICS_SKY_COADD` §2.3、§5。
7. HST 合成图 ≠ 无 PSF/噪声的宇宙真值：`INDEPENDENT_ORACLES` §7 已有边界。

## 4. 仍缺（B0 出口前必须补）

- **B5 Export 正本**：`Q` 对支持均值/支持积分的定义、NaN 无支持、合法负/零信号、HiPS 层级不是独立曝光、恢复一致性。这些散在 `OBSERVATION_COORDINATES` §5 与 `SPHERICAL_DRIZZLE` §5.2，尚无一处完整陈述；归 B5 任务，不在 B0 重复建定理。
- `INDEPENDENT_ORACLES` 尚未把“数值确定包围/随机统计/物理系统偏差/未知”四种语义映射到每个新反例；需要一次专门整理（D0）。
- 五份正本页首仍引用旧 `final-review.md` 的“无 P0/P1 设计阻碍”；本轮已加当前实现状态指针，但旧句不删除（历史保留），读者须以 `stage-report-02.md` 为当前裁决。
- 非交换性还缺“`Q R` 在 GeometryRevision/支持/权/sky 改变后不能复用旧 Q”的完整反例；B5 与 C0 应补。

## 5. 裁决

五份 foundation 的七条主线中六条已有可逐式复核的当前陈述，并补上了此前缺失的三处关键推导（乘积可辨识、同一 R 与非交换、P 的负交叉项）。**B0 不能标 PASS**：B5 的 Export 科学语义与 D0 的四语义映射仍未闭合；本报告只授权在 B0 范围内继续修订，不授权任何 Process 的“科学实现升级”。
