# 负向 mutation 目录（QA-MATRIX-001）

- 文档 ID：`QA-MATRIX-001-NEGATIVE-MUTATION-CATALOG`
- 机器目录（唯一事实源）：`reports/v6/qa-design/data/mutations.json`（56 条）。
- 驱动器：`reports/v6/qa-design/oracle/run_mutations.py`；实测结果：`reports/v6/qa-design/evidence/mutations.json`。
- 每条门能红的 mutation 清单已并入 `QA_MATRIX.md` §9 每门 `门能红 mutation` 字段与机器规格 `gate.mutations`。

## 1. 三类 mutation

| 类别 | 条数 | 注入点 | 断言 |
|---|---:|---|---|
| `science` | 39 | `qa_oracle.py` 的被测 subject（解析/MC/注入/基线/结构记录） | 目标门至少一个 check 变红（rc=1），且每个 target gate 都被覆盖 |
| `spec` | 14 | `qa_matrix.json` 结构/合同字段 | `validate_spec.py` 报违规（rc=1） |
| `doc` | 3 | `docs/validation/v6/QA_MATRIX.md` 渲染块 | `check_docs.py` 报人读/机读不一致（rc=1） |

## 2. 关键 science mutation（摘要，完整清单见 JSON）

| mutation | 注入 | 目标门 |
|---|---|---|
| MUT-A01 | GLS 退化为 OLS | G-ANA-01 |
| MUT-A02 | 组合系数 c ×1.10 | G-ANA-01/02 |
| MUT-A04 | GLS 协方差用忽略 a_k 的 R~ | G-ANA-04 |
| MUT-A05 | 白噪式取倒数 | G-ANA-05 |
| MUT-A06 | 单位表 W_info=ADU^2 | G-ANA-06 |
| MUT-A07 | PSFSW 跳过组内归一 | G-ANA-07 |
| MUT-A08 | Phase3 重采样输入 Q/W | G-ANA-08/INJ-07 |
| MUT-A09 | Phase3 用 Σc_k u_k | G-ANA-09 |
| MUT-A10/A14 | Drizzle 方差漏 D^2 / 漏平方 | G-ANA-10/INJ-08 |
| MUT-A11/A15/A16 | 常量 ADU 构造 / 无条件 pixfrac / S_p=F_p | G-ANA-11/INJ-02 |
| MUT-A13 | psfsw 权重当 ivar | G-ANA-07/MC-04 |
| MUT-M01 | 跨帧 rho→0 | G-ANA-03/MC-03 |
| MUT-M02 | 报告方差用错 W | G-MC-01 |
| MUT-M03 | 像素 ivar 近似忽略 a_k 且宣称最优 | G-MC-02 |
| MUT-M04 | psfsw 方差 = 1/W_psfsw | G-MC-04 |
| MUT-M05 | effective PSF 用 median 输入 FWHM | G-MC-05/INJ-05 |
| MUT-M06 | 宣称像素 ivar 等价 W_info | G-MC-06/BASE-02 |
| MUT-M07 | 丢弃 UPM 参数项 | G-MC-07 |
| MUT-M08 | k_corr=1 | G-MC-08 |
| MUT-I01 | 共享系统项当独立 | G-MC-03/ANA-03 |
| MUT-I02 | 注入恢复用 OLS | G-MC-01/INJ-01 |
| MUT-I03 | 方差从权重标量反推 | G-INJ-01/02 |
| MUT-I04/I05 | 逐帧阈值交集 / 样本派生 W_info | G-INJ-03 |
| MUT-I06 | 扫描方向反转 | G-INJ-04 |
| MUT-I07/I08 | 背景非正返回权重 / 回退 median SNR | G-INJ-06 |
| MUT-I09 | Phase3 delta-PSF 近似 | G-INJ-07 |
| MUT-B01..B04 | 基线最优性冒充 / psfsw Fisher / variance_from_weight / equal 最优 | G-BASE-01..04 |
| MUT-R01..R03 | 生产输出作唯一 expected / 缺 provenance / 未复验标 VERIFIED | G-RD-01..06 |

## 3. spec / doc mutation 摘要

| mutation | 注入 | 命中规则 |
|---|---|---|
| MUT-SPEC-01 | 清空某门 mutations | V-GATE-MUT（P0-01） |
| MUT-SPEC-04 | psf_snr_power 进 production_modes | V-MODES-*（G-BASE-05） |
| MUT-SPEC-05 | 诊断量进 weight_sources | V-TOKEN-WS（G-ANA-02） |
| MUT-SPEC-06 | 宣称 support×snr² 合法 | V-TOKEN-RETIRED（P0-04） |
| MUT-SPEC-07 | 清空 oracle.must_not | V-ORACLE-*（P0-02/RD-04） |
| MUT-SPEC-08 | oracle.kind 非白名单 | V-ORACLE-KIND（P0-03） |
| MUT-SPEC-09 | P0 账本去层号 | V-LEDGER-LAYER（P0-05） |
| MUT-SPEC-10 | pending 容差去 owner | V-CRIT-OWNER（P0-06） |
| MUT-SPEC-11 | 单位表 W_info=ADU^2 | V-UNITS（G-ANA-06） |
| MUT-SPEC-12 | 清空 must_not | V-ORACLE-MUSTNOT（P0-02） |
| MUT-SPEC-13 | legacy 0 进生产枚举 | V-MODES-FORBIDDEN（P0-04） |
| MUT-SPEC-14 | frozen 容差无锚 | V-CRIT-ANCHOR（P0-06） |
| MUT-SPEC-15 | executed_cases=0 | V-LEDGER-ZERO（P0-01） |
| MUT-SPEC-16 | skip-only | V-LEDGER-SKIP（P0-01） |
| MUT-DOC-01/02/03 | 删行 / 改容差 / 插入 psf_snr_power=production | check_docs 不一致（P0-01/06/BASE-05） |
