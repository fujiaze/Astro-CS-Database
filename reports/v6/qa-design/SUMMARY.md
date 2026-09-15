# QA-MATRIX-001 交付摘要（独立科学 QA 矩阵）

- 文档 ID：`QA-MATRIX-001-SUMMARY`
- 任务：`工程控制/AstroCS_PARALLEL_SCIENCE_IMPLEMENTATION_V6_20260915/tasks/QA-MATRIX-001.md`（wave 3）
- write_scope：`docs/validation/v6/`、`reports/v6/qa-design/`（全部产物均在域内）
- 基线 HEAD：`125bc0999363be1a42a1f2df3254601e0cc7b8fb`（实测）
- **建议状态：PASS**（仅指本设计交付；不构成冻结/发布）

## 1. 交付物

| 文件 | 内容 |
|---|---|
| `docs/validation/v6/QA_MATRIX.md` | 44 条门的规格（判据/容差来源/零用例即红/mutation/IO/单位/适用域/fail-closed）+ 机读渲染块 |
| `docs/validation/v6/BASELINE_COMPARISON_MATRIX.md` | 三生产模式 × 文档基线 + 延迟模式的比较与声明规则 |
| `docs/validation/v6/NEGATIVE_MUTATION_CATALOG.md` | 56 条 mutation 目录 |
| `docs/validation/v6/ORACLE_AND_ZERO_CASE_POLICY.md` | 独立 Oracle / 禁同源自证 / 零用例与 skip-only 即红政策 |
| `docs/validation/v6/P0_GATE_FAMILY.md` | 根因 R1/R2/R4/R5/R10 + 785 合并层账本 P0 门族（只登记） |
| `docs/validation/v6/README.md` | 目录索引与复跑入口 |
| `reports/v6/qa-design/qa_matrix.json` | 机器规格（唯一事实源，44 门） |
| `reports/v6/qa-design/case_ledger.json` | 零用例/ skip-only 账本（44 门） |
| `reports/v6/qa-design/data/{meta,gates_analytic,gates_mc,gates_inj,gates_realdata_baseline_p0,mutations,baseline_matrix}.json` | 组装输入与 mutation 目录 |
| `reports/v6/qa-design/oracle/{assemble,qa_oracle,validate_spec,render_docs,check_docs,run_mutations,run_all}.py` | 独立 Oracle / 校验 / 渲染 / 驱动 |
| `reports/v6/qa-design/evidence/` | oracle_baseline.json、mutations.json、rc_summary.json、logs/*.log |

> 说明：任务通用产出要求提到 `run/` 工作区；本任务卡 write_scope 只含 `docs/validation/v6/` 与 `reports/v6/qa-design/`，
> 且控制器 C-004.5 对 Wave 1–4 的允许写根为 `docs/**/v6/**`、`contracts/proposals/v6/**`、`reports/v6/**`。
> 为保证"只写 write_scope"，全部机器规格、证据与命令日志落在 `reports/v6/qa-design/`（tracked），未写 `run/`。

## 2. 验证命令与实测退出码（逐条，见 evidence/rc_summary.json）

| # | 命令（在 oracle/ 下） | rc | 结果 |
|---|---|---|---|
| 1 | `python3 assemble.py` | **0** | 组装 44 门 → qa_matrix.json |
| 2 | `python3 qa_oracle.py run --json ../evidence/oracle_baseline.json` | **0** | 独立 Oracle 35/35 PASS |
| 3 | `python3 validate_spec.py --ledger ../case_ledger.json` | **0** | 结构/合同校验 0 违规 |
| 4 | `python3 render_docs.py --write` | **0** | 渲染人读表 |
| 5 | `python3 check_docs.py` | **0** | 人读/机读逐字一致 |
| 6 | `python3 validate_spec.py`（含 case ledger） | **0** | 零用例/skip-only 规则全过 |
| 7 | `python3 run_mutations.py --json ../evidence/mutations.json` | **0** | 56/56 mutation 全部检出 |
| 8 | `python3 run_all.py`（一键复跑） | **0** | rc_total=0 |

## 3. 负向 mutation 结果（注入 → rc 是否非 0）

- **science 39/39 检出**：逐条注入被测 subject 错误（OLS 取代 GLS、c×1.10、GLS 协方差用错 R~、白噪式取倒数、单位表 W_info=ADU²、
  PSFSW 跳过组内归一并当 ivar、Phase3 重采样输入 Q/W、Σc_k u_k、Drizzle 漏 D²/漏平方、常量 ADU 构造/无条件 pixfrac/S_p=F_p、
  跨帧 rho→0、错误 W、像素 ivar 冒充最优、psfsw 方差=1/W_psfsw、effective PSF 用 median FWHM、丢 UPM 项、k_corr=1、
  共享项当独立、逐帧阈值交集、样本派生 W_info、方向反转、fail-closed 回退 median SNR、delta-PSF、基线最优性冒充、
  真实数据以生产输出为唯一 expected / 缺 provenance / 未复验标 VERIFIED），断言目标门至少一个 check 变红 → 全部 rc=1。
- **spec 14/14 检出**：空 mutation 列表、psf_snr_power 进生产、诊断量进 weight.sources、支持 support×snr² 合法、
  oracle.must_not 缺失/kind 非白名单、P0 账本去层号、pending 容差无 owner、单位表 W_info=ADU²、legacy 0 进生产、
  frozen 容差无锚、**executed_cases=0**、**skip-only** → `validate_spec.py` rc=1。
- **doc 3/3 检出**：删除渲染表门行、修改渲染容差数字、插入 `| psf_snr_power | production |` 行 → `check_docs.py` rc=1。
- 证据：`reports/v6/qa-design/evidence/mutations.json`（逐条 detected=true、rc、命中门/规则）。

## 4. 关键公式/符号/冻结条目（原样继承）

- point_information：`Q_k=a_k P_k^T C_k^-1 d_k`、`W_info,k=a_k^2 P_k^T C_k^-1 P_k`、`F_hat=Q/W`、`Var=1/W`；白噪条件式 `W=a^2/(sigma^2 A_NEA)`。
- surface_gls：`x_hat=(A^T C^-1 A)^-1 A^T C^-1 d`、`Cov=(A^T C^-1 A)^-1`；像素 ivar 仅条件近似且须 `Var_approx/Var_GLS<=1+epsilon`。
- psfsw_robust：`Wt=C_norm*S^alpha*Conc^beta/(N^gamma*B^delta)`、`W_psfsw=Wt/median(Wt)`、组内 median=1、无量纲、**不是 QA-only**；
  covariance 只能 `C_out=R C_in R^T`，禁 `1/W_psfsw`；effective PSF 必输。
- Drizzle：`S_p=Σ B_j a_jp/Σ a_jp`（面亮度保持，常量场 S_p=B0 全 pixfrac）、`variance_p=Σ v_j w_jp^2/D_p^2`、
  通量守恒条件不变量（pf<1 记 `flux_conservation_factor=pixfrac^2`）。
- Phase3：`Q=a pi^T C_y^-1 f`、`W=a^2 pi^T C_y^-1 pi`、`pi=S p`，禁重采样输入 Q/W；`var_out=Σ c_k^2 u_k`、行归一 `Σ_j R_ij=1`；variance BUNIT=(signal BUNIT)²。
- 单位：`signal_sb=ADU/px^2`、`sb_variance_out=ADU^2/px^4`、`W_info=ADU^-2`、`psfsw=1`、`Q=ADU^-1`、`flux=ADU`。
- 延迟：`psf_snr_power` 不进 V6 生产路由（C-004.1）；帧级 `median(SNR_F)` 仅诊断（C-004.2）。

## 5. 未决风险与需裁决事项

1. **pending_freeze 数值（SO-07）**：7 条门的容差为 pending_freeze，owner 已登记（ALG-P2-SURF/POINT/PSFSW/P3、ALG-P2-UPM、REAL-SCIENCE-001）；
   本任务只冻结度量与门存在性，未发明数值。
2. **真实数据/预注册执行**：G-RD-01/02/06、G-BASE-03 在 W10/W11 执行；`case_ledger.json` 标 `pends=true, counts_as_pass=false`。
3. **需负责人签字（只登记，未擅改）**：SO-01..SO-07（其中 G-ANA-11 继承 SO-02/SO-03；G-MC-08 继承 SO-07；G-ANA-06/G-ANA-11 相关）。
4. **控制器级（只登记）**：F1 基线分歧（CTRL-F1）、AR-033 根构建面（CTRL-AR033）、AR-034/AR-035/AR-036；本任务未裁决、未改 CI/构建面。
5. **独立 Oracle 局限**：本任务为设计期原型 Oracle（规模受限）；W5/W7/W8/W10 必须把原型扩展为完整规模并各自独立复现。

## 5.1 执行期观察（只登记）

本任务起始实测 HEAD = `125bc0999363be1a42a1f2df3254601e0cc7b8fb`；任务结束时 HEAD = `29747831f45faa98332960049307ca74a38420f3`
（main，并行控制器提交所致）；本任务全部产物为工作树未跟踪新文件，未 commit。F1 基线分歧（工作树≠HEAD）为控制器级事项，只登记（CTRL-F1）。

## 6. 合规声明

- 未 commit / push / add / 分支 / worktree / stash / reset / clean / rebase（仅只读 git）。
- 未越界写：只写 `docs/validation/v6/` 与 `reports/v6/qa-design/`；未改 `docs/science/*.md`、`docs/owner/**`、`docs/design/**`、
  `docs/references/**`、生产源码、根构建面、CI、台账。
- 未派生子代理；未宣布发布；未改冻结门/容差。
