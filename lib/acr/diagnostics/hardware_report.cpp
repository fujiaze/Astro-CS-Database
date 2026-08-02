// lib/acr/diagnostics/hardware_report.cpp — 完整硬件指纹报告
// 合并 hwloc + cpu_features + GPU 回调为 hardware.json。
//
// schema：CPU vendor/model/stepping/ISA mask/cache/NUMA/GPU UUID/PCI/driver/compiler/build。
// GPU 回调由 Phase D 注册，未注册时 gpu 字段为 null。
#include "astro/compute/topology.hpp"

#include <atomic>
#include <sstream>
#include <string>

namespace astro::compute {

namespace {

// 线程安全的 GPU 回调注册（首次生效）
std::atomic<GpuReportCallback> g_gpu_cb{nullptr};

// 编译器版本字符串
std::string compiler_string() {
#if defined(__clang__)
    return "clang " + std::to_string(__clang_major__) + "." +
           std::to_string(__clang_minor__) + "." + std::to_string(__clang_patchlevel__);
#elif defined(__GNUC__)
    return "g++ " + std::to_string(__GNUC__) + "." +
           std::to_string(__GNUC_MINOR__) + "." + std::to_string(__GNUC_PATCHLEVEL__);
#elif defined(_MSC_VER)
    return "MSVC " + std::to_string(_MSC_VER);
#else
    return "unknown";
#endif
}

int cxx_standard() {
#if defined(_MSVC_LANG)
    return static_cast<int>(_MSVC_LANG);
#elif defined(__cplusplus)
    return static_cast<int>(__cplusplus);
#else
    return 0;
#endif
}

} // anonymous namespace

void register_gpu_report_callback(GpuReportCallback cb) {
    // 首次注册生效，后续忽略（CAS 防止覆盖）
    GpuReportCallback expected = nullptr;
    g_gpu_cb.compare_exchange_strong(expected, cb, std::memory_order_acq_rel);
}

std::string generate_hardware_report() {
    // 子报告
    std::string topo_json = detect_topology();
    std::string isa_json  = detect_isa_caps();
    GpuReportCallback gpu_cb = g_gpu_cb.load(std::memory_order_acquire);
    std::string gpu_json = gpu_cb ? gpu_cb() : std::string("null");

    std::ostringstream os;
    os << "{";
    os << "\"schema\":\"acr.hardware.v1\"";
    os << ",\"topology\":" << topo_json;
    os << ",\"isa\":" << isa_json;
    os << ",\"gpu\":" << gpu_json;
    os << ",\"compiler\":{";
    os << "\"name\":\"" << compiler_string() << "\"";
    os << ",\"cxx_std\":" << cxx_standard();
    os << "}";
    os << ",\"build\":{";
    os << "\"date\":\"" << __DATE__ << "\"";
    os << ",\"time\":\"" << __TIME__ << "\"";
    os << "}";
    os << "}";
    return os.str();
}

} // namespace astro::compute
