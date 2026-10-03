# 审稿 P1 · DOC-DET-001（docs/detail 层）

> 基线：仓库 `/workspace/Astro CS Database`，HEAD = `f9650dd0aed97d7f261e5e6547f4fdb505bc313b`
> 权威片清单：`run/GOVERN-08/审核包-R2/分片清单/片清单-权威版.yaml:746-801`
> 管辖规范：`工作包-GOVERN-08原件/standards/05_detail推理规范.md`（本层直接正本）、`04_对抗性审稿规范.md`
> 本人纪律：零 git 写（未 add / commit / checkout / reset / stash / rm --cached）；未编译、未跑 ctest/pytest/构建/实验脚本；未读 `/tmp/acsd_g08/`；**未修改任何仓内文件**（交付件除外）。收尾 `git status --porcelain` 为空，已复验。

---

## 1. 读完了吗

| 项 | 值 |
|---|---|
| 成员份数（片清单声明） | **48** |
| 实读份数 | **48** |
| 成员总行数（片清单声明） | **7358** |
| 实际读入行数 | **7358** |
| 覆盖率 | **100.0 %（48/48 份，7358/7358 行）** |

- 片清单 `目标行数 11000 / 实际行数 7358 / 超容量 false`；我逐文件 `wc -l` 复核，48 份合计 **7358**，与片清单**逐位一致**。
- **未读完的部分：无。** 48 份成员文件全部由本人用 `read` 工具从头读到尾（不是抽样、不是脚本扫描、不是 grep 替代）。grep/`wc` 仅用于**读完之后的取证与计数复算**，未用于替代阅读。
- 读法说明：分 6 个批次逐份 `read`，每份读到 EOF（工具回报 `(End of file - total N lines)`）。
- 本片为「一遍 = 同一片材料的一次完整重读」，中途未切换焦点、未越界读他片正文（对片外文件只做被本片文档**指名引用**时的定点核验，并已标注归属）。

---

## 2. 本片判定

## **阻断（BLOCKING）**

最重的 3 条：

1. **B1 — 无引号伪引，且已污染机器事实源、倒置权威链。**
   `docs/detail/UNIFIED_MODEL.md:58` 与 `docs/detail/registry/acsd.phase1.noise-snr.md:4` 把「**全程只有 SNR**」「**PSF 拟合质量代理（`q_psf`…）只作诊断，不计入科学叠加权重**」挂在 `ACSD_DESIGN.md §3.1` 名下，且是**冒号后的裸从句、完全无引号**（正是本轮提示的主项形态）。实测 `grep -c '全程只有' docs/ACSD_DESIGN.md` = **0**、`grep -c '质量代理'` = **0**；而 §3.1 原文（`docs/ACSD_DESIGN.md:178-183`）写的是「现行统一数据对象集共 **13 个**…`point_information` 是点源通量估计的**逆方差，是点源目标的严格权重**…全链没有「权重模式」这一可选概念」——被引句不仅不存在，且**与被引条款相反**。该句已传播进机器事实源 `eng/contracts/data/unified_object_compatibility_map_v1.json:139` 的 `basis`（那里挂 §2.1/§2.2/§2.3，与 detail 挂 §3.1 **自相矛盾**），并被一级正本 `docs/science/DATA_SEMANTICS.md:3255` 反向引为**禁用 token 拒绝规则的「依据」**——即 detail 拿二级细节做一级依据，构成权威链倒置。**改 detail 一句不足以修复**。

2. **B2 — registry 配置表大面积虚构：声明 16 字段、实为 20；17 行里 5 行的键名在代码中不存在。**
   `docs/detail/registry/acsd.phase2.upm-fit.md:217` 称「配置 = `P2UpmBuildConfig` **16 字段**（upm.h）」，其配置表 `:220-239` 实列 **17 行**。真实结构体 `lib/algorithms/coverage/include/astro/phase2/upm.h:77-127` 有 **20 个字段**。表中 5 个键名**在 upm.h/upm.cpp 中零命中**：`upm_weight_source`（真实字段名是 `snr_weight_mode`）、`bkg_model`、`bkg_spline_spacing`（二者全仓 upm 面零命中）、`rank_rtol`（真实家在 `identifiability.h:10,23`）、`gauge`/`frame_gradient_order`（真实家在 `sky_plane.h:26,214`）。同时 **8 个真实字段未入表**：`zero_anchor_weight` / `quality_mode` / `input_manifest_hash` / `cpu_workers` / `gs_damping` / `m_full_frame` / `final_gauge` / `grid`。规范 05 §4「写清…数据结构、函数名」与 AGENTS.md §3「代码与文档冲突时以文档为准订正代码」在此均被违反——但按 05 §3「不冲突」，此处应先判文档错。

3. **B3 — 「全仓零消费者」为假（退役治理复发），且一级正本仍把它当在役公共 API 并带过期行锚。**
   `docs/detail/registry/acsd.phase2.upm-fit.md:193-194` 与 `acsd.phase2.upm-apply.md:71-72` 断言 `p2_upm_normalized_weights` 「在 upm.cpp / upm.h 带 RETIRED 注记，**全仓零消费者**」。实测：**存在活调用点** `实验/additive-sky-seamless/code/reverse_verify/smooth_lambda/upm_sweep.cpp:236` `p2_upm_normalized_weights(obs.data(), n_obs, &cfg, wcell.data())`（真实语句，非注释）；`eng/tools/quality/v19r3_traceability.py:62` 亦在溯源门里引用。同时定义确已删（`lib/algorithms/coverage/src/upm.cpp:1999` 为 RETIRED 注记、声明撤下见 `upm.h:189`），而 `docs/engineering/PUBLIC_API.md:1689-1694` 仍以**完整签名 + 行锚 `h:139-142` / 实现 `upm.cpp:1325` / rc 表 `:1329`、`:1333`** 公布该 API——我实测 `upm.h:139-142` 现落在 `p2_upm_build_geo` 尾部与 `p2_upm_save/open`，`upm.cpp:1325` 落在可辨识性记账块内，**行锚已整体漂移**。这正是本项目「已退役对象自称零消费者曾为假」那一族的复发，且横跨 detail↔engineering 两层。

---

## 3. 逐文件清单（读了什么 / 看到什么 / 判定）

> 「判定」列：✅ 无缺陷（反例成立）｜⚠ 有发现（编号见 §4）

