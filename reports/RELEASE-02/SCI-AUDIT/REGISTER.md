# RELEASE-02 独立科学审计 — 主发现台账（REGISTER）

- 审计员：独立科学审计 SubAgent（RELEASE-02 / SCI-AUDIT），独立验证者角色
- 日期：2026-09-18
- 工作目录：/workspace/Astro CS Database
- 立场：ASTROCS_DESIGN.md = 需求/最高权威；docs/science/**、docs/algorithms/**、现有代码、RELEASE-01/02 各报告与 change-claim 均为**待证主张**，本台账逐条独立复核，可推翻。
- 复算脚本与原始输出：run/RELEASE-02/p0_recompute.py + run/RELEASE-02/p0_recompute.log；文献核验 run/RELEASE-02/verify_refs*.py + .log。
- 零 git 写；未跑 ninja/cmake/ctest；只运行既有二进制与 Python 复算；TMPDIR=/dev/shm/astrocs_audit。

## 0 证据标准与三分类口径

- 文献：式号 + DOI/ADS bibcode，经 Crossref API（api.crossref.org/works/<DOI>）或出版社页核验真实存在。本轮共核验 30 条 DOI（见 run/RELEASE-02/verify_refs.log、verify_refs2.log、verify_refs3.log）。
- 开源：仓库 + 版本/commit + 文件:行 + 许可证；GPL 只读对照，未复制进仓库。
- 复算：脚本 + 原始输出；本报告所有常数均给复算值。
- 三分类：
  1. DESIGN-OK-DOC-WRONG：最高设计意图成立，是文档/推导/契约写错（含代码落后于正确设计）→ 不阻塞设计；
  2. DESIGN-OK-EVIDENCE-MISSING：设计意图合理但缺证据（缺文献/开源对照/常数无出处/证据不可复跑）→ 不阻塞，补证据；
  3. BLOCKS-DESIGN：与最高设计**冲突**（设计自相矛盾 / 科学上不可实现 / 设计要求的量与物理量不符）→ 需上呈负责人。

---

## 1 P0：RELEASE-01 登记的 11 个 UNRESOLVED 主题

### U-A frame_snr 规范语义（三方冲突）

| 编号 | 文档:行 | 主张原文（节选） | 独立证据 | 判定 | 三分类 | 建议处置 |
|---|---|---|---|---|---|---|
| R2-A-01 | ASTROCS_DESIGN.md:86,155-158,235 | 帧级 SNR 是**唯一帧级参考**、是**信噪比不是权重**、写入 HiPS 头、w=1/σ²=SNR²/F_ref²∝SNR² | Horne 1986 PASP 98,609（DOI 10.1086/131801，Crossref OK）给 σ_F⁻²=ΣP²/σ²；复算 E7：w=1/σ_F² 与 SNR²/F_ref² 逐位相等（F_ref 组内常数）；E8：功率比 PSFSNR 不能做该换算 | **CORRECT** | 设计本身成立 | 保持；作为唯一 canonical |
| R2-A-02 | docs/science/CONTROL_WEIGHT_SNR.md:10-14,32-33,120 | frame_snr/local_snr 实为**相对质量权重场**，**不是科学信噪比**；禁止解释为科学 SNR | 与 ASTROCS_DESIGN.md:86,155-158 和 docs/design/UNIFIED_MODEL.md:42 直接互斥；同文件 §4 自述是 weight_mode=0 legacy 分支 | **CONTRADICTS-DESIGN** | DESIGN-OK-DOC-WRONG | 该文件把 frame_snr 名字让给设计语义；质量权重改名 quality_weight（其 §2a 已用该名）；走 ENGINEERING_SPEC §3 变更 claim |
| R2-A-03 | lib/infrastructure/scheduler/src/module_adapters.cpp:3177-3183；lib/algorithms/noise_snr/wrapper_phase1/snr_frame_science.h:23-24 | 实装 frame["frame_snr"] = {flux5_adu,m5_mag,zero_point_mag} 5σ 深度对象，注释"**不产出整帧 SNR 标量**" | 直接读代码；contracts/schemas/unified/frame_snr.schema.json 要求 frame_snr_value（number）且 unified_object=frame_snr → 实装产物与契约 schema 不兼容 | **WRONG**（实现落后于设计） | DESIGN-OK-DOC-WRONG（代码缺口） | 实现按设计输出帧级 SNR（F_ref/σ_F，写 HiPS 头）；5σ 深度改键 depth_m5（该对象已有独立 schema） |
| R2-A-04 | contracts/schemas/unified/frame_snr.schema.json（description；frame_snr_value.description） | "真实信号/噪声比，**不受天光影响**"；frame_snr_value "无量纲；**不受天光影响**" | 复算 E6：天空受限时 σ_sky 翻倍 ⇒ SNR_F 精确减半（1→0.5→…→1/40）；07_noise_snr.md:41 与 UNIFIED_MODEL.md:42 均已改为"天光散粒噪声计入 σ_n、天光变亮降低 SNR" | **WRONG**（措辞） | DESIGN-OK-DOC-WRONG | schema 描述改为"对加性背景电平/梯度免疫；天光散粒噪声增大时如实下降" |
| R2-A-05 | ASTROCS_DESIGN.md:235 | "其信号/噪声估计方法学**对标 PixInsight 公开文档中的 PSFSNR（ratio-of-powers 信噪比）**" 与同句 F_ref/σ_F 通量型口径并列 | PixInsight 官方 .pidoc 式[18] 为 c3(Σf)²/(c4σ_n²)（功率比，本审计核验，见 R2-H-01）；功率比型不能与 w=SNR²/F_ref² 同时成立（E8）。但设计同时明确"数学定义采用通量型口径"，操作要求自洽 | **AMBIGUOUS** | DESIGN-OK-EVIDENCE-MISSING | 设计措辞把"方法学对标"与"公式对标"分开写清；或注明"仅方法学（恒星测光/稳健噪声/独立背景）对标，不采用功率比式" |

**U-A 结论**：设计意图（帧级通量型 SNR、写 HiPS 头、Phase2 现场换算逆方差）**科学成立、可复算、内部自洽**；冲突来自 CONTROL_WEIGHT_SNR.md 的旧重定义（R2-A-02）、实现落后（R2-A-03）与 schema 措辞（R2-A-04）。**不阻塞设计。**

### U-B 校准方差传播

| 编号 | 文档:行 | 主张原文（节选） | 独立证据 | 判定 | 三分类 | 建议处置 |
|---|---|---|---|---|---|---|
| R2-B-01 | docs/science/CALIBRATION.md:143-144（§9）、§9a | "**不传播母版方差**至 cal 的方差项（ivar 由 snr_estimator 独立估计）" | 与 ASTROCS_DESIGN.md:197（§3.6 明列"**校准方差传播**、共享 master 相关性"为硬约束）与 docs/design/PHASE1_DETAILED_DESIGN.md:52-58（"目标态不得再用'校准层不传播、后面重新猜一个噪声'替代物理传播"）互斥 | **CONTRADICTS-DESIGN** | DESIGN-OK-DOC-WRONG | 订正 CALIBRATION.md §9/§9a：本层传播 master 方差/相关性；ivar 由噪声模型在 cal 方差之上补充而非替代 |
| R2-B-02 | docs/plugins/algorithms_phase1/01_calibration.md:31；docs/design/PHASE1_DETAILED_DESIGN.md:55-58 | V(y)={V(r)+V(b)+α²[V(d)+V(b)]+y²V(f)}/f² | 独立代数：y=[r−(1−α)b−αd]/f ⇒ 正确一般式 V(y)=[V(r)+(1−α)²V(b)+α²V(d)+y²V(f)]/f²；原式对同一 bias master 把 V(b) 计成 (1+α²)V(b)，系数错。V6 实现 lib/algorithms/calibration/src/v6_calibration_covariance.cpp:313-319 同 master 时折叠 j_b=−(1−α)/f（正确），并有负向注入 double_bias_master（:378-384）复现错误式 | **WRONG** | DESIGN-OK-DOC-WRONG | 一般式改为 (1−α)²V(b)；不同 master_id 分支单列；文档与 PHASE1_DETAILED_DESIGN 同步 |
| R2-B-03 | lib/algorithms/calibration/src/v6_calibration_covariance.cpp；lib/algorithms/calibration/V6_CALIBRATION_COVARIANCE.md:5-8 | V6 协方差实现正确但文档自述"**未接线 Phase session**" | 代码在 lib/algorithms/calibration/ 存在且自洽；生产 Phase1 session 是否消费未证（lib/phase1_session/p1_session.cpp 路径未见接线） | **INSUFFICIENT-EVIDENCE**（实现接线面） | DESIGN-OK-DOC-WRONG（代码缺口） | 前台构建后确认生产 DAG 是否消费该协方差；未接线则属实现缺口（非设计阻塞） |
| R2-B-04 | 行业对照 | 设计要求的母版方差传播严于行业（ccdproc / LSST ip_isr 均不传播母版方差） | 该"行业不传播"为 RELEASE-01 SCI-S2 的 [S] 级结论，本轮未逐行核验其源码 | **INSUFFICIENT-EVIDENCE** | DESIGN-OK-EVIDENCE-MISSING | 若以"行业也不做"为由降级设计，需补 ccdproc/ip_isr 文件:行证据；否则按设计传播 |

**U-B 结论**：设计明确要求校准方差物理传播（§3.6 硬约束），**可实现**；CALIBRATION.md 与公式文档写错。**不阻塞设计。**

### U-C 天光面语义（加性 vs g·s+b）

| 编号 | 文档:行 | 主张原文（节选） | 独立证据 | 判定 | 三分类 | 建议处置 |
|---|---|---|---|---|---|---|
| R2-C-01 | ASTROCS_DESIGN.md:213（§4.2 流程图）、:246（§4.4） | UPM 联合相对模型 **g·s+b**；天光亮度平面 b_k(x) 用稀疏表示：每帧掩膜外稀疏采样点（带 SNR 权重），**全部帧联合构建参考天光面**，稀疏样条/插值校准梯度，栅格按需现场求值，SNR 加权最小 RMS | 与插件 docs/plugins/algorithms_phase2/10_sampling.md:32-56（WCS 映射到天球、w∝SNR²）、11_upm.md:26-56（y_k=g_k·s(x)+b_k(x)、WCS 联合拟合 B_ref、C_out=R C_in Rᵀ）逐条一致；SCAMP（GPL-3.0）乘性相对光度 + SWarp 加性背景为标准做法 | **CORRECT**（设计） | 设计本身成立 | 保持 |
| R2-C-02 | docs/science/PHASE2_UPM.md:7-8,81,153,157,182 | FROZEN：calibrated_f(p)=raw_f(p)−C_f(p)，8×8 control cell，**纯加性**；"不处理乘性尺度差（已撤销）"；§3a "无 WCS/天球参与" | 与 R2-C-01 设计互斥（设计要 g·s+b 且联合天球参考天光面）；该文件 §14a:182 自述登记为 UNRESOLVED。**独立复核（P1-D-012 + 本审计员）**：设计 g·s+b 并非未实现——p2_upm_ma_build 存在（upm.cpp:1898；upm.h:305 out_g/out_b/out_s），被 stage2.cpp:516 与 phase2_integrate.cpp:1627 调用；stage2_common.h:52-57 注「生产 config 模板显式 frame_gain=true」，缺省 false 以保 RELEASE-01 加性基线。落后的是：(a) FROZEN 科学文档仍写纯加性；(b) scheduler 的 fit_upm 适配器仍走加性 p2_upm_build_geo（module_adapters.cpp:4155） | **CONTRADICTS-DESIGN** | DESIGN-OK-DOC-WRONG | 走变更 claim 重写 PHASE2_UPM.md 为目标模型，或明确标注"现行实现口径，已被设计 §4.2/§4.4 取代"；FROZEN 状态需一并处理。<br>订正（2026-09-20）：已裁决（§9.38 A2，工程控制/RELEASE-02/GAP_AUDIT.md:1036-1043 + §9.40 C3:1122-1127）：UPM = 纯加性，g_k ≡ 1 本期不启用 ⇒ 「重写为目标模型 g·s+b」的处置作废；PHASE2_UPM.md:182 的 UNRESOLVED 关闭。旧文 =「走变更 claim 重写 PHASE2_UPM.md 为目标模型…」。加性组合的施加量另见 §9.67 定案 1:2548-2555（raw − δ_k、保留 B_ref）。 |
| R2-C-03 | docs/design/UNIFIED_MODEL.md:46-48 | 数据对象含 star_mask（天球坐标）、sky_samples（每帧掩膜外稀疏采样点，点权重 ∝SNR²）、sky_plane（稀疏样条参考面 B_ref+逐帧梯度 δ_k） | 与设计 §4.4 一致；实装 grep -rIl sky_plane lib/ contracts/ config/ 命中数待前台复核（GAP_AUDIT 记为 0） | **CORRECT**（文档）/ 实现缺口 | 设计本身成立 | 实现缺口登记，非设计阻塞 |

**U-C 结论**：设计目标模型（乘性相对定标 + 联合稀疏天光面）**物理可实现、与标准流程一致**；FROZEN 科学文档停留在纯加性/无 WCS 的旧实现口径。**不阻塞设计，但需负责人裁决"目标规范 vs 现行实现"的过渡（设计优先级已由负责人裁定为最高，故按设计订正文档）。**

> 订正（2026-09-20）：已裁决 —— UPM = 纯加性（§9.38 A2，工程控制/RELEASE-02/GAP_AUDIT.md:1036-1043；§9.40 C3:1122-1127），g_k ≡ 1 本期不启用；PHASE2_UPM.md:182 UNRESOLVED 关闭 ⇒「目标规范 vs 现行实现」的过渡不再需要负责人裁决（乘性目标模型已被否决）。旧文 =「需负责人裁决…的过渡」。**

### U-D weight_mode 词表

| 编号 | 文档:行 | 主张原文（节选） | 独立证据 | 判定 | 三分类 | 建议处置 |
|---|---|---|---|---|---|---|
| R2-D-01 | docs/contracts/PUBLIC_API.md:2110-2127；contracts/schemas/v6/astrocs.v6.weight-mode.v1.schema.json | 生产枚举 = 字符串 {point_information,surface_gls,psfsw_robust}；baseline {equal,pixel_ivar}；**legacy 整数被取代，0 一律拒绝**；生产 writer 只写字符串模式 | 实装 P2Stage2Config.weight_mode 为整数 0/1/2（lib/algorithms/coverage/include/astro/phase2/stage2_common.h:90；aio_hips.h:222,236 ASTROCS_WEIGHT_MODE 0/1/2）；lib/infrastructure/cli/session_commands.h:133 默认 {"weight_mode","2"}；lib/infrastructure/cli/v6_runtime_contract.h:149-171 把 legacy 2 路由为 **baseline pixel_ivar（非生产）** | **CONTRADICTS**（契约↔实现词表/语义） | DESIGN-OK-DOC-WRONG | 退役 legacy 整数字段或把默认改为生产字符串模式；确保生产默认不落到 baseline |
| R2-D-02 | ASTROCS_DESIGN.md:234；config/defaults.json:495-505 | 默认 point_information 逆方差叠加 | weight.default_mode=psf_information_weight，enum_token=point_information，与设计一致 | **CORRECT** | 设计本身成立 | 保持 |

**U-D 结论**：设计只要求"逆方差权重、SNR 不直接加权"，对配置表示不设硬性词表；冲突是契约/实现两套词表。**不阻塞设计。**

### U-E export 配置格式

| 编号 | 文档:行 | 主张原文（节选） | 独立证据 | 判定 | 三分类 | 建议处置 |
|---|---|---|---|---|---|---|
| R2-E-01 | ASTROCS_DESIGN.md:326（§6.3） | "模板命令输出**可直接运行的完整 JSON（填好默认值）**" | 实跑 build/astrocs export --template → {schema_version:"1", source:{hips_dir:""}, output_dir:".", center:{...}, width_px:1024, height_px:1024, scale_deg_per_px:0.001}；回喂 export --json <template> -y → **rc=2** "source.hips_dir 为空：没有输入产品"。模板未"填好默认值"，不可直接运行 | **WRONG**（实现违反设计） | DESIGN-OK-DOC-WRONG（CLI 缺口） | 模板填必填项或改为 schema 形态；把"必填留空"与"可直接运行"二选一写清 |
| R2-E-02 | contracts/schemas/phase_config_export.schema.json（required phase_name/config/inputs；config.wcs） | 正式 schema：{phase_name:"export", config:{output_dir,precision,output_mode,wcs:{projection,center_deg,s_out_deg,width_px,height_px}}, inputs:[{product}]} | 实跑 schema 合规 JSON → **rc=2** "output_dir 必须是非空字符串（收到 缺失）"、"source 缺失"；export --help 字段为 source/output_dir/center/width_px/height_px/scale_deg_per_px/projection/sampler/longitude_parity/coverage_output。**两套格式互斥，schema 不是 CLI 接受的格式** | **CONTRADICTS**（契约↔CLI） | DESIGN-OK-DOC-WRONG | 二者取一为唯一合同（建议以设计 §3.3 config+inputs 族为准），另一方走变更 claim |

**U-E 结论**：设计对配置形态有明确取向（config+inputs 数据块、模板可直接运行），实装另立一套。**不阻塞设计。**

### U-F 版本口径

| 编号 | 文档:行 | 主张原文（节选） | 独立证据 | 判定 | 三分类 | 建议处置 |
|---|---|---|---|---|---|---|
| R2-F-01 | ASTROCS_DESIGN.md:532-533（§12） | "Alpha 之前：程序与代码中**不包含任何版本信息**"；发布时 --version=0.0.1alpha | ./build/astrocs --version → astrocs 0.11.0-alpha.2+g958fa5b…；VERSION=0.11.0-alpha.2；ci/checks.json:6457、ci/check_version.py:566、build/astrocs.product.json:3 均硬绑 0.11.0-alpha.2 | **CONTRADICTS-DESIGN** | DESIGN-OK-DOC-WRONG（负责人已裁定本阶段不碰版本号） | 本阶段不动；FIN 阶段按 §12 清零版本信息 |
| R2-F-02 | 工程控制/RELEASE-01/GAP_AUDIT.md:68 | U-F 记"最高设计 §12 = 0.1alpha" | 现行 ASTROCS_DESIGN.md:533 写 0.0.1alpha；GAP_AUDIT 文本已过期 | **WRONG**（审计文本） | DESIGN-OK-DOC-WRONG | 更新 GAP_AUDIT 引用 |

### U-G 文档包 §7/§2（治理）

| 编号 | 文档:行 | 主张原文（节选） | 独立证据 | 判定 | 三分类 | 建议处置 |
|---|---|---|---|---|---|---|
| R2-G-01 | ENGINEERING_SPEC.md:90 | 根固定条目应含 ACCEPTANCE_SPEC.md | 现文本已含 CONTROL_PACK_SPEC.md / ACCEPTANCE_SPEC.md / memory.md / DEPENDENCIES.md → **已闭合** | **CORRECT** | — | 关闭 U-G 第 1 项 |
| R2-G-02 | docs/ci/01_CHECKS.md:42,61 | 应登记 CHK-E2E-REPRO 与 CHK-EXIT-CONSISTENCY | 现文本已登记两项（P0）→ **已闭合** | **CORRECT** | — | 关闭 U-G 第 2 项 |

### U-I runtime/scheduler 命名

| 编号 | 文档:行 | 主张原文（节选） | 独立证据 | 判定 | 三分类 | 建议处置 |
|---|---|---|---|---|---|---|
| R2-I-01 | ASTROCS_DESIGN.md:379 vs :397 | §7.1 目录树写 scheduler/；§7.2 架构图写 infrastructure/scheduler+runtime | 代码目录为 lib/infrastructure/scheduler/；同一设计内两口径 | **AMBIGUOUS**（设计自身笔误） | DESIGN-OK-DOC-WRONG | 统一为 scheduler；属命名治理，不涉科学 |

### U-J 其他登记面

| 编号 | 文档:行 | 主张原文（节选） | 独立证据 | 判定 | 三分类 | 建议处置 |
|---|---|---|---|---|---|---|
| R2-J-01 | ASTROCS_DESIGN.md:328 | 退出码"唯一源 include/astrocs/exit_codes.h" | find 无 include/astrocs/exit_codes.h；实际 lib/infrastructure/cli/exit_codes.h | **WRONG**（路径） | DESIGN-OK-DOC-WRONG | 改路径 |
| R2-J-02 | ASTROCS_DESIGN.md:321,445,447 | 可执行名 ACSD Cli / Linux acsd_cli | 实构建产物为 build/astrocs；全仓非归档无 acsd_cli/ACSD Cli 字样 | **WRONG**（命名） | DESIGN-OK-DOC-WRONG | 设计/实现统一命名（发布前由负责人定名） |
| R2-J-03 | 工程控制/RELEASE-01/GAP_AUDIT.md:126-157 | 21 个 contracts/schemas/<名>.schema.json 悬空（42 引用点） | 本轮未逐一复扫（引用清单已由前台给出）；属治理面 | **INSUFFICIENT-EVIDENCE**（本轮未复核计数） | DESIGN-OK-EVIDENCE-MISSING | 补契约或改引用；前台统一裁决归属 |

### U-H 科学表述订正

| 编号 | 文档:行 | 主张原文（节选） | 独立证据 | 判定 | 三分类 | 建议处置 |
|---|---|---|---|---|---|---|
| R2-H-01 | docs/plugins/algorithms_phase1/07_noise_snr.md:47；docs/research/SNR_WEIGHT_RESEARCH_PACK.md:21；docs/science/PSF_SIGNAL_WEIGHT.md:43 | PSFSNR 式[18] c3·(Σ_j f_j)²/(c4·σ_n²)，分子是**和的平方** | PixInsight 官方 .pidoc docs/ImageWeighting/02-PSF_Flux_Weighting_Algorithms.pidoc（GitLab pixinsight/Reference-Documentation master，commit 08b8eb85ae17611629d21ff52014842077b01a1b，2024-06-21）：\equation 逐字 SNR_PSF = c3(Σ_{j=1}^n f_j)²/(c4 σ_n²)；PSFFluxPower=Σf_j² 在 03-Implementation.pidoc:207 明标 "Currently not used, reserved" | **CORRECT**（已订正） | — | 保持 |
| R2-H-02 | 同 07:47/53、PSF_SIGNAL_WEIGHT.md:41-45 | 式号 [16] PSFSW / [17] c1,c2 / [18] PSFSNR / [19] c3,c4 / [20] SNR | 独立计数：01-Introduction.pidoc 有 3 式，02-...pidoc 有 17 式；故 02 第 13 式=式[16]（PSFSW）、15=式[18]（PSFSNR）、17=式[20]（SNR）→ 与文档逐一对上。（RELEASE-01 报告称"共 22 式"，实为 21 式；式号锚不受影响） | **CORRECT** | — | 可顺带修正"22 式"为 21 |
| R2-H-03 | 07:53；PSF_SIGNAL_WEIGHT.md:45；NOISE_MODEL.md:202-203；SCIENTIFIC_REFERENCES.md:162 | 文章版 c1=8.0832e-6,c2=9.0e6,c3=1.350e-7,c4=4.987e6；PCL "2.10.4" c1=5.326e-6,c3=1.316e-7 | .pidoc 式[17]/[19] 逐字核验文章版常数；PCL master（header 自述 **PCL 2.10.8**，Released 2026-09-16）PSFSignalEstimator.h:265,286 逐字 5.326e-6/9.0e+6、1.316e-7/4.987e+6 | **CORRECT**（常数）/ 版本标签不精确 | DESIGN-OK-DOC-WRONG（版本标签） | 引用常数带真实版本（PCL master 2.10.8 / 抓取 commit），不要写死 "2.10.4/Released 2026-06-21" |
| R2-H-04 | PSF_SIGNAL_WEIGHT.md:42,47；lib/algorithms/photometry/cpp/src/psfsw.cpp:313-315；include/astrocs/v6/psfsw.h:57-61 | 官方式[16] w=c1(Σf)(Σf̄)/(c2σ_nM*)；AstroCS 自有 Wt=C_norm·S^α·Conc^β/(N^γ·B^δ)，α=2,β=1,γ=2,δ=1，N=1.482602·MAD({fhat})（星间散度，非 σ_n），B=b̄·A_ref | .pidoc 式[16] 逐字核验；代码逐行核验：N=robust_scale_mad(valid)（psfsw.cpp:236）、conc=(s/n)/a_nea、b=b̄·a_ref、复合式 :313-315、kComposite* 常数（psfsw.h:57-61） | **CORRECT**（披露准确，"受启发非等价"表述正确） | — | 保持；但指数/阈值仍 PENDING_OWNER_SIGNOFF（见 R2-H-05） |
| R2-H-05 | psfsw.h:40-61；docs/algorithms/v6/phase2-psfsw/PSFSW_ALGORITHM_SPEC.md §13 | α=2,γ=2、N=星间散度、阈值组均无本项目标定记录 | 代码自标 PENDING_OWNER_SIGNOFF；无 L1 合成标定脚本/冻结记录；官方式[16] 为各一次方 + 图像噪声 | **INSUFFICIENT-EVIDENCE**（本项目标定缺失） | DESIGN-OK-EVIDENCE-MISSING | 用 L1 合成数据对标式(16)与等权/ivar 基线，出标定报告后冻结或改指数 |
| R2-H-06 | 07:41,114 | 帧级 SNR "对加性背景电平/梯度免疫；天光散粒噪声增大时如实下降" | 复算 E6 逐位证实；旧措辞"不随天光漂移"已订正 | **CORRECT**（已订正） | — | 保持；同步修 schema 措辞（R2-A-04） |
| R2-H-07 | NOISE_MODEL.md:202-203；SCIENTIFIC_REFERENCES.md:162 | N*_MAD=2.48308·MAD、N*_Sn=2.03636·S_n；PCL NStar() 取 2.03636 而注释写 2.05435 | .pidoc 式[14]/[15] 逐字；PCL master PSFSignalEstimator.h:229 2.03636*pcl::Sn，:60-62 注释写 "normalization constant 2.05435" → **PI 自身注释/实现冲突**（外源问题） | **CORRECT**（文档引用无误） | — | 保留 PI 内部冲突注记；不得用 2.05435 替换 2.03636 而不注明 |
| R2-H-08 | SCI-S1 U4：PI Moffat 式[5]（分母 2σ²）与式[10]（r_x=σ√(10^{1/β}−1)）差 √2 | .pidoc 式[5] 分母 2σ_x²；式[9] 高斯 r_x=k·2.1460·σ_x（与分母 2σ² 自洽）；式[10] Moffat r_x=k·σ_x√(10^{1/β}−1)（缺 √2，若分母为 2σ² 应为 σ_x√(2(10^{1/β}−1))）→ PI 文档内部 √2 不一致确认 | **AMBIGUOUS**（外源） | DESIGN-OK-EVIDENCE-MISSING | AstroCS 自身 Moffat（Q=0.5r²/σ²、FWHM=2√2σ√(2^{1/β}−1)）内部自洽；跨软件比 FWHM 时注明 PI 口径 |
| R2-H-09 | docs/science/PSF.md:23,81,88,119,128；docs/algorithms/STAR_PSF_ALGORITHMS.md:56-57,117；noise_model.cpp:37、snr_science.cpp:39、dpsf_psf.cpp | robust_residual_sigma=residual_scale/0.7316727929211932 | 复算 E2：central-80% 半正态均值精确 0.73167309528061342（closed-form 与 scipy.quad 一致到 1e-13；MC 相容）；文档/代码值相对差 **+4.13e-7**。且 STAR_PSF_ALGORITHMS.md:57 同处写 "E[trimmed mean |r|]=0.731673σ" 与所用常数不一致 | **WRONG**（微小但被当精确常数） | DESIGN-OK-DOC-WRONG | 统一改全精度 0.7316730952806139（含实现常数与 tools/docs_machine_consistency.py），需代码+文档+检查器协同 |
| R2-H-10 | PSF.md:20,51；STAR_PSF_ALGORITHMS.md:54；snr_science.cpp:37 | Moffat4 FWHM=1.230310·σ | 复算 E3：精确 2√2·√(2^{1/4}−1)=1.2303076525901024，相对差 **1.91e-6**（6 位舍入）；同文 :54 给出精确表达式 | **AMBIGUOUS**（舍入当不变量） | DESIGN-OK-DOC-WRONG | 标 "≈" 或改全精度 |
| R2-H-11 | STAR_DETECTION_ALGORITHMS.md:52-53,102 | s_factor=√(−2ln0.001)=3.7172 | 复算 E4：√(2ln1000)=3.7169221888，相对差 7.47e-5（舍入） | **AMBIGUOUS** | DESIGN-OK-DOC-WRONG | 同上 |
| R2-H-12 | NOISE_MODEL.md:53,124；noise_model.cpp:robust_sigma | σ_bg=1.482602218505602·MAD | 复算 E1：1/Φ⁻¹(3/4)=1.482602218505602 **逐位相等**；MC 4e6 高斯样本复核 | **CORRECT** | — | 保持 |
| R2-H-13 | SCIENTIFIC_REFERENCES.md:170 | Rousseeuw & Croux 1993, JASA 88, 1273，DOI 10.1080/01621459.1993.10476408 | Crossref：DOI → "Alternatives to the Median Absolute Deviation", JASA 88, 1273-1283 ✅ | **CORRECT** | — | 保持 |
| R2-H-14 | SCIENTIFIC_REFERENCES.md；PSF_SIGNAL_WEIGHT.md:112 等 | Naylor 1998, MNRAS 296, 339 | Crossref：10.1046/j.1365-8711.1998.01314.x → Naylor, "An optimal extraction algorithm for imaging photometry", MNRAS 296, 339-346 ✅ | **CORRECT** | — | 补 DOI |
| R2-H-15 | NOISE_MODEL.md:139；SNR_WEIGHT_RESEARCH_PACK.md:76 | Starck & Murtagh 1998, PASP 110, 193 | Crossref：10.1086/316124 → "Automatic Noise Estimation from the Multiresolution Support", PASP 110, 193-199 ✅ | **CORRECT** | — | 补 DOI |
| R2-H-16 | PHOTOMETRY.md:70；REJECTION.md:120 | Beaton & Tukey 1974, Technometrics 16, 147 | Crossref：10.1080/00401706.1974.10489171 → "The Fitting of Power Series…", Technometrics 16, 147-185 ✅ | **CORRECT** | — | 补 DOI/完整标题 |
| R2-H-17 | V6 头:13-15；SCI-S2 §4 | 量化噪声 q²/12：Bennett 1948, BSTJ 27, 446 | Crossref：10.1002/j.1538-7305.1948.tb01340.x → "Spectra of Quantized Signals", BSTJ 27, 446-472 ✅ | **CORRECT** | — | 补 DOI |
| R2-H-18 | DRIZZLE.md；SCI-S2 T6 | Sutherland & Hodgman 1974, CACM 17, 32 | Crossref：10.1145/360767.360802 → "Reentrant polygon clipping", CACM 17, 32-42 ✅ | **CORRECT** | — | 补 DOI |
| R2-H-19 | PHASE2_UPM.md:176 | Padmanabhan et al. 2008, ApJ 674, 1217 | Crossref：10.1086/524677 → "An Improved Photometric Calibration of the SDSS Imaging Data", ApJ 674, 1217-1233 ✅ | **CORRECT** | — | 补 DOI |
| R2-H-20 | CONTROL_WEIGHT_SNR.md:136；SCI-S2 T5 | Huang, S. et al. 2017, ApJ 838, 110（HSC 深度） | Crossref 无法把 ApJ 838, 110 对上 HSC 深度论文；10.3847/1538-4357/aa6574 实为 ApJ **838, 162**（"Absolute Ages and Distances of 22 GCs"，O'Malley et al.） | **INSUFFICIENT-EVIDENCE**（疑似错引） | DESIGN-OK-DOC-WRONG | 核 HSC 深度正确出处（疑为 Huang et al. 2018, PASJ 70, S6 或 Hildebrandt et al. 2017）；无把握则删 |
| R2-H-21 | PSF.md:42；SCIENTIFIC_REFERENCES.md:171 | Moffat 1969, A&A 3, 455 | ADS 页面被反爬（HTTP 405），本审计**未能逐页核验**；bibcode 1969A&A.....3..455M 为通行引用 | **INSUFFICIENT-EVIDENCE** | DESIGN-OK-EVIDENCE-MISSING | 以 ADS/A&A 印本补 page-level 证据 |
| R2-H-22 | CONTROL_WEIGHT_SNR.md:136 | Tonry 2012 ApJ 750,99；Ivezić 2019 ApJ 873,111 | Crossref：10.1088/0004-637X/750/2/99 → Tonry "THE Pan-STARRS1 PHOTOMETRIC SYSTEM" ✅；10.3847/1538-4357/ab042c → Ivezić "LSST: From Science Drivers…" ✅ | **CORRECT** | — | 保持 |
| R2-H-23 | REJECTION.md:120,166 | Maples et al. 2018, ApJS 238, 2, DOI 10.3847/1538-4365/aad23d | Crossref ✅ "Robust Chauvenet Outlier Rejection" | **CORRECT** | — | 保持 |
| R2-H-24 | REJECTION.md:119；PHASE2_REJECTION.md | Rosner 1983, Technometrics 25, 165（Generalized ESD） | Crossref：10.1080/00401706.1983.10487848 → "Percentage Points for a Generalized ESD Many-Outlier Procedure" ✅ | **CORRECT** | — | 保持 |
| R2-H-25 | DRIZZLE.md；SCI-S2 T6 | Van Oosterom & Strackee 1983, IEEE TBME 30, 125 | Crossref：10.1109/TBME.1983.325207 → "The Solid Angle of a Plane Triangle" ✅ | **CORRECT** | — | 保持 |
| R2-H-26 | PHASE2_UPM.md:177；PHASE2_UPM.md §14a | Gruen et al. 2014, PASP 126, 158；Holland & Welsch 1977；Huber 1964 | Crossref：10.1086/675080（SWarp clipping）✅；10.1080/03610927708827533（IRLS）✅；10.1214/aoms/1177703732（Huber）✅ | **CORRECT** | — | 保持 |
| R2-H-27 | PHASE3_HIPS_TO_FITS.md:150 | Fernique et al. 2015 HiPS, A&A 578, A114 | Crossref：10.1051/0004-6361/201526075 → "Hierarchical progressive surveys" ✅ | **CORRECT** | — | 保持 |
| R2-H-28 | PHASE2_UPM.md:22 | k_corr 冻结默认 1.4，"标定表 1.2112–**2.8971** 全在该域内" | sampler.cpp:96-99 实际表 {{1.2112,1.3925,1.4980},{2.3958,2.8971,**3.2035**}}，**最大值 3.2035**；文档区间漏 3.2035 | **WRONG** | DESIGN-OK-DOC-WRONG | 区间改 1.2112–3.2035 |
| R2-H-29 | PHASE2_UPM.md:61-62,169；sampler.cpp:81-83 | k_corr=1.4 由 pixfrac=0.8 的 2000 次 MC（1.3883）冻结为"保守" | 表值随 pixfrac 升（300″: 0.5→1.2112、0.8→1.3925、1.0→1.4980）；生产默认 pixfrac 已改 1.0（config/defaults.json:541-549，DOC-SCI-001 §3 裁决），故在标定域内 1.0 处 **1.4 并不保守（<1.4980）**；仅因生产真帧角尺度 0.9586″/px 落在 [300,600]″ 域外、代码一律回退 1.4 才一致。且 MC 证据源 control_median_mc_test **未注册（构建孤儿）不可复跑** | **INSUFFICIENT-EVIDENCE**（且保守性表述可疑） | DESIGN-OK-EVIDENCE-MISSING | 重跑/注册 k_corr MC，按当前 pixfrac=1.0 与真实角尺度重新标定；文档删"保守"或给证据 |

### U-K 采样核词表

| 编号 | 文档:行 | 主张原文（节选） | 独立证据 | 判定 | 三分类 | 建议处置 |
|---|---|---|---|---|---|---|
| R2-K-01 | docs/plugins/algorithms_phase3/15_resample.md:25,35；docs/contracts/DATA_SEMANTICS.md:2198；docs/api/PHASE3_API_V1.md:25；build/astrocs export --help | 采样核词表 nearest|bilinear，缺省 bilinear | docs/algorithms/v6/phase3/ALG-P3-001_KERNEL_REGISTRY.md §1 注册表为 nearest / bilinear_4quad / bilinear_area_overlap_exact / bicubic / lanczos，且**生产默认 = bilinear_area_overlap_exact**（bilinear_4quad 明确"否"） | **CONTRADICTS**（两套词表 + 生产默认不一致） | DESIGN-OK-DOC-WRONG | 统一 kernel_id；bilinear 需明确等于哪个注册核；生产默认唯一 |
| R2-K-02 | ALG-P3-001_KERNEL_REGISTRY.md §3.2-3.4 | bilinear_4quad 误差界 (h²/8)(max|Fxx|+max|Fyy|)，Oracle max_interp_err=0.0273955 ≤ 0.0277778 | 本轮未独立复跑该 Oracle（无构建）；可作独立复算项 | **INSUFFICIENT-EVIDENCE** | DESIGN-OK-EVIDENCE-MISSING | 独立复算 bilinear 误差界与负向 mutation（Python 可做） |

---

## 2 RELEASE-02 change-claim 独立复核

| 编号 | claim | 主张 | 独立证据 | 判定 | 三分类 | 处置 |
|---|---|---|---|---|---|---|
| R2-CC-01 | FIX-SCI-DRZ-001 | DRIZZLE §5 legacy 归一给 S_p=B0/pixfrac²，与 §7 S_p=B0 互斥；§5 错，应改为面亮度保持 S_p=ΣB_j a_jp/Σa_jp | 独立核验 Fruchter & Hook 2002 式(3)（I'=[d·a·w·s²+I·W]/W'，"s² to conserve surface intensity"）与式(7)（a_xy = "fractional area overlap of the drop"），arXiv:astro-ph/9808087（DOI 10.1086/338393 ✅）；独立代数复算 S_p^{legacy}=B0/pixfrac²、目标式=B0；代码 drizzle_engine.cpp:1531,1553-1554 仍 legacy、aio_hips_writer.cpp:707 sig=flux/area | **CORRECT**（文档订正成立） | DESIGN-OK-DOC-WRONG（实现仍 legacy = DISP-DRZ-009 代码缺口） | 文档保持；实现按 weight*=pixfrac² 修复（默认 pixfrac=1 数值不变） |
| R2-CC-02 | FIX-SCI-WCS-001 | ASTROMETRY §5a 旧"0-based/常量 1px 平移"是标签错误；求解器 u=det_x−w/2=p−CRPIX=q，输出 x=u+CRPIX=p 为 1-based，**无 1px/0.5px 误差** | Paper I（DOI 10.1051/0004-6361:20021326 ✅）式(1) q_i=Σm_ij(p_j−r_j)，r_j=CRPIX_j（arXiv:astro-ph/0207407 逐字）；代码 sdet_api.cpp:546（dx=x+0.5−cx）、ipv_select.cpp:943,947（cx=w/2）、ipv_wcs.cpp:158-162（crpix=cx+0.5）、:942-946（out.x=u+crpix）；astropy 7.0.1 复算：origin=1 [[512.5]] 与 origin=0 [[511.5]] 同给 CRVAL ⇒ 512.5 为 1-based 对象 | **CORRECT** | DESIGN-OK-DOC-WRONG（ipv_wcs.h:43,57-71 注释标签错误仍在，comment-only） | 文档保持；建议 comment-only 修 ipv_wcs.h 标签 |
| R2-CC-03 | FIX-SCI-S2-PHOT-001 | mag_tolerance=3.0 传入点为 pc_api.cpp:139,398；star_matcher.cpp:241 只是 psf_valid 诊断；F_syn 应为相对刻度 | pc_api.cpp:139-142,396-401 逐字 cleanAndScale(...,3.0 /* mag_tolerance */, ...)；star_matcher.cpp:241-245 为 out_diag->psf_valid=... → 锚订正成立；PHOTOMETRY.md §6 相对刻度降级合理（F_syn 不含 1/hc，常数被 IRLS location 吸收） | **CORRECT** | — | 保持 |
| R2-CC-04 | FIX-SCI-S2-P3-001 | PHASE3 §1 非目标与 §9a-10 由"variance/ivar 显式拒绝"改为"必须显式消费传播"，消除同合同互斥 | 现文 PHASE3_HIPS_TO_FITS.md:32 非目标已改为 weight/support，并注 "variance/ivar 子产品输入为例外"；:147-148 §9a-10 已改为"必须显式消费传播…weight/support 仍拒绝" → 互斥消除 | **CORRECT** | — | 保持 |
| R2-CC-05 | FIX-SCI-report.md §5 | "RELEASE-01 已完成非冻结文档订正"与现状不完全一致 | 复核属实：01_calibration.md:31 方差式未改、STAR_PSF_ALGORITHMS.md:73 LM 参数正文未改（见 R2-B-02 与 P1 分片）；报告已如实指出 | **CORRECT**（自述） | — | 归前台变更 claim |

