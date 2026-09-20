# RELEASE-02 设计增补落地（DESIGN-GAP-LAND）报告

- 分片：**DESIGN-GAP-LAND**（设计增补落地）
- 输入：`reports/RELEASE-02/design-gap-synthesis.md`（909 行）+ `.json`（**86 条台账**）
- 工作目录：`/workspace/Astro CS Database`　|　日期：2026-09-19
- 变更 claim：`工程控制/RELEASE-02/change-claims/DESIGN-GAP-LAND-001.md`（`ASTROCS_DESIGN.md` 逐条登记）
- 状态：**已收口（负责人令「前面的在飞改完收回」）**。本分片改动全部落地且自洽；`ci/checks.json`/`config/**`/`contracts/**` 已停止改动（靶子静止）；**未跑 e2e**（前台统一跑）；**零 git 写**（未 commit）。

## 0 收口摘要（先看这里）

| 项 | 结果 |
|---|---|
| 已落地 | **DG-C-01**（新门禁，P0）、**DG-A-01/A-07/A-08**（P0 最高设计）、**DG-A-03/04/06/09/10/11/12/14/15/16**（P1/P0 最高设计） |
| `ASTROCS_DESIGN.md` | **13 条增补**，633 → **686 行**；代码块围栏配平（32 行围栏 / 12 个 mermaid）；**未改任何科学公式与容差数值** |
| 新门禁 | `CHK-SPEC-NAMED-IMPL-ON-PROD-PATH`：注册 + 登记表 + 缺口台账 + `--self-test`（13 组全绿）+ 真仓红绿实证 |
| 自洽性 | `py_compile` OK；4 份 JSON 可解析；`check_registry_doc_sync` PASS；`check_impact_map` PASS；新门真仓 rc=0、`--strict` rc=1 |
| 未落地（明确登记） | **doc 侧 20 条 `needs_claim`**、**D-01..D-10 裁决项**、**DG-C-02/03/04/05**、**DG-B 主线大部分条目**（见 §4） |
| 风险 | §6；**首要风险 = 新门在真仓只抓「构建图/调用点」层，S1-002 的调用点缺口仍在**（已台账化，未接线） |

## 1 DG-C-01：新增门禁 `CHK-SPEC-NAMED-IMPL-ON-PROD-PATH`（P0，已落地）

### 1.1 交付物

| 文件 | 说明 |
|---|---|
| `ci/check_spec_named_impl.py`（新增，`24.5 KB`） | 检查器：CMake 根构建图解析 + 生产闭包 + 调用点判据 + 清单声明一致性；`--self-test` / `--strict` / `--repo` / `--json-out` |
| `ci/spec_named_impls.json`（新增） | **可维护登记表**：规范文档 → 点名的权威符号 → 期望的生产可达性（`kind` ∈ prod_path / call_site / inventory_claim） |
| `ci/ledgers/spec_named_impl_gaps.json`（新增） | **缺口台账**（`astrocs.ci-ledger/v1`，五字段强制）—— 非豁免：命中仍计入 `ledgered_gaps`、`--strict` 判红、不再复现判 `ledger_stale` |
| `ci/checks.json`（改） | 新增 top-level 项 `CHK-SPEC-NAMED-IMPL-ON-PROD-PATH`（profiles `fast/linux-main/windows-main`，`platform: any`，`waivable:false`，2 steps：主判据 + SELFTEST） |
| `ci/id_migration_map.json`（改） | `targets` + 3 条 `mappings` + `coverage.absorbed_entries`（R13 通过） |
| `docs/ci/01_CHECKS.md`（改） | §2 表同步一行（`CHK-REGISTRY-DOC-SYNC` PASS：54 == 54） |

### 1.2 判据（能红能绿，无「永远绿」）

