// ============================================================================
// dll_loader.h - 动态 DLL 加载器
// 功能: 运行时通过 LoadLibrary/GetProcAddress 加载 5 个模块 DLL, 提供统一的
//       函数指针获取接口。支持模块状态查询、版本获取、线程数下发等。
// 用途: 编排器 (Orchestrator) 通过本加载器调用各 C++ 模块 DLL。
//
// 设计说明:
//   - 5 个模块: CALIBRATE / PLATESOLVE / PSF / PHOTOMETRIC / DRIZZLE
//   - 模板方法 get_function<T> 允许调用方指定函数指针类型
//   - 加载失败时返回详细错误信息 (FormatMessageA)
//   - 析构时自动 FreeLibrary 释放所有已加载模块
// ============================================================================

#pragma once

#include <string>
#include <map>
#include <memory>

#ifdef _WIN32
#include <windows.h>
#endif

// 模块 ID 枚举
enum class ModuleId {
    CALIBRATE,      // astro_calibration.dll
    PLATESOLVE,     // ipv_solver.dll
    PSF,            // dynamic_psf.dll
    PHOTOMETRIC,    // photometric_calib.dll
    DRIZZLE         // healpix_drizzle.dll (任务描述称 hp_drizzle.dll)
};

// 模块加载状态
enum class ModuleStatus {
    NOT_LOADED,     // 未加载
    LOADED,         // 已加载成功
    LOAD_FAILED,    // 加载失败 (LoadLibrary 返回 NULL)
    NOT_FOUND       // 文件不存在
};

// 模块信息
struct ModuleInfo {
    ModuleId id;
    std::string name;           // 模块名
    std::string dll_filename;   // DLL 文件名
    std::string default_path;   // 默认路径 (相对于 lib_base_dir)
    ModuleStatus status;
    HMODULE handle;             // Windows HMODULE
    std::string error_msg;      // 错误信息
};

// DLL 加载器
class DllLoader {
public:
    DllLoader();
    ~DllLoader();

    // 加载所有 5 个模块 DLL
    bool load_all(const std::string& lib_base_dir);

    // 加载单个模块
    bool load_module(ModuleId id, const std::string& lib_base_dir);

    // 卸载所有模块
    void unload_all();

    // 卸载单个模块
    void unload_module(ModuleId id);

    // 获取函数指针 (模板, 必须放头文件)
    template<typename T>
    T get_function(ModuleId id, const std::string& func_name);

    // 查询状态
    bool is_loaded(ModuleId id) const;
    ModuleStatus get_status(ModuleId id) const;
    std::string get_error(ModuleId id) const;
    ModuleInfo get_info(ModuleId id) const;

    // 获取版本信息 (调用各模块的 *_version 函数)
    std::string get_version(ModuleId id);

    // 设置线程数 (调用各模块的 set_num_threads)
    bool set_num_threads(ModuleId id, int n);

private:
    std::map<ModuleId, ModuleInfo> modules_;

    // 辅助
    std::string get_default_path(ModuleId id, const std::string& lib_base_dir) const;
    std::string get_dll_filename(ModuleId id) const;
    std::string get_module_name(ModuleId id) const;

    // Windows API 封装
    HMODULE load_library(const std::string& path);
    void* get_proc_address(HMODULE handle, const std::string& func_name);
    void free_library(HMODULE handle);
    std::string get_last_error();
};

// ============================================================================
// 模板实现 (必须放在头文件中)
// ============================================================================
template<typename T>
T DllLoader::get_function(ModuleId id, const std::string& func_name) {
    auto it = modules_.find(id);
    if (it == modules_.end() || it->second.status != ModuleStatus::LOADED) {
        return nullptr;
    }
    void* ptr = get_proc_address(it->second.handle, func_name);
    return reinterpret_cast<T>(ptr);
}
