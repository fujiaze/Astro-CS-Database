// ============================================================================
// checkpoint.cpp - JSON 检查点管理器实现
// 功能: 检查点的保存/加载/查询/删除, 支持原子写入和断点续传
//
// 实现要点:
//   - JSON 序列化/反序列化使用简单字符串处理, 不依赖外部 JSON 库
//   - 原子写入: 写 .tmp 临时文件, 再 rename (Windows 用 MoveFileExA 替换)
//   - 文件名安全处理: 替换 \ / : * ? " < > | 等特殊字符为 _
//   - 时间戳采用 ISO 8601: YYYY-MM-DDTHH:MM:SS
// ============================================================================

#include "checkpoint.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <cstring>
#include <ctime>
#include <algorithm>
#include <cctype>
#include <chrono>
#include <filesystem>
#include <cstdio>

#ifdef _WIN32
#include <windows.h>
#endif

namespace fs = std::filesystem;

// ============================================================================
// 辅助: JSON 字符串转义 (序列化时使用)
// ============================================================================
static std::string json_escape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                    out += buf;
                } else {
                    out += c;
                }
                break;
        }
    }
    return out;
}

// ============================================================================
// 辅助: JSON 字符串反转义 (反序列化时使用)
// 将 \" \\ \n \r \t \uXXXX 还原为实际字符
// ============================================================================
static std::string json_unescape(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\\' && i + 1 < s.size()) {
            char next = s[i + 1];
            switch (next) {
                case '"':  out += '"';  ++i; break;
                case '\\': out += '\\'; ++i; break;
                case '/':  out += '/';  ++i; break;
                case 'n':  out += '\n'; ++i; break;
                case 'r':  out += '\r'; ++i; break;
                case 't':  out += '\t'; ++i; break;
                case 'b':  out += '\b'; ++i; break;
                case 'f':  out += '\f'; ++i; break;
                case 'u': {
                    // \uXXXX (4 位十六进制)
                    if (i + 5 < s.size()) {
                        unsigned int code = 0;
                        std::sscanf(s.c_str() + i + 2, "%4x", &code);
                        // 简化处理: 仅支持 ASCII 范围 (< 0x80)
                        if (code < 0x80) {
                            out += static_cast<char>(code);
                        }
                        i += 5;
                    }
                    break;
                }
                default:
                    out += s[i];
                    break;
            }
        } else {
            out += s[i];
        }
    }
    return out;
}

// ============================================================================
// 辅助: 在 JSON 文本中提取字符串字段值
// 查找 "key": "value" 模式, 返回 value (已反转义)
// 如未找到返回空字符串
// ============================================================================
static std::string json_get_string(const std::string& json, const std::string& key) {
    // 查找 "key"
    std::string pattern = "\"" + key + "\"";
    size_t kpos = json.find(pattern);
    if (kpos == std::string::npos) return "";

    // 跳过 "key" 后的空白和冒号
    size_t p = kpos + pattern.size();
    while (p < json.size() && (json[p] == ' ' || json[p] == '\t' ||
                                json[p] == '\n' || json[p] == '\r')) ++p;
    if (p >= json.size() || json[p] != ':') return "";
    ++p;
    while (p < json.size() && (json[p] == ' ' || json[p] == '\t' ||
                                json[p] == '\n' || json[p] == '\r')) ++p;
    if (p >= json.size() || json[p] != '"') return "";

    // 提取引号内的字符串 (处理转义)
    ++p;  // 跳过开头引号
    std::string raw;
    while (p < json.size() && json[p] != '"') {
        if (json[p] == '\\' && p + 1 < json.size()) {
            raw += json[p];
            raw += json[p + 1];
            p += 2;
        } else {
            raw += json[p];
            ++p;
        }
    }
    return json_unescape(raw);
}

// ============================================================================
// 辅助: 在 JSON 文本中提取整数字段值
// ============================================================================
static int json_get_int(const std::string& json, const std::string& key, int default_val = 0) {
    std::string pattern = "\"" + key + "\"";
    size_t kpos = json.find(pattern);
    if (kpos == std::string::npos) return default_val;

    size_t p = kpos + pattern.size();
    while (p < json.size() && (json[p] == ' ' || json[p] == '\t' ||
                                json[p] == '\n' || json[p] == '\r')) ++p;
    if (p >= json.size() || json[p] != ':') return default_val;
    ++p;
    while (p < json.size() && (json[p] == ' ' || json[p] == '\t' ||
                                json[p] == '\n' || json[p] == '\r')) ++p;

    // 解析整数 (支持负号)
    std::string num;
    if (p < json.size() && (json[p] == '-' || json[p] == '+')) {
        num += json[p];
        ++p;
    }
    while (p < json.size() && std::isdigit(static_cast<unsigned char>(json[p]))) {
        num += json[p];
        ++p;
    }
    if (num.empty() || num == "-" || num == "+") return default_val;
    return std::atoi(num.c_str());
}

