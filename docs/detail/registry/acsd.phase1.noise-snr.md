# 模块 acsd.phase1.noise-snr

> 上游：`docs/ACSD_DESIGN.md`「模块与 ABI」一节、「P2 跨帧绝对信噪比」一节、
> 「全程只有 SNR」口径、「Phase1 节点流程」一节与「输出合同」一节
> 科学正本：docs/science/noise_snr/NOISE_SNR.md（噪声模型A，「公式与推导」一节连续定义与平面场、「缓变」谓词与「精度归属」三节）、
> docs/science/noise_snr/NOISE_SNR.md（帧级SNR与点源信息权重两节的定义式）、
> docs/science/algorithms/common/NOISE_ESTIMATION.md（ALG-NOISE-001..003，「离散公式」一节推导与
> 「逐符号锚」与「缺陷登记」两节）、`docs/science/noise_snr/NOISE_SNR.md`「适用域与失效域」一节
> 数据正本：docs/detail/registry/acsd.phase1.noise-snr.md（DATA-P1-NOISE 端口表，本页输入输出端口表）；三态表见 `docs/science/unified/DATA_SEMANTICS.md`「方差与逆方差的三态编码」一节
> API 正本：docs/engineering/api/PUBLIC_API.md（API-NOISE-001）、API-P1-006
> （docs/engineering/api/PUBLIC_API.md「分阶段 API 面」）
> 数据对象：docs/detail/common/unified_model.md（frame_snr、sparse_snr_layer）

模块级事实以 `lib/algorithms/noise_snr/README.md` + `module.yaml` 与现行生产实现
`lib/algorithms/noise_snr/cpp/` 为准。现状构建 = `cpp/Makefile` + `cpp/build.ps1`
→ `snr_estimator.dll`，未编入根 CMake 主构建。端口 DATA 编目（DATA-P1-FLUX /
DATA-P1-SNR）为编排层词汇，模块合同 DATA 层 = DATA-P1-NOISE（本页输入输出端口表）。

## 职责与明确非职责

Registry production 模块（唯一源 = module_adapters.cpp 的 p1_noise_snr_descriptor）。
职责：从校准方差、背景、PSF 与光度响应估计逐像素噪声、逐源 SNR、深度 `m5`、
点源信息量 `point_information` 与**帧级 SNR**（写入 HiPS 文件头的唯一帧级参考），
并按配置产出稀疏控制点 SNR 层。**Phase1 的 HiPS 是唯一带 SNR 数据块的产品**
（帧级标量 + 稀疏**绝对** SNR 控制点层）。

三层噪声模型：`PhotometricCalibrationQuality` / `PsfFitQuality` /
`NoiseWeightModelV1`（空背景稳健方差 → ivar）。噪声模型 A = `NoiseWeightModelV1`
（逐像素，正本`docs/science/noise_snr/NOISE_SNR.md`「背景方差面」与「加权方差面」两节）为**唯一生产模型**；噪声 σ 来源 =
局部 patch + 星点掩膜 + 饱和过滤。生产内核 = `cpp/src/noise_model.cpp`（patch
采集与 σ 估计），模块入口 = `src/module_entry.cpp`，phase1 包装面 =
`wrapper_phase1/snr_frame_science.{h,cpp}`。

不做：PSF / 测光本身（`q_psf` 与 `sigma_cal` 不进 science weight）；SCI/ALG
合同之外的扩展；产出含义模糊的单一 `snr` 字段；把 median source SNR 当科学
叠加权重；产出外挂独立 SNR 文件；**不替 Phase2 / Phase3 产出 SNR** —— Phase2
在集成时现场消费单帧 SNR 换算逆方差权重、不输出 SNR 面，Phase3 无 SNR 数据块。

## 输入输出端口、DATA、单位、坐标、invalid

| 端口 | DATA | 必/可 | 单位 | 坐标 |
|---|---|---|---|---|
| `sources` | `DATA-P1-SOURCES` | 必 | `UnitId::DIMENSIONLESS` | `CoordinateFrame::ICRS` |
| `photprov` | `DATA-P1-PHOTPROV-001` | 必 | `UnitId::DIMENSIONLESS` | `CoordinateFrame::ICRS` |
| `cleaned` | `DATA-P1-COSMETIC` | 必 | `UnitId::ADU` | `CoordinateFrame::PIXEL` |
| `fluxes` | `DATA-P1-FLUX` | 必 | `UnitId::ADU` | `CoordinateFrame::ICRS` |
| `snr` | `DATA-P1-SNR` | 可 | `UnitId::DIMENSIONLESS` | `CoordinateFrame::ICRS` |

invalid = NaN/coverage=0（按 DATA 合同）。

输入面：定校信号、variance/ivar、validity、光度响应 `a_k`、检测目录（含逐源
`fwhm`；交付样本按测光有效源构造，不取 PSF 参数面）、图像 + 星表。

输出面（各自独立成面、独立 schema，取值互不代用）：

| 对象 | 语义 |
|---|---|
| `source_snr` | 逐源 SNR（源测量诊断，依赖源亮度） |
| `depth_m5` | 帧/位置深度表达 |
| `point_information` | 点源严格权重（通量估计方差的倒数） |
| `frame_snr` | 帧级 SNR，**写入 HiPS 文件头**；语义 = 点源（PSF）信号 SNR |
| `sparse_snr_layer` | 帧内稀疏控制点 SNR 层（`sparse_snr_layer=true` 时请求产出）；控制点值 = 该点的**绝对**通量型 SNR，与帧级 SNR 同口径、同一个配置缺省的参考星等档 `m_ref`，无量纲；消费时由控制点**直接重建**为稠密 SNR 场。**交付状态 = 合同面已冻结、生产侧尚无产者**：Phase1 既无稀疏层侧车写者，也无 HiPS 属性载体发布者；产者落地前 `sparse_reconstruct` 路径不可兑现，只余 `frame_reconstruct` 与稠密路径 |