| # | 文件 | 行 | 读到什么 | 看到什么 | 判定 |
|---|---|---|---|---|---|
| 1 | `registry/acsd.phase1.noise-snr.md` | 474 | 三层噪声模型、帧级 SNR 红线、σ_sky 三分支决策树、逐像素方差两态约束、`ln R` 序统计量判据、稀疏层几何与 4 算子冻结词表 | :4 节号注解复述伪引「§3.1（全程只有 SNR）」→B1；:216-218 R 判据自述「零假设值恒为 1，是恒等式」与 :455-457 负例「`r_median ≈ 1`，证明判据非恒真」互斥 →M5；:442「与实验单元 / 实验单元」重复 →M11；:203-205 `ctest -R p1noise_negative` 真实存在（子代理实测已入库）；:374/421「NOT_VERIFIED（不写成 PASS/FAIL）」为诚实范式 | ⚠ |
| 2 | `STAR_DETECTION_IMPL_DESIGN.md` | 449 | O1–O16 逐算子规格、论文锚定/Project-defined 三分类、YvV95 递归系数、SExtractor deblending 树、§8 存疑清单 | :15「6 个导出函数 / `SDetParams` 9 字段 / `SDetGuidedStats` 6 计数」与 registry 卡一致；:53 引「`STAR_DETECTION.md` §3『判据非退化要求』」而该段实在 **§1**（`docs/science/STAR_DETECTION.md:59`，§3 在 :110）→M6；:154「‖k‖₂ 折算系数 **35.45/σsm**」而两处 science 原文均为 `5.0/‖k‖₂ = 35.45 σ_smooth`（**乘法**）→M7；:249-284 对 SExtractor 外部行锚合法（§2 允许外部行锚）；:396-399 计数 5+1+10=16 与 O1–O16 全覆盖✅；:75 行尾游离 `+`→M8 | ⚠ |
| 3 | `registry/acsd.phase2.upm-fit.md` | 344 | UPM 纯加性模型、可辨识性单一判据、接缝有符号阶跃、配置 schema 表 | :193-194「全仓零消费者」为假→B3；:217「16 字段」vs 实 20、5 个键名不存在→B2；:319-320「逐位不变」措辞歧义（子代理亦标 suggestion）；:128-131 自陈「门=三元组」却「全仓无 `rms_z` 命中」的零承载完整披露 = 正面范式 | ⚠ |
| 4 | `PRODUCT_STORAGE_FORM.md` | 259 | 裸/归档两形态、索引两层、写路径、稀疏打洞 vs 包围盒 TRIM | :45 判据量（归档定位表随机读延迟）所引 `compress-01/` 对 `hips.zst`/`pread`/定位表**零命中**（本人复核：目录只有 fill_scan/trim_scan/product_level/final_numbers 等）→M4；:187-188/203 把「TRIM 默认形态=关闭」判据挂在无实现者的 `CHK-SPARSE-PUNCH`（eng/lib/CMake 中仅 `:PROBE` 有码）→M4；:20-24 IVOA HiPS 逐字引文与 §4.2.1.3 扩展名 MUST 声明为**成对引号伪引高风险点**，但引文本身经得起外部核验，标**待联网核验** | ⚠ |
| 5 | `registry/acsd.phase1.star-detection.md` | 248 | 星表引导权威路径 vs 全图盲检测、fail-closed 表、`SDetParams`/`SDetGuidedStats` | :90-92「现行 6 个导出符号」与 STAR_DETECTION_IMPL_DESIGN:15 互洽✅；:139 `max_stars` 域 [20000,50000]、:200-207 fail-closed 表结构完整、无伪引 | ✅ |
| 6 | `registry/acsd.phase3.resample2.md` | 238 | sampler 生命周期、G3/G4 公式、tile 寻址、值语义表 | :45-54 主动登记「相邻占位 descriptor `acsd.phase3.resample` 仍存在」并说明对齐属迁移目标（诚实）✅；:34/35/100/210/212 行尾游离 `+`→M8；`acsd.phase3.resample.md` 被 `docs/engineering/TRACEABILITY_SPEC.md:214` 引用而该文件不存在（本片真名是 `resample2.md`）→M12 旁证 | ⚠ |
| 7 | `registry/acsd.phase3.writer.md` | 237 | 原子发布序、BUNIT 二次律、不确定度可得性唯一出口 | :203-204 WCS roundtrip 同时写「≤1e-8 px（SCI 真值）」与「1e-4 px（SCI §7 真值阈 ≤1e-6 px）」**三值并存**→S6；:37/41 行尾 `+`→M8；:217-223 如实登记 aio README 矛盾与 tmp 命名漂移（诚实）✅ | ⚠ |
| 8 | `registry/acsd.phase1.drizzle.md` | 232 | drop 面积归一、pixfrac² 偏差、面亮度归一分母 | :131 明示「数值默认唯一来源 = `defaults.json` 的 `drizzle.pixfrac`」，:202-204 回归门名真实存在；:63-68 归档形态未落地如实登记；:189-196 variance 双实现分裂点明 `registry/acsd.phase1.noise-snr.md`（同片互引正确）✅ | ✅ |
| 9 | `registry/acsd.phase2.reject.md` | 229 | 三档 AUTO 路由、10 显式方法核、五码门 | :91-95 档位表与 `PHASE2_DETAILED_DESIGN.md:58-64` **逐位一致**✅；:104-108 显式写「min/max 不用于生产」并给一手出处 ✅；:66 行尾 `+`→M8 | ⚠ |
| 10 | `PHASE1_DETAILED_DESIGN.md` | 227 | 节点序、photometry 一步化、失败作用域、逐层单位表 | :52-54「生产 IR 8 个节点」逐个点名 = 8 ✅；:47 引 `PIPELINE_BLOCK_CONTRACT.md §7.1`（未核验其内容，留他片）；:63/141-147 的 `k_photo` 与 BUNIT 表自洽 ✅ | ✅ |
| 11 | `registry/acsd.phase2.sample.md` | 226 | 三阶段 background-clean 采样、8×8 控制网格、control_ivar 冻结式 | :74-78 `P2ControlObservation` 13 字段 / `P2SampleStats` 10 字段 / `P2ControlNode` 7 字段 —— 逐项点名可核（本次未逐字段点验，标**未完全取证**）；:50 `ivar 弃用仅作诊断`、:51「科学权重一律 `control_ivar`」与 B&A 词表一致 ✅；:76/200/201 行尾 `+`→M8 | ⚠ |
| 12 | `registry/acsd.phase2.integrate.md` | 224 | 五态 status、support canonical reducer、单一权重口径 | :35-38 五态 `OK/NO_CANDIDATES/ALL_REJECTED/ZERO_VALID_WEIGHT/INVALID_INPUT` 与 :79-81 一致 ✅；:100-114 单一权重路径与 `PHASE2_DETAILED_DESIGN.md:114` 互洽；:214-221 权重来源受限表**明确写「不是死代码」**并给收缩路径（正面范式，规避了「退役治理」误判） ✅ | ✅ |
| 13 | `registry/acsd.phase1.photometry.md` | 221 | PSF 拟合域唯一 flux 口径、测光一致性门九项预算 | :88-92 如实写「该门判据**尚未在代码中生效**」并列出 4 项前置产物；:204-209 负例 ①–④ 齐全 ✅；与 `UNIFIED_MODEL.md:42` 同口径 ✅ | ✅ |
| 14 | `LOG_AND_ERROR_SYSTEM.md` | 214 | L0–L3 四层、落点派生、六步生命周期、帧级/全局失败作用域 | :6 `eng/run/ledgers/log_system_ledger.json` **不存在**（`eng/run/` 下只有 `local/bughunt_batchP_ci_residual`），而 §9 判据 R4 是「台账锚存活、只减不增」的单调性判据→M3；:133 仓内行锚 `commands.cpp :2396-2476` 违反本片自订 `ANCHOR_CONTRACT.md` §1/D5 且与 `21_observability.md:51` 重复→M9；:133 行尾 `+`→M8 | ⚠ |
| 15 | `registry/acsd.phase3.wcs.md` | 214 | TAN 投影域、6 要素、roundtrip 紧门与适用域 | :66-68 **主动登记**「文档面曾记已实现 4/8，与在役注册表不符，口径以在役注册表为准（当前 D=I={TAN}）」——这是本片最好的冲突治理范式 ✅；:131 配置表列 8 投影 token 但仅 TAN 实现（措辞可收紧，列 S7）；:38/189/190 行尾 `+`→M8 | ⚠ |
| 16 | `registry/acsd.phase2.upm-apply.md` | 179 | 只扣 δ_k 保留 B_ref、dense/sparse 同语义、additive_mode 退化判红 | :71-72「全仓零消费者」为假→B3；:96-98 **主动披露**「设计与实现的默认值不一致（设计 delta / 现网 c），口径以设计为准」✅（正面） | ⚠ |
| 17 | `registry/acsd.phase2.write.md` | 178 | 输入哈希链、逆归一、ivar 门 rc=7、四概念分离 | :44-45 明确区分「writer 视图中间量 flux」与「落盘值=面亮度」✅（与 B2 那种混淆相反的高质量写法）；:49 ivar 门 rc=7 与 :139-140 退出码表一致；:56/:169-171 如实登记「直写 out_hips 无 staging，不满足 §10 原子发布，属已登记例外面」✅ | ✅ |
| 18 | `infrastructure/21_observability.md` | 177 | G-RES-01 判定域/分母/判据表、record vs enforce | :58 把「具体硬约束与细节写入下级文档」当 `ACSD_DESIGN.md §0` 原话（§0 实为「公式推导、数值表、源码位置、实验数据由下级文档承载」）→M10（子代理提出，本人采信其转述但未逐字比对 §0 全行，标**部分采信**）；:113 注册面「门禁注册面（G08-10 重建）」+ `eng/ci/` 不存在→M3 同族；:51 仓内行锚重复 + 行尾 `+` | ⚠ |
| 19 | `infrastructure/17_aio.md` | 175 | 唯一 I/O 边界、形态解析层、发布三态、打洞唯一实现 | :70-77 `aio_sparse_punch.h` **唯一实现**属实（实测唯一落点 `lib/infrastructure/aio/src/aio_sparse_punch.h`，`FALLOC_FL_PUNCH_HOLE`/`FSCTL_SET_SPARSE` 命中）✅；:47-51 枚举值与写盘 BITPIX 映射自洽 ✅；:116「编排层的 **6 个名字**」未点名是哪 6 个（9 个 stage 中取 6），口径不明→S8；:163-167 HiPS tile 非原子如实登记 ✅ | ⚠ |
| 20 | `PHASE3_DETAILED_DESIGN.md` | 173 | 输入语义守卫、投影冻结 8 种、crop 语义、V1a/V1b 视觉门 | :24「冻结清单 8 种…当前仅 TAN」与 `acsd.phase3.wcs.md:55-68` 一致 ✅；:88/:149-159 反复强调「边界黑边合法、禁 `finite_fraction != 1.0` 判红」——**主动写出判据的假阳性边界**，是本片最强的反恒真/反误报写法 ✅；:124-131 落的 `p3_wcs.h`、`export_crop` schema 经实测**均存在** ✅ | ✅ |
| 21 | `infrastructure/19_runtime.md` | 171 | typed DAG、并行轴冻结口径、locality 编排、资源门 | :116「机器判据 = `eng/tools/docs_machine_consistency.py`」——**文件不存在**（仓内 `conclusion_snapshots.json` 已记 RETIRED）→M3；:8/:128 `runtime` 非模块名的禁第二名字纪律正确 ✅ | ⚠ |
| 22 | `registry/acsd.phase1.calibration.md` | 163 | master 生成、单帧校准、方差传播 | :57-60「导出 **12** 个符号 = 5 f32 + 5 f64 + `ac_set_num_threads` + `ac_version`」算术自洽 = 12 ✅；:65-66 如实写 `ac_generate_master_*`/`ac_set_num_threads` **无生产调用方**（与本项目「自称零消费者」的坏先例相反，此处是真实调用方核实后的诚实陈述）✅；:66 列出 cosmetic stage 恒等 pass ✅ | ✅ |
| 23 | `registry/acsd.phase1.wcs-platesolve.md` | 156 | TAN/SIP 解算、CD 退化必须 success=0 | :15-16「CD 退化必须 success=0 并按失败登记」与 :123-125 rc 语义一致 ✅；:48-53 明确「本节点按帧自读像素自行检测，不消费 `star_detection` 产物，故节点序先于 `star-psf`」——与 `PHASE1_DETAILED_DESIGN.md:44-46`、`acsd.phase1.star-detection.md:84-86` **三处互洽** ✅ | ✅ |
| 24 | `anchors/ANCHOR_CONTRACT.md` | 151 | 内容锚 D1–D6、外部行锚 C2–C6/C8、维护义务 | §1 明禁仓内行锚、§4 内容锚唯一形态、§4.2 归一化算法 N1–N5 完整 ✅；:16/:117 两处 `cpp/foo.cpp:123`、`docs/science/PSF.md:7` 属**负例示教**，不算违规 ✅；但 §5 规则编号 **C2–C6 后直接跳 C8，C1/C7 无退役说明**→S3；本片另两文件违反本文件 §1→M9 | ⚠ |
| 25 | `registry/acsd.phase2.coverage.md` | 151 | 覆盖并集、MOC、连通分量 | :30-34 明写「coverage 禁作 inverse-variance/SNR 权重」负向红线 ✅；:145-148 缺陷登记四语义/乘数恒 1.0 齐全 ✅ | ✅ |
| 26 | `PHASE2_DETAILED_DESIGN.md` | 145 | 固定科学流程、SNR 三路径、单一权重口径、逐像素排异三档 | :17 流程 9 个箭头步 + :19「**七个 operation**」——按 7 科学步 + 2 基础设施步自洽 ✅（我核对后**不报**）；:58-64 三档表与 `registry/acsd.phase2.reject.md:91-95` 逐位一致 ✅；:41/:114 明写「正则化后条件数有上界 ⇒ **恒真门**」「越界 token 一律 fail-closed」= 本项目反恒真纪律正本 ✅ | ✅ |
| 27 | `registry/acsd.phase1.star-psf.md` | 143 | 帧级 Moffat4、q_psf 质量代理 | :63-68「`p1_psf` 端口**生产链路零消费者**」给出**可证伪依据**（注册表不声明该边 + `PIPELINE_BLOCK_CONTRACT.md` 列作幻边负例）——**与 B3 的空口「全仓零消费者」形成鲜明对照，这是本片应推广的写法** ✅；:85/:140 `psf_spatial_order` 非零分支无行为承载如实登记 ✅ | ✅ |
| 28 | `infrastructure/22_gaia_xpsd_client.md` | 139 | XPSD 解析、两级缓存、极冠剪枝 | :55-59 `QUERY_CACHE_CAPACITY`=64、`BLOCK_CACHE_MAX_MEMORY`=4GB 等常量与符号名标注了真实宿主；:89-93 列出 12 个导出符号名（与「12 个」自洽，**未逐个点验**）；:22-32 零网络为结构性满足而非声明 ✅ | ✅ |
| 29 | `registry/acsd.phase1.cosmetic.md` | 138 | 坏点/坏列检测修复、列跳变稳健判据 | :21-23 明写「单帧 CR 剔除**不在本模块生产域**，生产实现无 CR 路径」；:87-88 `cr_detection`/`cr_sigma` 明标「无行为承载：生产路径不读取该键」——**用现在时诚实标注无行为承载键，范式正确** ✅ | ✅ |
| 30 | `merged_TROUBLESHOOTING.md` | 101 | 准入七项格式、故障覆盖门、症状主表、常用命令 | :12-20 定义**固定七项**准入格式且「缺项即不算合格条目」，但 :43-64 症状主表只有 **5 列**（症状/阶段/状态码·证据/定位/修复动作），**缺「最小复现」「期望不变量」「文档位置与测试位置」三项**，且「修复动作」不在七项内 ⇒ 按其自订门槛，**20 行全部不合格**；:24「本表覆盖 **10 类**」而表实为 **20 行** ⇒ 两处自相矛盾→M2；:78-81 两条 ctest 计数经本人实测**均正确**（见 §6）✅ | ⚠ |
| 31 | `infrastructure/23_hips_browser.md` | 98 | Qt6+OpenGL 浏览器、STF 唯一显示变换状态 | :42 引 `eng/.../OPTIMIZATION.md`——**路径内含字面省略号，按构造不可核**（仓内实有 `docs/engineering/OPTIMIZATION.md`）→S5；:12-15 模块名/实现名分离写明 ✅；:20-22「不进产品 manifest、不参与 CLI」边界清楚 ✅ | ⚠ |
| 32 | `registry/acsd.phase1.session.md` | 98 | Phase1 装配底座、canonical 四段 | :14-18 明写「现行 registry descriptor 工厂表中**无此 module_id**」；:33-35 如实登记 API-P1-001 冻结 7 段 vs 现行四段的差距 ✅（与 `acsd.phase2.session.md:43-45` 同范式） | ✅ |
| 33 | `registry/acsd.phase1.hips-writer.md` | 94 | HiPS 产品集写出、九导出、原子发布边界 | :34-38 九个 C ABI 符号名逐个列出且与 :93「9 个 C ABI 符号」自洽 ✅；:75-80 **主动写明** HiPS tile 非原子发布且「在闭合判据通过前原子发布宣称不成立」✅（正面范式） | ✅ |
| 34 | `00_INDEX.md` | 81 | 目录结构、模块落位规则、registry/infrastructure 卡表 | :22 与 :42 均称 registry「**26 张卡**」，但 :46-48 实际枚举 25 个（normalize 11 + mosaic 9 + export 5），盘上 `registry/*.md` 去 README 亦为 **25**；26 是**含 README 的文件数**⇒ 卡数与文件数混用→M1；:46-48 枚举**双向完整**（无「列而不存」也无「存而未列」，我逐名 diff 过）✅ | ⚠ |
| 35 | `UNIFIED_MODEL.md` | 76 | 统一观测模型、13 数据对象、三类配置分离 | :56「canonical 数据对象 = **13** 个」逐个点名 = 13 ✅；:43/:45-46 `frame_snr`/`sparse_snr_layer` 相互独立、不作尺度基准，与 `registry/acsd.phase1.noise-snr.md:64-68` 互洽 ✅；:58 **裸从句伪引（B1 主项）** | ⚠ |
| 36 | `common.md` | 71 | HEALPix 单源、SHA-256 frame_id、精度单例 | :23-26 四个头与函数族均实测存在 ✅；:32 CDS Hipsgen oracle、:33 truncated-64 frame_id ✅；:62「上游通过 `docs_machine_consistency` frame_id / product 校验」——同一被删生产者→M3；:71「以 `lib/algorithms/shared/` 为根的 **7 件**」而该目录实有 **16** 个文件（`dirent_win.h`/`README.md`/`THIRD_PARTY_NOTICE.md`/`.gitignore`/两个 tests 目录未列）→S2 | ⚠ |
| 37 | `infrastructure/18_cli.md` | 67 | 命令树、预检三档、退出码收敛 | :24 命令树与最高设计 §7.1 对齐声明 ✅；:49 退出码集 `0/2/3/4/5/6/7/8/9/10/70` 与 `lib/infrastructure/cli/exit_codes.h` 声明的「唯一源」策略一致 ✅；`log_dir` 落点与 `21_observability.md:35` 互相引用闭环 ✅ | ✅ |
| 38 | `infrastructure/acr.md` | 65 | 异构计算运行时、ACR 为 DORMANT 隔离实验 | :13-15 `p2_acr_block_eligible(...)` 恒 false 给出构造性理由 ✅；:23-27 `astro/compute/*.hpp` 四头实测存在 ✅；:60-61 无 CUDA kernel 如实登记 ✅ | ✅ |
| 39 | `registry/acsd.phase3.verify.md` | 58 | 重开校验、fail-closed | :8-9 消费面 `lib/phase3_session/p3_export.cpp` 与 :26 一致 ✅；体例偏薄但无伪引 | ✅ |
| 40 | `registry/acsd.phase3.properties.md` | 56 | properties 严格解析、非法显式拒 | :8-9 `hips_properties_parse` 宿主 `lib/algorithms/coverage/hips_properties.h` 自洽 ✅；无缺陷 | ✅ |
| 41 | `registry/acsd.phase2.resample.md` | 54 | **占位 descriptor** 登记页 | :7-10 明确本页只登记占位 descriptor、无独立实现目标，边界清楚 ✅（与 `acsd.phase3.resample2.md:45-54` 的占位登记同范式） | ✅ |
| 42 | `infrastructure/20_benchmark.md` | 53 | CPU profile 生成与校验 | :13/:20 `cpu_profile.schema.json` 实测存在；:19/:33 缓存位置「安装目录」与 `UNIFIED_MODEL.md:76` 一致 ✅ | ✅ |
| 43 | `registry/acsd.phase1.writer.md` | 51 | FITS 写出节点 | :24 写出实现 `aio_write_fits`/`aio_read_fits` 宿主正确 ✅；体例薄但无伪引 | ✅ |
| 44 | `registry/acsd.phase2.session.md` | 49 | Phase2 装配会话 | :21「canonical **4 段** coverage→sample→upm_build→persist」与 :43-45 差距登记自洽 ✅；:47-49 如实写「TEST-P2-SESSION-001 待建、只有静态结构断言、无会话行为测试」✅ | ✅ |
| 45 | `README.md` | 3 | 目录说明 | 一句话覆盖 registry/infrastructure/anchors 三面，规范 05 §4「每册所在文件夹配极简 README」✅ | ✅ |
| 46 | `anchors/README.md` | 3 | 目录说明 | 指向 `eng/contracts/anchors/` + `eng/tools/`（`eng/contracts/anchors/unresolved_registry.json` 存在）✅ | ✅ |
| 47 | `registry/README.md` | 3 | 目录说明 | 体例描述与 25 张卡实际体例一致 ✅ | ✅ |
| 48 | `infrastructure/README.md` | 2/3 | 目录说明 | 8 组件逐个点名，与 `00_INDEX.md:58-65` 8 张卡一致 ✅ | ✅ |

