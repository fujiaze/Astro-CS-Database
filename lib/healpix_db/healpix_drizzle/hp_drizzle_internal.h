#ifndef HP_DRIZZLE_INTERNAL_H
#define HP_DRIZZLE_INTERNAL_H

/* hp_drizzle_internal.h - drizzle 生产源各 TU 之间的私有共享声明。
 *
 * 不进公共导出面 (无 C ABI 符号)。背景 (F-13 / P1-003 生产路由清理):
 * 原 hp_drizzle_run_hips 与 hp_drizzle_run 同处 hp_drizzle_api.cpp 单一 TU;
 * CLI 生产闭包只引用 hp_drizzle_run, 但静态库按 .o 取成员, 会把同一 .o 内
 * 未被引用的 run_hips 一并拉入 astrocs exe。将 hp_drizzle_run_hips 原样迁至
 * hp_drizzle_hips_api.cpp, 两 TU 经本头共享 run_drizzle_internal 与错误写入
 * 辅助; 签名/语义/数值零改动。
 *
 * 符号可见性: run_drizzle_internal 在 astrocs_p1_drizzle.so 内经链接
 * version-script (local: *) 降 local, 不进模块导出面。 */

#include "hp_drizzle_api.h"

#include <string>

/* 将 std::string 错误信息拷贝到 result->error_msg (截断到 511 字节)。
 * 原为 hp_drizzle_api.cpp 内 static 辅助, 迁入本头为 static inline
 * (逐 TU 内部链接, 行为逐字节不变)。 */
static inline void setErrorMsg(HpDrizzleResult* result, const std::string& msg) {
    if (!result) return;
    size_t n = msg.copy(result->error_msg, sizeof(result->error_msg) - 1);
    result->error_msg[n] = '\0';
}

/* 共享 Drizzle 执行体 (定义在 hp_drizzle_api.cpp; 供 hp_drizzle_run 与
 * hp_drizzle_run_hips 两个 C 导出薄壳调用)。 */
/* hips_profile: HiPS 直写档位 (仅 write_hips=true 时生效):
 *   0 = 通用直写 (write_hips_direct; signal+support+snr/variance, provenance)
 *   1 = Phase1 生产末端 (write_hips_phase1; 与旧 writer 节点产物逐字节等价)
 * hips_filter_passband: profile=1 的 obs_filter 透传 (可 NULL/空串). */
int run_drizzle_internal(PipelineFrame* frame,
                         int nside, int nested, double pixfrac,
                         const char* output_path,
                         const char* hips_dir,
                         bool write_hips,
                         bool write_legacy_hiss,
                         HpDrizzleResult* result,
                         int precision_mode,
                         int hips_profile,
                         const char* hips_filter_passband);

#endif /* HP_DRIZZLE_INTERNAL_H */
