// ============================================================================
// test_checkpoint.cpp - CheckpointManager 单元测试
// 功能: 验证检查点的保存/加载/原子写入/阶段更新/查询/删除/列举/清除/文件名安全/不存在加载
//
// 编译:
// g++ -O2 -std=c++17 -Wall -o tests/test_checkpoint.exe
// tests/test_checkpoint.cpp src/checkpoint.cpp -Iinclude -static
//
// 运行 (任意目录):
// lib\orchestrator\cpp\tests\test_checkpoint.exe
// ============================================================================

#include "checkpoint.h"

#include <iostream>
#include <string>
#include <vector>
#include <filesystem>
#include <cstdio>
#include <chrono>
#include <thread>
#include <sstream>

#ifdef _WIN32
#include <windows.h>
#endif

namespace fs = std::filesystem;

// ============================================================================
// 测试辅助宏
// ============================================================================
static int g_pass_count = 0;
static int g_fail_count = 0;

#define TEST_CHECK(cond, msg)                                                  \
    do {                                                                       \
        if (cond) {                                                            \
            std::cerr << "  [PASS] " << (msg) << std::endl;                    \
            ++g_pass_count;                                                    \
        } else {                                                               \
            std::cerr << "  [FAIL] " << (msg)                                  \
                      << " (line " << __LINE__ << ")" << std::endl;            \
            ++g_fail_count;                                                    \
        }                                                                      \
    } while (0)

#define TEST_SECTION(name)                                                     \
    do {                                                                       \
        std::cerr << "\n========================================================"   \
                  << "\n[测试] " << (name)                                     \
                  << "\n========================================================"   \
                  << std::endl;                                                \
    } while (0)

// ============================================================================
// 辅助: 创建临时目录 (基于时间戳 + PID, 避免冲突)
// ============================================================================
static std::string make_temp_dir(const std::string& prefix = "ckpt_test_") {
    auto now = std::chrono::steady_clock::now().time_since_epoch();
    auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(now).count();
    std::ostringstream oss;
    oss << prefix << ns;
    std::string dir = oss.str();
    std::error_code ec;
    fs::create_directories(dir, ec);
    return dir;
}

// 辅助: 删除目录 (递归)
static void remove_dir(const std::string& dir) {
    std::error_code ec;
    fs::remove_all(dir, ec);
}

// ============================================================================
// 测试 1: 保存和加载检查点
// ============================================================================
void test_save_and_load() {
    TEST_SECTION("测试 1: 保存和加载检查点");

    std::string tmp_dir = make_temp_dir();
    CheckpointManager mgr;
    mgr.set_checkpoint_dir(tmp_dir);

    // 构造测试数据: 2 个已完成阶段
    CheckpointData data;
    data.frame_name = "LDN43_Lum.fts";
    data.fits_path = "/path/to/LDN43_Lum.fts";
    data.current_stage_id = 2;
    data.fully_completed = false;
    data.created_at = "2026-07-13T10:30:00";

    CheckpointStage s0;
    s0.stage_name = "CALIBRATE";
    s0.stage_id = 0;
    s0.duration_sec = 1.5;
    s0.success = true;
    s0.timestamp = "2026-07-13T10:30:00";
    data.stages_completed.push_back(s0);

    CheckpointStage s1;
    s1.stage_name = "PLATESOLVE";
    s1.stage_id = 1;
    s1.duration_sec = 2.1;
    s1.success = true;
    s1.timestamp = "2026-07-13T10:30:02";
    data.stages_completed.push_back(s1);

    data.timings["CALIBRATE"] = 1.5;
    data.timings["PLATESOLVE"] = 2.1;

    // 保存
    bool ok = mgr.save("LDN43_Lum.fts", data);
    TEST_CHECK(ok, "save 应返回 true");

    // 验证文件存在
    TEST_CHECK(mgr.exists("LDN43_Lum.fts"), "exists 应返回 true");

    // 加载并验证字段
    CheckpointData loaded;
    ok = mgr.load("LDN43_Lum.fts", loaded);
    TEST_CHECK(ok, "load 应返回 true");
    TEST_CHECK(loaded.frame_name == "LDN43_Lum.fts", "frame_name 应正确");
    TEST_CHECK(loaded.fits_path == "/path/to/LDN43_Lum.fts", "fits_path 应正确");
    TEST_CHECK(loaded.current_stage_id == 2, "current_stage_id 应为 2");
    TEST_CHECK(loaded.stages_completed.size() == 2, "stages_completed 应有 2 条记录");
    TEST_CHECK(loaded.fully_completed == false, "fully_completed 应为 false");

    if (loaded.stages_completed.size() >= 2) {
        TEST_CHECK(loaded.stages_completed[0].stage_name == "CALIBRATE",
                   "阶段 0 名称应为 CALIBRATE");
        TEST_CHECK(loaded.stages_completed[0].stage_id == 0, "阶段 0 ID 应为 0");
        TEST_CHECK(loaded.stages_completed[0].duration_sec == 1.5,
                   "阶段 0 耗时应为 1.5");
        TEST_CHECK(loaded.stages_completed[0].success == true,
                   "阶段 0 success 应为 true");
        TEST_CHECK(loaded.stages_completed[1].stage_name == "PLATESOLVE",
                   "阶段 1 名称应为 PLATESOLVE");
        TEST_CHECK(loaded.stages_completed[1].stage_id == 1, "阶段 1 ID 应为 1");
        TEST_CHECK(loaded.stages_completed[1].duration_sec == 2.1,
                   "阶段 1 耗时应为 2.1");
    }

    // 验证 timings 映射
    TEST_CHECK(loaded.timings.count("CALIBRATE") > 0, "timings 应包含 CALIBRATE");
    TEST_CHECK(loaded.timings.count("PLATESOLVE") > 0, "timings 应包含 PLATESOLVE");
    if (loaded.timings.count("CALIBRATE") > 0) {
        TEST_CHECK(loaded.timings["CALIBRATE"] == 1.5,
                   "timings[CALIBRATE] 应为 1.5");
    }

    // 清理
    remove_dir(tmp_dir);
}

