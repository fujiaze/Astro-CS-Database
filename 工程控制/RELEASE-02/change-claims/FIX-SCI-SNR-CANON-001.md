# 变更 claim：FIX-SCI-SNR-CANON-001 — 负责人最高设计意图固化：UPM 纯加性、SNR 三路径/默认稀疏/产物边界、帧级 SNR 红线、像素剔除自研档

- 控制包：RELEASE-02 / 任务 CANON-FREEZE（设计意图固化分片）
- 变更对象（本轮已订正）：
  - `docs/science/PHASE2_UPM.md`（SCI-UPM-001，FROZEN）——关闭 §14a 的 UNRESOLVED，写入纯加性本期决议与理论依据；
  - `docs/plugins/algorithms_phase2/11_upm.md`、`docs/plugins/algorithms_phase2/10_sampling.md`——目标态由「乘性+加性」订正为**纯加性**；
  - `docs/plugins/algorithms_phase1/07_noise_snr.md`——SNR 三条路径 / 默认稀疏 / 配置键设计 / Phase1 唯一 SNR 产物 / 帧级 SNR 红线；
  - `docs/design/UNIFIED_MODEL.md`——数据对象表同步（frame_snr 语义、sparse_snr_layer 定位、SNR 路径对象）；
  - `docs/science/CONTROL_WEIGHT_SNR.md`、`docs/science/PSF_SIGNAL_WEIGHT.md`——两篇权威互斥定案（同名两义分离）；
  - `docs/science/PHOTOMETRY.md`（SCI-PHOT）、`docs/plugins/algorithms_phase1/06_photometry.md`、`docs/algorithms/CALIBRATION_ALGORITHMS.md`——**测光标定语义**：消除物理单位、只使用星等，`k_photo` 绝对值无物理意义，禁止物理闭合反推（§3.6）；
  - `docs/science/REJECTION.md`（SCI-REJ-001，FROZEN）及 `docs/algorithms/PHASE2_REJECTION.md`、`docs/algorithms/REJECTION_ALGORITHMS.md`、`docs/algorithms/PHASE2_MOSAIC_WRITE.md`、`docs/contracts/PUBLIC_API.md`、`docs/contracts/DATA_SEMANTICS.md`、`docs/validation/SCIENCE_FREEZE.md`、`docs/development/CONFIG_SCHEMA.md`、`docs/modules/phase2_rej.md`、`docs/modules/registry/astrocs.phase2.{reject,write}.md`——排异 canonical profile 订正为自研 `astrocs_adaptive_pixel`。
- 关联条目：
  - 裁决记录 `工程控制/RELEASE-02/GAP_AUDIT.md` §9.37（:1003-1030）、§9.38（:1031-1069）、§9.39（:1070-1113）、§9.40（:1114-1142）；
  - `工程控制/RELEASE-02/change-claims/FIX-A-UPM-001.md`（**被本 claim 取代 / superseded**，见 §2）；
  - `OPEN-P2S-02`（`docs/contracts/v6/frozen/astrocs.v6.contract-freeze.v1.json:2425`，"UPM 乘法尺度 g_k 的生产数据面/schema 与空间模型表示"）——本 claim 只关闭其**模型语义**部分，数据面/schema 闭合仍走合同流程；
  - 分片 FRAME-SNR-CANON（帧级 SNR 文献调研，结论未到，见 §6）。
- 日期：2026-09-19
- 依据条款：`ENGINEERING_SPEC.md` §3（科学正确性优先 + 变更 claim + 一致性回归）；`AGENTS.md` §8（科学疑义查证流程）；`工程控制/RELEASE-02/00_README.md` §3a（自主裁决授权与留痕要求）
- 状态：**文档订正已落地**；实现/配置/schema 跟随项**登记未实现**（见 §4、§6）

## 1 裁决原文（负责人逐字，不得改写）

- **D-1 / A2 UPM 模型 = 纯加性**：
  > 「我认为**纯加**。因为如果前面的步骤没有问题的话。阶段1产物应该就是**本身就在一个测光体系的真信号**，
  > 和**可以等效为加性的天光**，残留天光**无论是加还是乘，都可以用加法移除**。」
