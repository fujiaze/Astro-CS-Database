// ============================================================================
// 管线编排引擎实现
//
// 功能:
//   - 注册各阶段处理函数 (PipelineStageHandler)
//   - 单帧串行执行 (run_single)
//   - 多帧批量并行 (run_batch, OpenMP 16线程)
//   - 内存生命周期管理 (阶段间自动释放中间数据)
//   - XML 调试导出 (每阶段后可选导出)
//   - 错误处理 + 状态追踪
//
// 日志: 所有诊断/进度日志输出到 stderr
// ============================================================================

#include "aio_pipeline_engine.h"
#include "aio_pipeline.h"

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <string>
#include <sstream>

#ifdef _WIN32
#include <windows.h>
#endif

#ifdef _OPENMP
#include <omp.h>
#endif

// ============================================================================
// 引擎内部结构
// ============================================================================
struct PipelineEngine {
    PipelineStageHandler handlers[5];  // 每阶段处理函数
    const void*          params[5];    // 每阶段参数 (调用方管理生命周期)
    char                 debug_dir[512]; // 调试导出目录
    int                  debug_stage_mask; // 导出阶段位掩码
    int                  debug_skip_pixels; // 跳过像素数据
    int                  auto_free;    // 自动释放中间数据 (默认 1)
};

// ============================================================================
// 辅助: UTF-8 路径文件名提取 (去掉目录路径, 只留文件名)
// ============================================================================
static std::string extract_basename(const char* path) {
    if (!path || path[0] == '\0') return "frame";
    std::string s(path);
    // 查找最后一个路径分隔符
    size_t pos = s.find_last_of("/\\");
    if (pos != std::string::npos) {
        s = s.substr(pos + 1);
    }
    // 去掉扩展名
    pos = s.find_last_of('.');
    if (pos != std::string::npos) {
        s = s.substr(0, pos);
    }
    if (s.empty()) s = "frame";
    return s;
}

// ============================================================================
// 辅助: 从 source_path 生成调试 XML 文件路径
// ============================================================================
static std::string build_debug_xml_path(const char* debug_dir,
                                          const char* source_path,
                                          const char* stage_name) {
    std::string basename = extract_basename(source_path);
    std::ostringstream oss;
    oss << debug_dir << "/" << basename << "_" << stage_name << ".xml";
    return oss.str();
}

// ============================================================================
// 辅助: 阶段后自动释放中间数据 (按生命周期约定表)
// ============================================================================
static void auto_free_after_stage(PipelineFrame* frame, PipelineStage stage) {
    switch (stage) {
        case STAGE_PLATESOLVE:
            // 校准权重不再需要
            aio_pipeline_frame_free_weight(frame);
            fprintf(stderr, "[engine] auto-free: weight_data after platesolve\n");
            break;
        case STAGE_DRIZZLE:
            // 已转换为 HEALPix, 平面数据可释放
            aio_pipeline_frame_free_pixels(frame);
            aio_pipeline_frame_free_snr(frame);
            aio_pipeline_frame_free_weight(frame);
            fprintf(stderr, "[engine] auto-free: pixel_data/snr_data/weight_data after drizzle\n");
            break;
        case STAGE_STACK:
            // 已输出到 .ahps
            aio_pipeline_frame_free_healpix(frame);
            fprintf(stderr, "[engine] auto-free: healpix_* after stack\n");
            break;
        default:
            break;
    }
}

// ============================================================================
// 辅助: 阶段后调试导出
// ============================================================================
static void debug_export(const PipelineEngine* eng, const PipelineFrame* frame,
                          PipelineStage stage) {
    if (!eng->debug_dir[0]) return;
    // 检查阶段位掩码
    int bit = 1 << static_cast<int>(stage);
    if ((eng->debug_stage_mask & bit) == 0) return;

    const char* stage_name = aio_pipeline_stage_name(stage);
    std::string xml_path = build_debug_xml_path(eng->debug_dir,
                                                 frame->source_path,
                                                 stage_name);
    std::string comment = std::string("after ") + stage_name;
    if (eng->debug_skip_pixels) {
        comment += " (skip_pixels=1)";
    }

    int ret = aio_pipeline_export_xml(frame, xml_path.c_str(), comment.c_str());
    if (ret != 0) {
        fprintf(stderr, "[engine] WARNING: debug export failed for stage %s (ret=%d)\n",
                stage_name, ret);
    } else {
        fprintf(stderr, "[engine] debug export: %s\n", xml_path.c_str());
    }
}

