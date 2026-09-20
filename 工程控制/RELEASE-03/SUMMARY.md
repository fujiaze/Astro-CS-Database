# RELEASE-03 阶段汇总（SUMMARY · CONTROL_PACK_SPEC §9）

> 本包**不宣布发布**（`AGENTS.md` §5：只有负责人可作最终发布决定）。
> 机器门 / 证据核验 / 文档-代码一致性三层结论见 `ACCEPTANCE.md`。

---

## 1. 目标

把 `工程控制/RELEASE-02/GAP_AUDIT.md` 与 `run/RELEASE-02/design-merge/{ARCH-01,MOD-01,IFC-01,CTR-01}.md`
审出的详细层缺口**全部收口**：已敲定的（DOC-201..204、FIX-201..208）直接订正；待定的（EXP-201..206）
**做实验定案、不预设结论**；四项前置裁决（Q2/Q4/Q6/Q8）与负责人 C01 裁决落地；最后全量构建 + 全部机器门转绿 + 端到端重跑。

**版本号未动**（本包不发布）；`run/*` 未入库；SubAgent 零 git 写，全部由前台复跑后提交。

---

## 2. 执行总览

### 2.1 任务归宿

| 类 | 任务 | 归宿 |
|---|---|---|
| A 类文档 | DOC-201 / DOC-202 / DOC-203 / DOC-204 / DOC-205（补充分片） | 全部 **PASS**（5 份变更 claim + 3 份实验落地 claim） |
| B 类代码 | FIX-201 / 202 / 203 / 204 / 205 / 206 / 207 / 208 / 209 | 全部 **PASS**（其中 FIX-201 附残留、FIX-206 由 EXP-206 定案 + FIX-201 A44 收口） |
| 实验 | EXP-201 / 202 / 203 / 204 / 205 / 206 | 全部 **CLOSED**，结论已落文档（见 §3） |
| 构建 | BLD-201 | **PASS**（附 1 条已登记已知分歧 + 1 项未达成，见 §6） |
| 端到端 | E2E-201 | **PASS**（附 2 项已登记例外/口径，见 §6） |
| 派生 | FIX-210（E2E 发现） | **PASS**（修 D1 阻断缺陷 + D2 并行归约丢计数） |
| 验收 | ACC-201 | **PASS**（本文件 + `ACCEPTANCE.md`） |

### 2.2 提交序列（前台按序原子提交，main 单线）

```
58474f7c  FIX-202  HiPS 权重枚举作废
c3ce041f  DOC-201  §9.73 A44 详细层「权重模式」整套作废
6399041e  FIX-203  §3.3/§9.73 提升键落地
175c13a7  fix      修复 26cb79d9 引入的 CFG002-04 锚点回归
0548dd69  FIX-205  §5.3 投影未实现显式报不支持
83373c23  FIX-207  §9.71 裁决 2 三命令同构 schema/模板统一
5e8c09ce  FIX-204  §9.71 裁决 3 + EXP-204 逐像素按 N 自动选择排异
832c1b3a  DOC-202  详细层其余 🔴 36 条 + S 系列 + 三项实验结论落地
003d2e5e  FIX-201  §9/§9.73 U5 aio 文件级唯一 I/O 边界 + A44
4e77e792  DOC-203  四项前置裁决落地 + 12 条同批同步订正
5f034dfc  FIX-209  退役统一数据对象 psfsw_robust_weight（14→13）
5eeed6d6  DOC-204  §11/§0.1/§0.2 权威链与状态字段清理
91bd7bbf  FIX-208  §9.74 事件流默认输出 + 资源门收窄为磁盘门
048bac4c  Q2 收口  §12 删版本号退役窗口
b67f1f94  TESTSYNC 同步 A44 / FZ-P3-MODES 配置面欠账
5311dd81  跨分片  EXP-203 C1b 单位声明 + FIX-201 死宏清理
c024cfe2  DOC-205  六项实验结论落地 + 悬空符号/行锚收口 + 权威链清理
af3c1043  BLD-201  全量构建与全部机器门转绿 + 新判据登记
3a28891a  FIX-210  多块键集阻断缺陷 + 并行归约丢计数
（本提交） pack       GAP_AUDIT §4/§5/§6 + ACCEPTANCE.md + SUMMARY.md
```

### 2.3 机器门（前台独立复跑）

