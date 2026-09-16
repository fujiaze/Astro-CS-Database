// acr/runtime_internal.h — ACR runtime 内部头（不在公共 include 路径）
// 定义 EventImpl + EventState。tbb 封装在 runtime.cpp。
// acr.hpp 不 include 此文件；只有 runtime.cpp / event.cpp include。
#pragma once

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <string>

#include "astro/compute/acr.hpp"

namespace astro::compute::detail {

enum class EventState : int {
    Pending = 0, Running, Done, Cancelled, Failed,
};

class EventImpl {
public:
    std::atomic<EventState> state{EventState::Pending};
    std::atomic<bool> cancelled{false};
    StatusCode error_code{StatusCode::Ok};
    std::string error_msg;
    std::mutex mtx;
    std::condition_variable cv;

    void mark_done() { set_state(EventState::Done); }
    void mark_failed(StatusCode code, std::string msg) {
        std::lock_guard<std::mutex> lk(mtx);
        error_code = code;
        error_msg = std::move(msg);
        state.store(EventState::Failed, std::memory_order_release);
        cv.notify_all();
    }
    void mark_cancelled() { set_state(EventState::Cancelled); }
    void set_state(EventState s) {
        {
            std::lock_guard<std::mutex> lk(mtx);
            state.store(s, std::memory_order_release);
        }
        cv.notify_all();
    }
    void wait() {
        std::unique_lock<std::mutex> lk(mtx);
        cv.wait(lk, [&] {
            auto s = state.load(std::memory_order_acquire);
            return s == EventState::Done || s == EventState::Cancelled || s == EventState::Failed;
        });
    }
    bool ready() const noexcept {
        auto s = state.load(std::memory_order_acquire);
        return s == EventState::Done || s == EventState::Cancelled || s == EventState::Failed;
    }
};

} // namespace astro::compute::detail
