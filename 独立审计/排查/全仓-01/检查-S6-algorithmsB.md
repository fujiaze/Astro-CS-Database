# 检查-S6-algorithmsB：docs/algorithms 乙组（PHASE2/PHASE3＋anchors）对抗性静态复查

> 责任域：docs/science/algorithms/ 下 15 个文件（INTEGRATION_ALGORITHMS.md、PHASE2_COVERAGE.md、
> PHASE2_INTEGRATION.md、PHASE2_MOSAIC_WRITE.md、PHASE2_REJECTION.md、PHASE2_SAMPLER.md、
> PHASE2_SESSION.md、PHASE2_UPM_IMPL.md、PHASE3_FITS_IMPL.md、PHASE3_PROJ_IMPL.md、
> PHASE3_RESAMPLE.md、PHASE3_RSMP_IMPL.md、anchors/ANCHOR_CONTRACT.md、
> anchors/anchor_contract.json、anchors/unresolved_registry.json）。
> 状态：HEAD=52adba89（2026-09-27），git status 仅本排查目录未跟踪、零源码修改。
> 方法：逐文件读 + 逐锚开源核验（sed/grep -E/read 工具）、两台机器门只读复跑
> （PYTHONDONTWRITEBYTECODE=1、不带 --json-out，不产生任何写盘）、D 系列终裁比对、
> 文献 DOI 经 Crossref API 核验。零 git 写、零构建、零测试。
> 纪律：检查-修复验证.md PASS 表已通过项（红-1..8 除红-4 残留、黄-1..16 除黄-14、
> 行漂移 6 条、观察项 NOISE_MODEL/08_修复包/PHASE2_UPM_IMPL 嵌套注释等）一律未重报；
> 科学公式/默认容差类问题只登记红＋证据，不代改。

---

## 一、机器门证据（HEAD 只读复跑，均为 exit 1 / RED）

| 门 | 注册 | 结果 | 本责任域贡献 |
|---|---|---|---|
| ALG-LINE-ANCHORS | eng/ci/checks.json（waivable:false，fast/linux-main/windows-main） | **149 findings（L1:72 + L3:77）** | L1 29 条（8 个 PHASE 文档行数锚全错）；L3 77 条（UPM 58 / REJECTION 15 / INTEGRATION 3 / SAMPLER 1） |
| DOC-LINE-ANCHORS | 同上注册 | **97 findings（C2:11 C4:7 C5:1 C6:77 C7:1）** | C7 1 条（ANCHOR_CONTRACT §1 规模行）；C5 1 条（PHASE2_SAMPLER 豁免）；C4 1 条（P3RSMP-DESCRIPTOR）；C6 域内 ≥18 处；C2 11 条均在域外文档 |

复跑命令：
```
PYTHONDONTWRITEBYTECODE=1 python3 eng/tools/doccheck/check_doc_line_anchors.py   → REAL_EXIT=1
PYTHONDONTWRITEBYTECODE=1 python3 eng/tools/doccheck/check_alg_line_anchors.py   → REAL_EXIT=1
```

---

## 二、问题清单

### 红

---

**S6B-R01 / 红 / 全域（两台已注册机器门）/ ALG-LINE-ANCHORS 在干净 HEAD 全红，12 个域内文档的"实测"行数锚几乎整片过期**

- 问题：L1（`<file>（N 行` 计数锚）72 条中 29 条落在本域，8 个文档的行数声明全部与源不符，且文档均自称"实测/复测"。
- 证据（门输出 ↔ `wc -l` 实测）：
  | 文档:行 | 声明 | 实测 |
  |---|---|---|
  | PHASE2_INTEGRATION:6/:50/:183/:323/:324 | integrate.cpp 81 行 / integrate.h 74 行 | **89 / 83** |
  | PHASE2_MOSAIC_WRITE:11/:439 | stage2.cpp 1965 行实测 | **2015** |
  | PHASE2_REJECTION:6 | rejection.cpp 2949 行 | **2956** |
  | PHASE2_REJECTION:61/:506/:615/:729 | rejection.cpp 2950 行（同文与 :6 的 2949 矛盾） | 2956 |
  | PHASE2_REJECTION:8/:61/:730 | rejection.h 595 行 | **604** |
  | PHASE2_SAMPLER:15/:89 | sampler.cpp 1536 行 | **1503（-33）** |
  | PHASE2_SAMPLER:16/:89 | sampler.h 288 行 | **273（-15）** |
  | PHASE2_UPM_IMPL:10/:72 | upm.cpp 2793 行 | **2981（+188）** |
  | PHASE2_UPM_IMPL:11/:72 | upm.h 384 行 | **450（+66）** |
  | PHASE3_RSMP_IMPL:14/:57 | p3_resample.h 201 行 | **202** |
  | PHASE3_FITS_IMPL:51 | p3_wcs.h 166 行"实测" | **373** |
  | PHASE3_PROJ_IMPL:16 | p3_wcs.h（166 行…行数按新址复测） | 373 |