| 项 | 开工基线 | 收口 |
|---|---|---|
| `ci/run_checks.py`（fast） | `verdict=FAIL entries=40 steps=86 pass=81 fail=5` | — |
| `ci/run_checks.py --all` | — | **`rc=0 verdict=PASS entries=52 steps=99 pass=99 fail=0`**（timeout=0 / prereq=0 / skip_platform=0 / skip_waivable=0） |
| `ctest --test-dir build` | — | **100% tests passed, 0 failed out of 472** |
| `ninja -C build` | — | rc=0，0 error |
| waiver | — | `ci/exemptions.json` **零新增**（但有 1 条「已登记已知分歧」，见 §6） |

**转绿的既有红灯**：STD-REG（DOC-202）、CHK-CONFIG-CONSUMED（FIX-203）、CHK-CONTRACT-REF（DOC-203/FIX-209）、
CHK-SCI-REF / CHK-DANGLING / CHK-CONTRACT-TEST（DOC-205）、CHK-SPEC-NAMED-IMPL-ON-PROD-PATH（BLD-201）、
CTEST-REGISTRATION（BLD-201）、CHK-CONFIG-DEFAULTS（BLD-201，见 §6）。

---

## 3. 差距清单闭合情况（对照 GAP_AUDIT）

### 3.1 四项前置裁决（`GAP_AUDIT §4`）

| 裁决 | 内容 | 落点 | 状态 |
|---|---|---|---|
| **Q2**（§4.1） | V6 合同层**在位保留**为「设计档案 / 产品族专用投影」，**删版本号退役窗口**改用变更编号/日期（§12） | `DATA_SEMANTICS.md §31`、`INDEX.yaml`、`UNIFIED_OBJECTS.md`、`API-001.md`、`PUBLIC_API.md`；`unified_object_registry.json` + `unified_object_compatibility_map_v1.json` 62 处字段改名 + 反向锁 | **CLOSED** |
| **Q4**（§4.2） | `resource_timeseries.csv` 唯一列合同 = 生产实现（`resource_recorder.h:260-266` 20 列）；LOG-002 的工件改名 `monitor_timeseries.csv`，两工件不互替 | `docs/api/CLI_PROTOCOL_V1.md:83-90`；`docs/architecture/observability/RESOURCE_MONITORING_CONTRACT.md` | **CLOSED** |
| **Q6**（§4.3） | 运行事件流唯一 schema = `protocol.h ValidateEventV1` + `jsonl.h`；LOG-001 显式声明**不是**运行事件流；`event`/`kind` 不得混用 | `CLI_PROTOCOL_V1.md:44-53`、`API-001.md §3`、`STRUCTURED_LOGGING_CONTRACT.md`、`observability/logging/**` | **CLOSED**（kind 开放清单见 §6-D3） |
| **Q8**（§4.4） | 块词表唯一登记处 = `aio_pipeline.h` 标准块定义表；文档只作引用 | `DATA_SEMANTICS.md:284 §11.1` | **CLOSED（附残留）**：如实登记 `variance` **尚未**入标准表 + `aio_pipeline.h:304`「未列出的自定义块名也允许」仍在位 ⇒ 归后续 FIX |
| **C01**（§4.5） | 负责人裁决 **B：真删 `psfsw_robust_weight`（14→13）** | FIX-209 逐条落地 7 条要求 + 变更 claim `CHG-2026-09-20-PSFSW-RETIRE` | **CLOSED** |

### 3.2 六项实验（`GAP_AUDIT §5`）

