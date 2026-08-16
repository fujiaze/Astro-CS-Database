// lib/acr/scheduler/residency_manager.hpp — 数据驻留状态管理
//
// 08 §6 / 06 号规范：
// - Buffer 跟踪 Host/Device/Both valid 与 dirty 状态；
// - 相同输入不为每个 GPU 块重复整帧上传（只在上传一次后以 view 复用）；
// - 连续 GPU 算子中间结果保持 resident；
// - 只有 CPU 或外部模块需要结果时才 D2H；
// - 报告实际传输字节与驻留复用次数。
#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

namespace astro::compute::scheduler {

// ===== 驻留状态=====
enum class ResidencyState : std::uint8_t {
    HostValid = 0,     // 仅 host 有效
    DeviceValid = 1,   // 仅 device 有效
    BothValid = 2,     // host 与 device 均有效
    HostDirty = 3,     // host 修改，device 需重新上传
    DeviceDirty = 4,   // device 修改，host 需下载
};

const char* residency_state_str(ResidencyState s) noexcept;

// ===== Buffer 访问模式=====
enum class BufferAccess : std::uint8_t {
    Read = 0,        // 只读输入（可跨块复用）
    Write = 1,       // 只写输出
    ReadWrite = 2,   // 读改写（accumulate）
};

const char* buffer_access_str(BufferAccess a) noexcept;

// ===== 单 Buffer 驻留记录 =====
struct BufferResidency {
    ResidencyState state{ResidencyState::HostValid};
    std::string device_id{"cuda:0"};   // 最近驻留设备
    std::size_t bytes{0};
    BufferAccess access{BufferAccess::Read};
    std::uint64_t generation{0};       // host 修改代数（复用失效判断）
    std::uint64_t last_upload_ns{0};   // 最近上传时间戳（诊断）
    std::uint64_t upload_count{0};     // 实际上传次数
    std::uint64_t download_count{0};   // 实际下载次数
    bool device_allocated{false};      // 后端是否保留 device allocation
};

// ===== ResidencyManager =====
// 线程安全。按 buffer 键（如 buffer 指针地址或用户提供的字符串键）跟踪。
class ResidencyManager {
public:
    ResidencyManager();
    ~ResidencyManager();

    // 注册/更新 buffer（真实字节数与访问模式）
    void register_buffer(const std::string& key, std::size_t bytes,
                         BufferAccess access = BufferAccess::Read);

    // 注册/更新 buffer 并同步外部 binding generation（04 号契约 §2）：
    // - 新 buffer：记录 generation；
    // - 同 key 但 generation 高于已记录：host 内容已更新 → 设备副本失效
    // （DeviceValid/BothValid → HostDirty/HostValid），下次 GPU 执行必须重传；
    // - generation 未变：保持现有驻留状态（复用 device 副本）。
    void register_or_update(const std::string& key, std::size_t bytes,
                            BufferAccess access, std::uint64_t generation);

    // 查询已记录的 host generation（供诊断/测试）
    std::uint64_t recorded_generation(const std::string& key) const;

    // 标记 host 修改（输入更新 → device 副本失效）
    void mark_host_dirty(const std::string& key);

    // 标记 device 修改（GPU 算子写入 → host 副本失效）
    void mark_device_dirty(const std::string& key);

    // 上传后：device 有效（host 未变 → BothValid；host 已变 → 先上传再标 Both）
    void mark_uploaded(const std::string& key);

    // 下载后：host 有效
    void mark_downloaded(const std::string& key);

    // 后端确认保留 device allocation（真实 buffer 缓存）
    void mark_device_allocated(const std::string& key);
    bool is_device_allocated(const std::string& key) const;

    // host 修改代数（供 generation 复用失效判断）
    std::uint64_t generation(const std::string& key) const;

    // 查询：该 buffer 是否已在指定设备显存（可复用，无需再上传）
    bool is_device_valid(const std::string& key,
                         const std::string& device_id) const;

    // 查询：是否需要上传（host 有效但 device 无效/过期）
    bool needs_upload(const std::string& key,
                      const std::string& device_id) const;

    // 查询：是否需要下载（device dirty 且 host 需要结果）
    bool needs_download(const std::string& key) const;

    // 查询当前状态
    ResidencyState state(const std::string& key) const;

    // 报告：该 buffer 的实际传输次数与字节
    std::uint64_t upload_count(const std::string& key) const;
    std::uint64_t download_count(const std::string& key) const;

    // 全局统计
    std::uint64_t total_uploads() const noexcept;
    std::uint64_t total_downloads() const noexcept;

    // 状态 JSON（诊断/报告）
    std::string status_json() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace astro::compute::scheduler