// ============================================================================
// 辅助: 执行单个阶段
// 返回 0=成功, 非0=失败
// ============================================================================
static int execute_stage(PipelineEngine* eng, PipelineStage stage,
                          PipelineFrame* frame,
                          char* error_msg, int error_capacity) {
    int idx = static_cast<int>(stage);
    if (idx < 0 || idx >= 5) {
        if (error_msg && error_capacity > 0) {
            snprintf(error_msg, error_capacity, "invalid stage %d", idx);
        }
        return -1;
    }

    PipelineStageHandler handler = eng->handlers[idx];
    if (!handler) {
        // 未注册的阶段, 跳过
        fprintf(stderr, "[engine] stage %s: skipped (no handler)\n",
                aio_pipeline_stage_name(stage));
        return 0;
    }

    fprintf(stderr, "[engine] stage %s: start\n", aio_pipeline_stage_name(stage));
    size_t mem_before = aio_pipeline_frame_memory_usage(frame);
    fprintf(stderr, "[engine]   memory before: %.2f MB\n", mem_before / 1048576.0);

    char local_error[512] = {0};
    int ret = handler(frame, eng->params[idx], local_error, sizeof(local_error) - 1);

    size_t mem_after = aio_pipeline_frame_memory_usage(frame);
    fprintf(stderr, "[engine]   memory after:  %.2f MB (delta: %+.2f MB)\n",
            mem_after / 1048576.0,
            (double)(mem_after - mem_before) / 1048576.0);

    if (ret != 0) {
        fprintf(stderr, "[engine] stage %s: FAILED (ret=%d)\n",
                aio_pipeline_stage_name(stage), ret);
        if (local_error[0]) {
            fprintf(stderr, "[engine]   error: %s\n", local_error);
        }
        if (error_msg && error_capacity > 0) {
            snprintf(error_msg, error_capacity, "stage %s failed: %s",
                     aio_pipeline_stage_name(stage),
                     local_error[0] ? local_error : "unknown error");
        }
        return ret;
    }

    // 更新 stages_completed 位掩码
    frame->stages_completed |= (1 << idx);
    fprintf(stderr, "[engine] stage %s: success (stages_completed=0x%x)\n",
            aio_pipeline_stage_name(stage), frame->stages_completed);

    // 调试导出
    debug_export(eng, frame, stage);

    // 自动释放中间数据
    if (eng->auto_free) {
        auto_free_after_stage(frame, stage);
    }

    return 0;
}

// ============================================================================
// 创建/销毁引擎
// ============================================================================
AIO_EXPORT PipelineEngine* aio_pipeline_engine_create(void) {
    PipelineEngine* eng = new (std::nothrow) PipelineEngine();
    if (!eng) return nullptr;

    memset(eng->handlers, 0, sizeof(eng->handlers));
    memset(eng->params, 0, sizeof(eng->params));
    eng->debug_dir[0] = '\0';
    eng->debug_stage_mask = 0;
    eng->debug_skip_pixels = 0;
    eng->auto_free = 1;

    fprintf(stderr, "[engine] created (auto_free=1)\n");
    return eng;
}

AIO_EXPORT void aio_pipeline_engine_destroy(PipelineEngine* eng) {
    if (!eng) return;
    fprintf(stderr, "[engine] destroyed\n");
    delete eng;
}

// ============================================================================
// 注册阶段处理函数
// ============================================================================
AIO_EXPORT int aio_pipeline_engine_register(PipelineEngine* eng,
                                             PipelineStage stage,
                                             PipelineStageHandler handler,
                                             const void* params) {
    if (!eng) return -1;
    int idx = static_cast<int>(stage);
    if (idx < 0 || idx >= 5) return -2;

    eng->handlers[idx] = handler;
    eng->params[idx] = params;

    fprintf(stderr, "[engine] registered stage %s (handler=%p, params=%p)\n",
            aio_pipeline_stage_name(stage),
            (void*)handler, (void*)params);
    return 0;
}

