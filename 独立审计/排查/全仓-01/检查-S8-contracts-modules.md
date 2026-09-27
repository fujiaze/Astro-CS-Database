# 全仓对抗性静态排查 · 切片 S8：docs/contracts + docs/modules + docs/TRACEABILITY

**检查员**: S8 只读检查员（父会话 session-9a133fb6-13a3-4348-a148-42f24e6f1205 派发）
**日期口径**: 以当前工作树（未提交）为准；git 仅做只读 log/show 溯源，零写。
**责任域**: docs/contracts/ 19 文件、docs/modules/ 顶层 23 md + MODULE_MAP.yaml、docs/modules/registry/ 27 文件、docs/TRACEABILITY.csv（含与 docs/traceability/ 五件、docs/contracts/INDEX.yaml 的一致性）。
**纪律遵守**: 只查不改，本文件为唯一产出；科学公式/默认容差/冻结定义问题只登记上呈（红级+证据），不给越权改法。
**先读**: 独立审计/实验重做/总编对账/检查-修复验证.md PASS 表与 分歧台账.md D-01…D-11 终裁——本报告未重报任何 PASS 项（k_gauss 表、k_corr 行、TRACEABILITY ACR-IVAR/SCI-UPM-WEIGHT 两行、PERF 四条 record_and_justify→fail-closed 落地等均未重报），未翻任何 D 系旧案。

**计数：红 6 / 黄 11 / 绿 4（另有域外附带 1 条）**

---

## 一、红级（必须改；涉及容差/冻结口径者只登记上呈）

### 红-1 生产排异档位表：三份文档把「已作废的四档表」写成生产现状（面①②④）

- **文件:行**:
  - docs/modules/phase2_rej.md:46-48（「生产默认 profile astrocs_adaptive_pixel：n≤3→NONE、4..7→PERCENTILE、8..15→WINSORIZED、≥16→LINEAR_FIT」）
  - docs/modules/registry/astrocs.phase2.reject.md:43-44（同句）
  - docs/contracts/PUBLIC_API.md:1377-1378（同句，标注「生产默认 profile」）
- **问题**: 三处把四档路由表（4-7 percentile / 8-15 winsorized / ≥16 linear_fit）写成**生产默认 profile** 的现行路由；该表在仓内被明确判废，现行生产档是三档表。
- **证据（含反方核验）**:
  - 唯一正本：docs/ASTROCS_DESIGN.md:449「逐像素档位表（档界与算法名）的唯一正本 = docs/plugins/algorithms_phase2/12_rejection.md §9」；12_rejection.md:58-71 实际表 = **1≤N≤3 none / 4≤N≤5 percentile / N≥6 winsorized**，并在 :71 明写「档位表**由四档收成三档**」（M3 裁决，含 12.200%→0.067% 过拒实测）。
  - 实现：rejection.cpp:1154-1174 `astrocs_n_map_method`：1-3→NONE、n<6→PERCENTILE、6-15→WINSORIZED、**n≥16→WINSORIZED**（:1163-1173 注明「M3…n≥16 由 linear_fit 改投 winsorized_sigma」）；:1142-1143 注释逐字「原四档表（n≤3 不排异/4–7 percentile/8–15 winsorized/≥16 linear）**作废**」。
  - 科学正本 docs/science/REJECTION.md:63-65 同三档口径。
  - **反方核验（内部矛盾）**：同一份 phase2_rej.md:186-190 另一段「排异档位（权威 = docs/ASTROCS_DESIGN.md §5.5 / docs/science/REJECTION.md）」写的恰是 1≤N≤3 none / 4≤N≤5 percentile / N≥6 winsorized —— 单文档前后两说。
  - 对照档口径反证：PUBLIC_API:1379-1380 对 wbpp_2_9_1 的「n<6→PERCENTILE、6..15→WINSORIZED、>15→LINEAR_FIT」与代码一致，说明只有「生产默认」一句映射错了档。
- **建议改法**: 三处按 12_rejection.md §9 三档表改写「生产默认 profile」表述，删四档句或降级为「历史四档（已作废）」；改法归排异/文档 owner，本报告不代改（涉科学判据面，上呈）。
- **所属面**: ①（排异档位=冻结科学判据）+ ②（同文档两说）+ ④（文档与实现不符）

### 红-2 UNIT-DERIVE-01 后端口单位口径分裂：源码=面亮度、19 处文档端口表与 DATA_SEMANTICS 三节仍写 ADU（面③④）

- **文件:行**:
  - 源码（现行）: lib/infrastructure/scheduler/src/module_adapters.cpp:1074、:1092-1093、:1111-1113、:1131、:1151-1152、:1030-1031 —— samples / upm_model / calibrated_frames / corrected / stacked / fits 等端口 = `UnitId::SURFACE_BRIGHTNESS`。
  - 文档（未同步，仍 `UnitId::ADU`）:
    - 顶层层 7 行：phase2_rej.md:73、phase2_samp.md:82、phase2_upm.md:100-101、:107-109
    - registry 12 行：astrocs.phase1.writer.md:26-27、phase2.integrate.md:59-60、phase2.reject.md:63、phase2.sample.md:68、phase2.upm-fit.md:67-68、phase2.upm-apply.md:71-73、phase3.resample2.md:73
    - 合同正文：DATA_SEMANTICS.md §26:2302/:2315（corrected「单位=ADU」）、§23.2:1760（value「ADU」）、§25.3（σ「（ADU）」，:2086 一带）
