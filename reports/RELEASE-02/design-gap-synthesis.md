# RELEASE-02 设计增补汇总（DESIGN-GAP-SYNTHESIS）

- 分片：DESIGN-GAP-SYNTHESIS（设计文档增补汇总）
- 触发：负责人 2026-09-19 晚令 —— 「把前面你 run 里面记录的那些汇总一下，看看**有哪些需要增补到最高设计**等等，还有**系列文档，哪些需要完善**」（`工程控制/RELEASE-02/GAP_AUDIT.md` §9.49 第 9 条，:1478）
- 工作目录：`/workspace/Astro CS Database`
- 日期：2026-09-19
- 机器可读台账：`reports/RELEASE-02/design-gap-synthesis.json`（同目录，字段 `id/target/file/loc/current/proposed/rationale/basis/needs_claim/priority`）

## 输入（全部只读）

| # | 输入 | 用途 |
|---|---|---|
| 1 | `工程控制/RELEASE-02/GAP_AUDIT.md` §9.17–§9.49（:418-1537） | **主输入**：全部裁决与发现 |
| 2 | `工程控制/RELEASE-02/change-claims/FIX-SCI-SNR-CANON-001.md` | 已固化裁决 D-1..D-5 + C2 与影响面登记 |
| 3 | `reports/RELEASE-02/conformance/CONFORM-SWEEP-{1,2,3,4}.{md,json}` | 350 条规范陈述 / 90 条不符 / 30 条未实现 / 39 歧义 / 22 待定 + **规范侧 34 条** |
| 4 | `reports/RELEASE-02/` 其余报告（`canon-freeze`、`snr-config-land`、`f-instr-conform-fix`、`stardet-conform-invest`、`conform-fix-a`、`phot-verify`、`q1/q2/q3`、`c-delta-ruling`、`chain-audit`、`unc-prop-audit`、`fix-upmscale-report` 等） | 分片结论与实测证据 |
| 5 | `reverse_verify/docs/`（`frame-snr-canon`、`f-instr-canon`、`snr-propagation-design`、`p1-spatial-gain`、`smooth-lambda`）与 `reverse_verify/references/` | 论文雏形级设计与文献定案 |
| 6 | `ASTROCS_DESIGN.md`（**只读**）、`docs/design/UNIFIED_MODEL.md`、`docs/science/**`、`docs/plugins/**`、`docs/contracts/CONFIG_CONTRACT.md`、`docs/development/CONFIG_SCHEMA.md`、`ENGINEERING_SPEC.md`、`ACCEPTANCE_SPEC.md`、`ci/checks.json`、`ci/check_config_defaults.py`、`tools/config_consistency_check.py` | 现状核对（逐条给出 file:loc） |

## 纪律声明

- **只读汇总**：除本报告与 `design-gap-synthesis.json` 外**未改任何文件**；`ASTROCS_DESIGN.md` 与 `docs/**` 全程只读。
- **零 git 写**：未 commit / push / amend；只做只读 `git status` 归因。
- **未跑** `ninja` / `cmake` / `ctest`；未跑任何会写盘的门禁（`tools/config_consistency_check.py` 仅读文件头判据，未执行）。
- **不臆造**：每条均给 `GAP_AUDIT §` / change-claim / 审计条目号 / 报告 file:loc 之一作为依据。
- **不把「已订正」写成「待订正」**：已订正/已裁决/已撤销的条目单列 B-III 与 §E-11，只作状态登记。

## 0 结论速览（给负责人）

| 项 | 数字 |
|---|---|
| 需增补到最高设计（`ASTROCS_DESIGN.md`） | **16 条**（A 节，P0 9 条 / P1 6 条 / P2 1 条） |
| 需完善的系列文档 | **65 条**（B 节：主线 16 条 + 审计规范侧 **43** 条 + 已订正登记 6 条） |
| 门禁需增补/补齐的检查 | **5 条**（C 节） |
| 合计台账条目 | **86 条** |
| 需变更 claim / 走变更流程 | **33 条** |
| 待负责人裁决 | **1 条**（D 节：**D-08**；**订正（2026-09-20，V5 分片 3）**：原「**10 条**（D 节）」作废，其余 9 条已裁决，见 F-3 表注 ①） |
| 审计发现但未定案 | **11 条**（E 节） |
| 已订正 / 已裁决 / 已撤销（禁止再写成待订正） | **6 条**（B-III）+ 1 组被推翻的前台结论（E-11） |

**最高设计的三处「硬矛盾」**（必须改，否则下游按最高设计实现就是错的）：

1. **`§4.2:236` 的 mermaid 仍写 `upm 联合相对模型 g·s+b`（乘性+加性）** —— 与 D-1/A2「UPM = 纯加性」裁决直接矛盾（全仓唯一还写着乘性模型的权威位置）。
2. **`§4.3:256` 仍写「Phase2 自动检测输入 HiPS 是否有稀疏 SNR 层」** —— 与 §9.46「默认消费稀疏层（`snr_path` 默认 `sparse_reconstruct`）、不按判据决定」在语义上不同（前者无法区分「有层但配置为 `frame_reconstruct`」与「无层」）。
3. **`§3.2:103` 仍写 `star_detection 源探测`（盲检测）** —— 与 §9.49 定案 1「星表引导拟合」是范式级差异；且 `§3.2` 流程图**没有**「应用测光归一化到像素」这一步，正是 `apply_photometry` 零调用者（P0）能长期存活的根因。

---

## 1 状态口径（四分，全文统一）

| 标记 | 含义 | 处理 |
|---|---|---|
| **①** | **已裁决待落地** —— 负责人已拍板（§9.38/§9.39/§9.40/§9.42/§9.45/§9.46/§9.49 或 claim），或报告已明确「规范对、修实现」 | 直接列，按 file:loc 执行 |
| **②** | **待负责人裁决** —— 两篇权威打架、科学行为变更、或报告明确上呈 | 单列 **D 节**，附选项与后果 |
| **③** | **审计发现但未定案** —— 审计只登记不改，或严重度已登记但未定处置 | 单列 **E 节** |
| **④** | **已订正 / 已裁决 / 已撤销** —— 已完成或已被否决 | 单列 **B-III**，**禁止再写成待订正** |

---

## A 需增补到最高设计（`ASTROCS_DESIGN.md`）的条目

> 每条给：主题 / 为什么必须进最高设计（而不是留在插件文档）/ 建议增补到哪一节 / 建议条文要点 / 依据 / 是否需变更 claim / 优先级。
> **现状锚点全部为本次只读核对所得**（`ASTROCS_DESIGN.md` 共 633 行）。

### DG-A-01 星表引导的检测（catalog-guided detection）取代盲检测

- **建议增补到**：`ASTROCS_DESIGN.md` §3.2:103（`D["star_detection 源探测"]`）；§3.6:218-220；§3.3:117-137
- **现状**：§3.2:103 写 `D["star_detection 源探测"]`（盲检测语义）；§3.6:218-220 只写「详细硬约束见 docs/plugins/algorithms_phase1/ 与 docs/science/」；§3.3:117-137 的 config 块只有 precision/output_dir/sparse_snr_layer/snr_path，无星表选择或极限星等键。
- **为什么必须进最高设计**：这是 Phase1 检测的范式级定义（检测定义域从「整幅图像」变成「Gaia 投影位置」），一次决定四件事：假源上界、星云区虚警、拟合失败语义、资源开销。留在插件文档则 Phase2 权重链与四层验收都无法据此判定「什么算一个源」。负责人 2026-09-19 直接令。
- **建议条文要点**：① 用本帧 WCS 把 Gaia 星表投影到像素域（反向映射）；② 只对星表位置做质心/PSF 拟合；③ 拟合成功 = 星点，失败 = 直接丢弃（不计虚警、不报错）；④ top 2–5 万亮星上限（多了不可能检测到那么多星点）；⑤ 按焦距/画幅(FOV)/曝光时间估算充足极限星等，超过的不做逆映射；⑥ 极限星等估算须与 WCS CD 矩阵板比例互校 + 宽松经验上界 + 用实测星等-SNR 关系收紧，不得用物理闭合（§9.42）；⑦ 取消全图盲检测连通域路径（或显式降级为可选诊断并标注非权威）。
- **依据**：GAP_AUDIT §9.49 定案 1（:1480-1485）；§9.43（:1220-1234）；CONFORM-SWEEP-1-002（生产检测器 345,960 条 vs sdet 1,473 条、Gaia 纯度 0.212%）；§9.48 ②（:1404-1418）
- **需变更 claim**：**是**　|　**优先级**：**P0**　|　**状态**：① 已裁决待落地

### DG-A-02 帧间独立原则：测光标定逐帧独立、无组间对比门、异光学系统混装不得报错

- **建议增补到**：`ASTROCS_DESIGN.md` §3.5:179-216（三命令通用预检）；§3.6:218-220
- **现状**：§3.5:204-207 把「不匹配的帧」列为 warn 级，但未限定「不匹配」只能按帧内判据；§3.6:218-220 无帧间独立条款；实现侧存在组间 k 散度门（`P1_PHOT_MAX_SPREAD_DEX`，f-instr-conform-fix §4.4）。
- **为什么必须进最高设计**：它改写预检的判据来源（「不匹配的帧」只能按帧内可信度判、不得按跨帧一致性判），并直接约束 CLI error/warn 分级与退出码（§6.3）。若只落插件文档，预检实现仍会保留组间门（现实现 `P1_PHOT_MAX_SPREAD_DEX`，已造成 12/12 板块被拒）。
- **建议条文要点**：① 每帧独立用 Gaia 标定到同一测光坐标系；标定后所有帧已在同一体系；② 跨帧 k 不同是正常的、正确的（不同夜/望远镜透明度本就不同）⇒ 要求跨帧 k 一致 = 逻辑错误；③ 删除组间 k 散度门；④ 只做帧内极度异常值拒绝 + 抛错，其他合理范围一律接受；⑤ 不同光学系统的帧混装不得报错；⑥ 门只有一个 = 单帧标定是否可信（匹配星数、拟合残差、极度离群），与其它帧无关；⑦ 显式区分「帧间一致性」是语义目标（都标到同一 Gaia 绝对测光体系）而非门禁判据 —— 与 claim FIX-SCI-SNR-CANON-001 §3.6 的措辞需协调（见 D-01）。
- **依据**：GAP_AUDIT §9.49 定案 2（:1487-1495，同时否掉 (b) 同夜分组 / (c) 跨夜另立判据）；reports/RELEASE-02/f-instr-conform-fix.md §4.4（:166-175）、§5（:184）
- **需变更 claim**：**是**　|　**优先级**：**P0**　|　**状态**：① 已裁决待落地（含一处需澄清，见 D-01）

### DG-A-03 SNR 三路径 + 稀疏层 Δ=64px + Phase1 唯一 SNR 产物 + Phase2 默认消费

- **建议增补到**：`ASTROCS_DESIGN.md` §4.3:256（「自动检测」）；§3.4:174-177（稀疏层）；§4.3:257-258
- **现状**：§4.3:256 写「Phase2 **自动检测**输入 HiPS 是否有稀疏 SNR 层：有 → 帧级×帧内；无 → 帧级」；§3.4:174-177 写可选稀疏层但**未给 Δ**；§4.3 全节未写「Phase2 不输出 SNR 面」。
- **为什么必须进最高设计**：§4.3:256 的「自动检测输入 HiPS 是否有稀疏层」是产物检测驱动，与 §9.46 的配置驱动 + 默认消费裁决语义不同（前者无法区分「有层但配置为 frame_reconstruct」与「无层」）；三路径是论文核心实验的载体，必须进最高设计才能约束合同与验收。
- **建议条文要点**：① 路径由配置 `snr_path` ∈ {dense, sparse_reconstruct, frame_reconstruct} 显式指定，默认 `sparse_reconstruct`；② Phase1 默认产出稀疏层（`sparse_snr_layer=true`），控制点间隔 Δ=64 px（`sparse_snr_spacing_px`），复用 UPM 8×8/tile 控制网格；③ 无稀疏层而路径为 sparse_reconstruct ⇒ 按帧级执行但显式记 `snr_path_effective` 并计数，不得静默；稀疏层损坏/不可重建 ⇒ fail-closed；④ 三路径精度对比是论文核心实验（判据 SP-0 降为诊断量、不作默认开关）；⑤ 只有 Phase1 HiPS 带 SNR 数据块；Phase2 不输出 SNR 面（叠加中消费、不复用）；Phase3 无 SNR；⑥ 不得预设稀疏一定最好（须写「何时哪种最优由实验回答」）。
- **依据**：GAP_AUDIT §9.38 D-2/A3（:1045-1055）；§9.45（:1299-1324）；§9.46（:1326-1370）；§9.49 定案 7；change-claim FIX-SCI-SNR-CANON-001 §3.2/§3.3；reports/RELEASE-02/snr-config-land.md §1/§3
- **需变更 claim**：否　|　**优先级**：**P0**　|　**状态**：① 已裁决待落地（键名订正另见 DG-B-08）

### DG-A-04 帧级 SNR 定义式（点源 PSF 信号 SNR；F_ref 共同基准）

- **建议增补到**：`ASTROCS_DESIGN.md` §3.4:165-169
- **现状**：§3.4:165-169 只写「类似 PSF-SNR 的信噪比」「要求**可靠且独立**」「SNR 是信噪比，不是权重」；**无定义式**、无 F_ref 条款、无 gain 缺省分支、无点源/面源边界。
- **为什么必须进最高设计**：现条文只有「类似 PSF-SNR」「可靠且独立」的定性描述、无定义式；而帧级 SNR 是 Phase1 写入 HiPS 的唯一科学量、也是 Phase2 权重链的输入，定义必须冻结在最高设计（否则同名两义会再次分叉）。
- **建议条文要点**：SNR_frame(k)=F_ref/σ_F,k；σ_i²=σ_sky²+(σ_R/g)²+max(F,0)·P_i/g；σ_F^-2=ΣP_i²/σ_i²（Horne 1986 对角近似）；gain 不可得时用天空受限分支 σ_F=σ_sky·sqrt(A_NEA)、SNR=F_ref·sqrt(ΣP_i²)/σ_sky；无量纲、对信号线性缩放严格不变；F_signal 已扣局部背景、天光只进 σ_F；固定 F_s 下 B↑ ⇒ SNR 单调下降、B→∞ ⇒ SNR→0；点源口径，不得与面亮度 SNR 混用；F_ref 必须组内公共且显式落盘（`reference_flux_scope=group`）。待负责人确认：是否改用固定星等（如「本帧对 20 等星的 SNR」）作为 F_ref 定义 ⇒ 完全帧间独立、连「组」都不需要（见 D-02）。
- **依据**：GAP_AUDIT §9.38 D-4/A1（:1035-1066）、§9.39 C1（:1100-1110）、§9.49 定案 7（:1507-1514）；reverse_verify/docs/frame-snr-canon.md §0/§2.2/§2.3/§2.4；change-claim §3.4/§6.1
- **需变更 claim**：**是**　|　**优先级**：**P0**　|　**状态**：① 已裁决待落地 + 一处待确认（D-02）

### DG-A-05 UPM 纯加性（raw − C_k；乘性残留归 Phase1 空间增益；g_k ≡ 1）

- **建议增补到**：`ASTROCS_DESIGN.md` §4.2:236（mermaid `upm 联合相对模型 g·s+b`）；§4.4:272（`b_k(x)`）
- **现状**：§4.2:236 的 mermaid 写 `U["upm 联合相对模型 g·s+b"]`（乘性+加性）；§4.4:272 却写「UPM 的**加性**背景 `b_k(x)`」—— 两处不一致，且前者是全仓唯一还写着乘性模型的权威位置。
- **为什么必须进最高设计**：最高设计的 Phase2 流程图目前仍把 UPM 写成 `g·s+b`（乘性+加性），与已固化裁决直接矛盾 —— 这是全仓唯一还写着乘性模型的权威位置，不改则下游按最高设计实现会复活 g_k。
- **建议条文要点**：① 模型式 `calibrated_f(p) = raw_f(p) − C_f(p)`（纯加性，8×8 control cell 双线性）；② `g_k ≡ 1` 本期不启用，`÷g²` 保持恒等式（公式与实现不删，退化等价）；③ 乘性残留 = 低阶空间增益，归 Phase1 `I_photo = k_photo·m(x,y)·I_cal`；④ 非目标「不处理乘性尺度差」保持有效；⑤ 撤销 FIX-A-UPM-001 的乘性方向（已 superseded）。
- **依据**：GAP_AUDIT §9.38 D-1/A2（:1035-1043）；§9.40 C3（:1123-1127）；change-claim FIX-SCI-SNR-CANON-001 §2/§3.1；docs/science/PHASE2_UPM.md §1:7-8、§14a:182
- **需变更 claim**：否　|　**优先级**：**P0**　|　**状态**：① 已裁决待落地

### DG-A-06 测光标定语义：消除物理单位、只使用星等；禁物理闭合反推与绝对窗口

- **建议增补到**：`ASTROCS_DESIGN.md` §3.1:94（使命）；§3.6:218-220（硬约束）
- **现状**：§3.1:94 只写「光度与天球坐标已定义、噪声可传播、PSF 可解释、帧级 SNR 可靠独立」；§3.6:218-220 只转引插件/科学文档，**无标定语义条款**（无「消除物理单位/只使用星等/禁物理闭合」）。
- **为什么必须进最高设计**：这是标定因子验收判据的根级定义。不进最高设计，绝对窗口守卫（`k_photo ∈ [0.1,10]` 之类）会被反复重新发明 —— 已发生一次并被负责人否决（ESC-P1-PHOT-ABS-WINDOW）。
- **建议条文要点**：① 标定目标 = 真实测光坐标系（星等）；手段 = 星点光通量积分 + Gaia + CCD QE + 滤镜透过率；② 消除物理单位，只使用星等；`k_photo`/`a_k`/`scale` 绝对值无物理意义（吸收增益/口径/曝光等未知量）；③ 禁止任何物理闭合式（`k = g·h·c·1e9/(A·t)` 之类）反推仪器参数或论证标定因子合理性；禁止为标定因子设绝对窗口；④ 有意义的判据只有两个（均尺度无关）：测光一致性（施加后星点星等与 Gaia 残差散度/MAD 小）与帧间同一测光体系（语义目标，不是门禁 —— 见 DG-A-02/D-01）。
- **依据**：GAP_AUDIT §9.42（:1181-1214，含负责人逐字纠正）；change-claim FIX-SCI-SNR-CANON-001 §3.6；docs/science/PHOTOMETRY.md §1:9、§10:105-106；reports/RELEASE-02/canon-freeze.md §2.5
- **需变更 claim**：**是**　|　**优先级**：**P0**　|　**状态**：① 已裁决待落地

