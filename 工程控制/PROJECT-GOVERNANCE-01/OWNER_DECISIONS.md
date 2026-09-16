# OWNER_DECISIONS —— 需负责人裁决清单（前台汇总 · 2026-09-16）

> 依据 `AGENTS.md §8`：**科学定义歧义 / 最高权威冲突 / 发布类决定**必须停下问负责人，执行行不得自选口径。
> 本清单由 W3 复检轮的实际阻塞汇总而成；每条含「问题 / 证据 / 阻塞面 / 若你暂不裁决时的前台默认处置」。
> 裁决后前台会在 `GAP_AUDIT.md` 登记结论并把改动转为可派任务。

## A. 科学公式与冻结定义冲突（P0，阻塞 GAP-011 与 phase2/3 权重链）

| # | 问题 | 证据（实测） | 阻塞面 | 默认处置 |
|---|---|---|---|---|
| **D-1** | **CAR/AIT 基准点语义**：`CRVAL2` 未进入映射 | 独立 Oracle（astropy 7.0.1，不调生产）：`CRPIX` 处 astropy=(10,30)，而 v1 legacy 与 v2 `p3_proj_v6` 冻结式给 dec=0 ⇒ **30° 基准点偏差**（`p3_projection.cpp:198/204/217`；ALG:399 明文冻结） | P3-001 的 4 条 P0 + 八投影 registry（GAP-011） | 按 WCS 标准把 CRVAL2 纳入映射，**改 ALG §15.2/§15.3 冻结公式 + registry 版本递增登记** |
| **D-2** | **AIT 域界** `A<2 → A≤1` | astropy：X=162.06°=2√2 rad 截止；v6 接受到 229.18°=4 rad ⇒ **环带折叠**（ALG:424 亦需订正） | 同上 | 按 astropy 收紧为 `A≤1` |
| **D-3** | **CAR 奇点**：δ=±90° 整行塌缩与 θ 语义 | `M7-A-137` | P3-001 | 明确奇点处的规范化约定（定义或拒绝） |
| **D-4** | **八投影 vs 四投影**：DESIGN §5.3 列八投影，ALG:20/:348 只列四；SCI:100 称「alpha 仅 TAN」 | `GAP-011`、`M6b-C-003`、`M7-G-101` | GAP-011 整个实施 | 以 DESIGN §5.3 为准（八投影），ALG/SCI 相应章节升级为「变更 claim」 |
| **D-5** | `ASTROCS_P3_MAX_SIDE` **编译期放宽 SCI 冻结上限** | 3 处源 + `DATA_SEMANTICS:2209` | P3-001 | 删除编译期放宽，改为运行时按 SCI 上限校验 |
| **D-6** | **SYN-007 容差** 1e-3 vs 1e-2 **无冻结表** | `M1a-F-004` | P3-001 | 补一张冻结容差表（SCI 侧），实现随之 |
| **D-7** | **`weight_mode=2` 的唯一语义**：三套互斥权威并存 | ALG `PHASE2_INTEGRATION.md:125-127`（ivar_valid?ivar:support）vs `CONTROL_WEIGHT_SNR §4:71`（support×snr_v²）vs `SCI-UPM-WEIGHT-001 §5:54`（禁 production 乘 support^p）；实现默认 fail-closed（`stage2.cpp:565` 全产品缺失 return 7） | P2-002 的 4 条 P0；一处裁决可同销 `M7-A-125` | 以 SCI 条款为准，订正 ALG 与 CONTROL_WEIGHT_SNR 的式子与**分支号**（现文写 2、实现是 0） |
| **D-8** | **`w_UPM` 是绝对量还是 per-control 份额** | `PHASE2_UPM §5:46`（绝对式 quality×geom×control_ivar，§3:29 单位 ADU⁻²）与 `§5:47`（归一份额式）并存；实现 `upm.cpp:1344` 是份额式 | P2-002 | 明确唯一式（建议份额式，与实现一致），另一式标 RETIRED |
| **D-9** | `CONTROL_WEIGHT_SNR §4:71` 的**分支号错**（文中 2 / 实现 0） | `M3-A-002` | P2-002 | 直接订正为 0（纯勘误，不需科学判断） |
| **D-10** | **`k_corr` 三问**：查表适用域 / 标定域 `[300,600]″` 与生产尺度（真帧 0.9586″/px）**不相交** / 是否强制 k≥1 | `M7-A-112`、`M9-A-2`、`M7-A-205`、`M7-A-113/115` | P2-002 | 先裁「标定域是否覆盖生产尺度」，不覆盖则禁用在生产，或重标定 |

## B. 观测/资源门的权威落点（阻塞 OBS-001 的 22 条 OPEN）

| # | 问题 | 证据 | 默认处置 |
|---|---|---|---|
| **D-11** | **冻结门数值 85%/60%/10s/32MB/s 在新权威链零命中**（DESIGN / ENG_SPEC / CONTROL_PACK_SPEC / docs/plugins / docs/ci 全无），只存在于 OBS-001 卡正文与引用**已删宪章 §18.2** 的代码注释 | OBS-001 实测 | 由你指定权威落点（写进 `docs/plugins/**` 或 ENG_SPEC §8），否则该门无据 |
| **D-12** | **`allocated_capacity_cores` 分母取义** | `resource_gate.h:241` 自标 `NEEDS_DECISION`（granted_workers vs min(selected,available)） | 建议取 `min(selected, available)`（与「不超授权」一致） |
| **D-13** | **判定链三套并存**：C++ `cli/resource_gate.h`（90/85/70/60）、Python 冻结门（85/60/10s）、外挂 judge（85/60），集合与 10s 边界不一致 | `M5a-G-003`（P0）+ `V12-N-08` | 收敛为一套（建议以 Python 冻结门为唯一实现，C++ 退化为读取） |
| **D-14** | `--resource-detail` **timeseries 是否必须内嵌曲线点** | `V20-N-04` | 建议内嵌（观测价值）或明确不内嵌（体积） |
| **D-15** | **工件命名**连字符/下划线不一致（DESIGN §9 vs `CLI_PROTOCOL_V1:61`） | `M5a-C-003` | 订正文档为唯一写法 |

