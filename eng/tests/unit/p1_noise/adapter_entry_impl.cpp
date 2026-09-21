/* adapter_entry_impl.cpp - P1-NOISE-IMPL direct 通道 TU (hips 先例同款):
 * 直通 include 模块 adapter, 与 DLL 同源同文件编进测试可执行 (direct
 * adapter 不经 dlopen; 生产符号 snr_noise_* 由 p1noise_under_test STATIC
 * 解析 —— 同一 noise_model.cpp 权威源, P1-NOISE-TEST 冻结被测面)。
 * 桥接函数暴露 direct vtable 给测试 TU。 */
#include "../../../../lib//algorithms/noise_snr/src/module_entry.cpp"

extern "C" const acs_module_api_v1* p1noise_direct_api_v1(void) {
    return &g_noise_api;
}
