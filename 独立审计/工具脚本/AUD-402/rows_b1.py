# -*- coding: utf-8 -*-
"""AUD-402 authored rows, batch 1: defaults.json registration surface (59 fields).

Position convention: the defaults.json line carrying "value"; 备注 carries the
other sides' values with their own 路径:行 when a code/template/CLI/schema side
exists.  Four sides = 代码兜底 / defaults.json / 出厂模板 / CLI 生成骨架（+ 合同
schema default 作第五侧）。
"""

DJ = "eng/packaging/config/defaults.json"
NM = "lib/algorithms/noise_snr/cpp/src/noise_model.cpp"
RJ = "lib/algorithms/coverage/src/rejection.cpp"

ROWS = [
    ["calibration.dark_light_exposure_tolerance「, DJ + 」:20「, 」5", s",
     "暗场/亮场曝光时长一致性；文档侧只给 K=t_light/t_dark 语义「, 」未标注", 未标注（预检提示级，不阻断）",
     "无来源（负责人裁决未落 SCI 条文）",
     "defaults.json:24 记「负责人裁决 2026-09-16」；docs/science/CALIBRATION.md:19,21 无数值",
     "待确认", "normalize 预检（橘色 optimize 提示）",
     "四侧不一致：仅 defaults.json 一侧有值，lib/ 生产代码零命中（键名 leaf=dark_light_exposure_tolerance，全仓 0 命中）⇒ 无实现侧兜底值可比对，也无消费点；保守方向：容差取更小更安全（更严的匹配要求），影响面=预检提示与暗场优化建议路径"],

    ["calibration.master_flat_median_range「, DJ + 」:31「, 」[0.5, 2.0]", 1（无量纲）",
     "已归一 master flat 的中位数判定带；合同值 1.0「, 」带内/带外二值判定，无精度要求", median(flat) ∈ [0.5,2.0] 视为已归一",
     "可公式导出（数量级判带）", "docs/science/CALIBRATION.md:103；docs/algorithms/CALIBRATION_ALGORITHMS.md:56,495（DISP-CAL-013）",
     "文献值", "flat 消费边界（DATA 拒绝 rc=2）",
     "多侧同值：代码兜底 lib/include/astrocs/core/master_unit_guard.h:31 同带；由 eng/tools/quality/check_master_unit_guard.py 机器核对 ⇒ 属已登记且已闭合一侧；带 [0.5,2.0] 本身是数量级判据非科学容差"],

    ["detection.threshold_sigma「, DJ + 」:48「, 」5.0", sigma",
     "threshold = median(img) + 5.0·bgnoise（全局检测阈值）「, 」未标注（σ 倍数，fp32 路径见实现）", >0",
     "有文献（本仓 SCI 冻结）「, 」docs/science/STAR_DETECTION.md:21「, 」文献值", star detection 全局阈值",
     "代码侧异名同值：lib/algorithms/star_detection/src/sdet_api.cpp:1779 `T(5.0) * bgnoise`、:1827 `5.0 * bgnoise`、lib/algorithms/integration/v6_phase1/src/phase1_product.cpp:448 `detection_threshold_sigma = 5.0` ⇒ 键名零命中但值以三处字面量存在，无单一来源（改默认需同步三处）"],

    ["psf.default_model「, DJ + 」:62「, 」moffat4「, 」枚举 id「, 」点源响应模型标识「, 」不适用（枚举）", 唯一实现值",
     "有文献（本仓 SCI）「, 」docs/science/PSF.md:9；:105（ALG-STARPSF-001）「, 」文献值", PSF 拟合路径选择",
     "键名在 lib 生产代码零命中；合同侧 eng/contracts/schemas/phase_config_normalize.schema.json:474-479 algorithm_psf_model.enum=[\"moffat4\"]（无 default 声明）⇒ 值唯一性由 enum 保证，不产生第二侧数值"],

    ["psf.moffat_beta「, DJ + 」:76「, 」4.0「, 」1（无量纲）「, 」椭圆 Moffat 形状指数 β「, 」精确等于 4.0（改动属不可接受变化）", β=4",
     "有文献（本仓 SCI 冻结 + Moffat 1995 惯例）「, 」docs/science/PSF.md:94（改 β≠4 属不可接受变化）「, 」文献值", PSF 模型",
     "键名 lib 零命中；实现侧以 kMoffat4FwhmFactor / FWHM 因子承载（lib/algorithms/noise_snr/cpp/src/snr_estimator.cpp:72），β 值本身在 moffat 拟合函数内 ⇒ 未做成配置键，属冻结常数硬编码（合规：SCI 已冻结）"],

    ["noise.patch_grid「, DJ + 」:90「, 」[8, 8]「, 」patch（格数）「, 」噪声估计 patch 网格行列数「, 」整数，实现侧下限钳位 2", ≥2×2",
     "有文献（SCI 冻结定义）「, 」docs/science/NOISE_MODEL.md:54（§5 冻结 8×8）；:39,:52「, 」文献值", Phase1 patch 噪声估计",
     "代码兜底同值：noise_model.cpp:1248-1249 patch_grid_x/y=8；头文件注释 snr_estimator.h:131 亦记 8；差异登记 docs/algorithms/NOISE_ESTIMATION.md:180（下限钳位 2）⇒ 三处同值但各写各的，无单一来源"],

    ["noise.source_mask_radius_px「, DJ + 」:107「, 」10「, 」px「, 」逐星掩膜半径硬上界的 r0 项「, 」≥1（max(1,r0) 参与上界式）", r0∈[1,∞)，与 scale 相乘得 rmax=60 px",
     "可公式导出（上界式 rmax=max(1,r0)·max(1,scale)）", "docs/science/NOISE_MODEL.md:86（§5a 硬上界）；docs/algorithms/NOISE_ESTIMATION.md:134（拆分 10·6，实现锚非权威）",
     "公式导出「, 」Phase1 星点掩膜",  MASK-001 §1.4 判定旧 source_ref 为循环引用后改指 SCI；代码兜底 noise_model.cpp:1250 = 10.0（同名同值，第二处 r0 用法 :824 `std::max(1.0, c.source_mask_radius_px)`）"],

    ["noise.mask_radius_scale「, DJ + 」:121「, 」6「, 」1（无量纲）「, 」rmax 上界式的倍率项「, 」≥1", scale∈[1,∞)，与 r0 相乘",
     "可公式导出（同 rmax 式）「, 」docs/science/NOISE_MODEL.md:86（§5a）「, 」公式导出", Phase1 星点掩膜上界",
     "10·6=60 的组合是文档陈述的硬上界，两键单独取值（10 与 6）本身无独立权威 ⇒ 若只需 60 的上界，可由 60 = rmax 单键承载并消除分解的任意性；代码兜底 noise_model.cpp:1251 = 6.0"],

    ["noise.clip_sigma「, DJ + 」:135「, 」5.0「, 」sigma「, 」patch 内 |v-med|>clip_sigma·σ 掩膜「, 」≥1", >0",
     "有文献（SCI 冻结定义）「, 」docs/science/NOISE_MODEL.md:56（§5 稳健裁剪 5σ）；:126（§9 ≤2 轮）「, 」文献值", Phase1 patch 裁剪",
     "代码兜底异名同值：noise_model.cpp:1252 `cfg->cosmic_clip_sigma = 5.0`；**同名不同值风险**：lib/algorithms/coverage/src/sky_plane.cpp:230 `c.clip_sigma = 3.0`（Phase2 patch 估计器）与 sky_plane.h:98 区同名键值 3.0 ⇒ 两阶段各自一套 clip_sigma，配置键面只登记了 Phase1 的 5.0，Phase2 的 3.0 无 defaults 键（见该行）"],

    ["noise.min_patch_samples「, DJ + 」:149「, 」64「, 」count「, 」patch 合格样本数下限「, 」≥1", ≥1；受 §8 全帧兜底收紧到 max(64,9216)",
     "有文献（SCI §4 默认 64）「, 」docs/science/NOISE_MODEL.md:52「, 」文献值", Phase1 patch 噪声估计",
     "代码兜底同值：noise_model.cpp:1253；实现锚 snr_estimator.h:142 注释「默认 64」；调度侧 lib/infrastructure/scheduler/src/module_adapters.cpp:6599 亦以 64 兜底 ⇒ 三处同值字面量，无单一来源"],

    ["noise.max_clip_rounds「, DJ + 」:163「, 」2「, 」rounds「, 」patch 内裁剪迭代轮数上限「, 」≥0", 0..N",
     "有文献（SCI §5 冻结 ≤2 轮）「, 」docs/science/NOISE_MODEL.md:56,:126「, 」文献值", Phase1 patch 裁剪",
     "代码兜底同值：noise_model.cpp:1254；无 schema 侧 ⇒ 两处同值"],

    ["noise.saturation_level「, DJ + 」:177「, 」0「, 」ADU「, 」0/负/非有限 = 未提供电平（unset），>0 = 饱和电平", 整数/浮点；0 不表示「无饱和」",
     "未提供时优先级：显式 cfg > FITS SATURATE > DATAMAX", "需实验标定（缺失时的代价已由实验量化）",
     "docs/science/NOISE_MODEL.md:52（§4 饱和域，claim SC-008/SAT-001）；SAT-001 实测 run/PROJECT-GOVERNANCE-01/SAT-001（帧平均 ivar 比 0.218、平台 patch 方差 6.87e8 vs 真值 25）",
     "实验标定", "Phase1 blank-sky 统计与逐像素权重场",
     "代码兜底同值：module_adapters.cpp:6383 `saturation_level = 0.0`；未提供路径必须写显式降级声明 photo_stats.NOISE_SATURATION_FILTER=DISABLED_NO_METADATA ⇒ 缺陷不静默；对照 LSST ip_isr doSaturation 默认 True（defaults.json:187 note）"],

    ["noise.spatial_field_enabled「, DJ + 」:191「, 」1「, 」bool(0/1)「, 」1 且 n_ctrl≥4 用 LS 平面 var(x,y)=a+b·x+c·y，否则全局中位数兜底「, 」不适用（开关）", {0,1}",
     "有文献（SCI §4 启用条件）「, 」docs/science/NOISE_MODEL.md:47；docs/algorithms/NOISE_ESTIMATION.md:20,:38「, 」文献值", Phase1 空间噪声场",
     "代码兜底同值：noise_model.cpp:1255 `enable_spatial_field = 1`；头文件 snr_estimator.h 字段名 enable_spatial_field（异名）⇒ 键名 leaf 与符号名不统一，靠人肉对齐"],

    ["noise.variance_floor「, DJ + 」:205「, 」1e-12「, 」ADU^2「, 」ivar = 1/max(var, floor) 的分母下界「, 」保证 ivar 有限；对 σ≈1e-6 ADU 量级以下的场不起作用", >0",
     "无来源（数量级拍定，未给物理/数值推导）", "docs/science/NOISE_MODEL.md:23（§2 方差下界 1e-12）、:125（§9 保 ivar 有限）、:29（单位 ADU²）",
     "待确认「, 」Phase1 variance/ivar 全场", 代码兜底同值：noise_model.cpp:1256；**同类下界在他处另有取值**：lib/algorithms/coverage/src/rejection.cpp:1245 `p.normalization_floor = 1e-12`、实验侧 p1noise_oracle.hpp:112 `variance_floor : 0.0`、p1noise_tests_core.cpp:776 用 1.0e-46 ⇒ 同一语义的 epsilon 存在 1e-12/0/1e-46 三档，1e-12 未说明为何不是 ADU² 场景下的 1e-6 或 machine-eps 推导；保守方向：floor 越小越接近真实 ivar，但除零风险↑，影响面=全部下游权重"],

    ["noise.mask_k_sigma「, DJ + 」:219「, 」0.1「, 」sigma「, 」掩膜边缘残余面亮度系数：逐星半径由 源面亮度 = k·σ_bg 导出「, 」由污染预算导出（σ_bg 偏差 ≤0.5%）", >0，k 越大掩膜越大",
     "可公式导出（配合 SCI §11 源污染 oracle）", "docs/science/NOISE_MODEL.md:84（§5a 冻结 k=0.1）；MASK-001 §3.3 实测 k=0.1、r=10 px 偏差 +0.13%",
     "公式导出「, 」Phase1 逐星掩膜半径", 代码兜底同值：noise_model.cpp:1258 与 :826 二级兜底 `(c.mask_k_sigma > 0.0) ? ... : 0.1` ⇒ 同一默认值在实现内出现两次（配置层+使用层），改一处即不一致"],

    ["noise.mask_r_min_px「, DJ + 」:233「, 」1.5「, 」px「, 」逐星半径下界 r_min = max(1.5 px, 0.75·FWHM_i)「, 」≥1（约束写 ≥1 而值 1.5）", >0",
     "有文献（SCI §5a 同式）「, 」docs/science/NOISE_MODEL.md:85「, 」文献值", Phase1 逐星掩膜半径下界",
     "代码兜底同值：noise_model.cpp:1259 + 二级兜底 :827；约束「>= 1」与实际默认 1.5 不同源（约束是合法性下界，默认是取值），登记时易混"],

    ["noise.mask_fwhm_floor_scale「, DJ + 」:247「, 」0.75「, 」1（无量纲）「, 」r_min 的 FWHM 项系数 0.75·FWHM_i「, 」>0", >0",
     "有文献（SCI §5a 同式）「, 」docs/science/NOISE_MODEL.md:85「, 」文献值", Phase1 逐星掩膜半径下界",
     "代码兜底同值：noise_model.cpp:1260 + 二级兜底 :828；0.75 与 PSF 核心覆盖半径的关系未给推导（为何不是 1·FWHM 或 2σ）⇒ 属「可公式导出但未写导出式」的候选"],

    ["noise.mask_budget_min_patches「, DJ + 」:261「, 」8「, 」count「, 」天空预算：合格 patch 数下限 n_qualified ≥ 8「, 」≥4（空间场可辨识需 ≥4 非共线控制点）", ≥1",
     "可公式导出（下限 4 = 平面 LS 参数数；8 = 中位数稳健余量）「, 」docs/science/NOISE_MODEL.md:87（§5a）「, 」文献值", Phase1 掩膜天空预算",
     "代码兜底同值：noise_model.cpp:1261 + 二级兜底 :829；4 与 8 的关系（2× 余量）无量化依据 ⇒ 8 属"需实验标定"档，但文档以冻结值给出"],

    ["noise.mask_budget_min_sky「, DJ + 」:275「, 」9216「, 」px「, 」未被掩膜的合法像素数下限 N_sky「, 」SE(σ̂)/σ ≤ 1.5%", ≥1",
     "可公式导出（SE(σ̂)/σ ≈ 1.44/√N_sky ⇒ N=9216）", "docs/science/NOISE_MODEL.md:87；推导记于 defaults.json:285（c≈1.152/√N、中位数效率 1.25）与 MASK-001 §5.3/§3.5",
     "公式导出「, 」Phase1 天空预算与全帧兜底收紧", 代码兜底同值：noise_model.cpp:1262 + 二级兜底 :830（9216ull）；导出式已写出 ⇒ 9216 可按式重算（(1.44/0.015)²=9216），属可推导却被硬编码的典型（应由公式生成或至少加断言）"],

    ["rejection.sigma.lower_sigma「, DJ + 」:289「, 」4.0「, 」sigma「, 」sigma-clip 低侧阈值（正数）「, 」未标注", >0",
     "有文献（SCI 阈值冻结锚点）「, 」docs/science/REJECTION.md:97（§5 sigma/winsorized/averaged = 4.0/3.0/8）「, 」文献值", Phase2 排异（多值堆）",
     "代码兜底同值：rejection.cpp:1246；对称性异常：**lower 4.0 > upper 3.0**（低侧比高侧松），文档与实现一致但语义方向需科学侧确认（本表不判，交 AUD-203）"],

    ["rejection.sigma.upper_sigma「, DJ + 」:303「, 」3.0「, 」sigma「, 」sigma-clip 高侧阈值（正数）「, 」未标注", >0",
     "有文献（同三元组）「, 」docs/science/REJECTION.md:97「, 」文献值「, 」Phase2 排异", 代码兜底同值：rejection.cpp:1246"],

    ["rejection.sigma.max_iterations「, DJ + 」:317「, 」8「, 」iterations「, 」sigma-clip 迭代上限「, 」≥1", ≥1",
     "有文献（同三元组）「, 」docs/science/REJECTION.md:97「, 」文献值", Phase2 排异",
     "代码兜底同值：rejection.cpp:1246；**同名不同值族**：max_iterations 在 upm.cpp:270/287/2460=100、json_config.h:50=100、stage2_common.h:65 max_irls_iterations=100、ipv_robust_refine.h:48=10、sky_plane.cpp:457=30 ⇒ 五套迭代上限共用一个键名，配置键面只覆盖 rejection 的 8"],

    ["rejection.winsorized.lower_sigma「, DJ + 」:331「, 」4.0「, 」sigma「, 」winsorized 低侧阈值「, 」未标注", >0",
     "有文献（三元组共用）「, 」docs/science/REJECTION.md:97「, 」文献值「, 」Phase2 排异", 代码兜底同值：rejection.cpp:1247-1248；三方法（sigma/winsorized/averaged）取值完全相同 ⇒ 三份登记一份事实，可合并"],

    ["rejection.winsorized.upper_sigma「, DJ + 」:345「, 」3.0「, 」sigma「, 」winsorized 高侧阈值「, 」未标注", >0",
     "有文献（三元组共用）「, 」docs/science/REJECTION.md:97「, 」文献值「, 」Phase2 排异", 代码兜底同值：rejection.cpp:1247"],

    ["rejection.winsorized.max_iterations「, DJ + 」:359「, 」8「, 」iterations「, 」winsorized 迭代上限「, 」≥1", ≥1",
     "有文献（三元组共用）「, 」docs/science/REJECTION.md:97「, 」文献值「, 」Phase2 排异", 代码兜底同值：rejection.cpp:1248"],

    ["rejection.averaged_sigma.lower_sigma「, DJ + 」:373「, 」4.0「, 」sigma「, 」averaged_sigma 低侧阈值「, 」未标注", >0",
     "有文献（三元组共用）「, 」docs/science/REJECTION.md:97「, 」文献值", Phase2 排异",
     "代码兜底同值：rejection.cpp:1249；实现侧结构体字段名为 `p.averaged`（无 _sigma 后缀）⇒ 键名与符号名不一致"],

    ["rejection.averaged_sigma.upper_sigma「, DJ + 」:387「, 」3.0「, 」sigma「, 」averaged_sigma 高侧阈值「, 」未标注", >0",
     "有文献（三元组共用）「, 」docs/science/REJECTION.md:97「, 」文献值「, 」Phase2 排异", 代码兜底同值：rejection.cpp:1249"],

    ["rejection.averaged_sigma.max_iterations「, DJ + 」:401「, 」8「, 」iterations「, 」averaged_sigma 迭代上限「, 」≥1", ≥1",
     "有文献（三元组共用）「, 」docs/science/REJECTION.md:97「, 」文献值「, 」Phase2 排异", 代码兜底同值：rejection.cpp:1250"],

    ["rejection.linear_fit.lower_sigma「, DJ + 」:415「, 」5.0「, 」sigma「, 」linear_fit 低侧阈值「, 」未标注", >0",
     "有文献（SCI §5 冻结 linear_fit 5.0/3.5/8）「, 」docs/science/REJECTION.md:59「, 」文献值", Phase2 排异",
     "代码兜底同值：rejection.cpp:1251，且该处注释标「WBPP Light 默认」（来源是 Siril 惯例而非本仓 SCI）⇒ 同一数值两处来源叙述不一致（SCI:59 vs 实现注释 WBPP），登记为来源叙述冲突候选"],

    ["rejection.linear_fit.upper_sigma「, DJ + 」:429「, 」3.5「, 」sigma「, 」linear_fit 高侧阈值「, 」未标注", >0",
     "有文献（SCI §5）「, 」docs/science/REJECTION.md:59；实现侧注释 rejection.cpp:1058 记 WBPP Light 默认「, 」文献值", Phase2 排异",
     "同 lower_sigma 的来源叙述冲突"],
]
