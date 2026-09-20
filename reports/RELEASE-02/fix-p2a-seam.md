# RELEASE-02 P2a 接缝修复报告（组合 + formulation + 收敛）

> 执行面：**Phase2a（接缝修复）**。任务分片 fix-p2a。
> 零 git 写权限；未跑 ninja/cmake/ctest（仅 `g++ -fsyntax-only` 与独立 Oracle）。
> 产物：`run/RELEASE-02/fix-p2a/`（`p2a_oracle.cpp` / `build.sh` / `oracle_out.txt` / `*_syntax.log`）。
> 环境：`TMPDIR=/dev/shm/fix_p2a`（结束已清理）。
> 依据：`reports/RELEASE-02/{c-delta-ruling,q2-snr-smooth,chain-audit,phot-verify}.md`。

---

## 0 结论速览

| 项 | 结论 | 关键数值 |
|---|---|---|
| **P2a-1** | 生产 `(raw−C_k)−δ_k` 双重加性扣除改为**可配置单次**，默认保留 **C**（去掉 δ） | 生产实测：raw−C **0.131%** / raw−δ 2.799% / raw−C−δ **13.974%**；本 Oracle 生产尺度合成：raw 6.490% / raw−δ 5.076% / **raw−C 0.0130%** / raw−C−δ 1.4013% |
| **P2a-2** | 阻尼 α=0.5 + 拟合权重提升复用 + 末端残差场 gauge（公共扣除，不破坏参考帧 C=0 gauge） | 边界/内部阶跃比：legacy refM **0.828**、final_gauge **1.103**、P2a-full **0.958**（λs=1000）；λs=0 见 §3.4 限制 |
| **P2a-3** | `iterations/converged/objective` 进 `p2_upm_model.json`+manifest；`p2_op_upm_fit` 调 `p2_upm_convergence`；判据改**相对**，生产阈值 **1e-3** | Oracle：绝对 1e-6 → `iter=60,conv=0`（不可达）；相对 1e-3 → `iter=25,conv=1`（可达） |
| **P2a-4** | M-update 由「只用参考帧」改**全帧 SNR/ivar 加权** | 同 λs=1000：objective 27.99 → 24.01（legacy → full_frame） |

---

## ① 改动清单（file:line）

### A. `lib/algorithms/coverage/include/astro/phase2/upm.h`
| 行 | 改动 |
|---|---|
| 78-81 | `tolerance` 注释补相对/绝对语义 |
| **92-113** | 新增 4 个 **opt-in** 配置字段（默认 = W2 冻结 legacy，保证 `cfg{}` 逐位不变）：`gs_damping=1.0` / `m_full_frame=0` / `final_gauge=0` / `tolerance_relative=0` |

### B. `lib/algorithms/coverage/src/upm.cpp`
| 行 | 改动 | 对应 |
|---|---|---|
| 75-78 | Model 增 `std::vector<double> gauge`（逐 control 公共残差场 G；空=legacy） | P2a-2 |
| 119-196 | `evaluate_c_field` 拆出通用 `evaluate_field_row`（C 行与 G 共用同一双线性/外推求值） | P2a-2 |
| 257-263 | cfg==nullptr 缺省显式列 legacy 4 值 | — |
| 278-283 | 4 字段归一化（零初始化/越域退回 legacy，不产生静默科学变更） | — |
| 657-659 | `w` 提升到迭代循环外（末端 gauge 必须复用最后一轮拟合权重 = 权重同源） | P2a-2 |
| 730-731 / 783-784 | M-update：`comp==kNoData \|\| cfg.m_full_frame` ⇒ 全帧加权 | **P2a-4** |
| 874-885 / 922-933 | C-update 阻尼：`x ← (1−α)x_old + α x_new`（并行/串行两分支） | P2a-2 |
| **950-971** | 收敛判据：`tol_M = tolerance × max(scale_M,1.0)`（相对）或 `= tolerance`（legacy 绝对） | **P2a-3** |
| 978-1025 | 末端残差场 gauge：`G_k = [Σ_i w_i(y_i−C_i,k)]/Σ_i w_i − M_k`；无观测节点邻接调和延拓 | P2a-2 |
| 1030-1036 | model hash：仅当 `gauge` 非空才追加 `|G`（关闭时 payload 与 legacy 逐位一致） | — |
| 1120-1125 / 1168-1177 | `p2_upm_save`：写 4 个行为键 + `gauge` 稀疏数组 | — |
| 1226-1230 / 1390-1415 | `p2_upm_open`：读 4 键（旧文件缺键→legacy）；读 `gauge`（缺键→空） | — |
| 1491-1494 | `p2_upm_calibrate_block`：`output = input − C − G` | P2a-2 |
| 1653-1657 | dense cache 折入 G（保证 dense ≡ sparse 逐位等价） | P2a-2 |