`frame_snr` 与 `sparse_snr_layer` 是**相互独立**的两个对象，稀疏层**不作**
帧级标量的尺度基准。`point_information` 是权重、帧级 SNR 是信噪比，两者不是同一
对象。权重不落盘：HiPS 里只存帧级 SNR 与稀疏**绝对** SNR，权重由 Phase2 集成
时现场派生（最高设计数据对象口径）。端口只接同一种对象：Phase2 集成端口只接受
`frame_snr` / `sparse_snr_layer` 与 `point_information`，接错判红。

**不输出**：Phase2 / Phase3 产物不含 SNR 面/块。

### 数值落地口径

信息核、帧级SNR与换算的定义式正本 = `docs/science/noise_snr/NOISE_SNR.md`「绝对信噪比」与「定权」两节；本页只记落地约束。

#### 帧级 SNR（frame_snr）

- 是**唯一帧级参考**，写入 HiPS 文件头；
- **是未加权的原始信噪比，不是权重** —— 它只描述「这一帧的真实源信号相对真实
  噪声有多强」这一客观测量事实，不包含任何为某次叠加服务的加权；权重由 Phase2
  逆方差集成时从SNR现场计算（最高设计「信噪比重建与逆方差叠加」一节）；
- **信号经独立的局部背景估计与扣除**：天光不进入信号项；把天光计入信号的普通
  SNR 会随天光变亮而虚高，不能作唯一帧级参考；
- **天光只通过散粒噪声进入噪声项**：天光变亮 ⇒ 噪声增大 ⇒ SNR 单调下降，
  `B → ∞` 时 `SNR → 0`；这是真实物理变化，不是指标漂移。

**帧级 SNR 红线（不可协商）**：`SNR = F_signal / σ_F`，`F_signal` **必须已扣局部
背景**；天光**只作为噪声项**进入 `σ_F`；固定源通量、增大天光 ⇒ SNR **单调下降**
（`B → ∞` 时 `SNR → 0`），该性质必须可复现；帧级 SNR 是**点源（PSF）量**，与
面亮度 SNR **各自独立、互不代用**。`F_signal` 的测光口径与 `σ_F` 的稳健噪声
估计以`docs/science/noise_snr/NOISE_SNR.md`「绝对信噪比」与「定权」两节为正本。**验收方式**：对每条
SNR 路径做**注入-回收** —— 已知真值信号 + 已知天光 + 已知噪声，回收的 SNR
必须等于真值 `F_s/σ_F`；不满足者修正或退回重检。

**精度归属**：帧级信噪比标量、稀疏控制点值与控制点局部 `σ` 属**稀疏与元数据**面，
按最高设计的「数据对象与配置」章内「精度归属」条取**全程双精度**；JSON 显式指定
位深时以 JSON 为准。稠密逐像素方差面属**稠密大面**，按同条取单精度，两类面不得互相
代入（该归属与现行单一全局精度位的冲突登记见「已知限制」）。

**参考通量按参考星等档 `m_ref` 取，不由数据派生**：参考星等 `m_ref`（配置键
`snr.reference_mag`，缺省 6.0，可被输入 JSON 覆盖）在一次运行内取值不变、参考通量是该星等档在**本帧
仪器通量下的读数** `F_ref,k = 10^(−0.4·(m_ref − ZP_k))`，其中
`ZP_k = ZP_syn,k − 2.5·log10(k_photo,k)` 只来自本帧自身的测光标定；产品另记与帧
无关的**物理公共锚** `F0 = 10^(−0.4·(m_ref − ZP_syn))`，同波段同星场恒为同一数。
该口径的合同正本 = `eng/contracts/schemas/unified/frame_snr.schema.json` 的
`reference_baseline`（必落 `scope` / `reference_flux_source` /
`reference_flux_common` / `reference_mag` / `reference_mag_system`，
`scope` 生效值 = `frame_independent_fixed_magnitude`），逐帧 provenance
`snr_reference` 另记 `reference_mag` / `frame_zero_point_mag` / `frame_k_photo`
—— `m_ref` 随产品落盘，消费侧据此复算而不必猜口径。

**禁由数据派生参考通量**：逐帧检出通量中位数形态会丢掉帧间标度因子 `a_f²`，并把
本帧检出亮度混进入头 SNR，使帧间不可比、与权重链的 `F_ref` 口径不配对 ⇒ 该形态
**fail-closed**（`snr_frame_science` 的 `reference_flux_adu` 缺失 / 非有限 / ≤0 即
判红）；公共锚与帧参考通量的配对恒等式 `w = SNR_f²/F0² = a_f²/σ_f²` 只在两者
同源于一个 `m_ref` 时成立。

配对性只要求**同一帧内** SNR 与参考通量同源，**不要求跨帧相等**；不同指向 / 不同
光学系统的帧**合法地**有不同 `F_ref,k`，跨帧一致性降为**报告字段**、
**不设组间参考通量硬闸门**。参考星等档是**线性区外**的形式外推，作为**参考
电平**良定义，但该值的用途**限定为参考电平**（真实帧可饱和或超满井）。单位以产物
字段 `reference_flux_common_unit` 为准（现行 = Gaia XPSD 绝对谱积分合成通量；
显式 `snr.reference_flux_adu` 覆盖时为 ADU）。

HiPS 是数据库：帧产品长期保存、可被任意多次、任意科学目标的叠加消费，因此
入库的是客观的未加权 SNR（与具体集成无关的观测量）；权重在 Phase2 集成时按
天球像素对应的输入帧集合现场计算。稀疏帧内层启用时，每个控制点同样存
**未加权的绝对 SNR**（与帧级同一物理定义、同一参考通量口径、同一个
`m_ref`），而非权重。

