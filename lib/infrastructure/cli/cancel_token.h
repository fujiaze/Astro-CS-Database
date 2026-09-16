// astrocs 协作取消令牌 (API-001 acs_cancel 的 CLI 侧宿主实现) — CLI-002
// POSIX: SIGINT/SIGTERM → 原子置位; Windows: SetConsoleCtrlHandler → 原子置位。
// 语义: 单向置位, 内核在 ALG 5c 冻结的安全点轮询; 取消后不得产生"看似完整"的产物。
#pragma once
#include <atomic>

#ifdef _WIN32
#include <windows.h>
#else
#include <csignal>
#endif

namespace astrocs {

inline std::atomic<bool>& cancel_flag() {
    static std::atomic<bool> flag{false};
    return flag;
}

inline bool is_cancelled() { return cancel_flag().load(std::memory_order_relaxed); }

#ifdef _WIN32
inline BOOL WINAPI console_ctrl_handler(DWORD type) {
    (void)type;
    cancel_flag().store(true, std::memory_order_relaxed);
    return TRUE;  // 进程自行收尾(退出码 9)
}
inline void install_cancel_handlers() {
    SetConsoleCtrlHandler(console_ctrl_handler, TRUE);
}
#else
inline void posix_signal_handler(int) {
    cancel_flag().store(true, std::memory_order_relaxed);
}
inline void install_cancel_handlers() {
    std::signal(SIGINT, posix_signal_handler);
    std::signal(SIGTERM, posix_signal_handler);
}
#endif

}  // namespace astrocs
