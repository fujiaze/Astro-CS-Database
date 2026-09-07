# lib/drizzle — astrocs.p1.drizzle（P1-DRZ）

> 状态: CONTRACT_READY（P1-DRZ-DOC 冻结，2026-09-07）｜doc revision: r1
> 本 README 由源码逐函数核对后新建（P1-DRZ-DOC）：函数、单位、坐标、dtype、
> shape、invalid、错误、并发、内存、I/O 均以现行唯一生产实现
> `lib/healpix_db/healpix_drizzle/`（根 CMakeLists.txt:356-366 静态库
> `astrocs_drizzle`，C ABI 导出权威 `hp_drizzle_api.h:42,62,70,130,139,140`）
> 为准；`lib/drizzle/` 是 P1-DRZ 迁移目标目录（astrocs_p1_drizzle.dll 落码
> 由 P1-DRZ-IMPL 建立，当前本目录仅合同文件、无源码；落位依据
> MODULE_MIGRATION_MATRIX.csv P1-DRZ 行 target=astrocs_p1_drizzle.dll、
> legacy_paths="lib/phase1/drizzle;root astrocs_drizzle target"——
> lib/phase1/drizzle 目录不存在，root target 源码在
> lib/healpix_db/healpix_drizzle/，与 lib/cosmetic/、lib/calibration/ 先例
> 同构，本目录不与 legacy 目录重叠）。权威合同：
> SCI-DRZ-001 → ALG-DRZ-001 → DATA-P1-DRZ / API-DRZ-001（链接见 §4）。

## 1. 身份

| 字段 | 当前值 |
|---|---|
| MOD ID / DLL target | `MOD-astrocs-phase1-drizzle` / 现状实现编入 CMake 静态库 `astrocs_drizzle`（CMakeLists.txt:356-366，无独立 DLL 产物）；迁移目标 `astrocs_p1_drizzle.dll`（P1-DRZ-IMPL 建立，尚未存在） |
| module / ABI / doc revision | `astrocs.p1.drizzle` / C ABI（HP_DRIZZLE_API extern "C"，无版本化 query 入口，迁移缺口）/ r1 |
| owner / phase scope | SA-P1-DRZ / phase1（wave W1） |
| 文档状态 | CONTRACT_READY（实现存在于 lib/healpix_db/healpix_drizzle，模块化迁移未开始；不声明 IMPLEMENTED） |
| 构建 | 现状随 CMakeLists.txt:356-366 `astrocs_drizzle`（STATIC，链接 astrocs_contracts/common/aio/hips；UNIX 下 -fopenmp，:379-382 注释"omp 仅遗留内部，生产调度走 Runtime lease"）；独立目标由 P1-DRZ-IMPL 建立 |

## 2. 负责范围

负责：tiled 球面 drizzle（输入帧逐像素 WCS/SIP→天球，drop 收缩 footprint
与 HEALPix NESTED leaf 求交，球面 Sutherland–Hodgman 裁剪 + Eriksson 扇形
面积加权累加 sumFlux/sumArea/sumVarNum/nContrib）、auto nside 决策
（compute_auto_nside）、FP32/FP64 双精度模板通道、逐 tile SNR 控制点
（KD-tree IDW 评估器）、HiPS 直写 sink（tile_depth=9，512×512 leaf tile）
与 legacy HISS 写、Sphere→Plane 反向 drizzle（REV-101..107 球面面积语义）、
C ABI 门面六导出。数据语义 DATA-P1-DRZ（DATA_SEMANTICS §11）。

不负责：S_p=F_p/D_p 面亮度归一与 variance/ivar finalize（astro_image_io
侧 astro_sphere_sink 传出原始累加量后由 aio_hips_writer finalize_tile
完成，drizzle_engine.cpp:2-3 锚注释）；HiPS 产品集/文件布局管理
（astro_image_io AIO API）；FITS/XISF 通用读写（astro_image_io；模块内
fits_reader 仅为本模块专用读通道）；WCS/SIP 参数生成（上游 calibrate/
io 层）；master 生成/校准/坏点（P1-CAL / P1-COS）；线程授予（现状 omp
遗留通道 + Runtime lease，ThreadLease 接线由 P1-DRZ-IMPL 整改）；编排
stage 序列与 DLL 加载（orchestrator）；整 Phase 行为（禁止）。