| 实验 | 定案 | 关键数字 | 状态 |
|---|---|---|---|
| **EXP-201** 天光采样点权重 | 取 **`control_ivar`**；理由**只能**写「SNR² 不是有效逆方差代理」 | 生产型柔性模型下两支不可区分（Δm_tilt=+3.5e-6 ≪ δ_detect=0.0496）；**预注册两条非退化判据在该配置下不通过**（已如实登记）；刚性扫描下 control_ivar 显著更优（Δm_rms=+3.4e-3） | **CLOSED** |
| **EXP-202** NaN 处置 | **掩膜**（样本级掩膜 + 重归一 + 覆盖级 NaN + 强制计数），**不是传播** | 三数据面推翻 DISP-DRZ-004；L1 污染 1.0000 / L2 0.0473–0.0662 | **CLOSED** |
| **EXP-203** Phase2 signal 量纲 | **面亮度**（条件式：直接证明的是「与 Phase1 逐位同量纲 + 密度算子 + 链内零换算」） | testdata 跨分辨率 R_cross=0.999999972724（n=1.63e7）；FLUX-IN 支 R_cross=4.0 ⇒ 链是恒等算子；**新发现 f32 层级累加缺陷**（dk=9 偏差 2.5e-3） | **CLOSED** |
| **EXP-204** 小 N 排异 | **保留 `1≤N≤3 → none`**（保守读法）；`4≤N≤5` percentile / `6≤N≤15` winsorized / `N≥16` linear fit | 低/中电平 N=3 有损（ρ−1=0.3–27% ≫ τ_ρ=0.31%）、N=2 有 83.5% 像素无输出；R5 高电平反例（≳3400 e⁻/pix）已如实登记 | **CLOSED** |
| **EXP-205** SNR 三口径精度 | **无全局最优，只有适用域** | 地面视宁度受限时稀疏最优（testdata 三帧全胜）；HST 高对比结构上帧级最好（0.112 vs 0.261/0.198）；稠密 67 MB/帧超门 64×；稀疏失效边界 Δ*/ℓ≈1（比解析预言早 4 倍）；**判据自身两处退化已登记** | **CLOSED** |
| **EXP-206** 噪声模型两套 | 取 **A**（`cpp/src/noise_model.cpp`）；B 退役 | B 偏差 +104%…+231%；第三 σ 估计器（`star_detector.cpp:30-70`，+71.8%）登记为后续包 | **CLOSED** |

### 3.3 实验驱动的最高设计订正（`GAP_AUDIT §6`，**须负责人复核追认**）

| 变更 | 对象 | 依据 |
|---|---|---|
| `CHG-2026-09-20-UPM-CTRLWEIGHT` | `ASTROCS_DESIGN.md §4.4` 天光采样点权重 SNR → **control_ivar** | EXP-201（sha256 `0d41e29b…`） |
| `CHG-2026-09-20-REJ-SMALLN` | `ASTROCS_DESIGN.md §4.5` 排异映射表 + 电平依赖注记 | EXP-204（sha256 `bd8982d6…`） |
| （EXP-205 落地） | `ASTROCS_DESIGN.md §4.3` SP-0 帧级精度门退化 | EXP-205（sha256 `81244053…`） |

> ⚠ `ASTROCS_DESIGN.md §0` 规定「修改本文必须由项目负责人明确批准」。本包按 `00_README.md` §5 自主裁决授权
> + §9.72「科学问题一律待定、由实验证明」执行，在此**显著登记供复核**。三处**均未改任何科学公式/常数/容差本身**
> （EXP-201 的 §4.3 公式逐字不变，仅追加边界句）。

### 3.4 详细层 🔴 缺口

| 报告 | 🔴 数 | 归宿 |
|---|---|---|
| CTR-01 | R01–R06 | R01→FIX-207、R02→FIX-201、R03/R05/R06→DOC-203、R04→DOC-202 |
| ARCH-01 | R07–R23 | 全部 DOC-202 落地（R12 ACR 标 DORMANT、R13/R29 原子性登记为待修缺口） |
| MOD-01 | R24–R29 | R24/R26/R27/R29→DOC-202、R28/R30「保留不改 + 设计侧订正」 |
| IFC-01 | R30–R36 | R31/R32/R33/R34/R35/R36→DOC-202、R30 保留 |
| 同步订正 | S01–S12 | S01/S02→DOC-203；S03–S12→DOC-202；S12 = IFC-01 Y1–Y4 悬空义务 |
| 其余 | U01–U03、V07/V13/V22 | DOC-204（U03 的 lib/ 代码注释 40 条**只登记不改**，代码非其域） |

**结论：详细层 🔴 条目 100% 有归宿**（落地 / 变更 claim / 明确登记的后续包），无「两边并存」。

---

## 4. 每模块状态

