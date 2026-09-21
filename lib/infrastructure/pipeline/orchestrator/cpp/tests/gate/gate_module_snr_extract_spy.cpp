// ============================================================================
// gate_module_snr_extract_spy.cpp —— ORCH-001 批次 3 的**测试替身**（非生产）
// ----------------------------------------------------------------------------
// 只替身两个不在本门判据内的入口：
//   snr_extract_model_v3 / snr_free_model_v3
// 它们位于 run_stage_snr 的前半段（稀疏控制点提取）；本门的判据是**后半段的
// 科学配置交接**（header SATURATE/DATAMAX → SnrNoiseModelConfig → 噪声模型
// 运行结果），故用一个最小合法模型让流程走到噪声块即可。
//
// 噪声模型本体（snr_noise_model_v1 / _f64 / _default_config / _fill / _free）
// **不是替身**：由生产源 lib/algorithms/noise_snr/cpp/src/noise_model.cpp 提供
// （见 eng/tests/CMakeLists.txt 的 orchestrator_gate_module_snr 目标）。
//
// 语义：返回 0（成功）+ n_points=0（无控制点，合法：snr_model 块仍按 52B 头写）
// ⇒ run_stage_snr 不触发 ret==1/2/3 的提前返回，继续执行 NoiseWeightModelV1。
// ============================================================================

#include "snr_estimator.h"

#include <cstdint>

extern "C" {

SNR_API int snr_extract_model_v3(const double* psf, int n_stars,
                                 double sigma_residual,
                                 const SnrWcsParams* wcs,
                                 int value_dtype,
                                 const int64_t* star_ids,
                                 const uint32_t* quality_flags,
                                 const uint32_t* photometric_status,
                                 SnrModelV3* out_model) {
    (void)psf; (void)n_stars; (void)wcs; (void)star_ids;
    (void)quality_flags; (void)photometric_status;
    if (out_model == nullptr) return 3;
    if (sigma_residual <= 0.0) return 2;
    out_model->n_points = 0;          // 无控制点：本门不判据控制点面
    out_model->value_dtype = (uint8_t)value_dtype;
    out_model->points = nullptr;
    out_model->snr_phot = 1.0;
    out_model->median_snr = 1.0;
    out_model->idw_power = 2.0;
    out_model->median_source_snr = 0.0;
    out_model->frame_depth_flux5_adu = 0.0;
    out_model->frame_depth_m5_mag = 0.0;
    return 0;
}

SNR_API void snr_free_model_v3(SnrModelV3* model) {
    if (model == nullptr) return;
    model->points = nullptr;          // 本替身从不分配控制点数组
    model->n_points = 0;
}

}  // extern "C"
