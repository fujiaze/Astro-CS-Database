// lib/acr/scheduler/reduction_merger.cpp — ReductionMerger 实现
#include "reduction_merger.hpp"

#include <cstring>
#include <stdexcept>
#include <vector>

namespace astro::compute::scheduler {

struct ReductionMerger::Impl {
    std::mutex mtx;
    std::vector<unsigned char> identity;  // identity 值
    std::size_t elem_size{0};
    MergeFn fn{nullptr};
    std::vector<std::vector<unsigned char>> locals;
};

ReductionMerger::ReductionMerger() : impl_(std::make_unique<Impl>()) {}
ReductionMerger::~ReductionMerger() = default;

void ReductionMerger::init(const void* identity, std::size_t elem_size, MergeFn fn) {
    std::lock_guard<std::mutex> lk(impl_->mtx);
    if (elem_size == 0 || fn == nullptr) {
        throw std::invalid_argument("ReductionMerger::init: invalid elem_size/fn");
    }
    impl_->elem_size = elem_size;
    impl_->fn = fn;
    impl_->identity.assign(elem_size, 0);
    if (identity) std::memcpy(impl_->identity.data(), identity, elem_size);
    impl_->locals.clear();
}

void ReductionMerger::add_local(const void* local_acc) {
    if (local_acc == nullptr) return;
    std::lock_guard<std::mutex> lk(impl_->mtx);
    if (impl_->elem_size == 0) return;
    std::vector<unsigned char> buf(impl_->elem_size);
    std::memcpy(buf.data(), local_acc, impl_->elem_size);
    impl_->locals.push_back(std::move(buf));
}

void ReductionMerger::finalize(void* result_out) {
    if (result_out == nullptr) return;
    std::lock_guard<std::mutex> lk(impl_->mtx);
    if (impl_->elem_size == 0 || impl_->fn == nullptr) return;
    // result = identity
    std::memcpy(result_out, impl_->identity.data(), impl_->elem_size);
    // 逐个合并
    for (const auto& local : impl_->locals) {
        impl_->fn(result_out, local.data());
    }
}

std::size_t ReductionMerger::local_count() const noexcept {
    return impl_->locals.size();
}

} // namespace astro::compute::scheduler