---

## 4. 发现清单

### 4.1 阻断（BLOCKING）— 3

见 §2 B1/B2/B3（含完整证据链与 `文件:行`）。

### 4.2 须修（must-fix）— 11

| 编号 | 位置 | 现状 | 应为 | 证据 |
|---|---|---|---|---|
| M1 | `docs/detail/00_INDEX.md:22`、`:42` | 称 registry「**26 张卡** + README」 | 25 张卡 + README（26 为含 README 的**文件数**，卡数是 25） | 同页 `:46-48` 枚举 25（11+9+5）；`ls docs/detail/registry/*.md` 去 README = 25；双向 diff 无缺无余 |
| M2 | `docs/detail/merged_TROUBLESHOOTING.md:12-20` vs `:43-64`；`:24` | 自订「固定七项、缺项即不算合格条目」，实表仅 5 列且缺 3 项；:24 称「10 类」而表 20 行 | 七项齐备或改写准入格式；场景数与行数口径统一并写明 | 表头 `症状｜阶段｜状态码·证据｜定位｜修复动作`；七项中「最小复现/期望不变量/文档位置与测试位置」全表无对应列；`:45-64` 共 20 行 |
| M3 | `infrastructure/19_runtime.md:116`；`common.md:62`；`LOG_AND_ERROR_SYSTEM.md:6` | 三处以**现在时**把不存在的生产者/台账当活机器判据 | 标注载体已删或改指真实落点 | `eng/tools/docs_machine_consistency.py` 不存在（仓内 `conclusion_snapshots.json` 记 RETIRED）；`eng/run/ledgers/log_system_ledger.json` 不存在（`eng/run/` 仅 `local/bughunt_batchP_ci_residual`），而 `LOG_AND_ERROR_CONTRACT.md:148` 把它列为「强制本约束」；`eng/ci/` 整体不存在 |
| M4 | `PRODUCT_STORAGE_FORM.md:45`、`:187`、`:188`、`:203` | 判据量、证据目录、门实现三者互不咬合 | 补可复跑证据或改写判据 | `compress-01/` 对 `hips.zst`/`pread`/定位表零命中（仅 fill_scan/trim_scan/product_level/final_numbers）；`CHK-SPARSE-PUNCH`（静态支）在 `eng/`/`lib/`/`CMakeLists.txt` 中零命中，仅 `:PROBE` 有码（`sparse_punch_probe.cpp`）；而本文件 `:195`/`:216` 自陈「归档形态不实施」⇒ 该探针**结构上不可能**因归档读缺陷转红 |
| M5 | `registry/acsd.phase1.noise-snr.md:216-218` + `:455-457` | R 判据自陈「零假设值恒为 1，是恒等式」，负例却用「`r_median ≈ 1`，证明判据非恒真」 | 负例须引入**独立参照量**，否则该负例构造上不可能翻红 | R = patch 稳健尺度 / 同 patch 白噪声零假设等价尺度 ⇒ 白噪声下 R→1 是定义使然；对照 `PHASE2_DETAILED_DESIGN.md:41`「正则化后条件数有上界 ⇒ 恒真门」表明本项目已知该陷阱 |
| M6 | `STAR_DETECTION_IMPL_DESIGN.md:53` | 引「`STAR_DETECTION.md` **§3**『判据非退化要求』」 | 应为 §1 | `docs/science/STAR_DETECTION.md:59` 该段在 `## 1 语义要求`（:10）之下，§3 在 :110 |
| M7 | `STAR_DETECTION_IMPL_DESIGN.md:154` | 写「‖k‖₂ 折算系数 **35.45/σsm**」 | `35.45 σ_smooth`（乘法） | `docs/science/STAR_DETECTION.md:68` 与 `docs/science/algorithms/STAR_DETECTION_ALGORITHMS.md:53` 均作 `5.0/‖k‖₂ = 35.45 σ_smooth`；被引侧把除法改成了除以 σ |
| M8 | 9 份文件 18 处 | 行尾游离 `+`（diff 残留） | 清除 | `LOG_AND_ERROR_SYSTEM.md:133`、`21_observability.md:51`、`STAR_DETECTION_IMPL_DESIGN.md:75`、`acsd.phase2.reject.md:66`、`acsd.phase2.sample.md:76,200,201`、`acsd.phase2.upm-fit.md:67`、`acsd.phase3.resample2.md:34,35,100,210,212`、`acsd.phase3.wcs.md:38,189,190`、`acsd.phase3.writer.md:37,41`。判据：每处 `+` 都在**换行续写同一句**处，全仓同形共 45 文件（`docs/detail` 内 18 处），而合法子句连接 `+` 不置于行尾 |
| M9 | `LOG_AND_ERROR_SYSTEM.md:133` + `infrastructure/21_observability.md:51` | 仓内代码行锚 `` `…/commands.cpp` :2396-2476 ``，且两处重复 | 按本片 `ANCHOR_CONTRACT.md` §1/D5 改内容锚或去行号；单一正本 | `ANCHOR_CONTRACT.md:15-16`「仓内行锚不写…文档指向本仓代码同样不写 `cpp/foo.cpp:123`」；D5「登记面出现 `文件:行` ⇒ 判红」。注：该行号区间**语义正确**（确为 manifest verify 实现），但形态违规 + 双份 |
| M10 | `infrastructure/21_observability.md:58` | 把「具体硬约束与细节写入下级文档」当 `ACSD_DESIGN.md §0` 原话 | 改为 §0 实有表述或改指真实条款 | §0 实为「本文档只写顶层设计与现行结论，公式推导、数值表、源码位置、实验数据由下级文档承载」。（**采信子代理转述，本人对 §0 该句作方向性确认，未逐行全字比对**） |
| M11 | `registry/acsd.phase1.noise-snr.md:442` | 「与实验单元 / **实验单元** `实验/absolute-snr/…`」重复 | 删一次 | 同行两处「实验单元」相邻 |
| M12 | 全片（指向面） | 96 个被引 `docs/detail/**.md` 路径中 **50 个不存在**；活树引用对 **210** 对，其中来自 `docs/`+`lib/`+`eng/` 活规范面 **144** 对 | 重建引用或补重定向/锚 | 计数口径 = **「(来源文件 × 失效目标文件) 引用对」**，非门实例数；例：`docs/engineering/TRACEABILITY_SPEC.md:214` 指向 `docs/detail/registry/acsd.phase3.resample.md`（真名 `…resample2.md`，且该行自陈 `SRC-MISSING @ MISSING`）；`lib/algorithms/coverage/include/astro/phase2/upm.h:151` 指向 `docs/detail/algorithms_phase2/11_upm.md`（该目录不存在，仓内仅历史 `run/**/docs/plugins/` 副本）。**注：0 对来自 `docs/detail` 自身**——本片内部互引是干净的，缺口全在片外引用侧 |