// ============================================================================
// 辅助: 在 JSON 文本中提取浮点字段值
// ============================================================================
static double json_get_double(const std::string& json, const std::string& key, double default_val = 0.0) {
    std::string pattern = "\"" + key + "\"";
    size_t kpos = json.find(pattern);
    if (kpos == std::string::npos) return default_val;

    size_t p = kpos + pattern.size();
    while (p < json.size() && (json[p] == ' ' || json[p] == '\t' ||
                                json[p] == '\n' || json[p] == '\r')) ++p;
    if (p >= json.size() || json[p] != ':') return default_val;
    ++p;
    while (p < json.size() && (json[p] == ' ' || json[p] == '\t' ||
                                json[p] == '\n' || json[p] == '\r')) ++p;

    // 解析浮点 (支持 - + . e E 数字)
    std::string num;
    while (p < json.size()) {
        char c = json[p];
        if (std::isdigit(static_cast<unsigned char>(c)) || c == '-' || c == '+' ||
            c == '.' || c == 'e' || c == 'E') {
            num += c;
            ++p;
        } else {
            break;
        }
    }
    if (num.empty()) return default_val;
    return std::atof(num.c_str());
}

// ============================================================================
// 辅助: 在 JSON 文本中提取布尔字段值
// ============================================================================
static bool json_get_bool(const std::string& json, const std::string& key, bool default_val = false) {
    std::string pattern = "\"" + key + "\"";
    size_t kpos = json.find(pattern);
    if (kpos == std::string::npos) return default_val;

    size_t p = kpos + pattern.size();
    while (p < json.size() && (json[p] == ' ' || json[p] == '\t' ||
                                json[p] == '\n' || json[p] == '\r')) ++p;
    if (p >= json.size() || json[p] != ':') return default_val;
    ++p;
    while (p < json.size() && (json[p] == ' ' || json[p] == '\t' ||
                                json[p] == '\n' || json[p] == '\r')) ++p;

    if (p + 4 <= json.size() && json.compare(p, 4, "true") == 0) return true;
    if (p + 5 <= json.size() && json.compare(p, 5, "false") == 0) return false;
    return default_val;
}

// ============================================================================
// 辅助: 查找匹配的方括号/大括号结束位置
// 从 start 位置的 '[' 或 '{' 开始, 找到对应的 ']' 或 '}'
// 返回结束位置 (闭括号的下标), 如未找到返回 std::string::npos
// ============================================================================
static size_t find_matching_bracket(const std::string& s, size_t start) {
    if (start >= s.size()) return std::string::npos;
    char open = s[start];
    char close = (open == '[') ? ']' : '}';
    if (close == 0) return std::string::npos;

    int depth = 1;
    size_t p = start + 1;
    bool in_string = false;
    while (p < s.size()) {
        char c = s[p];
        if (in_string) {
            if (c == '\\' && p + 1 < s.size()) {
                p += 2;
                continue;
            }
            if (c == '"') in_string = false;
        } else {
            if (c == '"') {
                in_string = true;
            } else if (c == open) {
                ++depth;
            } else if (c == close) {
                --depth;
                if (depth == 0) return p;
            }
        }
        ++p;
    }
    return std::string::npos;
}

// ============================================================================
// 构造 / 析构
// ============================================================================
CheckpointManager::CheckpointManager() {
    // 默认检查点目录 (相对路径, 由 Orchestrator 在 set_output_dir 后覆盖)
    checkpoint_dir_ = ".checkpoint";
}

CheckpointManager::~CheckpointManager() {
}

// ============================================================================
// set_checkpoint_dir - 设置检查点目录
// ============================================================================
void CheckpointManager::set_checkpoint_dir(const std::string& dir) {
    checkpoint_dir_ = dir;
    // 确保目录存在
    std::error_code ec;
    fs::create_directories(checkpoint_dir_, ec);
    // 忽略错误: 目录可能已存在
    if (ec) {
        std::cerr << "[checkpoint] [警告] 创建检查点目录失败: " << checkpoint_dir_
                  << " (" << ec.message() << ")" << std::endl;
    }
}