- 反方核验：PHASE3_RESAMPLE 的 p3_resample.cpp 586 行、p3_nan_mask_test 437、test_p3_resample.py 164、test_p3003 141、p3_interp_test 137、p3_coverage_test 224（§3）、PHASE2_COVERAGE 的 coverage.cpp 455/coverage.h 172、PHASE2_SESSION 的 p2_session.cpp 318/39、PHASE3_FITS 的 p3_output.cpp 1082/h 166、PHASE3_PROJ 的 p3_wcs.cpp 593、PHASE3_RSMP 的 p3_session.cpp 441 均实测相符——**不是全错，而是"混代不更新"**；但凡错处都伴随"实测/复测"字样。
- 建议改法：按现行源统一重锚（同提交内更新，ANCHOR_CONTRACT §2.1/§5），或把"实测"降格为"锚定于某版本"并注明复测日期；行数声明逐条 grep -n 复核。
- 所属面：④

---

**S6B-R02 / 红 / docs/algorithms/anchors/ANCHOR_CONTRACT.md:28 及关联 / DOC-LINE-ANCHORS 全红：C7 规模行过期 + C5 陈旧豁免 + C4 绑定违规**

- 问题与证据：
  1. **C7**：ANCHOR_CONTRACT.md §1 声明"101 文档 / 1821 锚 = 1786 + 9 豁免 + 26 未解析"，门实测 **[105, 2102, 2059, 6, 26]**——豁免数 9 vs 实际仍生效 6（3 条已失效），规模基准整体过期，C7 规则直接判红。
  2. **C5**：`anchor_contract.json:175` 豁免项 raw `ledger.md:250`（PHASE2_SAMPLER 的 FROZEN_SCI_CLAIM）判 STALE（缺口 18 行）；该 ledger 为 run/local/bughunt 运行账本（reason 自述"属 run/ 运行产物"）。
  3. **C4**：PHASE3_RSMP_IMPL 绑定 P3RSMP-DESCRIPTOR 锚 `module_adapters.cpp:498` 不再含目标符号——`p3_resample2_descriptor` 现居 **:791/:15517**（BINDING_VIOLATION）；同时 PHASE3_RSMP_IMPL:40 自称"`grep -n` 实测 `:783`"亦错（实测 :791）。
- 反方核验：unresolved_registry.json 26 条（23 EXTERNAL_REFERENCE + 3 HISTORICAL_NARRATION）、owner_summary 23+3=26、max_entries=26 自洽，C8 无 STALE_REGISTRY 命中；3 条 HISTORICAL_NARRATION 指向的 api.cpp 行锚仍存活（ANCHOR_CONTRACT §6 正文仍引 :88-92）——**未解析登记本身干净，红在 C7 规模行与 C4/C5**。
- 建议改法：按实测重刷 ANCHOR_CONTRACT §1 规模行；C5 豁免改指稳定锚或撤销；P3RSMP-DESCRIPTOR 绑定重锚 :791（并同步 PHASE3_RSMP_IMPL:40）。
- 所属面：④

---

**S6B-R03 / 红 / docs/science/algorithms/PHASE2_INTEGRATION.md:6,:50,:183,:323-324 及 §3/§5/§9 表 / 行数与逐符号锚系统性过期且四处自称"实测"**

- 问题：行数见 R01；§3 逐符号表整片错位（列 doc→实际）：
  | 内容 | 文档锚 | 实际 |
  |---|---|---|
  | 权重门 | :44-48 | **:51-57**（:44-48 是 B2-A7 注释） |
  | 零权分支 | :49 | **:56** |
  | 累加 vs/wsum | :52-53 | **:59-60** |
  | n_used | :56 | **:61** |
  | rc 同步 | :58-63 | **:66-69** |
  | 无正权分支 | :65-69 | **:70-81** |
  | signal/support/OK | :70/:71/:72 | **:83/:84/:85** |
  | 函数体 | :19-74 | **:19-87** |
  | h 枚举 P2PixelResult | h:45-51 | **h:54-59** |
  | h canonical reducer 文本 | h:17-19 | **h:26-28** |
- 内部两版冲突：§3 表记 signal :70，§5 伪码（:121-123）记 :75/:76/:77——同文两个值，**实际第三值 :83/:84/:85**。§5 其余锚（:34-61/:66-69）与 §7/§10/§11.3 的 integrate.cpp:49-50/:51-57/:56 实测相符（混代证据）。
- 特别注意：`integrate.h:17-19` 被 §7/§10/§11.3 反复引用，实际 canonical 注释在 :26-28；源码 integrate.cpp:48 自己的注释也写着"integrate.h:17-19"（**源侧同漂移，一并登记**）。
- 反方核验：:182-184 的"实测文件为 81 行/74 行（行号以 §3 为准）"正是本条失实点——它把过期值冠以"实测"并指路给同样过期的 §3。
- 建议改法：全表重锚 + 消除 §3/§5 两版；SCI §5:63 的 10-79 声明按既有流程另行订正（本条不触科学语义）。
- 所属面：②④