// ============================================================================
// 测试 2: 原子写入 (检查临时文件被清理)
// ============================================================================
void test_atomic_write() {
    TEST_SECTION("测试 2: 原子写入 (检查临时文件被清理)");

    std::string tmp_dir = make_temp_dir();
    CheckpointManager mgr;
    mgr.set_checkpoint_dir(tmp_dir);

    CheckpointData data;
    data.frame_name = "frame_test";
    data.fits_path = "/test/frame.fits";
    data.current_stage_id = 0;
    data.fully_completed = false;

    bool ok = mgr.save("frame_test", data);
    TEST_CHECK(ok, "save 应返回 true");

    // 检查 .json 文件存在
    fs::path json_path = fs::path(tmp_dir) / "frame_test.json";
    TEST_CHECK(fs::exists(json_path), "目标 .json 文件应存在");

    // 检查 .tmp 文件不存在 (已被 rename 后清理)
    fs::path tmp_path = fs::path(tmp_dir) / "frame_test.json.tmp";
    TEST_CHECK(!fs::exists(tmp_path), ".tmp 临时文件应已被清理");

    // 再次保存 (覆盖), 验证临时文件依然清理干净
    data.current_stage_id = 1;
    ok = mgr.save("frame_test", data);
    TEST_CHECK(ok, "第二次 save 应返回 true");
    TEST_CHECK(fs::exists(json_path), "覆盖后 .json 文件应仍存在");
    TEST_CHECK(!fs::exists(tmp_path), "覆盖后 .tmp 临时文件应仍被清理");

    // 清理
    remove_dir(tmp_dir);
}

// ============================================================================
// 测试 3: 更新阶段状态
// ============================================================================
void test_update_stage() {
    TEST_SECTION("测试 3: 更新阶段状态");

    std::string tmp_dir = make_temp_dir();
    CheckpointManager mgr;
    mgr.set_checkpoint_dir(tmp_dir);

    // 初始: 检查点不存在
    TEST_CHECK(!mgr.exists("M42.fts"), "初始时检查点应不存在");

    // 添加阶段 0 (CALIBRATE)
    bool ok = mgr.update_stage("M42.fts", 0, "CALIBRATE", 1.2, true);
    TEST_CHECK(ok, "update_stage 阶段 0 应返回 true");
    TEST_CHECK(mgr.exists("M42.fts"), "添加阶段后检查点应存在");

    // 添加阶段 1 (PLATESOLVE)
    ok = mgr.update_stage("M42.fts", 1, "PLATESOLVE", 3.4, true);
    TEST_CHECK(ok, "update_stage 阶段 1 应返回 true");

    // 加载验证
    CheckpointData loaded;
    ok = mgr.load("M42.fts", loaded);
    TEST_CHECK(ok, "load 应返回 true");
    TEST_CHECK(loaded.stages_completed.size() == 2,
               "stages_completed 应有 2 条记录");
    TEST_CHECK(loaded.current_stage_id == 2, "current_stage_id 应为 2");

    // 测试覆盖已存在的阶段 (同 stage_id)
    ok = mgr.update_stage("M42.fts", 0, "CALIBRATE", 9.9, true);
    TEST_CHECK(ok, "覆盖阶段 0 应返回 true");
    ok = mgr.load("M42.fts", loaded);
    TEST_CHECK(loaded.stages_completed.size() == 2,
               "覆盖后 stages_completed 仍应有 2 条记录");
    bool found = false;
    for (const auto& st : loaded.stages_completed) {
        if (st.stage_id == 0) {
            found = true;
            TEST_CHECK(st.duration_sec == 9.9, "阶段 0 耗时应被覆盖为 9.9");
        }
    }
    TEST_CHECK(found, "应找到阶段 0 记录");

    // 清理
    remove_dir(tmp_dir);
}

