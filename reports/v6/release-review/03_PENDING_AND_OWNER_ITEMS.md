# 未决项与需负责人裁决事项（DOC-CONVERGE-001 / V6 并行包 Wave 12）

> 本文件**如实登记，不淡化、不写成已解决**。所有冻结条款保持原状态并 fail-closed。
> 数据源：`docs/contracts/v6/frozen/astrocs.v6.contract-freeze.v1.json`、
> `reports/v6/contract-review/{01_FREEZE_STATUS_MATRIX,04_OPEN_ITEMS_AND_SIGNOFF}.md`、
> `CONTROLLER_LOG.md` C-008/C-009、`reports/v6/{windows,performance,real-science}/`。

## 1. 条款状态（96 条款：FROZEN 39 / PENDING_OWNER_SIGNOFF 49 / OPEN 8）

- **49 条 PENDING_OWNER_SIGNOFF**（¶ 值唯一确定，但涉及 FROZEN 非 v6 SCI 修订/数值确认，须负责人按 `SO-xx` 签字后方可作为正式修订生效；**生效前实现不得放宽、相应面 fail-closed，任何产物不得写成已冻结**）。
  典型：`FZ-UNIT-VAR-IN`/`FZ-UNIT-VAR-SB`/`FZ-UNIT-IVAR-SB`/`FZ-BUNIT-SEMANTICS`（`SO-01`）、
  `FZ-FORMULA-DRIZZLE-SB`/`-VAR`/`FZ-COND-FLUX-CONSERV`（`SO-02`）、`FZ-GATE-CONST-SB`（`SO-03`）、
  `FZ-GATE-PARENT-VAR`（`SO-04`）、`FZ-AP1-DEFICIT-THRESH`、`FZ-PROV-KCORR-VALUE` 及全部 `PSFSW-T-*`/`PSFSW-COMPOSITE-*`/`FZ-AP2S-*`/`FZ-AP2PT-*`（`SO-07`）。
- **8 条条款级 OPEN**：`CF-T-P3-CORR-EPSILON`（`epsilon_corr` 无数值）、
  `QF-G-INJ-01`/`02`/`03`/`07`、`QF-G-RD-01`/`02`、`QF-G-BASE-03`（均在 W3/W10 预注册前保持待冻）。
- **`SO-01..07` 全部保持 PENDING_OWNER_SIGNOFF**，本任务不使任何条款生效（清单见 `reports/v6/contract-review/04_OPEN_ITEMS_AND_SIGNOFF.md` §3）。
- 开放项登记册另有 27 条（`DI-01..07`、`OI-01..05`、`OPEN-P2S-01..03`、`P3-OPEN-EPSILON-CORR`、`PF-01..07`、`AR-032/034/035/036-GAP`）：
  - `DI-01`（词表归一）与 `DI-07`（`weight_units` 双射）**由 W6 闭合**（单一词表）；
  - `DI-02`（数值阈值）、`DI-03`（shared systematic 低秩/相关核数据面）、`DI-04`（`k_corr` 标定脚本 + 固定种子 MC）、
    `DI-05`（AR-048 参数生效证明）、`DI-06`（exchange plane 枚举扩展与 runtime validator 同一提交，`F-UNC-003`）**保持 OPEN**；
  - `OI-04`（`docs/**/v6/**` 未登记进 `DOCUMENT_INDEX.yaml`，`docs_fully_covered` 红）**由本任务 W12 闭合**（§5）；
  - `OI-01`（Phase1/Phase2 PSFSW 接口拆分）、`OI-02`（`V_b` 合并关系）、`OI-03`（SCI-CAL-001 取代登记）、`OI-05`（共同星集阈值）**保持 OPEN**。

## 2. SO-05 资源门（最高优先，待负责人裁决）