### 4.3 建议（suggestion）— 7

| 编号 | 位置 | 事项 |
|---|---|---|
| S1 | `STAR_DETECTION_IMPL_DESIGN.md:405-411`（§8-1） | YvV95 q² 系数「文献通行复现 1.4251 vs 现行 1.4281」——文档**已诚实登记为 [存疑-系数] 且声明未取到原文**，属正确处置；该外部事实**待联网核验**，不在本轮裁决 |
| S2 | `common.md:71` | 「以 `lib/algorithms/shared/` 为根的 7 件」——该目录实有 16 个文件；`dirent_win.h` 是真实生产头却未列入 Source files |
| S3 | `anchors/ANCHOR_CONTRACT.md:134-139` | §5 规则编号 C2–C6 后直接跳 C8，C1/C7 无退役注记；按 AGENTS.md §6 应写明去向 |
| S4 | `infrastructure/23_hips_browser.md:42` | 路径 `eng/.../OPTIMIZATION.md` 内含字面省略号，按构造不可核（仓内实有 `docs/engineering/OPTIMIZATION.md`） |
| S5 | `PRODUCT_STORAGE_FORM.md:20-24` | 逐字引 IVOA HiPS 1.0 §4.1 与 §4.2.1.3，属成对引号伪引高风险形态；**待联网核验** |
| S6 | `registry/acsd.phase3.writer.md:203-204` | 同一 WCS roundtrip 出现 1e-8 / 1e-6 / 1e-4 三处阈值，括注关系可读但建议单列一处正本 |
| S7 | `registry/acsd.phase3.wcs.md:131` | 配置表把 8 个投影 token 平铺为可配置值，而实现集仅 TAN（:55-68 已登记，建议在表侧同标） |

