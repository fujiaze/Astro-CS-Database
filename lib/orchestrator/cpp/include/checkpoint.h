// ============================================================================
// checkpoint.h - JSON 检查点管理器 (断点续传)
// 功能: 每个管线阶段 (CALIBRATE/PLATESOLVE/PHOTOMETRIC/DRIZZLE) 完成后
// 将进度以 JSON 文件持久化到 <output_dir>/.checkpoint/<frame>.json
// 恢复时读取 JSON, 跳过已完成阶段, 从下一个阶段继续
// 用途: 编排器 (Orchestrator) 断点续传机制的基础组件
//
// 设计说明:
// - JSON 序列化/反序列化使用简单字符串处理, 不依赖外部 JSON 库
// - 原子写入: 先写 .tmp 临时文件, 再 rename 为目标文件 (Windows 用 MoveFileExA)
// - 文件名安全处理: 替换 \ / : * ? " < > | 等特殊字符为 _
// - 时间戳采用 ISO 8601 格式: YYYY-MM-DDTHH:MM:SS
// ============================================================================

#pragma once

#include <string>
#include <vector>
#include <map>

// ============================================================================
// 检查点阶段记录
// ============================================================================
struct CheckpointStage {
    std::string stage_name;     // 阶段名 (CALIBRATE/PLATESOLVE/PHOTOMETRIC/DRIZZLE)
    int stage_id;               // 阶段ID (0-4)
    double duration_sec;        // 耗时(秒)
    bool success;               // 是否成功
    std::string timestamp;      // 完成时间戳 (ISO 8601)
};

// ============================================================================
// 检查点数据
// ============================================================================
struct CheckpointData {
    std::string frame_name;                          // 帧名
    std::string fits_path;                           // FITS文件路径
    int current_stage_id;                            // 当前阶段ID
    std::vector<CheckpointStage> stages_completed;   // 已完成阶段列表
    std::map<std::string, double> timings;           // 阶段耗时映射
    std::string created_at;                          // 创建时间
    std::string updated_at;                          // 更新时间
    bool fully_completed;                            // 是否全部完成
};

// ============================================================================
// 检查点管理器
// ============================================================================
class CheckpointManager {
public:
    CheckpointManager();
    ~CheckpointManager();

    // 设置检查点目录
    void set_checkpoint_dir(const std::string& dir);

    // 保存检查点 (原子写入)
    bool save(const std::string& frame_name, const CheckpointData& data);

    // 加载检查点
    bool load(const std::string& frame_name, CheckpointData& data);

    // 检查检查点是否存在
    bool exists(const std::string& frame_name);

    // 删除检查点
    bool remove(const std::string& frame_name);

    // 列出所有检查点 (返回帧名列表)
    std::vector<std::string> list_all();

    // 清除所有检查点
    void clear_all();

    // 更新阶段完成状态 (便捷方法)
    bool update_stage(const std::string& frame_name, int stage_id,
                      const std::string& stage_name, double duration, bool success);

    // 检查阶段是否已完成
    bool is_stage_completed(const std::string& frame_name, int stage_id);

    // 获取恢复起点 (下一个待执行的阶段)
    // 返回: max(stage_id) + 1, 如无已完成阶段返回 0, 如 fully_completed 返回 -1
    int get_resume_stage(const std::string& frame_name);

    // 获取检查点目录路径
    std::string get_checkpoint_dir() const { return checkpoint_dir_; }

private:
    std::string checkpoint_dir_;

    // 文件路径生成
    std::string get_filepath(const std::string& frame_name);

    // JSON 序列化/反序列化 (简单实现, 不依赖外部库)
    std::string serialize(const CheckpointData& data);
    bool deserialize(const std::string& json, CheckpointData& data);

    // 原子写入 (临时文件 + rename)
    bool atomic_write(const std::string& path, const std::string& content);

    // 时间戳生成 (ISO 8601: YYYY-MM-DDTHH:MM:SS)
    std::string get_timestamp();

    // 文件名安全处理 (替换特殊字符, 去除路径前缀只保留文件名)
    std::string sanitize_frame_name(const std::string& name);
};