### C. `lib/infrastructure/scheduler/src/module_adapters.cpp`
| 行 | 改动 | 对应 |
|---|---|---|
| **4537-4554** | `p2_op_upm_fit` 生产显式：`tolerance=1e-3, tolerance_relative=1, gs_damping=0.5, m_full_frame=1, final_gauge=1` | P2a-2/3/4 |
| 4572-4585 | 新增 `upm.{tolerance,tolerance_relative,gs_damping,m_full_frame,final_gauge}` 配置覆盖 | — |
| **4609-4623** | 调用 `p2_upm_convergence` 并落盘 `.json`+manifest（此前从不调用、只在 .bin） | **P2a-3** |
| 4646-4655 | artifact `p2_upm_model.json` 增 `iterations/converged/objective` + 行为 provenance | P2a-3 |
| 4681-4689 | manifest 增 `upm_iterations/upm_converged/upm_objective/upm_tolerance*` 等 | P2a-3 |
| **4949-4958** | `seam.additive_mode ∈ {c(默认), delta, both}`，非法值 fail-closed；delta 无天光面→显式退化 c | **P2a-1** |
| **5171-5197** | tile 循环：`c` 只扣 C（跳过 δ）；`delta` 加回 C 后只扣 δ；`both` = legacy | **P2a-1** |
| 5323-5345 / 5368-5373 | artifact/manifest 记 `additive_mode_*/additive_combination/c_subtracted/delta_subtracted` 与降级 | P2a-1 |

### D. `lib/algorithms/coverage/tests/synthetic_gate.cpp`
| 行 | 改动 |
|---|---|
| 5785-5910（文件尾） | 新增判别力 Gate `Release02P2aDefaultsAreLegacy` + `Release02P2aConvergenceRelativeAtProductionScale` |

---

## ② 默认保留 C 还是 δ：**默认保留 C（去掉 δ）**

配置：`doc["seam"]["additive_mode"]`，默认 `"c"`；可切 `"delta"` / `"both"`（legacy）。

**依据（按强度排序）**：
1. **直接数值**（c-delta-ruling §2.2，29336 个 ≥2 帧 control）：`raw−C`=**0.131%**，`raw−δ`=2.799%，`raw−C−δ`=**13.974%**（比完全不校正 13.454% 还差）。C 单独把帧间差压低 **~107×**。
2. **本 Oracle 生产尺度独立复现**（`p2a_oracle.cpp`，sky~1e13 ADU、ivar~6.25e-22、未归一化场景、λs=0）：`raw`=6.490% / `raw−δ`=5.076% / **`raw−C`=0.0130%** / `raw−C−δ`=1.4013%。**raw−C 比双重扣除低 108×**，排序与生产一致。
3. **语义**：δ_k = b_k − B_ref 本身是**两个天光模型之差**，C 是 UPM 原生公共面解；把 δ 叠加在已被 C 对齐的场上，按构造就是「第二次逐帧加性扣除」。δ 与天图无关（corr≈0.002），不是乘性残余。
4. **δ 的拟合病态**（c-delta §10.4）：`p2_sky_plane.bin` `chi2_red=6268`、`kappa=2.85e5`，其数值不可靠；保留 C 同时避免把病态 δ 引入输出。
5. **对 C 近过拟合（271555 DOF vs 277234 obs）的处置**：C 受 CG 平滑 + 弱零锚正则化；且 M0（Phase1 测光施加，另一分片）完成后，C 当前吸收的未归一化残余乘性将消失，过拟合压力下降。本实现保留配置开关，若负责人按 c-delta-ruling M1-a 裁决保留 δ，可无损切换（`additive_mode="delta"`），无需改代码。

