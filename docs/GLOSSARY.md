# ACSD 术语与单位

本页为[最高设计](ACSD_DESIGN.md)的术语索引。科学定义以[数据语义](science/unified/DATA_SEMANTICS.md)及下表专题正本为准；机器字段和身份由工程合同维护，词典不另立序列化规则。

| 术语 | 含义与单位 | 正本 |
|---|---|---|
| ADU / DN | 探测器数字计数单位；FITS的BSCALE/BZERO只决定存储换算，不保证物理单位为ADU | [校准](science/calibration/CALIBRATION.md) |
| electron / gain | 电子计数；原生gain单位e⁻/ADU，有来源时用于计数和噪声转换 | [噪声](science/noise_snr/NOISE_SNR.md) |
| linear_unit | 当前积分信号的实际数值尺度，可为原生计数、相对或响应标定量 | [数据语义](science/unified/DATA_SEMANTICS.md) |
| signal / surface_brightness | 球面已成像面亮度，[linear_unit]/sr；负值与零合法 | [Drizzle](science/drizzle/DRIZZLE.md) |
| variance / covariance | 具名估计量的方差/协方差；单位为该量平方，条件、空间域和缺项须明确 | [噪声](science/noise_snr/NOISE_SNR.md) |
| ivar | 可用且正有限方差的倒数；单位为该量的负二次幂。未知方差不是物理零方差 | [数据语义](science/unified/DATA_SEMANTICS.md) |
| source_snr | 实际源通量除以其误差，无量纲 | [Normalize](science/normalize/NORMALIZE_SCIENCE.md) |
| reference_snr | 固定参考源、局部模板/背景/噪声条件下F_ref/√V_F | [噪声](science/noise_snr/NOISE_SNR.md) |
| frame_snr_ref | 空间参考精度的帧级摘要，不代替局部controls | [Normalize数据合同](engineering/contracts/NORMALIZE_DATA.md) |
| sparse reference controls | Drizzle后球面参考精度记录，正式载体frame_science.json | [Normalize数据合同](engineering/contracts/NORMALIZE_DATA.md) |
| point_information | I_F=1/V_F=SNR_ref²/F_ref²，单位[F]⁻² | [噪声](science/noise_snr/NOISE_SNR.md) |
| surface_transfer | 同条件T=V_B/V_F，单位[B]²/[F]²；参考源T不能无条件用于实际场景 | [噪声](science/noise_snr/NOISE_SNR.md) |
| surface precision | w_B=1/V_B，单位[B]⁻²；与点源信息和背景控制精度分名 | [噪声](science/noise_snr/NOISE_SNR.md) |
| background control precision | 特定背景位置估计量的逆方差，不是点源SNR | [UPM](science/sky/UPM.md) |
| geometric weight | a_jp/A_drop,j，无量纲的drop分配系数，不是统计逆方差 | [Drizzle](science/drizzle/DRIZZLE.md) |
| support / coverage | 必须说明是实际覆盖并集、累计面积、支持分数还是模型有效域；不以同名代替定义 | [数据语义](science/unified/DATA_SEMANTICS.md) |
| validity / rejection | 前者描述数据或模型可用性，后者描述异常样本选择；与覆盖及真实零值不同 | [排异](science/integration/REJECTION.md) |
| NaN | 浮点信号无数据/不可计算位置的表示；JSON未知数用null及状态，Inf不是有效产品值 | [数据语义](science/unified/DATA_SEMANTICS.md) |
| bad_mask | 校准/修复域掩膜，1=坏点、0=好点；修复不等于恢复独立观测 | [校准](science/calibration/CALIBRATION.md) |
| PSF / h | 单位点源通量的有效响应；平面为像元积分P，球面h=RP，二者单位不同 | [噪声](science/noise_snr/NOISE_SNR.md) |
| solver_detections | IPV内部盲检的天测候选，不先依赖已解WCS | [Normalize](science/normalize/NORMALIZE_SCIENCE.md) |
| science_sources | 已解WCS及Gaia引导的科学测光/PSF源表 | [Normalize](science/normalize/NORMALIZE_SCIENCE.md) |
| ICRS / RA / Dec | 外部天球坐标以度表示，RA∈[0,360)、Dec∈[−90,90]；坐标系与观测历元分别记录 | [数据语义](science/unified/DATA_SEMANTICS.md) |
| pixel coordinate / CRPIX | 内部数组中心0基；FITS像元中心1基；CRPIX为合法参考像元，未必位于图像中心 | [Normalize数据合同](engineering/contracts/NORMALIZE_DATA.md) |
| HEALPix / NESTED | 球面等面积层级网格及本项目采用的排序；leaf order与tile order分名 | [Drizzle](science/drizzle/DRIZZLE.md) |
| HiPS | 标准球面目录、tile与多分辨率表示；多阶层级不等于多个科学通道 | [投影](science/projection/HIPS_TO_FITS.md) |
| projection / resampling | WCS坐标映射与已有信号的数值重采样；Export不反演原始探测器数据 | [重采样](science/resample/RESAMPLE.md) |
| stacking / coaddition | 阶段二对经校正和选择的多帧样本做合成 | [集成](science/integration/INTEGRATION.md) |
| UPM | 球面相对背景/差模模型，按实际重叠连通分量处理规范 | [UPM](science/sky/UPM.md) |
| response / mixed response | 逐帧光谱响应与组合规则；混合响应可随位置及有效样本改变 | [测光](science/photometry/PHOTOMETRY.md) |
| frame_id / provenance | 输入occurrence的稳定帧身份与处理来源链；源hash另存，不把重复输入合并 | [Normalize数据合同](engineering/contracts/NORMALIZE_DATA.md) |
| development snapshot | 对科学对象的被动观测，不作为下游科学输入 | [开发模式](engineering/architecture/DEV_MODE.md) |

阶段一发布逐帧signal及必要科学模型/controls；阶段二最终仅signal HiPS；阶段三为普通二维WCS FITS。支持、方差及排异诊断的内部存在不改变最终产品通道定义。
