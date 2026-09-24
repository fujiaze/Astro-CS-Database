// hp_drizzle_hips_api.cpp - Phase1 Final Closure 正式末端 C 导出面。
//
// F-13 (P1-003 生产路由清理): 本文件自 hp_drizzle_api.cpp 原样迁出
// hp_drizzle_run_hips。迁出动机 —— 原二者同处一个 TU, CLI 生产闭包只引用
// hp_drizzle_run, 而静态库链接按 .o 取成员, 导致未被引用的 hp_drizzle_run_hips
// 被一并拉入 acsd exe (冻结测试 eng/tests/cli/test_p1003_drizzle_path.py 判红)。
// 拆分后 CLI 只拉 hp_drizzle_api.o, 本 TU 仅由真正调用 run_hips 的目标
// (模块 astrocs_p1_drizzle / 直接对拍测试) 按需拉取。
//
// 科学域零改动: 函数签名、参数顺序、write_hips=true 语义、-11 异常屏障、
// 错误信息文本与迁移前逐字节一致。

#define HP_DRIZZLE_EXPORTS
#include "hp_drizzle_internal.h"

#include <cstdio>
#include <cstring>
#include <exception>
#include <string>

// ============================================================================
// hp_drizzle_run_hips - Phase1 Final Closure 正式末端:
// Drizzle TileAccumulator -> AIO HiPS 直写 (无 HISS 中转)
// hips_dir: HiPS 产品集根目录 (signal/support/snr 子产品)
// legacy_hiss_path: 可选 legacy .hiss 路径 (nullptr=不写, 仅 validation 用)
// ============================================================================
// ============================================================================
// hp_drizzle_run_phase1_hips - Phase1 生产末端正式 C ABI (无容器中转):
// Drizzle TileAccumulator -> 标准 HiPS (signal/support/Moc/metadata/properties)。
// 产物与旧 writer 节点 (读中间容器后写 HiPS) 逐字节等价。
// hips_dir: HiPS 产品集根目录; filter_passband: obs_filter (可 NULL/空串)。
// 返回: 0=成功, 非 0=失败 (result->error_msg 给出原因)。
// ============================================================================
HP_DRIZZLE_API int hp_drizzle_run_phase1_hips(PipelineFrame* frame,
                                              int nside, int nested, double pixfrac,
                                              const char* hips_dir,
                                              const char* filter_passband,
                                              HpDrizzleResult* result,
                                              int precision_mode)
{
    // C 边界异常屏障, 同 hp_drizzle_run / hp_drizzle_run_hips 薄壳 (对齐 -11)。
    try {
    return run_drizzle_internal(frame, nside, nested, pixfrac,
                                nullptr, hips_dir,
                                /*write_hips=*/true,
                                /*write_legacy_hiss=*/false,
                                result, precision_mode,
                                /*hips_profile=*/1,
                                /*hips_filter_passband=*/filter_passband);
    } catch (const std::exception& e) {
        fprintf(stderr, "[hp_drizzle_api] hp_drizzle_run_phase1_hips: C 边界捕获异常: %s\n", e.what());
        if (result) {
            std::memset(result, 0, sizeof(HpDrizzleResult));
            setErrorMsg(result, std::string("内部异常: ") + e.what());
        }
        return -11;
    } catch (...) {
        fprintf(stderr, "[hp_drizzle_api] hp_drizzle_run_phase1_hips: C 边界捕获未知异常\n");
        if (result) {
            std::memset(result, 0, sizeof(HpDrizzleResult));
            setErrorMsg(result, "内部未知异常");
        }
        return -11;
    }
}

HP_DRIZZLE_API int hp_drizzle_run_hips(PipelineFrame* frame,
                                       int nside, int nested, double pixfrac,
                                       const char* hips_dir,
                                       const char* legacy_hiss_path,
                                       HpDrizzleResult* result,
                                       int precision_mode)
{
    // P1 (R9-A): C 边界异常屏障, 同 hp_drizzle_run 薄壳 (对齐 -11)。
    try {
    return run_drizzle_internal(frame, nside, nested, pixfrac,
                                legacy_hiss_path, hips_dir,
                                /*write_hips=*/true,
                                /*write_legacy_hiss=*/(legacy_hiss_path && legacy_hiss_path[0] != '\0'),
                                result, precision_mode,
                                /*hips_profile=*/0, /*hips_filter_passband=*/nullptr);
    } catch (const std::exception& e) {
        fprintf(stderr, "[hp_drizzle_api] hp_drizzle_run_hips: C 边界捕获异常: %s\n", e.what());
        if (result) {
            std::memset(result, 0, sizeof(HpDrizzleResult));
            setErrorMsg(result, std::string("内部异常: ") + e.what());
        }
        return -11;
    } catch (...) {
        fprintf(stderr, "[hp_drizzle_api] hp_drizzle_run_hips: C 边界捕获未知异常\n");
        if (result) {
            std::memset(result, 0, sizeof(HpDrizzleResult));
            setErrorMsg(result, "内部未知异常");
        }
        return -11;
    }
}
