# 工程控制 / RELEASE-02 验收记录（ACCEPTANCE）

> PASS 仅由前台独立验证后写入；证据为前台独立复跑结果，不复用 SubAgent 自述。不用 waiver 掩盖红灯。

## 1. 总表

| 任务 | 状态 | 机器门/证据 | 前台结论 |
|---|---|---|---|
| DOC-101 | **PASS** | 核验器 `工程控制/RELEASE-02/verify_doc_pack.py`（R1_missing=0 / R1_hash_mismatch=0 / R2_residue=0 / R3_unreachable=0 / R4_stale_version=0，verdict=PASS，授权差异 4 条）；`ENG-CONSTRAINTS` PASS；`CHK-REGISTRY-DOC-SYNC` PASS（2/2）；旧内容残留扫描 0 命中 | 36 篇替换核验通过；U-1/U-2 随新包闭合 |
| FIX-SCI | **PARTIAL** | 4 件变更 claim（DRZ-001/WCS-001/S2-PHOT-001/S2-P3-001）；`science_contract_lint` 4 文件 15 节 **PASS**（前台复跑）；`CHK-DANGLING` **PASS**；`CHK-SCI-REF` 唯一红点为**并行分片 FIX-A** 的 `C4_symbol_binding P2SMP-CELLSIDE`（前台已复现并确认非本任务，已发消息要求 FIX-A 收尾前修正）；DOI 经 ADS 核验真实（`2002PASP..114..144F`、`2017ApJ...836..187Z`） | 两项 P0 订正落地且 §5/§7 已自洽；**遗留 DISP-DRZ-009**（drizzle 实现仍 legacy 归一，默认 pixfrac=1 逐位不变）→ 门「文档-实现一致」未完全闭合 |
| FIX-A 天光面链 | **PASS** | 前台独立复跑：**全量 ctest 460/460 passed, 0 failed**（前台 `ninja -C build` 重建后运行；首轮曾见 10 红，根因为**前台自身 `/dev/shm` 写满**（7.9G/100%，测试编译临时文件写不下），清理后复跑全绿——非代码问题，已如实记录）；`DOC_LINE_ANCHORS_PASS`（39 docs/877 anchors，EXEMPT:9 **未增加**）；`CONSTRAINTS_PASS`；默认值已改为目标模型开启（`sky_plane_enabled=true`/`frame_gain_enabled=true`，选项 a）；`g_k` 偏差根因=MA 把加性结构吸收成伪乘法，新增 `p2_sky_estimate_gain_dc` 直流比交叉校验（dev>1% 则 reject 取 g=1 并记日志），新增 `v6_p2_sky_dcgain`/`v6_p2_sky_overflow` 用例（前台确认已在 CMake `foreach` 注册，13 例全过）；附带修复 `bi[b]==-1` 时 `H_data` 越界写真实 bug | P0-08/09/10 **CLOSED**；`FIX-A-UPM-001.md` 变更 claim 就位（`PHASE2_UPM.md` 冻结加性模型 vs 目标模型，待负责人裁决） |
| FIX-REJ 排异专项 | **PASS（研究+claim；实现待派）** | 前台独立复核根因：`module_adapters.cpp:4412-4418` **确认** `req.request=P2_REJECT_AUTO` 且 `nominal_contributors=frames.size()`（**整组一次解析，非逐像素**）、`underdetermined_n=2`；`algorithm_rejection_method` 在 `lib/` **零 C++ 消费者**（**确认**，即用户显式指定会被静默忽略）；L4 证据 `l4__p2_m42/p2_rejection.json` 实测 `plan.method=2`/`profile=wbpp_current`/`rejected_samples=21976`/`underdetermined_pixels=101649311`/`n_pixels=137101312`（**74.14% 像素为 n=2 白名单**，`accepted_pixels==n_pixels`）——与 SubAgent 数字**逐项吻合**。WBPP 一手逻辑已由前台从负责人提供源码解出并复核 | 根因：**排异子系统完整接线但 AUTO 按整组帧数解析，而 97.7%+ 像素只有 n=2/3/4，kernel 在这些 n 上零拒绝能力** ⇒ 卫星线原样进入叠加。实现/CLI 接线/按 n 映射表为 **P0-20**，待派实现分片 |
| FIX-B 方差链 |  | ivar 产品；cosmetic 修复；frame_snr 入头；noise_snr 构建 | P0-02/03/04/05/06/07 归宿 |
| FIX-C 集成链 |  | 方差最小性；variance_available；词表；sparse layer | P0-11/12/13 归宿 |
| FIX-D CLI/export |  | 三模板合同测试；预检三级颜色 | P0-14 归宿 |
| FIX-E 性能 |  | 多核/内存/缓存指标（合成与真实） | 性能违约群归宿 |
| TST-101 |  | 负例覆盖率；零测试模块；fixture 解锁；ctest |  |
| BLD-101 |  | cmake/ninja 0 警告；ctest 全绿；全部机器门 | P0-01/16 归宿 |
| E2E-102 |  | L3/L4 rc；科学路径核查；1/N 一致；WCS 闭合 |  |
| VIS-102 |  | L4 §5.2 逐项；行差 ≤2×；裁剪放大 |  |
| PERF-102 |  | G-RES-01 六判据真实数据面零违约 |  |
| DEL-102 |  | 两个 FITS 结构/数值/视觉/单位；限制清单 | 待负责人检查 |