---

## 3 P1 全面扫（docs/science/** 与 docs/algorithms/**）

> 由 4 个次级 SubAgent 分片执行（P1-A science 核心 / P1-B science 几何与 mosaic / P1-C algorithms phase1+shared / P1-D algorithms phase2/phase3+v6），证据标准与三分类口径由本审计员统一；次级结论经本审计员复核后并入。分片原始台账：run/RELEASE-02/p1/P1-A.md、P1-B.md、P1-C.md、P1-D.md。

### 3.1 P1-A（science 核心 10 篇）— 已完成，本审计员复核要点

- 28 条发现；CORRECT 14 / WRONG 6 / AMBIGUOUS 4 / INSUFFICIENT-EVIDENCE 2 / CONTRADICTS-DESIGN 2；**BLOCKS-DESIGN 0**。
- 与本审计员 P0 结论一致：frame_snr（P1A-007）与 sky_plane（P1A-022）为"文档/FROZEN 模型 ↔ 设计"冲突，设计正确 → DESIGN-OK-DOC-WRONG + 代码缺口，非 BLOCKS-DESIGN。
- 新增独立发现（本审计员抽查认可，纳入待订正清单）：
  - P1A-006 **WRONG（引用）**：NOISE_MODEL.md:184 的 DOI 10.1051/0004-6361:20021569 经 Crossref 解析为 Homeier et al. 2002 A&A 397,585；Starck/Donoho/Candès 2003 A&A 398,785 的正确 DOI = 10.1051/0004-6361:20021571。
  - P1A-013 **WRONG（文档低估实现）**：UNCERTAINTY_AND_COVARIANCE.md:100-102 称重采样器只做对角 Σc_k²u_k、"不构建 C_in"；实装 p3_rsmp_covariance.cpp:22-47 实现完整 M=R C_x、C_y=M Rᵀ（用于 p3_rsmp_propagation.cpp:98,212，并有 fail-closed 相关核门 p3_rsmp_failclosed.cpp:97-142）→ 设计 §11.1 / UNIFIED §7 在此已满足。
  - P1A-005 **AMBIGUOUS**：天空预算系数 1.44（NOISE_MODEL §5a）vs 1.144（noise_model.cpp:492）不一致。
  - P1A-004 **AMBIGUOUS**：零点标准误常数 1.253 应为 √(π/2)=1.2533141373（snr_science.cpp:246 硬编码）。