- **D-2 / A3 SNR 三条路径 + 默认稀疏**：
  > 「3.**这是最关键的。应该在论文中体现。**
  > **比较稠密的 SNR，稀疏 SNR 重建为稠密，和单帧级 SNR 重建为稠密的精度。**
  > **可以在算法中保留三种路径。** 而我们为了**取舍存储量和精度折中**选择，**默认使用稀疏 SNR**，
  > 可以在**配置文件 JSON 中显式手动指定**。」
- **D-3 / B1 SNR 产物边界**：
  > 「phase2 产物**不输出**，在 phase2 **叠加中消费掉单帧 SNR** 了。phase2 **不会直接用之前的**。
  > 而**论文任务应该研究不同情况对 phase2 产物重新计算 SNR，哪个高。效果对比。**
  > phase3 自然也没有了。**只有 phase1 的输出 HiPS 里带有 SNR 数据块。**」
- **D-4 / A1 帧级 SNR 语义**：
  > 「a1. 你是指**帧级 SNR**吗，就是这一帧的。这种不就是 **PSF signal SNR** 吗，
  > **去调研各类科学文献都在用什么，记录并且确定**。
  > 我**唯一的要求**就是这个必须是**纯信号/噪声**，**不能是被天光等「假信号」抬高的假信噪比**。」
- **C1 SNR 交叉验证判据**：
  > 「C1，我需要的是**能代表真实信号和噪声比例的 SNR**，**不局限于任何一种方法**，而是要求是**真实信号/噪声**。」
- **C2 像素剔除算法 = 自研的 ⇒ 改文档对齐代码**：
  > 「C2，**我自研的**」
- **C3 关闭 UPM 纯加性未决标记**：
  > 「C3 **同意**」

（以上逐字来源：`工程控制/RELEASE-02/GAP_AUDIT.md` §9.38/§9.39/§9.40，:1035-1066、:1100-1110、:1116-1127。）

## 2 supersede：否决 FIX-A-UPM-001 的提议方向

`FIX-A-UPM-001` 提议把 `docs/science/PHASE2_UPM.md` 的冻结模型从**纯加性**订正为**乘性 + 稀疏天光面**
（`y_k(x)=g_k·s(x)+b_k(x)`，生产式 `s(x)=(y_k(x)−C_k(x)−b_k(x))/g_k`，恢复 `g_k`，`g_ref=1`）。
负责人 A2 裁决**恰为相反方向**（纯加性），故：

- **FIX-A-UPM-001 的模型订正提议被否决（superseded by FIX-SCI-SNR-CANON-001）**；
- 其 §3 表中「恢复 g_k」「新增空间天光面取代加性场」两行**不采纳**；
- 其证据面（§2.1 文献、§2.2 开源对照、§2.3 可辨识性数值推导）**不因此作废**：它们仍可作为
  「加性天光面的稀疏表示（`B_ref`+逐帧 `δ_k`）如何拟合」的表示层证据，但**不得**用于支撑乘性模型；
- **否决理由（负责人给出的物理依据）**：Phase1 正确归一化后，帧本身已是**同一测光体系的真信号**加
  **可等效为加性的天光**；残留天光不论原始是加性还是乘性，**都可用加法移除**；乘性残留属**低阶空间增益**，
  已归 Phase1 处理（`I_photo = k_photo·m(x,y)·I_cal`），不属 Phase2；
- 与既有实现事实一致：FIX-P2a 默认路径为「保留 C 去 δ，`raw − C_k`」，`g_k ≡ 1` 本期不启用，
  `÷g²` 保持**恒等式**（`Var(corrected)=[σ_raw²+J_out C_θ J_outᵀ]/g²`，`g≡1` 时退化）。

## 3 逐条裁决 → 订正决定

### 3.1 D-1 UPM = 纯加性（含 C3 关闭未决标记）

- 冻结模型**不变**：`calibrated_f(p) = raw_f(p) − C_f(p)`（8×8 control cell 双线性加性场），
  `C_f(p)` 即加性天光面；**非目标**「不处理乘性尺度差」保持有效；
