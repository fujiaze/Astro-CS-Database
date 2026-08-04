// lib/acr/api/kernel_registry.cpp — KernelRegistry 实现（23 号计划 §1）
#include "astro/compute/kernel_registry.hpp"

#include <algorithm>
#include <mutex>

namespace astro::compute {

bool KernelRegistry::register_kernel(const KernelRegistration& reg) {
    if (reg.id.empty() || reg.cpu == nullptr) {
        // 至少需要一个 CPU launcher；空 id 拒绝注册
        return false;
    }

    std::lock_guard<std::mutex> lk(mtx_);
    const std::string key(reg.id);
    if (by_id_.count(key) != 0) {
        return false;  // 重复注册：保留首个
    }

    auto node = std::make_unique<Node>();
    node->id_storage = key;
    node->reg = reg;
    node->reg.id = node->id_storage;  // 指向稳定存储
    by_id_.emplace(key, node.get());
    nodes_.push_back(std::move(node));
    return true;
}

const KernelRegistration* KernelRegistry::find(OperationId id) const {
    if (id.empty()) return nullptr;
    std::lock_guard<std::mutex> lk(mtx_);
    const std::string key(id);
    auto it = by_id_.find(key);
    if (it == by_id_.end()) return nullptr;
    return &it->second->reg;
}

bool KernelRegistry::supports(OperationId id, const std::string& backend) const {
    const KernelRegistration* reg = find(id);
    if (reg == nullptr) return false;
    if (backend == "cpu") return reg->cpu != nullptr;
    if (backend.rfind("cuda", 0) == 0) return reg->cuda.has_value();
    if (backend.rfind("hip", 0) == 0) return reg->hip.has_value();
    return false;
}

std::vector<std::string> KernelRegistry::operation_ids() const {
    std::lock_guard<std::mutex> lk(mtx_);
    std::vector<std::string> ids;
    ids.reserve(nodes_.size());
    for (const auto& n : nodes_) {
        ids.push_back(n->id_storage);
    }
    return ids;
}

namespace {

// 全局默认注册表：经典实验 CPU launcher 由 classic backend 注册。
// 惰性初始化，首次访问时创建；register/find 线程安全。
struct GlobalRegistryHolder {
    KernelRegistry registry;
};

} // anonymous namespace

KernelRegistry& global_kernel_registry() {
    static GlobalRegistryHolder holder;
    return holder.registry;
}

} // namespace astro::compute