## C. 质量门阈值与既有裁决的登记面（阻塞 DOC-001/GOV-001 收尾）

| # | 问题 | 证据 | 默认处置 |
|---|---|---|---|
| **D-16** | **0.5 px 质心门 vs 实测 0.897 px** | `M3b-G-01`（P0，DOC-001 只登记未代裁） | 二选一：认定实现超标（改实现）或认定门过严（改 SCI 阈值并登记变更 claim） |
| **D-17** | 三族「负责人裁决」在**登记面 0 命中**：`PSF-FAST-001`、`P17-NSIDE`（nside 采样率等价）、**帧头 WCS 未授权** | `V8-N-07`、`W1-N-06`（GOV-001 实测） | 裁决**存在** ⇒ 补登记面（写入 docs/plugins 或 ENG_SPEC）；**不存在** ⇒ 撤销据此钉死的测试期望与注释 |

## D. 前台已自行处置（不需你裁决，供知悉）

- **`VERSION` 保留**：`cli/CMakeLists.txt:27` 有 `file(READ ../VERSION)`，删除会破构建 ⇒ 保留并登记（GOV-001 结论，前台认可）；
- **`FATDUCK_ACCESS.md` / `问题扫描/` 保留禁删**：前者运营必需（ROOT-006），后者是在用台账；
- **`HANDOVER.md` / `VISUAL_CHECK_README.md` 归档**至 `docs/archive/` + ARCHIVED_NON_NORMATIVE 抬头；
- **`core_pipeline` 测试恒红**：依赖被 GOV-002 归档删除的 fixtures（GAP-036）→ 归 QA-001（重建或显式退役）；
- **六层追溯门退役**（GAP-032）：输入在构建产物目录、口径属已废止世代、新规范未要求。

## E. 阻塞面统计（不裁决会怎样）

- **P3-001**：4 条 P0 全 OPEN；GAP-011（八投影 registry）**完全无法开工**；
- **P2-002**：4 条 P0 全 OPEN；`weight_mode=2` 的语义冲突使「改哪一支」无法判定；
- **OBS-001**：22 条 OPEN 中与门阈值/权威落点相关的部分无法定案；
- **DOC-001**：`M3b-G-01`（P0）只能登记；
- **GOV-001**：三族裁决登记面无法闭合。

---

## F. P1-001 汇总的 49 条「需权威裁决」（按共性归并 5 类）

> 出处：`run/PROJECT-GOVERNANCE-01/P1-001/自证摘要.md §7.1`、`处置表.md`（112 行）。
> 这 49 条**全部**因「`docs/science`/`docs/algorithms` 只读」或「属科学定义变更」而阻塞 ⇒ 归入本清单。

| 类 | 共性 | 代表条目 | 处置建议 |
|---|---|---|---|
| **F-1** | **SCI/ALG 文本与实现互斥**（公式量纲/自变量/母函数不一致） | `M1a-A-001`（SCI 公式与实现量纲/自变量互斥）、`M3b-A-01`（生产拟合用高斯母函数而非 SCI 冻结 Moffat4）、`M2a-A-1`（DRIZZLE §5/§7/§11 三种面亮度陈述不可同真） | 逐条裁「以 SCI 为准改实现」或「以实现为准改 SCI」；后者须走文档集变更流程 + 变更 claim |
| **F-2** | **科学门/容差缺依据**（门值或量测域未定义） | `M1a-A-002`（F1 精度门量测域未定义；RMS 0.151px vs 独立 median 0.897px ⇒ 与 D-16 同源）、`M1a-F-004`（SYN-007 1e-3 vs 1e-2 无冻结表） | 补冻结表（定义量测域 + 阈值来源），或下调门并登记 |
| **F-3** | **默认值与精度语义多源** | `M1a-C-001`（NB_GRID 冻结 7 vs 实现 41/81）、`M3b-F-02`（PSF.md 承诺的 scipy curve_fit Oracle 缺失） | 指定唯一默认值来源（SCI 或 config schema），另一处标 RETIRED |
| **F-4** | **数据对象一字段多义** | `M3b-A-03`（MAD 冒充 `flux_uncertainty`，兼他域） | 明确字段语义并在 `contracts/schemas/**` 与其实现面同步 |
| **F-5** | **Oracle 承诺无法兑现** | `M3b-F-02`（同上）、`M1a-F-001`（legacy 两 Oracle 与实现同式，对 CAR/AIT 系统性错映射无区分力） | 建**独立** Oracle（树内已有 astropy 版可复用），并把同源 Oracle 标为无效 |

### F.1 与其它条的合并关系

- F-2 的 `M1a-A-002` 与 **D-16**（0.5px 质心门 vs 实测 0.897px）**同源**，建议一并裁决；
- F-5 的 `M1a-F-001` 与 **D-1/D-2**（CAR/AIT）**同源**：基准点/域界裁决后，独立 Oracle 即为「改后绿」的验收基线；
- F-1 的 `M3b-A-01`（Moffat4 vs 高斯）与 **D-7**（weight_mode=2）同属「冻结 SCI 定义 vs 生产实现」族，建议同批处理。

