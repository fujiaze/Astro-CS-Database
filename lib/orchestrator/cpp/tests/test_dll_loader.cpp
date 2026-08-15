// ============================================================================
// test_dll_loader.cpp - DllLoader 单元测试
// 功能: 验证 DllLoader 类的加载/卸载/状态查询/函数指针获取/版本查询等功能
//
// 编译:
// g++ -O2 -std=c++17 -Wall -o tests/test_dll_loader.exe
// tests/test_dll_loader.cpp src/dll_loader.cpp -Iinclude -static
//
// 运行 (在项目根目录):
// lib\orchestrator\cpp\tests\test_dll_loader.exe
// ============================================================================

#include "dll_loader.h"

#include <iostream>
#include <string>
#include <vector>
#include <cstdint>

#ifdef _WIN32
#include <windows.h>
#endif

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

// 状态名称转换
static const char* status_name(ModuleStatus s) {
    switch (s) {
        case ModuleStatus::NOT_LOADED:  return "NOT_LOADED";
        case ModuleStatus::LOADED:      return "LOADED";
        case ModuleStatus::LOAD_FAILED: return "LOAD_FAILED";
        case ModuleStatus::NOT_FOUND:   return "NOT_FOUND";
        default:                         return "UNKNOWN";
    }
}

// 函数指针类型别名 (用于 get_function<T>)
using VersionFn = const char* (*)();

// 状态判断
static bool err_ok(ModuleStatus s) {
    return s == ModuleStatus::LOADED;
}

// ============================================================================
// 测试 1: 加载不存在的 DLL
// ============================================================================
void test_load_nonexistent_dll() {
    TEST_SECTION("测试 1: 加载不存在的 DLL");

    DllLoader loader;
    // 使用一个明显不存在的 lib_base_dir
    bool ok = loader.load_module(ModuleId::CALIBRATE, "Z:/nonexistent_path_xyz");
    TEST_CHECK(!ok, "加载不存在的 DLL 应返回 false");

    ModuleStatus st = loader.get_status(ModuleId::CALIBRATE);
    TEST_CHECK(st == ModuleStatus::NOT_FOUND,
               std::string("状态应为 NOT_FOUND, 实际=") + status_name(st));

    std::string err = loader.get_error(ModuleId::CALIBRATE);
    TEST_CHECK(!err.empty(), "错误信息应非空");
    std::cerr << "    错误信息: " << err << std::endl;

    TEST_CHECK(!loader.is_loaded(ModuleId::CALIBRATE), "is_loaded 应为 false");
}

// ============================================================================
// 测试 2: 加载所有 5 个模块 (从项目根目录执行)
// 注: lib_base_dir=".." 因为 cpp 目录在 lib/orchestrator/cpp/ 下, 上一级是项目根
// ============================================================================
void test_load_all_modules() {
    TEST_SECTION("测试 2: 加载所有 5 个模块 (lib_base_dir=\"..\")");

    DllLoader loader;
    // cpp 目录在 lib/orchestrator/cpp/, 项目根在 3 级之上
    std::string lib_base_dir = "../../..";
    bool ok = loader.load_all(lib_base_dir);

    // 各模块独立检查 (允许部分失败)
    int n_loaded = 0;
    std::vector<ModuleId> ids = {
        ModuleId::CALIBRATE, ModuleId::PLATESOLVE, ModuleId::PSF,
        ModuleId::PHOTOMETRIC, ModuleId::DRIZZLE
    };
    for (auto id : ids) {
        ModuleStatus st = loader.get_status(id);
        std::cerr << "    模块 " << loader.get_info(id).name
                  << ": status=" << status_name(st);
        if (!err_ok(st)) {
            std::cerr << " err=" << loader.get_error(id);
        }
        std::cerr << std::endl;
        if (st == ModuleStatus::LOADED) ++n_loaded;
    }

    std::cerr << "    加载成功: " << n_loaded << "/5" << std::endl;
    TEST_CHECK(n_loaded == 5, "应加载全部 5 个模块");

    // 显示完整状态
    if (!ok) {
        std::cerr << "    [警告] 部分模块加载失败, 详见上方错误信息" << std::endl;
        std::cerr << "    [提示] 请确保已运行各模块的 make 编译生成 DLL" << std::endl;
    }
}