## 1b. 最高设计补充（负责人指示）

| 项 | 内容 |
|---|---|
| 补充位置 | `ASTROCS_DESIGN.md` §4.5「逐像素排异：叠加的前置必需步骤」+ §4.3 交叉引用 |
| 依据 | 负责人明确指示：叠加不是纯逆方差加权 average，高 SNR 帧上的卫星线会显著拉高，应先排异再叠加；并要求 mosaic CLI 支持排异算法选择（留空/0/auto ⇒ 按输出像素输入集合大小 n 自动选择；显式指定 ⇒ 强制） |
| 一手证据 | PixInsight **WBPP 实测源码**（负责人提供 `scripts.zip`，解出 `run/RELEASE-02/FIX-REJ/wbpp/BatchPreprocessing/`）：`BPP-FrameGroup.js:1304-1312 bestRejectionMethod()` = `n<6→PercentileClip`、`6≤n≤15→WinsorizedSigmaClip`、`n>15→LinearFit`；`BPP-FrameGroup.js:1229-1293 rejectionIsGood()` 给出各算法 n 约束，且**拒绝 NoRejection 与 MinMax**（原文：Min/Max rejection should not be used for production work） |
| 门禁 | `verify_doc_pack.py` PASS（36 篇/授权差异 5 条/引用 83 处；self-test 6/6）；`ENG-CONSTRAINTS` PASS。新增授权差异 2 条已登记 manifest：`ASTROCS_DESIGN.md`（本补充）、`docs/plugins/algorithms_phase1/08_drizzle.md`（FIX-SCI-DRZ-001，**补登记**——FIX-SCI 提交时漏登记，前台复核时发现并补上） |
| 遗留 | 排异实现/CLI 接线/按 n 映射表冻结于 `docs/plugins/algorithms_phase2/12_rejection.md`，由 FIX-REJ 分片闭合（P0-20） |

## 1a. 独立科学审计（SCI-AUDIT，独立验证者角色）

> 立场：`ASTROCS_DESIGN.md` = 需求/设计意图 = 最高权威；`docs/science/**`、`docs/algorithms/**` = 待证主张。审计员不盲从文档、代码、其他 agent 结论。

