# 科学集（docs/science）

> 上游：`docs/ASTROCS_DESIGN.md` §1（项目定位）、§2（核心科学方法）。
> **科学主张以本集为权威**；与最高设计冲突时以最高设计为准（`docs/ASTROCS_DESIGN.md` §0.1/§0.2）。
> 佐证要求见 `docs/engineering/DOCUMENT_GOVERNANCE.md` §2（科学佐证纪律，全局）。

## 1 这一集是什么，不是什么

**是**：一级科学集。科学主张的正本——公式、常数、判据、容差、冻结定义；
数据语义（这批字节代表什么物理量、单位、哪个键对应哪个量）；
以及支撑这些主张的证据材料（研究包与文献档案）。

**不是**：

- 不是实现说明与逐符号实现锚定，那些在 `algorithms/` 子目录与 `docs/detail/`。
- 不是架构裁决与工程门禁，那些在 `docs/engineering/`。
- 不是实验数值产出，那些在 `实验/`。

**硬边界一句话**：本集说「世界是怎样的」，`docs/engineering/` 说「我们约定怎么做」。

## 2 篇目清单

### 2.1 统一模型与范围

- `UNIFIED_SCIENCE_MODEL.md` —— 跨阶段统一科学模型：量纲、信号、方差语义总纲。
- `SCIENCE_SCOPE.md` —— 科学范围与核心科学方法的边界。

### 2.2 数据语义与外部格式接口

- `DATA_SEMANTICS.md` —— 产品与中间量的语义权威：每个键代表什么物理量、什么单位、取值域。
  **它是机器校验面**：门读的是本文件，因此本文件的错会直接变成门判错。
- `IO_001_FITS_STREAM_INTERFACE.md` —— FITS 流式读写接口的语义约定。
- `IO_002_HIPS_INPUT_INTERFACE.md` —— HiPS 输入接口的语义约定。

### 2.3 公式正本

- `CALIBRATION.md` —— 定标科学公式（bias/dark/flat）与不确定度传播。
- `STAR_DETECTION.md` —— 星点检测定义与检测阈值口径。
- `PSF.md` —— PSF 模型、FWHM 与质心/形状参数定义。
- `PSF_SIGNAL_WEIGHT.md` —— PSF 信号权重与帧级 SNR 定义。
- `ASTROMETRY.md` —— WCS/SIP 天测公式与外部闭环口径。
- `PHOTOMETRY.md` —— 测光公式与测光一致性判据的误差预算。
- `NOISE_MODEL.md` —— 噪声模型与 σ 估计公式（含星点掩膜与天空预算）。
- `CONTROL_WEIGHT_SNR.md` —— 控制点权重与 SNR 三口径的适用域与公式。
- `DRIZZLE.md` —— Drizzle 重采样公式与面亮度归一。
- `INTEGRATION.md` —— 逆方差叠加与 SNR 重建公式。
- `UNCERTAINTY_AND_COVARIANCE.md` —— 不确定度与协方差传播的统一口径。
- `PHASE2_UPM.md` —— 统一相对模型（UPM）公式与可辨识性。
- `REJECTION.md` —— 逐像素排异定义与合法性窗口。
- `PHASE3_HIPS_TO_FITS.md` —— HiPS 到 FITS 投影导出公式与核语义。
- `ACR_EQUIVALENCE.md` —— ACR 等价性的科学边界（生产不可达）。

### 2.4 算法推导与逐符号实现锚定（`algorithms/`）

本子目录回答「上节的公式怎么算成代码」：每篇给出推导、项序、逐符号锚定与适用域。
门与容差表 `GATES_AND_TOLERANCES.md` 也在此——它是**判据面**，与 `docs/engineering/` 的
机器门登记面（`01_CHECKS.md`）是两回事：前者说阈值是多少，后者说阈值由哪个脚本判。

### 2.5 证据材料（研究包与文献档案）

本子域只承载佐证，**不定义公式、常数与门限**（`DOCUMENT_GOVERNANCE.md` §2）。

- `PHOTOMETRY_RESEARCH_PACK.md` —— 测光标定：Gaia XP 与系统响应到测光星等坐标系的文献与开源对照。
- `IVOA_HIPS_TILE_FORMAT_RESEARCH_PACK.md` —— IVOA HiPS 标准瓦片格式条款的一手查证。
- `CCD_LINEAR_DEFECT_LITERATURE.md` —— CCD/CMOS 线性缺陷（坏列/坏行/拖尾列）检测与修复的一手文献证据。