## 3. 输入与输出（DATA-P1-DRZ，DATA_SEMANTICS §11）

输入（hp_drizzle_run 帧通道）：PipelineFrame "data" 块 `[H][W]` 行主序
float32 或 float64（二选一，多通道 channels!=1 拒绝，api.cpp:486-503），
单位 ADU；"header" KV 块 WCS/SIP（CD 或 CDELT+CROTA2，缺 WCS 返回 -9，
api.cpp:541-545）、可选 "PRECISION"（"fp32"/"fp64"，缺省 FP32）、可选
"snr_model" 稀疏控制点块；nside（2 的幂）、nested（仅 1=NESTED，0=RING
硬拒绝）、pixfrac∈(0,1]（引擎层拒绝 0.0 与越界值，不夹逼；
drizzle_engine.cpp:1567-1574）。文件通道 hp_drizzle_fits_to_ahpx 另接
可选 SNR/权重 FITS。

输出（单位/dtype/shape/invalid 全表见 DATA_SEMANTICS §11.2）：HEALPix
NESTED tile 累加量（sumFlux/sumArea/sumVarNum 原始和 + nContrib 计数，
tile 内 leaf 连续数组）经 HiPS 直写（产品 SIGNAL/SUPPORT/variance/ivar）
或 legacy .hiss；HpDrizzleResult 统计（n_healpix_pixels/n_source_pixels/
nside/nested/pixfrac/elapsed_sec/error_msg[512]，hp_drizzle_api.h:22-30）；
operation_counts.json 剖面（api.cpp:1074-1117）。方差仅当 varianceValue>0
累加（drizzle_engine.cpp:1531-1534），无 variance 输入不产 variance 产品。

## 4. 合同链接

| 层 | ID | 权威文档 |
|---|---|---|
| SCI | SCI-DRZ-001 | docs/science/DRIZZLE.md（FROZEN T105 2026-08-23；SCI-DRZ-001/014/015/016 集合） |
| ALG | ALG-DRZ-001 | docs/algorithms/DRIZZLE_GEOMETRY.md（逐公式源码锚定 + DISP-DRZ 清单） |
| DATA | DATA-P1-DRZ | docs/contracts/DATA_SEMANTICS.md §11（上游 DATA-P1-CAL §9；编排现状引用 DATA-P1-STACK） |
| API | API-DRZ-001 / API-P1-007 | docs/contracts/PUBLIC_API.md / docs/api/PHASE1_API_V1.md（区间 API-P1-001..010） |
| MOD/SRC | MOD-astrocs-phase1-drizzle / SRC-DRZ-001 | docs/traceability/TRACEABILITY_MATRIX.json；实现源 lib/healpix_db/healpix_drizzle/（签名源 hp_drizzle_api.h:42-51,62-75,130-140） |
| TEST | TEST-DRZ-DESIGN-001 | docs/algorithms/DRIZZLE_GEOMETRY.md §9（可执行 TEST-P1-DRZ-001 由 P1-DRZ-TEST 落地） |

## 5. 实现事实（源码核对）

- **核心公式（与 SCI-DRZ-001 §5 一致）**：`w = overlap_area / drop_area`
  （drop_area 为球面 S-H 收缩 drop 总面积，<1e-20 拒绝，
  drizzle_engine.cpp:1436-1438,1505）；`sumFlux += x·w`（:1527）；
  `sumArea += overlap_area`（:1528）；`sumVarNum += v·w²`（double 中转，
  :1531-1534）。**S_p=F_p/D_p 归一不在本模块**——原始累加量传出，归一在
  astro_sphere_sink.cpp:100 + aio_hips_writer finalize_tile（分层设计）。