- **问题**: 同一量（Phase2 面亮度域端口/控制样本值）在机器源与文档面量纲不一致：代码与机器合同说面亮度（ADU/sr 族），合同与模块页说 ADU。且 DATA_SEMANTICS **同文档内部** §31.1a 推导链（:3182-3184「Phase2 逐样本消费 = ADU/sr」「Phase2 输出 = ADU/sr」）与 §23.2/§25/§26 的「ADU」直接冲突。
- **证据（含反方核验）**:
  - commit 7cde39cb（2026-09-23「统一物理单位口径为 ADU/sr（UNIT-DERIVE-01）」）diff 逐行可见 `{"corrected"...UnitId::ADU}` → `SURFACE_BRIGHTNESS`、`{"samples"...ADU}` → `SURFACE_BRIGHTNESS`（端口修正 19 处），提交说明「数值/公式/容差零改动」「门 CHK-BLOCKFLOW-PORTS-VS-CODE 81/81 PASS」「DATA_SEMANTICS 新增 §31.1a 为正本」。
  - 机器合同 module_ports.registry.json unit_vocabulary 规则：「面亮度载荷一律 SURFACE_BRIGHTNESS…**禁止用 ADU 承载面亮度载荷**」；DATA_SEMANTICS §31.1 单位表 `phase2_mosaic_signal` FROZEN = BUNIT(声明)/ADU/sr。
  - 反方核验：(a) 该 commit 的 docs 改动面（stat）**不含** docs/modules/*、不改 DATA_SEMANTICS §23/§25/§26 —— git blame 显示 §26「单位=ADU」仍停在 2026-09-08（f506ef7f）；(b) 部分页已同步（registry astrocs.phase1.drizzle.md `stacked`=SURFACE_BRIGHTNESS 与代码一致）⇒ 是**漏改**而非有意保留；(c) DATA-P1-COS/DATA-P1-COSMETIC 别名差异已在 unified_object_registry.json:245-265 登记为占位名，不属本条。
- **建议改法**: **只登记上呈**：由 UNIT-DERIVE-01 归口人核对后统一文档面（19 处端口表 + DATA_SEMANTICS §23.2/§25/§26 字段级单位），或给出「ADU 为标度类别记法」的显式换算声明；量纲问题不由检查员改。
- **所属面**: ③（跨文档/文档↔源码口径冲突）+ ④（文档说的与代码不符）

### 红-3 PERF_GATE_CONTRACT 第 4 条把归档实测值写成冻结阈值、且积压极性相反（面④）

- **文件:行**: docs/contracts/PERF_GATE_CONTRACT.md:15「| 4 | 无「连续 ≥142 s 低利用窗且无积压」 | 无 | red |」
- **问题**: FROZEN 合同的四条 L2 冻结判据之四，阈值写成 **142 s**、条件写成「**且无积压**」；唯一数值源与实现均不是这个口径。
- **证据（含反方核验）**:
  - 唯一数值源 eng/contracts/resource_gate_v1.json：`compute.queue_low_window_seconds_min = 10`、`queue_low_utilization_percent = 60`、`queue_low_window_requires_queued_work = true`（G-RES-01 ② 要求**有**积压才硬失败）。
  - CI 裁决面 docs/ci/CI_SPEC.md:213-214（§9.2 判据4）:「无连续 **≥10s** 且利用率 <60% 的低利用窗（**无就绪积压同样计违规**）」；实现 eng/ci/l2_frozen_gate.py:264-267 阈值取 `thresholds["low_window_seconds"]`=10，日志字串「无就绪积压同样计违规——串行/停顿与 CPU 饥饿同属性能缺陷」。
  - **142.049 的真实身份**：eng/ci/check_frozen_gate.py:79-85 —— 「合成违规证据…数值取 RELEASE-04 real16_w16 归档实测」的 fixture 值；`grep -rn "142 s"` 全 docs 仅命中 PERF_GATE_CONTRACT.md:15 一处。
  - 两处偏差叠加：阈值虚高 14.2×（10s→142s），极性反向（正本=低利用窗一律违规；本文=只有「无积压」才算违规）。
- **建议改法**: **只登记上呈**（FROZEN 门禁阈值面）：按 resource_gate_v1.json + CI_SPEC §9.2 订正该行，或走差异单。
- **所属面**: ④（说明层与机器合同漂移）+ ③（与 CI_SPEC 对不上）

### 红-4 TEST_MATRIX §6 验收断言不成立，且没有任何检查器执行该断链校验（面④）

- **文件:行**: docs/contracts/TEST_MATRIX.md:65「每个 ACTIVE SCI/ALG 在 TRACEABILITY.csv 有 TEST 映射（DOC-004 校验断链）」
- **问题**: 断言双重落空——覆盖不成立（计数），所谓校验器不存在（DOC-004 不读 TRACEABILITY.csv）。
- **证据（含反方核验）**:
  - docs/contracts/INDEX.yaml 中 type=SCI/ALG 且 status=ACTIVE/FROZEN 的条目 = **46** 个；docs/TRACEABILITY.csv 共 67 行、`requirement_type` 全为 `science`；以 requirement_id 计只有 **10/46** 命中，**34 个 ID 在 TRACEABILITY.csv 全文零命中**（ALG-CAL-001、ALG-DRZ-001、ALG-UPM-001、ALG-P2-HIPS-001..004、ALG-STARDET-001、SCI-WCS-001、SCI-CW-001、SCI-P3-001 等）。
  - 反方核验①：docs/traceability/TRACEABILITY_MATRIX.csv 覆盖 science_id/algorithm_id 列 28/46，仍有 18 个 ACTIVE ID 不在矩阵——两表合起来也盖不满，且本条**点名的文件就是 docs/TRACEABILITY.csv**。
  - 反方核验②：DOC-004 = eng/tools/check_ast_api.py（AST/API 漂移检查），grep "TRACEABILITY" **零命中**；eng/tools/check_traceability.py 的 docstring 明记已于 2026-09-16 退役且校验的是另一格式（claim_id 表头、artifacts 路径）；eng/tools/traceability/check_traceability_matrix.py 只读 docs/traceability/TRACEABILITY_MATRIX.json——**没有任何在册检查消费「TRACEABILITY.csv 的 SCI/ALG↔TEST 映射」**。
- **建议改法**: 上呈：要么补齐 TRACEABILITY.csv 的 SCI/ALG 行并落一个真检查，要么把 §6 该句改为如实口径（指向真正被检查器覆盖的 MATRIX 面）；验收判据不允许空挂。
- **所属面**: ④（文档声称的门/行为在源码里没接）+ ③

### 红-5 DATA_SEMANTICS.md 实现锚系统性漂移（抽样 13/15 不中，内容全部存在于更大行号处）（面④）

- **文件:行**: docs/contracts/DATA_SEMANTICS.md 全文 269 处代码锚（.cpp/.h/.py:line），可解析 250 处；随机抽样 15 处实开文件核对，**13 处锚行内容与文档声明无关**。
- **问题**: 合同文档的「实现级证据」成片错位，读者按锚打开文件看到的是无关代码。
- **证据（抽样对照，doc行 → 声明锚 vs 实开内容/实际位置）**:
  | 文档行 | 声明锚 | 锚行实开 | 内容实际位置 |
  |---|---|---|---|
  | :434 | aio_hips_writer.cpp:399-401「<512 返回 NULL」 | `if (uniq.empty()) return true;` | nside<512 判定在 src/hips/aio_hips_writer.cpp:1196 |
  | :542 | snr_estimator.h:108-122「SnrNoiseModelConfig 结构体」 | 注释「star_ids/quality_flags 可空」 | 结构体在 :130-155 |
  | :892 | orchestrator.cpp:2179-2187「float→uint16 clamp」 | `result.exit_code = ...GENERIC_ERROR` | clamp 在 :2202-2206 |
  | :896 | sdet_api.cpp:2357-2372「sdet_free_detect_ex」 | 注释「饱和星现在直接从 peaker…」 | 函数在 :2946 |
  | :1899 | p2_session.cpp:250-266「manifest JSON 字段」 | `p2_upm_close(model);` | — |
  | :2110 | upm.cpp:256「precision u32」 | 函数参数行 | precision 写点在 :1394/:1530 |
  | :697 | wcs_transform.cpp:241-242「CRPIX 1-based 换算」 | `// 近似: u_orig = U - A(u, v)` | — |
  | :1644 | acr_kernels.cpp:218 | `#pragma omp parallel`（碰巧近似） | — |
  | :2010 | common_abi_v1.h:110-117 host services | `typedef struct astrocs_host_services_v1 {` | ✓ 命中 |
  | :2480 | p3_session.cpp:128 coverage_output | `const std::string covout = doc.value("coverage_output"…)` | ✓ 命中 |
  另抽 :371/:496/:964/:1752/:2121 共 5 处同样不中（仅上表 2 处命中）。
  - **共享锚三连错**: DATA_SEMANTICS:2551/:2630 与 PUBLIC_API:1990 均引 `module_adapters.cpp:406-423` 作 astrocs.phase3.wcs descriptor——实开 :400-425 是 P10 node trace 代码，descriptor 实际在 :774。
- **建议改法**: 机器重锚（脚本按符号/字串重定位 + 人工抽验）后统一刷新；不涉及任何数值/公式。
- **所属面**: ④

### 红-6 SCHEDULER_CONTRACT 把 PHASE2_UPM §7 冻结容差误述为「绝对容差」（面①③，只登记上呈）

- **文件:行**: docs/contracts/SCHEDULER_CONTRACT.md:30「跨 worker 数（1..N）= **1e-12 绝对容差**（引用 PHASE2_UPM.md §7）」
- **问题**: 被引正本冻结的是**相对容差**形式，两份 FROZEN 文档对同一档容差的「形式」两说；测试作者按哪份写判据会得到不同门。
- **证据（含反方核验）**:
  - 正本 docs/science/PHASE2_UPM.md:189/:191：「并行确定性容差（三档，冻结）…(b) 跨 worker 数（1..N）= **相对容差 rtol 1e-12**（判据 `|ΔC| ≤ 1e-12·max(|C|, C_scale)`，C_scale = 该次构建 C 场量级），不是位精确」——带 C_scale 下限的混合式，既非纯绝对也非纯相对。
  - 反方核验：SCHEDULER §2.1 自身后续段（:32）还在讨论「绝对容差的适用量级域、超出该域按同值相对形式判」，说明其确以「绝对」定性该档；TEST_MATRIX §2 通用档（rtol/atol）与本条不冲突，冲突点仅此一处定性。
  - 不涉及任何数值变更（rtol 1e-12 数值一致），冲突在**形式/判据式**的表述。
- **建议改法**: **只登记上呈**（默认容差口径）：由 PHASE2_UPM 正本 owner 裁决 SCHEDULER §2.1 引述写法；两份冻结文档不得各自为说。
- **所属面**: ①（容差口径）+ ③（跨文档冲突）

---

## 二、黄级（建议改）

### 黄-1 docs/modules 顶层层 + registry 的 descriptor 行锚全数漂移（面④）

module_adapters.cpp 的 descriptor 区段已从 362-587 重构到 **694-1170**（id 行实测：phase1:694-1024、phase2:1048-1164、phase3:732-832），全部旧锚失效：
- 顶层层：phase2_rej.md:20/:68（700-717/707-710 → 实际 1125/1131-1132）；phase2_samp.md:23/:78（642-654 → 1067/1073-74）；phase2_upm.md:35-36/:64-65（661-694/618-632 → 1086/1105/1111-1113）；phase3_proj.md:72、phase3_rsmp.md:23/:80、phase3_fits.md:31/:89（406-423/362-380/425-439/445-460 → 774/730-732/793/813）；healpix_drizzle.md:98/:23（根 CMakeLists.txt:356-366/379-382 现为 RPATH/output-name 段）。
- registry 页锚出现频次前 9 全部指向旧区段：`:642-654 ×2、:661-678 ×2、:680-694 ×2、:406-423 ×2、:425-439 ×2、:445-460 ×2、:570-587、:1040-1057、:816`。
- **反方核验**: 内容本身均在（新行号可定位），属行漂移而非幻觉；CHEK 门（check_block_flow_ports_vs_code.py）校验的是 registry json 的 code 锚，不校验 md 里的行锚 ⇒ 这些漂移无门兜底。
- **建议**: 随一次「模块页重锚」批处理统一刷新，并考虑给 md 行锚加机器校验。

### 黄-2 DATA_ARTIFACTS.md §1.1 六处锚漂移 + DATA-TILE-001 在机器注册表零命中（面④）

- 声明 vs 实开：module_adapters.cpp:498-499/:459/:498（实际 hips/tile descriptor 在 :738-739/:761/:800）；module_ports.registry.json:246/:277（该两行是 DATA-P1-WCS/DATA-P1-COSMETIC，DATA-HIPS-001 首现 :751）；docs/modules/registry/astrocs.phase3.resample.md:22（实际 :26）、resample2.md:71（实际 :73）、properties.md:22（实际 :26）；docs/modules/phase3_rsmp.md:75（实际 :77）。
- 反方核验命中项：TRACEABILITY_MATRIX.csv:25/:31 两行 DATA-TILE-001/DATA-HIPS-001 VERIFIED ✓、core_pipeline_test.cpp:66-67 ✓。
- 额外：声称「registry 端口表早已在 …:246/:277 使用 DATA-HIPS-001/DATA-TILE-001」—— `grep DATA-TILE module_ports.registry.json = 0 命中`（DATA_SEMANTICS:3017 已自认「DATA-TILE-001 声明未登记」）。DATA_ARTIFACTS:66-70 的既存 ID 论证对此 ID 不成立。
- **建议**: 重锚；「已登记」措辞改为「DATA-HIPS-001 已登记（:751 起）、DATA-TILE-001 尚未进入端口注册表」。

### 黄-3 UNIFIED_OBJECTS.md §4a 证据锚系统性漂移（面④）

抽 11 处全部内容存在、行号错位（DATA_SEMANTICS:133→§9 实际 :172/:174、:181-182→:260/:262、:226-228→:337/:339；PUBLIC_API:102→:140、:152/:177→:190/:215、:214→:255 一带；DATA_ARTIFACTS:29→:31；MODULE_MAP.yaml:151→:154；registry astrocs.phase1.calibration.md:23→:27；calibration.md:8/:53→:10/:54；gaia_xpsd_client.md:23/:40→:25/:42）。偏移 +2 到 +111 不等（DATA_SEMANTICS 增补最甚）。
- **建议**: 与红-5 一并机器重锚。

### 黄-4 PUBLIC_API.md 锚抽样约半数漂移（面④）

27 处抽样中约 12 处不中（stage2.cpp:592 声明 aio_hips_product_begin → 实为字符串拼接；module_adapters.cpp:492-510「编排层词汇」→ 实为几何运算；upm.h:154-156「缺陷迁移语义」→ 实为 W2 接口注释；sdet_api.cpp:1612、stage2.cpp:1305、upm.cpp:256 等）；命中侧同样存在（p1_session.h:16/:19/:23/:36、p2_session.h:25、dll_loader.cpp:59、common_abi_v1.h:110、acr_kernels.cpp:218、p3_session.cpp:179 ✓）。漂移率低于 DATA_SEMANTICS，但显著高于零。
- **建议**: 纳入同一次重锚批次，优先重锚 §API 签名表段。

### 黄-5 源文件行数声明漂移：registry 19/26 条错，且同一文件三个互斥值（面②④）

- 实测 vs 声明（registry）：rejection.cpp 2956 vs 2949；rejection.h 604 vs 595；sampler.cpp 1503 vs **1536**（声明>实测）；sampler.h 273 vs 288（声明>实测）；upm.cpp 2981 vs 2793（×4 处）；upm.h 450 vs 384（×5 处）；integrate.cpp 89 vs 81；integrate.h 83 vs 74；stage2.cpp 2015 vs 1965；p3_resample.h 202 vs 201；**astrocs.phase3.writer.md:34 p3_wcs.h「166 行」实为 373**（与同行 p3_output.h=166 撞数，疑复制错）。
- 同文件三值互斥（②面）：sampler.cpp = phase2_samp.md:35「1156」/ registry「1536」/ 实测 1503；upm.cpp = phase2_upm.md:44「1565」/ registry「2793」/ 实测 2981；upm.h 同页两说 **:46「184 行」 vs :132「384 行」**（实测 450）；rejection.cpp = phase2_rej.md:34「2076」（其 :56 另称「ALG §3 实测 2076/329」）/ registry「2949」/ 实测 2956。
- 反方核验：phase3 面大半准确（p3_output.cpp 1082 ✓、p3_output.h 166 ✓、test_p3_resample.py 164 ✓、p3003 141 ✓、p3_interp 137 ✓）⇒ 不是计数方法问题，是陈旧快照。
- **建议**: 行数声明改由脚本生成或删除具体行数（保留「见该文件」）。

### 黄-6 死路径群：文档引不存在的文件（内容在别处）（面④）

- `eng/ci/ledgers/module_page_evidence.json` —— 10 个 registry 页把它当「证据源」（phase1.writer:44/:56 等「证据源 …无 …条目」），**全仓不存在该文件**（eng/ci/ledgers/ 下无）。NOT_VERIFIED 结论本身保守，但证据源是死锚。
- `eng/tests/unit/p3_wcs_test.cpp`（astrocs.phase3.wcs.md:165 称 474 行）与 `eng/tests/backend/p3_wcs_main.cpp`（:167）—— 不存在；实际在 lib/algorithms/projection/tests/p3wcs/p3_wcs_test.cpp（build 树可证）。docs/modules/phase3_proj.md 同引。
- `eng/tests/backend/p3_resample_probe_main.cpp`（resample2:190、phase3_rsmp.md 同引）—— 实际在 lib/algorithms/resample/tests/p3rsmp/。
- star_detector.md:90 `lib/include/star_detector.h` 不存在（同文档 :32 给的是正确路径 lib/algorithms/star_detection/include/star_detector.h ⇒ 单文档两说）；common.md:65 `lib/include/astro_scalar.h`、`lib/include/precision_context.h` 不存在（同文档 :19-20 已给正确路径 lib/algorithms/shared/include/）；plate_solve.md:63 `lib/include/ipv_api.h` 不存在（实际 lib/algorithms/platesolve/cpp/ipv/include/ipv_api.h）。
- **建议**: 迁移后路径统一刷新；同一文档内新旧路径并存处删旧留新。

### 黄-7 CONFIG_CONTRACT 计数与现状漂移 + 机器登记册内部不一致（面③④）

- §2 标题「defaults.json（**50 字段**，2026-09-17 快照）」：实测 `field_count=59`；分组表 calibration 1→实测 2、**weight 行（1 字段）在 defaults.json 已无任何 weight.* 键**（§9.73 A44 作废后残留），而 :36/:67 仍写「`weight.default_mode` 另有 enum_token/enum_target…首例」——**该键不存在**，现 defaults.json 的 enum 首例是 snr.path（:713，enum_token=sparse_reconstruct）；cosmetic 6 / paths 1 / snr 1 等新组未入表。（§2 自注「总数是快照、权威以 field_count 为准」，故总数本身有对冲，但分组表与 weight 首例是实打实的死引用。）
- §8:187-189 / §9:204-205 / §8.1「117 行 = 23 篇、none 60 / gap 25 / unregistered 32、science_param 82」：实测 plugin_knobs **120 行**（23 篇 ✓）、none 68 / gap 21 / unregistered 31、science_param 85（by_owner/by_registration/by_finding 三套 totals 与实测一致，唯 totals.plugin_knobs=**117 ≠ len=120**——机器登记册自身有个不被 CFG002-01 重算的陈旧标量，check_cfg002_registry.py:208-217 只校 rows/by_* 三键）。
- §8/§8.1/§12「eng/ci/checks.json 尚无 UT-CONFIG 步骤（交接）」：**checks.json:2762 已有 UT-CONFIG**（unittest discover -s eng/tests/config）；但 docs/ci/01_CHECKS.md §2 仍无 UT-CONFIG 条目 ⇒ 「双向对齐」只落了一半，本文件的「尚无」已失实。
- 反方核验（该文件做对的）：CFG002 全套锚（test_cfg001_contracts.py 类与 test_no_aggregate_second_definition:117、gate_assertions.py G4:89、config_registry 结构、filters 45/空 channel 7/44-45 记录、os_abi 负例 fixture）逐一存在；死键台账 eng/ci/ledgers/dead_config_keys.json 存在。
- **建议**: §2 分组表与 weight 首例、§9 实测分布、§8/§8.1/§12 的 UT-CONFIG 现状按实测刷新；config_registry totals.plugin_knobs 由登记册 owner 修正（机器侧，只登记）。

### 黄-8 API-001 §5 基线数字与家族声明失实 + API_CONTRACTS test_ids 全表悬空（面④）

- API-001.md:66「API_CONTRACTS.csv 登记 **423 行**现有 C ABI（aio_*/p2_*/**p1_*/p3_**），状态 VERIFIED」：实测 381 数据行（382 含表头），**历史版本（9902c788、c3ce041f）也是 382 行**——423 从未成立；`grep p1_/p3_ = 0 命中`，两家族不存在（实际家族 aio 106、p2_ 35、ipv 17、snr 17、sdet 9…）。
- 附带：全表 381 行 `test_ids` 均为 **TST-GEN-001**，该 ID 在 docs/、eng/（除本 CSV 与其检查器 fixture）**零定义**——测试映射无处可查，却整表标 VERIFIED。
- **建议**: 行数/家族按实况改写；TST-GEN-001 要么登记到某个测试索引、要么在 DOC-004 口径里声明其为占位。