// ============================================================================
// 测试 3: 获取函数指针
// ============================================================================
void test_get_functions() {
    TEST_SECTION("测试 3: 获取函数指针");

    DllLoader loader;
    std::string lib_base_dir = "../../..";
    loader.load_all(lib_base_dir);

    // CALIBRATE: ac_version / ac_set_num_threads
    if (loader.is_loaded(ModuleId::CALIBRATE)) {
        VersionFn ver_fn = loader.get_function<VersionFn>(
            ModuleId::CALIBRATE, "ac_version");
        TEST_CHECK(ver_fn != nullptr, "CALIBRATE: ac_version 函数指针非空");

        using SetThreadsFn = void (*)(int);
        SetThreadsFn st_fn = loader.get_function<SetThreadsFn>(
            ModuleId::CALIBRATE, "ac_set_num_threads");
        TEST_CHECK(st_fn != nullptr, "CALIBRATE: ac_set_num_threads 函数指针非空");

        // 测试调用
        if (ver_fn) {
            const char* v = ver_fn();
            std::cerr << "    CALIBRATE 版本: "
                      << (v ? v : "(null)") << std::endl;
        }
        if (st_fn) {
            st_fn(16);  // 设置 16 线程
            std::cerr << "    CALIBRATE set_num_threads(16) 调用成功" << std::endl;
        }
    } else {
        std::cerr << "    [跳过] CALIBRATE 未加载" << std::endl;
    }

    // PLATESOLVE: ipv_solve_create / ipv_solve_destroy / ipv_get_default_params
    if (loader.is_loaded(ModuleId::PLATESOLVE)) {
        using CreateFn = void* (*)();
        CreateFn cr_fn = loader.get_function<CreateFn>(
            ModuleId::PLATESOLVE, "ipv_solve_create");
        TEST_CHECK(cr_fn != nullptr, "PLATESOLVE: ipv_solve_create 函数指针非空");

        using DestroyFn = void (*)(void*);
        DestroyFn de_fn = loader.get_function<DestroyFn>(
            ModuleId::PLATESOLVE, "ipv_solve_destroy");
        TEST_CHECK(de_fn != nullptr, "PLATESOLVE: ipv_solve_destroy 函数指针非空");

        // 不存在的函数应返回 nullptr
        VersionFn bad_fn = loader.get_function<VersionFn>(
            ModuleId::PLATESOLVE, "ipv_nonexistent_function");
        TEST_CHECK(bad_fn == nullptr, "PLATESOLVE: 不存在函数应返回 nullptr");
    } else {
        std::cerr << "    [跳过] PLATESOLVE 未加载" << std::endl;
    }

    // PSF: dpsf_fit / dpsf_fit_batch / dpsf_free_results
    if (loader.is_loaded(ModuleId::PSF)) {
        using FitFn = int (*)(const uint16_t*, int, int);
        FitFn fit_fn = loader.get_function<FitFn>(
            ModuleId::PSF, "dpsf_fit");
        TEST_CHECK(fit_fn != nullptr, "PSF: dpsf_fit 函数指针非空");
    } else {
        std::cerr << "    [跳过] PSF 未加载" << std::endl;
    }

    // PHOTOMETRIC: pc_calibrate_simple
    if (loader.is_loaded(ModuleId::PHOTOMETRIC)) {
        using PcFn = int (*)(const float*, int, int);
        PcFn pc_fn = loader.get_function<PcFn>(
            ModuleId::PHOTOMETRIC, "pc_calibrate_simple");
        TEST_CHECK(pc_fn != nullptr, "PHOTOMETRIC: pc_calibrate_simple 函数指针非空");
    } else {
        std::cerr << "    [跳过] PHOTOMETRIC 未加载" << std::endl;
    }

    // DRIZZLE: hp_drizzle_fits_to_ahpx / hp_drizzle_run
    if (loader.is_loaded(ModuleId::DRIZZLE)) {
        using DrFn = int (*)(void*);
        DrFn dr_fn = loader.get_function<DrFn>(
            ModuleId::DRIZZLE, "hp_drizzle_fits_to_ahpx");
        TEST_CHECK(dr_fn != nullptr, "DRIZZLE: hp_drizzle_fits_to_ahpx 函数指针非空");

        DrFn run_fn = loader.get_function<DrFn>(
            ModuleId::DRIZZLE, "hp_drizzle_run");
        TEST_CHECK(run_fn != nullptr, "DRIZZLE: hp_drizzle_run 函数指针非空");
    } else {
        std::cerr << "    [跳过] DRIZZLE 未加载" << std::endl;
    }
}