### DG-A-07 Phase1 测光归一化必须真正落到像素 + 低阶空间乘法增益 m(x,y)

- **建议增补到**：`ASTROCS_DESIGN.md` §3.2:103-110（节点流程）；§3.6
- **现状**：§3.2:103-110 的流程图节点为 ingest→calibration→cosmetic→background/noise→star_detection→psf→platesolve→photometry→noise_snr→drizzle→验证→发布；**photometry 节点未标注「应用到像素」，全图无 apply 步骤** ⇒ 设计图上没有这个节点。
- **为什么必须进最高设计**：生产链上 `apply_photometry` 零调用者（P0），而最高设计 §3.2 的流程图里根本没有「应用测光归一化到像素」这一步 —— 这正是缺陷能长期存活的根因：设计图上没有这个节点，就没有门要求它存在。
- **建议条文要点**：① 节点流程显式补「apply photometry → 像素」步骤（在 drizzle 之前，或作为 calibration 的后继）；② 模型 `I_photo = k_photo·m(x,y)·I_cal`，m 为低阶（双线性/二次），必须用星点估（背景上乘性/加性不可辨识，数值 max|Δy|=3.6e-12）；③ 元数据如实落 `photappl`/`photscal`（不得写死 false/1.0）；④ 开启前置条件：合成实验 + 真实数据双重证明无问题（A4 裁决），且须先解决 F_instr 口径（A6）；⑤ 未开启时须显式记 `degraded_reason` 并 fail-closed，不得静默按未归一化 ADU 走完全链。
- **依据**：GAP_AUDIT §9.29（:753-795，P0 节点不存在）；§9.37 指令 1（:1003-1007）；§9.39 A4（:1072-1078）；change-claim §3.1；reports/RELEASE-02/phot-verify.md §2（:857-862）
- **需变更 claim**：**是**　|　**优先级**：**P0**　|　**状态**：① 已裁决待落地（A4 开启前置条件待实验满足）

### DG-A-08 合成测试必须模拟真实物理噪声过程（严禁纯加性天光）

- **建议增补到**：`ASTROCS_DESIGN.md` §11.1:566-574（科学正确性）；§11.3:589-600（L1 层）
- **现状**：§11.1:566-574 只写「独立解析/高精度/MC Oracle」「注入点源验证」「真实数据（M42/银心）验证接缝、背景、星形、排异、黑洞、预测/实测噪声」；**无噪声物理模型强制条款**；§11.3:595 的 L1 判据列也无「噪声物理模型声明」。
- **为什么必须进最高设计**：负责人把它定为强制方法论，它决定 Oracle 是否有效：纯加性天光实验测不出任何东西（实测真值无效应时度量恰为 0.0000%）。留在研究包则所有后续分片会继续产出无效合成实验（D4/Q1/Q2/Q3/A4/A5 均已发生）。
- **建议条文要点**：① 必须模拟噪声物理过程（不是算术改像素值）：源/天光/暗流各自 Poisson（电子域）；读出 Gaussian（电子域）；电子→ADU 的增益、饱和、量化；平场/空间响应 m(x,y)；天空梯度/加性天光面；按需宇宙线/坏点/PSF 变化；② 天光对 SNR 的影响必须通过散粒噪声体现（B↑ ⇒ σ² ⊃ B/g ↑ ⇒ σ_F ↑ ⇒ SNR↓），不得用「加常数」代替「加天光」；③ 优先用真实数据作底（负责人点名哈勃；本仓已有 M16 F657N/F673N，PHOTFLAM 提供合法绝对标定）；④ 每个合成测试必须能红能绿（负例真值「无效应」时度量归零）；⑤ 凡已有合成测试不符合本条者须重做或标注失效（D4 天光红线、Q1/Q2/Q3 合成部分、P1-SPATIAL-GAIN、A5、A4）。
- **依据**：GAP_AUDIT §9.41（:1143-1180）、§9.47（:1371-1392）；reverse_verify/docs/frame-snr-canon.md §3.0（:213-243）；reverse_verify/docs/snr-propagation-design.md §2.1.3/§13.1
- **需变更 claim**：**是**　|　**优先级**：**P0**　|　**状态**：① 已裁决待落地

### DG-A-09 单一星表 / 单一通量 / 三处复用（测光、PSF、SNR 共用检测与通量积分）

- **建议增补到**：`ASTROCS_DESIGN.md` §3.2:103-107；§3.6
- **现状**：§3.2 的 detection/psf/photometry/noise_snr 是四个独立节点、各自跑一遍；§3.4:155-161 只约束「一组输入 light → 一组 HiPS 输出」与「禁止静默丢弃」；**无「共用检测与通量积分」条款**。
- **为什么必须进最高设计**：现生产链检测→PSF→测光→SNR 各跑一遍，既是性能问题，也是 `fwhm_px`/`flux` 口径分叉的温床（三套 flux 定义并存，已造成 σ 高估 1.914005×）。复用是负责人令的一部分，属架构级约束。
- **建议条文要点**：① 一次检测 → 一次通量积分 → 测光/PSF/SNR 三处共用同一 `star_id` 绑定行；② 全链只有一个 flux 口径（PSF 拟合域 `flux = 2πA·sxsy/3`，单位 ADU）；③ 禁止任何模块另起盒和/振幅口径（除显式声明的诊断量）；④ `fwhm_px` 的轮廓定义（Gaussian FWHM=2.3548σ vs Moffat4 FWHM=1.230310σ）必须在生产者处声明并全链一致，禁止跨块反解。
- **依据**：GAP_AUDIT §9.49 定案 1 末段（:1485）；§9.43 A6 案例（:1220-1234）；CONFORM-SWEEP-1-001/004/024；CONFORM-SWEEP-2 PH-01/PH-02/PH-03
- **需变更 claim**：**是**　|　**优先级**：**P1**　|　**状态**：① 已裁决待落地

### DG-A-10 加性移除的理论依据 + 合成实验结论（归一化吃掉乘性；残余归空间增益）

- **建议增补到**：`ASTROCS_DESIGN.md` §4.4:266-272（或 §4.1:226-228）
- **现状**：§4.4:272 只写天光面的稀疏表示与「拟合目标为 SNR 加权下对各采样点的最小 RMS」；**无物理依据、无实验证据、无证据文件指向**。
- **为什么必须进最高设计**：§4.4 现在只描述天光面的工程表示，没有任何物理依据与实验证据。负责人明确要求该理论「进论文」，最高设计是它的锚点。
- **建议条文要点**：① 理论：Phase1 正确归一化后，帧 = 同一测光体系的真信号 + 可等效为加性的天光；残留天光无论原始是加性还是乘性，都可用加法移除（乘性残留 = 低阶空间增益，已在 Phase1 处理）；② 实验（合成已知真值）：世界(i) 纯加性（真值 α=0）⇒ 全局加性 −0.00076、HEALPix w64/128/256 = +0.00008/+0.00009/+0.00010（完美工作）；世界(ii) 乘性（中位 a−1≈+0.15，另有 ±10% 空间变化）⇒ 全局加性 +0.15456、HEALPix +0.19667~+0.21435，估计标量 a 后归一化 ⇒ 残余降到 +0.04473；③ 残余 0.045 归 Phase1 空间增益 m(x,y)（A2 裁决）；④ 证据指向 tests/validation/release02/q3_additive_truth/logs/step6_synth.txt。
- **依据**：GAP_AUDIT §9.49 定案 5（:1499-1505）；tests/validation/release02/q3_additive_truth/logs/step6_synth.txt（原文数值）
- **需变更 claim**：**是**　|　**优先级**：**P1**　|　**状态**：① 已裁决待落地

### DG-A-11 UPM 公共面构造 = (c) 排除自身 + 阻尼 + 拟合/叠加权重自洽 + 末端扣残差场；gauge 平移不改接缝

- **建议增补到**：`ASTROCS_DESIGN.md` §4.4:266-272
- **现状**：§4.4:272 只有「拟合目标为 SNR 加权下对各采样点的最小 RMS」一句，**无 formulation 条款**（无 exclude-self、无阻尼、无权重自洽、无末端扣残差场、无 gauge 说明）；§4.2:236 的模型式还是乘性。
- **为什么必须进最高设计**：现条文只写「拟合目标为 SNR 加权下对各采样点的最小 RMS」——已被代数与数值双重证明不足以消接缝（参考帧 gauge 不保证 g_k=ḡ 公共；naive Gauss-Seidel α=1 在链式/二部覆盖图上特征值 −1 ⇒ 周期 2 振荡）。这是 Phase2 核心科学行为的定义。
- **建议条文要点**：① ref(p) 用其他帧的加权组合（排除自身）；② 阻尼 α≈0.5（散差 11→0.13）；③ 拟合权重与叠加权重同源（解耦会让阶跃重现 0.4285）；④ 末端用叠加权重本身算残差场 R=Σw(z−c)/W 并从每帧扣除 ⇒ Σw_k g_k/W ≡ 0，任意覆盖子集/任意权重面阶跃恒为 0.0000；⑤ 明确登记：gauge 从「参考帧 δ_ref≡0」改为「全局加权均值为零」只平移一个全局常数、不改变空间阶跃（接缝对 gauge 严格不变），不得把 gauge 改动宣称为接缝修复；⑥ 拟合目标是否加堆叠平滑项属待裁决（D-03）。
- **依据**：GAP_AUDIT §9.28 Q1/Q2（:718-742）；§9.30（:796-847，数值：无噪/含噪 (a)0.209/0.260、(b)0.209/0.260、(c)0.035/0.060、(d)4.91/4.93）；§9.33（:911-919）
- **需变更 claim**：**是**　|　**优先级**：**P0**　|　**状态**：① 已裁决待落地（⑥ 待裁决）

### DG-A-12 叠加权重 = 残差制造者方差 PΣPᵀ（不是 σ²+Var(ĝ)）；拟合权重 ≠ 堆叠权重

- **建议增补到**：`ASTROCS_DESIGN.md` §4.3:257-258
- **现状**：§4.3:257-258 写「w = 1/σ² = SNR²/F_ref² ∝ SNR²」，**未区分拟合权重与堆叠权重**，未要求含梯度拟合参数协方差，未写「过渡期不得声称逆方差加权」。
- **为什么必须进最高设计**：§4.3 现在只写 w=1/σ²=SNR²/F_ref² ∝ SNR²，未区分拟合权重与堆叠权重，也未要求含梯度拟合参数协方差；朴素式 σ²+Var(ĝ) 会漏交叉项 −2ΣHᵀ（N=8 时高估 1.29×）。
- **建议条文要点**：① 拟合权重 w_fit=1/σ²(+Huber) 只用于拟合；② 堆叠权重 w_stack=1/Var(corrected) 必须用残差形式 PΣPᵀ（对角 Var(c_i(p))=Σ_j P_{i(p),j}²σ_j²）并含梯度拟合参数协方差；③ 不得把拟合权重当堆叠权重；不得无条件加 Var(ĝ)；④ 不动点下 c_i≡s，所有帧共享 Var(s) ——「参考帧」不是低噪声锚点；⑤ 过渡期 uncertainty_available=false 时不得声称逆方差加权。
- **依据**：GAP_AUDIT §9.28 Q3（:732-737）；§9.30 ⑤（:832-837）；§9.23（:585-635，M1/M2/M3）；change-claim §4.2
- **需变更 claim**：**是**　|　**优先级**：**P1**　|　**状态**：① 已裁决待落地

### DG-A-13 帧级 SNR 落盘完整性：σ_sky 局部估计 / m_5 真产出 / A_NEA 落盘

- **建议增补到**：`ASTROCS_DESIGN.md` §3.4:165-169
- **现状**：§3.4:165-169 无 σ_sky 局部化要求、无 m_5 产出要求、无 A_NEA 落盘要求、无「σ_sky 已含读出噪声则 (σ_R/g)² 置零」的防重复计入条款。
- **为什么必须进最高设计**：这三项决定帧级 SNR 是否可比、可追溯：现生产 m_5 为 0/71 帧非空、A_NEA 不落盘、σ_sky 是整帧 MAD（星云场同夜同仪器 38.22 vs 20.74，差 1.8×）。
- **建议条文要点**：① σ_sky 必须局部估计（不是整帧标量）；② m_5 若被声称为帧级科学基准则必须真的产出（需 ZP），否则不得宣称；③ A_NEA 必须落盘（帧间比较 SNR 须能追溯 PSF 形状）；④ 若 σ_sky 已含读出噪声，则 (σ_R/g)² 项必须置零以免重复计入。
- **依据**：reverse_verify/docs/frame-snr-canon.md §4.2 G1/G2/G7（:447/:448/:454）、§4.3 C4（:476）、§2.3（:130-136）
- **需变更 claim**：否　|　**优先级**：**P2**　|　**状态**：③ 审计发现但未定案（严重度已登记，实现缺口）

### DG-A-14 Phase1 噪声 σ 必须来自掩膜 patch（源污染红线）

- **建议增补到**：`ASTROCS_DESIGN.md` §3.4:165-169 / §3.6
- **现状**：§3.6:218-220 只写「校准方差传播、不裁切负值、共享 master 相关性、检测阈值、标量压缩门、信息权重等」见插件文档；**无 σ 来源（掩膜 patch）条款**。
- **为什么必须进最高设计**：规范（SCI-NOISE-001 §5/§5a）早已写对、实现没跑（合规实现因 CMakeLists.txt:240 被注释不在构建图）。把 σ 来源提升到最高设计可防止再次被整帧 MAD 替代。
- **建议条文要点**：σ_bg 必须来自 8×8 patch + 逐星掩膜 + 饱和过滤 + 天空预算门；禁止用整帧未裁剪 MAD 代替；同一产品内只允许一个 σ 口径（实测 σ_MAD/σ_clippedRMS = 1.3127）。
- **依据**：CONFORM-SWEEP-2 NS-01（:205）、NS-07（:289）；reverse_verify/docs/snr-propagation-design.md P1-1..P1-3（:1156-1158）；GAP_AUDIT §9.43（:1230-1232，源污染偏差）
- **需变更 claim**：**是**　|　**优先级**：**P1**　|　**状态**：① 已裁决待落地（规范对，修符合性）

### DG-A-15 WCS 绝对零点与 samples 原点一致性必须进验收（SDI-WCS-ZERO-POINT）

- **建议增补到**：`ASTROCS_DESIGN.md` §11.1:574（真实数据验证清单）
- **现状**：§11.1:574 写「真实数据（M42/银心）验证接缝、背景、星形、排异、黑洞、预测/实测噪声」——**无 WCS 绝对零点项、无 samples 原点一致性项、无 SIP 声明项**。
- **为什么必须进最高设计**：链内 WCS 在标准 FITS 口径下对 Gaia 有空间恒定 ≈(+0.26,+0.62) px 零点偏移（两帧复现、4×4 分区恒定、链内自检零鉴别力）；p1_wcs.json 的 samples[] 原点声明与实际差 1 px；samples 纯 TAN 不含 SIP（视场角上 SIP 达 ±17 px）。§11.1 的验收清单没有这一项，所以门看不见。
- **建议条文要点**：真实数据验证必须含：① WCS 对 Gaia 的绝对零点残差及其空间恒定性（分区检查）；② p1_wcs.json samples[] 原点声明与实际一致；③ SIP/畸变项的有无须显式声明并与 WCS 残差量级相容。容差与判据见 SDI-WCS-ZERO-POINT 调查项。
- **依据**：GAP_AUDIT §9.48 旁支新发现（:1420-1423）；§9.49 负责人第 8 条（:1477）；reports/RELEASE-02/stardet-conform-invest.md §3.1/§3.2（:302-320）
- **需变更 claim**：**是**　|　**优先级**：**P1**　|　**状态**：① 已裁决待落地（负责人「应该仔细检查」已立项 SDI-WCS-ZERO-POINT）

### DG-A-16 稀疏 SNR 层的数据面语义（value_semantics / support_scale_px）

- **建议增补到**：`ASTROCS_DESIGN.md` §3.4:174-177
- **现状**：§3.4:174-177 只写「稀疏控制点层作为标准层插入 HiPS」「实际 SNR = 帧级 × 帧内」；**无 value_semantics、无 support_scale_px、无 rho_c 相对场定义、无重建算子须返回预测方差的要求**。
- **为什么必须进最高设计**：合同 schema 只写 control_points[{x,y,sparse_snr_value}]，未冻结数值语义与支撑尺度 ⇒ 重建算子会把点值误当点采样。负责人已给出表示：稀疏 = 帧级平均 SNR × 附近稀疏控制点权重（线性插值），即 SNR(p)=SNR_frame×rho_c(p)、rho_c=SNR_c/SNR_frame（p50=1，无量纲）—— 这正是必须进最高设计的表示定义。
- **建议条文要点**：① 稀疏层存无量纲相对场 rho_c（p50=1）而非绝对 SNR；② 实际 SNR = SNR_frame × rho_c(p)，线性插值足够（ℓ=40.4–57.9 px、Δ=64 px 近临界采样）；③ support_scale_px 与 value_semantics 必须冻结在合同；④ 重建算子必须返回预测方差（否则无法进权重分母）。
- **依据**：GAP_AUDIT §9.46（:1357-1358）；reverse_verify/docs/snr-propagation-design.md §3.3/§3.7 X-1/X-2（:478-510、:1191-1192）
- **需变更 claim**：**是**　|　**优先级**：**P1**　|　**状态**：① 已裁决待落地 + 合同变更

---

## B 需完善的系列文档