// ============================================================================
// 设置调试导出
// ============================================================================
AIO_EXPORT int aio_pipeline_engine_set_debug(PipelineEngine* eng,
                                              const char* dir,
                                              int stage_mask,
                                              int skip_pixels) {
    if (!eng) return -1;

    if (dir && dir[0]) {
        // 截断到 511 字符
        strncpy(eng->debug_dir, dir, sizeof(eng->debug_dir) - 1);
        eng->debug_dir[sizeof(eng->debug_dir) - 1] = '\0';
    } else {
        eng->debug_dir[0] = '\0';
    }
    eng->debug_stage_mask = stage_mask;
    eng->debug_skip_pixels = skip_pixels;

    fprintf(stderr, "[engine] debug: dir='%s', stage_mask=0x%x, skip_pixels=%d\n",
            eng->debug_dir, eng->debug_stage_mask, eng->debug_skip_pixels);
    return 0;
}

// ============================================================================
// 设置自动释放
// ============================================================================
AIO_EXPORT int aio_pipeline_engine_set_auto_free(PipelineEngine* eng,
                                                   int auto_free) {
    if (!eng) return -1;
    eng->auto_free = auto_free ? 1 : 0;
    fprintf(stderr, "[engine] auto_free=%d\n", eng->auto_free);
    return 0;
}

// ============================================================================
// 单帧执行
// ============================================================================
AIO_EXPORT int aio_pipeline_engine_run_single(PipelineEngine* eng,
                                                PipelineFrame* frame,
                                                int from_stage, int to_stage,
                                                char* error_msg, int error_capacity) {
    if (!eng || !frame) {
        if (error_msg && error_capacity > 0) {
            snprintf(error_msg, error_capacity, "null engine or frame");
        }
        return -1;
    }
    if (from_stage < 0 || from_stage > 4 || to_stage < 0 || to_stage > 4 || from_stage > to_stage) {
        if (error_msg && error_capacity > 0) {
            snprintf(error_msg, error_capacity, "invalid stage range: %d -> %d", from_stage, to_stage);
        }
        return -2;
    }

    fprintf(stderr, "[engine] === run_single: %s, stages %d->%d ===\n",
            frame->source_path[0] ? frame->source_path : "(unnamed)",
            from_stage, to_stage);

    for (int s = from_stage; s <= to_stage; ++s) {
        PipelineStage stage = static_cast<PipelineStage>(s);
        int ret = execute_stage(eng, stage, frame, error_msg, error_capacity);
        if (ret != 0) {
            return ret;
        }
    }

    fprintf(stderr, "[engine] === run_single: success ===\n");
    return 0;
}

