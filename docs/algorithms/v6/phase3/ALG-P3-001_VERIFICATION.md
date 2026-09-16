> **DOC-001 溯源注记（2026-09-16）**：本文为 V6 产品族冻结/设计档案（上一轮治理产物），因仍被活动合同引用而保留在活动索引；文中 工程控制/旧 V6 控制包（ROOT-007 已删除）/** 等旧控制包路径为该轮任务溯源，该控制包已由 ROOT-007 删除，不作现状引用。

# ALG-P3-001 — 验证、独立 Oracle 与负向 mutation 证据

> 文档 ID：`ALG-P3-001-VERIFICATION`
> 机器证据：`run/v6/alg-p3/data/`（`oracle_results.json`、`kernel_oracle.json`、`oracle_mutations.json`、
> `gate_mutations.json`、`spec_check.json`、`spec_mutations.json`、`doc_check.json`、`doc_mutations.json`）
> 日志：`run/v6/alg-p3/logs/`；工具：`run/v6/alg-p3/tools/`
> 上位：`CONSTITUTION` §13.1/§14.4；`PROJECT_SPEC` §8；`UNIFIED` §10；`DESIGN-P3-001` §7；`SCI-P3-001-REVIEW` §8。

## 0. 基线与纪律

| 项 | 值 |
|---|---|
| 任务开工 HEAD | `125bc0999363be1a42a1f2df3254601e0cc7b8fb`（main） |
| 证据采集 HEAD | `29747831f45faa98332960049307ca74a38420f3`（控制器已集成 5 个并行 W3 姊妹任务：ALG-P2-PSFSW/POINT、DATA-DESIGN、ALG-P2-SURF、ALG-P1；均由控制器提交，非本任务） |
| 分支 | main |
| F1（工作树≠HEAD） | 未裁决，**只登记不裁决**（`CTRL-F1`；C-004.6）。本任务只读，不据此判 PASS/FAIL |
| 写域 | `docs/algorithms/v6/phase3/`（tracked）；`run/v6/alg-p3/`（证据工作区） |
| git 写操作 | 无（未 commit/push/add/branch/worktree/stash/reset/clean/rebase） |
| 子代理 | 未派生 |

## 1. 方法与独立性

- 数值 Oracle（`tools/alg_p3_oracle.py`）与合同门（`tools/alg_p3_gate.py`）、规格门（`tools/alg_p3_spec_check.py`）、
  文档一致性门（`tools/alg_p3_doc_check.py`）均为审查侧独立构造，**不 import / 不链接 / 不执行任何生产实现**
  （不调用 `lib/phase3_session`、`lib/algorithms/projection`、`lib/algorithms/resample`）。
- 真值来源：解析式（面积重叠、线性插值误差界、球面盈余面积）与固定种子 Monte Carlo（`SEED=20260915`、60000 draws）。
- 外部独立实现交叉：投影面积元以自实现球面四边形面积与 Paper II 解析纬度带公式二次校验（交叉残差 `1.27e-6`，
  为球面四边形大圆边与纬度小圆边曲率差）。
- 无零用例、无 skip-only：所有 check 均执行并断言。

## 2. 命令与实测退出码

| # | 命令（仓库根执行） | rc | 结果 |
|---|---|---|---|
| 1 | `python3 run/v6/alg-p3/tools/alg_p3_oracle.py run` | **0** | 独立数值 Oracle 10/10 PASS |
| 2 | `python3 run/v6/alg-p3/tools/alg_p3_oracle.py mutate-all` | **0** | 12/12 mutation 全检出；逐条单跑 rc=1 |
| 3 | `python3 run/v6/alg-p3/tools/alg_p3_gate.py positive` | **0** | 3/3 模式正向控制 ACCEPT |
| 4 | `python3 run/v6/alg-p3/tools/alg_p3_gate.py mutate-all` | **0** | 28/28 mutation 全检出；逐条单跑 rc=1 |
| 5 | `python3 run/v6/alg-p3/tools/alg_p3_spec_check.py check` | **0** | 机器规格结构门通过 |
| 6 | `python3 run/v6/alg-p3/tools/alg_p3_spec_check.py mutate-all` | **0** | 16/16 mutation 全检出 |
| 7 | `python3 run/v6/alg-p3/tools/alg_p3_doc_check.py check` | **0** | 人读↔机器规格一致性通过 |
| 8 | `python3 run/v6/alg-p3/tools/alg_p3_doc_check.py mutate-all` | **0** | 必需 token 删除 mutation 全检出 |

复跑入口（一键）：`bash run/v6/alg-p3/run_all.sh`（返回单一 rc）。

## 3. 独立 Oracle（O-P3-01..12）

| check | id | 断言 | 实测 |
|---|---|---|---|
| `sb_row_normalisation` | O-P3-01 | `Σ_j R_ij = 1`（常量面亮度不变量） | 行和误差 **0**；`max|R·1−1|=1.11e-16` |
| `flux_col_normalisation` | O-P3-02 | `Σ_i S_ij = 1`；点源总通量守恒；`Σpi=1` | 通量相对误差 **5.10e-12**；`Σpi=0.9999999999949` |
| `variance_diagonal_input` | O-P3-03 | 对角输入 `var_out=Σ c_k² u_k=diag(R C Rᵀ)` | 最大相对误差 **0.0** |
| `variance_input_correlation` | O-P3-04 | `rho>0` 时对角省略的亏损 | 亏损中位数 **23.31%**（rho=0.19）；完整式 vs MC **0.387%** |
| `covariance_completeness` | O-P3-05 | 完整 `C_y` 的 `W=piᵀ C_y⁻¹ pi` 与 MC 一致；对角化过度乐观 | 完整 rel err **0.0**；对角化声明 21.71 < 真实 30.45（**28.71%** 过度乐观） |
| `effective_psf` | O-P3-06 | `pi=S p`、`Σpi=1`、注入源恢复；δ 近似偏差 | 通量偏差 **2.74e-5**；方差 MC rel **0.683%**；δ 偏差 **94.77%** |
| `qw_output_frame_recompute` | O-P3-07 | 输出帧重算 Q/W vs 重采样输入 Q/W 不可交换 | `W_out=0.033248` vs `W_naive=0.002740`，**91.76%**；重算 vs MC **0.715%**；naive vs MC **91.70%** |
| `constant_sb_field` | O-P3-08 | 常量 SB 输入 `y=B0` | 偏差 **4.44e-16** |
| `kernel_bilinear_4quad` | O-P3-09 | `bilinear_4quad` 误差界 + 权重归一 + 边界 NaN | 误差 **0.0273955** ≤ 上界 **0.0277778**（比 0.9862）；`Σw` 偏差 0；边界 NaN ok；零填误差 1.3223 |
| `projection_solid_angle` | O-P3-10 | 逐像素 `Omega` 非常数 | CAR `max/min=2.0000`；TAN(dec60) `1.445`；小场 `1.00015`；自实现↔解析交叉 `1.27e-6` |
| 合同门 3 模式正向控制 | O-P3-11 | 三模式 fail-closed 记录被 ACCEPT | 3/3 rc=0 |
| provenance 最小集 | O-P3-12 | 记录 provenance 键完整 | `G-P3-PROV-01` 正向无违规 |

## 4. 负向 mutation 矩阵

### 4.1 数值 Oracle mutation（12/12 检出，逐条 rc=1）

| mutation | 注入 | 结果 |
|---|---|---|
| `sb_row_normalisation` | 未归一重叠当 SB 算子 | 行和误差 0.5 → 红 |
| `flux_col_normalisation` | 用 R 做通量算子 | 通量相对误差 0.556 → 红 |
| `variance_diagonal_input` | `Σ S_ij u_j` 线性平均 | 相对误差 0.44 → 红 |
| `variance_input_correlation` | 省略交叉项（对角公式） | vs MC 23.24% → 红 |
| `covariance_completeness` | 只保留对角 `C_y` | 声明 21.71 < 真实 30.45 → 红 |
| `effective_psf` | δ 近似替代 `pi=S p` | 通量偏差 94.77% → 红 |
| `qw_output_frame_recompute` | 用重采样输入 W | vs MC 91.70% → 红 |
| `constant_sb_field` | 未归一算子 | 偏差 4.63 → 红 |
| `kernel_bilinear_4quad` | 最近邻替代双线性 | 5.11× 上界 → 红 |
| `kernel_bilinear_4quad__unnormalised` | 权重不归一 | 42.78× 上界 → 红 |
| `kernel_bilinear_4quad__zero_fill` | 缺 tile 零填 | 零填误差 1.3223 → 红 |
| `projection_solid_angle` | 常数 `Omega` | 与解析不符 → 红 |

### 4.2 合同门 mutation（28/28 检出，逐条 rc=1）

12 条模式门 + kernel/covariance/QW/epsf/fits/provenance 扩展门：

| 门 | mutation | 命中规则 |
|---|---|---|
| `G-P3-SB-01` | `M-P3-SB-01` | flux 换算无 Omega |
| `G-P3-SB-02` | `M-P3-SB-02` | 测量却 uncertainty unavailable 无原因 |
| `G-P3-SB-03` | `M-P3-SB-03` | variance BUNIT ≠ signal² |
| `G-P3-SB-04` | `M-P3-SB-04` | 对角无相关核 |
| `G-P3-PSF-01` | `M-P3-PSF-01` | 缺 PSF |
| `G-P3-PSF-02` | `M-P3-PSF-02` | PSF 未归一 |
| `G-P3-PSF-03` | `M-P3-PSF-03` | 缺 point_information 且不可重建 |
| `G-P3-PSF-04` | `M-P3-PSF-04` | 仅对角无相关核 |
| `G-P3-PSF-05` | `M-P3-PSF-05` | 缺光度尺度 a |
| `G-P3-PSF-06` | `M-P3-PSF-06` | 未输出 effective PSF |
| `G-P3-VIS-01` | `M-P3-VIS-01` | visualization measurement_capable=true |
| `G-P3-GLB-01` | `M-P3-GLB-01` | variance_from=psfsw_robust_weight |
| `G-P3-KRN-01` | `M-P3-KRN-01` | 未注册核进生产 |
| `G-P3-KRN-02` | `M-P3-KRN-02` | nearest 作科学默认 |
| `G-P3-KRN-03` | `M-P3-KRN-03` | bilinear_4quad 注册缺 Oracle |
| `G-P3-COV-01` | `M-P3-COV-01` | 对角无相关核 |
| `G-P3-COV-02` | `M-P3-COV-02` | variance_from=weight |
| `G-P3-COV-03` | `M-P3-COV-03` | 上游相对权重写成 ivar |
| `G-P3-QW-01` | `M-P3-QW-01` | 重采样输入 Q/W |
| `G-P3-QW-02` | `M-P3-QW-02` | W=Σ 输入 W |
| `G-P3-QW-03` | `M-P3-QW-03` | 上游 W_info 重算/替换 |
| `G-P3-QW-04` | `M-P3-QW-04` | 对角化过度乐观未检出 |
| `G-P3-EPSF-01` | `M-P3-EPSF-01` | 只给 FWHM 标量 |
| `G-P3-FITS-01` | `M-P3-FITS-01` | 失败留可见半成品 |
| `G-P3-FITS-02` | `M-P3-FITS-02` | 缺 DATASUM/CHECKSUM |
| `G-P3-FITS-03` | `M-P3-FITS-03` | 整幅常驻内存 |
| `G-P3-FITS-04` | `M-P3-FITS-04` | 缺 tile 零填 |
| `G-P3-PROV-01` | `M-P3-PROV-01` | provenance 缺键 |

### 4.3 规格结构 mutation（16/16 检出）

`S1-remove-qw-freeze`、`S2-bilinear-no-oracle`、`S3-deferred-in-production`、`S4-break-unit-quadratic`、
`S5-remove-failclosed-gate`、`S6-diagnostic-in-requires`、`S7-remove-clause-qw`、`S8-remove-provenance-key`、
`S9-R-not-row-normalised`、`S10-remove-f3-disposition`、`S11-nearest-as-science-default`、`S12-epsf-not-required`、
`S13-gate-without-mutation`、`S14-remove-ctrl-registration`、`S15-bilinear-missing-boundary`、`S16-psfsw-as-ivar-unit`。

### 4.4 人读↔机器规格 mutation（必需 token 删除，全检出）

`alg_p3_doc_check.py mutate-all` 对每个必需 token（条款 id、门 id、mutation id、kernel id、formula id、
冻结 id、关键实测数值）删除其一，检查器必须报红。

## 5. 关键量化（独立 Oracle 实测）

| 量 | 值 | 用途 |
|---|---|---|
| 对角省略方差亏损中位数（rho=0.19） | **23.31%** | `G-P3-COV-01` 门 |
| 对角化声明方差过度乐观 | **28.71%**（21.71 vs 30.45） | `G-P3-QW-04` 门 |
| 重采样输入 W 相对差 | **91.76%** | `G-P3-QW-01` 门 |
| δ-PSF 通量偏差 | **94.77%** | `G-P3-EPSF-01`/`G-P3-PSF-06` 门 |
| CAR `Omega` 极差（±60° 视场） | **2.0000** | 逐像素 Omega 必需 |
| 通量守恒相对误差 | **5.10e-12** | `R`/`S` 语义分离 |
| `bilinear_4quad` 误差/上界 | **0.9862** | kernel 注册证据 |

## 6. 公式索引（机器规格 formula id ↔ 人读条款）

| formula id | 内容 | 条款 |
|---|---|---|
| `ALG-P3-F-COV` | `C_y=R C_x Rᵀ`、`Var(y_i)`、`rho_ij` | ALG-P3-004 |
| `ALG-P3-F-DIAGDOMAIN` | `Σc²u` 严格域=对角输入 | ALG-P3-005 |
| `ALG-P3-F-EPSF` | `pi=S p`、`Σpi=1`、FWHM 从 pi 测 | ALG-P3-007 |
| `ALG-P3-F-QW` | `Q=a piᵀC_y⁻¹f`、`W=a² piᵀC_y⁻¹pi`、`F_hat=Q/W` | ALG-P3-006 |
| `ALG-P3-F-QWNONCOMMUTE` | `R` 与匹配滤波不可交换 | ALG-P3-006 |
| `ALG-P3-F-UPSTREAMW` | 上游 `W_info` 消费不重算不替换 | ALG-P3-006 |
| `ALG-P3-F-BUNIT` | `var_out` BUNIT=(signal BUNIT)² | ALG-P3-009 |
| `ALG-P3-F-NOFILL` | 缺 tile/非有限输出 NaN | ALG-P3-008 |
| `ALG-P3-F-STREAMMEM` | 行带流式内存界 | ALG-P3-008 |
| `ALG-P3-F-ATOMIC` | 临时文件+fsync+CHECKSUM+rename+重开验证 | ALG-P3-008 |

采样核 registry 条款为 ALG-P3-003（见 `ALG-P3-001_KERNEL_REGISTRY.md`）；治理登记条款为 ALG-P3-013（见 SPEC §11）。

## 7. 建议状态

- 本任务建议状态：**PASS**。
- 依据：任务正文逐项完成（三模式采样 / `C_y=R C_x Rᵀ` / Q/W 输出帧重算 / effective PSF 传播 / 流式原子 FITS /
  采样核 registry 与 bilinear_4quad 独立 Oracle / 独立 Oracle 与负向门）；独立 Oracle 正向 rc=0、合同门正向 rc=0、
  规格门 rc=0、文档一致性门 rc=0；负向 mutation oracle 12/12、gate 28/28、spec 16/16、doc 全检出且逐条 rc=1；
  未越界写、未改冻结门/容差、未 commit/push、未派生子代理。

## 8. 局限（如实）

1. Oracle 用 1D/2D 合成算子与人工构造协方差验证数学恒等；真实数据的注入源与 M42/银心验证属 W10 `REAL-SCIENCE-001`。
2. 结构门是静态 JSON/文本证据，不是生产运行时可验证性证明；运行时可达性属 `RUNTIME-CI-001`（W9）。
3. 相关核近似误差阈值 `epsilon_corr` 的**具体数值**未在本任务擅自冻结（`SO-07`：由 `CONTRACT-FREEZE-001`/负责人确认）。
4. F1（工作树≠HEAD）与 AR-033（根构建面 owner）为控制器级事项，本任务只登记（`CTRL-F1`/`CTRL-AR033`）。
5. 词表归一归 W6 `SCHEMA-INTEGRATE-001`（C-004.3）；本任务不写 schema 实现。

## 9. 需控制器/负责人裁决事项

| 事项 | owner |
|---|---|
| `SO-07` Phase3 相关核近似误差阈值数值 | `CONTRACT-FREEZE-001` + 负责人 |
| `CTRL-F1` 基线分歧 | 控制器 |
| `CTRL-AR033` 根构建面 owner | 控制器 |
| `CTRL-AR034`/`CTRL-AR035`/`CTRL-AR036` | 控制器/负责人 |
| F3-05 FROZEN SCI 正文修正 | `SCI-ADJ-001`/`DOC-CONVERGE-001`（须负责人签字，不在本任务） |