// ============================================================================
// get_filepath - 文件路径生成
// 返回: checkpoint_dir_/sanitize_frame_name(frame_name) + ".json"
// ============================================================================
std::string CheckpointManager::get_filepath(const std::string& frame_name) {
    std::string fname = sanitize_frame_name(frame_name) + ".json";
    return (fs::path(checkpoint_dir_) / fname).string();
}

// ============================================================================
// sanitize_frame_name - 文件名安全处理
// 1. 去除路径前缀, 只保留文件名部分 (取最后一个 \ 或 / 之后的内容)
// 2. 替换特殊字符 \ / : * ? " < > | 为 _
// ============================================================================
std::string CheckpointManager::sanitize_frame_name(const std::string& name) {
    // 步骤 1: 去除路径前缀, 只保留文件名部分
    std::string fname = name;
    size_t last_sep = std::string::npos;
    for (size_t i = 0; i < fname.size(); ++i) {
        if (fname[i] == '\\' || fname[i] == '/') {
            last_sep = i;
        }
    }
    if (last_sep != std::string::npos) {
        fname = fname.substr(last_sep + 1);
    }

    // 步骤 2: 替换特殊字符
    std::string out;
    out.reserve(fname.size());
    for (char c : fname) {
        switch (c) {
            case '\\': case '/': case ':': case '*': case '?':
            case '"':  case '<': case '>': case '|':
                out += '_';
                break;
            default:
                // 控制字符也替换为 _
                if (static_cast<unsigned char>(c) < 0x20) {
                    out += '_';
                } else {
                    out += c;
                }
                break;
        }
    }
    return out;
}

// ============================================================================
// serialize - 序列化 CheckpointData 为 JSON 字符串
// 不依赖外部 JSON 库, 手动构建
// ============================================================================
std::string CheckpointManager::serialize(const CheckpointData& data) {
    std::ostringstream oss;
    oss << std::fixed;
    oss.precision(6);

    oss << "{\n";
    oss << "    \"frame_name\": \"" << json_escape(data.frame_name) << "\",\n";
    oss << "    \"fits_path\": \"" << json_escape(data.fits_path) << "\",\n";
    oss << "    \"current_stage_id\": " << data.current_stage_id << ",\n";

    // stages_completed 数组
    oss << "    \"stages_completed\": [\n";
    for (size_t i = 0; i < data.stages_completed.size(); ++i) {
        const CheckpointStage& st = data.stages_completed[i];
        oss << "        {";
        oss << "\"stage_name\": \"" << json_escape(st.stage_name) << "\", ";
        oss << "\"stage_id\": " << st.stage_id << ", ";
        oss << "\"duration_sec\": " << st.duration_sec << ", ";
        oss << "\"success\": " << (st.success ? "true" : "false") << ", ";
        oss << "\"timestamp\": \"" << json_escape(st.timestamp) << "\"";
        oss << "}";
        if (i + 1 < data.stages_completed.size()) oss << ",";
        oss << "\n";
    }
    oss << "    ],\n";

    // timings 对象
    oss << "    \"timings\": {";
    bool first = true;
    for (const auto& kv : data.timings) {
        if (!first) oss << ", ";
        first = false;
        oss << "\"" << json_escape(kv.first) << "\": " << kv.second;
    }
    oss << "},\n";

    oss << "    \"created_at\": \"" << json_escape(data.created_at) << "\",\n";
    oss << "    \"updated_at\": \"" << json_escape(data.updated_at) << "\",\n";
    oss << "    \"fully_completed\": " << (data.fully_completed ? "true" : "false") << "\n";
    oss << "}\n";

    return oss.str();
}