- `g_k ≡ 1` **本期不启用**；`÷g²` **保持恒等式**（公式与实现不删，退化等价）；
- 理论依据写入 SCI 文档：Phase1 归一化后帧 = 同测光体系真信号 + 等效加性天光；乘性残留 = 低阶空间增益，
  归 Phase1 `I_photo = k_photo·m(x,y)·I_cal`；Phase2 只做加性扣除 `raw − C_k`；
- `PHASE2_UPM.md` §14a 的 UNRESOLVED **关闭**为「本期决议：纯加性」；
- 插件目标态「乘性+加性」表述订正（`11_upm.md`、`10_sampling.md`）。

### 3.2 D-2 SNR 三条路径 + 默认稀疏

三条路径（全部保留在算法面，配置显式可选）：

| 路径 id（设计值） | 含义 | 存储 | 精度/成本定位 |
|---|---|---|---|
| `dense` | Phase1 直接产出稠密逐像素 SNR 面 | 最大 | 精度基准（论文对照组） |
| `sparse_reconstruct`（**默认**） | Phase1 产出稀疏控制点 SNR 层，Phase2 重建稠密 | 小 | 存储/精度折中（负责人选定默认） |
| `frame_reconstruct` | Phase1 只出帧级标量，Phase2 重建稠密 | 最小 | 单帧级对照（论文对照组） |

- 配置键设计（**须由配置文件 JSON 显式手动指定**；本轮只固化设计，未改配置模板/schema）：
  - Phase1（normalize）：`sparse_snr_layer`（bool，默认 **true**——默认稀疏路径要求稀疏层默认产出）、
    `sparse_snr_density`（点/度²，**仍 pending_authority**，见 §6）；
  - Phase2（mosaic）：`algorithm_snr_path` ∈ {`dense`, `sparse_reconstruct`, `frame_reconstruct`}，默认 `sparse_reconstruct`；
  - 消费侧行为：输入无稀疏层而路径为 `sparse_reconstruct`（含默认）⇒ 按帧级执行但**必须显式记录实际路径**（`snr_path_effective`）并计数，**不得静默**；稀疏层存在但损坏/不可重建 ⇒ fail-closed；
- **论文核心实验**：同条件比较三条路径重建稠密 SNR 的精度（判据 **SP-0**）；
- **诚实约束（实测事实，不得预设稀疏最优）**：同条件帧上**帧级标量已最优到 0.06%**（稀疏层净亏 3.3%）
  ⇒ 文档**不得**预设「稀疏一定最好」，须写明判据 SP-0 与「何时哪种最优」由实验回答
  （来源：`reports/RELEASE-02/q2-snr-smooth.md` 及 SP-0 相关实测记录）。

### 3.3 D-3 SNR 产物边界

- **只有 Phase1 的 HiPS 带 SNR 数据块**（帧级 + 稀疏区域，`sparse_snr_layer=true` 时）；
- **Phase2 不输出 SNR 面**；Phase2 在**叠加中消费单帧 SNR**（现场换算逆方差权重），**不直接复用** Phase1 的 SNR 产物；
- **Phase3 无 SNR**；
- **论文任务**：对不同情况**对 Phase2 产物重新计算 SNR**，比较哪个高、效果对比；
- 前台此前「成品输出 SNR 面」的建议**撤销**（属顶层合同变更，负责人**不批**）⇒
  `docs/contracts/DATA_SEMANTICS.md` 目标合同**只冻结 variance/ivar**，本 claim 不改该合同面。

### 3.4 D-4 帧级 SNR 语义红线（A1/C1）

