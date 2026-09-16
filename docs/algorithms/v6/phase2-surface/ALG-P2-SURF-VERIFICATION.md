> **DOC-001 溯源注记（2026-09-16）**：本文为 V6 产品族冻结/设计档案（上一轮治理产物），因仍被活动合同引用而保留在活动索引；文中 工程控制/旧 V6 控制包（ROOT-007 已删除）/** 等旧控制包路径为该轮任务溯源，该控制包已由 ROOT-007 删除，不作现状引用。

# ALG-P2-SURF-001 验证与证据（命令 / rc / Oracle / mutation）

- 文档 ID：`ALG-P2-SURF-VERIFICATION`（`ALG-P2-SURF-001` 子文档）
- 机器摘要：`run/v6/alg-p2-surf/summary.json`；日志：`run/v6/alg-p2-surf/logs/`
- 本任务条款：`ALG-P2S-VER.1..4`

## 1 基线与 F1 基线分歧（`ALG-P2S-VER.1`）

| 项 | 值 | 证据 |
|---|---|---|
| 派发基线 `HEAD` | `125bc0999363be1a42a1f2df3254601e0cc7b8fb`（main） | 任务开始时 `git rev-parse HEAD` |
| 取证时 `HEAD` | 见 `summary.json.baseline.head`（控制器在我工作期间集成了 W3 兄弟任务：`1c4e5f92` ALG-P2-PSFSW-001、`28e0ac6c` ALG-P2-POINT-001） | `run/v6/alg-p2-surf/logs/00_run_all.log` |
| 分支 | `main` | 同上 |
| 工作树 tracked dirty | 37（27 M + 10 D），与 BASE-OWN-001 `dirty_inventory.csv` 逐路径一致 | `summary.json.scope.only_cur/only_base` 均为 `[]` |
| **F1 回退** | 16 tracked 回退 + 10 tracked 删除（`CTRL-F1`，**未裁决**） | `run/v6/base/baseline_freeze.json` |

**基线分歧未裁决**：本任务一切**生产面判定以已提交 HEAD 为准**，不把回退态当已验证基线。
`CTRL-F1` 与 `CTRL-AR033` 为控制器级事项，**只登记不裁决**（见 §6）。
本任务未修改任何 tracked 文件（`summary.json.scope.tracked_dirty_matches_baseline = true`）；所有新文件均在写域
`docs/algorithms/v6/phase2-surface/` 与 `run/v6/alg-p2-surf/`（`summary.json.scope.out_of_scope = []`）。

## 2 命令与实测退出码（`ALG-P2S-VER.2`）

一键复跑：`bash run/v6/alg-p2-surf/oracle/run_all.sh`（在仓库根）

| # | 命令 | 实测 rc | 结果 |
|---|---|---|---|
| 1 | `python3 oracle/surf_oracle.py > logs/10_oracle.json` | **0** | 独立 Oracle 46/46 PASS |
| 2 | `python3 oracle/check_spec.py > logs/20_spec_check.json` | **0** | 结构 + 文档/JSON 一致 12/12 PASS |
| 3 | `python3 oracle/run_gate_and_mutations.py > logs/30_gate_mutations.json` | **0** | 2 正向控制绿、19 负向 mutation 全红 |
| 4 | `python3 oracle/build_summary.py > logs/40_summary.log` | **0** | 汇总 + 写域检查 PASS，建议状态 `PASS` |
| 5 | `bash oracle/run_all.sh > logs/00_run_all.log 2>&1` | **0** | 一键复跑单一 rc |

日志中的 rc 行：`oracle rc=0`、`spec_check rc=0`、`gate_mutations rc=0`、`summary rc=0`、`run_all rc=0`。

## 3 独立 Oracle（46 项，rc=0）（`ALG-P2S-VER.3`）

独立性：`run/v6/alg-p2-surf/oracle/surf_oracle.py` 纯 NumPy 从第一性原理实现，**不 import/link/exec 任何生产实现**；期望值为解析式或 Monte Carlo。

| 组 | check | 实测 |
|---|---|---|
| G1 GLS | 实际系数 `R C R^T == (A^T C^-1 A)^-1`；2 参数非对角正规矩阵可检出；GLS 方差 vs MC（rel<3%）；无偏 | G1.1/G1.4/G1.2/G1.3 PASS |
| B pixel-ivar 门 | 同点+a 一致 `rho=1`；5% a 扰动在门内；忽略 a_k **有偏**；跨元素泄漏 **有偏** 而 GLS 无偏；对角权重忽略相关 `rho>1+eps`；门以 p95+硬上限双判；对角方差低估 `c^T C c`；200 随机设计 `rho>=1` | B1–B7 PASS |
| U UPM | 乘加分离恢复 `s/g/b`；秩 == `n_p+2F-2`；恒常 s 退化；单帧禁拟 g；κ 好坏门；`J C_theta J^T` vs MC（rel<5%）；k_corr 门；加性/乘法单独模型无法互替（chi2 膨胀） | U1–U8 PASS |
| R rejection | probability 可靠性 `|obs-mean(p)|<=0.10`；BSS>0.10；省略 UPM 项导致顶箱失校准；n<=2 underdetermined；4 类污染 recall>=0.8；probability 加权 != ivar | R1–R5 PASS |
| E covariance/effective PSF | 注入单位点源 == 解析 `P_eff`（<1e-12）；`median(FWHM_k) != FWHM(P_eff)`；`FWHM(P_eff)` 依赖权重；`C_out` vs MC；`1/W` 代理 != 实际系数方差；相关核可重建 `C_out` 且对角低估 | E1–E6 PASS |

关键实测数值（日志 `logs/10_oracle.json`）：
- GLS var_pred=0.0245xxx vs var_mc（rel<3%）；
- 相关+异方差下对角 ivar `ratio≈1.09 > 1.05`（门拒绝）；
- UPM κ（良态）`< 1e6`、退化 `>1e6`；`J C_theta J^T` vs MC rel<5%；
- 加性-only 与乘法-only 模型的 chi2 相对可分离模型 `>1e3×`；
- 注入 `P_eff` 最大偏差 `<1e-12`；`1/W` 代理与实际系数方差差 `>20%`。

## 4 结构一致性检查（12 项，rc=0）（`ALG-P2S-VER.3` 续）

`check_spec.py` 校验 `spec JSON` 与 tracked 文档、SCI-ADJ-001 冻结清单、单位律、禁止项：

| 规则 | 内容 | 结果 |
|---|---|---|
| R0/R1/R2 | JSON 可解析、必需键齐全、frozen_constants 结构良好且 id 唯一 | PASS |
| R3 | README 冻结常量表与 JSON 逐值（value+unit）一致 | PASS |
| R4 | 引用的继承冻结 id 全部在 `adjudications.json.freeze_table` 解析（无悬挂） | PASS |
| R5 | 生产模式排除 `psf_snr_power`/legacy | PASS |
| R6a | 禁用权重来源 token 覆盖 FREEZE_LIST §5.1 全集 | PASS |
| R6b | 内嵌门拒绝 `support`/`rejection_probability` 作权重来源，接受干净记录 | PASS |
| R7 | 单位律 signal=ADU/px²、var=ADU²/px⁴、ivar=px⁴/ADU²、psfsw=1 | PASS |
| R8 | 文档无占位符/替换字符 | PASS |
| R9 | 每份规格声明的条款 id 在文中出现 | PASS |
| R10 | 门常量冻结（ε=0.05、sup=0.20、κ=1e6、min_frames=2） | PASS |

## 5 负向 mutation（19/19 必红）（`ALG-P2S-VER.4`）

正向控制（期望 rc=0）：`PC-oracle`（Oracle 全绿）、`PC-spec`（结构门全绿）→ **2/2 rc=0**（证明门非空门）。

负向 mutation（期望 rc≠0）：**19/19 实测 rc=1**。

| id | 注入 | 命中规则 |
|---|---|---|
| M01-eps-loosened | JSON ε 0.05 → 0.30 | R3,R10 |
| M02-delete-sup | 删除 `FZ-AP2S-EPS-PIXIVAR-SUP` | R3,R10 |
| M03-kappa-loosened | κ_max 1e6 → 1e9 | R3,R10 |
| M04-minframes-1 | min_frames 2 → 1 | R3,R10 |
| M05-psfsnr-production | `psf_snr_power` 进入 production | R5 |
| M06-delete-gate-key | 删除 `pixel_ivar_gate` 键 | R1 |
| M07-forbid-remove-prob | 从禁用 token 移除 `rejection_probability` | R6b |
| M08-unit-break | signal 方差单位 → ADU²（破坏二次律） | R7 |
| M09-dangling-freeze | 引用 `FZ-NOT-A-REAL-ID` | R4 |
| M10-calib-loosened | 校准容差 0.10 → 0.50 | R3 |
| MD01-readme-eps | README 表 ε 0.05 → 0.08 | R3 |
| MD02-placeholder | README 注入占位符 | R8 |
| MD03-drop-clause | GLS 文档条款范围改为不存在 id | R9 |
| OM-ideal_coefficients | Oracle 用理想公式替代实际系数 | G1.1 |
| OM-diag_normal | Oracle 丢弃非对角正规项 | G1.4 |
| OM-drop_upm_term | Oracle 去掉 UPM 协方差项 | U6.1,U6.2 |
| OM-hard_probability | probability 强制 0/1 | R1.1,R1.2 |
| OM-median_fwhm | effective PSF 用 median 输入 FWHM | E2.1 |
| OM-kcorr_one | k_corr 忽略相关取 1.0 | U7.1 |

mutation 记录/副本落 `run/v6/alg-p2-surf/mutations/`。

## 6 未决风险与需裁决事项

### 6.1 需负责人签字项（只登记不擅改）
| ID | 事项 | 理由 | 权威 |
|---|---|---|---|
| `SO-07-SURF` | pixel-ivar ε（0.05/0.20）、UPM `kappa_max`/`rank_rtol`、rejection 校准阈值的正式冻结确认 | 数值阈值由 W3 冻结、W4/负责人确认（`SCI-ADJ-001 §8 SO-07` 同构） | 宪章 §1.2 / SCI-ADJ-001 §8 |

### 6.2 控制器级事项（只登记不裁决）
| ID | 事实 | 处置 |
|---|---|---|
| `CTRL-F1` | 工作树 ≠ HEAD（16 回退 + 10 删除），未裁决 | registered_only；本任务以 HEAD 为准，未改 tracked 文件 |
| `CTRL-AR033` | 根/公共 CMakeLists 无 V6 owner | registered_only；本任务不写构建面 |

### 6.3 开放项（域外，登记）
| ID | 事项 | owner |
|---|---|---|
| `OPEN-P2S-01` | point_source 产品规范输出形态（map vs statistic） | ALG-P2-POINT-001/控制器 |
| `OPEN-P2S-02` | UPM 乘法尺度 g_k 的生产数据面/schema 与空间模型表示 | DATA-DESIGN-001/SCHEMA-INTEGRATE-001 |
| `OPEN-P2S-03` | 跨帧完整联合 C 的低秩/相关核表示 | DATA-DESIGN-001/CONTRACT-FREEZE-001 |

### 6.4 局限（如实）
- Oracle 用合成 1D/小矩阵问题验证数学恒等与统计性质；真实数据注入源与 M42/银心验证属 `REAL-SCIENCE-001`（Wave 10）。
- 结构一致性检查是静态文档/JSON 检查，不是运行时可达性证明；运行时可验证性属 `RUNTIME-CI-001`（Wave 9）。
- UPM 乘法尺度 g_k 是**目标态新增**（现行 `PHASE2_UPM_IMPL.md` 仅加性），实现归 `IMPL-P2-UPM-001`（W5）；本任务不写生产代码。
- schema 词表归一归 `SCHEMA-INTEGRATE-001`（W6，`C-004.3`），本任务不发明第三套词表。
- ε/κ/校准阈值为本任务（W3）冻结的工程-科学判据，正式冻结须 `CONTRACT-FREEZE-001`（W4）并负责人确认；**未获确认前实现不得放宽**。

## 7 声明

- 未 commit/push/add/分支/worktree/stash/reset/clean/rebase；只读 git。
- 未越界写：仅写 `docs/algorithms/v6/phase2-surface/` 与 `run/v6/alg-p2-surf/`（`summary.json.scope.out_of_scope = []`）；未改任何 tracked 文件。
- 未派生任何子代理；未调用 subagent/subagent_fork/ralph。
- 未宣布发布；未改冻结门/容差；未替 `SO-01..07` 签字。
