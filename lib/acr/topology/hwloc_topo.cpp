// lib/acr/topology/hwloc_topo.cpp — hwloc 拓扑/NUMA/PCI 探测实现
// ADR-003：hwloc 是 ACR 唯一的硬件拓扑/NUMA/PCI 探测来源。
//
// 设计要点：
// - PIMPL 封装 hwloc_topology_t，公共头不暴露 hwloc 类型
// - 无 hwloc（ACR_HAVE_HWLOC 未定义）时 available()=false，to_json() 返回 unavailable
// - 不抛异常，所有 hwloc 错误降级为缺失字段（null）或 unavailable
// - JSON 手写（ostringstream），不引入 nlohmann_json 依赖
// - 枚举：package/core/PU/cache(L1/L2/L3)/NUMA 节点/PCI 设备
#include "astro/compute/topology.hpp"

#include <atomic>
#include <sstream>
#include <string>
#include <vector>

#if defined(__has_include)
#  if __has_include(<hwloc.h>)
#    define ACR_HAVE_HWLOC 1
#  endif
#endif

#ifdef ACR_HAVE_HWLOC
#  include <hwloc.h>
#endif

namespace astro::compute {

#ifdef ACR_HAVE_HWLOC

namespace {

// RAII wrapper for hwloc_topology_t
struct HwlocGuard {
    hwloc_topology_t topo{nullptr};
    bool loaded{false};
    ~HwlocGuard() { if (topo) hwloc_topology_destroy(topo); }
};

// 转义 JSON 字符串（基本转义）
std::string esc_json(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 2);
    for (char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                    out += buf;
                } else {
                    out += c;
                }
        }
    }
    return out;
}

const char* cache_type_str(hwloc_obj_cache_type_t t) {
    switch (t) {
        case HWLOC_OBJ_CACHE_UNIFIED:  return "unified";
        case HWLOC_OBJ_CACHE_DATA:     return "data";
        case HWLOC_OBJ_CACHE_INSTRUCTION: return "instruction";
        default: return "unknown";
    }
}

} // anonymous namespace

struct HwlocTopology::Impl {
    HwlocGuard guard;
    bool ok{false};
};

HwlocTopology::HwlocTopology() : impl_(std::make_unique<Impl>()) {
    hwloc_topology_t t = nullptr;
    if (hwloc_topology_init(&t) != 0) return;
    impl_->guard.topo = t;
    // 启用 PCI 设备枚举（默认可能被过滤）
    unsigned long flags = 0;
    hwloc_topology_set_flags(t, HWLOC_TOPOLOGY_FLAG_WHOLE_SYSTEM |
                                HWLOC_TOPOLOGY_FLAG_IS_THISSYSTEM);
    (void)flags;
    if (hwloc_topology_load(t) != 0) return;
    impl_->guard.loaded = true;
    impl_->ok = true;
}

HwlocTopology::~HwlocTopology() = default;
HwlocTopology::HwlocTopology(HwlocTopology&&) noexcept = default;
HwlocTopology& HwlocTopology::operator=(HwlocTopology&&) noexcept = default;

bool HwlocTopology::available() const noexcept {
    return impl_ && impl_->ok;
}