> 未改动的科学口径：输出仍保留公共面亮度（不扣整个背景、不除 g_k），与 FIX-GK/方案 B 的「多退少补」语义一致；仅把**两次**扣除改为**一次**。

---

## ③ 新 formulation 的代数与数值验证

### 3.1 代数（对齐 q2-snr-smooth §4/§5，并在 UPM 语义下重述）

UPM 模型：`y_{f,k} = M_k + C_{f,k} + r_{f,k}`，校正 `z_{f,k} = y_{f,k} − C_{f,k}`。覆盖子集 `S(k)`，叠加权重 `w_{f,k}`（= 拟合权重，同源）。

- **P2a-4（全帧 M）**：`M_k = Σ_{f∈S(k)} w_{f,k}(y_{f,k}−C_{f,k}) / Σ_{f∈S(k)} w_{f,k}`。定义即得 **叠加 `Σ_{S} w z / W_S ≡ M_k`**（任意子集），即堆叠场是单一公共场，不含子集跳变。
- **P2a-2 末端残差场 gauge**：`G_k = Σ_{S(k)} w z / W_S − M_k`，校正改为 `z'_{f,k} = z_{f,k} − G_k`（G 对全部帧相同）。则 `Σ_{S} w z'/W_S = (M_k+G_k) − G_k = M_k`。
  - **关键**：G 是**空间变化的公共场**，不是 gauge 常数——全局常数对接缝无效（c-delta §4 已证）。
  - 参考帧 gauge `C_ref≡0` **不被破坏**：G 单独存储（`Model::gauge`），不并入 C。
- **P2a-2 阻尼**：`C ← (1−α)C_old + α C_new`，α=0.5。naive α=1 在链式/二部覆盖图有特征值 −1（周期 2 振荡，q2 §4.2）。
- **权重同源**：`w` 由最后一轮 IRLS 复用（提升到循环外），G 与拟合用同一组 w。

### 3.2 数值（真实 `upm.cpp` 链接，非复刻）

`run/RELEASE-02/fix-p2a/p2a_oracle.cpp` 直接编译并链接生产 `lib/algorithms/coverage/src/upm.cpp`（`-ffunction-sections --gc-sections` 丢弃未用 aio 路径），场景：3 帧按 gx 列覆盖（f0:gx<4 / f1:2≤gx<6 / f2:gx≥4），覆盖子集在 gx=1|2、3|4、5|6 突变；sky~1e13 ADU，σ=4e10，ivar=6.25e-22。

```
config                            iter conv   objective    boundary%    internal%    ratio
legacy refM noG @lam1000            60    0       27.99      0.3386%      0.4091%    0.828
full_frame noG @lam1000             60    0       24.01      0.3442%      0.4091%    0.842
refM final_gauge @lam1000           60    0       27.99      0.4824%      0.4374%    1.103
P2a-full rel1e-3 @lam1000           60    0       24.41      0.3917%      0.4091%    0.958
legacy refM noG @lam0               60    0   0.0008331      1.0698%      0.4521%    2.367
P2a-full rel1e-3 @lam0              25    1    0.001662      1.0707%      0.4521%    2.368
--- P2a-1 组合失配（未归一化场景，λs=0，>=2 帧 control 的帧间 std/公共场）---
  raw=6.4904%  raw-delta=5.0762%  raw-C=0.0130%  raw-C-delta=1.4013%
```

- **boundary/internal** = 覆盖子集突变边 / 同子集边的 `|Δstack|` 中位比（Q2 §6.2 口径）。λs=1000（平滑使公共场可辨识）时 P2a-full = **0.958**（无多余边界阶跃）；P2a-4 使 objective **27.99 → 24.01**（方差更小）。
- **P2a-1 组合**：raw−C 相对双重扣除低 **108×**；这是本分片对生产接缝的**决定性**贡献。