| 判据 | 内容 | 失败行为 |
|---|---|---|
| **E1 anchor_alive** | 登记表每条 `spec.doc` 存在，且 `quote` **逐字出现**（规范不再点名 ⇒ 必须显式改表） | `rc=2`（`ANCHOR_STALE`） |
| **E2 named_file_alive** | 登记的点名文件存在 | `rc=2`（`ANCHOR_STALE`） |
| **E3 prod_path** | 点名文件必须出现在**根 CMake 构建图**（自根 `CMakeLists.txt` 沿**未注释** `add_subdirectory` 递归）某 target 的源列表，且该 target 在生产入口 `astrocs` 的 `target_link_libraries` **传递闭包**内 | `rc=1` finding |
| **E4 call_site** | 点名生产节点函数体必须**真正引用**权威符号，且调用点文件本身在闭包内（防「权威实现被替代实现置换」） | `rc=1` finding |
| **E5 inventory_claim** | 清单 `PRODUCTION_EXECUTION_INVENTORY.csv` 的 `production=yes` 声明必须与实际可达性一致（防反标） | `rc=1` finding |
| **E6 scanned>0** | 登记表为空 / 生产闭包为空 ⇒ 判红 | `rc=2`（禁止空转判绿） |
| **台账纪律** | 每条 entry 必须带 `id/kind/reason/owner/exit_condition` 五字段（缺一 `rc=2`）；台账条目**不再复现** ⇒ `ledger_stale` 判红（只减不增） | `rc=1/2` |

**不得硬编码「永远绿」的实现要点**：判据全部由**实际 CMake 图 + 实际文件内容**推导；登记表只承载「规范点名了什么」，不承载「结论」；台账只承载**缺口理由**，命中不改变 stdout 的 GAP 计数与 JSON 的 `ledgered_gaps`。

### 1.3 覆盖的三条同类缺陷（真仓实测）

| 条目 | 缺陷 | 门禁结论（真仓） |
|---|---|---|
| `SNI-S1-002-PROD-PATH` | S1-002：`sdet_api.cpp` 是否在生产闭包 | **PASS**（`astrocs_p1_sdet` ∈ 闭包） |
| `SNI-S1-002-CALL-SITE` | S1-002：生产节点 `p1_op_star_psf_impl()` **未调用** `sdet_create` | **GAP（已台账）** |
| `SNI-S2-NS-01` | S2-NS-01：`noise_model.cpp` 不在构建图（源列表仅命中测试 target `orchestrator_gate_module_snr`/`p1noise_under_test`） | **GAP（已台账）** |
| `SNI-S4-P3X-06` | S4-P3X-06：`p3_v6_export.cpp` 源列表命中 **0 个 target** | **GAP（已台账）** |
| `SNI-S4-P3X-12` | S4-P3X-12：清单 `:338` 标 `production=yes` 但不可达 | **GAP（已台账）** |

### 1.4 自检与红绿输出（逐字）

```text
$ python3 ci/check_spec_named_impl.py --self-test
SELFTEST_PASS green_all_on_prod_path (want_fail=False, 0 finding(s))
SELFTEST_PASS red_target_not_linked_into_production (want_fail=True, 1 finding(s))
SELFTEST_PASS red_node_calls_other_impl (want_fail=True, 1 finding(s))
SELFTEST_PASS green_inventory_claim_matches (want_fail=False, 0 finding(s))
SELFTEST_PASS green_gap_ledgered (want_fail=False, 0 finding(s))
SELFTEST_PASS red_strict_exposes_ledgered_gap (want_fail=True, 2 finding(s))
SELFTEST_PASS red_ledger_stale (want_fail=True, 1 finding(s))
SELFTEST_PASS failclosed_ledger_missing_fields (want_fail=True, GateError: LEDGER_ENTRY_MISSING_FIELD ...)
SELFTEST_PASS failclosed_spec_quote_gone (want_fail=True, GateError: ANCHOR_STALE ...)
SELFTEST_PASS failclosed_empty_manifest (want_fail=True, GateError: MANIFEST_EMPTY ...)
SELFTEST_PASS red_inventory_claims_unreachable_file (want_fail=True, 1 finding(s))
SELFTEST_PASS green_inventory_claim_withdrawn (want_fail=False, 0 finding(s))
SELFTEST_PASS failclosed_named_file_missing (want_fail=True, GateError: ANCHOR_STALE ...)
SELFTEST_PASS: all cases match expectation      # rc=0（13 组：正例 3 + 负例 6 + fail-closed 4）

$ python3 ci/check_spec_named_impl.py            # 真仓（台账态）
CHK-SPEC-NAMED-IMPL-ON-PROD-PATH: 4 条已登记缺口（台账承载，非豁免；--strict 下判红）：
  SNI-S1-002-CALL-SITE, SNI-S2-NS-01, SNI-S4-P3X-06, SNI-S4-P3X-12
CHK-SPEC-NAMED-IMPL-ON-PROD-PATH_PASS entries=5 passed=1 ledgered_gaps=4 closure_targets=30   # rc=0

$ python3 ci/check_spec_named_impl.py --strict   # 审计用（忽略台账）
CHK-SPEC-NAMED-IMPL-ON-PROD-PATH_FAIL: 8 finding(s)   # 4 缺口 + 4 ledger_stale    # rc=1

$ python3 ci/run_checks.py --check CHK-SPEC-NAMED-IMPL-ON-PROD-PATH --quiet
verdict=PASS entries=1 steps=2 pass=2 fail=0 ...      # rc=0
```

