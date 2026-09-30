# Module: healpix_drizzle

> 上游：docs/ASTROCS_DESIGN.md §8.5（模块与 ABI）

> 合同 ID = ALG-DRZ-001（唯一合同引用，registry 登记名同此）；
> 模块合同落位 lib/algorithms/drizzle/（迁移目标目录，astrocs.p1.drizzle）；
> 本页登记职责、端口、线程/确定性、错误与测试面，
> 全部条目以现行源码与冻结合同为准。

## 职责

球面 Drizzle：HEALPix NESTED 网格重投影（tiled）+ 通量守恒累加
（sumFlux/sumArea/sumVarNum 原始和）+ 方差传播（v·w²，α² 缩放律）
+ auto nside + FP32/FP64 双精度通道 + Sphere→Plane 反向 drizzle +
操作计数诊断。归一（S=F/D、variance=sumVarNum/D²、ivar）在
astro_image_io finalize 层（astro_sphere_sink.cpp 传出，
aio_hips_writer finalize_tile 完成，登记见 DRIZZLE_GEOMETRY.md）。

## 非职责

不做多帧统计合并（Phase2）；不做 master/校准/坏点（P1-CAL/
P1-COS）；不做线程授予（omp 为内部通道，生产调度走 Runtime
lease，CMakeLists.txt；ThreadLease 接线为迁移目标（未落地））。

## Public API

hp_drizzle_api 六导出（extern "C"，lib/algorithms/drizzle/healpix_drizzle/hp_drizzle_api.h；
合同 API-DRZ-001，docs/engineering/PUBLIC_API.md）；球面
几何接口（spherical_overlap.h: compute_overlap_area_g_ctx_cached、
radec_to_vec、HP_CIRCUMRADIUS_FACTOR=1.25）。

## Data contract

DATA-P1-DRZ（DATA_SEMANTICS §11）：输入帧（"data" f32/f64 [H][W]
ADU + header WCS/SIP + 可选 PRECISION/snr_model）→ 目标 NESTED
tile 产品（SIGNAL/SUPPORT/variance/ivar，tile_depth=9、nside≥512
硬门）+ HpDrizzleResult 统计 + operation_counts.json。

## Ownership

调用方分配 frame/result/输出缓冲；模块内 RAII（SNR 控制点 vector，
hp_drizzle_api.cpp（依据 `ENGINEERING_SPEC.md`；
以符号名为准）；HiPS 目录树由模块写入、编排层负责 overwrite
清理。

## Thread safety

单 run 内 OpenMP 行条带并行（schedule(static) + per-thread tile
累加器，drizzle_engine.cpp）+ 按线程序合并（同文件内）
→ 1/N 确定性（同输入同线程数 bitwise；跨线程数不保证 bitwise）。
geometry cache：per-thread LRU 8192 + per-run generation 原子清空
（drizzle_engine.cpp）；同进程多 run 并发安全。ThreadLease 零命中。

## Errors

几何退化/无 WCS/非法参数 → 拒绝（文件通道正值 1..12（+12=C 边界内部异常）；帧通道正负
混用 -1..-8/-9/-12/-13，无集中枚举——登记缺陷）；**值像素 NaN/Inf 经 `F_p=Σx_j·w_jp` 直接传播、drizzle 层不掩膜**
（实现 `lib/algorithms/drizzle/healpix_drizzle/drizzle_engine.cpp` 对值像素直接传播；
科学锚 `docs/science/DRIZZLE.md`，回归
finalize 层，covered_area≤0 → variance NaN）。

## Science IDs

SCI-DRZ-001/014/015/016（docs/science/DRIZZLE.md，FROZEN）；
ALG-DRZ-001（docs/science/algorithms/DRIZZLE_GEOMETRY.md，含 TEST-DRZ-
DESIGN-001 与缺陷登记）；DATA-P1-DRZ；API-DRZ-001；
MOD-astrocs-phase1-drizzle（lib/algorithms/drizzle/module.yaml）。

## 性能特征

TargetGeomCache 复用（命中率为运行期统计量，读数与条件见实验单元 `实验/healpix-polar/`）；三层候选缓冲
（1.25/3.0·hp_res + fast 1.15 畸变系数）；候选零漏选（设计 oracle
语料，判据见 DRIZZLE_GEOMETRY.md；读数与条件见 `实验/healpix-polar/`）；
计数 `METRIC-P1-DRZ-CANDIDATES` 等
（`DrizzleOpCounters` 全字段见 `operation_counts.json` 剖面）。

## 有界 target-ipix 几何缓存

- TargetGeomCache（LRU，默认 8192，线程私有，run generation 切换
  清空，原子化替换）；
- 计数新增 target_boundary_builds / target_geometry_builds /
  geometry_cache_hits / geometry_cache_misses（DrizzleStats +
  [ops] 行）；
- 科学等价：candidate oracle、freeze 闭合门与 MC 复算结果不变（判据见
  docs/science/algorithms/DRIZZLE_GEOMETRY.md；读数与条件见 `实验/healpix-polar/`）；
  k_corr 规范取值 1.4。

## Tests

lib 内科学门（lib/algorithms/drizzle/healpix_drizzle/tests/*.cpp）：
candidate/overlap/variance oracle（测试源 `lib/algorithms/drizzle/healpix_drizzle/tests/` 的
oracle/matrix 测试，证据产物为测试期 JSONL）、
freeze/l0 闭合门、α² 缩放律、reverse false_hole/false_fill；
合同级 TEST-DRZ-DESIGN-001（DRIZZLE_GEOMETRY.md §9，可执行
TEST-P1-DRZ-001 可执行面待建）；Monte Carlo 方差
（SNR-011/012）。

## Source files

lib/algorithms/drizzle/healpix_drizzle/（10 源文件编入根 CMake 静态库
astrocs_drizzle，CMakeLists.txt；poly_clip.cpp 编入但
生产路径零调用，登记见 DRIZZLE_GEOMETRY.md）；模块合同
lib/algorithms/drizzle/。