// ============================================================================
// deserialize - 反序列化 JSON 字符串到 CheckpointData
// 简单解析: 使用字符串查找 + 截取
// ============================================================================
bool CheckpointManager::deserialize(const std::string& json, CheckpointData& data) {
    // 基本字段
    data.frame_name = json_get_string(json, "frame_name");
    data.fits_path = json_get_string(json, "fits_path");
    data.current_stage_id = json_get_int(json, "current_stage_id", 0);
    data.created_at = json_get_string(json, "created_at");
    data.updated_at = json_get_string(json, "updated_at");
    data.fully_completed = json_get_bool(json, "fully_completed", false);

    // 解析 stages_completed 数组
    data.stages_completed.clear();
    {
        std::string key = "\"stages_completed\"";
        size_t kpos = json.find(key);
        if (kpos != std::string::npos) {
            // 跳过 key, 找到 [
            size_t p = kpos + key.size();
            while (p < json.size() && json[p] != '[') ++p;
            if (p < json.size() && json[p] == '[') {
                size_t arr_end = find_matching_bracket(json, p);
                if (arr_end != std::string::npos) {
                    // 遍历数组中的每个对象
                    size_t cur = p + 1;
                    while (cur < arr_end) {
                        // 找到下一个 '{'
                        while (cur < arr_end && json[cur] != '{') ++cur;
                        if (cur >= arr_end) break;
                        size_t obj_end = find_matching_bracket(json, cur);
                        if (obj_end == std::string::npos || obj_end > arr_end) break;

                        // 提取对象子串
                        std::string obj = json.substr(cur, obj_end - cur + 1);
                        CheckpointStage st;
                        st.stage_name = json_get_string(obj, "stage_name");
                        st.stage_id = json_get_int(obj, "stage_id", 0);
                        st.duration_sec = json_get_double(obj, "duration_sec", 0.0);
                        st.success = json_get_bool(obj, "success", false);
                        st.timestamp = json_get_string(obj, "timestamp");
                        data.stages_completed.push_back(st);

                        cur = obj_end + 1;
                    }
                }
            }
        }
    }

    // 解析 timings 对象
    data.timings.clear();
    {
        std::string key = "\"timings\"";
        size_t kpos = json.find(key);
        if (kpos != std::string::npos) {
            size_t p = kpos + key.size();
            while (p < json.size() && json[p] != '{') ++p;
            if (p < json.size() && json[p] == '{') {
                size_t obj_end = find_matching_bracket(json, p);
                if (obj_end != std::string::npos) {
                    // 遍历对象中的每个 "key": value 对
                    size_t cur = p + 1;
                    while (cur < obj_end) {
                        // 跳过空白和逗号
                        while (cur < obj_end &&
                               (json[cur] == ' ' || json[cur] == '\t' ||
                                json[cur] == '\n' || json[cur] == ',')) ++cur;
                        if (cur >= obj_end || json[cur] != '"') break;

                        // 提取 key
                        ++cur;  // 跳过开头引号
                        std::string k;
                        while (cur < obj_end && json[cur] != '"') {
                            if (json[cur] == '\\' && cur + 1 < obj_end) {
                                k += json[cur];
                                k += json[cur + 1];
                                cur += 2;
                            } else {
                                k += json[cur];
                                ++cur;
                            }
                        }
                        if (cur >= obj_end) break;
                        ++cur;  // 跳过结尾引号

                        // 跳过空白和冒号
                        while (cur < obj_end &&
                               (json[cur] == ' ' || json[cur] == '\t' ||
                                json[cur] == '\n')) ++cur;
                        if (cur >= obj_end || json[cur] != ':') break;
                        ++cur;
                        while (cur < obj_end &&
                               (json[cur] == ' ' || json[cur] == '\t' ||
                                json[cur] == '\n')) ++cur;

                        // 提取数值
                        std::string num;
                        while (cur < obj_end) {
                            char c = json[cur];
                            if (std::isdigit(static_cast<unsigned char>(c)) ||
                                c == '-' || c == '+' || c == '.' ||
                                c == 'e' || c == 'E') {
                                num += c;
                                ++cur;
                            } else {
                                break;
                            }
                        }
                        if (!num.empty()) {
                            std::string key_str = json_unescape(k);
                            data.timings[key_str] = std::atof(num.c_str());
                        }
                    }
                }
            }
        }
    }

    return true;
}