| 项 | 结果 |
|---|---|
| **核心问题：文档问题是否阻塞最高设计** | **BLOCKS-DESIGN = 0 条**（覆盖 P0 全部 11 个 UNRESOLVED 主题 + RELEASE-02 全部 5 条 change-claim + P1-A 28 条 + P1-C 42 条 + P1-B/D 部分） |
| 台账 | `reports/RELEASE-02/SCI-AUDIT/{REGISTER.md,VERDICT.md,EVIDENCE-GAPS.md}` + `per-doc/` 11 篇（含对 `ASTROCS_DESIGN.md` 的独立评估） |
| 复算/证据 | `run/RELEASE-02/p0_recompute.py(.log)`、`verify_refs*.py(.log)`、`run/RELEASE-02/p1/P1-{A,B,C,D}.md` |
| 前台抽验（已独立确认） | ① `contracts/schemas/unified/frame_snr.schema.json` 描述**仍写「不受天光影响」**（与设计 §3.4/§4.3 及同包订正口径冲突），且自述「不得与 depth_m5 互填」而实装输出 5σ 深度对象 ⇒ **设计输出合同当前未兑现**；② `p2_upm_ma_build` **确已存在**（`upm.cpp:1898`，`stage2.cpp:516` 调用）⇒ 乘性模型并非未实现，落后的是 FROZEN `PHASE2_UPM.md` 与 scheduler 的 `fit_upm` 适配器（`module_adapters.cpp:4155` 仍加性）；③ 校准同母版 bias 折叠实测 `j_b=-(1-α)/denom`（`v6_calibration_covariance.cpp:313-319`）⇒ 插件 `01_calibration.md:31` 的 `(1+α²)` 应为 `(1-α)²` |
| 证据缺口规模 | 高 3（数值 Oracle 复跑脚本整体缺失；k_corr MC 构建孤儿不可复跑；PSFSW 指数无 L1 标定）、中 ~6、低 ~4；30 条 DOI 经 Crossref 核验，2 条疑似错引 |
| 状态 | **进行中**（P1-B/P1-D 收尾后更新 REGISTER） |

## 1c. 长期工作规则（负责人 2026-09-18 指示）

> 负责人原话：「**后面再遇到这类显然不合理的问题就应该像这次一样挖掘**」。

**规则：凡遇到"显然不合理"的现象，必须像 P0-21 一样挖到根因，不得绕过、不得当成环境问题、不得靠调参掩盖。**

具体动作（P0-21 的可复制套路）：
1. **先质疑前提**，不采信任何转述（含 SubAgent 报告、含我自己的前一轮结论）；
2. **用运行时证据**核对，而不是用配置/文档声明——**"声明 ≠ 执行"**；
3. 顺着数据流**逐层定位到 file:line**，给出可复现判据（如 `16777216 源像素 = 恰一帧`）；
4. **横向核对同类**（P0-21 是靠对比同文件其他操作器才发现"有的循环、有的只取 [0]"）；
5. **查测试为何没发现**，补上能红能绿的用例（含负例）；
6. 结论**如实入册**（GAP_AUDIT/ACCEPTANCE），包括**我自己的错误判断**与更正过程。

**已知因"没挖"而犯过的错**（作为反例记录）：
- 8.2 曾据 L4 证据断言「74% 像素 n=2 ⇒ 排异失效是产品缺陷」→ 实为输入被降采样，**前提错**；
- 8.2 又"更正"为「49 帧都已使用（预叠加）」→ 仍是错的，**因为只看配置声明、没核对实际产出**；
- 8.3 才靠日志计数定位真因：**三个决定性操作器只取 `input_lights[0]`**。

## 2. P0 归宿台账（逐条 CLOSED / 变更 claim / 上呈）

| P0 | 结论 | 证据（提交/报告/测试） |
|---|---|---|
| P0-01 模块门 |  |  |
| P0-02 ivar 产品 |  |  |
| P0-03 frame_snr |  |  |
| P0-04 校准方差 |  |  |
| P0-05 cosmetic 空转 |  |  |
| P0-06 cosmetic 方差 |  |  |
| P0-07 noise_snr 构建 |  |  |
| P0-08 采样/掩膜 |  |  |
| P0-09 天光面 |  |  |
| P0-10 g_k 接线 |  |  |
| P0-11 集成方差 |  |  |
| P0-12 sparse layer |  |  |
| P0-13 weight_mode |  |  |
| P0-14 export 合同 |  |  |
| P0-15 版本号 | 不在本包（FIN 阶段处理） | 负责人裁决：全部验收通过后再改 |
| P0-16 文档↔仓库冲突 | **CLOSED** | 新文档包 `ENGINEERING_SPEC.md` §7 根白名单补 `ACCEPTANCE_SPEC.md`（修 U-1）；`docs/ci/01_CHECKS.md` §2 补登记 `CHK-E2E-REPRO`、`CHK-EXIT-CONSISTENCY`（修 U-2）。前台独立复跑：`ENG-CONSTRAINTS` verdict=PASS、`CHK-REGISTRY-DOC-SYNC` verdict=PASS 2/2 |
| P0-17/18（二轮增量） |  |  |
| **P0-19（RELEASE-02 新增）** `hips_frame=icrs` 违反 IVOA HiPS | **待修（证据已闭环）** | 三方证据：IVOA 官方规范源码（`ivoa-std/HiPS` 关键字表与示例均为 `equatorial`）、CDS 生产 HiPS 实样（DSS/2MASS 均 `equatorial`）、本仓 `module_adapters.cpp:5284` 自述系前批 agent 误改。受影响：writer `:1122,1371`、`PHASE3_HIPS_TO_FITS.md:63`、`p1hips_tests_units.cpp:288-289`、adapter 注释 |

