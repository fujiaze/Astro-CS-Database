# CONTRACT-FREEZE-001 验证命令与证据

- 基线：`git rev-parse HEAD` = `ebefe00d3cb9018d61b7b3e8d3d7694191c1f333`（main）
- 一键复跑：`bash reports/v6/contract-review/run_all.sh`（单一 rc）

## 1. 逐条验证命令与实测退出码

| # | 命令 | 实测 rc | 证据日志 |
|---|---|---|---|
| 1 | `python3 reports/v6/contract-review/tools/gen_freeze.py` | 0 | `logs/10_gen.log` |
| 2 | `python3 reports/v6/contract-review/tools/oracle_contract_freeze.py` | 0（2056 checks，ALL PASS） | `logs/20_oracle.log` |
| 3 | `python3 reports/v6/contract-review/tools/mutate_and_check.py` | 0（正向控制 rc=0；23/23 检出） | `logs/30_mutations.log` |
| 4 | `python3 reports/v6/contract-review/tools/scope_check.py` | 0（violations=[]） | `logs/40_scope.log` |
| 5 | `python3 reports/v6/contract-review/tools/check_doc_consistency.py` | 0（96 条款/12 文档；删行负向控制检出） | `logs/50_doc_consistency.log` |
| 6 | `bash reports/v6/contract-review/run_all.sh` | **0** | `logs/00_run_all.log` |

> 零用例/skip-only/同实现自证不得 PASS：Oracle 含 2056 条值断言与未变异正向控制；`mutate_and_check` 先跑正向控制（必须 rc=0）再逐条注入；`check_doc_consistency` 自带文档删行负向控制。

## 2. 独立一致性 Oracle 覆盖面（冻结表 ↔ W3 规格 ↔ SCI-ADJ）

- 单位表三重一致：机器表 == `adjudications.json#units_table` == `data-design-catalog#frozen_units_table`；
- mode 三重一致：生产/基线/延迟 == adjudications == catalog；枚举越界（`psf_snr_power`/`auto`/`support_x_snr2`/`0`）即红；
- 42/42 SCI-ADJ 冻结条目的 id、值、scope、gate、anchor 逐条等于裁决表；19/19 `required_freeze_ids` 覆盖；
- 54 条数值阈值：每条要么唯一值 + 可在 W3 tracked 规格中定位的 source binding（md 表行 / 近邻子串，且冻结值与源一致），要么 `OPEN` + 明确 owner；数值与 W3 源不一致、模糊值（TBD/待定）、无源绑定即红；
- 禁止项：weight-source 禁 token 集、psfsw 产物禁键集与 catalog / FREEZE_LIST 一致；缺 token / 缺键即红；
- 声明权重来源与禁止 token 交集、把 median SNR / psfsw / support / coverage 当权重或 ivar 即红；
- SO-01..07 必须存在且为 `PENDING_OWNER_SIGNOFF` + owner；缺项 / 改状态即红；
- DI-01..07 / OI-01..05 / PF-01..07 必须存在且带 owner；被取代清单必须覆盖 DRIZZLE / CONTROL_WEIGHT_SNR / ACR_EQUIVALENCE / INTEGRATION / CALIBRATION / PHASE3；
- 计数自洽（frozen / pending / open / superseded / open_items）。

## 3. 负向 mutation 结果（23/23 全部 rc!=0；未变异正向控制 rc=0）

