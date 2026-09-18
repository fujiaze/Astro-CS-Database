# RELEASE-02 独立科学审计 — VERDICT（回答核心问题）

- 审计员：独立科学审计 SubAgent（RELEASE-02 / SCI-AUDIT）
- 日期：2026-09-18
- 关联台账：reports/RELEASE-02/SCI-AUDIT/REGISTER.md

## 1 核心问题的答案（结论先行）

**问题：这些科学/算法文档里的问题，会不会阻塞或推翻负责人的最高设计？**

**答案：不会。** 在 P0 全部 11 个 UNRESOLVED 主题、RELEASE-02 全部 5 条 change-claim、以及 P1-A 分片 28 条发现中，**BLOCKS-DESIGN = 0 条**。

没有任何一条发现表明：
- ASTROCS_DESIGN.md 自相矛盾到无法执行；
- 设计要求的量在科学上不可实现或与物理量不符；
- 设计与自身引用的科学不变量冲突。

已发现的全部问题都属于：**设计意图成立，而文档/契约/推导/实现落后于设计**（DESIGN-OK-DOC-WRONG），或**设计合理但缺证据/常数无出处/证据不可复跑**（DESIGN-OK-EVIDENCE-MISSING）。

## 2 设计意图本身是否成立（逐条判断）

| # | 设计条款 | 独立证据 | 设计是否成立 |
|---|---|---|---|
| 1 | 帧级 SNR = 未加权通量型信噪比 F_ref/σ_F（ASTROCS_DESIGN §3.4/§4.3） | Horne 1986 PASP 98,609（DOI 10.1086/131801 ✅）给出 σ_F⁻²=ΣP²/σ²；本审计复算 E7：w=1/σ_F²=SNR²/F_ref² 逐位相等；E8 证明功率比型不能做该换算 | **成立**（通量型口径科学正确） |
| 2 | SNR 是信噪比不是权重；Phase2 用逆方差叠加现场换算 w=1/σ²（§2/§4.3） | 与 Horne/Naylor 最优提取一致；设计明确禁止"直接用 SNR 加权" | **成立** |
| 3 | UPM 联合相对模型 g·s+b（§4.2）；天光面 b_k(x) 稀疏、跨帧联合、SNR 加权最小 RMS（§4.4） | SCAMP（GPL-3.0）标准做乘性相对光度 + SWarp 加性背景；插件 10_sampling/11_upm 与设计逐条一致；数学上可辨识、可实现 | **成立** |
| 4 | 稀疏控制点层：实际 SNR = 帧级 × 帧内（§3.4） | 与"帧级为参考、帧内为局部修正"语义一致 | **成立** |
| 5 | Drizzle 常量面亮度保持、总通量守恒（§11.1；Fruchter & Hook） | Fruchter & Hook 2002 式(3)(7)（DOI 10.1086/338393 ✅，arXiv:astro-ph/9808087）；本审计独立代数复算：面亮度保持式 S_p=ΣB_j a_jp/Σa_jp 给 B0，legacy 式给 B0/pixfrac² | **成立** |
| 6 | 协方差传播 C_out=R C_in Rᵀ；相关噪声禁止 1/Σw（§11.1） | Fruchter & Hook 2002；实装 p3_rsmp_covariance.cpp 已实现完整二次型（P1-A 独立发现） | **成立** |
| 7 | 校准方差传播（§3.6 硬约束） | 独立代数给出正确一般式 [V(r)+(1−α)²V(b)+α²V(d)+y²V(f)]/f²；V6 实现已按此折叠 bias 系数 | **成立** |
| 8 | 投影首批冻结 8 种（§5.3）；HiPS 面亮度语义（§5.1/§4） | WCS Paper II（DOI 10.1051/0004-6361:20021327 ✅）；IVOA HiPS 1.0 | **成立** |
| 9 | 独立帧 SNR_comb²=ΣSNR_k²；相关项存在时拒绝简单求和（§11.1） | 逆方差合并的代数恒等（RELEASE-01 E3 与 Horne）；成立条件明确 | **成立** |
| 10 | 三命令独立、阶段间只通过磁盘产品+manifest+哈希交换（§1.2/§7） | 本审计实跑 build/astrocs help 与配置预检，命令面存在；属工程架构，无科学冲突 | **成立** |