### 黄-9 INDEX.yaml 覆盖完整性缺口（面④）

- 反向覆盖：docs/contracts/ 实有 19 文件，**9 个从未在 INDEX.yaml 出现**：CONFIG_CONTRACT.md、DUAL_LINE_CONTRACT.md、HIPS_STORAGE_FORM_CONTRACT.md、LOG_AND_ERROR_CONTRACT.md、PERF_GATE_CONTRACT.md、PIPELINE_BLOCK_CONTRACT.md、RT-001.md、SCHEDULER_CONTRACT.md、config_separation_anchors.json（UNIFIED_OBJECTS.md/ unified_object_registry.json 仅以 note 字符串出现，未作条目 path）。
- 同族混合收录（更刺眼）：TEST-001 在册，而 TEST_MATRIX 定义的 TEST-CAL-001 等 **9 个 TEST ID 不在**；API-001/ARCH-001 在册，PUBLIC_API 的 API-CAL-001 等 **21 个 API ID 不在**；DATA-OBJ-* 在册，DATA_ARTIFACTS 的 DATA-IMG-RAW-001 等 **20 个 DATA ID 不在**；合计 contracts 面定义的 175 个合规 ID 中 95 个未入索引。
- 反方核验：INDEX.yaml 自述生成面=「docs/science 与 docs/algorithms 的 ID」，且 contract_index.schema.json id 正则只收 (SCI|ALG|DATA|ARCH|API|MOD|TEST|TST) 家族——CFG-001/LOG-004/RT-001/CONTRACT-501-* **结构上进不来**；即「Contract Index」实际不覆盖 contracts 目录的合同文档，覆盖口径需要一纸明确。图质量本身：116 条目、upstream/downstream 引用 98 个、**悬挂 0**；116 个 path 中 115 个实存（唯一空 path 属 OBSOLETE 的 psfsw 退役留痕，合法）；docs/DOCUMENT_INDEX.yaml 对 19 文件覆盖 19/19 ✓。
- **建议**: 明确 INDEX 的覆盖口径（若只管 SCI/ALG 图则改名/加 scope 声明）；若管 contracts 面则补 9 文档 + 同族 95 ID 或登记豁免。