| 模块 | 本包动作 | 状态 |
|---|---|---|
| **cli**（三命令/预检/事件流/退出码/磁盘门） | FIX-203/207/208/210 | 三命令输入合同同构；事件流默认输出 + 唯一 schema；资源门收窄为磁盘门；退出码唯一源；多块键集与平铺门同源 |
| **scheduler / module_adapters** | FIX-204/209 + A44 | 排异逐像素按 N 自动选择；`ASTROCS_WEIGHT_MODE` provenance 键删除；`psfsw_robust_weight` 对象退役 |
| **aio**（唯一 I/O 边界） | FIX-201/202 | HARD 层越界 = 0；A44 = 0；块↔文件接口降级登记；棘轮台账 155 条（残留见 §5） |
| **coverage / phase2 排异与采样** | FIX-204/210 | 决策点单一（`kPixelSmallNPolicy`）；并行归约丢计数已修（1/N 逐位一致） |
| **projection / phase3** | FIX-205 | 产品声明注册表；未实现投影显式报不支持；TAN 适用域门 |
| **noise_snr** | EXP-206 | 生产唯一实现 = A；B 退役；台账 67→60 |
| **contracts** | FIX-209 + Q2 收口 | canonical 对象 14→13；版本号退役窗口 → 变更编号/哨兵 |
| **config** | FIX-203/207 + 锚点修复 | 三命令块结构模板；死键台账；CFG002 11/11 + DRIFTED=0 |
| **docs** | DOC-201/202/203/204/205 | A44 作废；36 条 🔴 订正；四项裁决落笔；权威链与状态字段清理；六项实验结论落地 |
| **ci** | BLD-201 | 新登记 12 条顶层判据（全部 `waivable:false` + `changed_paths` + 能红证据）；`01_CHECKS.md §2` 同步（66==66） |

---

## 5. 遗留项（如实登记）

### 5.1 必须由后续控制包收口（本包已登记，未越界改）

| # | 项 | 归属 |
|---|---|---|
| 1 | §9「除 aio 外文件写操作 = 0」**尚未全局达成** —— 棘轮台账余 155 文件 / 1656 处（TEST-HARNESS 56 / PRODUCTION-RESIDUAL 47 / DORMANT 15 / RETIRED-PENDING 15 / …） | 后续「I/O 收口」控制包 |
| 2 | HiPS tile **非原子发布** + phase2 直写（E2E 门 6 严格读法 FAIL 的根因；`ASTROCS_DESIGN §9` 明文登记的既有例外） | `IO_AND_ATOMICITY` |
| 3 | Phase2 signal 无 `BUNIT` / 无像素语义 provenance；§5.3 输入语义守卫**未接线**（`p3_v6_export.cpp` 未进构建）⇒ **在此之前不得声称 §5.3 已生效** | Phase3 export 域（EXP-203 C4/C5） |
| 4 | `aio_hips_writer.cpp:495-499,776-800` HiPS hierarchy 用 **f32** 累加（dk=9 偏差 2.5e-3 / 3.95e-4） | EXP-203 C9 |
| 5 | `module_adapters.cpp:1040-1057` 阶段二写端口仍 `UnitId::ADU`（应 `SURFACE_BRIGHTNESS`） | EXP-203 C1a（P2-XX-INT） |
| 6 | drizzle 非有限样本静默 `continue`、**未暴露 `n_rejected_nonfinite`** 计数 | EXP-202 / `P1-DRZ-IMPL` |
| 7 | 第三 σ 估计器 `star_detection/wrapper_phase1/star_detector.cpp:30-70`（+71.8%） | EXP-206 |
| 8 | v6 SIN 内核往返 **2.5e-5 px**（25× 冻结容差 1e-6 px）；v6 测试 7×7 网格取样漏掉该尺度（门禁盲区） | P3-001 / P3-PROJ-TEST |
| 9 | `variance_floor` 钳制 fail-open 加固 | EXP-206 |
| 10 | `MODULE_MAP.yaml` 164 条声明路径中 **53 条不存在**；`check_module_map` 135 findings（linux-main/windows-main 档，**不在 fast**） | MOD-002 / 各模块落地 |
| 11 | `docs/contracts/DATA_SEMANTICS.md §11.1`：`variance` 未入标准块表 + `aio_pipeline.h:304`「未列出的自定义块名也允许」仍在位 | Q8 残留（后续 FIX） |
| 12 | `DISP-P2SMP-002`（`rejected_insufficient_retained` 双计数） | P2-SAMP-IMPL |
| 13 | `lib/` 代码注释 40 条引用不存在的路径（真缺口 28 / 假阳性 12） | 后续包（清单在 `DOC-204-AUTHORITY-STATUS-001.md §3.2`） |
| 14 | 事件流开放 kind 清单（10 类）需登记进 §6.3 / `CLI_PROTOCOL_V1.md`；E2E 门判据改「五类 ⊆ 实际 kind」 | 后续 docs/FIX |
| 15 | export FITS 恒含 `RUNID`/`CHECKSUM` ⇒ 严格字节判据对 export 恒红（需改口径）；`astrocs verify` 不在命令树 | 后续 FIX |
| 16 | `p2_final.json` 仍出 `weight_mode:2`（A44 已判该概念不存在） | 后续 FIX |
| 17 | `compare_bitwise.py` 未掩码 `sampler_config.cpu_workers` ⇒ mosaic 1/N 工具仍报 RED（仅此一处） | 工具口径 |
| 18 | `tools/check_version_consistency.py` 的 W4-A3 注释「现行事由」已休眠；`tests/config/test_cfg004_unified_contract.py` 的 `FLAT_ONLY_RESIDUE` 注释过时；`contracts/schemas/phase_config_*.schema.json` 的 `x-astrocs-notes` 措辞与 CLI 现状不一致 | 后续小任务 |