## 3 BLOCKS-DESIGN 清单

**空。** 逐条说明"最像阻塞"的候选为何不构成 BLOCKS-DESIGN：

1. **DRIZZLE legacy 归一（DISP-DRZ-009）** — 设计 §11.1 要求常量面亮度保持；Fruchter & Hook 式(3)(7) 支持设计的归一化；实装仍是 legacy w=a/A_drop（仅 pixfrac<1 偏 1/pixfrac²，默认 pixfrac=1 无影响）。这是**代码落后于正确设计**，不是设计问题。→ DESIGN-OK-DOC-WRONG（代码缺口）。
2. **PHASE2_UPM.md（FROZEN）纯加性、无 WCS** — 设计 §4.2/§4.4 要求 g·s+b 与跨帧联合稀疏天光面；科学文档停留在旧实现口径。设计可辨识、可实现（SCAMP/SWarp 先例）。→ DESIGN-OK-DOC-WRONG。
3. **frame_snr 三方冲突** — 设计口径（通量型 F_ref/σ_F、写 HiPS 头、w=SNR²/F_ref²）科学正确且可复算；CONTROL_WEIGHT_SNR.md 的"相对质量权重"是旧重定义，实装输出 5σ 深度对象。→ DESIGN-OK-DOC-WRONG（文档 + 代码缺口）。
4. **校准母版方差传播** — 设计 §3.6 明列为硬约束；CALIBRATION.md §9 说"不传播"；正确一般式存在且实装 V6 已实现。→ DESIGN-OK-DOC-WRONG。
5. **k_corr=1.4 "保守"性** — 该常数是 Project-defined 校正，非设计硬性量；且 k_corr 证据源不可复跑、在 pixfrac=1.0 标定域内 1.4<1.4980。这是**证据缺失/标定过时**，不是设计不可实现。→ DESIGN-OK-EVIDENCE-MISSING。

## 4 唯一需要"设计措辞澄清"的点（仍不构成阻塞）

ASTROCS_DESIGN.md:235 在同一句里既定义 frame_snr 为通量型 F_ref/σ_F，又说"其信号/噪声估计方法学**对标 PixInsight 公开文档中的 PSFSNR（ratio-of-powers 信噪比）**"。字面读，功率比型 PSFSNR（(Σf)²/σ_n²）与 w=SNR²/F_ref² 不能同时成立（复算 E8）。

但该句紧接着明确"数学定义采用通量型口径以保证逆方差换算严格成立"，操作要求自洽；且 UNIFIED_MODEL.md:42、07_noise_snr.md:66、研究包 §2.2 已把"方法学对标 ≠ 公式套用"写清。因此：

- 判定 **AMBIGUOUS / DESIGN-OK-EVIDENCE-MISSING**（措辞澄清）；
- **不是 BLOCKS-DESIGN**：设计自身给出了可执行且正确的通量型定义，歧义只在"对标"一词的强弱。
- **建议**：把该句改为"仅方法学（恒星测光取信号、稳健噪声、独立背景）对标 PSFSNR；数学定义采用通量型 F_ref/σ_F，不采用其功率比式[18]"。这是 doc-only 澄清，不触顶层合同。

## 5 对负责人的建议（方案与代价）

