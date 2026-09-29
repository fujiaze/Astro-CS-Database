# 模块 astrocs.p1.star_detection

> 上游：docs/ASTROCS_DESIGN.md §8.5（模块与 ABI）

> 星点检测模块登记页；权威签名头
> lib/algorithms/star_detection/include/star_detector.h，生产实现
> lib/algorithms/star_detection/src/sdet_api.cpp（检测内核 sdet_detect_impl）。
> 模块级事实以 lib/algorithms/star_detection/README.md（CONTRACT_READY）+
> module.yaml（MOD-astrocs-phase1-star，dll_target=astrocs_p1_star_detection.dll）为准。

- 模块词汇：`astrocs.phase1.star-detection`（registry descriptor 单源）；模块合同
  owner = SA-P1-S15；实现面 = lib/algorithms/star_detection（含 wrapper_phase1 桥接面）。
- 层级：Phase1 生产模块；现状构建 lib/algorithms/star_detection/Makefile:39 →
  star_detector.dll（orchestrator.cpp:1539-1549 显式加载，失败即错）；根
  CMakeLists.txt:441 astrocs_phase1_stars STATIC（lib/algorithms/star_detection/wrapper_phase1 P1-003
  桥接层，独立 sigma-clip 实现）为第二实现路径。
- 合同：SCI-P1-STAR-001（docs/science/STAR_DETECTION.md，冻结层，共享 SCI
  引用不改动）/ ALG-STARDET-001（STAR_DETECTION_ALGORITHMS §11 逐符号锚）/
  DATA-P1-STAR（DATA_SEMANTICS §17）/ API-STAR-001（PUBLIC_API 星点检测节）/
  编排上游 API-P1-003（PHASE1_API_V1 §2 一帧一次权威检测）。
- 生产调用：run_stage_psf（PSF/STAR_MEASURE 阶段的**全图盲检测**：
  权威检测范式 = **星表引导拟合**（星表位置为定义域），本实现是全图盲检测路径的生产源；
  全图盲检测连通域**不是**权威路径，见 `ASTROCS_DESIGN.md` §4.2；
  orchestrator.cpp:2067；sdet_create 参数构造 :1593-1612；FP64/FP32 通道
  :2172-2198；star_det 权威块 FLOAT64[N,6] 写入 :2218-2246）；PLATESOLVE
  fallback 读块禁重检测（:1748-1755、:1826-1829）。
- 端口：入 image（DATA-P1-COSMETIC cleaned 帧，FP32 通道 uint16 量化，
  登记见 STAR_DETECTION_ALGORITHMS.md §11.3 / FP64 通道 double 不降级）
  + SDetParams；出 star_det
  （DATA-P1-STAR，FLOAT64 [N,6] 列 x,y,flux,mag,saturated,has_saturated）。
- 并发：handle 级互斥（PHASE1_API_V1 §2 表行 no/no）；OpenMP 三处
  （sdet_api.cpp:448/:2321-2325/:2042-2044），dedup/sort/maxStars 串行，
  输出 bitwise 与线程数无关（determinism=fixed_reduction_order）；现状
  无 ThreadBudget 接线，ThreadBudget/取消接线属迁移目标（未落地）。
- 错误：入口 0（含 0 星空场）/−1；拟合 SDET_FIT_*；质量门 SfError 六码；
  编排 STAR_DETECT_FAILED（:2200-2212）。
- 已知限制：ThreadBudget 接线与取消检查点缺失（ALG-STARDET-001 §11.3 缺陷登记，
  现行实现保持此语义；整改面未落地）。
- 测试设计：TEST-STAR-DESIGN-001（ALG-STARDET-001 §11.4，冻结容差），
  可执行测试待建（TEST-P1-STAR-001）。