// ============================================================================
// 测试 4: is_loaded / get_status / get_error
// ============================================================================
void test_state_queries() {
    TEST_SECTION("测试 4: is_loaded / get_status / get_error");

    DllLoader loader;
    // 加载前所有模块应为 NOT_LOADED
    TEST_CHECK(!loader.is_loaded(ModuleId::CALIBRATE), "加载前 CALIBRATE is_loaded=false");
    TEST_CHECK(loader.get_status(ModuleId::CALIBRATE) == ModuleStatus::NOT_LOADED,
               "加载前 CALIBRATE 状态为 NOT_LOADED");
    TEST_CHECK(loader.get_error(ModuleId::CALIBRATE).empty(),
               "加载前 CALIBRATE 错误信息为空");

    // 加载一个模块
    loader.load_module(ModuleId::CALIBRATE, "../../..");
    if (loader.is_loaded(ModuleId::CALIBRATE)) {
        TEST_CHECK(loader.get_status(ModuleId::CALIBRATE) == ModuleStatus::LOADED,
                   "加载后 CALIBRATE 状态为 LOADED");
        TEST_CHECK(loader.get_error(ModuleId::CALIBRATE).empty(),
                   "加载成功后 CALIBRATE 错误信息为空");
    } else {
        // 失败时状态应为 NOT_FOUND 或 LOAD_FAILED
        ModuleStatus st = loader.get_status(ModuleId::CALIBRATE);
        TEST_CHECK(st == ModuleStatus::NOT_FOUND || st == ModuleStatus::LOAD_FAILED,
                   std::string("加载失败时状态为 NOT_FOUND 或 LOAD_FAILED, 实际=") + status_name(st));
        TEST_CHECK(!loader.get_error(ModuleId::CALIBRATE).empty(),
                   "加载失败后错误信息应非空");
    }

    // 未加载模块仍应为 NOT_LOADED
    TEST_CHECK(loader.get_status(ModuleId::DRIZZLE) == ModuleStatus::NOT_LOADED,
               "未加载的 DRIZZLE 状态为 NOT_LOADED");
}

// ============================================================================
// 测试 5: unload_all 后状态
// ============================================================================
void test_unload_all() {
    TEST_SECTION("测试 5: unload_all 后状态");

    DllLoader loader;
    loader.load_all("../../..");

    // unload_all
    loader.unload_all();

    std::vector<ModuleId> ids = {
        ModuleId::CALIBRATE, ModuleId::PLATESOLVE, ModuleId::PSF,
        ModuleId::PHOTOMETRIC, ModuleId::DRIZZLE
    };
    int n_unloaded = 0;
    for (auto id : ids) {
        ModuleStatus st = loader.get_status(id);
        TEST_CHECK(st == ModuleStatus::NOT_LOADED,
                   std::string("unload_all 后 ") + loader.get_info(id).name +
                   " 状态为 NOT_LOADED, 实际=" + status_name(st));
        if (st == ModuleStatus::NOT_LOADED) ++n_unloaded;
        TEST_CHECK(!loader.is_loaded(id),
                   std::string("unload_all 后 ") + loader.get_info(id).name + " is_loaded=false");
    }
    std::cerr << "    卸载成功: " << n_unloaded << "/5" << std::endl;

    // 再卸载一次 (应安全无异常)
    loader.unload_all();
    TEST_CHECK(true, "重复 unload_all 安全无异常");
}

// ============================================================================
// 测试 6: 获取各模块版本信息
// ============================================================================
void test_get_versions() {
    TEST_SECTION("测试 6: 获取各模块版本信息");

    DllLoader loader;
    loader.load_all("../../..");

    std::vector<ModuleId> ids = {
        ModuleId::CALIBRATE, ModuleId::PLATESOLVE, ModuleId::PSF,
        ModuleId::PHOTOMETRIC, ModuleId::DRIZZLE
    };
    for (auto id : ids) {
        std::string name = loader.get_info(id).name;
        std::string ver = loader.get_version(id);
        std::cerr << "    " << name << " 版本: " << ver << std::endl;
        if (loader.is_loaded(id)) {
            TEST_CHECK(!ver.empty(), name + " 版本字符串非空");
        }
    }
}

// ============================================================================
// 测试 7: set_num_threads
// ============================================================================
void test_set_num_threads() {
    TEST_SECTION("测试 7: set_num_threads");

    DllLoader loader;
    loader.load_all("../../..");

    // CALIBRATE 应支持
    if (loader.is_loaded(ModuleId::CALIBRATE)) {
        bool ok = loader.set_num_threads(ModuleId::CALIBRATE, 16);
        TEST_CHECK(ok, "CALIBRATE set_num_threads(16) 成功");
    }

    // 其他模块暂不支持
    bool psf_ok = loader.set_num_threads(ModuleId::PSF, 16);
    TEST_CHECK(!psf_ok, "PSF set_num_threads 应返回 false (暂未实现)");

    // 未加载模块应返回 false
    DllLoader loader2;
    bool noload_ok = loader2.set_num_threads(ModuleId::CALIBRATE, 16);
    TEST_CHECK(!noload_ok, "未加载模块 set_num_threads 应返回 false");
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
    std::cerr << "DllLoader 单元测试" << std::endl;
    std::cerr << "============================================================" << std::endl;

    test_load_nonexistent_dll();
    test_load_all_modules();
    test_get_functions();
    test_state_queries();
    test_unload_all();
    test_get_versions();
    test_set_num_threads();

    std::cerr << "\n============================================================" << std::endl;
    std::cerr << "测试汇总: " << g_pass_count << " 通过, "
              << g_fail_count << " 失败" << std::endl;
    std::cerr << "============================================================" << std::endl;

    return (g_fail_count == 0) ? 0 : 1;
}