**真实构建图红绿实证**（`/dev/shm` 真仓结构夹具；**未改真仓 CMake**）：

```text
GREEN  真仓结构夹具                                        rc=0
RED-A  从 astrocs_module_adapters 链接行摘掉 astrocs_p1_sdet
       → SNI-S1-002-PROD-PATH: 点名文件不在生产入口 'astrocs' 的传递闭包内   rc=1
       （恢复链接行后回绿 rc=0）
GREEN-B 把生产节点 p1_op_star_psf_impl 改为引用 sdet_create + 删对应台账条目
       → SNI-S1-002-CALL-SITE pass「生产节点 p1_op_star_psf_impl() 已引用 sdet_create」 rc=0
```

> 红绿自检同时证明：**判据不是「构建图里有没有这个文件」，而是「这个文件是否真的在生产可达路径上」** —— 摘链接必红、真接线必绿。

## 2 DG-A-01 / DG-A-07 / DG-A-08 与其余 design 条目（逐条落地）

> 行号为**改后** `ASTROCS_DESIGN.md`（633 → 686 行）。每条均记入 claim `DESIGN-GAP-LAND-001` §2。

### 2.1 P0

| 台账条目 | file:line（改后） | 改了什么 |
|---|---|---|
| **DG-A-07** apply photometry 节点 | `ASTROCS_DESIGN.md:109`（流程图新节点）、`:120`（条文）、`:252` 邻域 | 流程补 `PH → AP["apply photometry 测光归一化应用到像素 / I_photo = k_photo·m(x,y)·I_cal"] → S`；条文写死「`photometry` 之后必须有 apply photometry 步骤」「`m` 低阶空间乘法增益、**必须用星点估计**（背景上乘性/加性不可辨识）」「未启用 ⇒ 显式 `degraded_reason` + fail-closed，不得按未归一化 ADU 静默走完全链」「`photappl`/`photscal` 如实落盘（不得写死 false/1.0）」 |
| **DG-A-01** 星表引导检测（完整条文） | `:118`（§3.2 条文）、`:247-249`（§3.6 硬约束） | WCS 反向投影 Gaia → 只拟合星表位置；拟合失败**直接丢弃**（不计虚警/不报错）；**top 2–5 万**上限；极限星等**按焦距/画幅/曝光时间估计、宁多勿少**、须与 **WCS CD 矩阵板比例互校** + 宽松经验上界 + 实测星等–SNR 收紧，**禁止物理闭合**；全图盲检测连通域**不是权威路径** |
| **DG-A-08** 物理噪声合成 | `:618-619`（§11.1）、`:650`（§11.3 L1 行） | 源/天光/暗流 **Poisson（电子域）** + 读出 **Gaussian（电子域）** + 增益/饱和/量化 + 平场/空间响应 `m(x,y)` + 天空梯度；**严禁纯加性天光**（须经散粒噪声体现 `B↑ ⇒ σ_F↑ ⇒ SNR↓`），并写入实测反例「纯加性天光下真值无效应 ⇒ 度量恰 **0.0000%**（什么都测不出），光子域同天光 = −7.81%/−20.33%/−37.19%」；**每个合成测试必须能红能绿：负例 = 真值无效应时度量归零**；不符者重做或标注失效 |

### 2.2 P1（其余 design 条目）