### B-I 主线文档（16 条，非审计条目）

> 逐条给：文件:行 / 现状 / 应改成什么 / 依据 / 是否需变更 claim。

#### DG-B-01　`docs/plugins/algorithms_phase1/07_noise_snr.md` :112（默认列）

- **现状**：`| `sparse_snr_layer` | **true** | —— | ...`：默认列写 Markdown 加粗 `**true**`，与其余 94 行书写形态不一致。
- **应改成什么**：默认列改为裸 `true`（说明列可保留加粗强调）。
- **依据**：reports/RELEASE-02/snr-config-land.md §5.2 ①（:100）、§7.2（:165）；config/config_registry.json:663-673
- **需变更 claim**：否　|　**优先级**：**P2**　|　**状态**：① 已裁决待落地（SNR-CONFIG-LAND 给逐字订正建议）

#### DG-B-02　`docs/plugins/algorithms_phase1/08_drizzle.md` :46（默认列）

- **现状**：`| `sparse_snr_layer` | **true** | —— | 是否将稀疏帧内 SNR 层插入 HiPS（来自 noise_snr）；**本期决议默认产出** ...`：默认列同样加粗。
- **应改成什么**：默认列改为裸 `true`。
- **依据**：reports/RELEASE-02/snr-config-land.md §5.2 ②（:101）；config/config_registry.json:768/:770
- **需变更 claim**：否　|　**优先级**：**P2**　|　**状态**：① 已裁决待落地

#### DG-B-03　`docs/plugins/algorithms_phase1/07_noise_snr.md` :114（字段列）

- **现状**：字段列写 `` `snr_path`（Phase2 消费面键，在 mosaic 配置） ``：把括注写进字段名列。
- **应改成什么**：字段列只留 `snr_path`，括注移入说明列。
- **依据**：reports/RELEASE-02/snr-config-land.md §5.2 ③（:102）、§7.2（:165）；config/config_registry.json:687-702
- **需变更 claim**：否　|　**优先级**：**P2**　|　**状态**：① 已裁决待落地

#### DG-B-04　`docs/plugins/algorithms_phase1/07_noise_snr.md` :113（`sparse_snr_density` 行）、:130（§7 联锁条）

- **现状**：:113 单位写「点/度²」且标「仍为 pending_authority，禁止编造数值」；:130 写「`sparse_snr_density` 未定案 ⇒ 稀疏层不可生产（联锁缺口登记；不得编造数值）」。**但 §9.45 已定案 Δ = 64 px（像素域），由新键 `sparse_snr_spacing_px` 承载**（config/schema/登记册已落地）。
- **应改成什么**：① 注明 §9.45 已定案：稀疏层控制点间隔 Δ = 64 px（像素域），由 `sparse_snr_spacing_px` 承载；② 「点/度²」表述与 R-001 收口一并订正；③ :130 的「不可生产」改为「配置侧已定案；剩余联锁 = 实现未落地（见 §5.3）」，否则文档仍宣称不可生产。
- **依据**：GAP_AUDIT §9.45（:1299-1324）；reports/RELEASE-02/snr-config-land.md §5.2 ④（:103）；config/defaults.json:560/:562-572
- **需变更 claim**：否　|　**优先级**：**P1**　|　**状态**：① 已裁决待落地

#### DG-B-05　`docs/plugins/algorithms_phase1/07_noise_snr.md` §5 配置表（:106-114，表尾）

- **现状**：§5 配置表无 `sparse_snr_spacing_px` 行；该 phase_config 键现无插件文档行（仅由 defaults + schema + SNR-CONFIG-LAND 报告承载）⇒ CFG002-01 无法登记。
- **应改成什么**：补一行：`sparse_snr_spacing_px` | 64 | px | 稀疏层控制点间隔 Δ；复用 Phase2 UPM 8×8/tile 控制网格；Δ/ℓ=1.33（ℓ=40.4–57.9 px）。
- **依据**：reports/RELEASE-02/snr-config-land.md §5.2 ⑤（:104）、§4.2（:77）；contracts/schemas/phase_config_normalize.schema.json:127-132
- **需变更 claim**：否　|　**优先级**：**P1**　|　**状态**：① 已裁决待落地

#### DG-B-06　`docs/contracts/CONFIG_CONTRACT.md` §2:30（50 字段快照）、§2:53、§2:55-61（pending 表）、§3:74-75（可选算法选择列）

- **现状**：§2 快照仍写「`config/defaults.json`（50 字段，2026-09-17 快照）」；pending 表只列 3 项（`sparse_snr`/`scalar_gate`）；§3 normalize 列无 `sparse_snr_spacing_px`、mosaic 列无 `snr_path`；§2 的 `sparse_snr.density` 行锚仍指 `07_noise_snr.md:65`（现为空行）。**实际 `config/defaults.json` 的 `field_count` 已由 SNR-CONFIG-LAND 改为 53。**
- **应改成什么**：① §2 快照更新为 `field_count=53` 并补 2 键（`snr.path`、`sparse_snr.spacing_px`，状态 owner_adjudicated 而非 pending）；② §3 normalize 列补 `sparse_snr_spacing_px`、mosaic 列补 `snr_path`；③ `sparse_snr.density` 行锚由 `:65` 重锚到 `:113`。
- **依据**：reports/RELEASE-02/snr-config-land.md §5.2 ⑥（:105）、§6.3（:158-160）；config/defaults.json:11（field_count 51→53）
- **需变更 claim**：否　|　**优先级**：**P1**　|　**状态**：① 已裁决待落地

#### DG-B-07　`docs/plugins/algorithms_phase2/13_integration.md` :18-22（§3 SNR 路径条）、:37（§4.0 路径条）

- **现状**：只写「配置文件 JSON 显式指定」与 `snr_path_effective`，**未写键名** `snr_path`（mosaic 配置）。
- **应改成什么**：与 `07_noise_snr.md:114` 对齐，写明键名 `snr_path`（mosaic 配置）、取值域 {dense, sparse_reconstruct, frame_reconstruct}、默认 `sparse_reconstruct`。
- **依据**：reports/RELEASE-02/snr-config-land.md §5.2 ⑦（:106）、§3（:61-69）；contracts/schemas/phase_config_mosaic.schema.json:150-158
- **需变更 claim**：否　|　**优先级**：**P1**　|　**状态**：① 已裁决待落地

#### DG-B-08　`工程控制/RELEASE-02/change-claims/FIX-SCI-SNR-CANON-001.md` §3.2:89（配置键设计）、§4.1:165（影响面表）

- **现状**：claim 正文写 Phase2 键名为 `algorithm_snr_path`；实际落地与三篇权威（`ASTROCS_DESIGN.md` §3.3:124、`UNIFIED_MODEL.md:46`、`07_noise_snr.md:114`）以及负责人 §9.46 用语均为 `snr_path`。
- **应改成什么**：**补一行命名订正登记**：`algorithm_snr_path` → `snr_path`（依据 §3.3:124 与 §9.46），语义（三路径/默认稀疏/JSON 显式指定）逐字保留。不改裁决语义、不改已落地的配置/schema。
- **依据**：reports/RELEASE-02/snr-config-land.md §3（:64-69）、§5.2 ⑧（:107）
- **需变更 claim**：**是**　|　**优先级**：**P1**　|　**状态**：① 已裁决待落地（claim 侧补登记）

#### DG-B-09　`docs/science/PHASE2_UPM.md` §14a（:181-182 之后）

- **现状**：§14a 已写「本期决议：纯加性」与负责人理论依据，但**未写合成实验证据**（§9.49 定案 5 的数字与证据文件指向）。
- **应改成什么**：在 §14a 补「加性移除的实验确认」段：世界(i) 纯加性 −0.00076 / HEALPix +0.00008~+0.00010；世界(ii) 乘性 +0.15456 → 估标量归一化后 +0.04473；残余归 Phase1 空间增益 m(x,y)；证据 tests/validation/release02/q3_additive_truth/logs/step6_synth.txt。
- **依据**：GAP_AUDIT §9.49 定案 5（:1499-1505）；tests/validation/release02/q3_additive_truth/logs/step6_synth.txt
- **需变更 claim**：**是**　|　**优先级**：**P1**　|　**状态**：① 已裁决待落地

#### DG-B-10　`docs/plugins/algorithms_phase2/11_upm.md` §4.4（:62 附近「加权残差 RMS 最小」）、目标函数/gauge 段（:27-33、:53、:60）

- **现状**：目标仍写「最优解即对各采样点的加权残差 RMS 最小的天光面」与「参考帧 gauge」。Q2 已证：该目标不足以消接缝（参考帧 gauge 不保证 g_k=ḡ 公共），正确 formulation 是 (c) 排除自身 + 阻尼 α≈0.5 + 拟合/叠加权重自洽 + 末端扣残差场；gauge 平移只改全局常数、不改空间阶跃。
- **应改成什么**：目标函数改为「加权叠加平滑」目标（Σw_k g_k/W ≡ 0），写明 (c) + 阻尼 + 权重自洽 + 末端扣残差场；gauge 段明确「改 gauge 不修接缝」。**属科学行为变更，须先由负责人裁决**（见 D-03）。
- **依据**：GAP_AUDIT §9.28 Q1/Q2（:718-742）；§9.30（:796-847）；§9.33（:911-919）；§9.21（:505-527 旧设想文档已被否决，不得照搬）
- **需变更 claim**：**是**　|　**优先级**：**P0**　|　**状态**：② 待负责人裁决（科学行为变更；选项与后果见 D-03）

#### DG-B-11　`docs/plugins/algorithms_phase1/03_star_detection.md` §1:5（职责）、§4（算法与公式要点，:31-34）、§5（配置项）

- **现状**：职责写「检测图像中的源（星点/延展源）」= 盲检测范式；§4 写「检测阈值 = median(img)+5.0·bgnoise ... 实现 sdet_api.cpp:1782-1792」。而 §9.49 定案 1 已把检测改为星表引导，且 §9.48 ② 确认生产检测器 ≠ 规范权威源（`ALG-STARDET-001:8` 点名 sdet_api.cpp 为唯一权威源，生产跑的是 wrapper_phase1::StarDetector，345,960 vs 1,473 条）。
- **应改成什么**：① 职责改为「星表引导的源拟合（Gaia 投影位置为定义域）」；② 补 §9.49 定案 1 的六条（投影/只拟合星表位置/失败丢弃/top 2–5 万/极限星等截断/不得物理闭合）；③ 消解权威源冲突：明确生产权威实现（sdet_api.cpp 或 wrapper）并同步 `docs/algorithms/STAR_DETECTION_ALGORITHMS.md`、`docs/science/STAR_DETECTION.md`、`docs/validation/SCIENCE_FREEZE.md`；④ 行号锚重刷（S1-022）。
- **依据**：GAP_AUDIT §9.49 定案 1（:1480-1485）；§9.48 ②（:1404-1418）；CONFORM-SWEEP-1-002（:64-76）；reports/RELEASE-02/stardet-conform-invest.md §1（:24-197）
- **需变更 claim**：**是**　|　**优先级**：**P0**　|　**状态**：① 已裁决待落地（**订正（2026-09-20，V5 分片 3）**：原「② 待负责人裁决（是否切 sdet：7–12 人日 + 全链重验收，见 D-04）」作废 —— §9.49 定案 1 已定检测范式 = **星表引导**；`sdet` 去留 = 前台按星表引导后实测评估（§9.50 授权），不再是负责人待决项）

#### DG-B-12　`docs/plugins/algorithms_phase1/06_photometry.md` §1:6（边界，D-5 已订正）、§4（算法要点，:25-28）、§5 配置表（:30-34）

- **现状**：§4 只写「孔径测光 / PSF 测光 / 光度响应 a_k」；**无空间增益 m(x,y)**、无 F_instr 口径定案、无「归一化必须落到像素」的应用点与元数据要求。§5 配置表 `mode` 默认 `psf`、`aperture_radius` 默认 `2×FWHM`（与实现/README 冲突，见 S2-PH-05/PH-06）。
- **应改成什么**：① 补 `I_photo = k_photo·m(x,y)·I_cal` 的低阶空间增益（用星点估、低阶双线性/二次）；② 补 F_instr 口径（A6 定案后转录；F-INSTR-CONFORM-FIX 已实施 PSF 拟合域 `2πA·sxsy/3`，残留：噪声节点仍取盒和）；③ 补「归一化应用点 + photappl/photscal 如实落盘」；④ 消解 `mode`/`aperture_radius` 默认冲突（见 D-05）。
- **依据**：GAP_AUDIT §9.29（:753-795）；§9.37 指令 1（:1003-1007）；§9.39 A4/A6（:1072-1098）；reports/RELEASE-02/f-instr-conform-fix.md §1（:24-45）、§4.4（:166-175）
- **需变更 claim**：**是**　|　**优先级**：**P0**　|　**状态**：① 已裁决待落地（**订正（2026-09-20，V5 分片 3）**：原文「A4/A6 定案后转录」作废 —— A6 已定案 = PSF 拟合域 `2πA·sxsy/3`（§9.51 Q-C / §9.52 / §9.67 定案 6），A4 开启前置条件仍待实验满足）

#### DG-B-13　`docs/science/CONTROL_WEIGHT_SNR.md` :33（frame_snr provenance）、:36（sigma_F「当前实现不产出」）、§2a（「唯一帧级科学基准是 m_5」）

- **现状**：:36 写 σ_F「当前实现不产出」——**与实测不符**：生产已产出 `sigma_f_optimal_adu`（snr_science.cpp:181）与 `snr_reference.sigma_f_adu`，且 `snr_f == F_ref/sigma_f` 在 69/69 帧成立。:33 把 frame_snr 的 provenance 写成「整帧 Phase1 SNR 目录值的中位数」——实际写入 HiPS 的是 `snr_reference.snr_f`（参考源 SNR）。§2a 称 m_5 是唯一帧级科学基准，而 m_5 **0/71 帧非空**。
- **应改成什么**：① :36 改为「当前实现已产出（snr_science.cpp:181；p1_snr.json 的 snr_reference.sigma_f_adu）；但未落盘到独立字段供 Phase2 消费，且不含协方差/a_k」；② :33 改为「来源 = p1_snr.json 的 snr_reference.snr_f（参考通量 F_ref 上的 PSF SNR），不是 median_source_snr」；③ §2a 改为「帧级科学量有二：帧级 SNR（已产出）与 m_5（需 ZP，当前未产出，属实现缺口 G2）」。
- **依据**：reverse_verify/docs/frame-snr-canon.md §5（:505-518，逐字建议）、§4.3 C1（:473）；docs/science/CONTROL_WEIGHT_SNR.md:33/:36
- **需变更 claim**：否　|　**优先级**：**P1**　|　**状态**：① 已裁决待落地（FRAME-SNR-CANON 给逐字订正）

#### DG-B-14　`docs/plugins/algorithms_phase1/07_noise_snr.md` §4.1（数学定义，:39 附近）

- **现状**：§4.1 写 `W_psf,k = a_k² P_kᵀ C_k⁻¹ P_k` 但**未标实现状态**（实现是对角白噪声近似 `1/ΣP_i²/σ_i²`，无 a_k、无 C）；未声明 `F_ref` 的单位与参考轮廓；未声明源自身泊松的取舍；未要求落盘 `A_NEA`；未写帧级 SNR 定案式（DG-A-04）。
- **应改成什么**：① 加实现状态行：`W_psf` 是目标规范；当前实现为 `W_psf ≈ 1/σ_F²`、`σ_F² = ΣP_i²/σ_i²`（对角、白噪声），a_k 与协方差 C 未接入（附 file:line）；② 补 `F_ref` [ADU]、参考轮廓 = `median_fwhm_of_catalogue_sky_limited`、组内公共（`reference_flux_scope=group`）；③ 显式声明源泊松取舍：生产 gain 未知 ⇒ 源泊松项未计入（snr_science.cpp:176-177）⇒ 亮源端 σ_F 偏低、SNR 偏高（已知偏差登记）；④ 补 `a_nea_px` 落盘要求（sum_p2 已由 C ABI 返回）；⑤ 转录 DG-A-04 的定案式。
- **依据**：reverse_verify/docs/frame-snr-canon.md §5（:509-518）、§4.2 G3/G4/G5/G7（:449-454）
- **需变更 claim**：否　|　**优先级**：**P1**　|　**状态**：① 已裁决待落地

#### DG-B-15　`docs/design/UNIFIED_MODEL.md` :42（frame_snr 行）、:56（SNR 产物边界，已订正）

- **现状**：:42 已符合 D-4 红线，但缺「可复算判据」的指向；另 `p1_snr.json` 里名为 `frame_snr` 的字段实为 **5σ 深度容器**（31/32 产品自文档化 NOT a whole-frame scalar SNR），与 :42 的 `frame_snr` 科学量**同名不同物**。
- **应改成什么**：① :42 补一句「红线判据与数值见 reverse_verify/docs/frame-snr-canon.md §3；B→∞ ⇒ SNR→0 的解析断言见 §3.1」；② 给 `p1_snr.json` 的深度字段改名 `depth`（或加 `not_a_snr: true` 注解），把 `frame_snr` 留给 `snr_reference.snr_f`（属合同变更，见 D-06）。
- **依据**：reverse_verify/docs/frame-snr-canon.md §5（:511、:515）、§4.2 G10（:457）、§4.3 C5（:477）
- **需变更 claim**：**是**　|　**优先级**：**P2**　|　**状态**：① 已裁决待落地 + ② 改名属合同变更（D-06）

#### DG-B-16　`docs/development/CONFIG_SCHEMA.md` :3-4（工具自述）、:19（smoothing 默认）、配置行表