| 项 | 实测 | 冻结门 | 状态 |
|---|---|---|---|
| 16 worker 活跃窗口 CPU 均值（占已分配容量） | **65.09%** | ≥ 85%（§10.5/§18.2） | 低于门 |
| 16 worker p50 | **87.63%** | ≥ 90% | 低于门 |
| 连续 ≥10 s <60% | 正常负载未触发；注入 CPU 争用后 34.68 s 被记录 | 任一连续 10 s <60% 失败 | 负向证实门可红 |
| 内存增长（16w） | `alloc_growth_unbounded@16w`（34.01 MB/s ≥ 32），斜率随墙钟缩短升高 | 无界内存增长失败（§17.6） | **未定性** |
| alloc 回收 | 全预算 `alloc_reclaim_missing` | — | 未解释 |

- 处置：`status=record_only_pending_owner_signoff`、`hard_fail=false`、`auto_adjudication_allowed=false`（`C-007`；PERF-SCALE-001 §5）。
- **需负责人签字**：§10.5/§17.6 门「只记录 vs 自动判决」及 16w 低利用率是否判失败（`AR-036`/`SO-05`）。
- 依据文件锚：`reports/v6/performance/PERF-SCALE-001.md` §5/§8、`artifacts/v6/performance/p1_real_ldn43_4k_resource_gate_record.json`、`alloc_report.json`。

## 3. CI 红线（不得以 waiver 掩盖）

基线 `8e1e280e` 本地只读复跑（日志 `artifacts/v6/release-review/logs/ci_redline_recheck.log`）：

| 检查 | 结果 | rc | 事实 |
|---|---|---|---|
| `THREAD-BUDGET` | **FAIL** | 1 | `lib/core/src/module_adapters.cpp:245,252` 的 `omp_set_num_threads` 未登记（**属 P36 回退态**，修正需 owner 授权，`C-009` 裁决项 8） |
| `CTEST-REGISTRATION` | **FAIL** | 1 | 余 2 项**非 V6** IPV 残项：`ipv_dead_params_lock`/`ipv_dead_params_lock_selfcheck`（`lib/plate_solve/cpp/ipv/test/CMakeLists.txt`） |
| `AGENTS-GOV` | PASS | 0 | 10/10 治理要素齐备 |
| `VERSION-CONSISTENCY`（`ci/check_version.py`） | PASS | 0 | 根 `VERSION`=`0.11.0-alpha.2`，生成链一致 |
| `check_version_namespaces.py` | PASS（含 11 项他人路径 legacy 登记） | 0 | 未修改路径遗留 |

- **Linux CI 整条 V6 线自 `ebefe00d`（Wave 3）起持续红**（公共 API 逐 commit 核对）；AR-033 注册非肇因但红线未消。
- **Windows CI**：基线 SHA run `35012779853` = failure（step 5「Run MSVC tests and package candidate」exit 1），Upload candidate skipped → **无候选**；
  Fatduck Validation run 全 failure（`no-candidate`，CI-001 fail-closed）。
- **`§17.12` 门 7（同 SHA Linux/Windows CI 通过）与门 8（Fatduck Windows 复验）均未满足**。

## 4. Windows / Fatduck（AWAITING_WINDOWS_VALIDATION）

- Fatduck（100.104.10.71:22）**不可达**：ssh rc=255 Connection timed out、/dev/tcp rc=124、ping 100% loss、tailscale `windows offline, last seen 3h ago`。
- `WIN-VERIFY-001` = **AWAITING_WINDOWS_VALIDATION**：32/32 Windows 用例 UNAVAILABLE（同 SHA 候选安装 / ABI / 长路径 / >2GiB / 真实产品），sha256 清单如实留空。
- **`FD-F-003`（P0 OPEN）**：Windows C++ 单测门长期「0 用例 PASS」（`No tests were found!!!`）。
  **恢复「用例数>0」前，Windows C++ 单测面不得作为发布证据引用**（未改 `ci/`，仅登记）。
- Windows CI 失败根因需 token 取回 `win-stage-*.log` / `win-package-summary.json`。
- 依据：`reports/v6/windows/WIN-VERIFY-001.md`、`artifacts/v6/windows/{FATDUCK_REACHABILITY,GITHUB_ACTIONS_EVIDENCE}.json`、`artifacts/v6/windows/UNVERIFIED_ITEMS.json`。

## 5. 本任务已闭合项