// ============================================================================
// atomic_write - 原子写入
// 1. 写入临时文件 path + ".tmp"
// 2. 使用 std::rename 重命名 (跨平台)
// 3. Windows 下使用 MoveFileExA 替换 (MOVEFILE_REPLACE_EXISTING)
// ============================================================================
bool CheckpointManager::atomic_write(const std::string& path, const std::string& content) {
    std::string tmp_path = path + ".tmp";

    // 步骤 1: 写入临时文件 (二进制模式, 避免平台换行符转换)
    std::ofstream ofs(tmp_path, std::ios::binary | std::ios::trunc);
    if (!ofs.is_open()) {
        std::cerr << "[checkpoint] [错误] 无法创建临时文件: " << tmp_path << std::endl;
        return false;
    }
    ofs.write(content.data(), static_cast<std::streamsize>(content.size()));
    ofs.flush();
    ofs.close();

    // 步骤 2: rename 临时文件到目标文件
    // Windows 下 std::rename 在目标文件已存在时会失败, 使用 MoveFileExA 替换
#ifdef _WIN32
    BOOL ok = MoveFileExA(tmp_path.c_str(), path.c_str(),
                           MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
    if (!ok) {
        DWORD err = GetLastError();
        std::cerr << "[checkpoint] [错误] MoveFileExA 失败: " << tmp_path << " -> "
                  << path << " (错误码=" << err << ")" << std::endl;
        // 删除残留的临时文件
        std::remove(tmp_path.c_str());
        return false;
    }
#else
    if (std::rename(tmp_path.c_str(), path.c_str()) != 0) {
        std::cerr << "[checkpoint] [错误] rename 失败: " << tmp_path << " -> "
                  << path << std::endl;
        std::remove(tmp_path.c_str());
        return false;
    }
#endif
    return true;
}

// ============================================================================
// save - 保存检查点 (原子写入)
// 流程: 更新 updated_at → serialize → atomic_write
// ============================================================================
bool CheckpointManager::save(const std::string& frame_name, const CheckpointData& data) {
    // 确保目录存在
    std::error_code ec;
    fs::create_directories(checkpoint_dir_, ec);

    // 更新 updated_at 时间戳 (const_cast 因为接口签名是 const, 此处是合理修改)
    CheckpointData& mutable_data = const_cast<CheckpointData&>(data);
    mutable_data.updated_at = get_timestamp();
    if (mutable_data.created_at.empty()) {
        mutable_data.created_at = mutable_data.updated_at;
    }

    std::string json = serialize(mutable_data);
    std::string path = get_filepath(frame_name);

    if (!atomic_write(path, json)) {
        std::cerr << "[checkpoint] [错误] 保存检查点失败: " << frame_name << std::endl;
        return false;
    }
    return true;
}

// ============================================================================
// load - 加载检查点
// ============================================================================
bool CheckpointManager::load(const std::string& frame_name, CheckpointData& data) {
    std::string path = get_filepath(frame_name);
    if (!fs::exists(path)) {
        return false;
    }

    std::ifstream ifs(path, std::ios::binary);
    if (!ifs.is_open()) {
        std::cerr << "[checkpoint] [错误] 无法打开检查点文件: " << path << std::endl;
        return false;
    }

    std::stringstream ss;
    ss << ifs.rdbuf();
    ifs.close();

    std::string json = ss.str();
    return deserialize(json, data);
}

// ============================================================================
// exists - 检查检查点是否存在
// ============================================================================
bool CheckpointManager::exists(const std::string& frame_name) {
    std::string path = get_filepath(frame_name);
    std::ifstream ifs(path);
    bool ok = ifs.good();
    ifs.close();
    return ok;
}

// ============================================================================
// remove - 删除检查点
// ============================================================================
bool CheckpointManager::remove(const std::string& frame_name) {
    std::string path = get_filepath(frame_name);
    if (!fs::exists(path)) {
        return false;
    }
    if (std::remove(path.c_str()) != 0) {
        std::cerr << "[checkpoint] [错误] 删除检查点失败: " << path << std::endl;
        return false;
    }
    return true;
}

// ============================================================================
// list_all - 列出所有检查点 (返回帧名列表, 已去除 .json 后缀)
// ============================================================================
std::vector<std::string> CheckpointManager::list_all() {
    std::vector<std::string> result;
    if (!fs::exists(checkpoint_dir_) || !fs::is_directory(checkpoint_dir_)) {
        return result;
    }

    for (const auto& entry : fs::directory_iterator(checkpoint_dir_)) {
        if (!entry.is_regular_file()) continue;
        std::string ext = entry.path().extension().string();
        // 转小写
        std::transform(ext.begin(), ext.end(), ext.begin(),
                       [](unsigned char c) { return std::tolower(c); });
        if (ext == ".json") {
            // 返回 stem (不含扩展名)
            result.push_back(entry.path().stem().string());
        }
    }

    // 按字母顺序排序, 保证输出稳定
    std::sort(result.begin(), result.end());
    return result;
}

// ============================================================================
// clear_all - 清除所有检查点
// ============================================================================
void CheckpointManager::clear_all() {
    if (!fs::exists(checkpoint_dir_) || !fs::is_directory(checkpoint_dir_)) {
        return;
    }

    int n_removed = 0;
    for (const auto& entry : fs::directory_iterator(checkpoint_dir_)) {
        if (!entry.is_regular_file()) continue;
        std::string ext = entry.path().extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(),
                       [](unsigned char c) { return std::tolower(c); });
        if (ext == ".json") {
            std::error_code ec;
            if (fs::remove(entry.path(), ec)) {
                ++n_removed;
            } else if (ec) {
                std::cerr << "[checkpoint] [警告] 删除失败: "
                          << entry.path().string()
                          << " (" << ec.message() << ")" << std::endl;
            }
        }
    }
    std::cerr << "[checkpoint] 已清除 " << n_removed << " 个检查点文件" << std::endl;
}