### 黄-10 HIPS_STORAGE_FORM_CONTRACT §1「冻结四件事」实列六项（面②）

- HIPS_STORAGE_FORM_CONTRACT.md:14-21：「本合同冻结**四件事**，四者都是机器可校验的：」随后编号 1..6（新增 §7 体积削减、§10 形态键两件）。开头句与枚举自相矛盾，范围声明失真。
- **建议**: 改「六件事/若干件」并在首句点名新增两项。

### 黄-11 PIPELINE_BLOCK_CONTRACT §2 的 block unit 示例词表与机器注册表词表两套、无互引（面③④）

- PIPELINE_BLOCK_CONTRACT.md:31：`unit`「物理单位（ADU / ADU² / e⁻ / pixel / 无量纲 / null）」；机器侧 module_ports.registry.json unit_vocabulary tokens = **ADU / SURFACE_BRIGHTNESS / DIMENSIONLESS / DEGREE**（且规则「面亮度载荷一律 SURFACE_BRIGHTNESS」「禁止用 ADU 承载面亮度载荷」）。两套词表既不互引也不等价：示例表无面亮度 token（UNIT-DERIVE-01 后的主载荷），却列 e⁻（该口径已被判「全链无电子换算」）。
- 反方核验：eng/contracts/schemas/pipeline_block.schema.json 对 `unit` **无 enum 约束**（仅 dtype/lifecycle 有），所以机器面没红——漂移只在说明层，无门兜底。
- **建议**: §2 词表改为引用 unit_vocabulary（或注明「bunit 字符串面」并给映射），e⁻/pixel 逐项说明适用面。