// ============================================================================
// 测试 4: is_stage_completed
// ============================================================================
void test_is_stage_completed() {
    TEST_SECTION("测试 4: is_stage_completed");

    std::string tmp_dir = make_temp_dir();
    CheckpointManager mgr;
    mgr.set_checkpoint_dir(tmp_dir);

    // 添加阶段 0 和 1 (都成功)
    mgr.update_stage("NGC2264.fts", 0, "CALIBRATE", 1.0, true);
    mgr.update_stage("NGC2264.fts", 1, "PLATESOLVE", 2.0, true);

    TEST_CHECK(mgr.is_stage_completed("NGC2264.fts", 0),
               "阶段 0 应已完成");
    TEST_CHECK(mgr.is_stage_completed("NGC2264.fts", 1),
               "阶段 1 应已完成");
    TEST_CHECK(!mgr.is_stage_completed("NGC2264.fts", 2),
               "阶段 2 应未完成");
    TEST_CHECK(!mgr.is_stage_completed("NGC2264.fts", 3),
               "阶段 3 应未完成");

    // 添加阶段 2 但 success=false
    mgr.update_stage("NGC2264.fts", 2, "PHOTOMETRIC", 0.0, false);
    TEST_CHECK(!mgr.is_stage_completed("NGC2264.fts", 2),
               "阶段 2 success=false 应视为未完成");

    // 不存在的检查点应返回 false
    TEST_CHECK(!mgr.is_stage_completed("nonexistent.fts", 0),
               "不存在的检查点应返回 false");

    // 清理
    remove_dir(tmp_dir);
}

// ============================================================================
// 测试 5: get_resume_stage
// ============================================================================
void test_get_resume_stage() {
    TEST_SECTION("测试 5: get_resume_stage");

    std::string tmp_dir = make_temp_dir();
    CheckpointManager mgr;
    mgr.set_checkpoint_dir(tmp_dir);

    // 不存在的检查点应返回 0
    int stage = mgr.get_resume_stage("nonexistent.fts");
    TEST_CHECK(stage == 0, "不存在的检查点应返回 0");

    // 添加阶段 0
    mgr.update_stage("IC434.fts", 0, "CALIBRATE", 1.0, true);
    stage = mgr.get_resume_stage("IC434.fts");
    TEST_CHECK(stage == 1, "完成阶段 0 后 resume 应为 1");

    // 添加阶段 1
    mgr.update_stage("IC434.fts", 1, "PLATESOLVE", 2.0, true);
    stage = mgr.get_resume_stage("IC434.fts");
    TEST_CHECK(stage == 2, "完成阶段 0/1 后 resume 应为 2");

    // 添加阶段 2
    mgr.update_stage("IC434.fts", 2, "PHOTOMETRIC", 3.0, true);
    stage = mgr.get_resume_stage("IC434.fts");
    TEST_CHECK(stage == 3, "完成阶段 0/1/2 后 resume 应为 3");

    // 添加阶段 3 (全部 4 个阶段完成, current_stage_id >= 4)
    mgr.update_stage("IC434.fts", 3, "DRIZZLE", 4.0, true);
    stage = mgr.get_resume_stage("IC434.fts");
    TEST_CHECK(stage == -1, "全部完成后 resume 应为 -1");

    // 测试中间断点 (有阶段 0 和 2, 缺阶段 1) → 取最大已成功 +1
    // 注: 此情况比较特殊, 主要测试 max+1 的逻辑
    {
        std::string tmp_dir2 = make_temp_dir("ckpt_test_gap_");
        CheckpointManager mgr2;
        mgr2.set_checkpoint_dir(tmp_dir2);

        CheckpointData data;
        data.frame_name = "gap.fts";
        data.fits_path = "/gap.fts";
        data.current_stage_id = 0;
        data.fully_completed = false;
        data.created_at = "2026-07-13T10:00:00";

        CheckpointStage s0;
        s0.stage_name = "CALIBRATE";
        s0.stage_id = 0;
        s0.duration_sec = 1.0;
        s0.success = true;
        s0.timestamp = "2026-07-13T10:00:00";
        data.stages_completed.push_back(s0);

        CheckpointStage s2;
        s2.stage_name = "PHOTOMETRIC";
        s2.stage_id = 2;
        s2.duration_sec = 3.0;
        s2.success = true;
        s2.timestamp = "2026-07-13T10:01:00";
        data.stages_completed.push_back(s2);

        mgr2.save("gap.fts", data);
        int st = mgr2.get_resume_stage("gap.fts");
        TEST_CHECK(st == 3, "max stage_id=2 时 resume 应为 3 (max+1)");

        remove_dir(tmp_dir2);
    }

    // 清理
    remove_dir(tmp_dir);
}