---

**S6B-R04 / 红 / docs/science/algorithms/PHASE2_COVERAGE.md:89-126,:164-173（§3 伪代码）与 §1/§2/§11.1 / §3 行号锚非单调自相矛盾 + 要点锚过期，L3 存在性门全部漏检**

- 问题（§3 伪代码侧栏锚）：`:176-178 → :215-218 → :227-233 → :194-196 → :204-214` **非单调**；filter 行 `:215-218` 与 mismatch `:219-225`、回填声称 `:216-218/:219-224` 同行段双重认领——同一行被两个步骤认领，论证链不可复核。
- 要点锚实测（sed 逐一开源）：
  | 内容 | 文档 | 实际 |
  |---|---|---|
  | p2_coverage_build | :144-231 | **:173-272**（5 个历史修订 eaf32aad/9a2b5d11/ee99ca26/7cde39cb/fc34a9cf 中 build 恒在 :173） |
  | inspect_frame | :59-140 | **:60-169**（命名空间 :56-171） |
  | 匿名 namespace | :55-142 | **:56-171**（:55 是空行） |
  | parse_props | :20-45 | **:21-46** |
  | frame_id/max_leaf_order/field 回填 | :113-118/:119/:120,:136 | **:139-143/:144/:145,:165** |
  | target_order union | :194-196 | **:236-237** |
- 反方核验：hips_ordering :126-133 实测恰好相符（:126-133）——again 混代；L3 门因 presence-only（符号落即过）对上述范围错位一条未报。
- 建议改法：§3 侧栏按执行序重锚并消除双重认领；§11.1/§2 按现行源重测。
- 所属面：②④

---

**S6B-R05 / 红 / PHASE2_MOSAIC_WRITE.md:214,:284,:367 与 PHASE2_INTEGRATION.md:128,:133-136 / 把 §9.73 A44 已删除的 `legacy_allow_weight_fallback` 写成现行降级键（文档描述代码中不存在的行为）**

- 问题：两文档均把该键当作**仍可生效**的降级开关：MOSAIC:214 "缺失>0 且 !legacy_allow_weight_fallback → rc=7"、:284 ">0 且 `legacy_allow_weight_fallback=false` → rc=7"、:367 负测以该键为条件；INTEGRATION:133-136 "`ivar_valid?ivar:support` 是降级键 `legacy_allow_weight_fallback=true`（默认 false）时的显式降级路径（stage2.cpp:1113-1120）"。
- 证据（反方核验，逐条开源）：
  - stage2.cpp 实测 ivar 门在 **:783-807**：`if (ivar_product_missing > 0)` ⇒ 注释"§9.73 裁决 A44：原 legacy_allow_weight_fallback=true 的 support 降级分支已删除"、"ivar 产品缺失**恒** fail-closed"、`return 7;`——**无任何按该键分支的代码**；:1113-1120 现为 ACR compact weight 构造（"原 weight_mode=0 分支已删除"注释）。
  - 全仓 grep：该键只在 module_adapters.cpp:11375-13330（"已按 §9.73 A44 删除：出现即拒绝"）与 cli/parser.cpp:325（"已摘除"）出现——配置面出现即报错。
  - 文档自相矛盾：INTEGRATION:126-127 同一 bullet 先写"逐样本 ivar，**无任何 fallback**"，:133 又给出 fallback 路径。
- 建议改法：两文档删键改为"恒 fail-closed（A44 已删该键，出现即拒）"，负测改为"缺失即 rc=7"断言；不改任何科学判据（rc=7 语义未变）。
- 所属面：③④

---

**S6B-R06 / 红 / PHASE2_MOSAIC_WRITE.md:11,:19,:43,:46-47,:52-67,:214,:284 / "所有 stage2.cpp:NNN 行锚逐条实测"声明失实：行数、锚点、产品 title 串三重漂移**

- 问题与证据：
  1. 行数 1965×2 vs 实测 2015（R01）；:19 纪律声明"逐条 `grep -n`/`sed` 实测"因此为假。
  2. 锚漂移抽样（逐条 sed/grep 实测，doc→actual）：入口 main `:112→:120`；target_order 决议 `:203-204→:211-212`、rc=3 日志 `:205-208→:213-214`；frame_id 无效 `:223-228→:232-233`；manifest 排序/载荷/hash `:238-244→:246-252`；`AIO_HIPS_RD_SIGNAL/SUPPORT` 开启 `:536-537→:763-764`、`AIO_HIPS_RD_IVAR` `:557→:785`；ivar 门 `:553-574→:783-807`；canonical reducer 注释 `:1525-1526→:1773-1774`；diagnostics.json `:1748-1750→:2001-2003`。**唯 `p2_stage2_make_upm_cfg :428-430` 与 `p2_upm_build_geo :432-433` 精确命中**——混代特征。
  3. 产品 title：文档 :43 声称 `title="Astro Celestial Sphere Database（ACSD） Phase2 Mosaic"（:595）`——实测 stage2.cpp **:822** 传入的是 `"ACSD Phase2 Mosaic"`，且全 lib grep "Astro Celestial Sphere Database…Phase2 Mosaic" **0 命中**（仅本 md 自己有该串）；creator 串相符。
  4. 下游锚 `p2_session.cpp:81-92`（:46）未逐条复核，建议一并重锚。
