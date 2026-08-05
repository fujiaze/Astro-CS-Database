// lib/acr/scheduler/residency_manager.hpp — 数据驻留状态管理
//
// 08 号计划 §6 / 06 号规范：
//   - Buffer 跟踪 Host/Device/Both valid 与 dirty 状态；
//   - 相同输入不为每个 GPU 块重复整帧上传（只在上传一次后以 view 复用）；
//   - 连续 GPU 算子中间结果保持 resident；
//   - 只有 CPU 或外部模块需要结果时才 D2H；
//   - 报告实际传输字节与驻留复用次数。
#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

namespace astro::compute::scheduler {

// ===== 驻留状态（06 号规范 §1）=====
enum class ResidencyState : std::uint8_t {
    HostValid = 0,     // 仅 host 有效
    DeviceValid = 1,   // 仅 device 有效
    BothValid = 2,     // host 与 device 均有效
    HostDirty = 3,     // host 修改，device 需重新上传
    DeviceDirty = 4,   // device 修改，host 需下载
};

const char* residency_state_str(ResidencyState s) noexcept;

// ===== 单 Buffer 驻留记录 =====
struct BufferResidency {
    ResidencyState state{ResidencyState::HostValid};
    std::string device_id{"cuda:0"};   // 最近驻留设备
    std::size_t bytes{0};
    std::uint64_t last_upload_ns{0};   // 最近上传时间戳（诊断）
    std::uint64_t upload_count{0};     // 实际上传次数
    std::uint64_t download_count{0};   // 实际下载次数
};

// ===== ResidencyManager =====
// 线程安全。按 buffer 键（如 buffer 指针地址或用户提供的字符串键）跟踪。
class ResidencyManager {
public:
    ResidencyManager();
    ~ResidencyManager();

    // 注册/更新 buffer 大小
    void register_buffer(const std::string& key, std::size_t bytes);

    // 标记 host 修改（输入更新 → device 副本失效）
    void mark_host_dirty(const std::string& key);

    // 标记 device 修改（GPU 算子写入 → host 副本失效）
    void mark_device_dirty(const std::string& key);

    // 上传后：device 有效（host 未变 → BothValid；host 已变 → 先上传再标 Both）
    void mark_uploaded(const std::string& key);

    // 下载后：host 有效
    void mark_downloaded(const std::string& key);

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