## 3. 自决闭环留痕（§3a 授权事项）

| 编号 | 发现 | 研究证据 | 文档订正 | 代码/门禁变更 | 验证结果 |
|---|---|---|---|---|---|
| SD-01 | DOC-101 核验器 R4 把**授权发布版本串** `0.0.1alpha` 误判为旧世代残留（5 处假阳性）；根因：RELEASE-01 版定义了 `AUTHORIZED_VERSION_TEXT` 却**从未使用**（死代码） | 本包 ACCEPTANCE_SPEC.md:13/158/183、ASTROCS_DESIGN.md:533、ENGINEERING_SPEC.md:113 均为**发布纪律正文**，属授权文本；旧版 `0.1alpha` 不匹配 `0.0.\d+-?alpha` 故当时未暴露 | 无（文档本身正确） | `工程控制/RELEASE-02/verify_doc_pack.py`：R4 增加授权版本串例外；判据抽成可单测 `scan_text()`；新增 `--self-test`（R2/R3/R4 正负例 6 项） | `--self-test` **6/6 通过**（授权版本串 clean、旧版 `0.0.9-alpha`/`V19` hit、`CHK-*` clean、治理 ID hit、可达引用 clean、死引用 hit）；改后核验器 R4=0 且 verdict=PASS |
| SD-03 | RELEASE-01 SCI-001 的 P0-1：`DRIZZLE.md` §5 归一 `S_p=B0/pixfrac²` 与 §7 不变量 `S_p=B0` **互斥** | Fruchter & Hook 2002 PASP 114,144 式(5)（DOI 10.1086/338393，ADS 核验真实）为**一致加权均值**、drop 面积在分子分母相消；DrizzlePac Handbook §2.3.2；drizzlepac/SWarp 只读对照 | §5/§7/§11 订正为面亮度保持 `S_p=Σ_j B_j a_jp/Σ_j a_jp`、`w=a_jp/A_pixel,j`、条件通量守恒 `Σ_p F_p=pixfrac²Σ_j x_j`（claim `FIX-SCI-DRZ-001`） | **未改实现**：`drizzle_engine.cpp:1531` 仍 legacy `weight=overlap_area/drop_area`，登记 **DISP-DRZ-009**（最小修复 `weight=overlap_area*pixfrac²/drop_area`，默认 pixfrac=1 逐位不变） | 前台复算：`w=a/A_pixel` 且常数面亮度 `x_j=B0·A_pixel` ⇒ `S_p=B0`（与 pixfrac 无关）；legacy 给 `B0/pixfrac²`，证实 §5 错、§7 对；§5/§7 改后自洽；`science_contract_lint` PASS |
| SD-04 | RELEASE-01 SCI-001 的 P0-2：`ASTROMETRY.md` §5a 的「1px 平移」口径 | 求解器拟合自变量为 sdet 半整数像素中心 `det_x=i+0.5`（`sdet_api.cpp:546-549`），`u=det_x−w/2=p−CRPIX=q`（Paper I §2.1.1 式(1)）；astropy 7.0.1 复算 + fixture 独立 TAN+SIP 复算 + T4 大尺度闭环（solved median 0.1077 px） | §5a/§14a 与 `STANDARDS_REGISTRY` STD-F1 标签订正：旧「0-based 常量 1px 平移」是**标签错误**，实现与 Paper I **逐式一致、无 1px/0.5px 误差**（claim `FIX-SCI-WCS-001`） | **未改实现**（结论为实现正确）；建议 comment-only 改 `ipv_wcs.h:43,57-60,70-71` 与测试面 origin relabel | 前台确认订正后 §5a 与 STD-F1 表述一致；`science_contract_lint` PASS；风险 R2（半整数探测与 `cx=w/2` 的 0.5 相消无测试锁定）已登记 |
| SD-02 | 新文档包内部**自相矛盾**：同包 `ASTROCS_DESIGN.md` §153 已订正天光口径，但 §235 仍留旧措辞「天光作为独立背景分量扣除，**不受天光影响**」与旧「信号功率/稳健噪声功率」表述 | 同包 `ASTROCS_DESIGN.md:153`、`docs/plugins/algorithms_phase1/07_noise_snr.md` §4.1（本包已订正为「信号项不被加性天光背景虚高；天光散粒噪声计入 σ_n」）；DOC-101 步骤 4 亦点名「旧天光措辞『不受天光影响/不漂移』」为待扫残留 | `ASTROCS_DESIGN.md` §235 订正为「真实 PSF 源信号/稳健噪声，通量型口径 `F_ref/σ_F`；信号经独立局部背景估计与扣除、不被加性天光背景虚高，天光散粒噪声如实计入 σ_n」 | 无 | 核验器 PASS（R1–R4 全 0，该处登记为第 4 条授权差异）；残留扫描 0 命中；正向确认「不被加性天光背景虚高」「天光散粒噪声」在位 |