- **OI-04**：`docs/**/v6/**` 61 份登记进 `docs/DOCUMENT_INDEX.yaml` → `check_doc_index.py` rc=0，`DOC_INDEX_PASS`（覆盖 282 文件）。
- **concentration 单位文本错误**：`ADU/px` → `ADU/px²`（详见 `01_CONVERGENCE_CORRECTIONS.md` §1）。
- 状态词收敛：README/REVIEW/CHANGELOG 与冻结 V6 口径一致；未发布、未提升版本。

## 6. 负责人/控制器待裁决（本任务范围外，如实携带）

1. **SO-05 资源门**（§10.5/§17.6 记录 vs 判决；16w 65.09% 是否判失败；内存增长定性）。
2. **CI 红线归属**：P36 回退态 `module_adapters.cpp` 线程预算旁路是否授权修正；2 项 IPV CTEST-REGISTRATION 残项归属；Windows MSVC 失败根因取日志。
3. **F-CAR / F-AIT legacy 投影错误**：legacy `lib/phase3_proj/p3_projection.cpp` 的 CAR 赤纬反号（最大偏差 345600″=96°）、AIT 缺 Paper II √2（最大偏差 94885″、world→pix 残差 13.05 px）；legacy 文件与其逆向测试不属任何 V6 写域，**需负责人授权后独立修正**（`C-007`/`C-008`）。
4. **REAL-SCIENCE-001 口径诚实性**：`equal/exposure/ivar` 三口径为 **DOCUMENTED_BASELINE（驱动内实现）**，
   仅 `W_info` 与 `PSFSW` 走冻结库函数；该验收证明的是两条生产口径相对文档化基线的行为，
   **不是**全链生产路径端到端验收（`C-008` 诚实性登记）。
5. **Phase2 CLI 真实数据不可达**：三生产模式端到端 fail-closed（`node upm_fit: control ivar 缺失 / obs=0 overlap_controls=0`）；
   属数据流覆盖缺口，建议由真实多帧覆盖流程复验。
6. **非 v6 SCI 正文取代**（`AR-032-GAP`/`SO-06`）：`docs/science/*.md`、`docs/design/**`、`docs/references/**`、`docs/owner/**`
   的被取代段由 W4 登记（`reports/v6/contract-review/03_SUPERSEDED_SCI_SECTIONS.md`），**正式 amendment 须负责人签字**；
   控制包 `C-004.5` 禁止在本包过程中改写其正文 —— 本任务遵此，未改。
7. **schema/单位交叉张力**（`C-008` §2/§3）：`provenance.v1` 的 allOf（`bunit=ADU` ⇒ `pixel_semantics=surface_brightness ∧ pixel_area_power=-2`）与
   `signal.v1`（`integrated_flux ⇒ pixel_area_power=0`）使纯积分通量主面不可表达；`quantity.units` 在生产 schema 中仍为自由 string
   （篡改 `W_info.units` 仍可过 schema，建议收紧为 const/enum）—— 均需负责人/contracts owner 裁定，本任务未擅改 `contracts/**`。
8. **AIO FITS 实数格式**：`FitsCard::make_real` 用 `%.12g` 产生小写 e，astropy 判 not FITS standard；P1/P3 在产物层规范为大写 E 绕过，
   **AIO 本身待修**（属 `lib/astro_image_io/v6` 已提交写域，非本任务）。
9. **AR-034-GAP**：7 项历史 CI 红无 V6 逐名验收锚；**AR-035-GAP**：785 合并层缺陷账本无销账任务。
10. **`memory.md` 未收敛**：其「当前 SHA/版本」仍为 DOC-CONV-001 期（`da3c4b4a`），不在本任务 write_scope；建议后续任务或前台同步。
11. **concentration 订正的签署形式**：schema 的 `registered_text_error` 原文要求「DOC-CONVERGE-001 修正，须负责人签字后方可改 FROZEN 正文」；
    本任务按任务卡执行订正且不改任何冻结值，请负责人确认；`contracts/**` 内该字段文案由 contracts/ owner 刷新。
