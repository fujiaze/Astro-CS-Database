// lib/acr/api/event.cpp — Event 类实现
// Event 持有 std::shared_ptr<detail::EventImpl>，EventImpl 完整定义在 runtime_internal.h。
// 本文件不依赖 tbb（tbb 完全封装在 runtime.cpp）。

#include <utility>

#include "astro/compute/acr.hpp"
#include "astro/compute/runtime_internal.h"

namespace astro::compute {

Event::Event() = default;

Event::~Event() = default;

Event::Event(Event&& other) noexcept
    : impl_(std::move(other.impl_)) {}

Event& Event::operator=(Event&& other) noexcept {
    if (this != &other) {
        impl_ = std::move(other.impl_);
    }
    return *this;
}

Event::Event(std::shared_ptr<detail::EventImpl> impl)
    : impl_(std::move(impl)) {}

void Event::wait() const {
    if (impl_) impl_->wait();
}

bool Event::ready() const noexcept {
    if (!impl_) return true;
    return impl_->ready();
}

void Event::cancel() {
    if (impl_) {
        // 先置 cancelled 标志，让正在执行的 kernel 能观察到并提前返回；
        impl_->cancelled.store(true, std::memory_order_release);
        // 只在 kernel 尚未进入终态时才 mark_cancelled。
        // 对已成功完成（Done）的 kernel 取消应保持 Ok 状态，
        // 对已 Failed/Cancelled 的也不覆盖已有终态。
        auto s = impl_->state.load(std::memory_order_acquire);
        if (s == detail::EventState::Pending || s == detail::EventState::Running) {
            impl_->mark_cancelled();
        }
    }
}

bool Event::cancelled() const noexcept {
    return impl_ && impl_->cancelled.load(std::memory_order_acquire);
}

StatusCode Event::status() const noexcept {
    if (!impl_) return StatusCode::Ok;
    const auto s = impl_->state.load(std::memory_order_acquire);
    switch (s) {
        case detail::EventState::Pending:
        case detail::EventState::Running:
            // 未完成视为非错误（Ok）；调用者可用 ready 判断是否就绪
            return StatusCode::Ok;
        case detail::EventState::Done:
            return StatusCode::Ok;
        case detail::EventState::Cancelled:
            return StatusCode::Cancelled;
        case detail::EventState::Failed:
            return impl_->error_code;
        default:
            return StatusCode::InternalError;
    }
}

detail::EventImpl* Event::impl() const noexcept {
    return impl_.get();
}

} // namespace astro::compute