| 台账条目 | file:line（改后） | 改了什么 |
|---|---|---|
| **DG-A-04** 帧级 SNR 定义式 | `:168-176`（§3.4） | `SNR_frame(k)=F_ref/σ_F,k`；`σ_F⁻²=Σ_i P_i²/σ_i²`（Horne 1986 对角）；`σ_i²=σ_sky²+(σ_R/g)²+max(F,0)·P_i/g`；gain 不可得 ⇒ 天空受限分支 `SNR=F_ref·√(ΣP_i²)/σ_sky`；无量纲/线性缩放不变/单调性/`B→∞ ⇒ SNR→0`；点源口径不得与面亮度混用；`F_ref` 单位 ADU 且必须显式落盘作用域 |
| **DG-A-06** 测光标定语义 | `:94`（§3.1）、`:250`（§3.6） | 目标 = 真实测光坐标系（星等）；**消除物理单位、只使用星等**（`k_photo`/`a_k`/`scale` 绝对值无物理意义）；**禁止**物理闭合式反推仪器参数；**禁止**为标定因子设绝对窗口；有意义的判据只有**尺度无关**两条（测光一致性 / 帧间同一测光体系 = 语义目标） |
| **DG-A-09** 单一星表/单一通量/三处复用 | `:119`（§3.2）、`:251`（§3.6） | 一次检测 → 一次通量积分 → 测光/PSF/SNR **共用同一 `star_id` 绑定行**；全链**唯一 `flux` 口径**（`flux = 2πA·sxsy/3`，ADU）；禁另起盒和/振幅口径；`fwhm_px` 轮廓定义（Gaussian `2.3548σ` vs Moffat4 `1.230310σ`）生产者声明、**禁跨块反解** |
| **DG-A-10** 加性移除依据 + 实验证据 | `:311`（§4.4） | 理论（正确归一化后帧 = 同一测光体系真信号 + 可等效为加性的天光；乘性残留 = 低阶空间增益，已归 Phase1 `m(x,y)`）+ 世界(i)/(ii) 数值 + 证据文件 `tests/validation/release02/q3_additive_truth/logs/step6_synth.txt` |
| **DG-A-11** UPM 公共面 formulation | `:313-320`（§4.4） | 四要素：**排除自身** + **阻尼 α≈0.5** + **拟合/叠加权重同源** + **末端扣残差场**（`Σw_k g_k/W ≡ 0`）；**gauge 说明**：改 gauge 只平移全局常数、**接缝严格不变**，不得宣称为接缝修复；数值对照 (a)/(b)/(c)/(d) |
| **DG-A-12** 堆叠权重 = PΣPᵀ | `:292-293`（§4.3） | `w_fit = 1/σ²` **只用于拟合**；`w_stack = 1/Var(corrected)` 必须用残差制造者方差 `PΣPᵀ`（`Var(c_i(p))=Σ_j P²σ²`）**含梯度拟合参数协方差**；禁把拟合权重当堆叠权重、禁无条件加 `Var(ĝ)`（朴素式漏交叉项 `−2ΣHᵀ`，N=8 高估 1.29×）；过渡期 `uncertainty_available=false` 不得声称逆方差加权 |
| **DG-A-14** σ 来源（掩膜 patch 红线） | `:252`（§3.6） | `σ_bg` 必须来自 **8×8 patch + 逐星掩膜 + 饱和过滤 + 天空预算门**；**禁止**整帧未裁剪 MAD 代替；同一产品只允许**一个 σ 口径** |
| **DG-A-15** WCS 绝对零点进验收 | `:626-628`（§11.1） | 真实数据验证**必须**含：① WCS 对 Gaia 绝对零点残差 + **空间恒定性（分区检查）**；② `p1_wcs.json` 的 `samples[]` 原点声明与实际一致；③ SIP/畸变项有无须**显式声明**且与残差量级相容 |
| **DG-A-16** 稀疏层数据面语义 | `:190-192`（§3.4） | 稀疏层存**无量纲相对场** `rho_c = SNR_c/SNR_frame`（p50=1）**不是绝对 SNR**；实际 SNR = `SNR_frame × rho_c(p)`；`value_semantics`/`support_scale_px` 冻结在合同 schema；重建算子**必须返回预测方差** |
| **DG-A-03** 三路径补全（前台已改 §4.3 首条） | `:127-129`（§3.3 配置块）、`:189-192`（§3.4 Δ=64 px）、`:279-303`（§4.3） | §4.3 mermaid 由「自动检测稀疏层?」改为 **`snr_path` 显式三路径**；补「三路径精度对比是**论文核心实验**」「**不得预设稀疏一定最好**」「稀疏层损坏/不可重建 ⇒ **fail-closed**」「无层而路径为 sparse_reconstruct ⇒ 按帧级执行但**显式记 `snr_path_effective`** 并计数，不得静默」；§3.3 配置块补 `sparse_snr_spacing_px: 64`；`config/defaults.json` 说明由「稀疏层密度」改「稀疏层控制点间隔（sparse_snr_spacing_px）」 |
| **DG-A-02** 帧间独立（§3.5 部分，前台已改 §4.3） | `:218`（warn 语义）、`:227-233`（帧间独立原则） | warn 语义写死「**提示，不是判据**：不参与标定是否可信、不影响是否施加 `k_photo`；不得被实现重新变成门（防复发：曾有组间 k 散度门导致 12/12 板块全部被拒）」；新增**帧间独立原则**：逐帧独立标定到同一 Gaia 绝对测光体系、跨帧 `k` 不同**正常且正确**、**禁止任何组间/跨帧对比门**、**门只有一个 = 单帧标定是否可信**、不同光学系统混装**不得报错**、「帧间一致性」是**语义目标不是门禁** |