- **现状**：:3-4 自述「一致性由 tools/config_consistency_check.py 校验（V14 交付）」——该工具在 S3-017 时已失效（引用 4 条不存在路径）；:19 写 `smoothing auto→0.1`，与 ALG/模块「0.0 默认关」打架（S3-023）；缺 `snr_path`/`sparse_snr_spacing_px` 行。
- **应改成什么**：① 更新工具自述（路径与覆盖面；工具已由 GUARD-TOOLS-FIX 重写，见 DG-C-04）；② `smoothing` 默认按 §9.39 A5 裁决订正为「不得为 0」并标注取值待实验定（D-07）；③ 补 `snr_path`（mosaic）与 `sparse_snr_spacing_px`（normalize）两行。
- **依据**：CONFORM-SWEEP-3-017（:228）；CONFORM-SWEEP-3-023（:289）；reports/RELEASE-02/snr-config-land.md §5.2（:105）；GAP_AUDIT §9.39 A5（:1080-1090）
- **需变更 claim**：否　|　**优先级**：**P1**　|　**状态**：① 已裁决待落地（工具面已在办；默认值面见 D-07）

### B-II 四份符合性审计报告列出的规范侧问题

> 报告口径：**S1 7 条 + S2 13 条 + S3 9 条 + S4 5 条 = 34 条**（与任务描述一致）。
> 本台账把每条**路由到具体文档与行**，并标注状态。另有 **S3 §3 表内 021**（报告自身 9 vs 10 行口径不一致）与 **S4 的 9 条含规范侧成分但未计入「另一类」5 条**的条目，一并登记（如实标注「未计入其分列」），不混入 34 条统计。

#### B-II-1 CONFORM-SWEEP-1（7 条：018–024）

#### DG-B-S1-018　`docs/algorithms/STAR_PSF_ALGORITHMS.md` :72、:144（§3 伪代码与 §11.1 初值链）

- **现状**：§3 写 Moffat4 初值 `sx=sy=1.2`；§11.1 初值链只给符号 `sx0` 不给数值；实现 dpsf_psf.cpp:368 为 `sx0=0.15*rw`（rw≈17 ⇒ ≈2.55）；§11.1 只登记 LM 参数出入、未登记 sx0；`0.15*rw` 在 docs/ 全库无出处。
- **应改成什么**：订正 §3 并登记 `sx0=0.15*rw`，或把实现改为规范值；二者取一后在 §11.1 同步。
- **依据**：CONFORM-SWEEP-1-018（md:235-243）
- **需变更 claim**：否　|　**优先级**：**P1**　|　**状态**：③ 审计发现但未定案（规范侧，报告标「只登记，不改」）

#### DG-B-S1-019　`docs/algorithms/STAR_PSF_ALGORITHMS.md` :91（§4 表）、:160-163（§11.1 自陈）

- **现状**：§4 写「图像含 NaN/Inf ⇒ 该 patch 跳过拟合，status=BAD」；实际状态码只有 OK/NO_CONVERGENCE/INVALID_PARAMS/ITERATION_LIMIT（dynamic_psf.h:33-36），**无 BAD**；实现是逐像素跳过非有限像素（dpsf_psf.cpp:281-304）而非整 patch 跳过。§11.1 已自陈矛盾但 §4 正文未订正。
- **应改成什么**：订正 :91：状态码不存在 BAD；NaN/Inf 语义 = 逐像素跳过（计数 + LOG_WARN），全部非有限才 INVALID_PARAMS。
- **依据**：CONFORM-SWEEP-1-019（md:245-252）
- **需变更 claim**：否　|　**优先级**：**P1**　|　**状态**：③ 审计发现但未定案

#### DG-B-S1-020　`docs/algorithms/STAR_DETECTION_ALGORITHMS.md` :112-114（reject_star 条款）

- **现状**：规范写「reject_star(fit) ≠ OK ⇒ drop（FWHM>0.5、圆度≥0.5、RMSE=mad·1.482602218505602/A≤0.2、**FWHM 上限**；饱和豁免 RMSE）」，**未给 FWHM 上限公式**；实现 sdet_api.cpp:238-245 用 `fwhm_limit = se_smax*2.3548200450309493*(1+0.5*log(se_smax/2.0))`，`grep docs/` 零命中，三常数与对数放宽形态均无出处。
- **应改成什么**：在 :112-114 或 docs/algorithms/GATES_AND_TOLERANCES.md 补公式与出处；或把 sdet_api.cpp:243 改为规范值。
- **依据**：CONFORM-SWEEP-1-020（md:254-261）
- **需变更 claim**：否　|　**优先级**：**P1**　|　**状态**：③ 审计发现但未定案

#### DG-B-S1-021　`docs/algorithms/COSMETIC_ALGORITHMS.md` :4-13（:8-9「尚未存在生产符号」、:13「禁止声明 IMPLEMENTED」）、:20-31、:226-228

- **现状**：文档称迁移目标目录 `lib/algorithms/cosmetic/`「尚未存在生产符号」「禁止声明 IMPLEMENTED」；实际已落码 lib/algorithms/cosmetic/src/module_entry.cpp（1262 行）、CMakeLists.txt:223-229 编入、packaging/astrocs.product.json:15 status=IMPLEMENTED、build/install_manifest.txt:9 已安装。
- **应改成什么**：订正上述三处：模块已落码并入包标 IMPLEMENTED；同时如实登记「双实现并存 + 迁移模块未接线」（见 CONFORM-SWEEP-1-010）。
- **依据**：CONFORM-SWEEP-1-021（md:263-270）
- **需变更 claim**：否　|　**优先级**：**P2**　|　**状态**：③ 审计发现但未定案

#### DG-B-S1-022　`docs/algorithms/STAR_DETECTION_ALGORITHMS.md` :40（norm 硬编码 65535 锚）、:201（reject_star 锚）

- **现状**：:201 写 `reject_star | :189-239`，实测 sdet_api.cpp:209-247；:40 写 `65535（:1689）`，实测 65535 出现在 :1834 与 :2203，**:1689 无 65535**。文档头自陈「2555 行实测，2026-09-17 LEDGER-DOC 按附录 G 机械重锚」，但 §2 行内锚未随之刷新。
- **应改成什么**：刷新 :40/:201 等行内锚（并入 DG-C-02 的机械重锚机制）。
- **依据**：CONFORM-SWEEP-1-022（md:272-279）
- **需变更 claim**：否　|　**优先级**：**P2**　|　**状态**：③ 审计发现但未定案

#### DG-B-S1-023　`docs/algorithms/STAR_PSF_ALGORITHMS.md` :73（§3 LM 参数）、:145（§11.1）、:158-160（自陈）

- **现状**：§3 写 LM `iter≤50 tol=1e-6`；§11.1 写 `tol=1e-8 / max_iter=200`（硬编码）；实现 dpsf_psf.cpp:374-375 为 `1e-8, 200`。§11.1 自陈「§3 为旧稿」但 §3 正文未订正 ⇒ 同一文档两套冻结值。
- **应改成什么**：订正 :73 为 `tol=1e-8 / max_iter=200`（与实现一致），保留 §11.1 的出处说明。
- **依据**：CONFORM-SWEEP-1-023（md:281-289）
- **需变更 claim**：否　|　**优先级**：**P1**　|　**状态**：③ 审计发现但未定案

#### DG-B-S1-024　`docs/contracts/DATA_SEMANTICS.md` :753（flux 列语义）、另 docs/science/PSF.md:52/:87、docs/algorithms/STAR_DETECTION_ALGORITHMS.md:20

- **现状**：三套 `flux` 定义并存：DATA_SEMANTICS:753 = 检测侧椭圆高斯峰值振幅 A；PSF.md:52/:87 = `2πA·sxsy/3`（ADU）；ALG:20 未定义。实现三处三义：sdet_api.cpp:2304（振幅）、wrapper_phase1/star_detector.cpp:151（5×5 盒和）、dpsf_psf.cpp:429（解析通量）。DATA_SEMANTICS **无 DATA-P1-SOURCES 列语义表**。
- **应改成什么**：① 新增 DATA-P1-SOURCES 列语义表（冻结生产交付链的 flux 口径）；② ALG:20 补 flux 定义；③ 与 DG-A-09「单一通量」联动。
- **依据**：CONFORM-SWEEP-1-024（md:291-300）
- **需变更 claim**：**是**　|　**优先级**：**P1**　|　**状态**：③ 审计发现但未定案（合同面变更）

#### B-II-2 CONFORM-SWEEP-2（13 条 = 4 SPEC-ERR + 9 SPEC-CONFLICT/规范歧义）

> 报告口径（`CONFORM-SWEEP-2.md:711-713`）：SPEC-ERR 4 条（DZ-07、DZ-08、PH-11、SPEC-ERR-CAL）+ SPEC-CONFLICT/规范歧义 9 条（DATA-P1-FLUX 单位、`sparse_snr_layer`、`smoothing_lambda`、孔径、`mode`、方差低估、Phase3 variance BUNIT、FOV≤20°、面积单位）。
> 其中 **`sparse_snr_layer` → 已订正（B-III-02）**、**`smoothing_lambda` → 已裁决（B-III-03）**、**Phase3 variance BUNIT → 落在 S4（DG-B-S4X-AIO25 / P3X-08）**，故本节列 10 条 + 4 条报告正文带规范侧标签但未计入分列的条目。

#### DG-B-S2-DZ-07　`docs/science/DRIZZLE.md` :54（对照 :130/:191）

- **现状**：:54 写「`S_p(SB) = S_p(legacy)/pixfrac²`」，由 :53 的 `w_jp = pixfrac²·w_legacy` 应得 `S_SB = pixfrac²·S_legacy` ⇒ **订正方向写反**。
- **应改成什么**：:54 改为 `S_legacy = S_SB/pixfrac²`（即 SB 面亮度口径与 legacy 口径的换算方向纠正）。
- **依据**：CONFORM-SWEEP-2 DZ-07（md:419-426）、§4.2（md:753）
- **需变更 claim**：**是**　|　**优先级**：**P1**　|　**状态**：③ 审计发现但未定案（★报告标注需变更 claim）

#### DG-B-S2-DZ-08　`docs/science/DRIZZLE.md` lib/algorithms/drizzle/README.md:93、docs/algorithms/DRIZZLE_GEOMETRY.md:89、orchestrator.cpp:173/:191 注释、module_adapters.cpp:4119 注释

- **现状**：文档/注释写 HEALPix 尺度常数 ≈211034.6，代码表达式 `sqrt(π/3)·(180/π)·3600 = 211076.285`；代码无数值缺陷，仅文档/注释陈旧。
- **应改成什么**：文档与注释统一改 `211076.3`（并登记）。
- **依据**：CONFORM-SWEEP-2 DZ-08（md:427-434）、§4.2（md:753）
- **需变更 claim**：**是**　|　**优先级**：**P2**　|　**状态**：③ 审计发现但未定案（★报告标注需变更 claim）

#### DG-B-S2-PH-11　`lib/algorithms/photometry/README.md` :181；docs/algorithms/PHOTOMETRIC_FIT.md:209-213

- **现状**：两处称 Photometer「未接 orchestrator 管线」；实际已接生产调度节点 module_adapters.cpp:3050/:3099（经 astrocs_cli_runtime→astrocs_module_adapters，CMakeLists.txt:766-825/828-838）。
- **应改成什么**：两处补「生产调度节点已接线」并给 file:line。
- **依据**：CONFORM-SWEEP-2 PH-11（md:194-204）、§4 分列（md:711）
- **需变更 claim**：否　|　**优先级**：**P2**　|　**状态**：③ 审计发现但未定案

#### DG-B-S2-SPEC-ERR-CAL　`docs/algorithms/CALIBRATION_ALGORITHMS.md` :262、:465

- **现状**：文档称 `apply_photometry` 未编译未接线；实际 CMakeLists.txt:454 已编入 astrocs_calibration，且 module_adapters.cpp:3537-3538 有生产调用。
- **应改成什么**：订正 :262/:465 为「已编入并已接线」并给 file:line。
- **依据**：CONFORM-SWEEP-2 SPEC-ERR-CAL（md:648-656）、§4.2「被推翻的嫌疑」①（md:715）、§4 分列（md:711）
- **需变更 claim**：否　|　**优先级**：**P1**　|　**状态**：③ 审计发现但未定案（措辞须与 §9.29 严格区分）

#### DG-B-S2-UNIT　`docs/modules/registry/astrocs.phase1.noise-snr.md` :35；docs/modules/registry/astrocs.phase1.photometry.md:43（实现 module_adapters.cpp:829/:854）

- **现状**：同一端口 `DATA-P1-FLUX` 的单位两说：registry 一处写 ELECTRON、另一处写 ADU；实现写 ELECTRON；而 gain 不可得（PHOTOMETRY.md:30「e⁻ 需 gain 当前不可得」）。
- **应改成什么**：裁决并统一两处 registry 表（连同 CONFORM-SWEEP-2 PH-07 的实现侧单位）为 ADU（或按裁决结果）。
- **依据**：CONFORM-SWEEP-2 SPEC-CONFLICT-UNIT（md:652）、PH-07（md:151-161）、§4 分列（md:712）
- **需变更 claim**：**是**　|　**优先级**：**P1**　|　**状态**：③ 审计发现但未定案（需裁决单位）

#### DG-B-S2-PH-05　`docs/plugins/algorithms_phase1/06_photometry.md` :32（`aperture_radius` 默认 2×FWHM）；lib/algorithms/photometry/README.md:173-174（4.0 px）；实现 photometer.h:26-28 默认 4.0 px；module_adapters.cpp:3050 不读配置

- **现状**：插件规范写 `aperture_radius` 默认 `2×FWHM`，模块 README 与实现写 `4.0 px`；两条规范互相打架，且生产默认不可配。
- **应改成什么**：二者取一为权威（建议：规范改为显式像素值并说明与 FWHM 的关系），并把 `aperture_radius`/`sky_annulus` 入 config/defaults.json 并由 module_adapters.cpp:3050 读取。
- **依据**：CONFORM-SWEEP-2 PH-05（md:123-139）、§4 分列（md:713）
- **需变更 claim**：**是**　|　**优先级**：**P1**　|　**状态**：① 已裁决待落地（**订正（2026-09-20，V5 分片 3）**：原「② 待负责人裁决（口径定案，见 D-05）」作废 —— A6 口径 = PSF 拟合域 `2πA·sxsy/3`（§9.51 Q-C / §9.52）；**残留**：`aperture_radius` 默认值与 `mode` 键去留 = 前台按 §9.50 授权定案（需孔径无关性/seeing 鲁棒性证据，见 B 报告 U-3））

#### DG-B-S2-PH-06　`docs/plugins/algorithms_phase1/06_photometry.md` :31（`mode` 默认 `psf`）；docs/science/PHOTOMETRY.md:96；实现 module_adapters.cpp:3050-3121（只孔径，不读 `mode`）

- **现状**：插件默认 `mode=psf` 与 SCI 文档/实现（只孔径）冲突；`mode` 键在生产**无读者**。
- **应改成什么**：负责人裁决优先序（psf 还是 aperture 为生产默认），并让生产读 `mode` 或显式删除该键。
- **依据**：CONFORM-SWEEP-2 PH-06（md:140-150）、§4 分列（md:713）
- **需变更 claim**：**是**　|　**优先级**：**P1**　|　**状态**：① 已裁决待落地（**订正（2026-09-20，V5 分片 3）**：原「② 待负责人裁决（见 D-05）」作废 —— 口径 = PSF 域（§9.51/§9.52）；`mode` 键去留（生产读 `mode` 或显式删除该键）= 前台按 §9.50 授权定案）

#### DG-B-S2-PR-02　`docs/science/UNCERTAINTY_AND_COVARIANCE.md` :93（1+0.75ρ ⇒ 36.3%）vs docs/algorithms/v6/phase3/ALG-P3-001_SPEC.md:139、ALG-P3-001_VERIFICATION.md:55/:135、docs/contracts/v6/data/08_phase3.md:59（1+3ρ ⇒ 23.31%）

- **现状**：同一相关噪声方差增量在两处权威给出不同系数（36.3% vs 23.3%）；生产只算对角特例 Σc_k²u_k，无 C_in 相关项（实现侧条目 PR-02 判 C1 不符）。
- **应改成什么**：先由规范侧裁决权威表达式（1+0.75ρ 还是 1+3ρ），再决定 p3_resample.cpp:489-517 是增相关项还是显式输出相关核/近似误差。
- **依据**：CONFORM-SWEEP-2 PR-02（md:492-509）、§4 分列（md:713，主题「方差低估 36.3% vs 23.3%」）
- **需变更 claim**：**是**　|　**优先级**：**P1**　|　**状态**：③ 审计发现但未定案（规范侧数值冲突）

#### DG-B-S2-PR-04　`docs/science/PHASE3_HIPS_TO_FITS.md` :151（FOV≤20° 冻结限）；实现 p3_wcs.cpp:101-104、p3_session.cpp:106-115、p3_proj_v6.cpp:273/:395-396

- **现状**：冻结的 FOV ≤ 20° 限**无任何强制点**：20000px × 0.002°/px ≈ 40° 可静默通过。
- **应改成什么**：在 p3_session.cpp:109-115 加 `scale·max(W,H) ≤ 20°` 判门（fail-closed），或按 SPEC-CONFLICT 统一口径后落地。
- **依据**：CONFORM-SWEEP-2 PR-04（md:510-520）、§4 分列（md:713）
- **需变更 claim**：**是**　|　**优先级**：**P1**　|　**状态**：③ 审计发现但未定案

#### DG-B-S2-DZ-12　`docs/science/DRIZZLE.md` :29、:59（px²）vs docs/contracts/DATA_SEMANTICS.md:303（sr）；实现 drizzle_engine.cpp:53、astro_sphere_sink.cpp:295-296（sr）

- **现状**：面积单位两说：DRIZZLE.md 写 px²、DATA_SEMANTICS 写 sr，实现为 sr（S_p 实为 ADU/sr）。
- **应改成什么**：统一 DRIZZLE.md:29/:59 与 DATA_SEMANTICS.md:303 的单位表达（实现为 sr ⇒ 建议文档改为 sr 并说明与 px² 的换算关系）。
- **依据**：CONFORM-SWEEP-2 DZ-12（md:462-470）、§4 分列（md:713）
- **需变更 claim**：**是**　|　**优先级**：**P2**　|　**状态**：③ 审计发现但未定案

#### DG-B-S2-NS-09　`docs/plugins/algorithms_phase1/07_noise_snr.md` :108（`reference_flux` 单位列）