已迁出本子域的佐证件（其内容仍有效，落点见括注）：

- 帧级 SNR、PSF 权重与逆方差叠加的研究包 → `实验/absolute-snr/docs/SNR_WEIGHT_RESEARCH_PACK.md`
  （实验证据区，非本集规范文档；该件首行自承已作废，仅作历史留痕）。
- 压缩编码（zstd/Rice/TRIM/FITS tile-compression）评估 → `docs/engineering/COMPRESSION_CODEC_RESEARCH_PACK.md`
  （支撑的是压缩与落盘形态这类工程裁决，故归工程正本；其表件在
  `实验/engineering-evidence/compress-01/COMPRESSION_CODEC_RESEARCH_PACK_TABLES.md`）。

## 3 从哪看起

**想知道这个项目在算什么**：读 `UNIFIED_SCIENCE_MODEL.md` 的总纲，再读 `SCIENCE_SCOPE.md` 定边界。
不要从任何单篇公式开始——它们各自只覆盖一段链。

**要核对某个具体公式**：先在本目录找该主题的正本篇（见 §2.3），
再去 `algorithms/` 下同名或对应的推导篇看它怎么落成算子。

**要查某条主张的证据从哪来**：从正本篇的佐证锚出发，进 §2.5 的研究包；
研究包只作佐证，不改写正本。

**要弄清某个产品键代表什么物理量**：直接查 `DATA_SEMANTICS.md`，它是语义权威，
不要从代码或模块文档反推。

**要判断某个门为什么是这个阈值**：先查 `algorithms/GATES_AND_TOLERANCES.md` 的推导，
再去 `docs/engineering/01_CHECKS.md` 查它由哪个脚本判。

## 4 可信度现状

**已审定**：最高设计点名的 SCI/ALG 冻结条款。

**待审或待裁决**：见 §5。此外，跨文档一致性审查已发现一处**定义二义尚未裁决**：
PSF 侧 σ_bg 的换算常数在公式正本与数据语义合同之间有两种写法（MAD 常数乘 vs 截尾均值常数除），
而那两列实际是截尾平均、不是 MAD，两份文档都明文声明两种写法不可互换。
**该二义裁决前，引用 PSF 侧 SNR 的门不可判。**详见 `docs/science/PSF.md` 与 `DATA_SEMANTICS.md`。

**会清除的脚手架**：本集在迁移后尚未重跑全量覆盖度核查，
因此**不声明「最高设计每条科学主张都有对应正本篇」这一覆盖性结论**。

## 5 需要负责人裁决的点

1. **`COMPRESSION_CODEC_RESEARCH_PACK` 的归属**：它支撑的是压缩与落盘形态这类工程裁决
   （上游是最高设计 §8.3/§9），却按证据材料归入本集。需裁决是留本集还是移入工程集。
2. **证据材料是否设 `evidence/` 子层**：本集的公式正本与 `algorithms/` 推导已经很满，
   研究包与文献档案平铺在根下会与正本混读。需裁决是否单设子层。
3. **`DATA_SEMANTICS.md` 的位置**：它已从合同目录迁入本集根下（数据语义属科学面）。
   需确认：机器门读它时，跨集引用路径按新位置登记。
4. **σ_bg 换算常数的二义**（见 §4）：属冻结科学数值，按 SCI 变更流程裁决，不在工程流程内。

## 6 下钻指引

- 索引与分层规则：`ENGINEERING_SPEC.md` §8（仓库根，链外）。
- 文档分层准入判据与佐证纪律：`docs/engineering/DOCUMENT_GOVERNANCE.md` §2。
- 门禁入口与登记面：`docs/engineering/01_CHECKS.md`、`docs/engineering/CI_SPEC.md`。
- 引用承重判据两问（承重性 / 特异性）：见 `run/FINAL-07/lead-01/CARRYING_TEST_SPEC.md`。
- 实现代码在 `lib/`。本集与代码不一致时，**先查生产代码实际用的是哪个口径**，
  再按变更流程订正本集的文档——不要反过来按文档改代码。

---

> 本 README 只写现行设计，不含裁决记录、订正流水、任务编号与日期（`DOCUMENT_GOVERNANCE.md` §5）。