### 2.3 登记不改（避免重复派工）

| 台账条目 | 状态 | 说明 |
|---|---|---|
| **DG-A-05** UPM 纯加性（`§4.2:266` mermaid） | ④ 已订正（前台） | 本轮**未重改**，只在 §4.4 补「纯加性」物理依据与实验证据（DG-A-10） |
| **DG-A-13**（P2，status ③） | 未落地 | σ_sky 局部化 / `m_5` 真产出 / `A_NEA` 落盘 的**要求**已并入 DG-A-04 的 §3.4:172-173；**实现缺口**未修（属 `lib/**`） |
| **DG-A-02 的 D-01 澄清点** | 待裁决 | §3.5 已按 D-01 选项 A 口径落「语义目标、不作门禁」；claim 侧「帧间一致性」措辞订正仍待负责人 |

## 3 doc 侧条目（本轮落地情况）

| 台账条目 | 状态 | 说明 |
|---|---|---|
| **DG-B-S3-024**（`UNIFIED_SCIENCE_MODEL.md:122` UNRESOLVED 同步关闭） | **未落地** | 属任务书 P1 第 6 条；**收口令下达时尚未执行**。纯文档、零风险、可立即补做（改法：按 `FIX-SCI-SNR-CANON-001` §3.1 改写为「已关闭 + 纯加性」并登记 `OPEN-P2S-02` 仅剩数据面/schema） |
| **DG-B-01/02/03**（`07_noise_snr.md:112/113/114`、`08_drizzle.md:46` 书写形态） | 前台已落地 | 本轮核对确认已订正，**未重改** |
| **DG-B-04/05/06/07/13/14/16** 等主线条目 | **未落地** | 属 P1；本轮优先做 P0（门禁 + 最高设计），收口令后停止 |
| **doc 侧 20 条 `needs_claim=true`** | **仅登记** | 见 claim `DESIGN-GAP-LAND-001` §4；其中 4 条（DG-B-10/11、S2-PH-05/06）属 D-03/D-04/D-05 **待裁决**，按硬约束不动 |

## 4 未落地条目与原因（明确交代，不掩盖）

### 4.1 因收口令停止（**未开始**，可后续批次直接做）