// ============================================================================
// 批量执行 (多帧并行)
// ============================================================================
AIO_EXPORT int aio_pipeline_engine_run_batch(PipelineEngine* eng,
                                               PipelineFrame** frames, int n_frames,
                                               int n_threads,
                                               int from_stage, int to_stage,
                                               char* error_msg, int error_capacity) {
    if (!eng || !frames || n_frames <= 0) {
        if (error_msg && error_capacity > 0) {
            snprintf(error_msg, error_capacity, "invalid args: eng=%p frames=%p n=%d",
                     (void*)eng, (void*)frames, n_frames);
        }
        return -1;
    }
    if (from_stage < 0 || from_stage > 4 || to_stage < 0 || to_stage > 4 || from_stage > to_stage) {
        if (error_msg && error_capacity > 0) {
            snprintf(error_msg, error_capacity, "invalid stage range: %d -> %d", from_stage, to_stage);
        }
        return -2;
    }

    if (n_threads <= 0) n_threads = 16;

    // 判断是否包含 STACK 阶段
    int has_stack = (to_stage >= STAGE_STACK);
    int pre_stack_end = has_stack ? (STAGE_STACK - 1) : to_stage;

    fprintf(stderr, "[engine] === run_batch: %d frames, %d threads, stages %d->%d ===\n",
            n_frames, n_threads, from_stage, to_stage);

    // Phase 1: CALIBRATE → DRIZZLE (并行)
    int n_success = 0;
    int first_error_ret = 0;
    char first_error[512] = {0};

#ifdef _OPENMP
    omp_set_num_threads(n_threads);
#endif

    #pragma omp parallel for reduction(+:n_success) schedule(dynamic, 1)
    for (int i = 0; i < n_frames; ++i) {
        PipelineFrame* frame = frames[i];
        if (!frame) {
            #pragma omp critical
            {
                fprintf(stderr, "[engine] frame[%d]: null, skipping\n", i);
            }
            continue;
        }

        char local_error[512] = {0};
        fprintf(stderr, "[engine] frame[%d]: %s, processing stages %d->%d\n",
                i, frame->source_path[0] ? frame->source_path : "(unnamed)",
                from_stage, pre_stack_end);

        int ret = 0;
        for (int s = from_stage; s <= pre_stack_end; ++s) {
            PipelineStage stage = static_cast<PipelineStage>(s);
            ret = execute_stage(eng, stage, frame, local_error, sizeof(local_error) - 1);
            if (ret != 0) break;
        }

        if (ret == 0) {
            n_success++;
            fprintf(stderr, "[engine] frame[%d]: pre-stack success\n", i);
        } else {
            #pragma omp critical
            {
                if (first_error_ret == 0) {
                    first_error_ret = ret;
                    strncpy(first_error, local_error, sizeof(first_error) - 1);
                }
                fprintf(stderr, "[engine] frame[%d]: FAILED (ret=%d): %s\n",
                        i, ret, local_error);
            }
        }
    }

    fprintf(stderr, "[engine] pre-stack phase done: %d/%d success\n", n_success, n_frames);

    // Phase 2: STACK (串行)
    if (has_stack && n_success > 0) {
        // 收集成功的帧
        // 注意: STACK 阶段需要接收所有成功帧的 PipelineFrame 数组
        // 但 STACK handler 签名是 PipelineStageHandler(frame, params, ...)
        // 只接收单个 frame。需要特殊处理。

        // 方案: STACK 阶段的 handler 通过 params 传递所有帧的信息
        // 或者: 对每个帧执行 STACK handler（但 stack 是多帧合并，不是逐帧）
        // 实际上 STACK 阶段不适合用 PipelineStageHandler 签名

        // 临时方案: STACK 阶段跳过（在 Python 层单独处理）
        // 或者: 修改 STACK handler 签名
        // 这里先跳过，打印警告
        fprintf(stderr, "[engine] WARNING: STACK stage in batch mode requires Python-layer orchestration\n");
        fprintf(stderr, "[engine] STACK handler will be called per-frame (if registered)\n");

        if (eng->handlers[STAGE_STACK]) {
            for (int i = 0; i < n_frames; ++i) {
                if (!frames[i]) continue;
                if (!(frames[i]->stages_completed & (1 << STAGE_DRIZZLE))) continue;

                char local_error[512] = {0};
                int ret = execute_stage(eng, STAGE_STACK, frames[i],
                                        local_error, sizeof(local_error) - 1);
                if (ret != 0) {
                    fprintf(stderr, "[engine] frame[%d] STACK: FAILED: %s\n", i, local_error);
                }
            }
        }
    }

    if (n_success < n_frames && error_msg && error_capacity > 0) {
        snprintf(error_msg, error_capacity, "%d/%d frames failed. First error: %s",
                 n_frames - n_success, n_frames,
                 first_error[0] ? first_error : "unknown");
    }

    fprintf(stderr, "[engine] === run_batch done: %d/%d success ===\n", n_success, n_frames);
    return n_success;
}

// ============================================================================
// 阶段名称
// ============================================================================
AIO_EXPORT const char* aio_pipeline_stage_name(PipelineStage stage) {
    switch (stage) {
        case STAGE_CALIBRATE:   return "calibrate";
        case STAGE_PLATESOLVE:  return "platesolve";
        case STAGE_PHOTOMETRIC: return "photometric";
        case STAGE_DRIZZLE:     return "drizzle";
        case STAGE_STACK:       return "stack";
        default:                return "unknown";
    }
}