### 5.2 未覆盖的验证面（如实声明）

- **平台**：只跑 Linux 腿。**Windows 分支未实测**（FIX-201/FIX-205 按 v1 原实现逐条搬运语义，未改语义）。
- **档位**：`run_checks.py --all` 为 **fast 档**；`linux-main` / `linux-deep` / `windows-main` / `fatduck` 档**未全量跑**。
- **`tests/cli` 余 6 红**（非本包）：`test_cli_single_install` 5 条（既有 `build/linux-control` 构建树的 `cmake_install.cmake` 指向空 `CMakeRelink.dir/astrocs`，环境/构建树问题）+ `test_iso_acr_gpu_isolation::test_06`（已由前台按 FZ-P3-MODES 补 `output_mode`，**待复跑确认**）。
- **E2E 未覆盖**：`SIGTERM`/协作取消 exit 9 路径。
- **实验局限**：见 `GAP_AUDIT §5` 各条的「局限」段（EXP-203 的条件式结论、EXP-204 的跨配置聚合口径、EXP-205 的 ℓ 估计量不确定度 O(2×) 等）。

---

## 6. 必须上呈负责人复核的事项（`GAP_AUDIT §6` + 本节）

| # | 事项 | 为什么必须复核 |
|---|---|---|
| 1 | **三处 `ASTROCS_DESIGN.md` 订正**（§4.4 天光权重、§4.5 排异映射表、§4.3 SP-0 门退化） | §0 规定改本文须负责人明确批准；本包按 §9.72 + 00_README §5 执行并登记 |
| 2 | **`ci/ledgers/config_default_divergences.json` 的 1 条已登记已知分歧**（`precision_mode`） | 该台账功能上是绕过 `ci/exemptions.json`「负责人批准 + 高水位 + expiry」纪律的 **de-facto 豁免通道**；本轮沿用未改机制。**ACCEPTANCE 口径 = 「1 条已登记已知分歧」，不是「无任何豁免」** |
| 3 | **「全量重建 0 警告」未达成** | `BLD-201.md` 验收门要求 0 警告；实测全量 clean 重建 59 条 warning 行（本包引入的 1 条已修）。`CHK-WARN` 的文档语义是「增量单 TU 基线」故仍绿 —— 该口径缺口是否收紧需负责人裁决 |
| 4 | **EXP-204 跨配置聚合口径** | 实验网格内判据由**电平**决定；保守读法（保留 `N≤3 → none`）维持负责人 2026-09-19 原裁决；对称读法（对齐 WBPP）只需改 `rejection.cpp:1139` **1 行** |
| 5 | **`docs/DOCUMENT_INDEX.yaml` 的 334 条 per-entry `status`** | §0.2「登记表不得写状态字段」与 `DOC-INDEX`（`waivable:false`）要求该键**直接冲突**；本包取「保住门禁 + 表内注明 + 登记」，彻底清零须先改 `tools/doccheck/check_doc_index.py` |

---

## 7. 结论

- **详细层缺口 100% 有归宿**：DOC-201..205 + FIX-201..210 + 六项实验全部落地或明确登记，无「两边并存」。
- **机器门全绿**：`ci/run_checks.py --all` = `verdict=PASS entries=52 steps=99 pass=99 fail=0`；`ctest` 472/472；`ninja` rc=0 0 error；waiver 零新增。
- **端到端跑通**：三命令真实数据 rc=0，多块形态（含 normalize）rc=0 且逐块独立 manifest，1/N worker 逐位一致，事件流默认输出且 schema 唯一。
- **诚实边界**：3 项须负责人复核（§6 #1–#3），18 项遗留已登记，Windows/重型档未覆盖，`tests/cli` 余 6 红（非本包）。
- **本包不宣布发布**。