- **硬红线**：`SNR = F_signal / σ_F`，`F_signal` **必须已扣局部背景**；天光**只作为噪声项**进入 `σ_F`；
- **必须可证明**：固定源通量、增大天光 ⇒ SNR **单调下降**（`B→∞` 时 `SNR→0`）；
- 帧级 SNR 是**点源（PSF）**量，**不得**与面亮度 SNR 混用或互相宣称等价；
- **两篇权威互斥定案**（`CONTROL_WEIGHT_SNR.md:11-14` vs `07_noise_snr.md:39`）：
  冲突根因是**同名两义**，不是公式分歧 ⇒ 分离为两个对象：
  - `frame_snr`（Phase1 HiPS 文件头的科学量）= **帧级未加权原始信噪比**，通量型口径 `F_ref/σ_F`，
    信号来自 PSF/孔径混合测光**减独立局部背景**，天光散粒噪声计入 `σ_n`；即 D-4 的 PSF signal SNR；
  - Phase2 stage2 内部的 `local_snr` / `frame_snr_medians`（回退标量）= **相对质量权重场**
    （`quality_weight`，无量纲，非科学信噪比），仅 `weight_mode=0` legacy/诊断消费；
  - 二者**禁止同名互指**；stage2 侧字段改名 `quality_weight`（`snr_v` 为别名）登记为实现跟随项（§4）；
- **具体定义式**：`F_signal` 的测光口径（孔径/PSF 总通量/曲线增长）与 `σ_F` 的稳健噪声估计式
  **待分片 FRAME-SNR-CANON 文献结论补入**（§6）；本 claim 先固化**红线与方向**，
  并把「必须满足注入-回收（已知真值信号+已知天光+已知噪声 ⇒ 回收 SNR = 真值 `F_s/σ_F`）」定为验收方式（C1）。

### 3.5 C2 像素剔除 canonical profile = 自研 `astrocs_adaptive_pixel`

- **生产默认 `reject_profile = astrocs_adaptive_pixel`（AstroCS 自研）**；
  证据：`lib/infrastructure/scheduler/src/module_adapters.cpp:5952`（`doc.value("reject_profile", std::string(P2_PROFILE_ASTROCS_ADAPTIVE_PIXEL))`）、
  `lib/algorithms/coverage/include/astro/phase2/rejection.h:87`（`#define P2_PROFILE_ASTROCS_ADAPTIVE_PIXEL "astrocs_adaptive_pixel"`）、
  `lib/algorithms/coverage/src/rejection.cpp:1101-1119`（`astrocs_n_map_method`）、`:1140-1146`（白名单）；
- **科学依据**（不新增阈值，全部继承 SCI-REJ §5 冻结锚点）：
  1. 路由以**逐输出像素几何 `n`（nominal contributors）一次解析**为唯一依据，禁止 per-pixel `n_eff` 重选（SCI-REJ §7 阈值不变量）；
  2. 内置映射（`rejection.cpp:1101-1119`）：`n≤3 → none`（保守：不排异 + 直接加权积分，provenance 如实记
     `underdetermined_no_rejection`/`UNDERDETERMINED`，**不为低 n 发明排异方法**）；`4..7 → percentile`（low 0.2/high 0.1）；
     `8..15 → winsorized_sigma`（4.0/3.0/8 iter）；`n≥16 → linear_fit`（5.0/3.5/8 iter）；
  3. 与 WBPP 的**三处明示偏离**：`n≤3` 用 none（WBPP auto 会套 percentile）；`6≤n≤7` 用 percentile
     （消解 WBPP auto 与 validator 自相矛盾）；`16≤n<20` linear_fit 由调用方发 WARN；
  4. `wbpp_2_9_1` / `astrocs_adaptive` 的冻结 AUTO 路由**逐字未变**，保留为**对照档/回归基线**
     （阈值表来源仍如实标注为 WBPP 2.9.1 `bestRejectionMethod`，非学术文献）；
- **适用域**：像素候选栈域、逐输出像素几何 `n`；`n≥4` 才声明排异能力，`n≤3` 显式不声明（recall=0）；
  空间生长仅 `large_scale` 结构（trail），compact cosmic 不生长；`extreme_value_clip_prior_sigma` 为**显式 opt-in**、
  永不参与 AUTO 路由；
- 文档订正：SCI 与合同文档中「生产默认 auto + profile: `wbpp_2_9_1`」的表述改为
  「生产默认 `astrocs_adaptive_pixel`（自研）；`wbpp_2_9_1` 为对照档」；