- 最大证据缺口 G1（大）：NOISE_MODEL §5a/§11/§14.4、UNCERTAINTY V19R3、CALIBRATION §11 的数值 Oracle 均绑定 run/PROJECT-GOVERNANCE-01/{MASK-001,R-5,SAT-001,UNIT-001}/ 下**缺失的复跑脚本**，约 10 条数值主张当前不可独立复现。

### 3.2 P1-B（science 几何与 mosaic）— 进行中（DRIZZLE 分片已复核）

- 已复核并纳入本台账的独立发现：DRIZZLE legacy 归一（=R2-CC-01）、UPM 纯加性 vs 设计 g·s+b（=R2-C-02）、k_corr 表区间漏 3.2035 与 1.4 非保守（=R2-H-28/R2-H-29）。
- 本审计员已独立复算并认可：F&H 式(3)(5)(7) 逐字（arXiv:astro-ph/9808087v2 p.5/p.14）；drizzlepac master 3e922c23（BSD-3-Clause）cdrizzlebox.c:324-337/978-1018 一致加权均值；hp_res=√(π/3)/nside；极区 Lipschitz C=π/2、C45=π/(2√2)（gaia_client.c:969,1005-1007）；DRIZZLE §5「Girard 定理」命名与实现（Van Oosterom & Strackee 1983 atan2 扇形三角剖分）不符；§6「1.532·res 对角线」把直径当半径比（实测半径 0.7675·hp_res，最坏 1.044，1.25 缓冲结论仍成立）。
- **P1-B 最终**：57 条发现；CORRECT 38 / AMBIGUOUS 6 / WRONG 4 / INSUFFICIENT-EVIDENCE 8 / CONTRADICTS-DESIGN 1；triage DESIGN-OK-DOC-WRONG 11、DESIGN-OK-EVIDENCE-MISSING 10；**BLOCKS-DESIGN 0**。
  - **P1B-UPM-04 CONTRADICTS-DESIGN**（=R2-C-02）：PHASE2_UPM.md:8,157 + 加性实现落后于设计 §4.2 g·s+b；更正：MA 求解器存在（见 R2-C-02 独立复核）。
  - **P1B-P3-04 WRONG（新，标准符合性，本审计员已独立定案）**：PHASE3_HIPS_TO_FITS.md:63 与 aio_hips_writer.cpp:1119 声称 IVOA REC-HIPS-1.0 §4.4.1 的 hips_frame 值域 = {icrs, galactic, ecliptic}，并把标准值 equatorial 当"非标准值废止"。本审计员对 **REC-HIPS-1.0-20170519 PDF 逐字抽取**：hips_frame = 「equatorial」(ICRS) / 「galactic」/ 「ecliptic」，示例即 hips_frame = equatorial；**icrs 不是合法值**。实装 aio_hips_writer.cpp:1122,1371 写 hips_frame=icrs → 产品输出违反 IVOA HiPS 1.0（设计 §5.1 要求 IVOA HiPS）。见 §3.6 R2-L-01。
  - **P1B-P3-06 WRONG**：PHASE3:126,151 称 bilinear 误差 O(h²)；P1-B 在 astropy_healpix 单元中心复现精确四象限方案得 max_err/h=0.65 常数（nside 128→2048，翻倍比 2.00）⇒ **O(h)**（4 个最近中心不构成平行四边形，线性场不可复现）。该结论取决于"bilinear"指哪个注册核（U-K 未定）——若指生产 bilinear_area_overlap_exact 则不适用；需先统一词表（R2-K-01）。
  - P1B-DRZ-04（=R2-CC-01，代码缺口）；P1B-UPM-02/03（=R2-H-28/H-29）。
  - 已独立复算确认 CORRECT：F&H eq(5) 逐字；Paper I §2.1.1 eq(1)/§2.1.4；drizzlepac master 3e922c23 cdrizzlebox.c:324-337,1012,1018；HEALPix circumradius max 1.042·hp_res<1.25；Var(median)=πσ²/(2N) MC；percentile all-reject MC；Phase3 TAN vs astropy 7.0.1 = 1.0e-10 px；AE Lipschitz π/2、π/(2√2)；AIT 半轴 2√2、√2 rad。