1. **按设计订正文档，而非改设计**：U-A/U-B/U-C/U-D/U-E/U-H/U-K 的全部冲突都指向"文档/契约/实现落后"。建议走 ENGINEERING_SPEC §3 变更 claim 逐条订正（台账已给"建议处置"列）。代价：文档 + 少量契约/实现改动 + 一致性回归；不触三命令划分与产品数据模型。
2. **优先闭合 3 个"发布语义相关"项**（若本预览版要求语义自洽）：
   - frame_snr 唯一化（R2-A-02/A-03/A-04）——否则"帧级 SNR 入 HiPS 头"这一设计输出合同未兑现；
   - PHASE2_UPM 目标模型 vs 现行实现（R2-C-02）——决定 mosaic 是否宣称"相对定标"；
   - DRIZZLE pixfrac<1 面亮度（R2-CC-01 / DISP-DRZ-009）——默认 pixfrac=1 下潜伏，但任何 pixfrac<1 运行测光偏。
   代价：前两项需实现工作量（较大），第三项为一行权重修正 + 回归。
3. **补证据清单**见 EVIDENCE-GAPS.md；其中"k_corr MC 不可复跑""~10 条数值 Oracle 脚本缺失""PSFSW 指数无 L1 标定"属**证据欠账**，不阻塞设计但阻塞"科学已冻结"的宣称。
4. **不要**以"行业也不传播母版方差"为由把设计 §3.6 降级；若要降级，需补 ccdproc/ip_isr 逐行证据并改设计（属顶层变更，须负责人批准）。

## 6 发布门视角的诚实结论

- 本审计**未发现**任何需要修改 ASTROCS_DESIGN.md 的科学理由。
- 本审计**发现**大量"设计已对、文档/契约/实现未对齐"的条目，其中若干（frame_snr 输出合同、UPM 相对定标、DRIZZLE pixfrac<1 面亮度）会直接影响"产品是否兑现设计承诺"的判断。是否把"设计↔文档/实现未对齐"当作本预览版的发布阻断项，属**负责人治理裁决**，不是科学阻塞。
- 就"科学/算法文档里的问题会不会阻塞或推翻最高设计"这一核心问题：**不会**。

## 7 P1 全分片收尾后的更新（结论不变）

四个 P1 分片全部完成，各自独立报 **BLOCKS-DESIGN = 0**（P1-A 28 条 / P1-B 57 条 / P1-C 42 条 / P1-D 24 条；合计 151 条，加 P0 与 change-claim 共约 214 条发现）。没有任何一条改变"设计意图成立"的判断。

新增的**产品面严重发现**（仍属 DESIGN-OK-DOC-WRONG，非 BLOCKS-DESIGN）：

- **R2-L-01 hips_frame = icrs 违反 IVOA HiPS 1.0**：设计 §5.1 要求输出符合 IVOA HiPS。本审计员对 **REC-HIPS-1.0-20170519 官方 PDF 逐字抽取**：hips_frame 值域 = 「equatorial」(ICRS) / 「galactic」/ 「ecliptic」，示例即 hips_frame = equatorial；**icrs 不是合法值**。但 aio_hips_writer.cpp:1122,1371 写 icrs，PHASE3_HIPS_TO_FITS.md:63 还反过来把标准值 equatorial 当"非标准值废止"，单元测试 p1hips_tests_units.cpp:288-289 断言方向也反了。这是**产品输出与 IVOA 标准不符**的互操作缺陷——设计意图（符合 IVOA）成立，实现/文档写错。
- **R2-L-02 PHASE3 bilinear 误差阶**：文档称 O(h²)，P1-B 复现四象限方案实测 O(h)（max_err/h=0.65 常数）。但结论取决于 U-K 词表（生产默认核是 bilinear_area_overlap_exact），需先统一 kernel_id。
- **R2-L-03 PHASE2_INTEGRATION 自相矛盾**：§3:64/§11.3 仍登记已修复的 DISP-P2INT-001。

其余 P1 高价值项（PSFSW α/γ 无标定、k_corr 1.4 证据不可复跑且非保守、DRIZZLE legacy、投影 4/8、kernel registry 未在生产强制、多处 DOI 错引）均已进 REGISTER 与 EVIDENCE-GAPS。

**最终判断**：BLOCKS-DESIGN = 0；最高设计不需要修改。建议负责人优先处理三项"设计承诺未兑现"：frame_snr 输出合同、UPM 相对定标（g 已实现但未接线/文档未对齐）、hips_frame 标准符合性；其余按台账逐条订正。