- **现状**：单位列写 `e⁻/s`；实现用 `reference_flux_adu`（ADU）；PHOTOMETRY.md:30 已述「e⁻ 需 gain，当前不可得」。
- **应改成什么**：:108 单位列改 ADU，并补键名 `reference_flux_adu`（或按裁决保留 reference_flux 但声明单位 ADU）。
- **依据**：CONFORM-SWEEP-2 NS-09（md:315-324）；FRAME-SNR-CANON §2.3（:122，F_ref 单位 ADU）
- **需变更 claim**：否　|　**优先级**：**P1**　|　**状态**：③ 审计发现但未定案（未计入 S2 的 13 条分列）

#### DG-B-S2-NS-11　`lib/algorithms/noise_snr/README.md` :27（与 :9-14 自相矛盾）

- **现状**：README 称生产通道是 `dll_loader` 装载 `snr_estimator.dll`；实际为进程内 wrapper（module_adapters.cpp:3567/:3742）。
- **应改成什么**：:27 与 :9-14 统一为「进程内 wrapper」，并说明 `snr_estimator` 的现状（零生产编译/零调用，见 CONFORM-SWEEP-2 NS-10）。
- **依据**：CONFORM-SWEEP-2 NS-11（md:338-348）
- **需变更 claim**：否　|　**优先级**：**P2**　|　**状态**：③ 审计发现但未定案

#### DG-B-S2-DZ-11　`docs/science/DRIZZLE.md` :50 vs docs/plugins/algorithms_phase1/08_drizzle.md:17、:34

- **现状**：输入 weight/SNR 面的**量级语义未定**（掩膜 vs 乘性）；实现 drizzle_engine.cpp:1362 形参被注释、:1910-1913 只作零门使用。
- **应改成什么**：DRIZZLE.md §5 明确 weight 面的语义（是否只用 0/非 0 掩膜、量级是否参与权重），并同步 08_drizzle.md。
- **依据**：CONFORM-SWEEP-2 DZ-11（md:452-461）
- **需变更 claim**：否　|　**优先级**：**P2**　|　**状态**：③ 审计发现但未定案

#### DG-B-S2-P1-13　`docs/science/ASTROMETRY.md` :160（已废止口径）

- **现状**：规范已废止「7×7 网格 <1e-6 px」判据（鉴别力为零），但实现 module_adapters.cpp:2906/:2667 仍用；报告判 C1/SPEC-ERR。
- **应改成什么**：规范侧补「已废止」标注与替代判据（密集域采样 + 1e-4 px 判门），实现按替代判据改。
- **依据**：CONFORM-SWEEP-2 P1-13（md:628-640）
- **需变更 claim**：否　|　**优先级**：**P2**　|　**状态**：③ 审计发现但未定案

#### B-II-3 CONFORM-SWEEP-3（9 条：018 019 020 022 023 024 025 026 027）

> **报告自身口径不一致（如实登记）**：`§1.2`（md:50）与 JSON `spec_side_issues=9` 记 9 条；`§3` 汇总表（md:340-351）实为 **10 行**，多出 **021**（在 §1.2 已按 C3 计一次）。下表含 021，并标注其口径差异。

#### DG-B-S3-018　`docs/algorithms/PHASE2_SAMPLER.md` :14、:62（§3 逐符号锚）、:190-200（§5.4）；同族 PHASE2_UPM_IMPL.md:68、PHASE2_REJECTION.md:58

- **现状**：行数与逐符号锚系统性过期：sampler.cpp 1156→**1520**、sampler.h 136→**286**；upm.cpp 1565→**2732**、upm.h 184→**380**；rejection.cpp 2076→**2857**、rejection.h 329→**579**。例：§5.4「cvar :840-842」实测 :875；§5.5「veto :849-850」实测 :884-885；UPM §13「归一化门 :555/:1341」实测 :646/:1646、「module_adapters.cpp:3152-3176」实测 :5076-5097。
- **应改成什么**：按 LEDGER-DOC 先例机械重锚（§3 逐符号表 + §5/§13 行号），并与 DG-C-02 的自动锚检查共用事实源。
- **依据**：CONFORM-SWEEP-3-018（md:236-246）
- **需变更 claim**：否　|　**优先级**：**P1**　|　**状态**：③ 审计发现但未定案

#### DG-B-S3-019　`docs/algorithms/PHASE2_UPM_IMPL.md` :388-393（数值常数表）、:400-401（冻结声明）

- **现状**：把**已修**（upm.cpp:646 尺度无关门，FIX-UPMSCALE）与**未修**（upm.cpp:1646 仍为 1e-12）的两种归一化门写成同一条冻结常数「s>1e-12（:555/:1341）」，且与 docs/science/PHASE2_UPM.md:56 的份额式定义（无阈值）冲突。
- **应改成什么**：:389 拆分为「build 内：尺度无关 `s>0 ∧ finite`（FIX-UPMSCALE）」与「`p2_upm_normalized_weights`：应为同一判据，现状 1e-12 = 缺陷」，并登记 fix-upmscale-report.md 为唯一记录。
- **依据**：CONFORM-SWEEP-3-019（md:248-255）；reports/RELEASE-02/fix-upmscale-report.md
- **需变更 claim**：否　|　**优先级**：**P1**　|　**状态**：③ 审计发现但未定案

#### DG-B-S3-020　`docs/science/REJECTION.md` :32（§4 禁 per-pixel 路由）vs :47、:61（§5 生产默认逐输出像素几何 n）、:93（§7 阈值不变量）

- **现状**：§4 写「禁止 per-pixel effective 路由」，§5 却写「生产默认按逐输出像素几何 n 路由」；三处限定词不同（effective / 几何 / n_eff）⇒ 同文档自相矛盾；实现 module_adapters.cpp:6034-6048/:6243 跟 §5。
- **应改成什么**：:32 改为「禁止 per-pixel **effective（资格/掩膜后存活数）** 路由；**允许并强制** per-pixel **几何 n（nominal contributors，一次解析）** 路由（§5）」。
- **依据**：CONFORM-SWEEP-3-020（md:257-266）；change-claim FIX-SCI-SNR-CANON-001 §3.5（路由依据 = 几何 n，一次解析）
- **需变更 claim**：**是**　|　**优先级**：**P1**　|　**状态**：③ 审计发现但未定案

#### DG-B-S3-022　`lib/algorithms/coverage/README.md` :69-70、:160-164；lib/algorithms/sampling/README.md:123；lib/algorithms/integration/README.md:36、:62

- **现状**：三条 DISP 缺陷（DISP-COV-003 / DISP-P2SMP-004 / DISP-P2INT-001）**已修但未同步**；coverage README 与 PHASE2_COVERAGE.md:60-64 对 DISP-COV-003 的状态直接冲突。
- **应改成什么**：三处按实际状态改写并标「已关闭 / 部分关闭」，与 PHASE2_COVERAGE.md 对齐。
- **依据**：CONFORM-SWEEP-3-022（md:276-288）
- **需变更 claim**：否　|　**优先级**：**P2**　|　**状态**：③ 审计发现但未定案

#### DG-B-S3-024　`docs/science/UNIFIED_SCIENCE_MODEL.md` :122 vs docs/science/PHASE2_UPM.md:182

- **现状**：sky_plane 模型地位冲突：UNIFIED_SCIENCE_MODEL.md:122 登记为 **UNRESOLVED**（上呈裁决），而 PHASE2_UPM.md:182 已宣告**关闭**（负责人 2026-09-19 裁决，claim FIX-SCI-SNR-CANON-001）；A 未同步订正。
- **应改成什么**：UNIFIED_SCIENCE_MODEL.md:122 按 FIX-SCI-SNR-CANON-001 改写为「已关闭 + 纯加性」并登记 OPEN-P2S-02（仅剩数据面/schema 待合同流程）。
- **依据**：CONFORM-SWEEP-3-024（md:299-307）；change-claim FIX-SCI-SNR-CANON-001 §3.1；GAP_AUDIT §9.40 C3（:1123-1127）
- **需变更 claim**：否　|　**优先级**：**P1**　|　**状态**：① 已裁决待落地（引用既有 claim）

#### DG-B-S3-025　`docs/contracts/DATA_SEMANTICS.md` :1516-1537；docs/algorithms/PHASE2_SAMPLER.md:91-99；lib/algorithms/sampling/README.md:92-99；memory.md:21-22

- **现状**：P2SamplerConfig 规范写 15 字段，实现为 17 字段（sampler.h:33-62 新增 `star_mask_snr_factor`=10.0、`star_mask_radius_deg`=0.012）。
- **应改成什么**：四处增补两行（字段名/默认值/语义），并与 CONFORM-SWEEP-3-005 的硬编码 10.0/0.012 联动。
- **依据**：CONFORM-SWEEP-3-025（md:308-316）
- **需变更 claim**：否　|　**优先级**：**P2**　|　**状态**：③ 审计发现但未定案

#### DG-B-S3-026　`lib/algorithms/sampling/README.md` :318（对照 docs/contracts/DATA_SEMANTICS.md:1548）

- **现状**：README 称 `P2ControlObservation`「14 字段」，实列 13；DATA_SEMANTICS 亦 13；实现 upm.h:31-57 为 13。
- **应改成什么**：:318 改「13 字段」。
- **依据**：CONFORM-SWEEP-3-026（md:317-325）
- **需变更 claim**：否　|　**优先级**：**P2**　|　**状态**：③ 审计发现但未定案

#### DG-B-S3-027　`lib/algorithms/coverage/include/astro/phase2/sampler.h` :60（头注释）vs docs/algorithms/PHASE2_SAMPLER.md:284、:297-301

- **现状**：头注释写「0=auto：`omp_get_max_threads/hardware_concurrency`」，ALG 文档冻结「无 hardware_concurrency、模块不得自行开线程」；实现 sampler.cpp:916 跟 ALG，漂移未登记。
- **应改成什么**：sampler.h:60 按 ALG 改写（0 视为 1；无 OpenMP/hardware_concurrency），并在 ALG 侧登记该漂移已闭合。
- **依据**：CONFORM-SWEEP-3-027（md:326-337）；ASTROCS_DESIGN.md §7.1/§8
- **需变更 claim**：否　|　**优先级**：**P1**　|　**状态**：③ 审计发现但未定案

#### DG-B-S3-021　`docs/science/REJECTION.md` :24、:108（对照 lib/algorithms/rejection/README.md:119 的 DISP-P2REJ-002）

- **现状**：SCI §8 把 `NO_CANDIDATES` 列为**排异**退化态，但排异域 `P2_STATUS_*` 无该状态（该状态属积分域，integrate.h:47）。
- **应改成什么**：:108 改指 `P2_INTEGRATE_NO_CANDIDATES` 或删除该行。
- **依据**：CONFORM-SWEEP-3-021（md:267-275，§3 表 md:345）
- **需变更 claim**：否　|　**优先级**：**P2**　|　**状态**：③ 审计发现但未定案（报告未计入其 9 条分列）

#### B-II-4 CONFORM-SWEEP-4（5 条：P3F-11、P3R-10、P3R-11、P3R-12、P3X-12）

> 报告口径（`§0` 汇总表 md:46 / JSON `by_category.OTHER={total:5}`）：**另一类 = 规范本身错或规范之间打架**，共 5 条。
> 另有 **9 条含规范侧成分但被报告归入 C7/C1**（见下），其中 3 条报告明确标注需变更 claim / 合同变更流程。

#### DG-B-S4-P3F-11　`docs/algorithms/PHASE3_FITS_IMPL.md` :10-11（行数声明）、§2 行锚（:72-84、:212-214、:392-541 等）

- **现状**：文档 556 行 / 头 64 行，实测 p3_output.cpp **560** 行、p3_output.h **99** 行；§2 signal 锚 :212-214 实为 BSCALE/BZERO 块（signal 在 :236）、:72-84 实为 :76-87、:392-541 实为 :396-558。
- **应改成什么**：按实测重刷行锚与行数（并入 DG-C-02 的机械重锚）。
- **依据**：CONFORM-SWEEP-4 P3F-11（md:145-152）；CONFORM-SWEEP-4.json items[P3F-11]
- **需变更 claim**：否　|　**优先级**：**P2**　|　**状态**：③ 审计发现但未定案

#### DG-B-S4-P3R-10　`docs/algorithms/PHASE3_RSMP_IMPL.md` :13-14（行数）、§3/§4/§6（符号表与签名）

- **现状**：文档 239/58 行，实测 p3_resample.cpp **519** 行、p3_resample.h **150** 行；§4 符号表冻结旧签名（如 `p3_sample_nearest` 旧签名 vs 实测 h:96-98）；§3 内部结构锚全不对应。
- **应改成什么**：按实测重刷 §3/§4/§6 行锚与签名表。
- **依据**：CONFORM-SWEEP-4 P3R-10（md:261-268）
- **需变更 claim**：否　|　**优先级**：**P2**　|　**状态**：③ 审计发现但未定案

#### DG-B-S4-P3R-11　`docs/algorithms/PHASE3_RSMP_IMPL.md` :215-220（§7）；docs/algorithms/PHASE3_RESAMPLE.md:72

- **现状**：文档登记 tile cache 为「有界 FIFO（最旧插入逐出）」；实现已是真 LRU（p3_resample.cpp:74-100，:78/:88 用 lru.splice），`keys.erase` 零命中 ⇒ DISP-P3RSMP-002 结论过期。
- **应改成什么**：修订为 LRU 并关闭 DISP-P3RSMP-002。
- **依据**：CONFORM-SWEEP-4 P3R-11（md:269-276）
- **需变更 claim**：否　|　**优先级**：**P2**　|　**状态**：③ 审计发现但未定案

#### DG-B-S4-P3R-12　`docs/algorithms/PHASE3_RSMP_IMPL.md` :277（§11 表）

- **现状**：文档登记 DISP-P3RSMP-003（check_mode 无调用点）为未接线；实际 module_adapters.cpp:8140-8153 已调用守卫（:8147-8152）。
- **应改成什么**：关闭 DISP-P3RSMP-003 并更新 §11 表。
- **依据**：CONFORM-SWEEP-4 P3R-12（md:277-284）
- **需变更 claim**：否　|　**优先级**：**P2**　|　**状态**：③ 审计发现但未定案

#### DG-B-S4-P3X-12　`docs/architecture/PRODUCTION_EXECUTION_INVENTORY.csv` :335（p3_v6_export.cpp 行）

- **现状**：生产执行清单把 `p3_v6_export.cpp` 标 `production=yes`，实际不在任何 `add_library`/`add_executable` 源列表（CMakeLists.txt:718-728）。
- **应改成什么**：更正 :335 的 classification/production_reachable，或按 P3X-06 完成接线。
- **依据**：CONFORM-SWEEP-4 P3X-12（md:375-382）；S4 P3X-06（md:54）
- **需变更 claim**：否　|　**优先级**：**P1**　|　**状态**：③ 审计发现但未定案（与 DG-C-01 门禁联动）

#### B-II-5 S4 另 8 条含规范侧成分但**未计入**「另一类」5 条（诚实登记，不混入 34 条统计）

> 报告定义「另一类 = 规范本身错或规范之间打架」共 5 条（P3F-11/P3R-10/P3R-11/P3R-12/P3X-12，见 B-II-4）；以下 8 条被报告归入 C7/C1，但**含规范侧成分**（悬挂引用 / 合同自相矛盾 / 单位冲突 / 冻结公式冲突），其中 **3 条报告明确标注需变更 claim 或合同变更流程**（`partial-aio-002`/`003`/`025`，★）。

#### DG-B-S4X-CLI06　`docs/plugins/infrastructure/18_cli.md` :16（悬挂引用）

- **现状**：引用 `contracts/schemas/cli_output.schema.json`、`events.schema.json` 均不存在（实际为 `jsonl_event_v1.schema.json` 等）。报告原文明写「规范侧悬挂引用」，但判 C7/未实现，未计入「另一类」5 条。
- **应改成什么**：改指实际存在的合同文件，或补建缺失 schema。
- **依据**：CONFORM-SWEEP-4 partial-cli-06（md:635-642）
- **需变更 claim**：否　|　**优先级**：**P2**　|　**状态**：③ 审计发现但未定案（未计入 5 条）

#### DG-B-S4X-CLI07　`docs/plugins/infrastructure/23_hips_browser.md` :17（悬挂引用）

- **现状**：引用 `hips_product.schema.json` 不存在（同类悬挂引用）。
- **应改成什么**：改指实际 schema 或补建。
- **依据**：CONFORM-SWEEP-4 partial-cli-07（md:643-650）
- **需变更 claim**：否　|　**优先级**：**P2**　|　**状态**：③ 审计发现但未定案（未计入 5 条）

#### DG-B-S4X-CLI08　`contracts/config/cli_selftest.schema.json` :4；contracts/config/cli_modules_list.schema.json:4

- **现状**：两份合同服务于**已删除的命令面**（陈旧合同）。
- **应改成什么**：按 ENGINEERING_SPEC §8 显式退役（或在合同台账标注 DEPRECATED）。
- **依据**：CONFORM-SWEEP-4 partial-cli-08（md:651-660）
- **需变更 claim**：否　|　**优先级**：**P2**　|　**状态**：③ 审计发现但未定案（未计入 5 条）

#### DG-B-S4X-AIO02　`docs/algorithms/HIPS_WRITER.md` :196；docs/algorithms/PHASE3_HIPS_TO_FITS.md:63（同文件 :189 自承方向相反）

- **现状**：`hips_frame` 值域写 `icrs`，而同文件 :189 自承方向相反 ⇒ 同文档自相矛盾。
- **应改成什么**：**待 SCI/ALG 变更 claim 定稿后二选一**（报告★标注）。
- **依据**：CONFORM-SWEEP-4 partial-aio-002（md:401）；GAP_AUDIT §8.1（:38-62，P0-19）
- **需变更 claim**：**是**　|　**优先级**：**P1**　|　**状态**：③ 审计发现但未定案（★报告标注需变更流程）

#### DG-B-S4X-AIO03　`contracts/schemas/v6/astrocs.v6.phase3.v1.schema.json` :182（const 冻结对角特例）