- 其余（ASTROMETRY 闭环阈值、ACR CPU/GPU 等价）已并入 §3.6 证据缺口。

### 3.3 P1-C（algorithms phase1+shared）— 已完成，本审计员复核要点

- 42 条发现；CORRECT 25 / WRONG 12 / AMBIGUOUS 3 / INSUFFICIENT-EVIDENCE 2 / CONTRADICTS-DESIGN 0；**BLOCKS-DESIGN 0**。报告 run/RELEASE-02/p1/P1-C.md。
- 本审计员独立复核认可的硬发现：
  - **P1C-DRZ-01 WRONG**：DRIZZLE_GEOMETRY.md:89 写 HEALPix 尺度 211034.6″/nside；公式 √(π/3)(180/π)3600 = **211076.285142**（本审计员复算一致）。实现按公式算（drizzle_engine.cpp:684-685，注释 211076.3），但 module_adapters.cpp:3498、orchestrator.cpp:173-174,191、hp_drizzle_api.h:98 四处注释仍写 211034.6。nside 决策用计算值 ⇒ 无功能错误。
  - **P1C-WCS-02/03 WRONG（引用）**：PLATESOLVE.md:256 两个 DOI 错——10.1086/114121 经 Crossref 实为 Tomkin 1986 AJ 91,1428（Groth 1986 AJ 91,1244 正确 = **10.1086/114099**）；10.1086/133670 实为 Nemiroff 1995 PASP 107,1131（Valdes 1995 PASP 107,1119 正确 = **10.1086/133667**）。本审计员 Crossref 独立复核一致。
  - **P1C-COS-02 WRONG**：COSMETIC_ALGORITHMS.md:312 Pych 2004 DOI 10.1051/0004-6361:2003574 死链（404）。
  - **P1C-HIP-01 WRONG**：HIPS_WRITER.md:407 Fernique 2015 DOI 分隔符写成冒号（应为斜杠 10.1051/0004-6361/201526075）。
  - **P1C-STD-01/02/03 WRONG**：STAR_DETECTION_ALGORITHMS.md peaker/record 锚整体漂移约 +145-150；「生产候选阶段截断 maxStars×2」在 sdet_api.cpp 只存在于 legacy 路径（:1135,1470），生产 sdet_detect_impl 仅输出截断（:2394-2395）；s_factor 3.7172 应为 3.71692219。
  - **P1C-PSF-01/03**：MOFFAT4_FWHM_FACTOR 1.230310 应为 1.2303076526；kTrimMeanToSigma 0.7316727929211932 应为 0.7316730953（与 R2-H-09/H-10 一致）。
  - **P1C-ACR-01/02**：ACR 等价容差 1e-6（fp32）/1e-12（fp64）未声明数值域，非尺度不变（~6.5e4 ADU 时 fp32 max_abs 2.1e-2）→ DESIGN-OK-EVIDENCE-MISSING。
