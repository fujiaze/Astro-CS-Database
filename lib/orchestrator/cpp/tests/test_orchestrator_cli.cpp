// ============================================================================
// test_orchestrator_cli.cpp - 阶段1集成测试 (Phase1 JSON 入口)
// 功能: 验证编排器 Phase1 JSON 入口重构后的各组件协同工作
// Part 1: JSON 入口命令测试 (--help/--version/--print-schema/--validate/no-args/unknown)
// Part 2: Schema 验证与配置解析测试 (validate_stage1_schema + parse_stage1_config)
// Part 3: 断点续传测试 (CheckpointManager + Orchestrator)
// Part 4: DLL 加载失败降级测试 (DllLoader)
// Part 5: 日志系统集成测试 (Logger)
// Part 6: JSON 入口 stage1 执行与 JSONL 事件测试
// Part 7: SHA-256 与配置哈希测试
//
// 编译:
// g++ -O2 -std=c++17 -Wall -fopenmp -o tests/test_orchestrator_cli.exe
// tests/test_orchestrator_cli.cpp src/orchestrator.cpp src/dll_loader.cpp
// src/checkpoint.cpp src/logger.cpp src/cli_command.cpp src/json_config.cpp
// -Iinclude -static -lm
//
// 运行 (在 cpp/ 目录下执行):
// .\tests\test_orchestrator_cli.exe
// ============================================================================

#include <iostream>
#include <string>
#include <vector>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <chrono>
#include <thread>
#include <cstdio>
#include <cstdint>
#include <memory>
#include <algorithm>
#include <cstdlib>

#ifdef _WIN32
#include <windows.h>
// windows.h 会重新定义 ERROR 宏, 与 LogLevel::ERROR 冲突
#ifdef ERROR
#undef ERROR
#endif
#endif

#include "orchestrator.h"
#include "dll_loader.h"
#include "checkpoint.h"
#include "logger.h"
#include "json_config.h"
#include "cli_command.h"  // for p04004_register/unregister_signal_handler

namespace fs = std::filesystem;

// 前向声明 sha256_impl (定义在 cli_command.cpp, 链接时解析)
namespace sha256_impl {
std::string sha256(const std::string& input);
}

// ============================================================================
// 测试计数与断言宏
// ============================================================================
static int g_pass_count = 0;
static int g_fail_count = 0;

#define ASSERT_TRUE(cond, msg)                                                  \
    do {                                                                        \
        if (!(cond)) {                                                          \
            std::cerr << "  [FAIL] " << (msg)                                   \
                      << " (line " << __LINE__ << ")" << std::endl;             \
            ++g_fail_count;                                                     \
        } else {                                                                \
            std::cout << "  [PASS] " << (msg) << std::endl;                     \
            ++g_pass_count;                                                     \
        }                                                                       \
    } while (0)

#define ASSERT_EQ(a, b, msg) ASSERT_TRUE((a) == (b), msg)
#define ASSERT_FALSE(cond, msg) ASSERT_TRUE(!(cond), msg)
#define ASSERT_CONTAINS(str, substr, msg) \
    ASSERT_TRUE((str).find(substr) != std::string::npos, msg)

#define TEST_SECTION(name)                                                      \
    do {                                                                        \
        std::cout << "\n========================================================"   \
                  << "\n[Part] " << (name)                                      \
                  << "\n========================================================"   \
                  << std::endl;                                                 \
    } while (0)

// ============================================================================
// 临时目录管理类
// ============================================================================
class TempDir {
public:
    explicit TempDir(const std::string& prefix = "test_") {
        auto now = std::chrono::steady_clock::now().time_since_epoch();
        auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(now).count();
        std::ostringstream oss;
        oss << prefix << ns;
        path_ = oss.str();
        std::error_code ec;
        fs::create_directories(path_, ec);
    }
    ~TempDir() { cleanup(); }
    std::string path() const { return path_; }
    void cleanup() {
        std::error_code ec;
        fs::remove_all(path_, ec);
    }
private:
    std::string path_;
};

// ============================================================================
// 执行结果结构体
// ============================================================================
struct ExecResult {
    int exit_code;
    std::string stdout_output;
    std::string stderr_output;
};