- 建议改法：全文件 stage2.cpp 锚重测；title 改照录 `"ACSD Phase2 Mosaic"`（:822）。
- 所属面：②④

---

**S6B-R07 / 红 / docs/science/algorithms/PHASE3_RSMP_IMPL.md:103-117 / §4 符号冻结表结构断裂：截断 bullet 插入表中、三行脱表、同一注记重复两遍**

- 问题：:102 空行结束表格后，:103-107 的「签名漂移订正（本次）」bullet 在 "**并新增 `_ex`/`nanmask_ex`/`attach_cache`/**" 处**截断**；:108-110 三行表格行（`p3_sampler_close` / `P3SampleRejection` / `p3_sample_bilinear_nanmask_ex`）失去表头脱离主表（Markdown 渲染为纯文本）；:112-117 同一注记**完整版再次出现**（结尾多出 "`cache_stats`/`uncertainty_*` 等符号族。消费方按符号名核对，行号仅作导航。"）。
- 证据：上引行号为 read 工具逐行输出；:107 与 :116 文本逐字对比仅结尾不同。
- 反方核验：被脱表的三行恰是 NAN-SAMPLE-MASK 任务新增的三个符号——即表中最关键的新增面被渲染破坏。
- 建议改法：删 :103-107 截断残段，保留 :112-117 完整注记，把 :108-110 移回表格内。
- 所属面：②

---

**S6B-R08 / 红 / PHASE3_RSMP_IMPL.md:73（§3）,:218（§6.4）,:275（§7）,:347（§11 DISP-P3RSMP-002）,:382（§12） / FIFO/LRU 同文四说，DISP-002 已宣告失效却仍列在现行缺陷表**

- 问题：§3 "TileCache（**FIFO** 最旧逐出…，§7）"、§6.4 "（可能触发 **FIFO** 逐出）"、§11 缺陷表 DISP-P3RSMP-002 "tile cache 逐出为 **FIFO**…cpp:22-36（无访问序更新）"、§12 测试设计 "⑦ max_tiles 逐出行为（**FIFO**）" ↔ §7 "**有界 LRU 缓存**…DISP-P3RSMP-002（FIFO 登记）已随 P30 缓存改造失效，现行策略 = LRU"。同一文档两个结论并存，§11 表未标注失效。
- 证据（反方核验）：p3_resample.cpp 实测现行缓存 = `struct SharedTileCache` **:64-122**，get/put 均 `lru.splice(...)`（LRU 属实）；文档所指 "`TileCache` :22-36" 现为 **#include 区**——FIFO 结构已不存在。PHASE3_RESAMPLE.md:99 亦写"get 时 splice 到表头（LRU）"，与 §7 一致 → **四比一，FIFO 侧全错**。
- 建议改法：§3/§6.4/§11/§12 统一改 LRU，DISP-P3RSMP-002 移入"已失效"注记（保留历史可追溯）。
- 所属面：②③

---

**S6B-R09 / 红 / PHASE3_RSMP_IMPL.md 多处（:6,:14,:40,:43,:47-48,:57,:62,:71-77,:74,:75,:76,:84-110 表,:119,:130,:160-167,:196-214,:236-237,:258-263,:265-277 及 §8 表）/ 同文两代锚并存、"实测"多处失实、与自身头部矛盾**

- 证据（实际行号来自 p3_resample.cpp/p3_session.cpp/module_adapters.cpp/CMakeLists.txt 逐一开源）：
  1. **§3"生产源图（实测）"整体为旧版**：kTileWidth `18→48`、kReaderBuf `19→49`、"TileCache :22-36"（现为 include 区）、read_leaf `49-80→:186-202`（**§4 内部符号行给出的就是现行值 :186-202，§3 与 §4 同文冲突**）、p3_order_select `82-93→:204`、p3_resample_check_mode `95-107→:217`、p3_sampler_open `109→:264`、P3SamplerImpl `109-194→:64 起的现行布局`、bilinear/nearest 区 `196-239→:296-339`。
  2. **§6.3 与 §4 对 open_ex 同文两说**：§4 "cpp:269-296"（实测 :269 ✓）vs §6.3 "（cpp:130-150…130-159）"（✗）。
  3. **§8 消费链（自称"实测"）混代**：sampler 打开 :171（✓ 现行）、max_tiles :179-194（✓）、:260-273 attach cache（✓）↔ 采样分派 `236-237→:288-291`、行级取消 `→:277`、"cancelled at row" `260-261→:349-350`、worker open 失败→IO `263→:352-355`、worker 池 `211-214→:244-247`、worker 闭包 `217-244→:250-333`、worker open_ex `222→:255`、provenance `265-277→:365-382`（manifest_hash/missing 恒 nullptr :374-375；order/sampler 填实际值 :379-380）。§7 同时引用现行 :260-273 与过期 :217-244/:211-214。
  4. **§2 表**：descriptor ":783 实测"→实际 :791（R02）；**"生产源仍在 lib/phase3_session/"与自身头部"生产源: lib/algorithms/resample/…"直接矛盾**；CMake `根 CMakeLists.txt:460-465` → 实际 add_library **:933**，且 CMake 注释明示 p3_resample.cpp 已归 astrocs_p3_rsmp——**"p3_wcs.cpp/p3_resample.cpp 为五源文件之一"不成立**（add_library 现仅 p3_session.cpp 一个源）。
  5. **p3_coverage_test.cpp 同文两个行数**：§3 "224 行"（实测 224 ✓）vs §12 "（106 行）"（✗）。