// ============================================================================
// 测试 6: 删除检查点
// ============================================================================
void test_remove() {
    TEST_SECTION("测试 6: 删除检查点");

    std::string tmp_dir = make_temp_dir();
    CheckpointManager mgr;
    mgr.set_checkpoint_dir(tmp_dir);

    // 创建检查点
    mgr.update_stage("frame_to_delete.fts", 0, "CALIBRATE", 1.0, true);
    TEST_CHECK(mgr.exists("frame_to_delete.fts"),
               "创建后 exists 应返回 true");

    // 删除
    bool ok = mgr.remove("frame_to_delete.fts");
    TEST_CHECK(ok, "remove 应返回 true");
    TEST_CHECK(!mgr.exists("frame_to_delete.fts"),
               "删除后 exists 应返回 false");

    // 删除不存在的检查点应返回 false
    ok = mgr.remove("frame_to_delete.fts");
    TEST_CHECK(!ok, "删除不存在的检查点应返回 false");

    // 清理
    remove_dir(tmp_dir);
}

// ============================================================================
// 测试 7: 列出所有检查点
// ============================================================================
void test_list_all() {
    TEST_SECTION("测试 7: 列出所有检查点");

    std::string tmp_dir = make_temp_dir();
    CheckpointManager mgr;
    mgr.set_checkpoint_dir(tmp_dir);

    // 初始应为空
    std::vector<std::string> list = mgr.list_all();
    TEST_CHECK(list.empty(), "初始时 list_all 应为空");

    // 创建 3 个检查点
    mgr.update_stage("frameA.fts", 0, "CALIBRATE", 1.0, true);
    mgr.update_stage("frameB.fts", 0, "CALIBRATE", 2.0, true);
    mgr.update_stage("frameC.fts", 0, "CALIBRATE", 3.0, true);

    list = mgr.list_all();
    TEST_CHECK(list.size() == 3, "创建 3 个后 list_all 应返回 3 条");

    // 验证按字母排序
    bool sorted = true;
    for (size_t i = 1; i < list.size(); ++i) {
        if (list[i - 1] > list[i]) {
            sorted = false;
            break;
        }
    }
    TEST_CHECK(sorted, "list_all 返回结果应按字母顺序排序");

    // 验证具体内容 (list_all 返回 stem, 即去掉 .json 后缀的文件名)
    // 对于 "frameA.fts.json", stem() 返回 "frameA.fts"
    bool hasA = false, hasB = false, hasC = false;
    for (const auto& n : list) {
        if (n == "frameA.fts") hasA = true;
        if (n == "frameB.fts") hasB = true;
        if (n == "frameC.fts") hasC = true;
    }
    TEST_CHECK(hasA && hasB && hasC,
               "list_all 应包含 frameA.fts/frameB.fts/frameC.fts");

    // 删除一个后剩 2 条
    mgr.remove("frameB.fts");
    list = mgr.list_all();
    TEST_CHECK(list.size() == 2, "删除一个后 list_all 应返回 2 条");

    // 清理
    remove_dir(tmp_dir);
}