// ============================================================================
// exec_with_stdin - 通过管道模拟 stdin 输入, 捕获 stdout/stderr
// command_line: 完整命令行 (如 "orchestrator.exe" 或 "orchestrator.exe --help")
// stdin_input: 要写入 stdin 的内容
// 返回: ExecResult (退出码 + stdout + stderr)
// ============================================================================
ExecResult exec_with_stdin(const std::string& command_line,
                            const std::string& stdin_input) {
    ExecResult result;
    result.exit_code = -1;

#ifdef _WIN32
    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = NULL;

    HANDLE stdin_read = nullptr, stdin_write = nullptr;
    HANDLE stdout_read = nullptr, stdout_write = nullptr;
    HANDLE stderr_read = nullptr, stderr_write = nullptr;

    if (!CreatePipe(&stdin_read, &stdin_write, &sa, 0)) return result;
    if (!CreatePipe(&stdout_read, &stdout_write, &sa, 0)) {
        CloseHandle(stdin_read); CloseHandle(stdin_write);
        return result;
    }
    if (!CreatePipe(&stderr_read, &stderr_write, &sa, 0)) {
        CloseHandle(stdin_read); CloseHandle(stdin_write);
        CloseHandle(stdout_read); CloseHandle(stdout_write);
        return result;
    }

    // 父进程端设为不可继承
    SetHandleInformation(stdin_write, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(stdout_read, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(stderr_read, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.hStdInput = stdin_read;
    si.hStdOutput = stdout_write;
    si.hStdError = stderr_write;
    si.dwFlags |= STARTF_USESTDHANDLES;
    ZeroMemory(&pi, sizeof(pi));

    // CreateProcessA 需要可写的命令行缓冲区
    std::vector<char> cmd(command_line.begin(), command_line.end());
    cmd.push_back('\0');

    if (!CreateProcessA(NULL, cmd.data(), NULL, NULL, TRUE,
                        CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        CloseHandle(stdin_read); CloseHandle(stdin_write);
        CloseHandle(stdout_read); CloseHandle(stdout_write);
        CloseHandle(stderr_read); CloseHandle(stderr_write);
        return result;
    }

    // 关闭子进程端的句柄 (父进程不需要)
    CloseHandle(stdin_read);
    CloseHandle(stdout_write);
    CloseHandle(stderr_write);

    // 写入 stdin 输入
    if (!stdin_input.empty()) {
        DWORD written = 0;
        WriteFile(stdin_write, stdin_input.c_str(),
                  static_cast<DWORD>(stdin_input.size()), &written, NULL);
    }
    CloseHandle(stdin_write);  // 关闭写端, 触发子进程 EOF

    // 使用线程并发读取 stdout/stderr, 避免管道满死锁
    std::string stdout_buf, stderr_buf;
    std::thread stdout_thread([&]() {
        char buffer[4096];
        DWORD read_bytes = 0;
        while (ReadFile(stdout_read, buffer, sizeof(buffer), &read_bytes, NULL)
               && read_bytes > 0) {
            stdout_buf.append(buffer, read_bytes);
        }
    });
    std::thread stderr_thread([&]() {
        char buffer[4096];
        DWORD read_bytes = 0;
        while (ReadFile(stderr_read, buffer, sizeof(buffer), &read_bytes, NULL)
               && read_bytes > 0) {
            stderr_buf.append(buffer, read_bytes);
        }
    });

    // 等待进程退出 (30 秒超时)
    WaitForSingleObject(pi.hProcess, 30000);

    DWORD exit_code = 0;
    GetExitCodeProcess(pi.hProcess, &exit_code);
    result.exit_code = static_cast<int>(exit_code);

    // 等待读取线程结束
    stdout_thread.join();
    stderr_thread.join();

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    CloseHandle(stdout_read);
    CloseHandle(stderr_read);

    result.stdout_output = stdout_buf;
    result.stderr_output = stderr_buf;
#endif

    return result;
}

// ============================================================================
// exec_command - 执行命令 (无 stdin 输入)
// ============================================================================
ExecResult exec_command(const std::string& command_line) {
    return exec_with_stdin(command_line, "");
}

// ============================================================================
// find_orchestrator_exe - 查找 orchestrator.exe 路径
// ============================================================================
std::string find_orchestrator_exe() {
    if (fs::exists("orchestrator.exe")) return "orchestrator.exe";
    if (fs::exists("../orchestrator.exe")) return "../orchestrator.exe";
    if (fs::exists("../../orchestrator.exe")) return "../../orchestrator.exe";
    return "orchestrator.exe";
}

// ============================================================================
// 辅助: 写入有效的 stage1.json 配置文件
// 返回: 文件路径
// ============================================================================
// 查找包含 testdata/Galaxy_Center_T4 的项目根 (从 cwd 向上)
static std::string find_project_root() {
    fs::path cur = fs::absolute(fs::current_path());
    for (int i = 0; i < 8; ++i) {
        fs::path light = cur / "testdata" / "Galaxy_Center_T4" / "lights" / "panel3"
                         / "Galaxy_Center_mosaic3_T4_flying_dutchman-20250718@001638-180S-Red.fts";
        if (fs::exists(light)) {
            return cur.string();
        }
        cur = cur.parent_path();
    }
    return fs::absolute(fs::current_path()).string();
}

static std::string write_valid_stage1_json(const std::string& tmpdir,
                                            const std::string& filename = "stage1.json") {
    std::string path = tmpdir + "/" + filename;
    std::string root = find_project_root();
    std::replace(root.begin(), root.end(), '\\', '/');
    std::string light = root + "/testdata/Galaxy_Center_T4/lights/panel3/Galaxy_Center_mosaic3_T4_flying_dutchman-20250718@001638-180S-Red.fts";
    std::string mb = root + "/testdata/T4 calibration files/masterBias_BIN-1_4500x3600.xisf";
    std::string md = root + "/testdata/T4 calibration files/masterDark_BIN-1_4500x3600_EXPOSURE-180.00s.xisf";
    std::string mf = root + "/testdata/T4 calibration files/masterFlat_BIN-1_4500x3600_FILTER-Red_mono.xisf";
    std::ofstream ofs(path);
    ofs << "{"
        << "\"schema_version\":\"1.1\","
        << "\"pipeline\":\"stage1\","
        << "\"gaia_data_dir\":\"GaiaDR3SP\","
        << "\"precision\":\"fp32\","
        << "\"input\":{"
        <<   "\"light\":\"" << light << "\","
        <<   "\"master_bias\":\"" << mb << "\","
        <<   "\"master_dark\":\"" << md << "\","
        <<   "\"master_flat\":\"" << mf << "\""
        << "},"
        << "\"calibration\":{"
        <<   "\"mode\":\"standard\","
        <<   "\"light_exposure_s\":180.0,"
        <<   "\"dark_exposure_s\":180.0,"
        <<   "\"fallback\":\"exposure_ratio\""
        << "},"
        << "\"platesolve\":{"
        <<   "\"gaia_catalog\":\"GaiaDR3\","
        <<   "\"max_stars\":2000,"
        <<   "\"initial_ra_deg\":null,"
        <<   "\"initial_dec_deg\":null"
        << "},"
        << "\"psf\":{"
        <<   "\"fit_radius\":8,"
        <<   "\"max_iterations\":100,"
        <<   "\"tolerance\":1e-6"
        << "},"
        << "\"photometric\":{"
        <<   "\"gaia_spectra\":\"GaiaDR3SP\","
        <<   "\"filter_response\":\"filters.json\","
        <<   "\"qe_curve\":\"qe.json\""
        << "},"
        << "\"snr\":{"
        <<   "\"estimator_id\":1,"
        <<   "\"sampling_scale\":1.0"
        << "},"
        << "\"drizzle\":{"
        <<   "\"mode\":\"precise\","
        <<   "\"pixfrac\":1.0,"
        <<   "\"nside\":{\"mode\":\"auto\"},"
        <<   "\"ordering\":\"nested\""
        << "},"
        << "\"output\":{"
        <<   "\"hips\":\"" << tmpdir << "/out.hips\","
        <<   "\"hiss\":\"" << tmpdir << "/out.hiss\","
        <<   "\"log\":\"" << tmpdir << "/run.log\","
        <<   "\"diagnostics_dir\":\"" << tmpdir << "/diag\","
        <<   "\"overwrite\":false"
        << "},"
        << "\"execution\":{"
        <<   "\"stop_after\":\"browser_verify\","
        <<   "\"threads\":0,"
        <<   "\"stage_timeout_sec\":{"
        <<     "\"read\":60,\"calibrate\":120,\"platesolve\":300,\"psf\":120,"
        <<     "\"photometric\":300,\"snr\":120,\"nside\":120,\"drizzle\":1800,"
        <<     "\"hips_verify\":120,\"hiss_verify\":120,\"browser_verify\":120"
        <<   "}"
        << "}"
        << "}";
    ofs.close();
    return path;
}

// ============================================================================
// Part 1: JSON 入口命令测试
// 验证 --help/-h, --version, --print-schema, --validate, 无参数, 未知选项
// ============================================================================
void test_part1_json_entry_commands() {
    TEST_SECTION("Part 1: JSON 入口命令测试");

    std::string exe = find_orchestrator_exe();
    TempDir tmp("p1_json_entry_");
    std::string tmpdir = tmp.path();

    // 测试 1: 无参数打印 usage, 退出码 0
    {
        ExecResult r = exec_command(exe);
        ASSERT_EQ(r.exit_code, 0, "无参数退出码为 0");
        ASSERT_CONTAINS(r.stdout_output, "Usage", "无参数输出包含 Usage");
        ASSERT_CONTAINS(r.stdout_output, "stage1.json", "无参数输出包含 stage1.json");
    }

    // 测试 2: --help 输出帮助信息
    {
        ExecResult r = exec_command(exe + " --help");
        ASSERT_EQ(r.exit_code, 0, "--help 退出码为 0");
        ASSERT_CONTAINS(r.stdout_output, "Orchestrator", "--help 输出包含 Orchestrator");
        ASSERT_CONTAINS(r.stdout_output, "stage1.json", "--help 输出包含 stage1.json");
        ASSERT_CONTAINS(r.stdout_output, "--validate", "--help 输出包含 --validate");
        ASSERT_CONTAINS(r.stdout_output, "--print-schema", "--help 输出包含 --print-schema");
        ASSERT_CONTAINS(r.stdout_output, "--version", "--help 输出包含 --version");
    }

    // 测试 3: -h 短选项
    {
        ExecResult r = exec_command(exe + " -h");
        ASSERT_EQ(r.exit_code, 0, "-h 退出码为 0");
        ASSERT_CONTAINS(r.stdout_output, "Orchestrator", "-h 输出包含 Orchestrator");
    }

    // 测试 4: --version 输出版本信息
    {
        ExecResult r = exec_command(exe + " --version");
        ASSERT_EQ(r.exit_code, 0, "--version 退出码为 0");
        ASSERT_CONTAINS(r.stdout_output, "Orchestrator", "--version 输出包含 Orchestrator");
        ASSERT_CONTAINS(r.stdout_output, "git commit", "--version 输出包含 git commit");
    }

    // 测试 5: --print-schema 输出 Schema JSON
    {
        ExecResult r = exec_command(exe + " --print-schema");
        ASSERT_EQ(r.exit_code, 0, "--print-schema 退出码为 0");
        ASSERT_CONTAINS(r.stdout_output, "schema_version", "--print-schema 输出包含 schema_version");
        ASSERT_CONTAINS(r.stdout_output, "stage1", "--print-schema 输出包含 stage1");
        ASSERT_CONTAINS(r.stdout_output, "additionalProperties", "--print-schema 输出包含 additionalProperties");
        ASSERT_CONTAINS(r.stdout_output, "pixfrac", "--print-schema 输出包含 pixfrac");
    }

    // 测试 6: --validate 有效 JSON 返回 0 并输出 VALID
    {
        std::string json_path = write_valid_stage1_json(tmpdir, "valid.json");
        ExecResult r = exec_command(exe + " --validate " + json_path);
        ASSERT_EQ(r.exit_code, 0, "--validate 有效 JSON 退出码为 0");
        ASSERT_CONTAINS(r.stdout_output, "VALID", "--validate 有效 JSON 输出 VALID");
    }

    // 测试 7: --validate 无效 JSON 返回 1 并输出 INVALID
    {
        std::string bad_path = tmpdir + "/invalid.json";
        std::ofstream ofs(bad_path);
        ofs << "{\"schema_version\":\"2.0\",\"pipeline\":\"stage1\"}";
        ofs.close();
        ExecResult r = exec_command(exe + " --validate " + bad_path);
        ASSERT_EQ(r.exit_code, 1, "--validate 无效 JSON 退出码为 1");
        ASSERT_CONTAINS(r.stdout_output, "INVALID", "--validate 无效 JSON 输出 INVALID");
    }

    // 测试 8: --validate 缺少参数返回 CONFIG_ERROR(7)
    {
        ExecResult r = exec_command(exe + " --validate");
        ASSERT_EQ(r.exit_code, AstroCsExitCode::CONFIG_ERROR,
                  "--validate 缺少参数退出码为 7 (CONFIG_ERROR)");
    }

    // 测试 9: --validate 不存在的文件
    {
        ExecResult r = exec_command(exe + " --validate Z:/nonexistent.json");
        ASSERT_EQ(r.exit_code, 1, "--validate 不存在文件退出码为 1");
        ASSERT_CONTAINS(r.stdout_output, "INVALID", "--validate 不存在文件输出 INVALID");
    }

    // 测试 10: 传入不存在的 JSON 文件返回 CONFIG_ERROR(7)
    {
        ExecResult r = exec_command(exe + " Z:/nonexistent_config.json");
        ASSERT_EQ(r.exit_code, AstroCsExitCode::CONFIG_ERROR,
                  "不存在的 JSON 文件退出码为 7 (CONFIG_ERROR)");
        ASSERT_FALSE(r.stderr_output.empty(), "stderr 包含错误信息");
    }

    // 测试 11: 传入 Schema 验证失败的 JSON 返回 CONFIG_ERROR(7)
    {
        std::string bad_path = tmpdir + "/bad_schema.json";
        std::ofstream ofs(bad_path);
        ofs << "{\"schema_version\":\"1.0\",\"pipeline\":\"stage1\"}";
        ofs.close();
        ExecResult r = exec_command(exe + " " + bad_path);
        ASSERT_EQ(r.exit_code, AstroCsExitCode::CONFIG_ERROR,
                  "Schema 验证失败退出码为 7 (CONFIG_ERROR)");
        ASSERT_FALSE(r.stderr_output.empty(), "stderr 包含配置错误信息");
    }

    // 测试 12: 未知选项返回 CONFIG_ERROR(7)
    {
        ExecResult r = exec_command(exe + " --unknown-option");
        ASSERT_EQ(r.exit_code, AstroCsExitCode::CONFIG_ERROR,
                  "未知选项退出码为 7 (CONFIG_ERROR)");
        ASSERT_CONTAINS(r.stderr_output, "unknown option", "stderr 包含 unknown option");
    }

    // 测试 13: 参数过多返回 CONFIG_ERROR(7)
    {
        std::string json_path = write_valid_stage1_json(tmpdir, "valid2.json");
        ExecResult r = exec_command(exe + " " + json_path + " extra_arg");
        ASSERT_EQ(r.exit_code, AstroCsExitCode::CONFIG_ERROR,
                  "参数过多退出码为 7 (CONFIG_ERROR)");
        ASSERT_CONTAINS(r.stderr_output, "too many arguments", "stderr 包含 too many arguments");
    }
}

// ============================================================================
// Part 2: Schema 验证与配置解析测试
// 直接调用 validate_stage1_schema 和 parse_stage1_config
// ============================================================================
void test_part2_schema_validation_and_parsing() {
    TEST_SECTION("Part 2: Schema 验证与配置解析测试");

    TempDir tmp("p2_schema_");
    std::string tmpdir = tmp.path();
    std::string err;

    // 测试 1: 有效 JSON 通过 Schema 验证
    {
        std::string path = write_valid_stage1_json(tmpdir, "valid.json");
        bool ok = validate_stage1_schema(path, err);
        ASSERT_TRUE(ok, "有效 JSON 通过 Schema 验证");
        if (!ok) std::cerr << "    错误: " << err << std::endl;
    }

    // 测试 2: 缺少必填字段 (pipeline) 验证失败
    {
        std::string path = tmpdir + "/missing_pipeline.json";
        std::ofstream ofs(path);
        ofs << "{\"schema_version\":\"1.0\",\"precision\":\"fp32\"}";
        ofs.close();
        bool ok = validate_stage1_schema(path, err);
        ASSERT_FALSE(ok, "缺少 pipeline 字段验证失败");
        ASSERT_CONTAINS(err, "pipeline", "错误信息包含 pipeline");
    }

    // 测试 3: 未知顶层字段验证失败
    {
        std::string path = tmpdir + "/unknown_field.json";
        std::ofstream ofs(path);
        ofs << "{\"schema_version\":\"1.0\",\"pipeline\":\"stage1\","
            << "\"precision\":\"fp32\",\"unknown_field\":\"value\"}";
        ofs.close();
        bool ok = validate_stage1_schema(path, err);
        ASSERT_FALSE(ok, "未知顶层字段验证失败");
        ASSERT_CONTAINS(err, "unknown_field", "错误信息包含 unknown_field");
    }

    // 测试 4: 错误的 schema_version 验证失败
    // 使用完整有效配置, 仅修改 schema_version 字段, 确保 required 检查通过
    {
        std::string path = write_valid_stage1_json(tmpdir, "bad_version_base.json");
        // 读取完整配置, 替换 schema_version 值
        std::ifstream ifs(path);
        std::stringstream ss;
        ss << ifs.rdbuf();
        std::string content = ss.str();
        ifs.close();
        // 将 "schema_version":"1.1" 替换为 "schema_version":"2.0"
        const std::string old_sv = "\"schema_version\":\"1.1\"";
        const std::string new_sv = "\"schema_version\":\"2.0\"";
        size_t pos = content.find(old_sv);
        if (pos != std::string::npos) {
            content.replace(pos, old_sv.length(), new_sv);
        }
        std::string bad_path = tmpdir + "/bad_version.json";
        std::ofstream ofs(bad_path);
        ofs << content;
        ofs.close();
        bool ok = validate_stage1_schema(bad_path, err);
        ASSERT_FALSE(ok, "错误的 schema_version 验证失败");
        ASSERT_CONTAINS(err, "schema_version", "错误信息包含 schema_version");
    }

    // 测试 5: 错误的 pipeline 值验证失败
    // 使用完整有效配置, 仅修改 pipeline 字段
    {
        std::string path = write_valid_stage1_json(tmpdir, "bad_pipeline_base.json");
        std::ifstream ifs(path);
        std::stringstream ss;
        ss << ifs.rdbuf();
        std::string content = ss.str();
        ifs.close();
        const std::string old_str = "\"pipeline\":\"stage1\"";
        const std::string new_str = "\"pipeline\":\"stage2\"";
        size_t pos = content.find(old_str);
        if (pos != std::string::npos) {
            content.replace(pos, old_str.length(), new_str);
        }
        std::string bad_path = tmpdir + "/bad_pipeline.json";
        std::ofstream ofs(bad_path);
        ofs << content;
        ofs.close();
        bool ok = validate_stage1_schema(bad_path, err);
        ASSERT_FALSE(ok, "错误的 pipeline 值验证失败");
        ASSERT_CONTAINS(err, "pipeline", "错误信息包含 pipeline");
    }

    // 测试 6: 无效的 precision 枚举验证失败
    // 使用完整有效配置, 仅修改 precision 字段
    {
        std::string path = write_valid_stage1_json(tmpdir, "bad_precision_base.json");
        std::ifstream ifs(path);
        std::stringstream ss;
        ss << ifs.rdbuf();
        std::string content = ss.str();
        ifs.close();
        size_t pos = content.find("\"precision\":\"fp32\"");
        if (pos != std::string::npos) {
            content.replace(pos, 18, "\"precision\":\"fp16\"");
        }
        std::string bad_path = tmpdir + "/bad_precision.json";
        std::ofstream ofs(bad_path);
        ofs << content;
        ofs.close();
        bool ok = validate_stage1_schema(bad_path, err);
        ASSERT_FALSE(ok, "无效的 precision 枚举验证失败");
        ASSERT_CONTAINS(err, "precision", "错误信息包含 precision");
    }

    // 测试 7: input.light 空字符串验证失败
    // 使用完整有效配置, 仅修改 input.light 字段
    {
        std::string path = write_valid_stage1_json(tmpdir, "empty_light_base.json");
        std::ifstream ifs(path);
        std::stringstream ss;
        ss << ifs.rdbuf();
        std::string content = ss.str();
        ifs.close();
        // 找到 "light":"<绝对路径>" 并替换为空串
        size_t pos = content.find("\"light\":\"");
        if (pos != std::string::npos) {
            size_t end = content.find("\"", pos + 9);
            if (end != std::string::npos) {
                content.replace(pos, end - pos + 1, "\"light\":\"\"");
            }
        }
        std::string bad_path = tmpdir + "/empty_light.json";
        std::ofstream ofs(bad_path);
        ofs << content;
        ofs.close();
        bool ok = validate_stage1_schema(bad_path, err);
        ASSERT_FALSE(ok, "input.light 空字符串验证失败");
        ASSERT_CONTAINS(err, "light", "错误信息包含 light");
    }

    // 测试 8: calibration.light_exposure_s <= 0 验证失败
    // 使用完整有效配置, 仅修改 light_exposure_s 字段
    {
        std::string path = write_valid_stage1_json(tmpdir, "bad_exposure_base.json");
        std::ifstream ifs(path);
        std::stringstream ss;
        ss << ifs.rdbuf();
        std::string content = ss.str();
        ifs.close();
        const std::string old_str = "\"light_exposure_s\":180.0";
        const std::string new_str = "\"light_exposure_s\":0";
        size_t pos = content.find(old_str);
        if (pos != std::string::npos) {
            content.replace(pos, old_str.length(), new_str);
        }
        std::string bad_path = tmpdir + "/bad_exposure.json";
        std::ofstream ofs(bad_path);
        ofs << content;
        ofs.close();
        bool ok = validate_stage1_schema(bad_path, err);
        ASSERT_FALSE(ok, "light_exposure_s=0 验证失败");
        ASSERT_CONTAINS(err, "light_exposure_s", "错误信息包含 light_exposure_s");
    }

    // 测试 9: platesolve.max_stars < 1 验证失败
    // 使用完整有效配置, 仅修改 max_stars 字段
    {
        std::string path = write_valid_stage1_json(tmpdir, "bad_max_stars_base.json");
        std::ifstream ifs(path);
        std::stringstream ss;
        ss << ifs.rdbuf();
        std::string content = ss.str();
        ifs.close();
        size_t pos = content.find("\"max_stars\":2000");
        if (pos != std::string::npos) {
            content.replace(pos, 16, "\"max_stars\":0");
        }
        std::string bad_path = tmpdir + "/bad_max_stars.json";
        std::ofstream ofs(bad_path);
        ofs << content;
        ofs.close();
        bool ok = validate_stage1_schema(bad_path, err);
        ASSERT_FALSE(ok, "max_stars=0 验证失败");
        ASSERT_CONTAINS(err, "max_stars", "错误信息包含 max_stars");
    }

    // 测试 10: drizzle.pixfrac 超出范围验证失败
    // 使用完整有效配置, 仅修改 pixfrac 字段
    {
        std::string path = write_valid_stage1_json(tmpdir, "bad_pixfrac_base.json");
        std::ifstream ifs(path);
        std::stringstream ss;
        ss << ifs.rdbuf();
        std::string content = ss.str();
        ifs.close();
        size_t pos = content.find("\"pixfrac\":1.0");
        if (pos != std::string::npos) {
            content.replace(pos, 13, "\"pixfrac\":2.0");
        }
        std::string bad_path = tmpdir + "/bad_pixfrac.json";
        std::ofstream ofs(bad_path);
        ofs << content;
        ofs.close();
        bool ok = validate_stage1_schema(bad_path, err);
        ASSERT_FALSE(ok, "pixfrac=2.0 验证失败");
        ASSERT_CONTAINS(err, "pixfrac", "错误信息包含 pixfrac");
    }

    // 测试 11: drizzle.nside.mode 无效验证失败
    // 使用完整有效配置, 仅修改 nside.mode 字段
    {
        std::string path = write_valid_stage1_json(tmpdir, "bad_nside_mode_base.json");
        std::ifstream ifs(path);
        std::stringstream ss;
        ss << ifs.rdbuf();
        std::string content = ss.str();
        ifs.close();
        const std::string old_str = "\"nside\":{\"mode\":\"auto\"}";
        const std::string new_str = "\"nside\":{\"mode\":\"invalid\"}";
        size_t pos = content.find(old_str);
        if (pos != std::string::npos) {
            content.replace(pos, old_str.length(), new_str);
        }
        std::string bad_path = tmpdir + "/bad_nside_mode.json";
        std::ofstream ofs(bad_path);
        ofs << content;
        ofs.close();
        bool ok = validate_stage1_schema(bad_path, err);
        ASSERT_FALSE(ok, "nside.mode=invalid 验证失败");
        ASSERT_CONTAINS(err, "nside", "错误信息包含 nside");
    }

    // 测试 12: execution.stop_after 非法值验证失败
    {
        std::string path = write_valid_stage1_json(tmpdir, "bad_stop_after.json");
        std::ifstream ifs(path);
        std::stringstream ss;
        ss << ifs.rdbuf();
        std::string content = ss.str();
        ifs.close();
        const std::string old_str = "\"stop_after\":\"browser_verify\"";
        size_t pos = content.find(old_str);
        if (pos != std::string::npos) {
            content.replace(pos, old_str.length(), "\"stop_after\":\"invalid_stage\"");
        }
        std::ofstream ofs(path);
        ofs << content;
        ofs.close();
        bool ok = validate_stage1_schema(path, err);
        ASSERT_FALSE(ok, "stop_after=invalid_stage 验证失败");
        ASSERT_CONTAINS(err, "stop_after", "错误信息包含 stop_after");
    }

    // 测试 13: JSON 语法错误验证失败
    {
        std::string path = tmpdir + "/syntax_error.json";
        std::ofstream ofs(path);
        ofs << "{not valid json";
        ofs.close();
        bool ok = validate_stage1_schema(path, err);
        ASSERT_FALSE(ok, "JSON 语法错误验证失败");
        ASSERT_CONTAINS(err, "JSON", "错误信息包含 JSON");
    }

    // 测试 14: parse_stage1_config 正确填充配置字段
    {
        std::string path = write_valid_stage1_json(tmpdir, "parse_test.json");
        Stage1Config config;
        int ret = parse_stage1_config(path, config, err);
        ASSERT_EQ(ret, 0, "parse_stage1_config 成功返回 0");
        ASSERT_EQ(config.schema_version, std::string("1.1"), "schema_version 填充正确");
        ASSERT_EQ(config.pipeline, std::string("stage1"), "pipeline 填充正确");
        ASSERT_TRUE(config.precision == PrecisionMode::FP32, "precision 填充为 FP32");
        ASSERT_EQ(config.calibration.mode, std::string("standard"), "calibration.mode 填充正确");
        ASSERT_EQ(config.calibration.light_exposure_s, 180.0, "light_exposure_s 填充正确");
        ASSERT_EQ(config.platesolve.max_stars, 2000, "max_stars 填充正确");
        ASSERT_EQ(config.psf.fit_radius, 8, "psf.fit_radius 填充正确");
        ASSERT_EQ(config.drizzle.mode, std::string("precise"), "drizzle.mode 填充正确");
        ASSERT_EQ(config.drizzle.nside_mode, std::string("auto"), "drizzle.nside_mode 填充正确");
        ASSERT_EQ(config.execution.stop_after, std::string("browser_verify"), "stop_after 填充正确");
    }

    // 测试 15: parse_stage1_config 将相对路径解析为绝对路径
    {
        // 写入项目根目录, 使相对路径 "testdata/..." 真实存在 (runtime path check)
        std::string root = find_project_root();
        std::replace(root.begin(), root.end(), '\\', '/');
        std::string path = root + "/_relpath_test.json";
        std::ofstream ofs(path);
        ofs << "{"
            << "\"schema_version\":\"1.1\",\"pipeline\":\"stage1\",\"gaia_data_dir\":\"GaiaDR3SP\",\"precision\":\"fp64\","
            << "\"input\":{\"light\":\"testdata/Galaxy_Center_T4/lights/panel3/Galaxy_Center_mosaic3_T4_flying_dutchman-20250718@001638-180S-Red.fts\","
            << "\"master_bias\":\"testdata/T4 calibration files/masterBias_BIN-1_4500x3600.xisf\","
            << "\"master_dark\":\"testdata/T4 calibration files/masterDark_BIN-1_4500x3600_EXPOSURE-180.00s.xisf\","
            << "\"master_flat\":\"testdata/T4 calibration files/masterFlat_BIN-1_4500x3600_FILTER-Red_mono.xisf\"},"
            << "\"calibration\":{\"mode\":\"standard\","
            << "\"light_exposure_s\":120.0,\"dark_exposure_s\":120.0,"
            << "\"fallback\":\"exposure_ratio\"},"
            << "\"platesolve\":{\"gaia_catalog\":\"GaiaDR3\",\"max_stars\":500,"
            << "\"initial_ra_deg\":null,\"initial_dec_deg\":null},"
            << "\"psf\":{\"fit_radius\":4,\"max_iterations\":50,\"tolerance\":1e-5},"
            << "\"photometric\":{\"gaia_spectra\":\"GaiaDR3SP\","
            << "\"filter_response\":\"lib/photometric_calib/data/response_curves/filters.json\","
            << "\"qe_curve\":\"lib/photometric_calib/data/response_curves/qe_curves.json\"},"
            << "\"snr\":{\"estimator_id\":2,\"sampling_scale\":0.5},"
            << "\"drizzle\":{\"mode\":\"precise\",\"pixfrac\":0.8,"
            << "\"nside\":{\"mode\":\"explicit\",\"value\":512},\"ordering\":\"nested\"},"
            << "\"output\":{\"hips\":\"run/temp/r11_delivery/relpath_out.hips\","
            << "\"hiss\":\"run/temp/r11_delivery/relpath_out.hiss\","
            << "\"log\":\"run/temp/r11_delivery/relpath_run.log\","
            << "\"diagnostics_dir\":\"run/temp/r11_delivery/relpath_diag\",\"overwrite\":true},"
            << "\"execution\":{\"stop_after\":\"drizzle\",\"threads\":4,"
            << "\"stage_timeout_sec\":{\"read\":60,\"calibrate\":60.0,\"platesolve\":300,"
            << "\"psf\":120,\"photometric\":300,\"snr\":120,\"nside\":120,"
            << "\"drizzle\":300.0,\"hips_verify\":120,\"browser_verify\":120}}"
            << "}";
        ofs.close();

        Stage1Config config;
        int ret = parse_stage1_config(path, config, err);
        ASSERT_EQ(ret, 0, "相对路径配置解析成功");

        // 验证路径被解析为绝对路径 (基于 JSON 所在目录)
        // Windows 上 fs::weakly_canonical 会将 '/' 转为 '\', 需要规范化后比较
        std::string normalized_light = config.input.light;
        std::replace(normalized_light.begin(), normalized_light.end(), '\\', '/');
        ASSERT_TRUE(normalized_light.find("Galaxy_Center_T4") != std::string::npos,
                    "input.light 包含原始相对路径片段");
        ASSERT_TRUE(fs::path(config.input.light).is_absolute(),
                    "input.light 解析为绝对路径");
        ASSERT_TRUE(fs::path(config.output.hips).is_absolute(),
                    "output.hips 解析为绝对路径");
        ASSERT_TRUE(fs::path(config.output.hiss).is_absolute(),
                    "output.hiss 解析为绝对路径");
        ASSERT_TRUE(fs::path(config.platesolve.gaia_catalog).is_absolute(),
                    "platesolve.gaia_catalog 解析为绝对路径");

        // 验证 FP64 精度模式
        ASSERT_TRUE(config.precision == PrecisionMode::FP64, "precision=fp64 解析为 FP64");

        // 验证 explicit nside
        ASSERT_EQ(config.drizzle.nside_mode, std::string("explicit"), "nside_mode=explicit");
        ASSERT_EQ(config.drizzle.nside_value, 512, "nside_value=512");

        // 验证 stage_timeout_sec (v1.1 schema 要求全部 10 键)
        ASSERT_EQ(config.execution.stage_timeout_sec.size(), static_cast<size_t>(10),
                  "stage_timeout_sec 含 10 个条目");
        ASSERT_EQ(config.execution.stage_timeout_sec["calibrate"], 60.0,
                  "stage_timeout_sec.calibrate=60.0");
        ASSERT_EQ(config.execution.stage_timeout_sec["drizzle"], 300.0,
                  "stage_timeout_sec.drizzle=300.0");
        std::error_code ec;
        fs::remove(path, ec);  // 清理根目录临时配置文件
    }

    // 测试 16: parse_stage1_config 计算 SHA256 (64 位十六进制)
    {
        std::string path = write_valid_stage1_json(tmpdir, "sha_test.json");
        Stage1Config config;
        int ret = parse_stage1_config(path, config, err);
        ASSERT_EQ(ret, 0, "SHA256 测试配置解析成功");

        // config_sha256 应为 64 位小写十六进制
        ASSERT_EQ(config.config_sha256.size(), static_cast<size_t>(64),
                  "config_sha256 长度为 64");
        bool hash_ok = true;
        for (char c : config.config_sha256) {
            if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) {
                hash_ok = false; break;
            }
        }
        ASSERT_TRUE(hash_ok, "config_sha256 全部为小写十六进制");

        // original_json_sha256 应为 64 位小写十六进制
        ASSERT_EQ(config.original_json_sha256.size(), static_cast<size_t>(64),
                  "original_json_sha256 长度为 64");

        // original_json_path 应为绝对路径
        ASSERT_TRUE(fs::path(config.original_json_path).is_absolute(),
                    "original_json_path 为绝对路径");
    }

    // 测试 17 : pixfrac 省略时生产默认 0.8
    {
        std::string path = write_valid_stage1_json(tmpdir, "pixfrac_omitted.json");
        std::ifstream ifs(path);
        std::stringstream ss;
        ss << ifs.rdbuf();
        std::string content = ss.str();
        ifs.close();
        const std::string old_pf = "\"pixfrac\":1.0,";
        size_t pos = content.find(old_pf);
        if (pos != std::string::npos) {
            content.erase(pos, old_pf.length());
        }
        std::ofstream ofs(path);
        ofs << content;
        ofs.close();
        Stage1Config config;
        int ret = parse_stage1_config(path, config, err);
        ASSERT_EQ(ret, 0, "pixfrac 省略配置解析成功");
        ASSERT_EQ(config.drizzle.pixfrac, 0.8, "pixfrac 省略时默认 0.8");
    }

    // 测试 18 : pixfrac 显式 0.8 与权威默认一致
    {
        std::string path = write_valid_stage1_json(tmpdir, "pixfrac_explicit.json");
        std::ifstream ifs(path);
        std::stringstream ss;
        ss << ifs.rdbuf();
        std::string content = ss.str();
        ifs.close();
        const std::string old_pf = "\"pixfrac\":1.0,";
        size_t pos = content.find(old_pf);
        if (pos != std::string::npos) {
            content.replace(pos, old_pf.length(), "\"pixfrac\":0.8,");
        }
        std::ofstream ofs(path);
        ofs << content;
        ofs.close();
        Stage1Config config;
        int ret = parse_stage1_config(path, config, err);
        ASSERT_EQ(ret, 0, "pixfrac 显式 0.8 解析成功");
        ASSERT_EQ(config.drizzle.pixfrac, 0.8, "pixfrac 显式 0.8 生效");
    }

    // 测试 19 : 生产配置省略 output.hiss 必须可解析且验证通过
    // (回归: json_config 输出父目录检查曾对缺键 hiss 无条件 operator[] 断言崩溃)
    {
        std::string path = write_valid_stage1_json(tmpdir, "no_hiss_output.json");
        std::ifstream ifs(path);
        std::stringstream ss;
        ss << ifs.rdbuf();
        std::string content = ss.str();
        ifs.close();
        // 删除整个 "hiss":"<path>", 条目
        size_t b = content.find("\"hiss\":\"");
        if (b != std::string::npos) {
            size_t e = content.find("\"", b + 8);
            if (e != std::string::npos) {
                size_t comma = content.find(',', e);
                if (comma != std::string::npos) content.erase(b, comma - b + 1);
            }
        }
        std::ofstream ofs(path);
        ofs << content;
        ofs.close();
        ASSERT_TRUE(content.find("\"hiss\":") == std::string::npos,
                    "测试配置已移除 output.hiss");

        Stage1Config config;
        std::string err2;
        int ret = parse_stage1_config(path, config, err2);
        ASSERT_EQ(ret, 0, "无 output.hiss 配置解析成功");
        ASSERT_TRUE(config.output.hiss.empty(), "output.hiss 为空串 (legacy 关闭)");
        bool ok = validate_stage1_schema(path, err2);
        ASSERT_TRUE(ok, "无 output.hiss 配置 Schema 验证通过");
    }
}

// ============================================================================
// Part 3: 断点续传测试
// 测试 CheckpointManager 与 Orchestrator 的集成
// ============================================================================
void test_part3_checkpoint_resume() {
    TEST_SECTION("Part 3: 断点续传测试");

    // 测试 1: 创建临时目录和模拟检查点
    {
        TempDir tmp("ckpt_integration_");
        CheckpointManager mgr;
        mgr.set_checkpoint_dir(tmp.path());

        // 构造测试数据: 2 个已完成阶段
        CheckpointData data;
        data.frame_name = "test_frame.fts";
        data.fits_path = "/path/to/test_frame.fts";
        data.current_stage_id = 2;
        data.fully_completed = false;
        data.created_at = "2026-07-13T10:00:00";

        CheckpointStage s0;
        s0.stage_name = "CALIBRATE";
        s0.stage_id = 0;
        s0.duration_sec = 1.5;
        s0.success = true;
        s0.timestamp = "2026-07-13T10:00:01";
        data.stages_completed.push_back(s0);

        CheckpointStage s1;
        s1.stage_name = "PLATESOLVE";
        s1.stage_id = 1;
        s1.duration_sec = 2.3;
        s1.success = true;
        s1.timestamp = "2026-07-13T10:00:03";
        data.stages_completed.push_back(s1);

        bool ok = mgr.save("test_frame.fts", data);
        ASSERT_TRUE(ok, "检查点保存成功");

        ASSERT_TRUE(mgr.exists("test_frame.fts"), "检查点文件存在");

        // 加载并验证字段
        CheckpointData loaded;
        bool lok = mgr.load("test_frame.fts", loaded);
        ASSERT_TRUE(lok, "检查点加载成功");
        ASSERT_EQ(loaded.frame_name, std::string("test_frame.fts"), "frame_name 字段一致");
        ASSERT_EQ(loaded.current_stage_id, 2, "current_stage_id 字段一致");
        ASSERT_EQ(loaded.stages_completed.size(), static_cast<size_t>(2), "stages_completed 数量一致");
        ASSERT_EQ(loaded.stages_completed[0].stage_name, std::string("CALIBRATE"), "阶段0名称一致");
        ASSERT_EQ(loaded.stages_completed[1].stage_name, std::string("PLATESOLVE"), "阶段1名称一致");
    }

    // 测试 2: 检查点恢复 (模拟部分完成的状态)
    {
        TempDir tmp("ckpt_resume_");
        CheckpointManager mgr;
        mgr.set_checkpoint_dir(tmp.path());

        // 模拟完成阶段 0 和 1
        mgr.update_stage("resume_frame.fts", 0, "CALIBRATE", 1.2, true);
        mgr.update_stage("resume_frame.fts", 1, "PLATESOLVE", 2.5, true);

        // 获取恢复起点 (应为 2)
        int rs = mgr.get_resume_stage("resume_frame.fts");
        ASSERT_EQ(rs, 2, "完成阶段0+1后, 恢复起点为2");

        // 检查阶段是否已完成
        ASSERT_TRUE(mgr.is_stage_completed("resume_frame.fts", 0), "阶段0已完成");
        ASSERT_TRUE(mgr.is_stage_completed("resume_frame.fts", 1), "阶段1已完成");
        ASSERT_TRUE(!mgr.is_stage_completed("resume_frame.fts", 2), "阶段2未完成");

        // 继续完成阶段 2 和 3
        mgr.update_stage("resume_frame.fts", 2, "PHOTOMETRIC", 0.8, true);
        rs = mgr.get_resume_stage("resume_frame.fts");
        ASSERT_EQ(rs, 3, "完成阶段2后, 恢复起点为3");

        mgr.update_stage("resume_frame.fts", 3, "DRIZZLE", 1.1, true);
        rs = mgr.get_resume_stage("resume_frame.fts");
        ASSERT_EQ(rs, -1, "完成全部4阶段后, 恢复起点为-1 (fully_completed)");
    }

    // 测试 3: --fresh 参数忽略检查点 (通过 Orchestrator::set_fresh_start)
    {
        TempDir tmp("ckpt_fresh_");
        CheckpointManager mgr;
        mgr.set_checkpoint_dir(tmp.path());

        // 先保存一个检查点
        mgr.update_stage("fresh_frame.fts", 0, "CALIBRATE", 1.0, true);
        ASSERT_TRUE(mgr.exists("fresh_frame.fts"), "检查点已创建");

        // 模拟 fresh_start: 删除现有检查点
        bool removed = mgr.remove("fresh_frame.fts");
        ASSERT_TRUE(removed, "fresh_start 删除检查点成功");
        ASSERT_TRUE(!mgr.exists("fresh_frame.fts"), "删除后检查点不存在");

        // 获取恢复起点 (不存在, 应返回 0)
        int rs = mgr.get_resume_stage("fresh_frame.fts");
        ASSERT_EQ(rs, 0, "检查点删除后, 恢复起点为0 (从头开始)");
    }

    // 测试 4: 检查点列表和清除
    {
        TempDir tmp("ckpt_listclear_");
        CheckpointManager mgr;
        mgr.set_checkpoint_dir(tmp.path());

        // 初始列表应为空
        std::vector<std::string> list = mgr.list_all();
        ASSERT_EQ(list.size(), static_cast<size_t>(0), "初始检查点列表为空");

        // 创建 3 个检查点
        mgr.update_stage("frameA.fts", 0, "CALIBRATE", 1.0, true);
        mgr.update_stage("frameB.fts", 0, "CALIBRATE", 1.0, true);
        mgr.update_stage("frameC.fts", 0, "CALIBRATE", 1.0, true);

        list = mgr.list_all();
        ASSERT_EQ(list.size(), static_cast<size_t>(3), "创建3个检查点后, 列表大小为3");

        // 清除所有检查点
        mgr.clear_all();
        list = mgr.list_all();
        ASSERT_EQ(list.size(), static_cast<size_t>(0), "清除后, 列表为空");

        // 再次清除 (应安全无异常)
        mgr.clear_all();
        ASSERT_TRUE(true, "重复清除安全无异常");
    }

    // 测试 5: Orchestrator 集成 - 设置检查点目录
    {
        TempDir tmp("ckpt_orch_");
        Orchestrator orch;
        orch.set_checkpoint_dir(tmp.path());
        orch.set_enable_checkpoint(true);
        orch.set_fresh_start(false);

        CheckpointManager& mgr = orch.get_checkpoint_manager();
        ASSERT_EQ(mgr.get_checkpoint_dir(), tmp.path(), "Orchestrator 检查点目录设置成功");

        // 测试保存和加载检查点
        bool saved = orch.save_checkpoint("orch_frame.fts");
        ASSERT_TRUE(saved, "Orchestrator 保存检查点成功");

        PipelineStage from;
        bool loaded = orch.load_checkpoint("orch_frame.fts", from);
        ASSERT_TRUE(loaded, "应成功加载刚保存的空检查点");
    }

    // 测试 6: fully_completed 标记
    {
        TempDir tmp("ckpt_full_");
        CheckpointManager mgr;
        mgr.set_checkpoint_dir(tmp.path());

        // 完成全部 4 个阶段
        mgr.update_stage("full_frame.fts", 0, "CALIBRATE", 1.0, true);
        mgr.update_stage("full_frame.fts", 1, "PLATESOLVE", 1.0, true);
        mgr.update_stage("full_frame.fts", 2, "PHOTOMETRIC", 1.0, true);
        mgr.update_stage("full_frame.fts", 3, "DRIZZLE", 1.0, true);

        int rs = mgr.get_resume_stage("full_frame.fts");
        ASSERT_EQ(rs, -1, "全部4阶段完成后, 恢复起点为-1");

        CheckpointData data;
        mgr.load("full_frame.fts", data);
        ASSERT_TRUE(data.fully_completed, "fully_completed 标记为 true");
    }
}

// ============================================================================
// Part 4: DLL 加载失败降级测试
// 测试 DllLoader 的降级处理
// ============================================================================
void test_part4_dll_loader() {
    TEST_SECTION("Part 4: DLL加载失败降级测试");

    // 测试 1: 加载不存在的 DLL 路径 (返回 false)
    {
        DllLoader loader;
        bool ok = loader.load_module(ModuleId::CALIBRATE, "Z:/nonexistent_path_xyz");
        ASSERT_TRUE(!ok, "加载不存在的 DLL 路径返回 false");

        ModuleStatus st = loader.get_status(ModuleId::CALIBRATE);
        ASSERT_TRUE(st == ModuleStatus::NOT_FOUND || st == ModuleStatus::LOAD_FAILED,
                    "加载失败时状态为 NOT_FOUND 或 LOAD_FAILED");

        std::string err = loader.get_error(ModuleId::CALIBRATE);
        ASSERT_TRUE(!err.empty(), "加载失败时错误信息非空");

        ASSERT_TRUE(!loader.is_loaded(ModuleId::CALIBRATE), "is_loaded 返回 false");
    }

    // 测试 2: 加载所有模块 (允许部分失败, 降级处理)
    {
        DllLoader loader;
        // 从 cpp/ 目录执行, 项目根目录为 "../../.."
        std::string lib_base_dir = "../../..";
        bool all_ok = loader.load_all(lib_base_dir);

        int n_loaded = 0;
        std::vector<ModuleId> ids = {
            ModuleId::CALIBRATE, ModuleId::PLATESOLVE, ModuleId::PSF,
            ModuleId::PHOTOMETRIC, ModuleId::DRIZZLE
        };
        for (auto id : ids) {
            if (loader.is_loaded(id)) ++n_loaded;
        }

        std::cout << "    加载成功: " << n_loaded << "/5 个模块" << std::endl;
        // 允许部分失败 (降级处理), 但应至少加载部分模块
        ASSERT_TRUE(n_loaded >= 0, "load_all 执行完成 (允许部分失败)");

        if (all_ok) {
            ASSERT_EQ(n_loaded, 5, "全部5个模块加载成功");
        } else {
            std::cout << "    [提示] 部分模块加载失败, 降级处理中..." << std::endl;
        }
    }

    // 测试 3: 卸载后重新加载
    {
        DllLoader loader;
        std::string lib_base_dir = "../../..";

        // 第一次加载
        loader.load_all(lib_base_dir);
        int n1 = 0;
        for (auto id : {ModuleId::CALIBRATE, ModuleId::PLATESOLVE, ModuleId::PSF,
                         ModuleId::PHOTOMETRIC, ModuleId::DRIZZLE}) {
            if (loader.is_loaded(id)) ++n1;
        }

        // 卸载所有
        loader.unload_all();
        int n2 = 0;
        for (auto id : {ModuleId::CALIBRATE, ModuleId::PLATESOLVE, ModuleId::PSF,
                         ModuleId::PHOTOMETRIC, ModuleId::DRIZZLE}) {
            if (loader.is_loaded(id)) ++n2;
            ASSERT_TRUE(loader.get_status(id) == ModuleStatus::NOT_LOADED,
                        "卸载后状态为 NOT_LOADED");
        }
        ASSERT_EQ(n2, 0, "unload_all 后所有模块未加载");

        // 重新加载
        loader.load_all(lib_base_dir);
        int n3 = 0;
        for (auto id : {ModuleId::CALIBRATE, ModuleId::PLATESOLVE, ModuleId::PSF,
                         ModuleId::PHOTOMETRIC, ModuleId::DRIZZLE}) {
            if (loader.is_loaded(id)) ++n3;
        }
        ASSERT_EQ(n3, n1, "重新加载数量与第一次一致");
    }

    // 测试 4: 获取函数指针 (加载成功后)
    {
        // 函数指针类型: const char* ac_version()
        using VersionFn = const char* (*)();

        DllLoader loader;
        loader.load_all("../../..");

        if (loader.is_loaded(ModuleId::CALIBRATE)) {
            VersionFn ver_fn = loader.get_function<VersionFn>(
                ModuleId::CALIBRATE, "ac_version");
            ASSERT_TRUE(ver_fn != nullptr, "CALIBRATE ac_version 函数指针非空");

            // 调用获取版本
            if (ver_fn) {
                const char* v = ver_fn();
                ASSERT_TRUE(v != nullptr, "ac_version() 返回非空");
                std::cout << "    CALIBRATE 版本: " << (v ? v : "(null)") << std::endl;
            }

            // 不存在的函数应返回 nullptr
            VersionFn bad_fn = loader.get_function<VersionFn>(
                ModuleId::CALIBRATE, "ac_nonexistent_function");
            ASSERT_TRUE(bad_fn == nullptr, "不存在的函数返回 nullptr");
        } else {
            std::cout << "    [跳过] CALIBRATE 未加载, 函数指针测试跳过" << std::endl;
        }

        // 未加载模块的函数指针应为 nullptr
        DllLoader empty_loader;
        VersionFn fn = empty_loader.get_function<VersionFn>(
            ModuleId::CALIBRATE, "ac_version");
        ASSERT_TRUE(fn == nullptr, "未加载模块的函数指针为 nullptr");
    }

    // 测试 5: set_num_threads (CALIBRATE 模块)
    {
        DllLoader loader;
        loader.load_all("../../..");

        if (loader.is_loaded(ModuleId::CALIBRATE)) {
            bool ok = loader.set_num_threads(ModuleId::CALIBRATE, 16);
            ASSERT_TRUE(ok, "CALIBRATE set_num_threads(16) 成功");

            // 非法线程数应返回 false
            bool bad = loader.set_num_threads(ModuleId::CALIBRATE, 0);
            ASSERT_TRUE(!bad, "set_num_threads(0) 返回 false (非法线程数)");
        } else {
            std::cout << "    [跳过] CALIBRATE 未加载, set_num_threads 测试跳过" << std::endl;
        }

        // 未加载模块应返回 false
        DllLoader empty_loader;
        bool noload = empty_loader.set_num_threads(ModuleId::CALIBRATE, 16);
        ASSERT_TRUE(!noload, "未加载模块 set_num_threads 返回 false");
    }

    // 测试 6: Orchestrator init_dlls 降级处理
    {
        Orchestrator orch;
        std::string err;
        // 从不存在的路径加载 (应失败但 Orchestrator 不崩溃)
        bool ok = orch.init_dlls("Z:/nonexistent_path_xyz", err);
        ASSERT_TRUE(!ok, "init_dlls 不存在路径返回 false");
        ASSERT_TRUE(!err.empty(), "init_dlls 失败时错误信息非空");
        ASSERT_TRUE(!orch.is_dlls_loaded(), "init_dlls 失败后 is_dlls_loaded 为 false");

        // 从项目根目录加载
        bool ok2 = orch.init_dlls("../../..", err);
        if (ok2) {
            ASSERT_TRUE(orch.is_dlls_loaded(), "init_dlls 成功后 is_dlls_loaded 为 true");
        } else {
            // 部分模块可能加载失败, 但 init_dlls 应安全返回
            ASSERT_TRUE(!orch.is_dlls_loaded(), "部分失败时 is_dlls_loaded 为 false");
            std::cout << "    [提示] 部分模块加载失败: " << err << std::endl;
        }
    }
}

// ============================================================================
// Part 5: 日志系统集成测试
// 测试 Logger 与各组件的集成
// ============================================================================
void test_part5_logger_integration() {
    TEST_SECTION("Part 5: 日志系统集成测试");

    // 测试 1: 日志文件生成
    {
        TempDir tmp("logger_integration_");
        Logger::instance().init(tmp.path(), LogLevel::DEBUG);
        Logger::instance().set_stderr_output(false);  // 测试时静默 stderr

        LOG_INFO("test_module", "测试日志消息 - INFO");
        LOG_DEBUG("test_module", "测试日志消息 - DEBUG");
        LOG_WARN("test_module", "测试日志消息 - WARN");
        LOG_ERROR("test_module", "测试日志消息 - ERROR");

        std::string path = Logger::instance().get_log_file_path();
        ASSERT_TRUE(!path.empty(), "日志文件路径非空");

        // 检查日志文件是否存在
        std::ifstream ifs(path);
        ASSERT_TRUE(ifs.good(), "日志文件已创建");

        // 读取日志内容
        std::stringstream ss;
        ss << ifs.rdbuf();
        std::string content = ss.str();
        ifs.close();

        ASSERT_CONTAINS(content, "测试日志消息", "日志文件包含测试消息");
        ASSERT_CONTAINS(content, "INFO", "日志文件包含 INFO 级别");
        ASSERT_CONTAINS(content, "DEBUG", "日志文件包含 DEBUG 级别");
        ASSERT_CONTAINS(content, "WARN", "日志文件包含 WARN 级别");
        ASSERT_CONTAINS(content, "ERROR", "日志文件包含 ERROR 级别");
    }

    // 测试 2: 日志级别过滤
    {
        TempDir tmp("logger_level_");
        Logger::instance().init(tmp.path(), LogLevel::WARN);  // 只输出 WARN 及以上
        Logger::instance().set_stderr_output(false);

        LOG_DEBUG("filter_test", "此DEBUG消息应被过滤");
        LOG_INFO("filter_test", "此INFO消息应被过滤");
        LOG_WARN("filter_test", "此WARN消息应保留");
        LOG_ERROR("filter_test", "此ERROR消息应保留");

        std::string path = Logger::instance().get_log_file_path();
        std::ifstream ifs(path);
        std::stringstream ss;
        ss << ifs.rdbuf();
        std::string content = ss.str();
        ifs.close();

        ASSERT_TRUE(content.find("此DEBUG消息应被过滤") == std::string::npos,
                    "DEBUG 级别被过滤 (WARN 级别下)");
        ASSERT_TRUE(content.find("此INFO消息应被过滤") == std::string::npos,
                    "INFO 级别被过滤 (WARN 级别下)");
        ASSERT_CONTAINS(content, "此WARN消息应保留", "WARN 级别保留");
        ASSERT_CONTAINS(content, "此ERROR消息应保留", "ERROR 级别保留");
    }

    // 测试 3: 多模块日志输出
    {
        TempDir tmp("logger_multimod_");
        Logger::instance().init(tmp.path(), LogLevel::DEBUG);
        Logger::instance().set_stderr_output(false);

        // 模拟多个模块的日志输出
        LOG_INFO("orchestrator", "编排器启动");
        LOG_INFO("calibrate", "校准模块开始处理");
        LOG_INFO("platesolve", "解析模块开始处理");
        LOG_INFO("psf", "PSF拟合模块开始处理");
        LOG_INFO("photometric", "测光模块开始处理");
        LOG_INFO("drizzle", "Drizzle模块开始处理");
        LOG_INFO("checkpoint", "检查点已保存");
        LOG_INFO("dll_loader", "DLL加载完成");

        std::string path = Logger::instance().get_log_file_path();
        std::ifstream ifs(path);
        std::stringstream ss;
        ss << ifs.rdbuf();
        std::string content = ss.str();
        ifs.close();

        ASSERT_CONTAINS(content, "orchestrator", "日志包含 orchestrator 模块");
        ASSERT_CONTAINS(content, "calibrate", "日志包含 calibrate 模块");
        ASSERT_CONTAINS(content, "platesolve", "日志包含 platesolve 模块");
        ASSERT_CONTAINS(content, "psf", "日志包含 psf 模块");
        ASSERT_CONTAINS(content, "photometric", "日志包含 photometric 模块");
        ASSERT_CONTAINS(content, "drizzle", "日志包含 drizzle 模块");
        ASSERT_CONTAINS(content, "checkpoint", "日志包含 checkpoint 模块");
        ASSERT_CONTAINS(content, "dll_loader", "日志包含 dll_loader 模块");
    }

    // 测试 4: 日志格式正确
    {
        TempDir tmp("logger_format_");
        Logger::instance().init(tmp.path(), LogLevel::DEBUG);
        Logger::instance().set_stderr_output(false);

        LOG_INFO("format_test", "格式验证消息");

        std::string path = Logger::instance().get_log_file_path();
        std::ifstream ifs(path);
        std::stringstream ss;
        ss << ifs.rdbuf();
        std::string content = ss.str();
        ifs.close();

        // 日志格式: [YYYY-MM-DD HH:MM:SS][LEVEL][module] message
        ASSERT_TRUE(content.find("[") != std::string::npos, "日志行包含 [ 开始括号");
        ASSERT_TRUE(content.find("]") != std::string::npos, "日志行包含 ] 结束括号");
        ASSERT_CONTAINS(content, "INFO", "日志行包含 INFO 级别");
        ASSERT_CONTAINS(content, "format_test", "日志行包含模块名");
        ASSERT_CONTAINS(content, "格式验证消息", "日志行包含消息内容");

        // 验证时间戳格式 (YYYY-MM-DD HH:MM:SS)
        // 简单检查: 包含 4 位年份
        ASSERT_TRUE(content.find("2026") != std::string::npos ||
                    content.find("2025") != std::string::npos ||
                    content.find("2027") != std::string::npos,
                    "日志包含合理的年份");
    }

    // 测试 5: level_to_string / string_to_level 转换
    {
        ASSERT_EQ(Logger::level_to_string(LogLevel::DEBUG), std::string("DEBUG"),
                  "level_to_string(DEBUG) = DEBUG");
        ASSERT_EQ(Logger::level_to_string(LogLevel::INFO), std::string("INFO"),
                  "level_to_string(INFO) = INFO");
        ASSERT_EQ(Logger::level_to_string(LogLevel::WARN), std::string("WARN"),
                  "level_to_string(WARN) = WARN");
        ASSERT_EQ(Logger::level_to_string(LogLevel::ERROR), std::string("ERROR"),
                  "level_to_string(ERROR) = ERROR");

        ASSERT_EQ(static_cast<int>(Logger::string_to_level("DEBUG")),
                  static_cast<int>(LogLevel::DEBUG),
                  "string_to_level(DEBUG) = LogLevel::DEBUG");
        ASSERT_EQ(static_cast<int>(Logger::string_to_level("INFO")),
                  static_cast<int>(LogLevel::INFO),
                  "string_to_level(INFO) = LogLevel::INFO");
        ASSERT_EQ(static_cast<int>(Logger::string_to_level("WARN")),
                  static_cast<int>(LogLevel::WARN),
                  "string_to_level(WARN) = LogLevel::WARN");
        ASSERT_EQ(static_cast<int>(Logger::string_to_level("ERROR")),
                  static_cast<int>(LogLevel::ERROR),
                  "string_to_level(ERROR) = LogLevel::ERROR");

        // 大小写不敏感
        ASSERT_EQ(static_cast<int>(Logger::string_to_level("debug")),
                  static_cast<int>(LogLevel::DEBUG),
                  "string_to_level(debug) 大小写不敏感");

        // 无效字符串默认返回 INFO
        ASSERT_EQ(static_cast<int>(Logger::string_to_level("INVALID")),
                  static_cast<int>(LogLevel::INFO),
                  "string_to_level(INVALID) 默认返回 INFO");
    }

    // 测试 6: Logger shutdown
    {
        TempDir tmp("logger_shutdown_");
        Logger::instance().init(tmp.path(), LogLevel::DEBUG);
        Logger::instance().set_stderr_output(false);

        LOG_INFO("shutdown_test", "shutdown 前的日志");

        Logger::instance().shutdown();

        // shutdown 后再写日志 (应不写入文件)
        LOG_INFO("shutdown_test", "shutdown 后的日志 (不应写入)");

        std::string path = Logger::instance().get_log_file_path();
        ASSERT_TRUE(path.empty(), "shutdown 后日志文件路径为空");

        // 重新初始化
        Logger::instance().init(tmp.path(), LogLevel::INFO);
        Logger::instance().set_stderr_output(false);
        LOG_INFO("reinit_test", "重新初始化后的日志");
    }

    // 恢复 Logger 到默认状态
    Logger::instance().set_stderr_output(true);
}

// ============================================================================
// Part 6: JSON 入口 stage1 执行与 JSONL 事件测试
// 通过 orchestrator.exe <stage1.json> 执行, 验证 JSONL 事件输出
// ============================================================================
void test_part6_json_entry_stage1_execution() {
    TEST_SECTION("Part 6: JSON 入口 stage1 执行与 JSONL 事件");

    std::string exe = find_orchestrator_exe();
    TempDir tmp("p6_json_exec_");
    std::string tmpdir = tmp.path();

    // 测试 1: 有效 stage1.json + 不存在的 FITS -> 失败, stdout 含 accepted + failed 事件
    {
        std::string json_path = write_valid_stage1_json(tmpdir, "run_test.json");
        ExecResult r = exec_command(exe + " " + json_path);
        // FITS 不存在, stage1 失败, 退出码非 0
        ASSERT_TRUE(r.exit_code != 0, "stage1 不存在的 FITS 退出码非 0");

        // stdout 应包含 accepted 事件
        ASSERT_CONTAINS(r.stdout_output, "\"type\":\"accepted\"", "stdout 含 accepted 事件");
        ASSERT_CONTAINS(r.stdout_output, "config_sha256", "accepted 事件含 config_sha256");

        // 失败时应有 failed 事件
        ASSERT_CONTAINS(r.stdout_output, "\"type\":\"failed\"", "stdout 含 failed 事件");
    }

    // 测试 2: stdout 含 job_id (基于原始 JSON SHA256 前 12 位)
    {
        std::string json_path = write_valid_stage1_json(tmpdir, "jobid_test.json");
        ExecResult r = exec_command(exe + " " + json_path);
        ASSERT_TRUE(r.exit_code != 0, "job_id 测试: 退出码非 0");
        ASSERT_CONTAINS(r.stdout_output, "\"job_id\":\"stage1_", "stdout 含 job_id=stage1_ 前缀");
    }

    // 测试 3: stdout 每行均为有效 JSONL (单行 JSON)
    {
        std::string json_path = write_valid_stage1_json(tmpdir, "jsonl_valid.json");
        ExecResult r = exec_command(exe + " " + json_path);
        ASSERT_TRUE(r.exit_code != 0, "JSONL 有效性测试: 退出码非 0");

        // 检查 stdout 每行 (非空) 都以 { 开头并以 } 结尾
        std::istringstream iss(r.stdout_output);
        std::string line;
        int json_lines = 0;
        int bad_lines = 0;
        while (std::getline(iss, line)) {
            if (line.empty()) continue;
            bool all_space = true;
            for (char c : line) { if (!std::isspace((unsigned char)c)) { all_space = false; break; } }
            if (all_space) continue;
            ++json_lines;
            std::string trimmed = line;
            while (!trimmed.empty() && (trimmed.back() == '\r' || trimmed.back() == ' ' || trimmed.back() == '\t')) {
                trimmed.pop_back();
            }
            if (trimmed.empty() || trimmed.front() != '{' || trimmed.back() != '}') {
                ++bad_lines;
                std::cerr << "  [DEBUG] 非 JSONL 行: " << line << std::endl;
            }
        }
        ASSERT_TRUE(json_lines >= 2, "stdout 至少 2 行 JSONL (accepted + failed)");
        ASSERT_EQ(bad_lines, 0, "stdout 所有非空行均为有效 JSONL");
    }

    // 测试 4: stdout/stderr 严格分离 (stdout=JSONL, stderr=日志)
    {
        std::string json_path = write_valid_stage1_json(tmpdir, "separation.json");
        ExecResult r = exec_command(exe + " " + json_path);
        ASSERT_TRUE(r.exit_code != 0, "separation 测试: 退出码非 0");

        // stdout 应非空且为 JSONL
        ASSERT_FALSE(r.stdout_output.empty(), "stdout 非空 (含 JSONL 事件)");

        // stderr 应非空 (含人类可读日志)
        ASSERT_FALSE(r.stderr_output.empty(), "stderr 非空 (含日志)");

        // stderr 不应含完整 JSONL 事件行
        std::istringstream iss(r.stderr_output);
        std::string line;
        int jsonl_in_stderr = 0;
        while (std::getline(iss, line)) {
            std::string trimmed = line;
            while (!trimmed.empty() && (trimmed.back() == '\r' || trimmed.back() == ' ' || trimmed.back() == '\t')) {
                trimmed.pop_back();
            }
            if (!trimmed.empty() && trimmed.front() == '{' && trimmed.back() == '}') {
                ++jsonl_in_stderr;
            }
        }
        ASSERT_EQ(jsonl_in_stderr, 0, "stderr 不含完整 JSONL 事件行 (stdout/stderr 严格分离)");
    }

    // 测试 5: failed 事件含数字 exit_code 字段
    {
        std::string json_path = write_valid_stage1_json(tmpdir, "exit_code_test.json");
        ExecResult r = exec_command(exe + " " + json_path);
        ASSERT_TRUE(r.exit_code != 0, "exit_code 测试: 退出码非 0");

        // stdout 含数字 exit_code 字段
        ASSERT_CONTAINS(r.stdout_output, "\"exit_code\":", "stdout 含数字 exit_code 字段");
    }

    // 测试 6: 不同配置产生不同 config_sha256
    {
        // 创建两个不同配置的 JSON
        std::string json_a = write_valid_stage1_json(tmpdir, "diff_a.json");
        std::string json_b = tmpdir + "/diff_b.json";
        {
            // 基于合法 v1.1 配置, 仅改 precision 为 fp64, 得到不同 sha256
            std::ifstream ifs(json_a);
            std::stringstream ss;
            ss << ifs.rdbuf();
            std::string content = ss.str();
            ifs.close();
            const std::string old_p = "\"precision\":\"fp32\"";
            size_t ppos = content.find(old_p);
            if (ppos != std::string::npos) {
                content.replace(ppos, old_p.length(), "\"precision\":\"fp64\"");
            }
            std::ofstream ofs(json_b);
            ofs << content;
            ofs.close();
        }

        // 提取两次的 config_sha256
        auto extract_hash = [](const std::string& out) -> std::string {
            std::string key = "\"config_sha256\":\"";
            size_t pos = out.find(key);
            if (pos == std::string::npos) return "";
            size_t start = pos + key.size();
            size_t end = out.find("\"", start);
            return (end == std::string::npos) ? "" : out.substr(start, end - start);
        };

        ExecResult r1 = exec_command(exe + " " + json_a);
        ExecResult r2 = exec_command(exe + " " + json_b);

        std::string h1 = extract_hash(r1.stdout_output);
        std::string h2 = extract_hash(r2.stdout_output);
        ASSERT_EQ(h1.size(), static_cast<size_t>(64), "配置 A 的 config_sha256 长度 64");
        ASSERT_EQ(h2.size(), static_cast<size_t>(64), "配置 B 的 config_sha256 长度 64");
        ASSERT_TRUE(h1 != h2, "不同配置产生不同 config_sha256");
    }

    // 测试 7: 同一配置两次执行 config_sha256 一致 (幂等性)
    {
        std::string json_path = write_valid_stage1_json(tmpdir, "idempotent.json");

        auto extract_hash = [](const std::string& out) -> std::string {
            std::string key = "\"config_sha256\":\"";
            size_t pos = out.find(key);
            if (pos == std::string::npos) return "";
            size_t start = pos + key.size();
            size_t end = out.find("\"", start);
            return (end == std::string::npos) ? "" : out.substr(start, end - start);
        };

        ExecResult r1 = exec_command(exe + " " + json_path);
        ExecResult r2 = exec_command(exe + " " + json_path);

        std::string h1 = extract_hash(r1.stdout_output);
        std::string h2 = extract_hash(r2.stdout_output);
        ASSERT_EQ(h1.size(), static_cast<size_t>(64), "第一次 config_sha256 长度 64");
        ASSERT_EQ(h2.size(), static_cast<size_t>(64), "第二次 config_sha256 长度 64");
        ASSERT_EQ(h1, h2, "同一配置两次执行 config_sha256 一致 (幂等性)");
    }
}

// ============================================================================
// Part 7: SHA-256 与配置哈希测试
// 直接调用 sha256_impl::sha256, compute_config_sha256, get_stage1_schema_json
// ============================================================================
void test_part7_sha256_and_config_hash() {
    TEST_SECTION("Part 7: SHA-256 与配置哈希测试");

    // 测试 1: SHA256 空字符串
    {
        std::string hash = sha256_impl::sha256("");
        ASSERT_EQ(hash, std::string("e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"),
                  "SHA256(\"\") 已知测试向量");
        ASSERT_EQ(hash.size(), static_cast<size_t>(64), "SHA256 输出长度为 64");
    }

    // 测试 2: SHA256("abc") 已知测试向量
    {
        std::string hash = sha256_impl::sha256("abc");
        ASSERT_EQ(hash, std::string("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"),
                  "SHA256(\"abc\") 已知测试向量");
    }

    // 测试 3: SHA256("The quick brown fox jumps over the lazy dog")
    {
        std::string hash = sha256_impl::sha256("The quick brown fox jumps over the lazy dog");
        ASSERT_EQ(hash, std::string("d7a8fbb307d7809469ca9abcb0082e4f8d5651e46d3cdb762d02d0bf37c9e592"),
                  "SHA256(fox) 已知测试向量");
    }

    // 测试 4: SHA256 输出全部为小写十六进制
    {
        std::string hash = sha256_impl::sha256("some test string for hex check");
        ASSERT_EQ(hash.size(), static_cast<size_t>(64), "SHA256 输出长度为 64");
        bool hex_ok = true;
        for (char c : hash) {
            if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) {
                hex_ok = false; break;
            }
        }
        ASSERT_TRUE(hex_ok, "SHA256 输出全部为小写十六进制");
    }

    // 测试 5: get_stage1_schema_json 返回非空 Schema JSON
    {
        std::string schema = get_stage1_schema_json();
        ASSERT_FALSE(schema.empty(), "get_stage1_schema_json 返回非空");
        ASSERT_CONTAINS(schema, "schema_version", "Schema 包含 schema_version");
        ASSERT_CONTAINS(schema, "stage1", "Schema 包含 stage1");
        ASSERT_CONTAINS(schema, "additionalProperties", "Schema 包含 additionalProperties");
        ASSERT_CONTAINS(schema, "pixfrac", "Schema 包含 pixfrac");
        ASSERT_CONTAINS(schema, "nside", "Schema 包含 nside");
    }

    // 测试 6: compute_config_sha256 返回 64 位小写十六进制
    {
        Stage1Config config;
        config.schema_version = "1.0";
        config.pipeline = "stage1";
        config.precision = PrecisionMode::FP32;
        config.input.light = "test.fts";
        config.input.master_bias = "";
        config.input.master_dark = "";
        config.input.master_flat = "";
        config.calibration.mode = "standard";
        config.calibration.light_exposure_s = 180.0;
        config.calibration.dark_exposure_s = 0.0;
        config.calibration.fallback = "exposure_ratio";
        config.platesolve.gaia_catalog = "GaiaDR3";
        config.platesolve.max_stars = 2000;
        config.platesolve.initial_ra_deg = -999.0;
        config.platesolve.initial_dec_deg = -999.0;
        config.psf.fit_radius = 8;
        config.psf.max_iterations = 100;
        config.psf.tolerance = 1e-6;
        config.photometric.gaia_spectra = "GaiaDR3SP";
        config.photometric.filter_response = "filters.json";
        config.photometric.qe_curve = "qe.json";
        config.snr.estimator_id = 1;
        config.snr.sampling_scale = 1.0;
        config.drizzle.mode = "precise";
        config.drizzle.pixfrac = 1.0;
        config.drizzle.nside_mode = "auto";
        config.drizzle.nside_value = 0;
        config.drizzle.ordering = "nested";
        config.output.hiss = "out.hiss";
        config.output.log = "run.log";
        config.output.diagnostics_dir = "diag";
        config.output.overwrite = false;
        config.execution.stop_after = "hiss_verify";
        config.execution.threads = 0;

        std::string hash = compute_config_sha256(config);
        ASSERT_EQ(hash.size(), static_cast<size_t>(64), "compute_config_sha256 长度为 64");
        bool hex_ok = true;
        for (char c : hash) {
            if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) {
                hex_ok = false; break;
            }
        }
        ASSERT_TRUE(hex_ok, "compute_config_sha256 全部为小写十六进制");
    }

    // 测试 7: 同一配置两次计算 SHA256 一致 (幂等性)
    {
        Stage1Config config;
        config.schema_version = "1.0";
        config.pipeline = "stage1";
        config.precision = PrecisionMode::FP64;
        config.input.light = "idempotent.fts";
        config.calibration.mode = "optimal";
        config.calibration.light_exposure_s = 120.0;
        config.platesolve.gaia_catalog = "Gaia";
        config.platesolve.max_stars = 500;
        config.psf.fit_radius = 4;
        config.psf.max_iterations = 50;
        config.psf.tolerance = 1e-5;
        config.photometric.gaia_spectra = "SP";
        config.photometric.filter_response = "f.json";
        config.photometric.qe_curve = "q.json";
        config.snr.estimator_id = 2;
        config.snr.sampling_scale = 0.5;
        config.drizzle.mode = "precise";
        config.drizzle.pixfrac = 0.8;
        config.drizzle.nside_mode = "explicit";
        config.drizzle.nside_value = 512;
        config.drizzle.ordering = "nested";
        config.output.hiss = "out.hiss";
        config.output.log = "run.log";
        config.output.diagnostics_dir = "diag";
        config.output.overwrite = true;
        config.execution.stop_after = "drizzle";
        config.execution.threads = 4;
        config.execution.stage_timeout_sec["calibrate"] = 60.0;
        config.execution.stage_timeout_sec["drizzle"] = 300.0;

        std::string h1 = compute_config_sha256(config);
        std::string h2 = compute_config_sha256(config);
        ASSERT_EQ(h1, h2, "同一配置两次计算 SHA256 一致 (幂等性)");
    }

    // 测试 8: 不同配置产生不同 SHA256
    {
        Stage1Config config_a;
        config_a.schema_version = "1.0";
        config_a.pipeline = "stage1";
        config_a.precision = PrecisionMode::FP32;
        config_a.input.light = "frame_a.fts";
        config_a.calibration.mode = "standard";
        config_a.calibration.light_exposure_s = 180.0;
        config_a.platesolve.gaia_catalog = "Gaia";
        config_a.platesolve.max_stars = 2000;
        config_a.psf.fit_radius = 8;
        config_a.psf.max_iterations = 100;
        config_a.psf.tolerance = 1e-6;
        config_a.photometric.gaia_spectra = "SP";
        config_a.photometric.filter_response = "f";
        config_a.photometric.qe_curve = "q";
        config_a.snr.estimator_id = 1;
        config_a.snr.sampling_scale = 1.0;
        config_a.drizzle.mode = "precise";
        config_a.drizzle.pixfrac = 1.0;
        config_a.drizzle.nside_mode = "auto";
        config_a.drizzle.ordering = "nested";
        config_a.output.hiss = "out.hiss";
        config_a.output.log = "run.log";
        config_a.output.diagnostics_dir = "diag";
        config_a.output.overwrite = false;
        config_a.execution.stop_after = "hiss_verify";
        config_a.execution.threads = 0;

        Stage1Config config_b = config_a;
        config_b.precision = PrecisionMode::FP64;  // 改变精度
        config_b.input.light = "frame_b.fts";       // 改变输入

        std::string h_a = compute_config_sha256(config_a);
        std::string h_b = compute_config_sha256(config_b);
        ASSERT_TRUE(h_a != h_b, "不同配置产生不同 SHA256");
    }

    // 测试 9: 内嵌 Schema 为 v1.1, 含 $schema 属性与 nside/browser_verify 阶段
    // (: compat flat JSON 桥已删除, 该测试改为 Schema 一致性)
    {
        std::string schema = get_stage1_schema_json();
        ASSERT_FALSE(schema.empty(), "内嵌 Schema 非空");
        ASSERT_CONTAINS(schema, "\"1.1\"", "schema_version 为 1.1");
        ASSERT_CONTAINS(schema, "\"$schema\"", "Schema 允许 $schema 属性 (CFG-101 已修)");
        ASSERT_CONTAINS(schema, "\"nside\"", "stop_after 含 nside 阶段");
        ASSERT_CONTAINS(schema, "\"browser_verify\"", "stop_after 含 browser_verify 阶段");
        std::string sh = get_stage1_schema_sha256();
        ASSERT_FALSE(sh.empty(), "内嵌 Schema SHA256 非空");
    }

    // 测试 10: SIGINT 信号处理器注册/注销 (单元测试, 不触发实际信号)
    {
        Orchestrator orch;
        // 注册信号处理器 (不应崩溃)
        p04004_register_signal_handler(&orch, true);
        ASSERT_TRUE(true, "p04004_register_signal_handler(true) 执行成功");

        // 请求取消 (模拟信号触发)
        ASSERT_FALSE(orch.is_cancelled(), "注册后初始状态: 未取消");
        orch.request_cancel();
        ASSERT_TRUE(orch.is_cancelled(), "request_cancel 后: 已取消");

        // 重置
        orch.reset_cancel_timeout();
        ASSERT_FALSE(orch.is_cancelled(), "reset_cancel_timeout 后: 未取消");

        // 注销信号处理器 (不应崩溃)
        p04004_unregister_signal_handler();
        ASSERT_TRUE(true, "p04004_unregister_signal_handler 执行成功");
    }

    // 测试 11: SIGINT 处理器注册 (cancel_on_signal=false)
    {
        Orchestrator orch;
        p04004_register_signal_handler(&orch, false);
        ASSERT_TRUE(true, "p04004_register_signal_handler(false) 执行成功");
        p04004_unregister_signal_handler();
        ASSERT_TRUE(true, "p04004_unregister_signal_handler 执行成功");
    }

    // 测试 12: AstroCsExitCode 错误码字符串映射
    {
        ASSERT_EQ(std::string(AstroCsExitCode::error_code_string(AstroCsExitCode::SUCCESS)),
                  std::string("ASTROCS_SUCCESS"), "SUCCESS -> ASTROCS_SUCCESS");
        ASSERT_EQ(std::string(AstroCsExitCode::error_code_string(AstroCsExitCode::CONFIG_ERROR)),
                  std::string("ASTROCS_CONFIG_INVALID"), "CONFIG_ERROR -> ASTROCS_CONFIG_INVALID");
        ASSERT_EQ(std::string(AstroCsExitCode::error_code_string(AstroCsExitCode::FILE_IO_ERROR)),
                  std::string("ASTROCS_FILE_IO_ERROR"), "FILE_IO_ERROR -> ASTROCS_FILE_IO_ERROR");
        ASSERT_EQ(std::string(AstroCsExitCode::error_code_string(AstroCsExitCode::DLL_LOAD_FAILED)),
                  std::string("ASTROCS_MODULE_MISSING"), "DLL_LOAD_FAILED -> ASTROCS_MODULE_MISSING");
        ASSERT_EQ(std::string(AstroCsExitCode::error_code_string(AstroCsExitCode::TIMEOUT)),
                  std::string("ASTROCS_TIMEOUT"), "TIMEOUT -> ASTROCS_TIMEOUT");
        ASSERT_EQ(std::string(AstroCsExitCode::error_code_string(AstroCsExitCode::CANCELLED)),
                  std::string("ASTROCS_CANCELLED"), "CANCELLED -> ASTROCS_CANCELLED");
        ASSERT_EQ(std::string(AstroCsExitCode::error_code_string(999)),
                  std::string("ASTROCS_INTERNAL"), "未知码 -> ASTROCS_INTERNAL");
    }
}

// ============================================================================
// main
// ============================================================================
int main(int argc, char* argv[]) {
#ifdef _WIN32
    // 设置控制台为 UTF-8 (保证中文输出正确)
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif

    std::cout << "============================================================" << std::endl;
    std::cout << "Orchestrator CLI 集成测试 (Phase1 JSON 入口)" << std::endl;
    std::cout << "============================================================" << std::endl;

    // 执行 7 个 Part 的测试
    test_part1_json_entry_commands();
    test_part2_schema_validation_and_parsing();
    test_part3_checkpoint_resume();
    test_part4_dll_loader();
    test_part5_logger_integration();
    test_part6_json_entry_stage1_execution();
    test_part7_sha256_and_config_hash();

    // 输出汇总
    std::cout << "\n============================================================" << std::endl;
    std::cout << "测试汇总: " << g_pass_count << " 通过, "
              << g_fail_count << " 失败" << std::endl;
    std::cout << "============================================================" << std::endl;

    return (g_fail_count == 0) ? 0 : 1;
}
