# SCI-P1-STAR-001 — Phase1 星点检测（P1-STAR 冻结层）

> 状态: FROZEN（P1-STAR-DOC 冻结，2026-09-07，SA-P1-S15）
> 性质: P1-STAR 任务的 SCI 冻结层落点。共享 SCI（PSF / PHOTOMETRY /
> ASTROMETRY，docs/science/ 既有文档）不因本任务改动；本页仅声明
> matrix P1-STAR 行的 sci_doc 指向与语义映射，格式沿用 STAR_PSF_ALGORITHMS
> §11.5 先例。
> ALG 权威: ALG-STARDET-001（docs/algorithms/STAR_DETECTION_ALGORITHMS.md §11）。

## 1 语义要求（semantic anchors，公式锚见 ALG-STARDET-001 §2/§11.1）

- subpixel centroid: 亚像素质心为连续估计（一阶导零交叉 / 二阶导零交叉 /
  Moffat4 GSL-LM 中心），不引入 0.5px 网格量化损失；合成场验收容差
  |Δc|≤0.3 px（SNR≥20）见 ALG-STARDET-001 §11.4 F1。
- completeness / false positive (synthetic fields): 完备性与虚警由合成星场
  验收（召回 ≥99% @SNR≥10；虚警 ≤0.1/千像素，纯噪声场）——检测是经验性
  图像处理流程，不宣称解析保证；全局检测阈值为
  `threshold = median(img) + 5.0·bgnoise`（5σ 语义）。
- saturation / blend / edge: 饱和判定=3×3 邻域双条件
  （meanhigh−bg ≥ 0.7·dynrange 且 pixel0−minhigh ≤ 0.1·dynrange）；饱和平台
  中心=edge-walking 几何中心；饱和与正常星重叠（d²<4.0）丢正常星保饱和星；
  距边界 <2px 允许丢弃。
- deterministic ordering: 输出按 mag 升序全序确定（NaN 恒排末尾），
  dedup/sort/maxStars 截断串行，输出与线程数 bitwise 无关
  （ALG-STARDET-001 §5）。

## 2 与共享 SCI 的关系

- SCI-PSF-001（PSF）：检测输出（cx/cy/FWHM/振幅/背景）为 PSF 拟合与
  plate solve 的输入；Moffat4/Gaussian 拟合模型语义归 SCI-PSF-001 域，
  本模块只登记检测侧调用事实（ALG-STARDET-001 §2）。
- SCI-PHOTOMETRY-001（PHOTOMETRY）：正常星 mag=−2.5·log10(Σ_box(pixel−B_fit))
  为粗测光（检测侧自估），最终测光归 PHOTOMETRY 域；饱和星 mag 量纲差异
  已登记 DISP-STAR-004（ALG-STARDET-001 §11.3）。
- SCI-ASTROMETRY-001（ASTROMETRY）：像素中心=索引+0.5 约定
  （ALG-STARDET-001 §2 残差坐标），star_det 权威块消费方（plate solve
  fallback）按此约定解析坐标（DATA_SEMANTICS §17）。

## 3 基线选择与验收语义

- 基线算法=peaker 七步候选 + Moffat4 GSL trust-region LM 拟合
  （sdet_detect_impl 生产路径，sdet_api.cpp:1599-2353）；旧 CC 结构图路径
  （sdet_detect/:992-1274）为 DISP-STAR-005 登记的遗留双实现，不作为
  基线（去留归 P1-STAR-IMPL 整改）。
- 现状缺陷不隐瞒（DISP-STAR-001..005 显式登记，ALG-STARDET-001 §11.3）；
  本 SCI 冻结不声称缺陷已修复，整改项由 P1-STAR-IMPL/P1-STAR-INT 承接。
- 本页与 ALG-STARDET-001/DATA-P1-STAR/API-STAR-001 组成 P1-STAR 冻结层；
  禁止编排层词汇（descriptor astrocs.phase1.star-psf）反向改写本层。

## 4 测试设计锚

TEST-STAR-DESIGN-001（ALG-STARDET-001 §11.4）为冻结测试设计：
F1 合成场统计（质心/FWHM/召回/虚警）、F2 饱和/混合/边缘专项、F3 确定性
（线程数 bitwise 一致+全序断言）、F4 FP64 独立 oracle 与 FP32 量化容差、
F5 状态码负例、F6 回归锚。可执行 TEST-P1-STAR-001 由 P1-STAR-TEST 按本设计
落地，容差冻结不得放宽。
