# AstroCS 术语与单位词典（DOC-001 冻结）

本词典是**唯一术语权威**。每个核心术语恰一个含义;legacy alias 列出迁移去向。
任何文档/代码/接口与本文冲突时,以本文锚点所指的权威文件为准并回改词典——禁止两套定义并存。

| term | 含义（唯一） | 单位/极性/域 | legacy alias → 迁移 | 权威锚点 |
|---|---|---|---|---|
| adu | 线性信号单位,同滤镜/增益标度下可比 | 信号单位 | DN → 禁用,一律写 ADU | docs/science/CALIBRATION.md#27 |
| electron | DRIZZLE 域允许的信号等价标注(增益标定后) | 信号单位(=ADU 等价标注) | e⁻ → electron | docs/science/DRIZZLE.md#27 |
| variance | 逐像素随机方差,Drizzle 传播 variance_p=Σ v_j·w_jp²/D_p²;无覆盖像素=0 | 信号单位²(ADU²) | - | docs/contracts/DATA_SEMANTICS.md#4a |
| ivar | 逆方差=1/variance;variance=0/缺失 → ivar=0(显式不可用,禁止伪装);NaN/负 variance=产品损坏 | ADU⁻² | - | docs/contracts/DATA_SEMANTICS.md#4a |
| pixel_weight | 像素级科学权重=ivar(UPM/integration/ACR);legacy snr² 权重仅 ablation 域 | 无量纲 | snr²-weight → pixel_weight(ivar) | docs/contracts/DATA_SEMANTICS.md#4a |
| frame_quality_weight | 帧质量权重=support×snr_v²(SCI-CW 域专用);禁止 snr=1.0 伪装 unknown | 无量纲 | - | docs/science/CONTROL_WEIGHT_SNR.md#42 |
| support | 覆盖/有效支持度,连续 [0,1];0=无覆盖 | 无量纲 | coverage → support | docs/contracts/DATA_SEMANTICS.md#4 |
| invalid | 非法样本判定:NaN 或 support<=0;有效样本=finite 且 support>0 | 布尔判定 | - | docs/contracts/DATA_SEMANTICS.md#4 |
| nan | 非法值唯一载体;无有效样本必须有明确 status,禁止静默输出 0 或 ±Inf | 浮点值 | - | docs/contracts/DATA_SEMANTICS.md#4 |
| bad_mask | 校准域坏点掩膜(char 数组):**1=坏点(需修复/替换),0=好点(保留)** | 极性:1=bad | mask(裸用) → 必须写 bad_mask | lib/calibration/src/cosmetic_corrector.cpp#158 |
| product_bit_flags | HiPS 产品位标志(AIO_HIPS_PRODUCT_VARIANCE=8, IVAR=16);是产品位,不是像素 mask | 位标志 | - | docs/contracts/DATA_SEMANTICS.md#4a |
| ra_dec | 天球坐标,ICRS/equatorial;RA∈[0,360),Dec∈[-90,90] | 度;内部球面计算弧度,公共 ABI 度 | - | docs/contracts/DATA_SEMANTICS.md#1 |
| pixel_coordinate | 内部像素坐标 0-based x∈[0,w-1](y 同);FITS 1-based xp=x+1;CRPIX=1-based (w/2+0.5,h/2+0.5) 恒成立 | 无量纲(px) | xp/yp(1-based) → 显式标注 FITS 1-based | docs/science/ASTROMETRY.md#14 |
| healpix_ordering | NESTED 是唯一允许 ordering(ring 未迁移);order K,nside=2^K,非法值拒绝 | 无量纲 | ring → 拒绝 | docs/contracts/DATA_SEMANTICS.md#2 |
| frame_id | 帧身份=truncated-64(SHA-256 of science payload identity),取前 16 hex 为 uint64;与路径/重命名无关;禁止描述为 FNV-1a/路径派生 | uint64 | - | docs/contracts/DATA_SEMANTICS.md#5 |
| signal | 科学表面亮度(float32/64),不使用 display stretch;负值保留,不自动 pedestal/clamp | 信号单位 | - | docs/contracts/DATA_SEMANTICS.md#4 |
| surface_brightness | 输出面亮度 S_p=F_p/D_p(通量按覆盖面积归一);禁止把每像素常量通量与常量天空面亮度混淆 | 信号单位/px² | flux(混淆用法) → 显式区分 flux 与 surface_brightness | docs/science/DRIZZLE.md#46 |
| calibration_units | t_expo:s;K:无量纲;flat_norm:无量纲(median=1.0,floor 0.1);sigma:无量纲(MAD 倍数);像素坐标无量纲 | 见含义 | - | docs/science/CALIBRATION.md#27 |
