// lib/acr/core/task_descriptor.cpp — TaskDescriptor 工具实现
// Phase B3：TaskDescriptor 本身是 POD 结构（header-only），
// 这里提供诊断/日志/序列化辅助函数，以及 CMake 编译单元。
#include "task_descriptor.hpp"

#include <sstream>
#include <string>

namespace astro::compute {

// ===== TaskDescriptor 诊断字符串（日志/调试用）=====
std::string task_descriptor_summary(const TaskDescriptor& d) {
    std::ostringstream os;
    os << "{";
    os << "\"operation_id\":\"" << d.operation_id << "\"";
    os << ",\"task_class\":\"" << task_class_str(d.traits.task_class) << "\"";
    os << ",\"access\":\"" << access_pattern_str(d.traits.access) << "\"";
    os << ",\"intensity\":\"" << intensity_class_str(d.traits.intensity) << "\"";
    os << ",\"uniformity\":\"" << work_uniformity_str(d.traits.uniformity) << "\"";
    os << ",\"precision\":\"" << precision_str(d.precision) << "\"";
    os << ",\"work_size\":" << d.work_size();
    os << ",\"bytes_read\":" << d.bytes_read;
    os << ",\"bytes_written\":" << d.bytes_written;
    os << ",\"input_residency\":" << d.input_residency;
    os << ",\"output_residency\":" << d.output_residency;
    os << ",\"halo_x\":" << d.traits.halo_x;
    os << ",\"halo_y\":" << d.traits.halo_y;
    os << ",\"splittable\":" << (d.traits.splittable ? "true" : "false");
    os << ",\"mixed_device_safe\":" << (d.traits.mixed_device_safe ? "true" : "false");
    os << "}";
    return os.str();
}

// ===== TaskTraits 默认值校验（测试/诊断用）=====
bool task_traits_valid(const TaskTraits& t) noexcept {
    if (t.active_fraction_hint < 0.0 || t.active_fraction_hint > 1.0) return false;
    return true;
}

} // namespace astro::compute