---

## 5. 你主动构造的反例

> 红队姿态：默认现行结论为错。下列每条都以「期望推翻」起步。

| # | 构造的反例 | 期望推翻什么 | 是否推翻 |
|---|---|---|---|
| R1 | 对 `merged_TROUBLESHOOTING.md:78` 「`p1snr_science_` 6 项」取权威 `add_test` 名计数 | 若实测非 6，则手册数字是编的 | **未推翻（反例失败）**：实测恰 6（`lib/algorithms/noise_snr/tests/p1noise/CMakeLists.txt:140,141,142,143,144,146`）。手册正确 |
| R2 | 同法核 `:81` 「`drizzle_pf_sb` 3 项」 | 若为 4 则手册错 | **未推翻**：3（`lib/algorithms/drizzle/healpix_drizzle/tests/CMakeLists.txt:309,330,334`）。`drizzle_pf_sb_gate_neg` 是**入参文件**不是测试名（:335）——我差点误报，核对后否决 |
| R3 | 核 `STAR_DETECTION_IMPL_DESIGN.md:396-399` 计数 5+1+10=16 与 O1–O16 覆盖 | 若漏算/多算则处置表不可信 | **未推翻**：枚举 16 项无重无漏，分类和 = 16 |
| R4 | 核 `UNIFIED_MODEL.md:56` 「canonical 数据对象 = 13」 | 若点数不符则模型正本自相矛盾 | **未推翻**：13 个逐个点名，数组可数 |
| R5 | 核 `00_INDEX.md:23` 「infrastructure 8 张卡」 | — | **未推翻**：盘上 8 张，枚举 8 个 |
| R6 | 反查 B3：全仓 grep `p2_upm_normalized_weights` 是否真的零消费者 | 若零，则 detail 无误、是我误报 | **推翻成功**：命中 `实验/…/upm_sweep.cpp:236` 活调用 + `eng/tools/quality/v19r3_traceability.py:62` + `docs/engineering/PUBLIC_API.md:1689`。**B3 成立** |
| R7 | 反查 `17_aio.md:70` 「`aio_sparse_punch.h` 是打洞唯一实现」 | 若有第二实现则该条错 | **未推翻**：`git ls-files` 仅 1 个仓内实现（`lib/infrastructure/aio/src/aio_sparse_punch.h`），两处系统调用命中均在该文件 |
| R8 | 反查 `18_cli.md:49` 退出码集与 `infrastructure/acr.md`、`20_benchmark.md` 等路径 | 若路径不存在则基建卡虚构 | **未推翻**：22 项基础设施路径抽查全 OK（唯一例外 `aio_sparse_punch.h` 在 `src/` 非 `include/`，但该文档未给路径，不算错） |
| R9 | 盲复算 `upm-fit.md:217` 「16 字段」 | 若实为 16，则配置表可信 | **推翻成功**：实为 20，且 5 个键名在代码中零命中。**B2 成立** |
| R10 | 反查 B1：ACSD_DESIGN §3.1 是否含「全程只有 SNR」 | 若含，则 detail 引用准确 | **推翻成功**：`全程只有`/`质量代理` 在该文件均 **0 命中**，§3.1 实写 13 对象且含「严格权重」字样 ⇒ 引用与被引条款相反。**B1 成立** |
| R11 | 反查 M2：把「症状主表」当作合法的 5 列简表而非七项不合格 | 若表可按简表读，则自订门槛只是建议 | **部分推翻**：门槛原文是「每条条目的准入格式是**固定的七项**，缺项即**不算合格条目**」——强断言，20 行皆缺 3 项；即便退一步，§2「10 类」vs 20 行仍自相矛盾。**M2 成立（:24 半独立成立）** |
| R12 | 反查 M5：把 `r_median ≈ 1` 读作「经验读数恰好接近 1」而非恒等式 | 若可作独立证据，则负例有判别力 | **推翻成功**：R 的定义式（同页 :216-217）就是「自身 / 零假设尺度」，白噪声下 R→1 由定义保证，构造上不可能翻红。**M5 成立** |
| R13 | 反查 M8 的 18 处 `+` 是否为合法子句连接 | 若为合法排版，则不构成残留 | **推翻成功**：18 处全部位于**换行续写同一句**的位置（如 :442→:443、:75→:76），且全仓 45 文件同形；合法 `+` 连接的对照样本（`docs/engineering/PUBLIC_API.md:1799`、`:2006`）`+` 居行中。**M8 成立** |
| R14 | 核 `acsd.phase3.wcs.md:66-68` 的「文档面曾记 4/8」冲突登记 | 若为隐瞒则应报 | **未推翻**：属主动登记，口径以在役注册表为准，是本片正范式。**不报** |