std::string HwlocTopology::to_json() const {
    if (!impl_ || !impl_->ok || !impl_->guard.topo) {
        return R"({"status":"unavailable"})";
    }
    hwloc_topology_t topo = impl_->guard.topo;
    std::ostringstream os;
    os << "{";
    os << "\"status\":\"ok\"";

    // ----- package（CPU socket）-----
    unsigned n_pkg = hwloc_get_nbobjs_by_type(topo, HWLOC_OBJ_PACKAGE);
    os << ",\"packages\":[";
    for (unsigned i = 0; i < n_pkg; ++i) {
        hwloc_obj_t pkg = hwloc_get_obj_by_type(topo, HWLOC_OBJ_PACKAGE, i);
        if (i) os << ",";
        os << "{";
        os << "\"os_index\":" << static_cast<long long>(pkg->os_index);
        const char* vendor = hwloc_obj_get_info_by_name(pkg, "CPUVendor");
        const char* model   = hwloc_obj_get_info_by_name(pkg, "CPUModel");
        const char* stepping = hwloc_obj_get_info_by_name(pkg, "CPUStepping");
        const char* family   = hwloc_obj_get_info_by_name(pkg, "CPUFamily");
        os << ",\"vendor\":\"" << (vendor ? esc_json(vendor).c_str() : "") << "\"";
        os << ",\"model\":\"" << (model ? esc_json(model).c_str() : "") << "\"";
        if (stepping) os << ",\"stepping\":\"" << esc_json(stepping) << "\"";
        if (family)   os << ",\"family\":\"" << esc_json(family) << "\"";
        os << "}";
    }
    os << "]";

    // ----- NUMA 节点 -----
    unsigned n_numa = hwloc_get_nbobjs_by_type(topo, HWLOC_OBJ_NUMANODE);
    os << ",\"numa_nodes\":[";
    for (unsigned i = 0; i < n_numa; ++i) {
        hwloc_obj_t node = hwloc_get_obj_by_type(topo, HWLOC_OBJ_NUMANODE, i);
        if (i) os << ",";
        os << "{";
        os << "\"os_index\":" << static_cast<long long>(node->os_index);
        os << ",\"local_memory\":" << static_cast<unsigned long long>(node->attr->numanode.local_memory);
        // page_type 等略，ACR 不需要
        os << "}";
    }
    os << "]";

    // ----- core / PU 计数 -----
    unsigned n_core = hwloc_get_nbobjs_by_type(topo, HWLOC_OBJ_CORE);
    unsigned n_pu   = hwloc_get_nbobjs_by_type(topo, HWLOC_OBJ_PU);
    os << ",\"cores\":" << static_cast<long long>(n_core);
    os << ",\"pus\":" << static_cast<long long>(n_pu);

    // ----- cache 层级（L1/L2/L3）-----
    auto emit_cache = [&](hwloc_obj_type_t t, const char* name) {
        unsigned n = hwloc_get_nbobjs_by_type(topo, t);
        os << ",\"" << name << "\":[";
        for (unsigned i = 0; i < n; ++i) {
            hwloc_obj_t c = hwloc_get_obj_by_type(topo, t, i);
            if (i) os << ",";
            os << "{";
            os << "\"os_index\":" << static_cast<long long>(c->os_index);
            os << ",\"depth\":" << static_cast<long long>(c->attr->cache.depth);
            os << ",\"size\":" << static_cast<unsigned long long>(c->attr->cache.size);
            os << ",\"linesize\":" << static_cast<unsigned long long>(c->attr->cache.linesize);
            os << ",\"type\":\"" << cache_type_str(c->attr->cache.type) << "\"";
            os << "}";
        }
        os << "]";
    };
    emit_cache(HWLOC_OBJ_L1CACHE, "l1_cache");
    emit_cache(HWLOC_OBJ_L2CACHE, "l2_cache");
    emit_cache(HWLOC_OBJ_L3CACHE, "l3_cache");

    // ----- PCI 设备（GPU PCI 局部性由 Phase D 关联）-----
    unsigned n_pci = hwloc_get_nbobjs_by_type(topo, HWLOC_OBJ_PCI_DEVICE);
    os << ",\"pci_devices\":[";
    for (unsigned i = 0; i < n_pci; ++i) {
        hwloc_obj_t p = hwloc_get_obj_by_type(topo, HWLOC_OBJ_PCI_DEVICE, i);
        if (i) os << ",";
        os << "{";
        os << "\"domain\":" << static_cast<unsigned long long>(p->attr->pcidev.domain);
        os << ",\"bus\":" << static_cast<unsigned long long>(p->attr->pcidev.bus);
        os << ",\"dev\":" << static_cast<unsigned long long>(p->attr->pcidev.dev);
        os << ",\"func\":" << static_cast<unsigned long long>(p->attr->pcidev.func);
        os << ",\"vendor_id\":" << static_cast<unsigned long long>(p->attr->pcidev.vendor_id);
        os << ",\"device_id\":" << static_cast<unsigned long long>(p->attr->pcidev.device_id);
        os << ",\"class\":" << static_cast<unsigned long long>(p->attr->pcidev.class_id);
        const char* name = hwloc_obj_get_info_by_name(p, "PCIDeviceName");
        if (name) os << ",\"name\":\"" << esc_json(name) << "\"";
        os << "}";
    }
    os << "]";

    // ----- topology depth -----
    int depth = hwloc_topology_get_depth(topo);
    os << ",\"topology_depth\":" << depth;

    os << "}";
    return os.str();
}

#else // !ACR_HAVE_HWLOC

struct HwlocTopology::Impl {};

HwlocTopology::HwlocTopology() : impl_(std::make_unique<Impl>()) {}
HwlocTopology::~HwlocTopology() = default;
HwlocTopology::HwlocTopology(HwlocTopology&&) noexcept = default;
HwlocTopology& HwlocTopology::operator=(HwlocTopology&&) noexcept = default;

bool HwlocTopology::available() const noexcept { return false; }

std::string HwlocTopology::to_json() const {
    return R"({"status":"unavailable"})";
}

#endif // ACR_HAVE_HWLOC

// ===== 自由函数 =====
std::string detect_topology() {
    HwlocTopology topo;
    return topo.to_json();
}

} // namespace astro::compute
