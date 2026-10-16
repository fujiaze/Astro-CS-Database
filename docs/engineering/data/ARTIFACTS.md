# 数据工件

工件把科学对象绑定到单位、坐标、形状、有效域、所有权与序列化。物理定义以[数据语义](../../science/unified/DATA_SEMANTICS.md)为准，对象职责见[统一工程对象](../UNIFIED_OBJECTS.md)，阶段一复合字段见[Normalize 数据](../contracts/NORMALIZE_DATA.md)。本页登记承载关系，不从旧 schema 或枚举推导新的物理模型。

## 1. 每份工件携带什么

| 项 | 要求 |
|---|---|
| 身份 | run/frame/input occurrence、对象类型、revision、生产者及输入版本 |
| 数组 | dtype、shape、axis 顺序、stride/布局、所有权与借用寿命 |
| 物理 | 被估计量、实际单位、标度是否施加、应用身份、必要 reference/响应 |
| 几何 | 坐标系、像元原点、WCS 或 NESTED order/ipix、tile-local 映射 |
| 有效性 | 覆盖、掩膜、排异、模型能力与缺项分别表达 |
| 持久化 | 格式/schema、相对路径、字节数、内容哈希、完成状态与模型引用 |

没有来源的单位和误差不能补成 ADU、0或1。FITS 读取施加 BSCALE/BZERO 后保存物理标度；校准值、测光应用后值、电子数和 SI 量分别具名。内部零基像元中心与 FITS WCS 一基约定只转换一次，tile-local 与 NESTED 的手性由共享实现处理。

## 2. 既有 DATA ID 与承载

下表保留既有 ID 的定位用途。相同 ID 在旧登记中有歧义时，以科学对象和具体 API 类型解释并登记迁移，不能称全部已明确。

| ID | 对象与承载 |
|---|---|
| `DATA-IMG-RAW-001` | 原始亮场，[H,W] y,x；FITS/XISF 读取后的线性值 |
| `DATA-IMG-CAL-001` | 校准图，[H,W]；保留有限负值及真实单位 |
| `DATA-IMG-VAR-001` / `DATA-IMG-IVAR-001` | 与当前平面、标度和被估计量配对的方差/逆方差 |
| `DATA-IMG-WEIGHT-001` | 旧表用于[0,1]质量权重，旧积分映射又用于逆方差；两者需按实际类型区分 |
| `DATA-IMG-SUPPORT-001` / `DATA-IMG-MASK-001` | 支持与坏像元标记；状态和几何不作统计精度 |
| `DATA-WCS-001` | WCS 描述与来源，角度单位 deg；无解显式登记 |
| `DATA-CAT-PSF-001` / `DATA-CAT-PHOT-001` | PSF/测光源表，含坐标、真实通量单位与测量状态 |
| `DATA-HIPS-SIGNAL-001` | signal HiPS，NESTED、实际线性单位/sr |
| `DATA-HIPS-VAR-001` / `DATA-HIPS-IVAR-001` | 输入帧可有来源的辅助精度；单位分别为 signal 单位平方及其倒数 |
| `DATA-UPM-MODEL-001` | 与所拟合信号同单位的加性背景模型，不能一律写探测器 ADU |
| `DATA-UPM-CONTROL-UNC-001` | UPM control cell 统计量自身的方差/逆方差，含其相关性模型与支持 |
| `DATA-REJ-MAP-001` | 具名拒绝原因或资格载体 |
| `DATA-FRAME-ID-001` | 帧身份登记；内容身份与输入 occurrence 不能混用 |
| `DATA-P3-FITS-001` | 二维 signal WCS FITS；内存行主序 [H,W]，FITS NAXIS1=W/NAXIS2=H |
| `DATA-P1-PHOTPROV-001` | p1_phot.json 逐帧应用状态、标度与来源；组级摘要不能覆盖失败帧 |
| `DATA-GAIA-001` | Gaia 行及光谱数组，实际坐标历元、光谱采样、单位与内存释放由 Gaia API 说明 |
| `DATA-COV-001` | coverage 联合 MOC 单元及输入引用，调用方数组与两阶段容量协议 |
| `DATA-HIPS-001` / `DATA-TILE-001` | HiPS properties 和 float FITS tile 的输入角色；几何/单位来自实际产品 |
| `DATA-UNC-001` | 既有不确定度容器登记；名称存在不表示完整生产噪声模型 |
| `DATA-P2-VAR-001` / `DATA-P3-UNC-001` | 旧辅助图像/扩展登记；不属于当前最终 Mosaic/Export 科学图像发布集合 |
| `DATA-P2-REJ-001` / `DATA-P3-REJ-001` | 内部样本接受位、拒绝计数及诊断；具体载体以真实调用为准 |
| `DATA-P2-PROV-001` | Mosaic 来源/能力元数据；不能凭键存在推断不确定度已验证 |

旧完整字段表、接口分支、常量与未接线测试名称保存在[来源档案](../../../实验/documentation-refactor/docs/PRODUCTS_SOURCE.txt)。历史 ID 保留可定位性；当前阶段产品政策由[交换合同](PHASE_PRODUCT_EXCHANGE.md)唯一规定。

## 3. 统一对象与机器 schema

`eng/contracts/schemas/unified/` 的13类对象为 signal、variance、ivar、source_snr、depth_m5、frame_snr、point_information、sparse_snr_layer、support、coverage、validity、rejection、provenance。其职责、精度与当前载体见统一工程对象，机器 schema 编码形状与兼容值域，不拥有物理定义。

Normalize 只发布一份 canonical frame_science.json；Catalogue、帧摘要和规则像素 SNR 网格是派生/兼容视图。available/unavailable 形状夹具中的假身份和哈希不能作为可消费科学产品。reader 要验证真实引用、模型单位/域与跨字段关系。

新 typed/JSON 用显式 status、optional/null 表示未知精度。既有 variance=0/ivar=0 的 unavailable 哨兵只在 legacy adapter 解释，不能拿0当真实零方差，也不能求倒数得到 Inf。无覆盖、未知噪声、损坏输入与排异分别表示。

## 4. 易混字段的消费者解释

积分 reducer 消费调用方传入的 value/weight/support/accepted，不负责选择科学权重。点源 `SNR²/F_ref²` 是通量信息量，变成面亮度权重还需要同条件 T；不能把它直接写成 signal 图像的通用精度。质量因子、几何支持与 UPM 控制权也具有不同单位。

UPM `raw_w=quality_factor×control_ivar` 和每 control 的归一化是当前实现步骤，科学统计意义回到[背景模型](../../science/sky/UPM.md)。`k_corr`、MAD一致化系数、帧 SNR、PSF宽度和角像元尺度分别引用专题，不能由一个泛称 sigma/weight 的列承载。

## 5. 所有权与发布

大数组由 Runtime/调用方按明确合同持有，节点借用，异步任务在回收前完成或转移所有权。持久化对象通过[产物库](ARTIFACT_STORE.md)和[原子发布](../contracts/ATOMIC_PUBLISH.md)管理，跨阶段仅消费完成磁盘产品。保留过程失败与来源，真实观测 FITS 不入 Git；构建与发行材料留存见[发行合同](../build/RELEASE.md)。
