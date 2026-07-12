#ifndef AIO_PIPELINE_ENGINE_H
#define AIO_PIPELINE_ENGINE_H

#include "aio_pipeline.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 阶段处理函数签名 (in-place 模式: 直接修改 frame) */
typedef int (*PipelineStageHandler)(PipelineFrame* frame,
                                     const void* params,
                                     char* error_msg, int error_capacity);

/* 引擎句柄 (不透明指针) */
typedef struct PipelineEngine PipelineEngine;

/* 创建/销毁引擎 */
AIO_EXPORT PipelineEngine* aio_pipeline_engine_create(void);
AIO_EXPORT void aio_pipeline_engine_destroy(PipelineEngine* engine);

/* 注册阶段处理函数
 * stage: 阶段枚举 (STAGE_CALIBRATE ~ STAGE_STACK)
 * handler: 处理函数 (nullptr 表示跳过该阶段)
 * params: 阶段参数 (引擎不拥有, 调用方管理生命周期)
 * 返回: 0=成功, 非0=参数错误
 */
AIO_EXPORT int aio_pipeline_engine_register(PipelineEngine* engine,
                                             PipelineStage stage,
                                             PipelineStageHandler handler,
                                             const void* params);

/* 设置调试导出
 * dir: 导出目录 (nullptr 或空串则不导出)
 * stage_mask: 导出阶段位掩码 (bit0=calibrate后, bit1=solve后, ...)
 *             -1 = 所有阶段后导出
 * skip_pixels: 1=跳过像素数据只导出元数据, 0=导出全部
 */
AIO_EXPORT int aio_pipeline_engine_set_debug(PipelineEngine* engine,
                                              const char* dir,
                                              int stage_mask,
                                              int skip_pixels);

/* 设置自动释放 (默认开启)
 * auto_free: 1=阶段后自动丢弃阶段对应的块, 0=保留所有块
 * 块丢弃策略 (替代旧版 auto_free):
 *   PLATESOLVE  后丢弃: weight
 *   PHOTOMETRIC 后丢弃: star_det, gaia_cat, psf
 *   DRIZZLE     后丢弃: data, snr, weight, grad_map, cal_stats, photo_stats
 *   STACK       后丢弃: healpix
 */
AIO_EXPORT int aio_pipeline_engine_set_auto_free(PipelineEngine* engine,
                                                   int auto_free);

/* 自定义某阶段后要丢弃的块 (覆盖默认策略)
 * block_names: 逗号分隔的块名列表 (如 "weight,psf"), nullptr 或空串表示不丢弃
 * 返回: 0=成功, 非0=失败
 * 注意: 调用后 auto_free 对该阶段不再生效 (由自定义策略接管)
 */
AIO_EXPORT int aio_pipeline_engine_set_block_drop(PipelineEngine* engine,
                                                    PipelineStage stage,
                                                    const char* block_names);

/* 单帧执行
 * frame: 输入帧 (已填充 pixel_data 等)
 * from_stage: 起始阶段 (STAGE_CALIBRATE ~ STAGE_STACK)
 * to_stage: 结束阶段 (STAGE_CALIBRATE ~ STAGE_STACK)
 * error_msg: 错误信息输出缓冲区
 * error_capacity: 错误缓冲区大小
 * 返回: 0=成功, 非0=失败 (错误信息写入 error_msg)
 */
AIO_EXPORT int aio_pipeline_engine_run_single(PipelineEngine* engine,
                                                PipelineFrame* frame,
                                                int from_stage, int to_stage,
                                                char* error_msg, int error_capacity);

/* 批量执行 (多帧并行)
 * frames: 帧指针数组
 * n_frames: 帧数
 * n_threads: 线程数 (默认 16, <=0 则用 16)
 * from_stage/to_stage: 同 run_single
 * 注意: 若 to_stage 包含 STAGE_STACK, 引擎会在所有帧并行完成后串行执行 stack
 * error_msg/error_capacity: 同上 (记录第一个失败的错误)
 * 返回: 成功帧数 (若全部成功则 == n_frames)
 */
AIO_EXPORT int aio_pipeline_engine_run_batch(PipelineEngine* engine,
                                               PipelineFrame** frames, int n_frames,
                                               int n_threads,
                                               int from_stage, int to_stage,
                                               char* error_msg, int error_capacity);

/* 获取阶段名称字符串 (用于日志/调试) */
AIO_EXPORT const char* aio_pipeline_stage_name(PipelineStage stage);

#ifdef __cplusplus
}
#endif

#endif /* AIO_PIPELINE_ENGINE_H */