// ============================================================================
// 测试 8: 清除所有检查点
// ============================================================================
void test_clear_all() {
    TEST_SECTION("测试 8: 清除所有检查点");

    std::string tmp_dir = make_temp_dir();
    CheckpointManager mgr;
    mgr.set_checkpoint_dir(tmp_dir);

    // 创建多个检查点
    mgr.update_stage("clearA.fts", 0, "CALIBRATE", 1.0, true);
    mgr.update_stage("clearB.fts", 0, "CALIBRATE", 2.0, true);
    mgr.update_stage("clearC.fts", 0, "CALIBRATE", 3.0, true);
    TEST_CHECK(mgr.list_all().size() == 3, "创建 3 个检查点");

    // 清除所有
    mgr.clear_all();
    TEST_CHECK(mgr.list_all().empty(), "clear_all 后应无检查点");

    // 验证单个 exists 也返回 false
    TEST_CHECK(!mgr.exists("clearA.fts"), "clear_all 后 clearA 不应存在");
    TEST_CHECK(!mgr.exists("clearB.fts"), "clear_all 后 clearB 不应存在");

    // 再次清除 (空目录应安全无异常)
    mgr.clear_all();
    TEST_CHECK(true, "对空目录 clear_all 应安全无异常");

    // 清理
    remove_dir(tmp_dir);
}

// ============================================================================
// 测试 9: 文件名安全处理 (特殊字符)
// ============================================================================
void test_sanitize_frame_name() {
    TEST_SECTION("测试 9: 文件名安全处理 (特殊字符)");

    std::string tmp_dir = make_temp_dir();
    CheckpointManager mgr;
    mgr.set_checkpoint_dir(tmp_dir);

    // 测试 1: 包含路径分隔符的 frame_name (应只保留文件名部分)
    // Windows 路径
    mgr.update_stage("C:\\data\\subdir\\frame_win.fts", 0, "CALIBRATE", 1.0, true);
    TEST_CHECK(mgr.exists("C:\\data\\subdir\\frame_win.fts"),
               "Windows 路径 frame_name 应能保存检查点");

    // Unix 路径
    mgr.update_stage("/home/user/data/frame_unix.fts", 0, "CALIBRATE", 1.0, true);
    TEST_CHECK(mgr.exists("/home/user/data/frame_unix.fts"),
               "Unix 路径 frame_name 应能保存检查点");

    // 测试 2: 包含特殊字符的 frame_name (应被替换为 _)
    mgr.update_stage("frame:with*special?chars.fts", 0, "CALIBRATE", 1.0, true);
    TEST_CHECK(mgr.exists("frame:with*special?chars.fts"),
               "含特殊字符的 frame_name 应能保存检查点");

    // 验证实际文件名已被 sanitize (列出所有检查点查看)
    std::vector<std::string> list = mgr.list_all();
    bool found_win = false, found_unix = false, found_special = false;
    for (const auto& n : list) {
        // 文件名应不含 \ / : * ? 等特殊字符
        bool has_bad = false;
        for (char c : n) {
            if (c == '\\' || c == '/' || c == ':' || c == '*' ||
                c == '?' || c == '"' || c == '<' || c == '>' || c == '|') {
                has_bad = true;
                break;
            }
        }
        if (has_bad) {
            TEST_CHECK(false, std::string("文件名仍含特殊字符: ") + n);
        }

        if (n == "frame_win.fts") found_win = true;
        if (n == "frame_unix.fts") found_unix = true;
        if (n.find("frame_with_special_chars.fts") != std::string::npos) {
            found_special = true;
        }
    }
    TEST_CHECK(found_win, "Windows 路径应只保留文件名 frame_win.fts");
    TEST_CHECK(found_unix, "Unix 路径应只保留文件名 frame_unix.fts");
    TEST_CHECK(found_special, "特殊字符应被替换为 _");

    // 测试 3: 同名 (sanitize 后) 应覆盖, 不产生多个文件
    // 例如 "data\\frame.fts" 和 "data/frame.fts" sanitize 后都应为 "frame.fts"
    {
        std::string tmp_dir2 = make_temp_dir("ckpt_test_collision_");
        CheckpointManager mgr2;
        mgr2.set_checkpoint_dir(tmp_dir2);

        mgr2.update_stage("data\\frame.fts", 0, "CALIBRATE", 1.0, true);
        mgr2.update_stage("data/frame.fts", 0, "CALIBRATE", 2.0, true);

        std::vector<std::string> list2 = mgr2.list_all();
        TEST_CHECK(list2.size() == 1,
                   "sanitize 后同名的 frame 应只产生 1 个检查点");

        remove_dir(tmp_dir2);
    }

    // 清理
    remove_dir(tmp_dir);
}