| 条目 | 优先级 | 原因 / 建议批次 |
|---|---|---|
| **DG-B-S3-024** `UNIFIED_SCIENCE_MODEL.md:122` | P1 | 收口时未执行；**纯文档、零风险**，建议下一批第一件 |
| **DG-B-04/05/06/07/13/14/16** | P1 | 主线文档订正（稀疏层 Δ、CONFIG_CONTRACT 快照 50→53、Phase2 路径键名、frame_snr provenance、§4.1 实现状态、CONFIG_SCHEMA 自述） |
| **DG-B-01/02/03/08/09/12/15** | P1/P2 | 其中 08/09/12/15 需 claim（claim 正文已登记待补） |
| **DG-C-02**（ALG 文档行号锚检查） | P1 | 与 `tools/doccheck/check_alg_line_anchors.py`（工作区未提交新文件）相关，属他人在飞改动 |
| **DG-C-03**（`check_config_defaults.py` 取值面） | P1 | 需改 `ci/check_config_defaults.py`；**收口令明令不再改 ci/checks.json/config/contracts**，且需红绿自检 |
| **DG-C-04**（GUARD-TOOLS-FIX 提交 + 注册） | P0 | **已核对完成度**：`tools/config_consistency_check.py` 重写**已工作**（`--self-test` 16 组全绿；真仓 rc=0），但**未提交**且**未挂进 `ci/checks.json`**；`tools/check_p2_symbol_map.py` 仍 `FileNotFoundError`（`docs/refactor/P2_SYMBOL_MAP.md` 不存在，**未退役也未修路径**）⇒ **未完成**，已登记待办（见 §5） |
| **DG-C-05**（`parser.cpp:325` kSessionKeys ⊆ schema） | P1 | 需改 `lib/infrastructure/cli/parser.cpp`（`lib/**` 面，任务书默认不改）+ 新检查器 |

### 4.2 按硬约束**不动**（待负责人裁决 D-01..D-10，共 10 条）

`DG-B-10`（D-03 UPM 拟合目标）、`DG-B-11`（D-04 是否切 sdet）、`DG-B-S2-PH-05`/`PH-06`（D-05 孔径/`mode` 默认）等；另有 D-01/02/06/07/08/09/10 的**附加决策点**落在已落地条目上（如 D-01 对应 §3.5 帧间独立措辞）。**本轮只登记、未动**。

### 4.3 需 `lib/**` 或合同改动（本分片不改）

`DG-B-S2-DZ-08`（HEALPix 常数注释）、`DG-B-S2-PR-04`（FOV≤20° 判门）、`DG-B-S2-UNIT`（单位裁决）、`DG-B-S4X-AIO02/03/25`、`DG-B-S4X-P3X13`、`DG-A-13`（实现缺口）—— 均属 `lib/**`/`contracts/**` 或待裁决。

## 5 DG-C-04 核对结论（GUARD-TOOLS-FIX 是否已完成）

| 检查 | 结果 |
|---|---|
| `tools/config_consistency_check.py` 是否已重写 | **是**（工作区 `M`；引入 `include_consts/resolve_const`、L0a–L0e 活体/退役事实源判据、registry 豁免台账） |
| 能否运行 | **能**：`--self-test` → `CONFIG_CONSISTENCY_SELFTEST_PASS: 16 组（正例 1 + 注入红/恢复绿 + fail-closed）` rc=0；真仓 `--root .` → `"pass": true` rc=0 |
| 是否已挂进 `ci/checks.json` | **否**（`grep config_consistency_check ci/checks.json` 零命中） |
| `tools/check_p2_symbol_map.py` 是否修好 | **否**：仍 `FileNotFoundError: docs/refactor/P2_SYMBOL_MAP.md`（未更新路径、未显式退役） |
| 补丁脚本 | `run/tmp/guard_tools_fix/patch_cc{,2,3}.py` 仍在（在办证据） |
| **结论** | **未完成** ⇒ **登记待办**：① 提交重写；② 把工具挂进 `ci/checks.json`（新 CHK-* 或并入既有）；③ 处置 `check_p2_symbol_map.py`（修路径或按 `docs/ci/01_CHECKS.md` §2.1 显式退役，退役须 `main` 打印退役标识 + `exit 2`）；④ 更新 `docs/development/CONFIG_SCHEMA.md:3-4` 自述 |

> 注：③④ 需改 `ci/**` 与 `docs/**`，本分片按收口令**未动**。

## 6 风险点（自评，按严重度）

