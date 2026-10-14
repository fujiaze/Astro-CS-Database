# 统一数据对象与序列化登记

物理定义来自 [DATA_SEMANTICS.md](../science/unified/DATA_SEMANTICS.md) 与科学专题。schema 定义机器形状，不能以机器 description 的历史公式反向推翻科学正本。Normalize 复合对象、字段和能力状态以 [NORMALIZE_DATA.md](contracts/NORMALIZE_DATA.md) 为准。

## 科学对象

| 对象 | 估计量/单位 | 用途 |
|---|---|---|
| signal | 当前线性单位/sr 或明确的平面积分单位 | 观测信号 |
| variance/ivar | 对应 signal 的随机方差/逆方差 | 同被估计量精度；不能混用点源/背景量 |
| source_snr | 实际某源 F_hat/sigma_F | 逐源诊断 |
| frame_snr | 具名 reference 下的帧摘要 | 不替代空间控制点/最终像元权重 |
| depth_m5 | 明确噪声模型下解出的 5σ 深度 | 诊断；源泊松依 F 时须解 SNR(F)=5 |
| point_information | 1/V_F | 点源通量条件精度，含已声明 covariance/nuisance |
| sparse_snr_layer | 空间参考精度的像素网格兼容视图 | 不能自动替代球面 canonical controls |
| support/coverage | 具名有效域/累计面积或分数 | 几何，不是统计精度 |
| validity/rejection | 像元/模型状态与异常选择 | 不冒充信号或噪声 |
| provenance | 来源、输入/模型身份、缺项和能力 | 保持科学解释 |

计数 ADU、photo-scaled 线性单位与 SI 量不可一概写成 ADU。一般单位随真实 signal 变化；已有 v1 schema 中固定 ADU 的 description 是待迁移实现词表。variance=0/ivar=0 的已有 unavailable 编码只在 legacy adapter 解释，新 typed/JSON 合同用显式 status 和 optional/null，禁止把未知变成真实零方差。

## schema 与兼容

eng/contracts/schemas/unified/*.schema.json 保留既有对象/reader 的 v1 形状；它们不表示全部生产模型已经完成。产品包可以组合复用这些科学量，没有“两个 schema 属性名一律不得重叠”这样的科学要求。各 schema 的对象判别式与角色须明确，形状与科学语义分别验证。

阶段一的正式复合序列化为 eng/contracts/schemas/normalize_frame_science.schema.json，只发布一份 frame_science.json。Catalogue 浏览、legacy frame_snr、像素网格 SNR 均为派生/兼容读面；不得与它并列成为第二份 canonical 精度。

新 schema/产品关系登记于 eng/contracts/data/unified_object_compatibility_map_v1.json。现有 v1 示例用于读形状兼容；新增 available/unavailable 示例是假身份/hash的形状夹具，不是真实可消费科学产品。执行者实现 serializer/reader 时须检查引用文件、单位、WCS、模型域与跨字段恒等式。

## 精度、重建与字段语义

大面默认 FP32，WCS、稀疏 controls、标定参数及所有相关数值 metadata FP64。AIO 一个全局 precision mode 不能表达两者，目标按对象 dtype 处理而不修改最高设计来迁就实现。

阶段二现场重建算法不由输入 schema 的缺省 natural_bicubic_spline 冻结。输入传测量、reference、support 和 noise/PSF 模型；实际 consumer 记录生效算子/参数与预测支持。不因 HST 名称直接切换重建方法，不凭操作员声明代替几何和留出数值证据。重建 I_F 后必须有相同条件下的 T 才能得面亮度 precision，不能直接令 w_B=SNR²/F_ref²。

字段需结合明确类型表达统计量；typed ReferenceControl.reference_snr 不因含 snr 字符串而违规。模糊的是跨对象共享一个没有上下文的 weight，而不是所有有名类型内的常规字段。工作配置、机器画像和运行清单各守作用域；产品能力状态不设置形式审批门。