| mutation | 注入类别 | Oracle rc | 是否检出 | 首个失败检查 |
|---|---|---|---|---|
| M01_delete_required_freeze | 删除冻结（required freeze 缺项） | 1 | 是 | C5c: missing SCI-ADJ freeze clause(s): ['FZ-UNIT-WINFO'] |
| M02_change_frozen_value | 改语义冻结值 | 1 | 是 | C6:FZ-UNIT-WINFO: value mismatch vs SCI-ADJ |
| M03_pending_written_as_frozen | 把 pending 写成 frozen | 1 | 是 | C13g: counts.frozen mismatch |
| M04_pending_numeric_as_frozen | 把 pending 数值写成 frozen | 1 | 是 | C8-own:PSFSW-T-DEPTH: FROZEN numeric clause must not carry owner_signoff |
| M05_change_numeric_value | 改数值（与 W3 源不一致） | 1 | 是 | C8-value:PSFSW-T-DEPTH: frozen value 0.5 inconsistent with W3 binding '0.05' |
| M06_median_snr_as_weight_source | 把 median(SNR_F) 写成权重来源 | 1 | 是 | C9:point_information: declared weight sources hit forbidden tokens: ['median_source_snr'] |
| M07_psfsw_written_as_ivar | 把 psfsw 权重写成 ivar | 1 | 是 | C9:psfsw_robust: declared weight sources hit forbidden tokens: ['psfsw_robust_weight'] |
| M08_psf_snr_power_in_production | 把 psf_snr_power 加入生产枚举 | 1 | 是 | C2c: production modes != adjudications |
| M09_remove_forbidden_token | 删除禁用权重 token | 1 | 是 | C4: forbidden weight_source_tokens != adjudications |
| M10_units_table_mismatch | 单位表不一致（W_info） | 1 | 是 | C2: units_table != adjudications.units_table |
| M11_null_value_claimed_frozen | 把无值 OPEN 写成 frozen 且无 owner | 1 | 是 | C8-null:QF-G-INJ-01: null numeric value without pending+owner |
| M12_drop_signoff_item | 删除签字项 SO-05 | 1 | 是 | C10:SO-05: missing signoff item SO-05 |
| M13_remove_open_item | 删除开放项 PF-07 | 1 | 是 | C11: missing open items: ['PF-07'] |
| M14_remove_superseded_drizzle | 删除被取代段 DRIZZLE | 1 | 是 | C12:DRIZZLE.md: superseded list missing DRIZZLE.md |
| M15_empty_gate | 清空验证门 | 1 | 是 | C6:FZ-FORMULA-GLS: gate mismatch vs SCI-ADJ |
| M16_empty_psfsw_forbidden_keys | 清空 psfsw 禁止键 | 1 | 是 | C4c: psfsw_forbidden_keys != catalog |
| M17_psfsw_units_mismatch_units_table | psfsw 单位表写成 ivar | 1 | 是 | C2: units_table != adjudications.units_table |
| M18_coverage_as_weight_source | 把 coverage 写成权重来源 | 1 | 是 | C9:equal: declared weight sources hit forbidden tokens: ['coverage'] |
| M19_fuzzy_numeric_claimed_frozen | 把模糊数值(TBD)写成 frozen | 1 | 是 | C8-fuzzy:FZ-AP2S-KAPPA-MAX: fuzzy numeric value: 'TBD' |
| M20_psfsw_group_not_normalized | psfsw group_normalized=false | 1 | 是 | C9d: psfsw_robust group_normalized != true |
| M21_source_binding_broken | 改数值破坏 W3 源绑定 | 1 | 是 | C8-value:FZ-AP2S-IDENT-RTOL: frozen value 0.001 inconsistent with W3 binding '1e-9' |
| M22_declared_baseline_promotes_deferred | 把 psf_snr_power 加进文档基线 | 1 | 是 | C2d: baseline modes != adjudications |
| M23_coverage_as_variance_in_psfsw | 移除 psfsw variance 禁止键 | 1 | 是 | C4c: psfsw_forbidden_keys != catalog |

### 3.1 任务强制五类 mutation 逐条对应

| 强制类别 | mutation | 实测 |
|---|---|---|
| 删除冻结 | `M01_delete_required_freeze` | rc=1（C5c/C5e/C13g） |
| 改数值 | `M02_change_frozen_value`、`M05_change_numeric_value`、`M21_source_binding_broken` | rc=1 ×3（C6 / C8-value） |
| 把 pending 写成 frozen | `M03`、`M04`、`M11`、`M19` | rc=1 ×4（C7b / C8-own / C8-null / C8-fuzzy） |
| 把 median SNR 或 psfsw 写成 ivar | `M06`、`M07`、`M18`、`M23` | rc=1 ×4（C9 / C9c / C4d） |
| 加入 psf_snr_power 生产 | `M08`、`M22` | rc=1 ×2（C3 / C14b / C2d） |

## 4. 越界写审计

| 项 | 值 |
|---|---|
| write_scope 内文件（manifest） | 55（untracked 48 + gitignored 日志/缓存 7） |
| write_scope 外新增/修改 | 0（violations=[]） |
| tracked 文件改动总数（全部为既有 CTRL-F1 预存 dirty，均在 scope 外） | 37 |
| scope 内 tracked 改动 | 0 |

## 5. 明确声明

- 未 commit / push / add / 分支 / worktree / stash / reset / clean / rebase；git 仅只读。
- 仅写 `docs/science/v6/frozen/`、`docs/contracts/v6/frozen/`、`docs/algorithms/v6/frozen/`、`reports/v6/contract-review/`；未越界写。
- 未改冻结门/容差；未把 `psf_snr_power` 解冻进生产（`FZ-MODE-DEFERRED`，M08/M22 可红）；未把 `median(SNR_F)`/`psfsw` 接入权重面（C-004.1/.2，M06/M07 可红）。
- SO-01..07 全部保持 `PENDING_OWNER_SIGNOFF`；F1 与 AR-033 只登记不裁决；未派生子代理；未宣布发布。