---

## 6. 盲复算

> 方法：遮蔽既有判定（不读前三轮审稿包的结论作为前提），只用片清单 + 原文 + 代码 + git 事实独立取证，再与我的最终结论比对。

**盲复算 A（本轮新做的最强一项）——「本片所有计数型声明」逐条独立复算。**
先从 `docs/detail` 机械抽出全部 `[数字] + 量词` 声明 60 余条，再逐条独立取权威源复算：

| 声明 | 口径 | 复算 | 判定 |
|---|---|---|---|
| `00_INDEX.md:22/42` registry 26 张卡 | 登记卡（不含 README） | 25 | **偏松（我的结论）** |
| `00_INDEX.md:23` infrastructure 8 张卡 | 同上 | 8 | 一致 |
| `UNIFIED_MODEL.md:56` canonical 13 个 | 数据对象 | 13 | 一致 |
| `STAR_DETECTION:396-399` 5+1+10 | 算子分类 | 16 | 一致 |
| `STAR_DETECTION:15` 6 导出 / 9 字段 / 6 计数 | 符号/字段 | 与 registry 卡互洽（未逐字段点验） | 一致（弱） |
| `merged_TROUBLESHOOTING:78/81` 6 项 / 3 项 | `add_test` 注册名 | 6 / 3 | 一致 |
| `merged_TROUBLESHOOTING:24` 10 类 | 症状行数 | 20 | **偏松（我的结论）** |
| `upm-fit:217` `P2UpmBuildConfig` 16 字段 | 结构体字段 | 20 | **偏松（我的结论）** |
| `calibration:57` 12 符号 | 头导出 | 5+5+1+1=12 | 一致 |
| `phase1.session`/`phase2.session` 四段/四段 | 段数 | 自洽 | 一致 |
| `PHASE1:52` 8 个节点 | 节点名枚举 | 8 | 一致 |
| `PHASE2:19` 七个 operation | 科学步（不含 2 基础设施步） | 7 | 一致 |

