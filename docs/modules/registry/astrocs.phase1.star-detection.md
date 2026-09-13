---
id: MOD-astrocs-phase1-star
version: 1.0.0
status: ACTIVE
owner: astrocs-core
source_commit: 5ecc60df2d5021d18be04e0e6359d45b7b125b33
upstream: [SCI-P1-STAR-001, ALG-STARDET-001, API-P1-003]
downstream: [TEST-P1-STAR-001]
---

# 模块 astrocs.p1.star_detection

> P1-STAR-DOC 手写合同页（2026-09-07，SA-P1-S15）：registry 无
> astrocs.p1.star_detection descriptor——现行占位 descriptor 为
> astrocs.phase1.star-psf（lib/core/src/module_adapters.cpp:492-510，编排层
> 词汇，占位 ID ALG-002/TEST-P1-PSF-001 由 P1-PSF-INT/P1-STAR-INT 对齐），
> 本页手写登记，重生成时须保留（astrocs.phase1.session.md 先例）。权威签名头
> lib/star_detector/include/star_detector.h:1-73；模块级事实以
> lib/star_detector/README.md（r1，CONTRACT_READY）+ module.yaml
> （MOD-astrocs-phase1-star，dll_target=astrocs_p1_star_detection.dll，
> entrypoint=MISSING）为准。

- 模块词汇：`astrocs.p1.star_detection`（MODULE_MIGRATION_MATRIX P1-STAR 行
  module_id；owner=SA-P1-S15，legacy_paths="lib/star_detector;lib/phase1/stars"，
  迁移目标 astrocs_p1_star_detection.dll——合同值，尚未存在）。
- 层级：Phase1 生产模块；现状构建 lib/star_detector/Makefile:39 →
  star_detector.dll（orchestrator.cpp:1539-1549 显式加载，失败即错）；根
  CMakeLists.txt:441 astrocs_phase1_stars STATIC（lib/phase1/stars P1-003
  桥接层，独立 sigma-clip 实现）为 legacy_paths 第二路径。
- 合同：SCI-P1-STAR-001（docs/science/STAR_DETECTION.md，冻结层，共享 SCI
  引用不改动）/ ALG-STARDET-001（STAR_DETECTION_ALGORITHMS §11 逐符号锚）/
  DATA-P1-STAR（DATA_SEMANTICS §17）/ API-STAR-001（PUBLIC_API 星点检测节）/
  编排上游 API-P1-003（PHASE1_API_V1 §2 一帧一次权威检测）。
- 生产调用：run_stage_psf（PSF/STAR_MEASURE 阶段权威检测，
  orchestrator.cpp:2067；sdet_create 参数构造 :1593-1612；FP64/FP32 通道
  :2172-2198；star_det 权威块 FLOAT64[N,6] 写入 :2218-2246）；PLATESOLVE
  fallback 读块禁重检测（:1748-1755、:1826-1829）。
- 端口：入 image（DATA-P1-COSMETIC cleaned 帧，FP32 通道 uint16 量化
  DISP-STAR-001 / FP64 通道 double 不降级）+ SDetParams；出 star_det
  （DATA-P1-STAR，FLOAT64 [N,6] 列 x,y,flux,mag,saturated,has_saturated）。
- 并发：handle 级互斥（PHASE1_API_V1 §2 表行 no/no）；OpenMP 三处
  （sdet_api.cpp:448/:2321-2325/:2042-2044），dedup/sort/maxStars 串行，
  输出 bitwise 与线程数无关（determinism=fixed_reduction_order）；现状
  无 ThreadBudget 接线（整改归 P1-STAR-IMPL）。
- 错误：入口 0（含 0 星空场）/−1；拟合 SDET_FIT_*；质量门 SfError 五码；
  编排 STAR_DETECT_FAILED（:2200-2212）。
- 已知限制：DISP-STAR-001..005 + ThreadBudget/取消缺失（ALG-STARDET-001
  §11.3，登记不改码，整改归 P1-STAR-IMPL/INT）。
- 测试设计：TEST-STAR-DESIGN-001（ALG-STARDET-001 §11.4，冻结容差），
  由 P1-STAR-TEST 执行落 TEST-P1-STAR-001 + EVIDENCE。