- **登记未改**：`contracts/data/phase2_uncertainty_rejection_provenance_v1.json:67`、
  旧工具默认 `lib/algorithms/coverage/include/astro/phase2/stage2_common.h:69`（`wbpp_2_9_1`）
  ⇒ 见 §4 影响面（本轮不改代码/合同 JSON）。

### 3.6 测光标定语义：消除物理单位、只使用星等（负责人 2026-09-19 纠正，GAP_AUDIT §9.42）

> 「**显然是错误的。我给你的 m42 数据口径是半米。而且不要用这类公式。**
> 正是因为 **fits 文件头里面没有办法获得 adu，口径之类的具体数据**。
> 所以我才要求**使用星点光通量积分和 gaia 数据库 + CCDQE 和滤镜透过率曲线**
> **将图像标定到真实测光坐标系，消除物理单位，只使用星等**。」

- **标定目标 = 真实测光坐标系（星等）**；手段 = 星点光通量积分 + Gaia + CCD QE + 滤镜透过率曲线；
- **消除物理单位，只使用星等**；`k_photo`（及 `scale`/`a_k`）的**绝对值无物理意义**——吸收增益/口径/曝光等未知量；
- **禁止**任何物理闭合式（如 `k = g·h·c·1e9/(A·t)`）反推仪器参数或论证标定因子合理性；**禁止**为标定因子设绝对窗口；
- **有意义的判据只有两个（均尺度无关）**：① 测光一致性（施加后星点**星等**与 Gaia 残差散度/MAD 小）；② 帧间一致性（各帧落**同一测光体系**）；
- 与本 claim 的关系：D-1 中「乘性残留归 Phase1 低阶空间增益」的 `k_photo` **只作为相对标定因子**出现，其绝对量值不作为任何判据；D-4 的帧级 SNR「纯信号/噪声」要求**不变**，但**不得**用物理单位闭合论证。

## 4 影响面（本轮只登记，不实现）

### 4.1 配置 / schema（**须跟随**）

| 面 | 现状 | 须做 |
|---|---|---|
| `config/templates/normalize.phase_config.json` | `"sparse_snr_layer": false` | 默认稀疏路径 ⇒ 改 `true`；并补 `sparse_snr_density`（数值待 §6 定案） |
| `config/templates/mosaic.phase_config.json` | 无 SNR 路径键 | 增 `"algorithm_snr_path": "sparse_reconstruct"` |
| `config/defaults.json` | 有 `sparse_snr.density`（`pending_authority`） | 增 `snr.path`（默认 `sparse_reconstruct`）与 `sparse_snr_layer` 默认值登记；**须守 `authority.transcription_rule`**（值只能转录自 `docs/science|docs/algorithms`）⇒ 先由 SCI 侧承接 |
| `contracts/schemas/phase_config_{normalize,mosaic}.schema.json` | 无 SNR 路径键 | 增字段 + enum（`additionalProperties` 约束下不得静默忽略） |
| `ASTROCS_DESIGN.md` §3.3 示例 JSON | `"sparse_snr_layer": false` | 与默认稀疏裁决矛盾；**属根级最高设计，超出本分片文件域**，须前台/负责人授权后订正 |

### 4.2 代码（**须跟随**，本轮未改）

- `lib/algorithms/coverage/include/astro/phase2/stage2_common.h:69`：工具默认 `wbpp_2_9_1` ⇒ 与生产默认
  `astrocs_adaptive_pixel` 分歧（`CHK-CONFIG-DEFAULTS` 的 divergent 面，已有登记）；
- Phase1 SNR：三路径的产出/重建实现（dense 面、稀疏层重建、帧级重建）与 `algorithm_snr_path` 消费；
- Phase2：确认「不输出 SNR 面」；SNR 消费点为叠加现场换算 `w=1/σ_F²`；
- stage2 字段改名：`local_snr`/`frame_snr_medians` ⇒ `quality_weight`（`snr_v` 别名），与 `frame_snr` 科学量分离；
- `lib/algorithms/coverage/src/stage2_common.cpp:258-265`：错误串字面量拼接缺分隔（既有缺陷登记），随改名一并修；
- **标定因子验收面（§3.6）**：删除/否决任何 `k_photo`/`a_k`/`scale` 的**绝对窗口守卫**（如 `k_photo ∈ [0.1,10]`；`ESC-P1-PHOT-ABS-WINDOW` 登记据此裁决），改用**尺度无关**判据（测光一致性 + 帧间一致性）。