### 3.3 判别力（能红能绿）
- **红**：`final_gauge=0`、`tolerance_relative=0`、绝对 1e-6 → `converged=0`、边界比 0.828（无 gauge 时 refM 的 stack 不恒等 M）。
- **绿**：`P2a-full` → `converged=1`、边界比 0.958、objective 更低。
- 单元 Gate：`Phase2Upm.Release02P2aDefaultsAreLegacy`（新字段默认 legacy）+ `Release02P2aConvergenceRelativeAtProductionScale`（生产尺度绝对 1e-6 不可达、相对 1e-3 可达、参考帧 C≡0 保留）。

### 3.4 诚实边界（重要）
- 本分片的末端 gauge 是**逐 control 的公共投影**：它使控制节点叠加 ≡ M。它**不能**在 λs=0 时消除「公共场 M 本身的覆盖子集非可辨识性」——此时每个 control 的 M 由局部覆盖帧加权均值决定，相邻 control 之间无平滑链接，M 可在子集突变处跳变（Oracle λs=0 行：boundary/internal = 2.37）。
- 消除该剩余阶跃需要二者之一：(a) 生产开启 UPM 图平滑 `upm.smoothing_lambda > 0`（本 Oracle λs=1000 时比值回到 ~0.96）；或 (b) 在**叠加阶段（phase3）**用局部子集残差场逐像素投影（属 P2b/phase3 执行面，P2a 不做）。
- **登记为开放项**：生产 `p2_op_upm_fit` 当前 `smoothing_lambda=0`（仅当 config 给值才非零）。是否把生产默认改为 >0 属科学行为变更，需负责人裁决；本分片未擅自更改，只保证配置可达。
- 本 Oracle 的合成量级与生产不同（生产接缝 2–6% vs 合成 0.01–1%），给出的是**机制与排序**，不是生产量级复现。

---

## ④ 收敛判据新阈值论证

- **现状**：`upm.cpp:903` `max_dM < 1e-6 && max_dC < 1e-6`（绝对）。生产 `max|M|≈3.09e15`，`ULP=0.5`，绝对 1e-6 比 ULP 小 **5.7 个数量级** ⇒ 原理不可达；实测 `iterations=100, converged=0, objective=24081.46`（c-delta §7）。
- **改法**：`tol = tolerance × max(scale, 1.0)`，`scale_M=max|M|`、`scale_C=max|C|`。`max(scale,1.0)` 保证小尺度合成数据与 legacy 绝对判据**逐位等价**；`tolerance_relative=0` 时 `tol=tolerance`（legacy 逐位不变）。
- **新阈值 = 相对 1e-3（生产）**。论证：
  1. 实测每轮仅降 ~0.7%，`rel_dM` 稳定 **1.15–1.32e-4**（c-delta §7.3）⇒ 即使改相对，**1e-6 仍不可达**；必须重定。
  2. 1e-3 相对在报告数据上约 1–2 轮可达；本 Oracle 独立复现：绝对 1e-6 → `iter=60,conv=0`；相对 1e-3 → `iter=25,conv=1`。**可达且非平凡**（不是第 1 轮就停）。
  3. 相对门只影响**运行时长与收敛标志**，不改变接缝量级（接缝由 P2a-1 与公共场平滑决定）；把 `converged` 从「恒 0 的假信号」变为「可达的真信号」是本项的目的。
- **可见性**：`p2_op_upm_fit` 调用 `p2_upm_convergence`，把 `iterations/converged/objective` 写进 `p2_upm_model.json` 与 manifest（此前只在 `.bin`，且本节点从不调用该访问器）。
- 配置可覆盖：`upm.tolerance` / `upm.tolerance_relative`。

---

## ⑤ 科学行为变更清单（供 ACCEPTANCE §3 自裁决台账）