**复算结论：一致 9 / 偏松 3 / 偏严 0。** 三处「偏松」都集中在**计数与键名**这一类——即文档自称的数量与覆盖面大于实物。另有两类我原本可能**偏严**，经复算后主动下调：
- M10（`21_observability.md:58` §0 伪引）：我未逐行比对 §0 全文字面，**下调为 must-fix 并标注「部分采信子代理转述」**，不按 blocking 计。
- S7/M6 类节号错：属可核验的定位偏差，不与 B1 同档。

**与既有结论比对（前三轮只当线索，不作事实）**：我刻意在读完 48 份原文**之后**才检索前三轮包，确认 B1/B2/B3、M1/M2/M8 以及两处「零消费者」议题**未在既有台账中以本片定位出现**（`p2_upm_normalized_weights` 曾被记为「已 RETIRED、零消费者」，与本片 B3 的实测**冲突**——本片证据支持 B3 而非旧台账）。故判为**本片新问题**。

---

## 7. 子代理派发记录

**派发总数：6 个运行 / 3 份不同简报**（简报 A、B 各被 harness 重复触发一次，成对运行；C 未重复。**如实披露**：任务书要求 3–5 个，我实际发出 6 次调用，内容上为 3 个不同方向）。

| 简报 | 方向 | 范围 | 结果 |
|---|---|---|---|
| A | `registry/*.md` 25 份 vs 代码 | 逐符号/路径/键名/常量/节点名核验 | 运行 1 与运行 2 同简报（重复触发）；本轮仅收到其一完整回报 |
| B | `infrastructure/*` + `anchors/*` + `PRODUCT_STORAGE_FORM.md` 12 份 vs 代码 | CLI/配置键/schema/常量/路径/退役治理/README 义务 | 未在截止前回报 |
| C | 顶层 10 份（PHASE1/2/3、STAR_DETECTION_IMPL、UNIFIED_MODEL、common、00_INDEX、LOG_AND_ERROR、merged_TROUBLESHOOTING、README）+ 伪引/假门专项 | 索引双向完整性、类型/字段名、伪引（含**无引号裸从句**）、恒真门双向体检 | 回报 3 条消息（首报 + 升级 + 补遗），**本片全部采纳项均由我独立复验** |

**逐条复核结果**（凡我未亲自取证者一律不计入发现）：

| 子代理条目 | 我的复核 | 处置 |
|---|---|---|
| #1 `UNIFIED_MODEL.md:58` 裸从句伪引 | **亲自复核**：`grep -c '全程只有' docs/ACSD_DESIGN.md`=0、`'质量代理'`=0；读 §3.1 原文（:178-183）确认 13 对象且含「严格权重」 | **采纳，升为 B1 主项** |
| #1 升级：伪引已污染 `unified_object_compatibility_map_v1.json:139` 并被 `DATA_SEMANTICS.md:3255` 反向引为依据 | **亲自复核**：两处原文均已打开确认；另补验 `clause_registry.json:194,2809,2810` 亦带「按全程只有 SNR 口径」 | **采纳，扩写为 B1 的传播链部分**（含权威链倒置） |
| #2 `STAR_DETECTION_IMPL_DESIGN:53` 节号错（§3→§1） | **亲自复核**：`STAR_DETECTION.md` 章节行号表（§1 在 :10、§3 在 :110、「判据非退化要求」在 :59） | **采纳为 M6** |
| #3 `:154` 35.45/σsm 改述 | **亲自复核**：两处 science 原文均作 `35.45 σ_smooth` | **采纳为 M7** |
| #8 `PRODUCT_STORAGE_FORM:45` 判据—证据—门三重不匹配 | **亲自复核**：`compress-01/` 对 `hips.zst`/`pread`/定位表零命中；`CHK-SPARSE-PUNCH` 仅 `:PROBE` 有码 | **采纳为 M4**（与 #9 合并） |
| #9 `CHK-SPARSE-PUNCH` 静态支无实现者 | 同上复核 | **采纳，合并入 M4** |
| #10 `19_runtime.md:116` 已删生产者 | **亲自复核**：`ls eng/tools/docs_machine_consistency.py` 不存在 | **采纳为 M3** |
| #11 `eng/ci/` 悬空 | **亲自复核**：`eng/run/` 实际内容确认目录不存在 | **采纳，合并入 M3** |
| #12 `noise-snr.md` 重言式负例 | **亲自复核**（我读原文时已独立存疑 R12）：R 定义式确为「自身/零假设尺度」 | **采纳为 M5** |
| #15 `LOG_AND_ERROR_SYSTEM.md:6` 台账不存在 | **亲自复核**：`eng/run/ledgers/log_system_ledger.json` 不存在 | **采纳，合并入 M3** |
| #14 三条判绿门经核实为真 | 我复核 R1/R2（ctest 计数）一致；`p1noise_*` 由其取证 | **接受为「已核真」，不再复查** |
| #5 `21_observability.md:58` §0 伪引 | **未逐字比对 §0 全行** | **降级为 M10 并标注「部分采信」** |
| #6 `STAR_DETECTION:107` 动态背景路径 | 代理自标 could-not-confirm；我未独立取证 | **否决，不计入发现**，留待他轮 |
| #7 `docs/science/PHASE2_UPM.md:335` 成对引号伪引 | 证据形态可信（零命中），但**不在本片** | **否决出本片计数**，转交对应片；仅在本件 §4.2 末尾备注 |
| #13 `upm-fit:319-320`「逐位不变」 | 措辞歧义而非确证缺陷 | **否决**，降为 S6 相邻观察 |
| #16 `23_hips_browser:42` 省略号路径 | **亲自复核**确认 | **采纳为 S4** |
| #D 附注（`AGENTS.md §5` 流水号/日期） | 属规范 02/05 范围外 | **否决出本片计数**，`G08-10` 记入 M3 附注 |

**否决/降级合计 6 条**（#5 降级、#6 否决、#7 转交、#13 否决、#D 否决、以及我对 M10 的自我降级），**采纳 11 条**（含 1 条升级扩写）。

**对子代理覆盖率的独立评价**：C 简报自陈只精读 5/48、其余 43 为 grep 采样——**该覆盖率不足以支撑整片结论**。本片 48 份成员文件**全部由我本人读完**；子代理产出仅作为线索被我逐条复验，未用作覆盖依据。这一点是本片与「以子代理采样代替通读」做法的实质区别。

---

## 8. 自证段（可复跑命令）