// ============================================================================
// update_stage - 更新阶段完成状态 (便捷方法)
// 流程: load 现有 → 添加/更新阶段记录 → 更新 current_stage_id → save
// ============================================================================
bool CheckpointManager::update_stage(const std::string& frame_name, int stage_id,
                                      const std::string& stage_name,
                                      double duration, bool success) {
    CheckpointData data;
    bool existed = load(frame_name, data);
    if (!existed) {
        // 创建新检查点
        data.frame_name = frame_name;
        data.fits_path = "";
        data.current_stage_id = 0;
        data.fully_completed = false;
        data.created_at = get_timestamp();
    }

    // 添加或更新阶段记录 (同 stage_id 则覆盖)
    bool found = false;
    for (auto& st : data.stages_completed) {
        if (st.stage_id == stage_id) {
            st.stage_name = stage_name;
            st.duration_sec = duration;
            st.success = success;
            st.timestamp = get_timestamp();
            found = true;
            break;
        }
    }
    if (!found) {
        CheckpointStage st;
        st.stage_name = stage_name;
        st.stage_id = stage_id;
        st.duration_sec = duration;
        st.success = success;
        st.timestamp = get_timestamp();
        data.stages_completed.push_back(st);
    }

    // 更新 timings 映射
    data.timings[stage_name] = duration;

    // 更新 current_stage_id (取最大 stage_id + 1, 但不小于当前)
    int max_id = -1;
    for (const auto& st : data.stages_completed) {
        if (st.success && st.stage_id > max_id) {
            max_id = st.stage_id;
        }
    }
    data.current_stage_id = max_id + 1;

    // 若已完成全部 4 个阶段 (0-3), 标记 fully_completed
    // 注: 阶段 ID 0=CALIBRATE, 1=PLATESOLVE, 2=PHOTOMETRIC, 3=DRIZZLE
    if (data.current_stage_id >= 4) {
        data.fully_completed = true;
    }

    return save(frame_name, data);
}

// ============================================================================
// is_stage_completed - 检查阶段是否已完成
// ============================================================================
bool CheckpointManager::is_stage_completed(const std::string& frame_name, int stage_id) {
    CheckpointData data;
    if (!load(frame_name, data)) {
        return false;
    }
    for (const auto& st : data.stages_completed) {
        if (st.stage_id == stage_id && st.success) {
            return true;
        }
    }
    return false;
}

// ============================================================================
// get_resume_stage - 获取恢复起点
// 返回: max(stage_id) + 1, 如无已完成阶段返回 0, 如 fully_completed 返回 -1
// ============================================================================
int CheckpointManager::get_resume_stage(const std::string& frame_name) {
    CheckpointData data;
    if (!load(frame_name, data)) {
        // 检查点不存在, 从头开始
        return 0;
    }

    if (data.fully_completed) {
        return -1;
    }

    // 找已成功的最大 stage_id
    int max_id = -1;
    for (const auto& st : data.stages_completed) {
        if (st.success && st.stage_id > max_id) {
            max_id = st.stage_id;
        }
    }
    return max_id + 1;
}

// ============================================================================
// get_timestamp - 生成 ISO 8601 格式时间戳
// 格式: YYYY-MM-DDTHH:MM:SS
// ============================================================================
std::string CheckpointManager::get_timestamp() {
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm_local;
#ifdef _WIN32
    localtime_s(&tm_local, &t);
#else
    localtime_r(&t, &tm_local);
#endif
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", &tm_local);
    return std::string(buf);
}