- 建议改法：以"符号名优先"纪律全量重锚 §2/§3/§6/§8，行数与库成员声明按现行 CMake 复测；§12 的106 改 224。
- 所属面：②④

---

**S6B-R10 / 红 / PHASE3_PROJ_IMPL.md:16,:41,:59-65,:61,:64,:67-69,:70-79,:153,:217,:237,:265,:343 与 PHASE3_FITS_IMPL.md:51 / 行数与路径多组自相矛盾、§6-§8 全旧锚与 §3 新锚同文两说**

- 证据：
  1. p3_wcs.h 行数：PROJ:16 "166 行（…行数按新址复测）" vs PROJ:59 §3 表 "373" vs 实测 **373**——头部"复测"为假；**FITS:51 又写 "p3_wcs.h 166 行，实测"（✗），与 PROJ §3 的 373 直接冲突（③两文档打架）**。
  2. p3_wcs_test.cpp：PROJ:64 表 "90 行" vs PROJ:343 "（474 行）"——同文两值，**实测 474（§343 对、§64 错）**；且路径 `eng/tests/unit/p3_wcs_test.cpp` **不存在**，实际在 `lib/algorithms/projection/tests/p3wcs/p3_wcs_test.cpp`（迁移后未更）。
  3. p3_session.cpp：PROJ:61 "329 行" → 实测 **441**；消费点 ":232 逐像素 pix2world" → :232 现为 `std::atomic missing_px`，pix2world 实在 **:280**；":247-253/:17" 部分存活（:160 make ✓）。
  4. **§6-§8 节头整段旧锚 vs §3 现行锚同文两说**：§6 ":30-123"（:153）、§7.1 ":126-150"（:217）、§7.2 ":152-175"（:237）、§8 ":177-195"（:265） ↔ §3 "p3_wcs_make :92-171 / pix2world :515-540 / world2pix :542-568 / fits_keywords :570-593"。实测：make **:86**、pix2world **:515** ✓、world2pix **:542** ✓、keywords **:567**——**§3 基本对、§6-§8 全错**；§7.1 伪代码内还新旧混排（`:95-114` 旧与 `:524-527` 现行并存）。
  5. 头部声明锚（§3）：p3_wcs_make `h:31-34→实际 :47`、pix2world `38-39→:54`、world2pix `42-43→:58`、fits_keywords `46→:62`——全部 +16；**PHASE3_FITS:53 "p3_wcs.h:47-62 声明"才是现行值**（两文档再次打架）。
  6. CMake `460-465` → 实际 **:933**（同 R09-4）；§12 测试面同文 90 vs 474（见证据 2）。
- 反方核验：kTanApplicability 实测 :212-221、p3_wcs_applicability :232、值 1e-8/1e-6/0.9/20.0/85.0/128.0 与 PROJ/FITS/RESAMPLE/SCI 四文档所载完全一致——**容差事实源面干净，红在锚与行数**。
- 建议改法：PROJ 头部行数改 373、测试路径改现行目录、§3 测试行数统一 474、§6-§8 节头与 §7.1 伪码按 §3 现行锚重写（或删行号仅留符号名）；FITS:51 的 p3_wcs.h 计数改 373。
- 所属面：②④

---

**S6B-R11 / 红 / PHASE2_REJECTION.md:6 vs :61/:506/:615/:729（行数）, :142（§4.1）, :49-52（§2 默认值锚）/ 同文行数两版、status 枚举锚两版且均不中、默认值锚整体 -17 错位**