1. **门禁只覆盖「构建图 + 调用点」，不覆盖运行期实际分派**（**高**）：E3/E4 判据是**静态**的（CMake 源列表 + 函数体符号引用）。若生产节点用函数指针/字符串查表间接调用权威实现，或权威实现在闭包内但被条件分支旁路，本门**看不到**。缓解：`call_site` 的 `must_reference` 可填函数指针名或分派常量；更深一层需运行期门（如 `CHK-ALGO-WIRING` 的 `nm` 符号表面）配合。
2. **CMake 解析器是自制简化解析**（**中**）：按行去 `#` 注释 + 括号配平 + `set()` 单层变量展开；**不**处理 `if()/foreach()/function()/macro()/generator 表达式` 与多值 `set()`。若未来有 target 在条件分支里定义源列表，本门可能**漏判为不可达（假红）**或**漏判可达（假绿）**。缓解：`ANCHOR_STALE`/`MANIFEST_EMPTY` 与 `closure_targets` 计数可观测；真仓实测闭包 30 个 target（`astrocs` 传递链完整），与 `nm` 面 `CHK-ALGO-WIRING` 结果方向一致。
3. **登记表覆盖率仍偏窄**（**中**）：目前只登记 **3 个缺陷类 / 5 条**，`ALG-NOISE` 只登记了 `noise_model.cpp` 一个权威源（`snr_science.cpp` 等未登记）；`docs/science/**` 的「唯一权威」措辞尚未系统纳入。建议下一批做一次「规范点名句」全库抽取（`唯一权威|唯一生产源|生产符号唯一源|唯一权威生产源`）并批量登记。
4. **台账 4 条缺口 = 已知红**（**中**）：`--strict` 下 8 findings（4 缺口 + 4 stale）。这是**有意设计**（缺口必须可见），但会让 `--strict` 长期为红；若 CI 某处用 `--strict` 会误判。当前 `ci/checks.json` 注册**不带** `--strict`，不会红。
5. **`ASTROCS_DESIGN.md` 增补的科学数值全部为转录**（**低**）：所有数字（0.0000%/−7.81%/−20.33%/−37.19%、−0.00076/+0.15456/+0.04473、0.035/0.060、1.29×、2.3548σ/1.230310σ、Δ=64 px 等）均取自 synthesis 台账/既有 claim/实测日志，**未新造**；但**未逐条回原始日志复核**（时间盒），建议前台抽查 §4.4 与 §11.1 两处数字。
6. **`§3.4` 与 `§4.3` 的 SNR 表述仍有重复面**（**低**）：§3.4 定义式 + §4.3 权重换算两处都提 `F_ref/σ_F`，措辞一致但非单一真源；未合并以免破坏既有章节风格。
7. **未跑 `ninja/cmake/ctest`**（**按约束**）：`ASTROCS_DESIGN.md` 是纯文档，无编译面；新门是 Python，已 `py_compile` + 自检 + 真仓复跑。**但新门从未在真实 CI 环境（有 `nm`、有构建产物）跑过**。
8. **编号口径差异**（**低**）：任务书把「§4.3:256 自动检测」写作 DG-A-02、「§3.2:103 星表引导」写作 DG-A-03，与 `design-gap-synthesis.json` 台账 id 不一致。本分片一律以**台账 id** 为准并在 claim §2 注明，**未按其行文口径改**，避免误改「已订正」条目。

## 7 复现与追溯

```bash
export TMPDIR=/dev/shm/astrocs_dgl
python3 ci/check_spec_named_impl.py --self-test                  # 13 组全绿
python3 ci/check_spec_named_impl.py --json-out run/ci/fix-gates/spec_named_impl.json
python3 ci/check_spec_named_impl.py --strict                     # 8 findings（审计面）
python3 ci/run_checks.py --check CHK-SPEC-NAMED-IMPL-ON-PROD-PATH --quiet
python3 ci/check_registry_doc_sync.py                            # 54 == 54
python3 tools/quality/check_impact_map.py                        # IMPACT_MAP_PASS
python3 ci/validate_registry.py --registry ci/checks.json --strict   # 仅剩预存 CTEST-AIO-* 孤儿（非本分片）
```

- 台账：`reports/RELEASE-02/design-gap-synthesis.json`（86 条）
- 登记表 / 台账：`ci/spec_named_impls.json` / `ci/ledgers/spec_named_impl_gaps.json`
- 变更 claim：`工程控制/RELEASE-02/change-claims/DESIGN-GAP-LAND-001.md`
- 临时目录：`/dev/shm/astrocs_dgl`（已清理）