- **pixfrac**：`half=0.5·pixfrac` 半宽四角收缩（:1303）；(0,1] 严格校验。
  API 文件通道接受 pixfrac=0.0（api.cpp:191 `<0.0` 才拒）而引擎层拒绝
  ——两层边界口径差（DISP-DRZ-003）。
- **auto nside**：最小 2 次幂 ≥ 211034.6/finest_arcsec，钳位
  [16, 2^22]（drizzle_engine.cpp:624-710）。
- **NaN/Inf**：值像素非有限 → 主循环静默 continue（:1712），不进累加器、
  无计数暴露（与 DRIZZLE.md:96 "不掩膜传播 NaN" 不符——DISP-DRZ-004）；
  几何 NaN（ra/dec）显式拒绝（:1319-1320），半球检查失败返回 NAN
  （spherical_overlap.cpp:196-216）。SNR/权重/variance 面非有限或 ≤0
  同样跳过（:1715-1729）。
- **几何**：三层候选缓冲（quick-reject `max_angle+1.25·hp_res`
  〔HP_CIRCUMRADIUS_FACTOR=1.25〕→ queryDisc 回退 `+3.0·hp_res` →
  fast 面 delta×1.15 畸变系数 + 极冠/跨 face 回退，spherical_overlap.cpp:42
  等）；面积 = 球面 S-H 裁剪 + Eriksson 扇形三角剖分（:186-239）。
- **FP32/FP64**：模板双实例（float/double 显式实例化
  drizzle_engine.cpp:2171-2178）；precision_mode 0/1/-1（-1=读 header
  "PRECISION" KV）；FP32 累加器真 binary32（逐项舍入依赖测试门 1e-5）。
- **生产调用现状**：orchestrator DLL 通道（orchestrator.cpp:3256-3371）
  经函数指针调 `hp_drizzle_run_hips` 直写 HiPS；registry descriptor
  （module_adapters.cpp:508-525）未接节点。p1_session 无 drizzle stage
  （阶段序 io_read→calibrate→cosmetic→io_write，p1_session.cpp:2）。
- **反向 drizzle**：HpReverseDrizzleInput 严格校验（nside 2 的幂
  [1,2^22]、仅 NESTED、pixfrac∈(0,1]、f32/f64 二选一、ipix 越界/重复
  拒绝，reverse_drizzle.cpp:255-334）；capability 位 0x01..0x20、版本
  "1.0.0"（api.cpp:145-151）。

### 5.1 配置 schema（现状：C 参数直传 + orchestrator Stage1Config）

- C 参数：nside（2 的幂）、nested（仅 1）、pixfrac∈(0,1]、
  precision_mode（0/1/-1）、output 路径。
- orchestrator 键（Stage1Config.drizzle）：pixfrac（缺省 0.8，CFG-001）、
  ordering（"nested" 默认）、nside_mode/nside_value（"1x_to_2x_drizzle"
  策略或 explicit）；precision（FP32/FP64 经 header KV "PRECISION"
  传递，orchestrator.cpp:3313-3325）。
- 正式版本化 schema 由 P1-DRZ-IMPL 冻结。

## 6. 并发与资源

资源分类：cpu_heavy（O(源像素×候选) 球面几何，重核）。现状并行=OpenMP
`parallel for schedule(static) num_threads(num_threads) reduction`
（drizzle_engine.cpp:1670-1671）+ per-thread tile 累加器/计数器
（:1645-1646），线程数=config.threads（0=omp_get_max_threads，
:1639-1644，不改全局 ICV）；合并=串行按线程序 t=1..N-1 合入
threadTiles[0]，仅合并 touched leaf，字段序 sumFlux→sumArea→sumVarNum→
nContrib（:1762-1785）——**1/N 确定性成立**（同输入同线程数 bitwise
可复现；跨线程数浮点和序不同不保证 bitwise，见 ALG-DRZ-001 §6）。
target 几何缓存 per-run generation 原子递增（:1659-1660，B4-22 修复
data race）+ per-thread LRU 8192。**ThreadLease 全模块零命中**——
CMakeLists.txt:379-382 注明 omp 为遗留内部通道、生产调度走 Runtime
lease；ThreadLease 迁移整改点（P1-DRZ-IMPL）。