---

## 三、绿级（可不改）

1. **绿-1（②）** DATA_SEMANTICS.md 章节序：`## 29`（:2439）排在 `## 28`（:2536）之前，编号逆序（交叉引用按号可解析，仅观感）。
2. **绿-2（④）** UNIFIED_OBJECTS.md:74 引 `docs/contracts/unified_object_registry.json#port_contract`——实际键名为 `port_contracts`（:371）与 `port_contract_ref`（:395），JSON Pointer 严格解析会落空；语义内容齐全。
3. **绿-3（④）** module_id_migration_baseline.json:34 历史条目称「另 **9 页**无 MODULE_MAP 对应」但只点名 8 个（缺 astrocs.phase3.resample2.md）；实测无 module_id 页恰 14 = 5（被行锚锁定）+ 9 ✓，仅枚举漏名。
4. **绿-4（④）** HIPS_STORAGE_FORM_CONTRACT:145 引 `run/RULING-DOC-01/REPORT.md` 裁决 C——run/ 为可回收目录（round_start 会清），现工作区该路径已不存在；同数值（13.3%）在 docs/design/PRODUCT_STORAGE_FORM.md §9 与 artifacts/evidence/compress-01/ 有稳定锚，建议合同只引后者。

## 四、域外附带登记

- **域外·黄（④，源码注释）** lib/algorithms/coverage/src/rejection.cpp:1142-1143 注释称「原四档表…作废（**docs/ASTROCS_DESIGN.md §4.5 已加作废横幅**）」——docs/ASTROCS_DESIGN.md 全文 `grep 作废 = 0`，且 §4.5 实为「运行前预检」；作废事实的真锚在 docs/plugins/algorithms_phase2/12_rejection.md:71。注释本身需改锚（与红-1 同源，供 A 线派单）。