// ============================================================================
// 测试 10: 不存在的检查点加载 (返回 false)
// ============================================================================
void test_load_nonexistent() {
    TEST_SECTION("测试 10: 不存在的检查点加载 (返回 false)");

    std::string tmp_dir = make_temp_dir();
    CheckpointManager mgr;
    mgr.set_checkpoint_dir(tmp_dir);

    // load 不存在的检查点应返回 false
    CheckpointData data;
    bool ok = mgr.load("nonexistent_frame.fts", data);
    TEST_CHECK(!ok, "load 不存在的检查点应返回 false");

    // exists 不存在的检查点应返回 false
    TEST_CHECK(!mgr.exists("nonexistent_frame.fts"),
               "exists 不存在的检查点应返回 false");

    // is_stage_completed 不存在的检查点应返回 false
    TEST_CHECK(!mgr.is_stage_completed("nonexistent_frame.fts", 0),
               "is_stage_completed 不存在的检查点应返回 false");

    // remove 不存在的检查点应返回 false
    TEST_CHECK(!mgr.remove("nonexistent_frame.fts"),
               "remove 不存在的检查点应返回 false");

    // get_resume_stage 不存在的检查点应返回 0 (从头开始)
    int stage = mgr.get_resume_stage("nonexistent_frame.fts");
    TEST_CHECK(stage == 0, "get_resume_stage 不存在的检查点应返回 0");

    // 清理
    remove_dir(tmp_dir);
}

// ============================================================================
// 附加测试 11: fully_completed 标记 (覆盖完整性)
// ============================================================================
void test_fully_completed() {
    TEST_SECTION("附加测试 11: fully_completed 标记 (覆盖完整性)");

    std::string tmp_dir = make_temp_dir();
    CheckpointManager mgr;
    mgr.set_checkpoint_dir(tmp_dir);

    // 完成 4 个阶段后应自动标记 fully_completed
    mgr.update_stage("done.fts", 0, "CALIBRATE", 1.0, true);
    mgr.update_stage("done.fts", 1, "PLATESOLVE", 2.0, true);
    mgr.update_stage("done.fts", 2, "PHOTOMETRIC", 3.0, true);

    // 完成 3 个阶段后, fully_completed 应为 false, resume 应为 3
    TEST_CHECK(mgr.get_resume_stage("done.fts") == 3,
               "完成 3 个阶段后 resume 应为 3");

    // 手动构造 fully_completed=true 测试反序列化
    {
        CheckpointData data;
        data.frame_name = "manual.fts";
        data.fits_path = "/manual.fts";
        data.current_stage_id = 4;
        data.fully_completed = true;
        data.created_at = "2026-07-13T10:00:00";

        for (int i = 0; i < 4; ++i) {
            CheckpointStage st;
            st.stage_name = "STAGE" + std::to_string(i);
            st.stage_id = i;
            st.duration_sec = static_cast<double>(i + 1);
            st.success = true;
            st.timestamp = "2026-07-13T10:00:0" + std::to_string(i);
            data.stages_completed.push_back(st);
            data.timings[st.stage_name] = st.duration_sec;
        }

        bool ok = mgr.save("manual.fts", data);
        TEST_CHECK(ok, "save fully_completed=true 应返回 true");

        CheckpointData loaded;
        ok = mgr.load("manual.fts", loaded);
        TEST_CHECK(ok, "load 应返回 true");
        TEST_CHECK(loaded.fully_completed == true,
                   "loaded fully_completed 应为 true");
        TEST_CHECK(mgr.get_resume_stage("manual.fts") == -1,
                   "fully_completed 时 get_resume_stage 应返回 -1");
    }

    // 清理
    remove_dir(tmp_dir);
}

// ============================================================================
// main
// ============================================================================
int main(int argc, char* argv[]) {
#ifdef _WIN32
    // 设置控制台为 UTF-8
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif

    std::cerr << "============================================================" << std::endl;
    std::cerr << "CheckpointManager 单元测试" << std::endl;
    std::cerr << "============================================================" << std::endl;

    test_save_and_load();
    test_atomic_write();
    test_update_stage();
    test_is_stage_completed();
    test_get_resume_stage();
    test_remove();
    test_list_all();
    test_clear_all();
    test_sanitize_frame_name();
    test_load_nonexistent();
    test_fully_completed();

    std::cerr << "\n============================================================" << std::endl;
    std::cerr << "测试汇总: " << g_pass_count << " 通过, "
              << g_fail_count << " 失败" << std::endl;
    std::cerr << "============================================================" << std::endl;

    return (g_fail_count == 0) ? 0 : 1;
}
