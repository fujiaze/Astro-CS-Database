#ifndef HP_LOD_API_H
#define HP_LOD_API_H

// ============================================================================
// LOD 金字塔模块 C API 导出层
// 供 Python (ctypes) 或其他 C 程序调用
//
// 所有返回 char* 的函数: 调用方负责用 hp_lod_free_string() 释放
// ============================================================================

#include <stddef.h>
#include <stdint.h>

#ifdef _WIN32
#define LOD_EXPORT __declspec(dllexport)
#else
#define LOD_EXPORT __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// LOD 金字塔管理
// ============================================================================

// 生成完整 LOD 金字塔
// dbPath: 堆栈数据库路径
// bandIndex: 波段索引 (0..N-1)
// 返回: 0=成功, -1=失败
LOD_EXPORT int hp_lod_generate_full(const char* dbPath, int bandIndex);

// 增量更新
// dbPath: 数据库路径
// bandIndex: 波段
// changedTilesJson: JSON 数组 [tileIpix, ...]
// 返回: 0=成功, -1=失败
LOD_EXPORT int hp_lod_update_incremental(const char* dbPath, int bandIndex,
                                          const char* changedTilesJson);

// 按需计算
// dbPath: 数据库路径
// bandIndex: 波段
// level: LOD 层级 (0..N-1)
// tileIpix: 请求的 tile
// 返回: JSON 字符串 (malloc 分配, 需用 hp_lod_free_string 释放)
//   JSON 格式: {"nside":..,"tileIpix":..,"pixels":[..],"values":[..],"weights":[..],"counts":[..]}
// 失败返回 NULL
LOD_EXPORT char* hp_lod_compute_on_demand(const char* dbPath, int bandIndex,
                                            int level, int64_t tileIpix);

// 释放 API 返回的字符串
LOD_EXPORT void hp_lod_free_string(char* str);

// 获取 LOD 层级数
// dbPath: 数据库路径
// 返回: 层级数, 失败返回 -1
LOD_EXPORT int hp_lod_get_level_count(const char* dbPath);

#ifdef __cplusplus
}
#endif

#endif // HP_LOD_API_H