#### SNR 三条路径与稀疏帧内层

三条路径全部保留在算法面（Phase1 产出 / Phase2 重建），由配置 JSON 显式指定：

| 路径 | Phase1 产出 | Phase2 重建稠密 | 定位 |
|---|---|---|---|
| `dense` | 稠密逐像素 SNR 面 | 直接使用 | 精度基准 |
| `sparse_reconstruct`（**默认**） | 稀疏控制点**绝对** SNR 层 | 由稀疏层重建 | 存储/精度折中（现行默认） |
| `frame_reconstruct` | 仅帧级标量 | 由帧级标量重建 | 单帧级对照 |

三条口径都**直接**产出同一物理量（绝对通量型 SNR）的稠密表示，只在重建方式
上不同；`sparse_reconstruct` **不乘帧级标量**。SNR 是信噪比，不是权重。稀疏层的
位置 / 值 / 采样覆盖写入 manifest。**不静默降级**：输入无稀疏层而路径为
`sparse_reconstruct`（含默认）⇒ 按帧级执行但**必须显式记录实际路径**
（`snr_path_effective`）并计数；稀疏层存在但损坏/不可重建 ⇒ 显式失败。

**适用域边界（三条，判据 = 重建权重效率相对最优权重效率的超出量；完整适用域
图谱与读数 = `实验/absolute-snr`）**：

1. **HST 类高对比域：失效边界与重建算子强绑定**。失效边界 = 「该算子判据首次
   劣于帧级臂的最小 Δ」：双线性档与默认自然样条档的边界相同，mesh 中值档的
   边界更大。生产 Δ=64 ⇒ 该域上现行双线性与默认档都不如「退化成帧级」的
   兜底臂，**只有 mesh 档胜出**。⇒ 该域的结论必须写成「在某个算子下」，不能
   只写「帧级更优」；
2. **未分辨结构域（cell 内结构尺度 ≪ Δ）**：没有任何重建算子能救回来。cell 稳健
   尺度被 cell 内未分辨结构抬偏是主导项（该域判据比可分辨域高两个数量级），
   且域内算子间极差小 ⇒ 算子选型在此域不是主要矛盾，控制点估计量（结构感知
   局部 σ）才是；
3. **M42 类真实地面帧：稠密与稀疏互有胜负**，差值量级不足以抵偿稀疏层的存储
   优势 ⇒ 该域只给**定性**结论。

判据口径的两条已知局限（与结论同读）：① 判据对乘性偏差完全免疫 —— 两个臂可以
判据完全相同而水平偏差相差数个数量级 ⇒ 判据只回答「权重效率」，**不能替代
水平偏差判据**；② 「帧级臂 RMSE ≤ K·场尺度」类判据对任意真值场恒真，**无
证据资格**。

#### σ_sky 口径冻结（防读出噪声双计）

**唯一口径规则**：逐像素噪声由天光散粒方差、读出噪声项与源泊松项三项合成，
**读噪只出现一次**。`sigma_sky_adu` 入参**只承载一种语义**，调用方必须显式
声明是哪种；**择一依据 = 显式声明**。

| `sigma_sky_source` | 含义 | 是否再加读出噪声项 |
|---|---|---|
| `shot_noise_only` | 天光 + 暗流的**散粒**方差（不含读噪） | **加**（默认路径，需 gain > 0 且读噪 > 0） |
| `empirical_total_rms` | 经验**总** rms（含读出噪声，如生产调用点传入的整帧 2 轮裁剪 RMS；噪声模型 A 的稳健尺度是另一生产者，承载逐像素 `variance`） | **不加**（已含读噪） |

决策树（调用点实现口径）：

1. `shot_noise_only` 且 gain > 0 且读噪 > 0 ⇒ 走「天光散粒 + 读噪 + 源泊松」
   合成（设计本意路径）；
2. `empirical_total_rms` ⇒ 只加源泊松项，**不再加**读出噪声项；
3. gain 未知（≤ 0，PSF 行路径）⇒ **源泊松项同样不可加**（该项含增益倒数，无
   增益即无法计算）⇒ 退回**天空受限**口径，此口径下 SNR **系统性偏高**，是
   **上界**而不是绝对 SNR ⇒ 产品必须标 `snr_caliber = upper_bound_no_gain`
   并带 `snr_degraded_reason`，该值**仅限上界诊断**（最高设计交付物口径
   是绝对 SNR）；
4. 声明与实际来源不一致（声称散粒而来源为经验总 rms，或反之）⇒ **fail-closed
   拒绝**，告警不构成放行。

**必须保持的不变量**：固定源通量、天光增大 ⇒ SNR 单调下降、斜率 −1/2；双计会
破坏该斜率。**负例**：`empirical_total_rms` 时再加读出噪声项必须判红。

**C ABI legacy 路径**：`SNR_SIGMA_SKY_UNSPECIFIED = 0` 保留为直接 C API 调用方
的兼容缺省（与 `SHOT_ONLY` 组合逐位一致，由 `p1snr_science_skysource` 的向后
兼容锁固定）；该路径的适用范围 = 直接 C API 调用方（生产链走显式声明）——
「缺失即 fail-closed」由调用点层（而非 C 函数层）保证，因为 C 函数层无法观测
入参的**实际来源**。

#### 噪声模型 A 与逐像素方差面

**噪声模型 A 为唯一生产模型**：实现 `cpp/src/noise_model.cpp`，接口
`snr_noise_model_v1` / `_f64` / `_fill` + `NoiseWeightModelV1`（**逐像素**），
编入根 CMake 目标 `acsd_phase1_noise`（STATIC），经 `acsd_phase1_session`
的 PUBLIC 闭包链入主程序。模型定义、参数语义、稳健噪声估计与掩膜规则的正本见
docs/science/noise_snr/NOISE_SNR.md。

