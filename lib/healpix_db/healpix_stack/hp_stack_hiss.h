#ifndef HP_STACK_HISS_H
#define HP_STACK_HISS_H

// ============================================================================
// 内存 sigma-clip 堆叠 (新版 .hiss → .hcsd 路径)
//
// 流程:
//   1. 第一遍扫描: 逐帧 hiss_read, 建立全局 ipix → index 映射
//   2. 分配 count/sum/sum_sq 三个 float64 数组 (内存与帧数无关)
//   3. 第二遍扫描: 按 ipix 对齐累加
//   4. sigma-clip 迭代 (最多 max_iter 次): 重新读帧, 剔除 |v-mean|>sigma*std
//   5. 输出 mean = sum/count, 调用 hcsd_write 写入 .hcsd (含子叶索引)
//
// 内存占用: 3 × n_unique_pix × 8B (count/sum/sum_sq 三个 double 数组)
// 依赖: healpix_io.dll (hiss_read / hcsd_write / hio_free)
// ============================================================================

#include <stdint.h>

#ifdef _WIN32
#define HP_STACK_HISS_EXPORT __declspec(dllexport)
#else
#define HP_STACK_HISS_EXPORT __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

// 堆叠多个 .hiss 文件到 .hcsd
// hiss_paths: .hiss 文件路径数组 (UTF-8 字符串数组)
// n_frames: 帧数
// output_hcsd_path: 输出 .hcsd 路径 (UTF-8)
// sigma: sigma-clip 阈值 (通常 3.0)
// max_iter: 最大迭代次数 (通常 5)
// 返回: 0=成功, <0=失败
HP_STACK_HISS_EXPORT int hp_stack_hiss(const char** hiss_paths, int n_frames,
                                       const char* output_hcsd_path,
                                       double sigma, int max_iter);

#ifdef __cplusplus
}
#endif

#endif // HP_STACK_HISS_H
