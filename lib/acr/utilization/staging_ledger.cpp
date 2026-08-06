// lib/acr/utilization/staging_ledger.cpp — StagingLedger 实现
#include "staging_ledger.hpp"

#include <mutex>
#include <sstream>

namespace astro::compute::utilization {

struct StagingLedger::Impl {
    mutable std::mutex mtx;
    std::size_t limit{0};
    std::size_t used{0};
};

StagingLedger::StagingLedger() : impl_(std::make_unique<Impl>()) {}
StagingLedger::~StagingLedger() = default;

void StagingLedger::configure(std::size_t limit_bytes) noexcept {
    std::lock_guard<std::mutex> lk(impl_->mtx);
    impl_->limit = limit_bytes;
    if (impl_->used > impl_->limit) impl_->used = impl_->limit;
}

std::size_t StagingLedger::limit() const noexcept {
    std::lock_guard<std::mutex> lk(impl_->mtx);
    return impl_->limit;
}

std::size_t StagingLedger::used() const noexcept {
    std::lock_guard<std::mutex> lk(impl_->mtx);
    return impl_->used;
}

bool StagingLedger::reserve(std::size_t bytes) noexcept {
    if (bytes == 0) return true;
    std::lock_guard<std::mutex> lk(impl_->mtx);
    if (impl_->limit > 0 && impl_->used + bytes > impl_->limit) {
        return false;
    }
    impl_->used += bytes;
    return true;
}

void StagingLedger::release(std::size_t bytes) noexcept {
    std::lock_guard<std::mutex> lk(impl_->mtx);
    if (bytes >= impl_->used) {
        impl_->used = 0;
    } else {
        impl_->used -= bytes;
    }
}

std::string StagingLedger::status_json() const {
    std::lock_guard<std::mutex> lk(impl_->mtx);
    std::ostringstream os;
    os << "{\"limit_bytes\":" << impl_->limit
       << ",\"used_bytes\":" << impl_->used << "}";
    return os.str();
}

} // namespace astro::compute::utilization