---

## 五、已查无问题面（逐面说明查过且未发现问题的部分及抽查方式）

### ① 科学性（常数/公式/单位/量纲/适用域；对 D 系终裁）

- **D 系对数**：对照分歧台账 D-01…D-11 逐条扫 contracts/modules 相关表述——外接半径 1.0415+20% 裕量（healpix_drizzle.md:30 仅引 HP_CIRCUMRADIUS_FACTOR=1.25，无 1.1284/1.044 旧值残留）、k_gauss 全表 1.637/1.316/1.144/1.083/1.046（DATA_SEMANTICS:1739 已按红-7 PASS 形态、无 1.26 正文残留）、k_corr 1.4=实现记录/D-08 两因子查表（TEST_MATRIX:41、DATA_ARTIFACTS:119、phase2.md:76、TRACEABILITY.csv:3 均已带 D-08 口径）、idw_power=1.0（DATA_SEMANTICS:1409 引 D-05）、depth≥9 生产=12、P4 默认算子、Aitken DOI（ARCH-001/DATA_SEMANTICS:180 均为已核状态+订正注）——**未发现翻案或与终裁相左的新表述**。
- **常数/公式抽验**：HIPS 合同 zstd level 3 依据、2.77%/13.31% 与 docs/design/PRODUCT_STORAGE_FORM.md §9 逐值一致；LOG 合同 4096 字节上限两处一致；exit code 11 码与 lib/infrastructure/cli/exit_codes.h 逐码一致（含 10=磁盘、70=internal），ErrorDomain 8 域与 core/contracts.h:17-26 逐字一致；GAIA「12 个 GAIA_EXPORT」= gaia_client.h 实际导出函数 12 个（:82-131，20 次命中含宏/注释）；TEST_MATRIX §2 通用容差（rtol1e-12/atol1e-13、5e-6/1e-6）与 SCHEDULER §2.1 引用一致。
- **负例/判据非退化**：LOG §9 R1-R5、PIPELINE_BLOCK §6、HIPS §9（18/18 反例矩阵）、PERF §5 三注入、SCHEDULER §6 均为「能红」形态并给出注入方式，无恒真门表述（唯一空挂门是红-4 的 TEST_MATRIX §6）。
- 局限：**文献外部核验未能完成**——emva.org 标准页与 PDF 均 404、web_search 本轮返回空源，故仅做仓内交叉对照（EMVA/GUM/FITS 4.0/WD-HiPS 引文与 docs/research、docs/design 同源一致），外部一手核验留待有外网的轮次补做。