| SD-05 | **口径冲突①**：§4.5「按该输出像素的 n 选择排异算法」 vs `docs/science/REJECTION.md` §4/§6/§7「**禁止 per-pixel effective 路由**」 | 精读冻结文档：它禁的是**按 `n_eff`（资格/掩膜后存活数）重选**；其自身规定路由依据是 **`n` = nominal contributors（几何可贡献数，一次解析）**。而实现 `module_adapters.cpp:4412-4418` 传的是 `nominal_contributors = frames.size()`（**整组帧数**，全图一个值） | §4.5 第 2 条加「`n` 精确定义」消歧：路由依据 = **几何覆盖数**（coverage 覆盖图，与掩膜无关）；**不得**用整组帧数、**不得**用 `n_eff` | 待实现分片改：以 per-pixel 几何覆盖数作 `nominal_contributors` | **裁决：非冲突**。文档与设计意图一致，是实现取错了 `n`（用整组数代替几何数）。**无需改冻结文档**，也无需上呈 |
| SD-06 | **口径冲突②**：§3.5 `-force`「连 error 也跳过」 vs 实现「结构错不可 force」 | 负责人 2026-09-18 原话：「**force 运行可以跳过所有检查直接进入运行过程**」——以负责人表述为准 | §3.5 已写「`-force` 跳过全部检查，直接进入运行过程」 | 待实现分片改 CLI：`-force` 绕过预检直接进入运行；结构非法时由运行期自然报错（给明确理由） | 裁决：**按设计（负责人口径）改实现**，不改设计 |
| SD-07 | **口径冲突③**：§12「Alpha 前无版本信息」 vs `ci/check_version.py` + `VERSION-CONSISTENCY` **反向强制版本存在**（绿 = 必然违反 §12） | 负责人指示「版本号本包不碰（代码里的 0.11.0 暂不动）」，收尾包才设 `0.0.1alpha` | 无（§12 不动） | 待改门禁：`ci/check_version.py` 改为「**存在则校验一致性、不存在不得判红**」，去掉强制版本存在的反向固化 | 裁决：**本包不动版本号**（遵负责人指示）；但**必须纠正门禁方向**，否则 CI 持续固化违规 |
| SD-08 | **口径冲突④**：§9.1「aio 为唯一 I/O」 vs `algorithms/fits_output` 直调 CFITSIO | `DESIGN-CONFORMANCE` 条目 + 前台确认 `lib/algorithms/**/fits_output` 直接调用 CFITSIO，绕过 `infrastructure/aio` | 无 | 待修：经 aio 统一出口（或按 ENGINEERING_SPEC 正式登记偏差并说明理由） | 裁决：**属架构违规，须修**（不得默认接受）；排入主线修复批次，优先度低于 P0-21/权重链 |
| SD-09 | `DESIGN-CONFORMANCE` 发现「**测试把 P0-21 单帧 bug 固化为期望**」：`tests/unit/p1001_real_nodes_test.cpp` fixture 用 **2 个 light**（`:288/:340/:530/:618`）却断言 `call_count==1`（`:324`/`:589`） | 前台独立复核确认（读源码 + fixture 定义）；全仓无 N→N 基数断言 | 无 | 待修：该断言改为**按帧数**（2 帧 ⇒ 2 次），并补 N→N 基数正/负例 | 裁决：**测试必须改**（不是改实现凑绿，而是测试固化了错误行为）。这是 P0-21 长期漏网的直接原因 |