### 4.3 合同 / 产品

- `docs/contracts/DATA_SEMANTICS.md`：目标合同**只冻结 variance/ivar**（D-3 明确不批 SNR 面），本 claim 不改该冻结面；
- `contracts/data/phase2_uncertainty_rejection_provenance_v1.json:67` 的 `source` 串仍写 `wbpp_2_9_1` ⇒ 须改
  `astrocs_adaptive_pixel`（合同 JSON 超出本分片文件域）；
- `OPEN-P2S-02`：模型语义部分随本 claim 关闭；数据面/schema 部分仍待合同流程。

### 4.4 测试（**须跟随**）

- SNR 三路径精度对比（论文核心实验，判据 SP-0）：正例（三路径同输入可重建）+ 负例（无稀疏层而路径为
  `sparse_reconstruct` 时静默降级必须判红；稀疏层损坏/不可重建必须 fail-closed）；
- 帧级 SNR 注入-回收：已知 `F_s`、已知天光 `B`、已知噪声 ⇒ 回收 SNR 必须等于 `F_s/σ_F`；
  **天光单调性负例**：固定 `F_s`、`B` 增大 ⇒ SNR 单调下降，`B→∞` 时 `SNR→0`（判红当前任何「被天光抬高」的实现）；
- 面亮度/点源口径分离负例：把面亮度 SNR 当帧级 SNR 使用必须判红；
- **物理单位反推负例（§3.6）**：任何以物理闭合式（`k = g·h·c·1e9/(A·t)` 之类）论证标定因子合理性、或反推口径/增益/曝光的分析、测试或门禁，必须判红；标定因子验收只用「星等残差散度/MAD」与「帧间同一测光体系」两个尺度无关判据；
- 排异：`astrocs_adaptive_pixel` 的 `n≤3 → none` 保守档与 provenance
  `underdetermined_no_rejection` 正/负例；冻结 `wbpp_2_9_1` 路由回归保持不变。

## 5 一致性回归

见 `reports/RELEASE-02/canon-freeze.md` §4（逐项命令、原始输出与结论），含
`CHK-REGISTRY-DOC-SYNC`、`CHK-CONFIG-DEFAULTS`、`CHK-DANGLING`、`CHK-SCI-REF`（含
`DOC-LINE-ANCHORS`）、`tools/science_contract_lint.py`、`工程控制/RELEASE-02/verify_doc_pack.py`。

## 6 未闭合项（显式登记，不猜测）

1. **帧级 SNR 具体定义式**：待分片 **FRAME-SNR-CANON** 文献调研结论补入（本 claim 只固化红线与方向：
   纯信号/噪声、`F_signal` 已扣局部背景、天光只进 `σ_F`、单调性、点源口径、不得与面亮度 SNR 混用）；
2. **A6 `F_instr` 测光口径**：负责人要求「查论文，科学软件算法等」定案，是 A4 的前置条件；
   本 claim 涉及星点通量口径处**只登记待 A6 定案，不自行定案**；
3. **`sparse_snr_density` 数值**：仍 `pending_authority`（`config/defaults.json` 已登记）；
   在数值定案前，Phase1 生产默认启用稀疏层会缺节点密度 ⇒ 属**联锁缺口**，不编造数值；
4. **`ASTROCS_DESIGN.md` §3.3 示例与合同 JSON / 配置模板**：见 §4.1/§4.3，超出本分片文件域，须前台执行。

## 7 状态

- 文档订正：**已落地**（本 claim，含 `FIX-A-UPM-001` 的 supersede 标注）；
- 配置/schema/合同 JSON/代码/测试跟随项：**未落地，已登记**（§4）；
- 未闭合项：**已登记**（§6），不得据本 claim 宣称已闭合。