**逐像素方差接线现状**：A 已编入 `acsd_phase1_noise` 并链入主程序；生产
调度路径**已挂** `variance` 帧内命名块 —— module_adapters.cpp（`p1_op_drizzle`）
按 `NoiseWeightModelV1` blank-sky variance 经 `snr_noise_model_v1_fill` 填面后
`aio_frame_add_block(frame, "variance", AIO_BLOCK_FLOAT32, …)`，引擎侧
`sumVarNum += v·w²`，sink/writer finalize 出 variance/ivar 子产品；
`uncertainty_available` 为 provenance 判定结果（由磁盘事实给出，
`true` ⇒ variance|ivar 位同时置位），显式降级非静默（带 `var_status` /
`var_reason`）；口径正本 = registry/acsd.phase1.drizzle.md。

**逐像素方差面的两态约束（必须成立）**：`snr_noise_model_v1_fill` 输出的每一
像素必须落在两态之一 —— **可用**（`variance > 0` 且有限，且 `ivar` 为其倒数）
或**不可用**（`variance = 0` 且 `ivar = 0`）。平面预测 ≤ 0、或生效 floor /
输出值在 float32 中不可表示（下溢为 0、上溢为非有限）的像素取不可用态；
不可用方差的落盘值 = 显式不可用态本身 —— floor clamp 会把「模型在此处失效」
发布成极大逆方差；`(0, +inf)` 这类自相矛盾的对同样按不可用态处理。正本 =
`docs/science/noise_snr/NOISE_SNR.md`「适用域与失效域」一节数值地板与三态、「精度归属」一节与`docs/science/unified/DATA_SEMANTICS.md`「方差与逆方差的三态编码」一节三态表。
该组判据的负例面（平面预测不可用、float32 下溢/上溢对）与自证面（证明判据能红、
非恒真）已取证，载体不在本仓可复算路径上。

**影响面**：**不影响**帧级 SNR 路径（科学上正确）；逐像素**不确定度产品面**由
生产调度路径经 A 的插件路径产出。凡「逐像素方差/不确定度已传播到产品」的主张
**必须**附方差 tile 数大于 0 的磁盘证据。

#### 背景方差面的自适应拟合与审计面

背景方差面的**控制点有效性**与**拟合可行域**都由数据自身给出，**不含按数据集
标定的常数**（正本 = `docs/science/noise_snr/NOISE_SNR.md`「背景方差面」一节空间方差场）。

- **控制点有效性判据是自校准统计量** `R` = 该 patch 的稳健尺度 / 同一 patch
  在**白噪声零假设**下的等价尺度。该判据的**零假设值恒为 1**，是恒等式而不是
  标定值 ⇒ `R` 的比较基准 = 零假设值 1 本身；与写死的绝对倍数比较属另一口径；
- **判据实现 = `ln R` 的序统计量**：取最大的跳变序号 `j` 使第 `j` 个跳变超过保留
  主体的极差；保留主体下限 = max(⌈n/2⌉, 预算 patch 数)；`n` 低于该下限时判据**不
  武装**，这些 patch 计入 `n_r_unavailable_patches`，**按不可用登记**。序号记作
  `j` 而非 `k`，以免与本模块背景方差口径的掩膜边缘残余系数 `k` 混名（后者的取值
  与出处正本 = docs/science/noise_snr/NOISE_SNR.md 的常数表）；
- **拟合可行域**：平面必须在**控制点凸包内结构非负**（凸包内预测 ≤ 0 是拟合
  缺陷，不是合法外推）；**合法解集 = 凸包内结构非负的平面**（常数场属另一
  形态）；
- **拟合权重 = 相对误差加权**（以控制点方差的中位数为基准）；
- **可观测量必须写入 provenance**，缺任一必落字段即该帧方差面**不可审计**：
  `<帧>/p1_final.json#variance_audit`（内容逐字读回
  `<帧>/p1_stack.json#variance_audit`）。必落字段 =
  `ctrl_variance_range`、`plane_a` / `plane_b` / `plane_c`、
  `hull_nonpositive_frac`、`n_structure_rejected_patches`、`r_min` / `r_median` /
  `r_max` / `r_fence`；逐帧另有 `variance_audit_present` /
  `variance_audit_available` / `variance_audit_missing_fields` /
  `variance_audit_source`。清单的机器唯一源 =
  `acsd::noise::variance_audit_required_fields()`（生产者、校验者与测试
  **必须**引用同一份）；
- **失败语义**：本帧**已发布** variance/ivar 子产品而必落字段不齐 ⇒
  **fail-closed**（DATA 类，`p1_final.json` 不落盘）；`hull_nonpositive_frac != 0`
  ⇒ 该方差面不可审计。未发布方差面时如实登记「本次没有这张面」，**「无此产品」
  与「字段丢了」各按各自语义读**。

#### 标量降级门与质量代理

标量降级门：仅当帧内点源信息量场的鲁棒相对离散与系统趋势低于阈值才存帧级
标量；否则存 map / 控制点 / 多项式 / HEALPix，摘要带 p05/p50/p95、最大系统
偏差、覆盖、模型误差。PSF 拟合质量代理（FWHM、残差尺度等）只作诊断，
**不计入科学叠加权重**；帧级 SNR 对标的是「未加权原始信噪比」这一量。

#### 稀疏帧内层几何与重建算子

- 稀疏控制点间隔 Δ 复用 Phase2 UPM 的 8×8/tile 控制网格（Δ = tile_width / 8，
  tile_width = 512 ⇒ Δ = 64 px）；默认保持 Δ = 64 px（HST 类高对比数据推荐
  32 px，下限 16 px），取值依据、失效边界与完整适用域正本见 `实验/absolute-snr`；