- 已独立复算确认 CORRECT：1.482602218505602=1/Φ⁻¹(0.75)、0.6744897501960817=Φ⁻¹(0.75) 精确；Moffat4 flux=2πA·sxsy/3；HP_CIRCUMRADIUS_FACTOR 1.25 vs 最坏 1.044；噪声默认 8×8/r0=10/scale=6/clip5/min64/rounds2/floor1e-12；MOC UNIQ/(511−x)·512+y；SIP NB_GRID=41/81；s0=206.265·pixel_um/focal_mm；rejection 路由阈值。

### 3.4 P1-D（algorithms phase2/phase3+v6）— 进行中（已并入 17 条）

- 已并入本台账的独立发现：
  - **P1D-012（更正）**：g·s+b 的乘性-加性求解器 **存在**（p2_upm_ma_build，upm.cpp:1898；stage2.cpp:516；phase2_integrate.cpp:1627）；落后的是 FROZEN 科学文档与 scheduler fit_upm 适配器（module_adapters.cpp:4155 走 p2_upm_build_geo）。见 R2-C-02。
  - **P1D-001 WRONG**：PHASE2_INTEGRATION.md §3:64/§11.3 仍登记 DISP-P2INT-001（sup_max 在 w==0 continue 之后更新），与 §5/§7/§10.2 及实装 integrate.cpp:49-50 自相矛盾。
  - **P1D-002 WRONG**：PSFSW_ALGORITHM_SPEC.md §1.4 Wt 量纲应为 1/px²（按 α=2,β=1,γ=2,δ=1 代数），非 component_flux_unit/px²。
  - **P1D-003 / P1D-008**：α=2,γ=2、N=星间散度无本项目标定（=R2-H-05）；MAD 常数本身正确但适用域（高斯核）需声明。
  - **P1D-009/010/013**：k_corr=1.4 的 MC 源 control_median_mc_test 为构建孤儿（CMake/ci 零命中）；独立 MC（OBSERVATION_MODEL_REVIEW.md:177-179）平面几何得 2.1909；sampler.cpp:93-117 对 300-600″/px 表做双线性插值而生产为 0.96″/px（永远回退 1.4）→ 证据缺失/文档冲突（=R2-H-29）。
  - **P1D-006**：K=5 时 rho_s=0.8 精确置换 p=16/120=0.1333，文档 ≈0.10 是 t 近似。
  - **P1D-004**：像素 ivar 门只测上界 rho≤1+ε，未机器校验无偏 R̃A=I。