```bash
cd "/workspace/Astro CS Database"
git -c core.quotepath=false log -1 --format='%H'          # 期望 f9650dd0aed97d7f261e5e6547f4fdb505bc313b
git -c core.quotepath=false status --porcelain            # 期望空（本次未改任何仓内文件）

# 成员清单与行数基线（期望 48 行 / 合计 7358）
sed -n '754,801p' "run/GOVERN-08/审核包-R2/分片清单/片清单-权威版.yaml" \
  | sed 's/^ *- "//; s/"$//' > /tmp/mine.txt
wc -l < /tmp/mine.txt
while IFS= read -r f; do wc -l < "$f"; done < /tmp/mine.txt | paste -sd+ | bc   # 7358

# B1 伪引主项：ACSD_DESIGN 中两个被引短语零命中
git -c core.quotepath=false grep -c '全程只有' -- docs/ACSD_DESIGN.md   # 0
git -c core.quotepath=false grep -c '质量代理'   -- docs/ACSD_DESIGN.md   # 0
sed -n '177,186p' docs/ACSD_DESIGN.md                                    # §3.1 实写 13 对象
git -c core.quotepath=false grep -n "全程只有 SNR" -- docs eng | head     # 仅 detail 两处 + 机器源三处

# B1 传播链与权威链倒置
git -c core.quotepath=false grep -n "全程只有 SNR" -- eng/contracts/data/unified_object_compatibility_map_v1.json
git -c core.quotepath=false grep -n "UNIFIED_MODEL.md:58" -- docs        # DATA_SEMANTICS.md:3255 反向引用

# B2 配置表虚构
sed -n '77,127p' lib/algorithms/coverage/include/astro/phase2/upm.h \
  | grep -cE '^\s+(int|double|const char\*|float|bool)\s+[a-z_][A-Za-z0-9_]*\s*(=|;)'   # 20
sed -n '220,239p' docs/detail/registry/acsd.phase2.upm-fit.md | grep -c '^| `'         # 17（声称 16）
for f in upm_weight_source bkg_model bkg_spline_spacing; do
  echo -n "$f: "; git -c core.quotepath=false grep -c "\b$f\b" -- \
    lib/algorithms/coverage/include/astro/phase2/upm.h lib/algorithms/coverage/src/upm.cpp | paste -sd, ; done  # 0,0

# B3 零消费者为假 + PUBLIC_API 行锚漂移
git -c core.quotepath=false grep -n "p2_upm_normalized_weights" -- 实验/additive-sky-seamless eng/tools | head
sed -n '234,237p' 实验/additive-sky-seamless/code/reverse_verify/smooth_lambda/upm_sweep.cpp
sed -n '1997,2001p' lib/algorithms/coverage/src/upm.cpp      # 定义已删（RETIRED 注记）
sed -n '137,143p' lib/algorithms/coverage/include/astro/phase2/upm.h   # 声明已撤下
sed -n '1689,1694p' docs/engineering/PUBLIC_API.md          # 仍公布 + 行锚 h:139-142 / upm.cpp:1325

# M1 索引计数
ls docs/detail/registry/*.md | grep -v '/README.md' | wc -l                # 25（声称 26）
sed -n '46,48p' docs/detail/00_INDEX.md | grep -o '`acsd\.[a-z0-9.-]*`' | tr -d '`' | sort -u | wc -l   # 25

# M2 排障手册自相矛盾
sed -n '12,20p' docs/detail/merged_TROUBLESHOOTING.md                      # 七项准入格式
sed -n '43,44p' docs/detail/merged_TROUBLESHOOTING.md                      # 实际 5 列表头
sed -n '45,64p' docs/detail/merged_TROUBLESHOOTING.md | grep -c '^|'       # 20 数据行（声称 10 类）

# M3 三处已删生产者/台账
ls eng/tools/docs_machine_consistency.py eng/run/ledgers/log_system_ledger.json 2>&1   # 均 No such file
find eng/run -maxdepth 2

# M4 判据—证据—门三重不匹配
git -c core.quotepath=false grep -rn "hips.zst\|pread\|定位表" -- 实验/engineering-evidence/compress-01/   # 零命中
git -c core.quotepath=false grep -rn "CHK-SPARSE-PUNCH" -- eng lib CMakeLists.txt                          # 仅 :PROBE

# M5 重言式负例
sed -n '216,218p;455,457p' docs/detail/registry/acsd.phase1.noise-snr.md

# M6/M7 伪引与改述
git -c core.quotepath=false grep -n "判据非退化要求\|^## " -- docs/science/STAR_DETECTION.md | head
git -c core.quotepath=false grep -n "35.45" -- docs/science

# M8 行尾游离 +
git -c core.quotepath=false grep -nE '）\+$' -- docs | wc -l        # 45 文件；docs/detail 内 18 处

# M9 本片自订锚合同的自我违反
git -c core.quotepath=false grep -nE '`[A-Za-z0-9_./+-]+\.(h|cpp)\`\s?:[0-9]+' -- docs/detail
sed -n '15,16p;93p' docs/detail/anchors/ANCHOR_CONTRACT.md

# M12 悬空引用面（口径 = 来源×失效目标 的引用对）
git -c core.quotepath=false grep -oh "docs/detail/[A-Za-z0-9_./-]*\.md" -- . \
  | sed 's/[.,)）]*$//' | sort -u > /tmp/cited.txt
while IFS= read -r p; do [ -f "$p" ] || \
  git -c core.quotepath=false grep -lF "$p" -- . ; done < /tmp/cited.txt | wc -l   # 210 对（去 run/ 后）

# R1/R2 反例失败项（手册计数正确）
git -c core.quotepath=false grep -n "add_test" -- lib | grep -E "p1snr_science"     # 6
git -c core.quotepath=false grep -n "add_test" -- lib | grep -E "drizzle_pf_sb"      # 3
```

---

## 9. 与本项目已固化检查项的对应

| 已固化项 | 本片落点 | 结果 |
|---|---|---|
| 恒真门三型（代数恒等/结构对称/往返自证）**双向**体检 | M5（`r_median ≈ 1` 是构造性恒等式，负例不可能翻红）；R12 自行也判 PHASE3 视觉门**恒红误报**边界写得好 | 命中 1 条（双向都查了） |
| 自愈判据（复现动作覆写归档） | 全片未见「读归档、首跑红次跑绿」形态；M4 属「门不存在」而非自愈 | 未命中 |
| 代码改了、归档没重跑 | B3 正是此族（RETIRED 改了 upm.h/upm.cpp，PUBLIC_API.md 与 detail 未同步，且活调用点未清） | **命中** |
| 伪引（含**无引号裸从句**） | **B1（主项，无引号裸从句）**、M6（节号错）、M7（数值改述）、M10（§0 改述） | **命中 4 条** |
| 判据读不到真实对象 | M4（探针测的是裸形态打洞，而本文档自陈归档形态不实施 ⇒ 结构上不可能对归档缺陷翻红） | **命中** |
| 选择性报告 | 未命中（各卡普遍标注「引用不冒认」「零承载」「NOT_VERIFIED」，反而是正范式密集） | 未命中 |
| 文档与代码冲突 | B2（配置表 5 键名不存在）、M9（行锚漂移） | **命中 2 条** |
| 退役治理（自称「零消费者」曾为假） | **B3（复发）**；同时 `star-psf.md:63-68` 提供了正确写法 | **命中** |

**另记本片的三条正面范式（建议作为整改模板）**：
1. `acsd.phase3.wcs.md:66-68`——冲突登记 + 「口径以在役注册表为准」；
2. `acsd.phase1.star-psf.md:63-68`——「零消费者」必须附**可证伪依据**（注册表不声明该边 + 上游合同列作幻边负例），而非空口断言；
3. `PRODUCT_STORAGE_FORM.md:197-198` / `21_observability.md:109-123`——打洞失败「不 fail-closed」的**判据五步可执行验证**（含「`st_blocks` 必须下降，否则须判红而不是静默通过」），是本片最完整的判别力设计。