### ② 行文逻辑

- UNRESOLVED/「待裁决」关键词：docs/contracts/DATA_SEMANTICS.md、PUBLIC_API.md、docs/modules/registry/26 页 **全域零命中**（脚本扫）——无未决混充结论。
- 订正注（`<!-- 订正 -->`）：contracts 内仅 3 处（CONFIG_CONTRACT:52、DATA_ARTIFACTS:120、DATA_SEMANTICS:1739），逐条读过，均为「新口径+旧对照」闭合形态且与 PASS 表一致，无新旧两说。
- 单文档内部矛盾除红-1（phase2_rej 两说）、黄-5（upm.h 184/384 等）、黄-6（star_detector/common 新旧路径）、黄-10（四件事/六项）外，其余通读未再发现。
- registry 26 页 front-matter（id/upstream/downstream）齐备，无缺头页（脚本全查）。

### ③ 跨文档冲突

- **TRACEABILITY 三方**：docs/traceability/TRACEABILITY_MATRIX.csv ↔ .json 按 module_id 键逐列比对 **0 漂移**（30 行、列名 24 个一致）；traceability_warn_baseline.json entries=6、schema/键齐全；TRACEABILITY_SPEC:93/:117 已如实声明「docs/TRACEABILITY.csv 沿用各自格式，不由本合同重写」——两表分工有明文，非隐性冲突。
- **docs/TRACEABILITY.csv 质量**：67 行、impl 文件 100% 存在、test_files 100% 存在、**implementation_symbols 136/136 在声明文件内正则命中**、release_gate=PRE_RELEASE_ENGINEERING_FOUNDATION 在 docs/standards/RELEASE_STANDARD.md:12 有定义；上轮已修的 ACR-IVAR-001/SCI-UPM-WEIGHT-001 行未回退。
- **权威链五方**：module_ports.registry.json（v2、20 modules、carrier_contract 齐）↔ module_adapters.cpp 双向由 CHK-BLOCKFLOW 门覆盖；docs/modules 与 registry 页的端口/单位面冲突已全量计入红-2/黄-1（除此外无新增口径冲突）。
- config_separation_anchors.json：两条 machine_assertions 在 test_unified_object_contract.py:647/:660/:678 实存、star_detection_config $defs 实存、聚合 phase_config.schema.json 确未创建（与 CFG-001 回归锁一致）、gate G4 实存。
- UNIFIED_OBJECTS/registry：13 canonical 对象 = eng/contracts/schemas/unified/ 恰 13 个对象 schema（+port_contract 单列）、4 条机器断言方法全在 test_unified_object_contract.py（:108/:128/:142/:176）、6 个负例文件 n1-n6 齐、canonical_object_classes=13、legacy 映射方法（:786/:832）在——U-02 结论的机器面闭合。

