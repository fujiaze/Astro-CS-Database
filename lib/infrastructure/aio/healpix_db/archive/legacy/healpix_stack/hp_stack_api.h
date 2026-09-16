#ifndef HP_STACK_API_H
#define HP_STACK_API_H

// ============================================================================
// 稀疏 HEALpix 堆栈存储模块 C API 导出层
// 供 Python (ctypes) 或其他 C 程序调用
//
// 所有返回 char* 的函数: 调用方负责用 hp_stack_free_string() 释放
// ============================================================================

#include <stddef.h>
#include <stdint.h>
#include "aio_pipeline.h"

#ifdef _WIN32
#define HP_STACK_EXPORT __declspec(dllexport)
#else
#define HP_STACK_EXPORT __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

// 不透明类型 (隐藏 C++ 实现)
typedef struct StackDatabase StackDatabase;

// ============================================================================
// Stack 阶段结果 (hp_stack_run 输出)
// ============================================================================
typedef struct {
    int64_t pixel_count;   // 堆栈后的像素数
    int64_t tile_count;    // 涉及的 tile 数 (hp_stack_run 固定为 1)
    int     success;       // 0=失败, 1=成功
} HpStackResult;

// ============================================================================
// 数据库管理
// ============================================================================

// 创建数据库 (写入 meta.json)
// configJson: 配置 JSON (可空, 使用默认配置)
//   {"nsideData":32768,"tileNside":512,"bands":["L","R",...],
//    "sigmaClipLow":3.0,"sigmaClipHigh":3.0,"nested":true}
HP_STACK_EXPORT StackDatabase* hp_stack_db_create(const char* dbPath, const char* configJson);

// 打开已有数据库
HP_STACK_EXPORT StackDatabase* hp_stack_db_open(const char* dbPath);

// 关闭数据库
HP_STACK_EXPORT void hp_stack_db_close(StackDatabase* db);

// ============================================================================
// 堆栈更新
// ============================================================================

// 全局更新: 处理一组帧, 更新数据库
// framesJson: JSON 数组, 每个元素含 pixels 数组
//   [{"pixels":[{"healpixPix":1,"value":2.5,"snr":10,"weight":1.0},...]}, ...]
// 返回: 处理的像素数, 失败返回 -1
HP_STACK_EXPORT int hp_stack_update_global(StackDatabase* db, const char* framesJson);

// 局部更新: 只更新指定文件范围
// fileRangeJson: JSON 对象 {文件路径: [pixels...]}
//   {"frame1.ahpx":[{"healpixPix":1,"value":2.5,"snr":10,"weight":1.0},...]}
// 返回: 处理的像素数, 失败返回 -1
HP_STACK_EXPORT int hp_stack_update_range(StackDatabase* db, const char* fileRangeJson);

// ============================================================================
// 读取堆栈数据
// ============================================================================

// 读取 tile 数据, 返回 JSON 字符串 (malloc 分配, 需用 hp_stack_free_string 释放)
// JSON 格式:
//   {"tileIpix":..,"nside":..,"tileNside":..,"pixelCount":..,"bandCount":..,
//    "pixels":[..],"bands":[{"values":[..],"variance":[..],"counts":[..]},...]}
// 失败返回 NULL
HP_STACK_EXPORT char* hp_stack_read_tile(StackDatabase* db, int64_t tileIpix);

// 释放 API 返回的字符串
HP_STACK_EXPORT void hp_stack_free_string(char* str);

// ============================================================================
// Stack 阶段 PipelineFrame 入口
// ============================================================================

// Stack 阶段: 接收多帧 PipelineFrame, 输出 .ahps 文件
// frames: PipelineFrame 数组 (每帧含 healpix_pixels/healpix_snr/healpix_ipix)
// n_frames: 帧数
// output_path: 输出 .ahps 文件路径
// result: 输出堆栈结果统计 (可空)
// 返回: 0=成功, 非0=失败
HP_STACK_EXPORT int hp_stack_run(const PipelineFrame** frames, int n_frames,
                                  const char* output_path, HpStackResult* result);

// ============================================================================
// 梯度校正叠加 (.hiss → .hcsd, 含球面 TPS 梯度校正)
//
// 完整流程: 采样 → Gauss-Seidel 拟合 → 校正叠加 → .hcsd 输出
// 内部: gradient_sampler + gradient_fitter + corrected_stacker
//
// hiss_paths: .hiss 文件路径数组 (UTF-8)
// n_frames: 帧数
// gaia_data_dir: Gaia 数据目录 (用于星拒绝, 可空=跳过星拒绝)
// output_hcsd_path: 输出 .hcsd 路径 (UTF-8)
// sigma: sigma-clip 阈值 (通常 3.0)
// max_iter: sigma-clip 最大迭代 (通常 5)
// gradient_max_iter: Gauss-Seidel 最大迭代 (通常 10)
// gradient_lambda: TPS 正则化参数 (通常 1e-4)
// sigma_clip_method: sigma-clip 方法 (GAP-017 新增)
//                    "standard" 或 nullptr = 普通 sigma-clip (默认, 向后兼容)
//                    "winsorized"          = Winsorized sigma clip (稳健)
// winsorize_low_pct: Winsorized 下分位数 (默认 0.05, 仅 method="winsorized" 时生效)
// winsorize_high_pct: Winsorized 上分位数 (默认 0.95, 仅 method="winsorized" 时生效)
// 返回: 0=成功, <0=失败
// ============================================================================
HP_STACK_EXPORT int hp_stack_gradient_corrected(
    const char** hiss_paths, int n_frames,
    const char* gaia_data_dir,
    const char* output_hcsd_path,
    double sigma, int max_iter,
    int gradient_max_iter, double gradient_lambda,
    const char* sigma_clip_method,
    double winsorize_low_pct,
    double winsorize_high_pct);

// ============================================================================
// HEALpix 工具函数
// ============================================================================

// RA/Dec(度) → 像素号
HP_STACK_EXPORT int64_t hp_radec2pix(int nside, int nested, double ra_deg, double dec_deg);

// 像素号 → RA/Dec(度)
HP_STACK_EXPORT void hp_pix2radec(int nside, int nested, int64_t ipix,
                                  double* ra_deg, double* dec_deg);

// 像素分辨率 (角秒)
HP_STACK_EXPORT double hp_pixel_resolution_arcsec(int nside);

#ifdef __cplusplus
}
#endif

#endif // HP_STACK_API_H