| SD-09（更正） | DESIGN-CONFORMANCE 断言「测试把 P0-21 单帧 bug 固化为期望」（`p1001_real_nodes_test.cpp:324/:589` 的 `call_count==1`） | **前台核实后判定：审计员该条有误，P0-21 分片的反驳成立。** `runtime.cpp:167-168` 原文：「MODULE_CALL 观测：真实调用 module execute（本层每节点正常恰好一次 execute → call_count=1 为观测值）」；`context.cpp:460/496` 递增。故 `call_count` = 节点调用次数，帧循环在节点内部；断言 `==1` 正确 | 无需订正文档 | 采纳分片做法：`call_count==1` 保持不变；帧基数改由产品面断言表达（`n_products == input_lights.size()`、逐帧 signal/properties 存在、3 产品内容两两不同） | 真问题是缺少帧基数断言（覆盖缺口），不是固化 bug；已补，且故障注入实测转 RED |
| SD-10 | `CONTROL_WEIGHT_SNR.md:140`（frame_snr = 相对质量权重） vs `UNIFIED_MODEL.md` §2 / `07_noise_snr.md:55-70` / 设计 §3.4（帧级未加权通量型 SNR `F_ref/σ_F`）互斥 | 最高设计 + Horne 1986（DOI 10.1086/131801）+ SCI-AUDIT 复算 E7/E8 | 需订正 `CONTROL_WEIGHT_SNR.md`（走变更 claim） | 权重链按设计侧落地（`weight_chain.h/cpp`），并以 `FrameSnrKind` 语义门 fail-closed 拒绝质量权重冒充 | 裁决：以最高设计为准（通量型未加权 SNR）；文档走 §3 变更 claim，不得反向改设计 |
| SD-11 | `sparse_snr_layer.schema.json` 未冻结 `sparse_snr_value` 是绝对帧内 SNR 还是相对帧级因子 | 设计 §3.4:174「实际 SNR = 帧级 × 帧内」的乘法形式 ⇒ 帧内量必为无量纲相对因子 | 无（设计已足够，需在合同冻结） | 权重链按相对因子解释（`actual = frame_snr × intra_snr`） | 裁决：冻结为相对因子；要求合同侧在 schema 显式写明单位/语义并由测试锁定 |
| SD-12 | 权重链模块内 `legacy_allow_weight_fallback=true` 的处置 | 该标志造成 L3「等权走通却报成功」假绿 | 无 | 模块内已改恒 fail-closed（`unclosed_legacy_fallback_rejected`，weights 为空，`production_allowed=false`） | 裁决：接受。真正的静默降级点在 hub（`module_adapters.cpp:4938-4961`）——列入 hub 批必修 |

| SD-13 | **DC-514（阻断级）配置形态**：官方模板 `config/templates/*.phase_config.json` 为**嵌套形态**（`phase_name`/`config`/`inputs`），而 CLI/预检只认**扁平形态**（顶层 `output_dir`/`input_lights`/`master_*`）⇒ 官方模板 `export --json` **rc=2**，唯一事实源不可运行 | `ASTROCS_DESIGN.md` §3.3 输入合同**本身就是嵌套形态**（`config{precision,output_dir,sparse_snr_layer}` + `inputs[]{light,bias,dark,flat,filter}`）⇒ **嵌套形态才是设计权威**；扁平形态是实现的私有简化 | 无（§3.3 不动） | **待修（hub 批）**：实现必须接受 §3.3 嵌套形态（扁平形态可保留为兼容别名）；`inputs[]` 的逐条校准帧要支持（T2/T3 不同母版共处一个数据块）；模板必须可跑通 rc=0 | 裁决：**以设计 §3.3 嵌套形态为准**。这不是模板问题，是**实现偏离了设计输入合同**；与 P0-21 的「一组进一组出」同源（§3.3 的 `inputs[]` 就是那『一组』）。列入 hub 批必修 |