- 已独立复算确认 CORRECT：Huber δ=1.345 恰 95% 高斯效率；Var(median)≈πσ²/(2N)（MC ratio 1.00003）；pixivar ε 数值（2.47%/0.027 mag、9.54%/0.099 mag）；rejection 路由阈值。
- **P1-D 最终**：24 条发现；CORRECT 10 / WRONG 6 / INSUFFICIENT-EVIDENCE 3 / AMBIGUOUS 4 / CONTRADICTS-DESIGN 1；triage DESIGN-OK-DOC-WRONG 9、DESIGN-OK-EVIDENCE-MISSING 6；**BLOCKS-DESIGN 0**。
  - **P1D-006 投影 CONTRADICTS-DESIGN（=实现落后）**：ASTROCS_DESIGN.md:269 要求冻结 8 种投影；registry v3 仅 4/8，生产仅 TAN（p3_wcs.cpp）→ DESIGN-OK-DOC-WRONG（实现缺口，PHASE3_PROJ_IMPL §1/§6.5 已自登记）。
  - **P1D-005 FZ-P3-KERNEL-REGISTRY 未在生产强制**：admit() 仅在 p3_v6_export.cpp:474（测试路径）；生产 p3_session.cpp:116-118 直接校验 sampler∈{nearest,bilinear} 并调 legacy p3_sampler_open_ex → 与 R2-K-01 同源。
  - P1D-001（PHASE2_INTEGRATION 自相矛盾）、P1D-002（PSFSW Wt 量纲应为 1/px²）、P1D-003/008/009/010/013（PSFSW/k_corr 证据缺失）已并入上文。
  - 已独立复算确认 CORRECT：bilinear_4quad kernel oracle（逐位复现 run/v6/alg-p3/data/kernel_oracle.json：0.0273955/0.9862/1.3223、界 1/36=0.0277778）；rejection 路由阈值（rejection.cpp:1076-1079,1053-1065 与 REJECTION_ALGORITHMS F1-F6 一致）；Huber δ=1.345 恰 95% 高斯效率；Var(median)≈πσ²/(2N)（MC 1.00003）；pix-ivar ε（2.47%/0.027 mag、9.54%/0.099 mag）；GAIA AE Lipschitz；A_cell=4π/(12 nside²)；PHASE3_FITS 关键字类型。
  - 未深审（后续候选）：PSFSW α/γ 推导、k_corr MC、pix-ivar rho≥1 无偏性、PHASE2_SESSION/UPM_IMPL/SAMPLER、INTEGRATION_ALGORITHMS、PHASE2_MOSAIC_WRITE、v6/phase1/** 冻结阈值、phase3 CAR/TAN 立体角。

### 3.5 P1 跨分片新增高价值发现（本审计员独立定案）

| 编号 | 文档:行 | 主张原文 | 独立证据 | 判定 | 三分类 | 建议处置 |
|---|---|---|---|---|---|---|
| R2-L-01 | docs/science/PHASE3_HIPS_TO_FITS.md:63；lib/infrastructure/aio/src/hips/aio_hips_writer.cpp:1119,1122,1371 | 声称 IVOA REC-HIPS-1.0 §4.4.1 的 hips_frame 值域 = {icrs, galactic, ecliptic}；写出侧把非标准值 equatorial 废止，改写 icrs | **本审计员对 REC-HIPS-1.0-20170519 官方 PDF 逐字抽取**（pypdf 6.19.0）：hips_frame – Format: 「equatorial」(ICRS), 「galactic」, 「ecliptic」；示例 hips_frame = equatorial；HiPS 2.0 草案同。**icrs 非法** | **WRONG**（标准符合性） | DESIGN-OK-DOC-WRONG | 写出侧改回 hips_frame=equatorial（或按标准别名处理）；读侧接受 icrs 作旧产品别名；同步 PHASE3_HIPS_TO_FITS.md:63 与单元测试 p1hips_tests_units.cpp:288-289（该测试当前断言 icrs 且拒绝 equatorial，方向反了） |
| R2-L-02 | docs/science/PHASE3_HIPS_TO_FITS.md:126,151 | bilinear 采样误差 O(h²) | P1-B 在 astropy_healpix 单元中心复现四象限方案：max_err/h=0.65 常数（nside 128→2048，翻倍比 2.00）⇒ O(h)（4 中心非平行四边形，线性场不可复现）；kernel registry 的 (h²/8) 界只对平面平行四边形/规则网格成立。**但**生产默认核是 bilinear_area_overlap_exact（registry §1），若文档指该核则结论不适用 | **AMBIGUOUS**（依赖 U-K 词表定案） | DESIGN-OK-DOC-WRONG | 先统一 kernel_id（R2-K-01），再按真实生产核给误差阶；若为四象限方案须改 O(h) 并加 order_sel 约束 |
| R2-L-03 | docs/algorithms/PHASE2_INTEGRATION.md:64,235-241 | 登记 DISP-P2INT-001「sup_max 在 w==0 continue 之后更新（:54-55）」 | 同文 §5:112/§7:174-179/§10.2 与实装 integrate.cpp:49-50 均表明 sup_max 在权重分支之前 → 同文自相矛盾 | **WRONG** | DESIGN-OK-DOC-WRONG | 删除/订正 §3:64 与 §11.3 的 DISP-P2INT-001 残留 |

### 3.6 分片证据缺口（并入 EVIDENCE-GAPS.md）

- P1-B：candidate_oracle 9003 例门与 STD-F1 九宫格桥接数值不可跑；DrizzlePac Handbook 引文未核；control_median_mc_test/kcorr_matrix_test 构建孤儿；REJECTION「domain exactly n=4」n=3 可达性未证；NIST ESD「120/120」无可定位来源；PHASE3 部分覆盖 C 语义冲突；ACR CPU/GPU 等价不可测（DORMANT）。
- P1-D：PSFSW α/γ 推导、k_corr MC、pix-ivar rho≥1 无偏性、PHASE2_SESSION/UPM_IMPL/SAMPLER、INTEGRATION_ALGORITHMS、PHASE2_MOSAIC_WRITE、v6/phase1/** 冻结阈值、phase3 CAR/TAN 立体角未深审。
- P1-C：付费文献内部方程未重推；WCS 外部闭环阈值自述 UNJUSTIFIED（发布门=N）；ACR 绝对容差非尺度不变。
- P1-A：G1 数值 Oracle 复跑脚本整体缺失；Moffat 1969/Gaia DR3 article-level only。

---

## 4 台账汇总（截至本轮）

### 4.1 P0 + change-claim + 跨分片新增（本审计员直接产出，共 63 行）

| 判定 | 条数 |
|---|---|
| CORRECT（含已订正/已闭合） | 31 |
| WRONG | 11 |
| AMBIGUOUS | 6 |
| CONTRADICTS-DESIGN | 7 |
| INSUFFICIENT-EVIDENCE | 8 |
| **合计** | **63**（P0 55 + change-claim 5 + 跨分片新增 3） |

| 三分类 | 条数 |
|---|---|
| **BLOCKS-DESIGN** | **0** |
| DESIGN-OK-DOC-WRONG | 约 24（含代码落后） |
| DESIGN-OK-EVIDENCE-MISSING | 8 |
| （设计本身成立，无需处置） | 31 |

### 4.2 P1 分片（次级 SubAgent，本审计员统一口径并抽查复核）

| 分片 | 发现数 | CORRECT | WRONG | AMBIGUOUS | INSUFF | CONTRADICTS | BLOCKS-DESIGN |
|---|---|---|---|---|---|---|---|
| P1-A（science 核心） | 28 | 14 | 6 | 4 | 2 | 2 | **0** |
| P1-B（science 几何/mosaic） | 57 | 38 | 4 | 6 | 8 | 1 | **0** |
| P1-C（algorithms phase1+shared） | 42 | 25 | 12 | 3 | 2 | 0 | **0** |
| P1-D（algorithms phase2/3+v6） | 24 | 10 | 6 | 4 | 3 | 1 | **0** |
| **P1 小计** | **151** | **87** | **28** | **17** | **15** | **4** | **0** |

### 4.3 结论汇总

- **BLOCKS-DESIGN = 0**（P0 11 主题 + 5 change-claim + P1 全部分片 151 条 + 跨分片新增 3 条；合计约 214 条发现）。
- 四个 P1 分片各自独立报 **BLOCKS-DESIGN 0**；两条初判 BLOCKS-DESIGN 候选（DRIZZLE legacy、UPM 缺 g）经本审计员规则复核后降级为 DESIGN-OK-DOC-WRONG（设计正确、实现/文档落后），其中 UPM 缺 g 经独立复核确认 MA 求解器已存在（见 R2-C-02）。
- 绝大多数发现是：**文档/契约/实现落后于正确设计**（DESIGN-OK-DOC-WRONG），或**设计合理但缺证据/常数无出处/证据不可复跑**（DESIGN-OK-EVIDENCE-MISSING）。
- 最高设计的科学意图逐条经一手文献与独立复算验证成立（见 VERDICT.md §2）。
- **产品面最严重的新发现**：R2-L-01 hips_frame=icrs 违反 IVOA HiPS 1.0（本审计员对官方 PDF 逐字定案）——这是产品输出的标准符合性缺陷，设计 §5.1 要求 IVOA HiPS。