- **现状**：合同 const 冻结「对角特例」与 FROZEN 的 `C_out = R C_in Rᵀ` 口径冲突。
- **应改成什么**：**需 SCI/合同变更流程**统一（报告★标注）。
- **依据**：CONFORM-SWEEP-4 partial-aio-003（md:409）；docs/science/UNCERTAINTY_AND_COVARIANCE.md
- **需变更 claim**：**是**　|　**优先级**：**P1**　|　**状态**：③ 审计发现但未定案（★报告标注需合同变更）

#### DG-B-S4X-AIO25　`docs/science/PHASE3_HIPS_TO_FITS.md` :63（ADU）vs docs/science/DRIZZLE.md:59、冻结 FZ-UNIT-SIGNAL-SB（ADU/px²）

- **现状**：signal 面亮度单位两说：SCI-P3:50 的 'ADU' vs FZ 单位表的 'ADU/px^2'；实现 p3_rsmp_units.cpp:77、p3_v6_export.cpp:235/:503/:587、p3_output.cpp:216 需按裁决统一。
- **应改成什么**：**先由规范侧裁决权威单位表达，再统一实现四处**（报告★标注需 SCI 变更 claim）。
- **依据**：CONFORM-SWEEP-4 partial-aio-025（md:585）、P3X-08（md:343-350）
- **需变更 claim**：**是**　|　**优先级**：**P1**　|　**状态**：③ 审计发现但未定案（★报告标注需 SCI 变更 claim）

#### DG-B-S4X-P3X13　`docs/contracts/v6/frozen/astrocs.v6.contract-freeze.v1.json` :798（FZ-FORMULA-COV-PROP）

- **现状**：冻结公式 FZ-FORMULA-COV-PROP 与生产 `Σc_k²u_k` 口径冲突（报告 P3X-13）。
- **应改成什么**：由合同变更流程统一（或把生产实现升级为含相关项）。
- **依据**：CONFORM-SWEEP-4 P3X-13（md:383-390）
- **需变更 claim**：**是**　|　**优先级**：**P2**　|　**状态**：③ 审计发现但未定案

#### DG-B-S4X-CLI05　`lib/infrastructure/cli/module.yaml` :13（entrypoint）、:25（data_contracts）

- **现状**：entrypoint 与 data_contracts 路径均不存在（合同悬挂）。报告判 C7，未计入「另一类」5 条。
- **应改成什么**：修正路径或补建目标（并与 CHK-MODULE-MANIFEST / CHK-CONTRACT-REF 对齐）。
- **依据**：CONFORM-SWEEP-4 partial-cli-05（md:627-634）
- **需变更 claim**：否　|　**优先级**：**P2**　|　**状态**：③ 审计发现但未定案（未计入 5 条）

### B-III 已订正 / 已裁决 / 已撤销（**只作状态登记，禁止再写成待订正**）

> 本节存在的唯一目的：防止后续分片或审计把**已经完成**的事项当待办重复派工，或把**已被否决**的建议重新提出。

#### DG-B-III-01　`reports/RELEASE-02/conformance/CONFORM-SWEEP-1.md` CONFORM-SWEEP-1-003（md:77-86）

- **现状**：曾报「质心 index-is-center vs 规范 index+0.5 ⇒ 恒定 0.5 px 系统偏移」。
- **应改成什么**：**已撤销，判为非缺陷**：链内有登记在册的坐标契约（star_coord_contract.h），全链 7 个消费点逐点审计无一处要求 index+0.5；数值 (p1_sources.x+0.5)−真值 = −0.0140/+0.0340 px（sd 0.079），两读法经 WCS 对 Gaia 之差 = (+0.512,+0.471) ≈ 恰好 (0.5,0.5)；**禁止**按原建议加 +0.5（会同时破坏 4 条消费路径与 2 道门）。修复 = 纯文档（DATA-P1-SOURCES 列语义表 + 规范补注 + 门登记）。
- **依据**：GAP_AUDIT §9.48 ③（:1395-1402）；reports/RELEASE-02/stardet-conform-invest.md §2.4（:264-297）
- **需变更 claim**：否　|　**优先级**：**—**　|　**状态**：④ 已订正/已裁决/已撤销（仅登记，不得再写成待订正）

#### DG-B-III-02　`config/templates/normalize.phase_config.json` :6-7；contracts/schemas/phase_config_normalize.schema.json:122-132；config/config_registry.json:663-673/:768-777

- **现状**：S2-NS-05 曾报 `sparse_snr_layer` 规范默认 true vs 生产 false（联锁）。
- **应改成什么**：**已订正并落地**：模板 false→true；schema 补 `default: true` 并新增 `sparse_snr_spacing_px`（integer ≥1，default 64）；登记册两处 `declared_default` false→true；defaults 新增 `sparse_snr.spacing_px`=64（owner_adjudicated）。文档侧仅剩 3 处**书写形态**（DG-B-01/02/03）。
- **依据**：reports/RELEASE-02/snr-config-land.md §2（:36-49）、§5.1（:89-92）；GAP_AUDIT §9.45/§9.46
- **需变更 claim**：否　|　**优先级**：**—**　|　**状态**：④ 已订正/已裁决/已撤销（仅登记，不得再写成待订正）

#### DG-B-III-03　`docs/development/CONFIG_SCHEMA.md` :19；docs/algorithms/PHASE2_UPM_IMPL.md:382；lib/algorithms/upm/README.md:138；upm.h:75

- **现状**：S2/S3-009/023 曾报 `smoothing_lambda` 默认冲突（auto→0.1 vs 0.0 默认关）。
- **应改成什么**：**已裁决（§9.39 A5）**：`smoothing_lambda` **不能为 0**（不能保持生产现值 0）；风险 = 真实亮结构（M16 核心）被过平滑压暗，取值须平衡「消台阶」与「保亮结构」；必须做合成实验（虚拟数据 + 加性天光）与真实数据实测，并给 λ 扫描曲线。**具体数值待实验**（见 D-07）。
- **依据**：GAP_AUDIT §9.39 A5（:1080-1090）；reports/RELEASE-02/seam-archaeology.md；reports/RELEASE-02/snr-config-land.md §6.3
- **需变更 claim**：否　|　**优先级**：**—**　|　**状态**：④ 已订正/已裁决/已撤销（仅登记，不得再写成待订正）

#### DG-B-III-04　`docs/science/REJECTION.md` :21、:38、:47-62、:88、:161、:173、:177（已订正）

- **现状**：曾报生产默认 `reject_profile` 与合同 `wbpp_2_9_1` 不符。
- **应改成什么**：**已裁决（C2）+ 文档已订正**：canonical = 自研 `astrocs_adaptive_pixel`；SCI 与合同文档已按 claim FIX-SCI-SNR-CANON-001 §2.4 订正；`wbpp_2_9_1` 保留为对照档/回归基线。**残留**：合同 JSON `contracts/data/phase2_uncertainty_rejection_provenance_v1.json:67` 与工具默认 `stage2_common.h:69` 仍待跟随（登记未改）。
- **依据**：change-claim FIX-SCI-SNR-CANON-001 §2.4/§3.5；reports/RELEASE-02/canon-freeze.md §2.4；CONFORM-SWEEP-2 §4.2 ②（md:716）
- **需变更 claim**：否　|　**优先级**：**—**　|　**状态**：④ 已订正/已裁决/已撤销（仅登记，不得再写成待订正）

#### DG-B-III-05　`docs/science/PHASE2_UPM.md` :181-182（UNRESOLVED 已关闭）

- **现状**：曾登记「设计-实现模型冲突」UNRESOLVED（纯加性冻结 vs 乘性+加性目标态）。
- **应改成什么**：**已关闭**为「本期决议：纯加性」（负责人 C3 同意 + D-1）；`11_upm.md`/`10_sampling.md`/`UNIFIED_MODEL.md`/`PHASE2_DETAILED_DESIGN.md` 目标态已同步；`OPEN-P2S-02` 仅剩数据面/schema 待合同流程。**残留**：`docs/science/UNIFIED_SCIENCE_MODEL.md:122` 未同步（= DG-B-S3-024，已列）。
- **依据**：GAP_AUDIT §9.40 C3（:1123-1127）；change-claim §3.1；reports/RELEASE-02/canon-freeze.md §2.2
- **需变更 claim**：否　|　**优先级**：**—**　|　**状态**：④ 已订正/已裁决/已撤销（仅登记，不得再写成待订正）

#### DG-B-III-06　`docs/contracts/DATA_SEMANTICS.md` :1108-1113（目标合同只冻结 variance/ivar）

- **现状**：曾建议「成品输出 SNR 面」（前台 B1 建议）。
- **应改成什么**：**已否决不批**（负责人 D-3/B1）：只有 Phase1 HiPS 带 SNR 数据块；Phase2 不输出 SNR 面（叠加中消费、不复用）；Phase3 无 SNR；属顶层合同变更，负责人不批。目标合同只冻结 variance/ivar。
- **依据**：GAP_AUDIT §9.38 D-3/B1（:1057-1066）；change-claim §3.3；reverse_verify/docs/snr-propagation-design.md §11.5（:1310-1311）
- **需变更 claim**：否　|　**优先级**：**—**　|　**状态**：④ 已订正/已裁决/已撤销（仅登记，不得再写成待订正）

---

## C 门禁需增补的检查（5 条）

> 背景：`fix-gates` 已把 §9.34 ⑥ 建议的 7 项门（`CHK-ALGO-WIRING`/`CHK-REGISTRY-IR-PARITY`/`CHK-CONFIG-CONSUMED`/`CHK-CONFIG-DEFAULTS`/`CHK-PROD-SCALE`/`CHK-PROVENANCE-CONSISTENCY`/`CHK-REALDATA-E2E`）**落地进 `ci/checks.json`**（已核对：42 个 `CHK-*` 中均存在）。
> 本节只列**仍然存在的盲区**，不重复已落地项。

#### DG-C-01　`ci/checks.json` 新增检查项（CI 注册表）；建议实现 ci/check_spec_named_impl.py

- **现状**：不存在该门。ci/checks.json 现有 42 个 CHK-*，无任何一项断言「规范点名的权威实现是否真在生产链上」。本轮三条同类缺陷（S1-002 生产检测器≠ALG-STARDET-001 点名的 sdet_api；S2-NS-01 规范点名的 noise_model.cpp 因 CMakeLists.txt:240 被注释不在构建图；S4-P3X-06 整个 V6 三模式导出链未编入生产 target）全部落在这个盲区。
- **应改成什么**：① 建立「规范点名 → 符号/文件」映射表（从 ALG/SCI 文档的行锚与 `docs/algorithms/anchors/anchor_contract.json` 抽取）；② 对每条映射断言：目标文件在根 CMake 的某个生产 target 源列表中，且该符号在生产可达调用图上；③ 不可达者必须由显式台账承载（reason/owner/exit_condition），不得静默；④ 能红能绿（负例：把某 target 的源列表去掉该文件必须判红）。
- **依据**：GAP_AUDIT §9.44「暴露的门禁盲区」（:1295-1298，负责人令「须新增门禁类 CHK-SPEC-NAMED-IMPL-ON-PROD-PATH」）；S1-002（:64-76）；S2-NS-01（:205）；S4-P3X-06（S4.md:54）
- **需变更 claim**：否　|　**优先级**：**P0**　|　**状态**：① 已裁决待落地（§9.44 明令新增）

#### DG-C-02　`docs/algorithms/anchors/` check_doc_line_anchors.py + anchor_contract.json（扩展）；ci/checks.json DOC-LINE-ANCHORS 步骤

- **现状**：现 DOC-LINE-ANCHORS 覆盖 C4_symbol_binding（符号是否还在/移到哪），不检查 ALG 文档 §3 逐符号行号表与行数声明。实测漂移：PHASE2_SAMPLER 1156/136→1520/286、PHASE2_UPM_IMPL 1565/184→2732/380、PHASE2_REJECTION 2076/329→2857/579、PHASE3_FITS_IMPL 556→560、PHASE3_RSMP_IMPL 239→519；STAR_DETECTION_ALGORITHMS.md:40/:201 行内锚失效。
- **应改成什么**：① 机械抽取 ALG 文档中的「文件（N 行）」声明与「符号 :a-b」行号锚；② 与工作树实测比对，漂移即判红（或按 LEDGER-DOC 先例提供自动重锚命令）；③ 与 C-04 的 config_consistency_check 共用事实源解析。**登记状态**：本项已由 GUARD-TOOLS-FIX 在办（工作区 tools/config_consistency_check.py 已重写，见 DG-C-04），但「ALG 文档行号锚」这一检查项本身尚未落地为 CHK-*。
- **依据**：CONFORM-SWEEP-3-017（:226-234，修复面明写「建议增『ALG 文档行数锚 vs 源文件实测』检查项」）；CONFORM-SWEEP-3-018（:236-246）；CONFORM-SWEEP-1-022（:272-279）；S4 P3F-11/P3R-10
- **需变更 claim**：否　|　**优先级**：**P1**　|　**状态**：① 已裁决待落地（GUARD-TOOLS-FIX 在办，登记状态）

#### DG-C-03　`ci/check_config_defaults.py` :36-70（config_key_set 只取 defaults.json 的 fields[].key 末段，不取值）

- **现状**：该门的 D1 判据 = 「配置键集合 = templates 叶子键 ∪ defaults.json fields[].key 末段 ∪ phase_config schema 属性 ∪ 台账 watch_keys」；键集合只从 defaults.json 取键名，值不参与比较 ⇒ defaults.json#snr.path / #sparse_snr.spacing_px 与模板/schema 的同值性无机器门覆盖。
- **应改成什么**：① 把 defaults.json 的 fields[].value 纳入比较面（对 owner_adjudicated 与 pending_authority 分别处理）；② 对声明了 enum_token/enum_target 的键，断言 defaults 值 ∈ schema enum；③ 能红能绿（负例：把 defaults 的 snr.path 改成 dense 而模板仍 sparse_reconstruct 必须判红）。
- **依据**：reports/RELEASE-02/snr-config-land.md §7.1（:164）；ci/check_config_defaults.py:36-70（判据自述）
- **需变更 claim**：否　|　**优先级**：**P1**　|　**状态**：① 已裁决待落地（SNR-CONFIG-LAND 登记）

#### DG-C-04　`tools/config_consistency_check.py` :1-46（文件头审计根因与 L0–L5 判据）；tools/check_p2_symbol_map.py

- **现状**：S3-017 判定时：tools/config_consistency_check.py 只输出 env_missing×4（引 工程控制/schemas/stage2.schema.json、工程控制/configs/stage2.template.json、lib/phase2/include/astro/phase2/stage2_common.h、lib/phase2/src/stage2_common.cpp，全部不存在）；tools/check_p2_symbol_map.py 报 FileNotFoundError（docs/refactor/P2_SYMBOL_MAP.md）；tools/check_module_readmes.py 不校验行号/常数/默认值。**登记状态：GUARD-TOOLS-FIX 在办**——工作区 tools/config_consistency_check.py 已重写（引入 include_consts/resolve_const 具名常量解析、L0a–L0e 活体/退役事实源判据、registry 豁免台账；见文件头 :1-46），补丁脚本落 run/tmp/guard_tools_fix/；**未提交**。
- **应改成什么**：① 完成 GUARD-TOOLS-FIX 并提交（含 check_p2_symbol_map.py 的路径更新或显式退役）；② 把该工具挂进 ci/checks.json（当前不在 CHK-* 注册表）；③ 在 docs/development/CONFIG_SCHEMA.md:3-4 的自述处更新工具路径与覆盖面。
- **依据**：CONFORM-SWEEP-3-017（:226-234）；tools/config_consistency_check.py:1-46（现状自述）；run/tmp/guard_tools_fix/patch_cc*.py（在办证据）；git status 显示 tools/config_consistency_check.py 为 M（未提交）
- **需变更 claim**：否　|　**优先级**：**P0**　|　**状态**：① 已裁决待落地（GUARD-TOOLS-FIX 在办，登记状态）

#### DG-C-05　`lib/infrastructure/cli/parser.cpp` :325（kSessionKeys 白名单）