### ④ 幻觉与锚（file:line 实开、文档说有代码没接）

- **docs/modules 入口/注册表对表**：MODULE_MAP 23 模块、entrypoint=`astrocs_module_query_v1` 的 22 个与 lib/**/module_entry.* 实现对账（有实现 5 模块 ↔ declared_absent missing_implementation 17 模块）——**UNDECLARED_ABSENT=0、STALE_DECL=0**；`declared_absent_paths` 双向对账：实际缺失 40 条路径 **全部在册**、在册 65 条 **无一已落地未删**（按其自身「缺口落地即删」规则）；entry 符号 astrocs_module_query_v1 在 secure_loader.c/module_entry.* 可解析。
- **registry 逐条快查（27 文件全覆盖，不跳页）**：front-matter 26/26 有 id；module_id 声明 12 页且全部 ∈ MODULE_MAP（0 个未知 module_id）；端口表 46 行逐行与 descriptor 比对（差异全部计入红-2；2 处「缺端口」经复核为解析器多行误报，photprov 实际在 :945/:968/:1002，非问题；DATA-P1-COS vs DATA-P1-COSMETIC 属 unified_object_registry.json:245-265 已登记占位名，非问题）；行数声明 26 页全查（→黄-5）；路径引用 211 处全查（→黄-6）。
- **INDEX.yaml**：116 条目 path 实存 115/116（唯一空 path 为 OBSOLETE 退役留痕）、图闭包 0 悬挂、schema id 正则核对（→黄-9 覆盖口径）。
- **锚抽样规模**：DATA_SEMANTICS 15、PUBLIC_API 27、UNIFIED_OBJECTS §4a 11、DATA_ARTIFACTS §1.1 8、modules 顶层/registry 描述子锚 20+、TRACEABILITY 136 符号 + 全量文件存在性；命中/不中已分别计入红-5、黄-1..黄-4。
- **未做成的**：外部文献题录抽验（见①段局限）；未运行任何构建/测试/门（纪律），机器门结论均以读代码方式推断并注明。

---

**复现口径**：本报告全部结论可用只读命令复现（python3 只读解析 + grep/sed 实开 + git log/show 只读溯源）；未写入除本文件外的任何路径，未执行任何 git 写操作。