- 证据：
  1. 行数：:6 "2949 行" vs §3/§7/§13 "2950 行"（同文两值）vs 实测 **2956**；rejection.h "595 行"×4 vs 实测 **604**——均冠"实测"。
  2. status 枚举：§3 表 "P2RejectStatus | rejection.h :102-111" vs §4.1 "stack status（rejection.h:79-90）"——**同文两说**；实测 `enum P2RejectStatus` 在 **:107-115**（P2_STATUS_INTERNAL_ERROR=:115 落在两版锚之外）。
  3. §2 默认值锚错位 −17：`σ_lo/σ_hi` ":1229（默认）"→实测 **:1246**、`alpha` ":1236"→**:1253**、`plow/phigh` ":1237"→**:1250 区**（:1236/:1237 现为 underdetermined 注释块）；**数值本身 4.0/3.0/0.05/0.2/0.1 与代码一致（科学值未漂，仅锚错）**。
  4. 反方核验（抽样为对的部分）：method_minimum_n ":982-999" 实测 :982 ✓、p2_reject_stack_ex ":1996-2201" 实测 :2016 ∈ 区间 ✓、OK 状态 ":2196-2198" 实测 ✓、astrocs_n_map_method ":1148-1158" 实测 :1154 ∈ 区间 ✓——红集中在 1/2/3 三组。
- 建议改法：行数与枚举锚统一重测；§4.1 与 §3 归一为同一值；§2 默认值锚 +17 修正。
- 所属面：②④

---

**S6B-R12 / 红 / PHASE2_UPM_IMPL.md:10-11,:72（行数）, :78-94（导出符号实现列）, :145-148,:477-479（module_adapters 锚）/ 行数、实现锚、适配器锚三层过期（L3 单文档 58 条）**

- 证据：
  1. 行数 2793/384 "实测" vs 实测 **2981/450**（R01）。
  2. 导出符号实现列（doc→实际，grep -E 逐个）：p2_upm_build `:1205-1208→:1372`、build_geo `:1210-1214→:1377`、save `:1216-1282→:1383`、close `:1929-1933→:2218`（偏移 +167…+289，非均匀）；upm.h 声明列 build :121/geo :129 仍准、close `doc :222→实际 :219`。
  3. module_adapters 锚：fit/apply sci_id ":676/:696"、descriptor 段 ":665-698"、节点链注释 ":557" → 实测 `p2_upm_fit_descriptor :1084`、`p2_upm_apply_descriptor :1103`、链注释 **:1042**（+408…+485）。
  4. **门 L3 在本文件报 58 条**（presence-in-range 失败的符号行），是全切片最大簇。
  5. 交叉一致性：§2 ":49 control_variance ← upm.cpp:2785" 与 PHASE2_SAMPLER:329 订正注 "upm.cpp:2954-2975（rc=1 @ :2966）" ——后者实测 **✓ 精确**（:2954=`p2_upm_control_variance`、:2966=`if (k_corr < 1.0) return 1;`），但前者（:2785）在现行源为旧值且 SAMPLER 订正锚（2954-2975）**超出本文档声称的文件长度 2793**——两文档互相暴露对方行数声明过期。
- 反方核验：§附录"收敛配置面只有两字段 tolerance/tolerance_relative（upm.h:77-81/:113）"实测 **✓**（:81 tolerance、:113 tolerance_relative，全 lib 无 tol_step 字段，仅 :109/:147 注释语词）——该节内容可信；k_corr 段（:424-437，D-08 两因子公式 + 1.4 为代码默认 + MC1.3883）与终裁一致且带两处订正注，未重报。
- 建议改法：行数重锚、导出列/module_adapters 列全量重测、消化 L3 58 条。
- 所属面：②④

---

**S6B-R13 / 红 / PHASE2_SAMPLER.md:15-16,:89,:100 / 行数"实测"失实 + p2_sample_controls_cached 声明锚漂移**

- 证据：1536/288×4 处 vs 实测 **1503/273**（L1×4）；§3 表 `p2_sample_controls_cached | h:156-168` → 实测声明在 **sampler.h:141**（门 L3 唯一条）。C5 豁免问题见 R02。
- 反方核验：sampler.h 其余声明 default_config :66 / frame_id :99 / median :103 / mad :104 / controls :109 全部与文档 :66/:99/:103/:104/:103-114 相符（controls :109 落在其声称区间内）；sampler.cpp `p2_sample_controls_cached :1255` 与文档 :1255-1271 **精确**、:126 订正注（cpu_workers :320）语义完好——**红仅限行数 4 处 + cached 声明 1 处**。
- 建议改法：6 处重锚。
- 所属面：②④

---

**S6B-R14 / 红 / PHASE2_SESSION.md:22 vs :383-384（行数）, :20,:50-57,:304,:376,:385（CMake 锚）, :31,:83-100,:155,:183（module_adapters 锚）/ 同文行数两版 + CMake/adapter 锚整体大漂移**

- 证据：
  1. p2_session.cpp：头部 :22 "（318 行，实测；本文件行号一律照录实测值）" 与 §12 :376 "318 行（复测）" ↔ §13 :383-384 "**（298 行，复测）**"——同文两版；实测 **318**（298 版错）。
  2. CMake：astrocs_phase2_session ":454-458"、acsd 链接 ":501-506/:504"、QA-001 ":517-529"、module_adapters 链接 ":538" → 实测 add_library **:925**、include :926-928、link :929、acsd 侧 **:1012/:1033/:1070**（+471…+555）。
  3. module_adapters：P2Api 五委托 ":777-784"→实测 `struct P2Api :1201`；phase2_descriptor ":283-300"→**:711**；注册段 ":746-751"→**:15443 附近**（d2 注册）；P2Api::last_error ":717"、注释 ":105" 同批 +400~900 漂移。