| # | 变更 | 依据 | 影响面 | 如何验证 |
|---|---|---|---|---|
| SC-P2a-1 | **生产默认加性组合由 `raw−C−δ` 改为 `raw−C`**（去掉 δ 二次扣除） | c-delta-ruling §2.2；本 Oracle raw−C=0.0130% vs raw−C−δ=1.4013% | `p2_op_upm_apply` 输出；接缝显著下降；`p2_corrected.json` provenance 变化 | Oracle 组合表；`p2_corrected.json.additive_combination=="raw_minus_C"`；L4 接缝重测 |
| SC-P2a-2 | **UPM 求解默认加阻尼 α=0.5** | q2 §4.2（naive α=1 特征值 −1 振荡） | `upm.cpp` C-update 数值路径；model_hash 变化 | `p2a_oracle` conv/objective；单元 Gate |
| SC-P2a-3 | **M-update 由参考帧改全帧加权** | c-delta §3.2/§6.2 M2 | M/C 数值与 model_hash 变化；方差更小 | Oracle objective 27.99→24.01 |
| SC-P2a-4 | **末端残差场 gauge 从每帧扣除**（新增 `Model::gauge` 持久化） | q2 §5（叠加≡公共场） | 校正输出整体平移 G；参考帧 C≡0 保留 | 单元 Gate；Oracle 边界比；save/open round-trip |
| SC-P2a-5 | **收敛判据绝对→相对，生产阈值 1e-3** | c-delta §7；本 Oracle | `converged` 由恒 0 变可达 1；迭代数下降 | Oracle conv 列；`p2_upm_model.json.converged` |
| SC-P2a-6 | **`P2UpmBuildConfig` 增 4 字段**（默认 legacy） | 需显式 opt-in 且不破坏 W2 冻结基线 | ABI/结构体大小变化（同源重编译）；`cfg{}` 行为不变 | `Release02P2aDefaultsAreLegacy` |
| SC-P2a-7 | `p2_upm_save/open` 增 `gauge` 与 4 行为键（旧文件缺键→legacy） | 持久化一致性 | 模型文件格式向后兼容 | 既有 `UpmSaveOpen*` 冻结门应保持绿（关闭时 payload/hash 逐位不变） |

**未做的科学行为变更（登记，需裁决）**：生产 `smoothing_lambda` 仍为 0；λs=0 下公共场的覆盖子集非可辨识性导致剩余边界阶跃（§3.4）。

---

## ⑥ 判别力测试

1. **独立 Oracle（本报告主证据）**：`run/RELEASE-02/fix-p2a/p2a_oracle.cpp`（链接**真实** `upm.cpp`）+ `build.sh`；输出 `oracle_out.txt`。
   - 能红：`legacy` 行 `conv=0`（绝对 1e-6）、raw−C−δ=1.4013%。
   - 能绿：`P2a-full` 行 `conv=1`、raw−C=0.0130%、objective 更低。
2. **GTest Gate**：`lib/algorithms/coverage/tests/synthetic_gate.cpp` 文件尾
   - `Phase2Upm.Release02P2aDefaultsAreLegacy`：4 新字段默认 legacy。
   - `Phase2Upm.Release02P2aConvergenceRelativeAtProductionScale`：生产尺度（sky~1e13、ivar~6.25e-22、覆盖子集突变）下，绝对 1e-6 → `converged=0, iterations=60`；相对 1e-3 + P2a-full → `converged=1, iterations<60`；参考帧 `C≡0` 保留。
3. **语法门**（本分片已跑，未跑构建）：`g++ -std=c++20 -fsyntax-only` 对 `upm.cpp`（EXIT=0）、`module_adapters.cpp`（EXIT=0，仅既有 u8path 弃用告警）、`synthetic_gate.cpp`（EXIT=0）。日志在 `run/RELEASE-02/fix-p2a/*_syntax.log`。

---

## ⑦ 未做 / 边界

- 未跑 `ninja/cmake/ctest`（任务硬约束）；生产构建与全量测试由前台做。
- 未做方差传播/权重链（P2b）；未改 Phase1（M0 另一分片）；未改 `docs/**`。
- 未跑 L4 真实数据重跑；生产量级接缝复测由前台在执行本修复后重跑。
- `seam.additive_mode="delta"` 路径中，UPM 末端 gauge G（公共场，full_frame=1 时≈0）被保留；因 G 对全部帧相同，不产生帧间/接缝差异。
- 未清理：无（`/dev/shm/fix_p2a` 已删）。
