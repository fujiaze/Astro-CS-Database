// lib/acr/scheduler/residency_manager.cpp — ResidencyManager 实现
#include "residency_manager.hpp"

#include <chrono>
#include <mutex>
#include <sstream>
#include <string>
#include <unordered_map>

namespace astro::compute::scheduler {

const char* residency_state_str(ResidencyState s) noexcept {
    switch (s) {
        case ResidencyState::HostValid:   return "host";
        case ResidencyState::DeviceValid: return "device";
        case ResidencyState::BothValid:   return "both";
        case ResidencyState::HostDirty:   return "host_dirty";
        case ResidencyState::DeviceDirty: return "device_dirty";
    }
    return "unknown";
}

const char* buffer_access_str(BufferAccess a) noexcept {
    switch (a) {
        case BufferAccess::Read:      return "read";
        case BufferAccess::Write:     return "write";
        case BufferAccess::ReadWrite: return "read_write";
    }
    return "unknown";
}

struct ResidencyManager::Impl {
    mutable std::mutex mtx;
    std::unordered_map<std::string, BufferResidency> buffers;
    std::uint64_t total_uploads{0};
    std::uint64_t total_downloads{0};
};

ResidencyManager::ResidencyManager()
    : impl_(std::make_unique<Impl>()) {}
ResidencyManager::~ResidencyManager() = default;

void ResidencyManager::register_buffer(const std::string& key,
                                       std::size_t bytes,
                                       BufferAccess access) {
    std::lock_guard<std::mutex> lk(impl_->mtx);
    auto& b = impl_->buffers[key];
    b.bytes = bytes;
    b.access = access;
    if (b.state == ResidencyState::DeviceValid ||
        b.state == ResidencyState::DeviceDirty) {
        // 保留现有驻留状态
    }
}

void ResidencyManager::mark_host_dirty(const std::string& key) {
    std::lock_guard<std::mutex> lk(impl_->mtx);
    auto& b = impl_->buffers[key];
    if (b.state == ResidencyState::DeviceValid ||
        b.state == ResidencyState::DeviceDirty) {
        b.state = ResidencyState::HostDirty;  // host 修改，device 需重新上传
    } else {
        b.state = ResidencyState::HostValid;
    }
    ++b.generation;  // host 修改代数递增（复用失效）
}

void ResidencyManager::mark_device_dirty(const std::string& key) {
    std::lock_guard<std::mutex> lk(impl_->mtx);
    auto& b = impl_->buffers[key];
    b.state = ResidencyState::DeviceDirty;
}

void ResidencyManager::mark_uploaded(const std::string& key) {
    std::lock_guard<std::mutex> lk(impl_->mtx);
    auto& b = impl_->buffers[key];
    b.state = (b.state == ResidencyState::DeviceValid ||
               b.state == ResidencyState::DeviceDirty)
        ? ResidencyState::DeviceValid : ResidencyState::BothValid;
    ++b.upload_count;
    ++impl_->total_uploads;
    b.last_upload_ns = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count());
}

void ResidencyManager::mark_downloaded(const std::string& key) {
    std::lock_guard<std::mutex> lk(impl_->mtx);
    auto& b = impl_->buffers[key];
    b.state = ResidencyState::HostValid;
    ++b.download_count;
    ++impl_->total_downloads;
}

void ResidencyManager::mark_device_allocated(const std::string& key) {
    std::lock_guard<std::mutex> lk(impl_->mtx);
    impl_->buffers[key].device_allocated = true;
}

bool ResidencyManager::is_device_allocated(const std::string& key) const {
    std::lock_guard<std::mutex> lk(impl_->mtx);
    auto it = impl_->buffers.find(key);
    return it != impl_->buffers.end() && it->second.device_allocated;
}

std::uint64_t ResidencyManager::generation(const std::string& key) const {
    std::lock_guard<std::mutex> lk(impl_->mtx);
    auto it = impl_->buffers.find(key);
    return (it == impl_->buffers.end()) ? 0 : it->second.generation;
}

bool ResidencyManager::is_device_valid(const std::string& key,
                                       const std::string& device_id) const {
    std::lock_guard<std::mutex> lk(impl_->mtx);
    auto it = impl_->buffers.find(key);
    if (it == impl_->buffers.end()) return false;
    const auto& b = it->second;
    if (b.device_id != device_id) return false;
    return (b.state == ResidencyState::DeviceValid ||
            b.state == ResidencyState::BothValid);
}

bool ResidencyManager::needs_upload(const std::string& key,
                                    const std::string& device_id) const {
    std::lock_guard<std::mutex> lk(impl_->mtx);
    auto it = impl_->buffers.find(key);
    if (it == impl_->buffers.end()) return true;
    const auto& b = it->second;
    if (b.device_id != device_id) return true;
    return !(b.state == ResidencyState::DeviceValid ||
             b.state == ResidencyState::BothValid);
}

bool ResidencyManager::needs_download(const std::string& key) const {
    std::lock_guard<std::mutex> lk(impl_->mtx);
    auto it = impl_->buffers.find(key);
    if (it == impl_->buffers.end()) return true;
    const auto& b = it->second;
    return b.state == ResidencyState::DeviceDirty;
}

ResidencyState ResidencyManager::state(const std::string& key) const {
    std::lock_guard<std::mutex> lk(impl_->mtx);
    auto it = impl_->buffers.find(key);
    return (it == impl_->buffers.end())
        ? ResidencyState::HostValid : it->second.state;
}

std::uint64_t ResidencyManager::upload_count(const std::string& key) const {
    std::lock_guard<std::mutex> lk(impl_->mtx);
    auto it = impl_->buffers.find(key);
    return (it == impl_->buffers.end()) ? 0 : it->second.upload_count;
}

std::uint64_t ResidencyManager::download_count(const std::string& key) const {
    std::lock_guard<std::mutex> lk(impl_->mtx);
    auto it = impl_->buffers.find(key);
    return (it == impl_->buffers.end()) ? 0 : it->second.download_count;
}

std::uint64_t ResidencyManager::total_uploads() const noexcept {
    return impl_->total_uploads;
}

std::uint64_t ResidencyManager::total_downloads() const noexcept {
    return impl_->total_downloads;
}

std::string ResidencyManager::status_json() const {
    std::lock_guard<std::mutex> lk(impl_->mtx);
    std::ostringstream os;
    os << "{";
    os << "\"buffers\":{";
    bool first = true;
    for (const auto& kv : impl_->buffers) {
        if (!first) os << ",";
        first = false;
        os << "\"" << kv.first << "\":{"
           << "\"state\":\"" << residency_state_str(kv.second.state) << "\""
           << ",\"device\":\"" << kv.second.device_id << "\""
           << ",\"bytes\":" << kv.second.bytes
           << ",\"uploads\":" << kv.second.upload_count
           << ",\"downloads\":" << kv.second.download_count
           << "}";
    }
    os << "}";
    os << ",\"total_uploads\":" << impl_->total_uploads;
    os << ",\"total_downloads\":" << impl_->total_downloads;
    os << "}";
    return os.str();
}

} // namespace astro::compute::scheduler