- **几何（冻结）**：控制点坐标是**像素中心坐标**（像素序号 p 对应坐标 p）；规则
  网格下每个控制点落在**所属 Δ×Δ cell 的中心**。这与 Phase2 UPM 控制网格是同一
  约定（控制观测与系数节点位于 cell 中心）；**把节点当 cell 角点会使重建场整体
  平移半个 cell（Δ=64 时 31.5 px）**。cell 网格原点由 `grid_origin_x/y` 显式
  声明（缺省 0 = 帧原点），消费侧以「节点—cell 中心」一致性门 **fail-closed**
  拦截该错位；
- **定义域 = 层覆盖的 cell 并集**：cell 内非节点处由重建算子插值给出，最外
  半个 cell 由端点节点常数延拓；越出该并集即 **fail-closed**（不外推、不回退
  帧级）；
- 稀疏控制点存**绝对** SNR（与帧级 SNR 同口径、同一个配置缺省的参考星等档 `m_ref`）；
  重建算子在控制点上重建出稠密绝对 SNR 场；帧级标量与稀疏层相互独立，不作其
  尺度基准，消费时也不参与还原；
- **重建算子（冻结词表；算子标识 = 唯一配置面）**。算子标识把「核 + 是否开
  3×3 mesh 中值前置滤波 + 是否做值域钳制」**整组绑定**，不做成可自由组合的
  独立开关：钳制是正值与有界性的必要条件，滤波只在特定域必需，独立布尔可组合
  出从未实测的配置。算子标识入 manifest（`SparseReconstruction.operator_id`）。

| 算子标识 | 语义 | 定位 |
|---|---|---|
| `natural_bicubic_spline_clip_v1` | 可分离**自然边界**双三次样条 + **钳到有效控制值值域** | **默认**（地面/seeing-limited、解析可分辨域与一般情形） |
| `natural_bicubic_spline_clip_mesh_median_v1` | 同上，且在样条前插入 **3×3 mesh 中值滤波**（边界 replicate、无条件替换） | **高对比域开关**：cell 内含未分辨亮源（HST/空间高分辨率） |
| `bilinear_regular_grid_v1` | 规则网格双线性（边界 clamp） | 保留为**对照/回退**（现行实现的忠实保留） |
| `nearest_control_point_v1` | 最近控制点 + 显式覆盖半径 | 散点层唯一合法算子 |

- 算法（默认路径，全部 O(N)）：① 无效控制点（NaN，即 schema 的 `invalid_repr`）
  取最近有效控制点（等距并列取平均），**全部无效 ⇒ fail-closed**；② 可分离
  自然边界双三次样条（y 向 natural BC → x 向 natural BC）；③ 钳到有效控制值
  值域。高对比档在 ① 与 ② 之间插入 3×3 mesh 中值滤波。非 NaN 的非有限值与
  ≤ 0 是**非法值**（不是 invalid 表示）⇒ fail-closed；未识别算子标识 ⇒
  fail-closed（默认档只对已识别算子标识生效）；
- **值域钳制不可省**：去掉钳制后，光滑插值类在病态控制网格上的权重效率损失
  显著上升，并会给出**负的噪声估计**（非物理）；正常面上钳制的代价可忽略；
- **mesh 中值滤波是必需项而非可选优化，且必须按域显式开启**：HST 类高对比域
  上默认路径单独使用劣于帧级兜底，叠加滤波后转为胜出；而在解析可分辨域与
  M42 类真实地面帧上滤波造成显著损失 ⇒ **默认关**；
- **选择规则（按数据来源，不按控制网格自身推断）**：可判定 cell 内含未分辨
  点源（空间高分辨率 / HST 类）⇒ 声明 mesh 中值档；地面 seeing-limited 与一般
  情形 ⇒ 默认档；**无法判断 ⇒ 默认档**（默认目标域上滤波有害）。该规则由
  `sparse_recon_operator_for_source()` 显式承载。**从控制网格自身推断不可行**：
  两个候选标量诊断（相邻控制值差分比、`std(log10 ctrl)/ε_cell`）都不能把
  「滤波有益」与「滤波有害」的域分开，且在 16-bit 整数真实数据上直接失效；
- 任何算子都必须满足四条**与选型无关**的约束：① **显式声明**——算子标识入
  manifest；② **正齐次性**（控制点值整体缩放时重建场按同一因子缩放）——这是
  「帧内共模因子在权重口径下相消」成立的前提，带**数据无关固定先验均值**或
  向固定值收缩正则的算子（如固定先验均值的 GP / kriging）**不满足**该条；
  ③ 在控制点处**精确复现**节点值；④ 越出定义域即 **fail-closed**，域外取值
  一律显式拒绝（外推 / 帧级回退各属另一路径）。不正齐次的算子若被选用，必须把
  `homogeneity` 声明与「该算子下相对场**不能**由绝对场缩放得到」写入 manifest，
  消费侧据共模相消做的等价假设**以该声明为前提**；
- 控制点位置、取值、采样覆盖、`reconstruction_operator` 与 `snr_path_effective`
  一并写入 manifest 与产品内容证据块；
- **插值设置配置化 + 运行日志输出，不随产物落盘**：词表只冻结算子标识与语义；
  算子级数值设置属**配置级**参数，由配置承载。备选 IDW 口径保持**备选定位**
  （不在上列冻结词表内），其幂次默认 1.0（适用域 = 含噪场景，最优带在 0.5–1
  的上端；幂次 2 属无噪/光滑极限口径）、邻居数 16、重合点守卫小于 1e-10，
  以及 mesh 滤波开关等，均按配置解析。**实现侧接线现状**：`snr_evaluator` 的
  兜底默认**与规范一致为 1.0**（成员初始化与两处非正守卫均回落 1.0），配置
  显式传值时不触发兜底；每次重建把**实际生效**的插值设置写入**运行日志**，**不
  随产物落盘** —— 产物面只登记算子标识与层几何。幂次依赖场形态与噪声档，不
  冻结单一「最优值」，随日志积累重标定；