- 反方核验：p2_session.cpp **内部**锚（五函数 :56-273、四段边界 :120/:152/:181/:223-226、常量面 :184-194、DISP-P2SES-001..008 各锚）与 318 行现行文件自洽（文件未再变），未见内部矛盾——红在两组外部锚 + 行数两版。
- 建议改法：§13 298→318；CMake/adapter 全量重锚。
- 所属面：②④

---

**S6B-R15 / 红（只登记，不代改）/ 域外源码 eng|lib / D-05 已批裁决（idw_power 默认 1.0 + 配置化 + 日志实测 p*）未落码**

- 证据：
  - 生产默认 **2.0**：`lib/algorithms/drizzle/healpix_drizzle/snr_evaluator.h:46,:109`（idw_power=2.0），fallback 2.0 于 snr_evaluator.cpp:199/:255；archive 侧 healpix_stack 同 2.0。
  - **无配置键**：eng/packaging/config/*.json 与 defaults.json grep idw 均 0 命中；lib/**/configs/*.json 无 idw_power。
  - **无实测 p* 日志**：orchestrator.cpp:4508 仅回显 model.idw_power，全链无 argmin/实测 p* 记录；生产 idw_power 仅经 aio hp_drizzle_api 模型文件字段持久化。
- 反方核验：分歧台账 D-05 裁决原文"defaults/配置（idw_power 2.0→1.0，配置化+日志，负责人已批）"；本切片 12 个域内文档 grep "idw_power" **0 命中**——文档侧无违述，红在**裁决与源码之间**（属 ④"已批裁决未落码"，跨域登记给总编）。
- 建议改法：按 D-05 走变更实施（默认 1.0、配置键、p* 日志），或由负责人复认裁决状态；本检查不触源码。
- 所属面：④

---

### 黄

---

**S6B-Y01 / 黄 / docs/science/algorithms/INTEGRATION_ALGORITHMS.md:41 及 anchor_contract.json 豁免条目 / FROZEN_SCI_CLAIM 豁免理由失实、豁免范围为死区**

- 问题：`integrate.cpp:10-79`（×4 豁免）理由称"本 ALG 文档 §3 已给现行行号"，但该文档 §3（:43-62）是**伪代码、不含任何行号**——理由不成立；且豁免区间 10-79 **不覆盖实际 signal :83 / OK :85**（函数体 :19-87），锚只能以 bypass 而非内容通过 C3。
- 证据：门 C3 输出对这 4 条记 bypass（非通过）；integrate.cpp 实测 signal :83、OK :85、函数 :19-87；文档 §3 逐行 read 无行号。
- 建议改法：豁免理由改写为真实依据（如"SCI FROZEN 不回改、现行行号见 docs/science/algorithms/PHASE2_INTEGRATION §3"），并把区间更新为现行范围或显式声明其为 SCI 历史锚。
- 所属面：④

---

### 绿

---

**S6B-G01 / 绿 / INTEGRATION_ALGORITHMS.md:81-85 / 章节序异常："## 5c SIMD 安全与取消点"(:85) 排在 "## 7"(:81) 之后、"## 8"(:90) 之前。** 语义无损，建议有空时把 5c 归位。所属面：②

**S6B-G02 / 绿 / PHASE2_UPM_IMPL.md:532-540 / 无编号附加节"收敛容差与报告字段（现行登记）"位于"参考文献与参考代码库"之后。** 内容实测可信（见 R12 反方核验），仅结构建议加节号并移至参考文献前。所属面：②

**S6B-G03 / 绿 / PHASE2_INTEGRATION.md:337 / Aitken 引用年份 1935 vs Crossref 记 1936。** DOI 10.1017/S0370164600014346 经 Crossref 实测命中目标文献（Proc. Roy. Soc. Edinburgh **55, 42-48**，"On Least Squares and Linear Combination of Observations"）——卷页与 DOI 正确，仅年份口径（卷 55 跨 1935-36）。所属面：①

---

## 三、已查无问题面

**① 科学性（对照分歧台账 D-01…D-11 终裁 + 非退化负例）**
- D-01/D-02/D-09/D-11（sup 外径、depth-8 残差、0.1043885/N²、VOS）：本切片 12 文档 grep 无相关量，零违述。
- D-04（k=1.152）：域内 grep "1.152" **0 命中**。
- D-05：域内文档无 idw_power 表述；裁决未落码部分已按 R15 登记（源侧）。
- D-08：PHASE2_UPM_IMPL:424-437 与 PHASE2_SAMPLER §5.4 均按"公式面 = k_gauss(N_retained)×k_geo 几何查表、1.4 = 代码默认/回退（kControlCorrDefault :83）、MC 实测 1.3883"表述，与终裁一致（两处历史订正注保留可追溯，未重报）。
- D-10：域内 12 文档 grep "idw_power|natural_bicubic|bilinear_regular" **0 命中**（无"bilinear 为生产默认"残留）；生产默认 natural_bicubic_spline_clip_v1 在 weight_chain.cpp:142 实测 ✓。
- A-P3-08（adaptive_max_depth=12）：域内 grep "adaptive_max_depth|WCS_ADAPTIVE|spherical_overlap|depth|深度" **0 命中**——本切片无"8"残留。
- 数值默认面逐值比对：rejection σ 4.0/3.0/α0.05/带宽0.2-0.1（源 :1246/:1253 ✓）；UPM 收敛两字段 tolerance=1e-6/tolerance_relative=0（upm.h:81/:113 ✓）；WCS 容差 1e-8/1e-6/0.9″/FOV20/|dec|85/包络128（kTanApplicability :212-221 ✓）；PHASE2_SESSION 常量面 huber 1.345 等与 SCI §5/§7 口径一致。
- 非退化负例检查：PHASE2_INTEGRATION §11.3 的 support-reducer 双臂负例（0.9 vs 0.3、signal 两臂 =11 正交）具判别力；§11.4 F1b 权重判别门显式声明 F1 恒真无判别力——合格。
- 抽查方法：grep/sed 逐值开源比对、分歧台账逐条对照。

**② 行文逻辑**
- 全部域内文档 grep "UNRESOLVED" **0 命中**——无未解析登记混入正文充当结论；DISP 均以"登记不改码、整改归属"态出现（唯一例外 R08 的 DISP-002 已登记）。
- 订正注（HTML 注释）抽查：PHASE2_SAMPLER:126/:329、PHASE2_UPM:427/:434 均为"旧对照+新值"完整两段格式且新值实测为真（:329 的 upm.cpp:2954-2975/:2966 精确命中）。
- 已知结构性问题见 G01/G02；其余文档章节链（上游 SCI→符号→公式→DISP→TEST→追溯）完整可追。

**③ 跨文档冲突（五链权威）**
- 容差唯一事实源四文档一致：PHASE3_RESAMPLE:160、PHASE3_PROJ:143、PHASE3_FITS:332-338、SCI PHASE3_HIPS_TO_FITS:101 同指 kTanApplicability/p3_wcs_applicability（实测 :212-221/:232），无第二套数值。
- PHASE3_FITS 与源逐锚相符（p3_output 函数 :142/:151/:166/:182/:197/:449/:457、BUNIT ADU/sr :284/:351/:675、p3_resample open_ex BUNIT 回填、p3_session :160/:346-350/:391-397 消费链）——是全切片锚最准的文档（其唯一红为 R01 的 p3_wcs.h 计数 + R10 联动）。
- ASTROCS_DESIGN/源码链：CMake"五源文件之一"类陈旧声明已随 R09/R10 登记；docs↔docs 冲突（166 vs 373、h 声明锚两套）已随 R10 登记。
- 与 PASS 表衔接：上轮已 PASS 的 k_gauss 1.316 表、k_corr 记法、QA_MATRIX 措辞等未重报。

**④ 幻觉与锚**
- 两台机器门只读复跑并给出全量分解（R01/R02）；anchor_contract.json 7 条豁免逐条打开核对（1 条理由失实 = Y01，其余 6 条为 SCI/ledger 类且 C5 已点名）；unresolved_registry.json 26 条自洽、与域内文档零交叉（见 R02 反方核验）。
- "文档说键/字段/串存在、代码没接"专项：域内 grep 未发现幽灵配置键（唯一反例是 R05 已删键被当活键、R06 title 长串全 lib 不存在）。
- 文献核验：Huber 1964 DOI 10.1214/aoms/1177703732、Holland & Welsch 1977 DOI 10.1080/03610927708827533、Aitken DOI 10.1017/S0370164600014346 均经 Crossref API 实测命中目标文献（title/journal/volume/page 见 G03）；PHASE3_RESAMPLE 所引 Górski 2005 / Greisen & Calabretta 2002 / Calabretta & Greisen 2002 / Fernique 2015 在上轮已核 ✓；Kahn 1962、Merkle 1988、FIPS 180-4 为标准条目未再联网。
- 逐锚开源抽验量级：本报告每条红均附 sed/grep/read 实测行号，累计开源核验 ≥120 个锚点；未复核项（如 PHASE2_REJECTION 下游 stage2.cpp:1098/:1361/:1379 等少数外围锚）不计入红。

## 四、统计

- 红：**15 条**（R01-R15）
- 黄：**1 条**（Y01）
- 绿：**3 条**（G01-G03）
- 责任域 15 文件全部逐一检查，无遗漏；其中 PHASE3_FITS_IMPL 仅 1 条红（且为与 PROJ 联动的计数问题），PHASE3_RESAMPLE 无独立红。
