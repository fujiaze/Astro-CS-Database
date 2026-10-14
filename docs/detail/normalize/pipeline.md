# Normalize 实现细节入口（不再另立阶段一设计）

> **状态：实现参考/文档导航。** 原文件曾以“Phase1 目标态详细设计”为标题，自己定义从校准到 Drizzle 的完整流程、参考 F0、master 方差、产品清单等。其中部分条款与现行科学正本冲突。2026-10-14 收敛后，**已在本文件移除这些重复、相互冲突的定义**；旧正文仍可通过 Git 历史读取，但不得当作当前设计。所有修改优先进入科学/工程对应的唯一正本。

## 1. 单一权威阅读路径

1. docs/ACSD_DESIGN.md §4：用户目标、三个命令、Normalize 输入输出。
2. **docs/science/normalize/NORMALIZE_SCIENCE.md**：唯一定义完整阶段一科学因果链、IPV WCS、Gaia XP、PSF、canonical reference SNR、Drizzle 和数据状态。
3. **docs/engineering/architecture/NORMALIZE_PIPELINE.md**：真实 C++ 的 p1_op_*、IPV、Typed 数据依赖与工程修改结构。
4. docs/science/unified/UNIFIED_SCIENCE_MODEL.md 和 DATA_SEMANTICS.md：通用观察模型、坐标/单位/支持域。
5. docs/engineering/governance/NORMALIZE_CONVERGENCE.md：各旧文档和代码之间的不一致及哪些结论被废弃。

实现细节仅在专题及实际源码里维护：science/calibration/CALIBRATION.md、science/calibration/CCD_DEFECT.md、science/detection/ASTROMETRY.md、science/detection/STAR_DETECTION.md、science/psf/PSF.md、science/photometry/PHOTOMETRY.md、science/noise_snr/NOISE_SNR.md、science/drizzle/DRIZZLE.md；模块接口可以看 docs/detail/registry/acsd.phase1.*，但它们的旧 API 与源码不一定完全一致。

## 2. Normalize 生产节点对应关系

| 概念节点 | 当前代码生产入口 | 中间对象/科学职责 |
|---|---|---|
| calibration | p1_op_calibrate | calibrated linear plane/master provenance |
| cosmetic | p1_op_cosmetic | cleaned + validity |
| plate solve | p1_op_wcs / IPV | solved WCS/TAN+SIP 或带原因的 unsolved |
| source + PSF | p1_op_star_psf_impl | Gaia-guided sources / PSF |
| photometry | p1_op_photometry | Gaia XP per-frame scale applied once |
| noise/SNR | p1_op_noise | 噪声条件、帧级 SNR；canonical sparse controls 尚未闭合 |
| drizzle | p1_op_drizzle | 已解 WCS、已施加 scale 的球面映射 |
| writer | p1_op_writer | 单帧 HiPS、model/provenance 和产品清单 |

它们的正式运行入口通过 scheduler P1NodeModule 调度。lib/phase1_session/p1_session.cpp 仅为校准/坏点局部会话，**不是**另一条 WCS→HiPS 完整产线。文件 p1_wcs.json/p1_phot.json 等目前是阶段内有实际读者的工作数据，不能作为 DEV_MODE 控制的“可关闭诊断产物”。

## 3. 旧文件中不再具有独立效力的主张

| 旧内容/标题 | 现在解释 |
|---|---|
| “原始 FITS 缺几何的帧不能处理” | 错误归因；应通过 **IPV 真正解析** 后再判断 solved/unsolved |
| 校准母版方差总是完全传播 | 物理目标保留，但旧生产并未全面建模；按 calibration §5.7 的 unmodelled 状态 |
| 全部阶段都固定 ADU/sr | 只有数值仍是 ADU 线性单位时成立；photo scale 可以更改所选模型尺度 |
| 一切图像使用相同的组内 ADU 通量中位数 F0 | 不等同于跨仪器共同参考 SED；正确 F_ref,k 必须与每帧 R_k 和标度配对 |
| 逐源/帧级 SNR 中位数即参考空间控制点 | 不成立；需 PSF/噪声/背景条件下的 V_F，旧 (A−B)/MAD 只是 legacy proxy |
| 不同设备/滤镜不得组合 | 用户自由决定 Mosaic 组合；Normalize 内部区分母版/响应是模型分区，不是过滤 |
| 一套文档自称全部 science/algorithms/API 唯一权威 | 已取消；只保留此页作接口索引，数学与数据分属以上正本 |

## 4. 旧模块文档的正确用途

docs/detail/registry/acsd.phase1.* 和 docs/science/algorithms/normalize/* 记录历史函数、数据域与局部算法。若某项已经在源码实现，可以当定位线索；**不能**由“合同已冻结/验收门已通过”的标题推断其物理实现真实达到目标。修改相应 API 后由前台将旧接口说明更新为真实签名/数据类型；Agent 不另起独立科学版本。

工程与科学的冲突登记只放 NORMALIZE_CONVERGENCE，不在每个文件再复制一份覆盖性的 Normalize pipeline。项目无研发流程门禁，但文件不存在、WCS 无效、模型数值不可计算等数据错误照常准确处理。

完整数据与代码接口已在 [NORMALIZE_DATA.md](../../engineering/contracts/NORMALIZE_DATA.md)、[contracts.md](contracts.md) 定稿；本页只导航，不再由执行者补选科学定义。
