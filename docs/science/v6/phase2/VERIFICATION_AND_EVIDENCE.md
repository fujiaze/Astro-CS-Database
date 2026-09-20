> **⚠ 已按 §9.73 A44 作废**：本文件属历史/冻结层。其中「权重模式 / 权重档位 / mode0·mode1·mode2」这一整套概念**不存在**（负责人 2026-09-20 裁决，GAP_AUDIT.md §9.73 A44；ASTROCS_DESIGN.md §2.1）。本文件内容**保持历史原样**、仅作留痕，**不构成现行规范**；权重 = 阶段二按该天球像素对应帧集合**现场算出的派生量**。

> **DOC-001 溯源注记（2026-09-16）**：本文为 V6 产品族冻结/设计档案（上一轮治理产物），因仍被活动合同引用而保留在活动索引；文中 工程控制/旧 V6 控制包（ROOT-007 已删除）/** 等旧控制包路径为该轮任务溯源，该控制包已由 ROOT-007 删除，不作现状引用。

# SCI-P2-001 验证与证据（命令 / rc / 结果）

文档 ID：`SCI-P2-001-VERIFICATION`
机器摘要：`run/v6/sci-p2/summary.json`；日志：`run/v6/sci-p2/logs/`

## 1. 基线与 F1 基线分歧（**未裁决**，必须显式标注）

| 项 | 值 | 证据 |
|---|---|---|
| `git rev-parse HEAD` | `4b508f28bbcada66c417a8a9324aca7eb92869ff` | `logs/00_baseline.log` |
| `git rev-parse main` | `4b508f28bbcada66c417a8a9324aca7eb92869ff` | 同上 |
| 分支 | `main` | 同上 |
| 工作树 dirty（porcelain 行数） | 173 | `summary.json.baseline.worktree_dirty_entries` |
| **F1 回退** | 16 tracked 回退到祖先 blob + 10 tracked 删除 | `run/v6/base/baseline_freeze.json.worktree_vs_head`（BASE-OWN-001） |

**基线分歧未裁决**：本任务一切生产面判定**以已提交 HEAD 为准**（结构探针统一走 `git show HEAD:<path>`），
不把回退态当已验证基线。示例：工作树 `lib/infrastructure/scheduler/src/module_adapters.cpp` 相对 HEAD 为 `+27 / −135` 行
（`git diff --stat HEAD`），故该文件只按 HEAD 读取。

## 2. 命令与实测退出码

| # | 命令（在 `run/v6/sci-p2/` 下执行） | rc | 结果 |
|---|---|---|---|
| 1 | `python3 oracle/phase2_three_mode_oracle.py > logs/10_oracle.json 2> logs/10_oracle.err` | **0** | 独立 Oracle 32/32 PASS |
| 2 | `python3 oracle/probe_production_surface.py > logs/30_probe.json 2> logs/30_probe.err` | **0** | 结构探针 16/16 PASS |
| 3 | `python3 oracle/run_gate_and_mutations.py > logs/20_gate_mutations.json 2> logs/20_gate_mutations.err` | **0** | 5 正向控制 ACCEPT；17 门 mutation 全红；3 Oracle 自我 mutation 全红 |
| 4 | `bash oracle/run_all.sh > logs/00_run_all.log 2>&1` | **0** | 一键复跑（含 1/2/3 与 summary 生成） |
| 5 | `python3 oracle/weight_provenance_gate.py mutations/<record>.json` | 0 / 1 | 见 §5 |

复跑入口：`bash run/v6/sci-p2/oracle/run_all.sh`（单 rc）。

## 3. 独立 Oracle（32 项，rc=0）

独立性：纯 NumPy 从第一性原理实现，**不 import/link/exec 任何生产实现**；期望值为解析式或蒙特卡洛统计。

| 组 | check（关键词） | 实测结果 |
|---|---|---|
| C1 point_information | Q/W == GLS 解；Var=1/W == (AᵀC⁻¹A)⁻¹；实际系数 cᵀCc==1/W | 差 0.0 / 0.0 / 0.0 |
| C2 | 独立帧 Q=ΣQ_k、W=ΣW_k；白噪近似 W=a²/(σ²A_NEA)；SNR²=F_ref²W | 逐项相等 |
| C3 相关帧 | 联合 C 正定；ΣW 过度乐观须用联合 C | var_joint=3.541 vs var_naive=2.249，比值 **1.575** |
| C4 surface_gls | GLS 协方差 vs 蒙特卡洛（rel<3%）；退化 ivar；忽略 a_k 的 ivar 非最优；实际系数传播 | var_pred=0.7331，var_mc=0.7365，rel=**0.48%**；ivar 近似比值 **1.161** |
| C5 | G C Gᵀ == (AᵀC⁻¹A)⁻¹ | 最大绝对误差 **1.1e-16** |
| C6 effective PSF | 注入源==解析（equal/pixel_ivar/W_info）；proper-coadd MF PSF；median 输入 FWHM≠FWHM(P_eff)；FWHM(P_eff) 依赖权重 | FWHM：equal 3.805、pixel_ivar 5.460、W_info 5.248、MF 7.694、median 输入 4.756 |
| C7 psfsw_robust | 组内 median=1 且全正；四单变量方向；与 W_info 不成比例；方差从实际系数而非权重；MC 一致；fail-closed | 方向全部正确；var_from_coeffs=3.286 vs 权重代理 11.371；MC rel=**0.20%**；3 类 fail-closed |
| C8 诊断不可冒充 | 改星表亮度 median SNR×4 而 W_info 不变；support/coverage 不进 W；FWHM 不足定 W；residual 不自动等于 W；**五诊断量近同却 W 差 12.4%** | median SNR 19.72→78.87（×4，W 不变）；W_a=0.008815 vs W_b=0.0125；**diag 相对差 ≤1.36%，W 相对差 12.36%**；预测误差下界 5.13e-4 |
| C9 UPM | UPM 尺度不确定度进入 covariance | 含项/不含项比值 **2.0** |

## 4. 结构探针（16 项，rc=0，基线 = HEAD）

关键断言与实测：

| 断言 | 结果 |
|---|---|
| Phase2 生产源码/头文件无 point_information / surface_gls / psfsw_robust / psf_information_weight / psfsw_robust_weight / W_info / effective_psf 字面量 | 命中 = {}（全部为空） |
| `P2PixelResult` 只含 signal/support/n_used/计数，无 variance/covariance/effective PSF | 成立 |
| `module_adapters.cpp` 只接受 `weight_mode ∈ {1,2}` 并拒绝 0 | 成立（`"weight_mode must be integer (1=equal, 2=ivar)"` 命中） |（已按 §9.73 A44 作废：该概念不存在）
| 交换矩阵无 point_information / psfsw / effective_psf | 成立 |

结论：HEAD 三模式 **NOT_IMPLEMENTED**（与 `gap_baseline.md` §1 一致），实现责任在 Wave 5/8，本复核不据此判 FAIL。

## 5. 门与负向 mutation 明细

正向控制（`expected rc=0`）：PC1_point_information、PC2_surface_gls、PC3_psfsw_robust、PC4_equal、PC5_pixel_ivar → **5/5 rc=0**（门非空门）。

门负向 mutation（`expected rc≠0`）：M01–M17 → **17/17 rc=1**。
逐条注入与命中规则见 `WEIGHT_PROVENANCE_GATE.md` §3（含 median SNR、support/coverage、FWHM、residual 别名、psfsw→ivar、variance 反推、缺共同星集、FWHM-only PSF 等）。

Oracle 自我 mutation（`expected rc≠0 且指定 check 变红`）：

| id | 注入 | rc | 变红 check |
|---|---|---|---|
| OM1 | 组合系数 c 乘以 1.10（covariance 不从实际系数传播） | **1** | C1.3 |
| OM2 | 令 Pw 的 W 等于 Pn 的 W（诊断量可决定信息权重） | **1** | C8.5、C8.6 |
| OM3 | 跨帧相关 rho → 0（把相关帧当独立帧求和） | **1** | C3.1 |

## 6. 独立性与方法边界

- Oracle/gate/probe 均为审查侧独立构造，**不调用被测生产实现**，故不构成"同一实现自证"。
- 无零用例、无 skip-only：全部 check 均执行并断言（Oracle 32/32、探针 16/16、控制 5/5、mutation 17/17 与 3/3）。
- 负向门可红：om/mutation 的 rc 均为 1，已逐条记录（§5）。
- 本任务不改生产源码、不改科学公式/容差/冻结门、不 commit/push、不派生子代理。

## 7. 局限（如实）

- Oracle 的 PSFSW 指数 `(α,β,γ,δ)=(2,1,2,1)` 是**示例性可容许参数化**，冻结数值属 `ALG-P2-PSFSW-001`；
  本 Oracle 只验证对任意正指数成立的结构性质（归一、单调方向、非 ivar、fail-closed）。
- Oracle 用 1D PSF 与人工构造协方差验证数学恒等；真实数据的注入源与 M42/银心验证属 Wave 10 `REAL-SCIENCE-001`。
- 结构探针是静态字面量/字段证据，不是运行时可达性证明；运行时可验证性属 `RUNTIME-CI-001`。