| SD-14 | **口径冲突⑤（实质）**：`docs/science/REJECTION.md` §7/§8 规定「n≤2 恒 UNDERDETERMINED、不做剔除」 vs 设计 §4.5 + FIX-REJ 映射表的 **n=2 排异能力**（`extreme_value_clip_prior_sigma`，k=Φ⁻¹(1−α/(2N))） | 冻结文档该条成文于「尚无 n=2 方法」之时；现有**外部先验 σ**（方案 A：该帧该 tile 31×31 邻域中位数/MAD）时，n=2 可做单趟极值剔除且实测无污染零误剔。设计 §4.5 是负责人意图，且 n=2 白名单正是卫星线进入叠加的直接原因（L4 实测 74% 像素 n=2） | **需订正 `REJECTION.md` §7/§8**（走 §3 变更 claim）：改为「n≤2 在**无外部先验**时 UNDERDETERMINED；**有有效先验**时按极值剔除」 | kernel 已提供能力（新方法 11 / 新 profile `astrocs_adaptive_pixel` / `p2_reject_plan_resolve_n`），未擅改冻结默认 | 裁决：(a) **订正文档**（设计优先，且有证据）；(b) **生产 mosaic 默认切 `astrocs_adaptive_pixel`**（§4.5 要求 auto 即内置映射），冻结路由测试同步更新；(c) scheduler 必须为 n=2 提供**外部 `prior_sky`**（只给 `prior_sigma` 会让中心回退中位数 → 全拒 → 冻结容错反转为全接受，排异失效） |

| SD-15 | 权重链 HiPS 键名未冻结；且 HUB-A 指出 **Phase1 当前不写帧级 SNR 键** ⇒ 生产权重链必然 fail-closed（行为正确但产出不了权重） | HUB-A 读侧已按 `ASTROCS_FRAME_SNR`/`ASTROCS_REFERENCE_FLUX` 实现；设计 §3.4 要求帧级通量型 SNR 写 HiPS 头 | 无 | 冻结键名；Phase1 写侧补写（HUB-B 任务②） | 裁决：**冻结 `ASTROCS_FRAME_SNR` + `ASTROCS_REFERENCE_FLUX`**；Phase1 必须写入，否则权重链永远 fail-closed |
| SD-16 | `coordinate_frame` 取 `icrs` 还是 `equatorial` | P0-19 已三方取证（IVOA REC-HIPS-1.0 §4.4.1 / WD-HiPS-1.0 / CDS Aladin Lite API） | 无 | HUB-A 已改 4 处（`:3961/5970/7101/7123`） | 裁决：**取 `equatorial`**，与 P0-19 同源；旧 `icrs` 产品混用会 fail-closed（可接受） |
| SD-17 | HUB-A 上呈：n=2 先验「**逐输出像素**」vs「每 tile 一次」——逐像素粗估 L4 需 ~1000-1200s（单线程） | 先验语义是「以该输出像素为中心的局部天光/噪声」（方案 A 31×31 邻域中位数/MAD），**逐像素才是科学正确**；每 tile 一次是空间近似 | 无 | 保持逐像素；性能由 benchmark 定档，**不得为性能改科学语义** | 裁决：**保持逐像素**。性能是后话，科学优先 |

## 4. 上呈事项（穷尽：仅 §3a 列明类别）

| 事项 | 证据 | 方案与代价 | agent 推荐 |
|---|---|---|---|
|  |  |  |  |

## 5. 提交台账

| commit | 目的 |
|---|---|
| `3de869ae` | DOC-101 文档包替换与核验（36 篇；U-1/U-2 闭合；核验器 R4 判据改进 + self-test；ASTROCS_DESIGN §235 口径订正） |
| `958fa5b7` | DOC-101 提交台账登记 |
| `25b159e2` | FIX-SCI 两项冻结科学文档 P0 订正 + 4 件变更 claim（DRIZZLE §5/§7 归一化、ASTROMETRY §5a 标签；登记 DISP-DRZ-009） |
