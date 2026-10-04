# Astro Celestial Sphere Database（ACSD） 术语与单位词典

> 上游：ACSD_DESIGN.md 附录 A（术语）

本词典是**唯一术语权威**。每个核心术语恰一个含义；legacy alias 列出迁移去向。
任何文档/代码/接口与本文冲突时，以本文锚点所指的权威文件为准并回改词典——定义只有一套。
同一符号在不同正本给出不同定义时，词典只登记锚点正本的那一个，并按权威链把另一处登记为待裁决，不在词典里另立第二含义。

| term | 含义（唯一） | 单位/极性/域 | legacy alias → 迁移 | 权威锚点 |
|---|---|---|---|---|
| adu | 线性信号单位，同滤镜/增益标度下可比；ADU 域 = `BSCALE·样本 + BZERO` 的值域，未经该换算的原始存储样本不在 ADU 域内 | 信号单位 | DN → 禁用，一律写 ADU（EMVA 1288 等原文的 DN 即本仓 ADU，换算比 1:1，数值不变）；电子数标度另见 electron | docs/science/calibration/CALIBRATION.md §2.2 |
| electron | 光电子数标度；只在调用方另行给出增益 g（单位 e⁻/ADU，ADU = N_e/g）时成立，本链不建模增益，数据面一律 ADU，不作 ADU 的等价标注 | 信号单位（须增益换算） | e⁻、N_e → electron | docs/ACSD_DESIGN.md §2.2；docs/science/unified/SCIENCE_SCOPE.md |
| variance | 逐像素随机方差，Drizzle 传播 variance_p=sumVarNum/N_p²（等价参数化写法 Σ_j v_j·w_jp²·k²/D_p²，k:=D_p/N_p=pixfrac²）；D_p 写法须配按像元面积归一的权重 w'_jp=a_jp/A_pixel,j（此时 k² 已并入权重，两写法同值），把 drop 面积归一的 w_jp 代入 D_p 写法会整体偏大 1/pixfrac⁴ 倍，仅 pixfrac=1 时同值；无覆盖像元在方差分子累加中贡献 0，产品值上无覆盖与 signal 同态写 NaN | 信号单位²（ADU²；面亮度域为 ADU²/sr²） | - | docs/science/drizzle/DRIZZLE.md §3.6；docs/science/unified/DATA_SEMANTICS.md §3.3 |
| ivar | 逆方差 = 1/variance，与同承载面的 variance 严格互倒（有限域）；variance=0（方差不可用）→ ivar=0，是显式不可用标记；无覆盖 → NaN（与 signal 同态，对象级缺失登记为 null）；方差分子非有限或为负 = 产品损坏，硬失败；禁 1/0→Inf | 信号单位⁻²（面亮度域为 sr²/ADU²） | - | docs/science/unified/DATA_SEMANTICS.md §3.3 |
| pixel_weight | 像素级科学权重 = ivar（UPM/integration 共用）；数据层没有权重对象，权重是消费时按该输出像素对应样本集合现场换算的派生量；snr² 权重只用于 ablation 对照 | 无量纲 | snr²-weight → pixel_weight（ivar） | docs/science/unified/DATA_SEMANTICS.md §3.3/§3.9 |
| frame_quality_weight | 帧质量权重 = support×snr_v²（SCI-CW 域专用，非生产；权重是阶段二按天球像素对应帧集合现场算出的派生量）；生产 = 逐样本 ivar；snr = 1.0 一律按 unknown 显式标注 | 无量纲 | - | docs/science/noise_snr/NOISE_SNR.md §6.4 |
| support | 有效输入/面积贡献度，连续 [0,1]；0 = 无覆盖；只作门，不作权重 | 无量纲 | - | docs/engineering/UNIFIED_OBJECTS.md §2；docs/detail/UNIFIED_MODEL.md §2 |
| coverage | 几何/数据有效域，连续 [0,1]；0 = 无覆盖（空域）；与 support 是**各自独立的 canonical 对象**，语义不同且互不替代；也不是 rejection | 无量纲 | - | docs/engineering/UNIFIED_OBJECTS.md §2；docs/detail/UNIFIED_MODEL.md §2 |
| invalid | 非法样本判定：NaN 或 support<=0；有效样本 = finite 且 support>0 | 布尔判定 | - | docs/science/unified/DATA_SEMANTICS.md §3.2 |
| nan | 非法值唯一载体；无有效样本必须有明确 status，0 或 ±Inf 不作非法值载体 | 浮点值 | - | docs/science/unified/DATA_SEMANTICS.md §3.2 |
| bad_mask | 校准域坏点掩膜（char 数组）：**1=坏点（需修复/替换），0=好点（保留）**；判据是 bad_mask[i]≠0 即该像元判坏，不参与邻居取样、由插值结果替换 | 极性：1=bad | mask（裸用） → 必须写 bad_mask | lib/algorithms/calibration/src/cosmetic_corrector.cpp 的 `interpolate_pixels`；docs/science/algorithms/COSMETIC_ALGORITHMS.md |
| product_bit_flags | HiPS 产品位标志（AIO_HIPS_PRODUCT_VARIANCE=8，IVAR=16）；是产品位，不是像素 mask | 位标志 | - | docs/engineering/data/ARTIFACTS.md（DATA-P2-VAR-001 的 HiPS 产品位登记）；lib/infrastructure/aio/include/aio_hips.h |
| ra_dec | 天球坐标，ICRS/equatorial；RA∈[0,360)，Dec∈[-90,90] | 度；内部球面计算弧度，公共 ABI 度 | - | docs/science/unified/DATA_SEMANTICS.md §3.1 |
| pixel_coordinate | 内部像素坐标 0-based x∈[0,w-1]（y 同）；FITS 1-based xp=x+1；CRPIX = 1-based（w/2+0.5，h/2+0.5）恒成立 | 无量纲（px） | xp/yp（1-based） → 显式标注 FITS 1-based | docs/science/detection/ASTROMETRY.md §2.3 |
| healpix_ordering | NESTED 是唯一允许的 ordering，其他 ordering 一律判错；order K，nside=2^K，非法值拒绝 | 无量纲 | ring → 拒绝 | docs/science/unified/DATA_SEMANTICS.md §3.1 |
| projection | 投影 = 天球面到平面 WCS 的坐标映射类型（首批 TAN/SIN/CAR/AIT/STG/MOL/CEA/ZEA，缺省 TAN，未实现的显式报不支持）；投影与重采样算子**分名但同属 drizzle**：drizzle 前向做平面→球面、反向做球面→平面（反向实现与校验见 `docs/science/algorithms/DRIZZLE_GEOMETRY.md` 的反向 drizzle 一节）；两者都只做坐标变换与重采样，不做跨帧组合 | — | - | docs/ACSD_DESIGN.md §6.3；docs/science/drizzle/DRIZZLE.md |
| stacking | 跨帧样本组合的唯一实现是加权积分（coaddition），在阶段二执行：消费已落到天球像素的样本，先按排异给出的接受掩码筛出合格样本，再加权求和；不做任何坐标变换 | — | 排异积分 → stacking（排异与积分分名，排异是前置门） | docs/science/INTEGRATION.md；docs/science/REJECTION.md |
| frame_id | 帧身份 = truncated-64（SHA-256 of science payload identity），取前 16 hex 为 uint64；与路径/重命名无关；描述口径 = SHA-256 truncate（FNV-1a/路径派生均不适用） | uint64 | - | docs/science/unified/DATA_SEMANTICS.md §3.7 |
| signal | 科学表面亮度（float32/64），不使用 display stretch；负值保留，不自动 pedestal/clamp | ADU/sr | - | docs/science/unified/DATA_SEMANTICS.md §3.2 |
| surface_brightness | 输出面亮度 S_p=F_p/N_p（通量按**立体角**归一）；每像素常量通量与常量天空面亮度各自具名 | ADU/sr（每立体角；variance=ADU^2/sr^2，ivar=sr^2/ADU^2） | flux（混淆用法） → 显式区分 flux 与 surface_brightness | docs/science/drizzle/DRIZZLE.md §3.3 |
| calibration_units | t_expo：s；K：无量纲；flat_norm：无量纲（median=1.0，floor 0.1）；sigma：MAD 倍数，无量纲；像素坐标无量纲 | 见含义 | - | docs/science/calibration/CALIBRATION.md §4 |
