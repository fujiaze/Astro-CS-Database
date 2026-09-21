// ── RETIRED-CODE-RETAINED (ENGINEERING_SPEC §2 保留则注释) ─────────────
// WHAT:       healpix_core 兼容 shim（B4-01 去重后仅转发至 astrocs::healpix 权威实现；
//             .cpp 只有 3 行、无独立实现）。
// WHY-KEPT:   历史消费方仍以 healpix_drizzle 的头名 include 本文件；删除需同批改
//             lib/algorithms/drizzle/healpix_drizzle 下的消费点与 CMake 源列表，
//             并确认无 legacy 测试面引用（本轮未做引用清点）。
// STATUS:     非权威实现（唯一实现 = lib/algorithms/shared/healpix/healpix_core.h/.cpp）；
//             本文件禁止新增/修改任何数值算法。
// EXIT:       引用清点为零（drizzle 模块内 grep 无 #include "healpix_core.h" 消费点、
//             CMake 源列表移除）后删除本 shim；权威实现不受影响。
// AUTHORITY:  ENGINEERING_SPEC.md §2（历史实现处置：保留则注释）/§9；
//             本文件原注记「DEPRECATED: B4-01 去重」。
// ──────────────────────────────────────────────────────────────────────
// DEPRECATED: B4-01 去重 shim — 唯一实现见 lib/algorithms/shared/healpix/healpix_core.h/.cpp
// 本文件仅为兼容占位，无独立实现；所有逻辑由 healpix_core.h 的 shim 类转发至 astrocs::healpix。
#include "healpix_core.h"
