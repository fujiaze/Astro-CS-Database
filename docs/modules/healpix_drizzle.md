# Module: healpix_drizzle

> P1-DRZ-DOC（2026-09-07）事实修订：合同 ID 收敛为 ALG-DRZ-001（旧
> ALG-DRZ-CAND-001/ALG-DRZ-OVERLAP-001/ALG-DRZ-VAR-* 为历史登记名，
> 不再作合同引用）；模块合同落位 lib/drizzle/（迁移目标目录，
> astrocs.p1.drizzle）；实现源不变。

## 职责

球面 Drizzle：HEALPix NESTED 网格重投影（tiled）+ 通量守恒累加
（sumFlux/sumArea/sumVarNum 原始和）+ 方差传播（v·w²，α² 缩放律）
+ auto nside + FP32/FP64 双精度通道 + Sphere→Plane 反向 drizzle +
操作计数诊断。归一（S=F/D、variance=sumVarNum/D²、ivar）在
astro_image_io finalize 层（astro_sphere_sink.cpp:100 传出，
aio_hips_writer finalize_tile 完成——DISP-DRZ-007）。

## 非职责

不做多帧统计合并（Phase2）；不做 master/校准/坏点（P1-CAL/
P1-COS）；不做线程授予（omp 为遗留内部通道，生产调度走 Runtime
lease，CMakeLists.txt:379-382；ThreadLease 由 P1-DRZ-IMPL 接线）。

## Public API

hp_drizzle_api 六导出（extern "C"，hp_drizzle_api.h:42,62,70,130,
139,140；合同 API-DRZ-001，docs/contracts/PUBLIC_API.md）；球面
几何接口（spherical_overlap.h: compute_overlap_area_g_ctx_cached、
radec_to_vec、HP_CIRCUMRADIUS_FACTOR=1.25）。

## Data contract

DATA-P1-DRZ（DATA_SEMANTICS §11）：输入帧（"data" f32/f64 [H][W]
ADU + header WCS/SIP + 可选 PRECISION/snr_model）→ 目标 NESTED
tile 产品（SIGNAL/SUPPORT/variance/ivar，tile_depth=9、nside≥512
硬门）+ HpDrizzleResult 统计 + operation_counts.json。

## Ownership

调用方分配 frame/result/输出缓冲；模块内 RAII（SNR 控制点 vector，
api.cpp:609-613）；HiPS 目录树由模块写入、编排层负责 overwrite
清理。

## Thread safety

单 run 内 OpenMP 行条带并行（schedule(static) + per-thread tile
累加器，drizzle_engine.cpp:1670-1671）+ 按线程序合并（:1762-1785）
→ 1/N 确定性（同输入同线程数 bitwise；跨线程数不保证 bitwise）。
geometry cache：per-thread LRU 8192 + per-run generation 原子清空
（:1659-1660，B4-22）；同进程多 run 并发安全。ThreadLease 零命中。

## Errors

几何退化/无 WCS/非法参数 → 拒绝（文件通道正值 1..11；帧通道正负
混用 -1..-8/-9/-12/-13，无集中枚举——登记缺陷）；值像素 NaN/Inf
静默跳过（DISP-DRZ-004）；无 NO_DATA 语义输出（面亮度归一在
finalize 层，covered_area≤0 → variance NaN）。

## Science IDs

SCI-DRZ-001/014/015/016（docs/science/DRIZZLE.md，FROZEN）；
ALG-DRZ-001（docs/algorithms/DRIZZLE_GEOMETRY.md，含 TEST-DRZ-
DESIGN-001 与 DISP-DRZ-001..008）；DATA-P1-DRZ；API-DRZ-001；
MOD-astrocs-phase1-drizzle（lib/drizzle/module.yaml）。

## 性能特征

TargetGeomCache 复用（hit 率 91.7% 小图实测）；三层候选缓冲
（1.25/3.0·hp_res + fast 1.15 畸变系数）；候选零漏选（9003 例
false_negative=0）；计数 METRIC-P1-DRZ-CANDIDATES 等
（DrizzleOpCounters 全字段见 operation_counts.json 剖面）。

## V19R3 bounded target-ipix geometry cache

- TargetGeomCache（LRU，默认 8192，线程私有，run generation 切换
  清空，B4-22 原子化）；
- 计数新增 target_boundary_builds / target_geometry_builds /
  geometry_cache_hits / geometry_cache_misses（DrizzleStats +
  [ops] 行）；
- 科学等价：candidate oracle 9003/0、freeze 42/42、UPMW-005 MC
  k_corr=1.3883 不变；详见 docs/algorithms/DRIZZLE_GEOMETRY.md。

## Tests

lib 内科学门（lib/healpix_db/healpix_drizzle/tests/*.cpp）：
candidate/overlap/variance oracle（evidence/drizzle/*.json）、
freeze/l0 闭合门、α² 缩放律、reverse false_hole/false_fill；
合同级 TEST-DRZ-DESIGN-001（DRIZZLE_GEOMETRY.md §9，可执行
TEST-P1-DRZ-001 由 P1-DRZ-TEST 建立）；Monte Carlo 方差
（SNR-011/012）。

## Source files

lib/healpix_db/healpix_drizzle/（10 源文件编入根 CMake 静态库
astrocs_drizzle，CMakeLists.txt:356-366；poly_clip.cpp 编入但
生产路径零调用——DISP-DRZ-008）；模块合同 lib/drizzle/。