## 7. provider 能力与 fallback

现状无 provider 概念：纯 CPU 标量 + OpenMP 遗留通道，无 ACR/CUDA/GPU、
无 SIMD kernel 注册（ISA 迁移由 P1-DRZ-IMPL 按约束逐内核 benchmark
决定，本合同不预设）。FP32/FP64 双精度模板即现状"精度通道"选择。

## 8. 验证（TEST-DRZ-DESIGN-001，ALG-DRZ-001 §9）

- 合成 fixture：常量场/点源高斯/梯度场/脉冲像素、pixfrac 扫描
  {0.1..1.0}、尺度代表点 {0.2..10}"/px、边缘 patch；全离线零真实数据。
- 独立 oracle：独立点包含判定（1e-12 容差）、9003 例候选零漏选门
  （candidate_oracle_test）；通量闭合门 FP64<1e-6（主域）/逐 leaf
  <1e-5（FP32/FP64）、方差 α² 缩放律 worst_rel<1e-4
  （variance_propagation_test）、reverse false_hole/false_fill=0。
- 不变量：常量场均匀性、总通量守恒、NESTED 地址往返、1/2/4 线程
  结果一致性、HEALPix 地址 parent/local 位分解。
- 负面：DATA_SEMANTICS §11.1/§11.2 invalid 列 + 参数矩阵逐行断言
  （pixfrac 0/负/>1、RING、多通道、缺 WCS、NaN 面）。

## 9. 构建与已知限制

现状构建：根 CMake 目标 `astrocs_drizzle`（STATIC，10 源文件，
CMakeLists.txt:356-366）+ 遗留 Makefile 通道（healpix_drizzle.dll，
Python ctypes 用）；迁移目标 astrocs_p1_drizzle.dll + C ABI adapter +
plan/execute/cancel/inspect + ThreadLease 接线由 P1-DRZ-IMPL 建立
（module.yaml 已登记 manifest；禁止跨 DLL 传 STL/异常/RTTI）。
legacy 源目录 lib/healpix_db/healpix_drizzle/ 整体去留由 P1-DRZ-IMPL
决定（poly_clip.cpp 编入 target 但生产路径零调用，DISP-DRZ-008）。

已知限制（完整清单 = ALG-DRZ-001 §10 DISP-DRZ-001..008 + 源码内在
缺陷候选，P1-DRZ-IMPL/INT 处理）：值像素 NaN 静默跳过无计数；
API/引擎 pixfrac=0.0 边界双轨；hp_drizzle_run 错误码正负两套混用
（负值 -1..-13 / 正值 0/1，api.cpp:541,1044,1066）；无取消机制
（长 run 不可中断）；schedule(static) 行条带负载不均；shim 对非法
nside 容忍不抛（校验依赖调用方）；sip_order 注释 0..4 vs 校验 [0,5]
（DISP-DRZ-001）；方差锚点行号漂移（DISP-DRZ-007，归一实际在
astro_sphere_sink.cpp:100 + aio_hips_writer finalize_tile）。

## 10. 迁移（P1-DRZ-IMPL 目标，不声明完成）

本目录（lib/drizzle/）为迁移落点：astrocs_p1_drizzle.dll、
module.yaml（同目录，已冻结 manifest：entrypoint=MISSING——registry
入口未接）、C ABI adapter、plan/execute/cancel/inspect、ThreadLease
接线（替换 omp 遗留通道，保持 1/N 合并序）、DISP-DRZ 清单消化见
ALG-DRZ-001 §0/§10 与 module.yaml 注释。迁移不得改变 ALG-DRZ-001
公式语义与 DATA-P1-DRZ 数据语义（SCI-DRZ-001 未变更前）。