- **控制点局部 σ 必须结构感知（数值准确的必要条件）**：控制点存绝对 SNR 只保证
  **表示正确**，不保证**数值准确** —— 后者完全由局部 σ 估计器决定。把估计作用域
  从整帧朴素地换成区域**不够**（整帧口径的结构污染只是被挪到更小尺度，cell 内的
  未分辨结构仍被算进稳健尺度）。控制点的局部 σ **必须**用**结构感知**估计器：
  mesh 局部背景扣除后的逐区域残差稳健尺度，或跨帧差分（唯一零结构偏差口径）；
  估计器标识、`sigma_rho` 与 `quality_flags` 一并入 manifest。估计器认证、逐
  区域偏差与适用域见`docs/science/noise_snr/NOISE_SNR.md`「适用域与失效域」一节。

## 公共 header、核心 symbol 与生命周期

模块级 API = API-NOISE-001（docs/engineering/api/PUBLIC_API.md 噪声/SNR 节）；编排级
API = API-P1-006（phase session extern "C"）；生命周期
create→validate→run→inspect→destroy。

头与导出面（`lib/algorithms/noise_snr/`）：

- `include/acsd/information_weight.h`：`CovarianceView` / `PointEstimate` /
  `w_info_diagonal` / `w_info_dense` / `w_info_low_rank` / `w_info_solve` /
  `white_noise_gate` / `w_info_white_noise` / `diag_approx_report` /
  `combine_point_estimates`；
- `include/acsd/noise/types.h`：C 面类型；
- `include/acsd/noise/variance_plane_policy.h`：`VariancePlaneVerdict` /
  `classify_variance_plane`；
- `include/acsd/noise/saturation_policy.h`：`resolve_saturation_level` /
  `resolve_effective_saturation` / `saturation_filter_state`；
- 模块入口 `src/module_entry.cpp`，导出面 `src/acsd_p1_noise.def`。

entrypoint = 信号 + ivar + PSF + `a_k` → {source_snr, depth_m5,
point_information, frame_snr[, sparse_snr_layer]}。帧级 SNR 经 drizzle 写入
HiPS 文件头；稀疏层作为标准层插入 HiPS。各类输出独立 schema。

### 源文件

`lib/algorithms/noise_snr/include/acsd/`（公共头族）、`lib/algorithms/noise_snr/src/`
（模块入口与导出面）、`lib/algorithms/noise_snr/wrapper_phase1/`（phase1 包装面）、
`lib/algorithms/noise_snr/cpp/`（噪声模型 A 的独立构建面）。

## Registry descriptor 与配置 schema

module_id=`acsd.phase1.noise-snr`; execution_class=`cpu_heavy`;
parallel_ok=True。配置 = phase config JSON：

| 字段 | 默认 | 单位 | 说明 |
|---|---|---|---|
| `snr.reference_mag` | 6.0 | mag | **参考星等档** `m_ref`（缺省 6.0，可被输入 JSON 覆盖），SNR 与深度定义必需。参考通量由该星等档按本帧测光零点换算（`F_ref,k = 10^(−0.4·(m_ref − ZP_k))`，见「数值落地口径」的参考通量条）；**不由数据派生**，`m_ref` 随产品落盘（合同必落字段见 `eng/contracts/schemas/unified/frame_snr.schema.json` 的 `reference_baseline`）。星等档是线性区外的形式外推，**用途限定为参考电平** |
| `snr.reference_flux_adu` | —— | ADU | 显式给出的参考通量（覆盖按 `m_ref` 换算的结果，优先级最高）；缺失 / 非有限 / ≤0 ⇒ 该帧 fail-closed，不回退到数据派生形态。单位以产物字段 `reference_flux_common_unit` 为准 |
| `scalar_gate_rd` | —— | —— | 标量降级鲁棒离散门 |
| `scalar_gate_trend` | —— | —— | 标量降级系统趋势门 |
| `psfsw_enable` | true | —— | 是否产出 PSF 信号权重复合分量（诊断/基线对照；PSF 拟合质量代理不计入科学叠加权重） |
| `sparse_snr_layer` | true | —— | 是否产出稀疏帧内 SNR 层（控制点值 = 绝对通量型 SNR，与帧级同口径、同参考通量）。**请求面已冻结；生产侧尚无产者**，落地前该键不产生可消费产物 |
| `sparse_snr_spacing_px` | 64 | px | 稀疏层控制点间隔 Δ：复用 Phase2 UPM 的 8×8/tile 控制网格（tile_width / 8 = 512 / 8 = 64） |
| `sparse_snr_density` | —— | 点/度² | 稀疏层控制点密度（按面积表述）；生产由像素域控制点间隔承载该量，本键不承载生产取值 |
| `snr_path` | —— | —— | **该键属 mosaic 配置，normalize 配置不含它**，本页只记 Phase1 侧的产出形态；取值域、缺省与登记正本见 `docs/detail/registry/acsd.phase2.integrate.md` 的配置表行 |

**重建算子的声明面不是配置键**：算子标识由稀疏层自身声明
（`sparse_snr_layer.reconstruction_operator`，冻结词表见本页「稀疏帧内层几何与
重建算子」），随层入
manifest；上表**不**登记该键 —— 避免出现「配置一套、层里另一套」的双事实源。

## Execution class、并行轴、ThreadBudget lease、确定性