- **现状**：snr_path 与 sparse_snr_spacing_px 已由 contracts/schemas/phase_config_{mosaic,normalize}.schema.json 声明、由 config/templates/*.json 提供、由 config/defaults.json 登记，但 kSessionKeys 不含这两键 ⇒ 按现实现会被判 unknown key 并退出 3（与 RELEASE-02 SD-15 同类）。现有 CHK-SCHEMA / CHK-CONFIG-CONSUMED 都不比较「schema 属性集合 ⊆ CLI 白名单」。
- **应改成什么**：① 断言 phase_config_*.schema.json 的 config 属性集合 ⊆ parser.cpp kSessionKeys（或显式台账豁免）；② 对 config/templates/*.json 的每个叶子键做同样的可达性断言；③ 能红能绿（负例：从 kSessionKeys 删掉一个已声明键必须判红）。
- **依据**：reports/RELEASE-02/snr-config-land.md §5.3 第 1 条（:112）；ci/ledgers/dead_config_keys.json:34/:36/:47/:49（两条死键登记的 exit_condition 明写「同时 CLI kSessionKeys 补键使配置可达」）
- **需变更 claim**：否　|　**优先级**：**P1**　|　**状态**：① 已裁决待落地（SNR-CONFIG-LAND 登记为实现跟随项）

---

## D 决策清单（10 条；9 条已裁决，**仅 D-08 待负责人裁决**）

> **订正（2026-09-20，V5 分片 3）**：原标题「D **待负责人裁决**清单（10 条）」作废。D-01/04/10 = §9.49 定案 2 + §9.62；D-02 = §9.50 定案 7 + §9.60 + §9.67 定案 4；D-05 = §9.50 定案 3 + §9.51/§9.52 + §9.67 定案 6；D-07 = §9.55 S5（⚠ 与 §9.67 定案 5 是否同指未判定）；D-09 = §9.50 补传 + §9.67 定案 7；D-03 = §9.67 定案 5（本期不加堆叠平滑项 + 实验义务）；D-06 = §9.67「前台须同时执行」第 4 条（`frame_snr` 改名，纯命名消歧）；仅 **D-08**（`uncertainty_available` / W2 协方差 API）仍待负责人裁决（§9.66 B 同族）。

> 本节为**单列一节**，对应状态 ②。每条给：背景 / 选项 / 后果 / 前台倾向。
> **口径说明**：D 节是**决策级**清单（10 项），一项决策可覆盖多个台账条目；台账里 `status` 直接标 ② 的只有 4 条（`DG-B-10`、`DG-B-11`、`DG-B-S2-PH-05`、`DG-B-S2-PH-06`），其余决策点落在标 ①/③ 的条目上（例：D-03 覆盖 `DG-A-11` + `DG-B-10`）。

### D-01 ✅ 已裁决（§9.49 定案 2 / §9.62）：「帧间一致性」是语义目标，不是门禁判据

> **订正（2026-09-20，V5 分片 3）**：原状态「② 待负责人裁决」作废。依据 §9.49 定案 2（`工程控制/RELEASE-02/GAP_AUDIT.md:1487-1495`：「门只有一个：单帧标定是否可信…与其它帧无关」「删除组间 `k` 散度门」）+ §9.62（`:2258-2264`：组间门降级为报告字段）。选项 A/B 为等价口径（已落地）；选项 C 作废。

- **背景**：`change-claim FIX-SCI-SNR-CANON-001 §3.6` 写「有意义的判据只有两个：① 测光一致性 ② **帧间一致性**（各帧落同一测光体系）」；而 §9.49 定案 2 明令「**这玩意应该是帧间独立的，为啥要组间对比**」「删除组间 k 散度门」「门只有一个：单帧标定是否可信，与其它帧无关」。两处措辞会被读成互相矛盾。
- **选项 A（推荐）**：把 §3.6 的「帧间一致性」明确为**语义目标**（所有帧都标定到**同一 Gaia 绝对测光体系**这一共同坐标系），并在 claim 补一行「**不作门禁判据**；跨帧 k 不同是正常的」；最高设计 `DG-A-02`/`DG-A-06` 同口径。
- **选项 B**：保留「帧间一致性」为判据，但改写成「帧内可信度通过即接受」（等价于 A，仅措辞不同）。
- ~~**选项 C**：维持现状（两处措辞并存）。~~ ⇒ **已作废（2026-09-20，V5 分片 3；依据 §9.49 定案 2 / §9.62）**：维持现状＝保留已实测致 12/12 板块被拒的组间门（见下「后果」行）。
- **后果**：A/B 纯文档、零代码风险，与定案 2 自洽；**C 的后果是已实测的**：`f-instr-conform-fix §4.4` 显示 0.02 dex 组间门导致 **12/12 板块全部被拒**（`photometry_applied=false`、`degraded_reason=photscale_incomplete`），而真正「同组」（同夜）的帧对全数据集只有 2 对。

### D-02 ✅ 已裁决（§9.50 定案 7 / §9.60 / §9.67 定案 4）：`F_ref` = 固定星等共同基准，`m_ref = 6.0`

> **订正（2026-09-20，V5 分片 3）**：原状态「② 待负责人裁决」作废。依据 §9.50 定案 7（`工程控制/RELEASE-02/GAP_AUDIT.md:1546,1558`「直接用 6 等星」）+ §9.60（`:2122-2128` 候选 A 采用 / 候选 B 否决）+ §9.67 定案 4（`:2570-2582`「取 6 等星，即『不能』那一支」）。**注意：`m_ref = 6.0`，不是本节举例的「20 等星」**；选项 B（组内公共）作废。

- **背景**：`FRAME-SNR-CANON §2.3` 现定案 `F_ref` 必须**组内公共**（`reference_flux_scope="group"`）；但生产实测 **13/13 个多帧产品的 `F_ref` 组内相对偏差 0.023–0.533**（如 `t3_m4_red` 6 帧跨度 **2.14×**，18 个「偏差=0」的全是单帧）⇒ 帧间 SNR 不可比。§9.49 定案 7 已由前台建议「用**固定星等**（如『本帧对 20 等星的 SNR』）⇒ 连『组』的概念都不需要，完全帧间独立」，并注明**待负责人确认**。
- **选项 A（前台建议，推荐）**：`F_ref` 定义为**固定星等**对应的通量（如「本帧对 20 等星的 SNR」）⇒ 完全帧间独立，与定案 2 自洽；需改 `FRAME-SNR-CANON §2.3/§2.4` 与 HiPS 头键语义（`reference_flux_scope: group → fixed_mag`）。
- **选项 B**：保留「组内公共 `F_ref`」⇒ 需先修 Phase1 写侧使组内取同一 `F_ref`，且「组」概念与定案 2 存在张力。 ⇒ **已作废（2026-09-20，V5 分片 3；依据 §9.60 / §9.66 A / §9.67 定案 4）**：现行为逐帧 `F_ref,k` + 公共锚 `F0`，`reference_flux_scope="frame_independent_fixed_magnitude"`。
- **后果**：A 一次性消除「参考星选择」假信号（实测跨度 2.14×）并去掉「组」；B 需改实现且保留一个与帧间独立原则冲突的概念。

### D-03 ✅ 已裁决（§9.67 定案 5）：本期**不加**堆叠平滑项 + 必须做实验验证

> **订正（2026-09-20，V5 分片 3；依据更晚裁决 §9.67 定案 5，`GAP_AUDIT.md:2584-2588`）**：原状态「② 待负责人裁决」作废 —— 负责人「**我认为不加也能完美平滑**」⇒ 本期不加；须做实验验证「(c) 排除自身 + 阻尼 α≈0.5 + 权重自洽 + 末端扣残差场」在不加平滑项时能否达到完美平滑（非退化判据：保留背景前提下的帧间一致性 + 边界跳变，并与「加平滑项」对照）。选项 A（登记为下一期研究项）已采纳；选项 B（本期即加）未采纳。**注**：B 报告 G27/G28（`DG-B-10`/`DG-A-11` 的 status 行）仍标 ②，未在本分片改动范围内，见 F-3 表下注 ②。

- **背景**：§9.28 Q2 **代数证明**——模型 `y_i=s+g_i` 有 gauge 自由度，堆叠图 `M(p)` 只被平移一个逐像素常数，**台阶 `Δ=M_B−M_A` 在平移下不变 ⇒ 接缝对 gauge 严格不变**；「全局加权均值为零」与「参考帧 `δ_ref≡0`」**接缝完全相同**；旧设想文档的目标 `Σw_i·g_i/W→0` **只能全局成立（一个自由度），不可能逐像素成立**。§9.30 又证明 **(c) 排除自身 + 阻尼 α≈0.5 + 权重自洽 + 末端扣残差场** 可把阶跃压到 0.035/0.060（对照 (a)/(b) 0.209/0.260、(d) 4.91 发散）。**要再压低边界跳变必须改拟合目标（加堆叠平滑项）**，但该项无现成文献锚。
- **选项 A（推荐）**：本期只做 (c) + 阻尼 + 权重自洽 + 末端扣残差场（已有代数 + 数值双重证据，且真实数据样本重拟合边界跳变比 `raw 2.04 / (a) 2.10 / (b) 2.10 / (c) 1.62 / (d) 1.63` 同向支持）；「堆叠平滑项」登记为下一期研究项。
- **选项 B**：本期即加堆叠平滑项 ⇒ 需定义目标函数、正则化与 gauge 的关系，属新科学，须论文级论证与新的 Oracle。
- **后果**：A 风险低、可立即落地、可验收；B 可能进一步压低但工期与科学风险显著上升，且 (c) 的收益（4–6×）尚未在真实数据上吃满（真实数据仍受 `ivar/snr` stub 阻塞，见 E-10）。

### D-04 ✅ 已裁决（§9.49 定案 1 / §9.50 授权）：检测范式 = 星表引导；`sdet` 去留转前台按实测评估

> **订正（2026-09-20，V5 分片 3）**：原状态「② 待负责人裁决」作废。依据 §9.49 定案 1（`GAP_AUDIT.md:1480-1485`：星表引导检测）+ §9.50（`:1563-1566` 授权前台按实验证据自决）。选项 A 已采纳；B/C 未采纳。

- **背景**：§9.48 ② 确认 **C1 不符**：生产 `wrapper_phase1::StarDetector` 给 **345,960** 条源、Gaia 外部纯度 **0.212%**（中位距离 56.1 px）；规范点名的 `sdet_api.cpp` 给 **1,473** 条、纯度 **45.553%**（中位 1.1 px）。四条独立证据同向指向 sdet，切换代价 **7–12 人日 + 全链重验收**，属科学影响面变更。**但** §9.49 定案 1 已把检测改为**星表引导**——星表引导本身就会消灭 34.5 万假源（星云结构不在 Gaia 位置上），可能使「切 sdet」不再必要。
- **选项 A（推荐）⇒ 已采纳（2026-09-20）**：**先实现星表引导，再评估是否切 sdet**。星表引导下两检测器的差异只体现在「星表位置上的拟合质量/完备性」，可比性与决策依据都更强，且避免重复全链重验收。
- **选项 B**：立即按 §9.48 ② 方案 A 切 sdet，再做星表引导 ⇒ 代价 7–12 人日且可能与星表引导的工作重叠（PSF 行集、测光样本、`k_photo` 标度、F0、权重场全部改数）。
- **选项 C**：只做星表引导，`sdet` 退役（登记 DORMANT）⇒ 会丢失 sdet 的 20 条冻结公式与外部纯度证据（45.553%），且 `ALG-STARDET-001`/`TEST-STAR-DESIGN-001` 需整体重写。
- **后果**：A 把决策建立在星表引导后的实测上（最省且最稳）；B 立刻消除「规范点名的权威源不在生产链」这一 C1 缺陷（门禁 `CHK-SPEC-NAMED-IMPL-ON-PROD-PATH` 会继续报红）；C 消除缺陷但牺牲证据链。

### D-05 ✅ 已裁决（§9.50 定案 3 + §9.51 Q-C / §9.52 + §9.67 定案 6）：口径 = PSF 域；判据 = 测光正确 + 拟合收敛

> **订正（2026-09-20，V5 分片 3）**：原状态「② 待负责人裁决」作废。依据 §9.50 定案 3（`GAP_AUDIT.md:1555`「`MAD ≤0.03 等` **不是硬门**」⇒ 判据 = 测光正确 + 拟合收敛）+ §9.51 Q-C（`:1647-1651` A6 不照搬 PMM 孔径口径）+ §9.52（`:1705-1715` PSF 域 0.0127 mag vs 盒和 0.3429 mag）+ **§9.67 定案 6**（`:2590-2594`：`0.03 mag` 阈值**作废**，新判据须从误差预算逐项推导）。**残留**（不在本决策内）：`aperture_radius` 默认值与 `mode` 键去留 = 前台按 §9.50 授权定案（见 PH-05/PH-06 状态行）。

- **背景**：负责人 A6 令「**查论文，科学软件算法等**」；`F-INSTR-CONFORM-FIX` 已把 `F_instr` 改为 PSF 拟合域通量 `2πA·sxsy/3`（与 `PSF.md:52/:87` 一致），但**验收判据未达**：测光一致性（残差 MAD）目标 ≤0.03 mag，after 中位 **0.0617**、最坏 **0.1160** mag（before 最坏 1.8459）。同族冲突：`06_photometry.md:32` 孔径默认 `2×FWHM` vs 模块 README/实现 `4.0 px`（PH-05）；`06_photometry.md:31` `mode` 默认 `psf` vs `PHOTOMETRY.md:96`/实现（只孔径，且 `mode` 键无读者）（PH-06）。
- **选项 A（推荐）**：接受 PSF 拟合域 `2πA·sxsy/3` 为 canonical；把「≤0.03 mag」判据按拥挤场（M42）+ 2.0–2.6 px seeing + 16bit 数据的现实**重定**（如改为「受控 seeing 判别力 + 帧间一致性 + 残差 MAD 上限按场条件分档」）；同时裁决孔径默认（建议规范改为显式像素值并说明与 FWHM 的关系）与 `mode`（建议生产读 `mode` 或删除该键）。
- **选项 B**：立项 **D2 PSF 加权最优提取**（更严拟合质量剔除 / 去混叠）以冲 ≤0.03 mag。 ⇒ **已立项（2026-09-20，§9.54:1831-1833）**；但 `≤0.03 mag` 作为阈值已由 §9.67 定案 6 作废。
- **后果**：A 立即可落地且诚实；B 增加工期，但不保证在拥挤场可达（报告已注明「两种口径都未达」）。

### D-06 ✅ 已裁决（§9.67「前台须同时执行」第 4 条）：改名，纯命名消歧

> **订正（2026-09-20，V5 分片 3；依据更晚裁决 §9.67，`GAP_AUDIT.md:2608`）**：原状态「② 待负责人裁决」作废 —— §9.67「前台须同时执行（不需再问）」第 4 条明列「**D-06 `frame_snr` 改名（纯命名消歧）**」⇒ 选项 A 方向（改名 `depth` / 加 `not_a_snr: true`，`frame_snr` 留给 `snr_reference.snr_f`）由前台按合同流程落地，不再是负责人待决项。

- **背景**：该字段实为 **5σ 深度容器**（31/32 产品带 `"NOT a whole-frame scalar SNR"` 自文档化），而真正的帧级科学量是 `snr_reference.snr_f`（`ASTROCS_FRAME_SNR`，`aio_hips_writer.cpp:1169`）；与 `07_noise_snr.md §4.1`/`UNIFIED_MODEL.md:42` 对 `frame_snr` 的定义**同名不同物**，直接违反 `UNIFIED_MODEL.md §2`「禁止互相冒充」。
- **选项 A（推荐）**：改名 `depth`（或加 `not_a_snr: true`），把 `frame_snr` 留给 `snr_reference.snr_f` ⇒ 属**合同变更**（`DATA-P1-SNR` schema），随 Phase1 SNR 实现分片一并做。
- **选项 B**：保留字段名 + 强制 `definition` 注解（现状已 31/32 自文档化）⇒ 零成本，但同名不同物在合同层仍在。
- **后果**：A 彻底消歧、需下游适配；B 零成本但项目自己的产品继续违反自己的数据对象纪律。

### D-07 ⚠ 判不了（**默认值未冻结**）：§9.55 S5（λ=0.1）与 §9.67 定案 5（本期不加堆叠平滑项）是否同指**未判定**

> **订正（2026-09-20，V5 分片 3）—— 判不了（默认值未冻结）**：原状态「② 待负责人裁决」作废（取值已不属负责人待决项），但**两条裁决是否同指未判定** ⇒ **本项按「判不了」处理，不写死默认值**。两条并列：① §9.55 S5（`GAP_AUDIT.md:1846-1859`）：「**λ 生产默认取 0.1**（`P2_SMOOTHING_LAMBDA_AUTO=0.1`）…**保留** `λ=0` 作为**显式 opt-in**」；ALG §13 冻结值订正走变更 claim。② §9.67 定案 5（`:2584-2588`）：「本期**不加**堆叠平滑项」+ 必须做实验验证。**缺什么证据**：§9.67 定案 5 原文未出现 `smoothing_lambda`／`P2_SMOOTHING_LAMBDA_AUTO` 字样，两条裁决的**适用对象未在裁决文本中对齐**（⑦ 批次 §4 判不了 1 同结论）⇒ `docs/plugins/algorithms_phase2/11_upm.md §5` 只登记字段、默认值标「未冻结」。

- **背景**：§9.39 A5 已裁决「**肯定不能等于 0**」，但须防过平滑（M16 核心这类突兀亮度峰值会被压暗），且须做**合成实验（虚拟数据 + 加性天光）与真实数据实测**并给 λ 扫描曲线；§9.41 又要求这些合成实验必须按**物理噪声过程**重做。现状：生产默认 **0**（`module_adapters.cpp:4536-4537`）vs 工具 `stage2_common.cpp:140` `smoothing:auto→0.1` ⇒ 双实现漂移（已确证）。
- **选项 A**：先做实验（物理噪声合成 + M16 核心真实数据）再定值；期间生产保持 0 ⇒ 「不能为 0」的裁决在实验完成前**未落地**。
- **选项 B（推荐）**：先取保守小值（如 `0.1`，与工具侧 `auto→0.1` 对齐）**立即消除生产/工具漂移**，并在实验后按 λ 扫描曲线调整。
- **后果**：A 严谨但漂移继续存在；B 立即消除一个已确证缺陷（H5 双实现漂移），代价是可能对亮结构过平滑（可由扫描曲线快速校正）。

### D-08 `uncertainty_available` 期望值与 W2 协方差 API（UNC-PROP M2）

- **背景**：`UNC-PROP §④ M2` 明写「生产需给 W2 加协方差 API（或改用 MA）—— **非最小改动，须裁决**」；当前 `param_cov_included` 硬编码 `false` ⇒ `uncertainty_available` 恒 false（这是**正确的 fail-closed**，不是缺陷），但 `p2_corrected.json` 已写出的逐帧 `PΣPᵀ` 面成为 **write-only**，tier-1 权重 `1/Var(corrected)` 是死代码。
- **选项 A**：本期保持 `uncertainty_available=false`（**不得声称逆方差加权**），把 W2 协方差出口立项为下一期。
- **选项 B（推荐）**：本期加 W2 协方差出口（只读，最小接口）以启用 tier-1 权重 `w=1/Var(corrected)` ⇒ 让 P2b-1/P2b-2 真正生效。
- **后果**：A 诚实但 Phase2 权重链继续用帧级代理，与 §9.46「Phase2 默认消费稀疏层」的裁决存在张力（稀疏层要进权重分母就需要逐像素方差）；B 工作量中等但使裁决可真正执行。

### D-09 ✅ 已解决（§9.50:1547 补传 + §9.67 定案 7 定用途）：三帧齐备；用途 = 真实信号模板 + 仿真采样帧重建（不是叠加）

> **订正（2026-09-20，V5 分片 3）**：原状态「② 待负责人裁决」作废。依据 §9.50（`GAP_AUDIT.md:1547`「502 我已经补传」；`:1568-1571` 三帧完整）+ **§9.67 定案 7**（`:2596-2602`：「两帧够了」；M16 用途 = 以真实帧**代表真实信号**、建立**仿真采样帧再重建**，**不是**用来做多帧叠加/排异/接缝）。

- **背景**：`m16.zip` 中央目录声明 625,577,079 B，实有 610,546,710 B（**缺 ~14.3 MB**）。按本地文件头绕过提取：`F657N`（Hα）**完整** 268,917,120 B、`F673N`（[S II]）**完整**、`F502N`（[O III]）**截断**（仅恢复 93.3%）。两帧均为 8400×8000 `>f4`、`BUNIT=ELECTRONS/S`、`PHOTFLAM=2.2290223e-18/2.2397195e-18`（**一手绝对流量标定**，非反推）、同一指向、纯 TAN 无 SIP。
- **选项 A（推荐）**：负责人**补传 F502N** ⇒ 三帧齐备，可满足 §9.47 数据类型矩阵的「按波段：Red/Green/Blue/Ha/OIII」要求。
- **选项 B**：确认只用 F657N + F673N 两帧 ⇒ 波段覆盖降为 2，须在论文与测试矩阵里如实登记缺 [O III]。
- **后果**：A 满足矩阵要求；B 使「按波段」这一维在真实数据侧不完整（合成侧可补，但负责人明确要求真实+合成两条腿）。

### D-10 ✅ 已裁决（§9.49 定案 2 / §9.62）：`warn` 仅提示、不参与判据

> **订正（2026-09-20，V5 分片 3）**：原状态「② 待负责人裁决」作废。依据 §9.49 定案 2（`GAP_AUDIT.md:1490-1495`：异光学系统混装**不得报错**、其他合理范围一律接受）+ §9.62（`:2258-2264`：组间门降级为报告字段）。选项 B 已采纳并写死（`design-gap-land.md:127`）。

- **背景**：§9.49 定案 2 明令「不同光学系统的帧混装**不得报错**」「其他合理范围一律接受」；但 §3.5 三级预检规定「不合适（设置/帧不匹配/可优化）→ **warn（不阻塞）**」且页面须显示明细。
- **选项 A**：完全不提示（最严格的帧间独立）。
- **选项 B（推荐）**：报 `warn` 但**仅作提示**，文档明确「该 warn **不参与**标定判据、不影响是否施加 `k_photo`」。
- **后果**：A 最纯粹但用户失去诊断信息（且与 §3.5「不匹配的帧 → warn」的既有条款冲突）；B 保留可观测性且不与定案 2 冲突（不阻塞、不参与判据），代价是必须在最高设计里把「warn 的语义 = 提示，不是判据」写死，防止实现把它重新变成门。

---

## E 审计发现但未定案（11 条）

| # | 事项 | 证据 / 依据 | 建议处置 |
|---|---|---|---|
| **E-01** | **S4 另有 9 条含规范侧成分但未计入「另一类」5 条**：`partial-cli-05`（module.yaml entrypoint/data_contracts 悬挂）、`partial-cli-06`（18_cli.md 悬挂引用，报告原文明写「规范侧悬挂引用」）、`partial-cli-07`（23_hips_browser.md 悬挂引用）、`partial-cli-08`（两份陈旧合同）、`partial-aio-002`（hips_frame 值域自相矛盾）、`partial-aio-003`（合同 const 冻结对角特例 vs FROZEN `C_out=RC_inRᵀ`）、`partial-aio-025`（signal 面亮度单位 ADU vs ADU/px²）、`P3X-13`（FZ-FORMULA-COV-PROP 与生产 `Σc_k²u_k` 冲突）。其中 **3 条报告明确标注需变更 claim / 合同变更流程**（aio-002/003/025） | `CONFORM-SWEEP-4.md:401/409/585/627/635/643/651/383`；JSON 归 C7/C1 | 已逐条登记为 `DG-B-S4X-*`；请负责人确认是否并入「规范侧 34 条」台账（并计 3 条变更流程） |
| **E-02** | **SWEEP-3 报告自身口径不一致**：`§1.2`（md:50）与 JSON `spec_side_issues=9` 记 9 条，但 `§3` 汇总表（md:340-351）实为 **10 行**（多出 **021**，在 §1.2 已按 C3 计一次） | `CONFORM-SWEEP-3.md:50` vs `:340-351` | 建议按 10 条登记（本台账已含 021）或修正报告口径；**不重复计数** |
| **E-03** | **SWEEP-2 的 13 条按「主题」而非条目号列出**：可映射 7 个 ID（SPEC-CONFLICT-UNIT、NS-05、PH-05、PH-06、PR-02、PR-04、DZ-12），2 个主题在本分片无独立条目（`smoothing_lambda` → S3-009/023；Phase3 variance BUNIT → S4），1 个主题（方差低估 36.3% vs 23.3%）只有与 PR-02 同源的实现侧条目。按 ID 口径本分片为 **12 条**，计入 NS-09 的 SPEC-ERR 子项则 13 | `CONFORM-SWEEP-2.md:711-713` | 建议在报告侧补 ID 映射表；本台账已按 ID 逐条登记并标注 |
| **E-04** | **`s>1e-12` 归一化门两处**：`upm.cpp:646` 已修为尺度无关门（FIX-UPMSCALE，`reports/RELEASE-02/fix-upmscale-report.md` 是唯一记录）、`upm.cpp:1646`（`p2_upm_normalized_weights`）**仍为 1e-12** ⇒ 生产 `control_ivar` 尺度（中位 5.6e-22）下全部权重归 0 | `CONFORM-SWEEP-3-003`/`019`；§9.17 | 属实现缺陷（S3-003，C1）未定批次；文档侧拆分见 `DG-B-S3-019` |
| **E-05** | **`k_corr = 1.4` 的 MC 测试是构建孤儿**（`control_median_mc_test` 未登记 CMake/ctest/CI）⇒ 该常数的证据不可复现；而所有 `control_variance` 相关量按 `k_corr` 线性缩放 | `snr-propagation-design §7.2 P2-11`、`§9 U6` | 恢复该测试或按 P2-11 登记；属实现/CI 面 |
| **E-06** | **母版方差未传播**（`CALIBRATION.md:236` 登记 UNRESOLVED 且**无项目公式**）；`V(y_p)={V(r)+V(b)+α²[V(d)+V(b)]+y_p²V(f)}/f²` 的系统项缺失 | `snr-propagation-design §7.1 P1-11`、`§9 U7` | 需变更 claim + 实现；本轮只登记 |
| **E-07** | **drizzle 后相关噪声只文档化不存**（`UNCERTAINTY_AND_COVARIANCE.md:15-22`）⇒ 方差低估 **36.3%**（σ 20.2%）。**注意 SNR-EXP-AUDIT 的复核更正**：成品 σ 相对误差是 **31.3%**（不是前台曾说的 131%）；修好后 **20.2%–28.6%**（`eps_drizzle=20.2%` **从未进入求和**） | `snr-propagation-design §7.1 P1-12`、`§13`；§9.48「前台自身错误结论」 | 与 PR-02/partial-aio-003/P3X-13 同族，三处应同批裁决 |
| **E-08** | **八投影冻结集合仅实现 4/8**，且 v3 registry **未编入生产 target** | `CONFORM-SWEEP-4 P3P-03`（md:179-186） | 属 C3 未接线；与 `CHK-SPEC-NAMED-IMPL-ON-PROD-PATH`（DG-C-01）联动 |
| **E-09** | **整个 V6 三模式 Phase3 导出链未编入生产 target** ⇒ `FZ-P3-MODES`/`FZ-P3-QW-RECOMPUTE`/`FZ-P3-BUNIT-QUADRATIC`/`FZ-P3-KERNEL-REGISTRY` 四条 FROZEN 合同在生产运行面**无载体**，而 `PRODUCTION_EXECUTION_INVENTORY.csv:335` 反标 `production=yes` | `CONFORM-SWEEP-4 P3X-06`（md:54）、`P3X-12`（md:375） | P0 级；与 DG-C-01 门禁联动（这正是该门要抓的缺陷类） |
| **E-10** | **真实数据 `ivar`/`snr` 全为 0 stub** ⇒ Q2 的权重链无法读取，必须改用 `1/uncertainty²`（中位 4.23e10） | `GAP_AUDIT §9.30 ⑥`（:843-844） | 属 FIX-P2b-3（`ivar`/`snr`=0 stub 修复），未落地 |
| **E-11** | **前台自身错误结论已被 SNR-EXP-AUDIT 推翻（须更正，不得再引用）**：成品 σ 相对误差 131%→**31.3%**；修好后 3.7%→**20.2%–28.6%**；`eps_theta` 3.4%/2200%→**+20.1%/+608.6%**；`(N+1)/(N−1)` 确认生产 exclude-self **不成立**（那是自含均值算子；exclude-self 低估 1.6%）；EXP-3「帧标量最优 0.06%、稀疏净亏 3.3%」**方向对、数字不成立**（0.063% 是窗口产物，换 2048² 即 4.42%；物理真值下净亏 **0.175%**）；ℓ≈48px → **40.4–57.9px 区间**；推荐 kriging → **真实数据上双线性更优** | `GAP_AUDIT §9.48`「前台自身错误结论」（:1440-1449）；`snr-propagation-design §13.7/§13.8` | 论文与文档引用这些数字时**必须用复核后的值**；旧值不得再出现在新增文本中 |

---

## F 统计

### F-1 各 target 条目数

| target | 条数 | 构成 |
|---|---|---|
| **design**（`ASTROCS_DESIGN.md`） | **16** | A-01..A-16 |
| **doc**（系列文档） | **65** | B-I 主线 **16** + B-II 审计规范侧 **43**（S1 7 + S2 14 + S3 9 + S4 5 + S4 另 8）+ B-III 登记 **6** |
| **gate**（门禁） | **5** | C-01..C-05 |
| **合计** | **86** | —— |

> B-II 的 43 条 = 任务要求的 **34 条全部覆盖**（S1 7 / S2 13 / S3 9 / S4 5），另加 **9 条**如实标注「报告未计入其分列」的条目：
> **S3-021**（报告 §3 表有、9 条分列无）+ **S4 的 8 条含规范侧成分项**（`partial-cli-05/06/07/08`、`partial-aio-002/003/025`、`P3X-13`）。
> 分节计数：S1 **7** + S2 **14**（13 条分列中 `sparse_snr_layer`/`smoothing_lambda` 归 B-III、Phase3 BUNIT 归 S4，另加 4 条未计入分列项）+ S3 **9**（含 021；023 归 B-III-03）+ S4 **13**（5 条「另一类」+ 8 条未计入项）= **43**。

### F-2 需变更 claim / 走变更流程的条数

**33 条**（`needs_claim=true`）：

| 类别 | 条数 | 条目 |
|---|---|---|
| 最高设计增补 | **13** | DG-A-01、DG-A-02、DG-A-04、DG-A-06、DG-A-07、DG-A-08、DG-A-09、DG-A-10、DG-A-11、DG-A-12、DG-A-14、DG-A-15、DG-A-16 |
| 文档侧 | **20** | DG-B-08（claim 命名订正）、DG-B-09、DG-B-10、DG-B-11、DG-B-12、DG-B-15、DG-B-S1-024、DG-B-S2-DZ-07、DG-B-S2-DZ-08、DG-B-S2-UNIT、DG-B-S2-PH-05、DG-B-S2-PH-06、DG-B-S2-PR-02、DG-B-S2-PR-04、DG-B-S2-DZ-12、DG-B-S3-020、DG-B-S4X-AIO02、DG-B-S4X-AIO03、DG-B-S4X-AIO25、DG-B-S4X-P3X13 |
| 门禁 | **0** | 门禁条目均为工程面，不需 claim |
| **合计** | **33** | —— |

> 精确清单以 `design-gap-synthesis.json` 的 `needs_claim=true` 过滤为准（33 条）。

### F-3 待裁决 / 未定案

| 类别 | 条数 | 说明 |
|---|---|---|
| **② 待负责人裁决**（**D 节，跨条目的决策项**） | **1** | **仅 D-08**（`uncertainty_available` / W2 协方差 API；§9.66 B 同族）。**订正（2026-09-20，V5 分片 3）**：原「**10** | D-01..D-10」作废 —— 其余 9 条已裁决，逐条出处见 F-3 表下注 ① |
| **③ 审计发现但未定案**（**E 节 11 条**） | **41** | 台账条目 `status` 标 ③ 的条数；E 节为其中的**决策级归纳**（11 项） |
| **① 已裁决待落地** | **35** | 台账条目 `status` 标 ① |
| **④ 已订正/已裁决/已撤销（仅登记）** | **6** | `DG-B-III-01`..`06` |
| **合计** | **86** | **38 + 1 + 41 + 6 = 86**（`status` 口径；**订正（2026-09-20，V5 分片 3）**：`DG-B-11`/`DG-B-S2-PH-05`/`DG-B-S2-PH-06` 由 ② → ①（G21/G22/G29/G30），原「35 + 4」作废；`DG-B-10` 仍标 ②，见注 ②） |

> **两种口径不要混用**：`status` 是**逐条目**状态（合计 86）；D/E 节是**决策级**清单（D 10 项、E 11 项），一个决策项可覆盖多个台账条目（例：D-03 覆盖 `DG-A-11` 与 `DG-B-10`）。
>
> **注 ①（2026-09-20，V5 分片 3）D 节逐条出处**：D-01/D-04/D-10 = §9.49 定案 2 + §9.62；D-02 = §9.50 定案 7 + §9.60 + §9.67 定案 4；D-03 = **§9.67 定案 5**（`:2584-2588` 本期不加堆叠平滑项 + 必须做实验验证）；D-05 = §9.50 定案 3 + §9.51 Q-C/§9.52 + **§9.67 定案 6**（0.03 mag 作废、须误差预算推导）；D-06 = **§9.67「前台须同时执行」第 4 条**（`:2608` `frame_snr` 改名，纯命名消歧）；D-07 = §9.55 S5（⚠ 与 §9.67 定案 5 是否同指未判定）；D-09 = §9.50 补传 + **§9.67 定案 7**（用途 = 真实信号模板 + 仿真采样帧重建，不是叠加）。
> **注 ②（冲突登记，未改）**：`DG-B-10`（md:285）/`DG-A-11`（md:162）的 `status` 仍写「② 待负责人裁决（…D-03）」，而 D-03 已由 §9.67 定案 5 裁决（本期不加 + 实验义务）⇒ 该两行 status **已过期但本轮未改**（B 报告 G27/G28 判 KEEP，本分片纪律「只做 DELETE 项」）——**请前台/后续分片按 §9.67 定案 5 订正**。

### F-4 按优先级

| 优先级 | 条数 |
|---|---|
| P0 | 14 |
| P1 | 40 |
| P2 | 26 |
| —（登记项） | 6 |

---

## G 我认为最该先做的 5 条

1. **`DG-A-05`（最高设计 §4.2:236 的 `g·s+b` → 纯加性）** —— 一行 mermaid 的改动，但它是**全仓唯一还写着乘性模型的权威位置**；不改则任何按最高设计实现的人都会复活 `g_k`，与 D-1/A2 裁决直接对撞。零成本、零歧义、立即消除最高设计级矛盾。
2. **`DG-C-01`（`CHK-SPEC-NAMED-IMPL-ON-PROD-PATH`）** —— 本轮 S1-002、S2-NS-01、S4-P3X-06 **三条同类缺陷**（检测器、噪声模型、V6 导出链）全部落在同一个门禁盲区；§9.44 已明令新增该门。一条门一次性覆盖三条 P0/C6 缺陷类，是投入产出比最高的防复发动作。
3. **`DG-A-07` + `DG-A-01`（最高设计补「应用测光归一化到像素」节点 + 检测改星表引导）** —— 这是两条 P0 的实现前提：`apply_photometry` 零调用者之所以能长期存活，正是因为**设计图上没有这个节点**；而星表引导一次解决 34.5 万假源、星云区虚警、拟合失败语义与资源开销四个问题，并顺带消灭「切不切 sdet」的大部分动机（D-04）。
4. **`DG-A-08`（合成测试必须模拟真实物理噪声过程，进 §11.1）** —— 这是**所有**后续 Oracle 的有效性前提。已实测：纯加性天光下真值无效应时度量恰为 0.0000%（什么都测不出），而光子域同样天光给 −7.81%/−20.33%/−37.19%。不先把这条写进最高设计，A4/A5/A6 的实验会继续产出无效结论。
5. **`DG-B-III-02` + `DG-B-01/02/03`（`sparse_snr_layer` 已订正的登记 + 3 处书写形态）** —— 这是**最便宜**的一组：语义与配置/schema/登记册**已落地**，只剩 3 处 Markdown 书写形态（`**true**`×2 + 字段列括注×1），改完即消除 SNR 面仅剩的 CFG002-01 红灯。同时必须把 B-III-02 的「已订正」状态广播出去，防止后续分片把它当待办重做。

> 紧随其后（第 6–8 位）：`DG-C-04`（GUARD-TOOLS-FIX 提交 + 注册，工作区已重写未提交）、`DG-B-S3-024`（`UNIFIED_SCIENCE_MODEL.md:122` 的 UNRESOLVED 同步关闭，纯文档）、`DG-B-06`（`CONFIG_CONTRACT.md` 快照 50→53 + 2 键 + 重锚）。

---

## H 复现与追溯

- 台账：`reports/RELEASE-02/design-gap-synthesis.json`（`schema = astrocs.release02.design-gap-synthesis/v1`，86 条，字段 `id/target/file/loc/current/proposed/rationale/basis/needs_claim/priority/status`）。
- 本报告所有 `ASTROCS_DESIGN.md` 行号、`docs/**` 行号、`ci/**` 行号均为本次只读核对所得；`GAP_AUDIT` 行号格式为 `:NNN`，`CONFORM-SWEEP-N` 行号格式为 `md:NNN`。
- 规范侧 34 条的抽取由**独立子代理**并行完成并与本分片自身阅读**逐条交叉核对**；两处口径差异（S3 的 9 vs 10、S2 的主题 vs ID）已在 **E-02/E-03** 如实登记，未做平滑处理。