`cpu_heavy`; parallel=是（资源门拒绝 heavy+serial 组合）; worker 数 =
ThreadBudget.max_workers（唯一取值源）。现状 patch 采集单线程顺序执行、median
局部。

确定性 = NOT_VERIFIED（未取得验收证据；不写成 PASS/FAIL）。

## 内存/cache/I-O/所有权

cache/内存按 ALG 合同（bounded）; I-O 单 writer。平面判定与审计字段 =
`VariancePlaneVerdict` + `variance_audit_required_fields()`（10 字段）。

所有权 = 结果 buffer 由调用方分配与持有。

## 错误、日志、指标、取消和 checkpoint

错误码与退出码唯一源 = lib/infrastructure/cli/exit_codes.h（本页不复制数值表）。
无合格 patch → `NO_DATA` / fallback。

- **重建算子与层几何**：未识别的算子标识 ⇒ fail-closed；声明为规则网格算子而
  层是散点、或反之 ⇒ fail-closed；控制点不在所属 cell 中心（角点锚定的控制
  网格，半 cell 相位）⇒ fail-closed；控制值为 0 / 负 / ±inf（非法值，不是
  invalid 表示）⇒ fail-closed；控制点为 NaN（= schema 的 `invalid_repr`）⇒ 按
  最近有效控制点填充（等距并列取平均）并把填充数入 manifest，**全部无效 ⇒
  fail-closed**；查询点越出层覆盖的 cell 并集 ⇒ fail-closed（`out_of_domain`，
  不外推、不回退帧级）；
- 缺 `a_k` / PSF / 方差 ⇒ fail-closed（信息权重不可凭空造）；
- 标量门失败 ⇒ 自动升级为空间模型（标量结果只保留具名降级登记）；
- 参考通量取不到时 m5/SNR 不可输出。三条来源形态各自具名落盘
  （`reference_flux_source`）：① `fixed_magnitude` = 配置缺省的 `m_ref` 按本帧测光
  零点换算（生效路径，作用域 `frame_independent_fixed_magnitude`）；② `config` =
  显式 `snr.reference_flux_adu`；③ `group_median` = **块级**公共 F0（块内逐帧检出
  通量中位数的中位数，作用域 `group`），只在 ①② 都不可得时启用，`scope` 随之落盘
  为 `group`、**不伪造**；三者皆不可得 ⇒ `unavailable`，该帧 fail-closed。
  **以本帧检出通量中位数充当本帧 `F_ref`** 的形态（作用域冒充 `fixed_magnitude`）
  ⇒ fail-closed：它丢 `a_f²` 且使帧间不可比；
- **跨帧参考通量相等只作登记事实**；缺 `variance` 块 ⇒ `uncertainty_available=false`，
  **逐像素方差的产出主张以 `variance` 块在盘为准**；
- 帧级 SNR 无法计算（如缺真实信号参考）⇒ fail-closed；受天光影响的普通 SNR 属
  另一个量，两者互不代用；
- **负例与归零分支 N06/N07/N08（T05–T07 负向轮，零尺度 / NaN / sigma=0 显式拒绝）**：
  N06 零尺度输入构造 = `σ_bg_raw = 0` 的常数 patch（≥50% 像素同值）。预期行为 =
  `control_ivar == 0` 且 `control_variance` 非有限，不发布伪方差。落盘标记 =
  控制点 `ivar/variance` 对；<50% 同值 patch 走正常分支（正负例各一）。
  N07 NaN 输入构造 = 全 NaN patch（`isfinite` 归约全败）。预期行为 = 显式判红，
  不静默通过。落盘标记 = 错误码 + 拒收计数。N08 `sigma = 0` 输入构造 =
  `hot_sigma <= 0` 或 `dark == NULL`（或 `sigma_bg = 0`）。预期行为 = 检测禁用、
  `out == data` 且 `out_hot == 0`，归零而不断言虚假修正。落盘标记 = 禁用位 +
  零修正计数；任一分支静默落数值地板继续产出 SNR ⇒ 判红；
- 指定 `sparse_reconstruct` 路径而输入无稀疏层 → 按帧级执行并**显式记录实际
  路径**（不静默）；稀疏层损坏/不可重建 → fail-closed；
- **稀疏层值语义判红**：把控制点值按**相对因子**解释（含乘 / 除帧级标量做
  还原）⇒ 必须判红。控制点值是**绝对**通量型 SNR，由 schema 的
  `sparse_snr_semantics` 冻结为 `absolute_flux_type_snr`；声明为相对语义或缺失
  该键 ⇒ schema 判红；
- 任何信号项未扣局部背景、被天光/背景抬高的 SNR → 判红（帧级 SNR 红线）；
- **σ_sky 声明义务**：**生产调用点**（module_adapters 的 `p1_op_noise`）必须
  显式声明 `sigma_sky_source`，缺失即判红；声明与实际来源不一致 ⇒ fail-closed
  拒绝。
- **负例与归零分支 N21（T05–T07 负向轮，本卡死值与静默 scale）**：负例输入构造甲 =
  缺 `variance` 块仍要求逐像素方差产品；负例输入构造乙 = 缺 `sigma_sky_source`
  声明的 `p1_op_noise` 调用。预期行为甲 = `uncertainty_available = false`，
  两位均不置位，禁占位子产品，产出主张以 `variance` 块在盘为准。落盘标记甲 =
  `uncertainty_available` + 缺块计数。预期行为乙 = 判红 / fail-closed。
  落盘标记乙 = 缺声明登记；静默回退 support/SNR 代替 ivar ⇒ 判红；

取消 = 协作取消（契约：宿主 cancel 通道 → 停止调度新单元 → 等运行中单元完成
→ exit 9，最高设计协作取消口径；接线以实测为准）；模块内无 checkpoint。

## 独立 synthetic 验证命令与容差

测试标识 = `TEST-P1-SNR-001`（registry descriptor 单源）；执行证据 = NOT_VERIFIED
（未取得验收证据）；容差 = NOT_VERIFIED（同源）。设计矩阵覆盖
SNR-001..015 全矩阵（pedestal / scale / star-pop / Gaussian / Poisson / 场恢复
/ coadd / 独立性 / MC 协方差）。

Oracle 面：

- 注入点源：理论点源信息量（通量不确定度 = 点源信息量的平方根倒数）与实测
  散度一致；改变星表亮度分布不改变点源信息量、但改变 median source SNR
  （跨模块验证）；seeing / 背景 / 透明度按理论改变信息权重；
- **加性背景平移不改变信号项**（注入恒定背景偏置，测光信号不变）；**天光散粒
  噪声增强时噪声增大、帧级 SNR 按理论下降**；**单调性负例**：固定源通量、天光
  增大 ⇒ SNR 单调下降，天光趋于无穷时 SNR 趋于零；
- **注入-回收**：已知真值信号 + 已知天光 + 已知噪声 ⇒ 回收 SNR = 真值（三条
  路径各一组；不满足者判红）；
- 稀疏层：启用/不启用输出结构正确，稀疏层值可重建验证；**三路径精度与存储量
  对比**并报告精度差与存储量；无稀疏层而路径为 `sparse_reconstruct` 时实际路径
  须被显式记录（负例：静默降级判红）；
- **重建算子 Oracle**（判据载体 = 实验单元
  `实验/absolute-snr/docs/EXP-04-RECONSTRUCTION.md` 的算子实现与逐像素对拍表；
  仓内**没有**独立的 oracle 源码目录，独立复算的承载形态是下述逐条判据）：
  ① 正例——默认算子与独立复算的自然样条+钳制逐点一致、控制点自身复现残差
  ~0、预置路径与单次调用逐位一致、1/8 worker 求值逐位一致；② 与实验单元的算子
  实现逐像素对拍（容差 1e-12）；③ 负例注入——移除值域钳制 ⇒
  病态网格效率爆增判红；把 mesh 滤波档设为全局默认 ⇒ 默认目标域上默认档与
  滤波档持平判红；移除 cell 中心几何门 ⇒ 角点锚定网格被接受判红；同时移除
  钳制与正值守卫 ⇒ 重建场出现负噪声估计判红；
- **稀疏层绝对语义判据**：① 绿——控制点自身复现（节点坐标处重建值 == 落盘
  控制点值，残差 ~0），且与帧级标量**无关**（同层配不同帧级标量，重建结果
  逐位相同）；② 红——按相对值解释（用帧级标量乘除中位数还原）时节点重建值 ≠
  落盘值，该差异必须判红；③ 红——schema 层：值语义声明为相对或缺失 ⇒
  合同测试判红；
- 点源/面亮度口径分离：把面亮度 SNR 当帧级 SNR 使用必须判红；
- **参考通量跨帧独立性负例**：人为要求组内参考通量相等（组间硬闸门）⇒ 必须
  判红 —— 不同指向 / 不同光学系统的帧合法地有不同 `F_ref,k`；逆方差配对性**只在
  同一帧内**成立；另一负例 = 以本帧检出通量中位数冒充 `F_ref`（作用域写作
  `fixed_magnitude`）⇒ 该帧 fail-closed；
- **背景方差面判据**：注入结构（未分辨源 / 强梯度）⇒ 被 `ln R` 序统计量判据
  剔除、`n_structure_rejected_patches > 0`；真值无结构 ⇒ 判据**不动作**
  （`n_structure_rejected_patches == 0`、`r_median ≈ 1`，证明判据非恒真）；
  `R` 不可算的 patch 计入 `n_r_unavailable_patches` 而**不**被剔除；
- **审计面**：正常帧必落字段齐全、`hull_nonpositive_frac == 0`、
  `ctrl_variance_range > 0`；删掉整块或只删单个字段后重跑 writer ⇒ 判红；
- 1 worker vs N worker 输出一致。

## 已知限制

- 缺陷与整改登记 = `docs/science/algorithms/common/NOISE_ESTIMATION.md`［A-1］；
- gain 未知时源泊松项不可加，帧级 SNR 退化为天空受限上界口径
  （`snr_caliber = upper_bound_no_gain`），该值仅限上界诊断；
- 以本帧检出通量中位数充当本帧参考通量的形态为 fail-closed（数据派生参考通量
  会丢 `a_f²` 并使帧间不可比）；
- 平场大尺度残差类结构项无对照数据；
- 现状构建产物 `snr_estimator.dll` 未编入根 CMake 主构建（生产走根 CMake 目标
  `acsd_phase1_noise` 静态链入）；
- 三条 SNR 路径的适用域由 `实验/absolute-snr` 判定，高对比域结论必须绑定算子
  才能成立；
- **精度归属待裁决**：最高设计要求稀疏与元数据（帧级信噪比等）全程双精度，而
  `docs/engineering/UNIFIED_OBJECTS.md` 的对象登记对全部对象只给「float32 或
  float64」一个全局精度位、`lib/infrastructure/aio/src/aio_api.cpp` 也只有一个全局
  精度开关（`aio_set_precision_mode`）—— 一套全局位无法同时满足「稠密大面单精度」
  与「稀疏元数据双精度」。本卡按最高设计登记双精度归属，实现侧如何满足该归属
  （按块分精度位 / 分 AIO 句柄 / 接受全局单精度并下调最高设计）属负责人裁决项，
  未决前不在本页给出实现结论；
- 全局限制登记 = artifacts/evidence/known-limitations-ledger/LIMITATIONS.md。
