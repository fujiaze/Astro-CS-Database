// ============================================================================
// dll_loader.cpp - 动态 DLL 加载器实现
// 功能: 实现 DllLoader 类, 通过 Windows API 加载/卸载模块 DLL,
//       获取函数指针, 查询模块版本与状态。
// ============================================================================

#include "dll_loader.h"

#include <iostream>
#include <fstream>
#include <sstream>

#ifdef _WIN32
#include <windows.h>
#endif

// ============================================================================
// 辅助: 模块元数据
// ============================================================================
std::string DllLoader::get_module_name(ModuleId id) const {
    switch (id) {
        case ModuleId::CALIBRATE:   return "CALIBRATE";
        case ModuleId::PLATESOLVE:  return "PLATESOLVE";
        case ModuleId::PSF:         return "PSF";
        case ModuleId::PHOTOMETRIC: return "PHOTOMETRIC";
        case ModuleId::DRIZZLE:     return "DRIZZLE";
        default:                    return "UNKNOWN";
    }
}

std::string DllLoader::get_dll_filename(ModuleId id) const {
    switch (id) {
        case ModuleId::CALIBRATE:   return "astro_calibration.dll";
        case ModuleId::PLATESOLVE:  return "ipv_solver.dll";
        case ModuleId::PSF:         return "dynamic_psf.dll";
        case ModuleId::PHOTOMETRIC: return "photometric_calib.dll";
        case ModuleId::DRIZZLE:     return "healpix_drizzle.dll";  // 实际 DLL 文件名
        default:                    return "";
    }
}

std::string DllLoader::get_default_path(ModuleId id, const std::string& lib_base_dir) const {
    std::string sub;
    switch (id) {
        case ModuleId::CALIBRATE:   sub = "lib/calibration/";                 break;
        case ModuleId::PLATESOLVE:  sub = "lib/plate_solve/cpp/ipv/";         break;
        case ModuleId::PSF:         sub = "lib/dynamic_psf/";                 break;
        case ModuleId::PHOTOMETRIC: sub = "lib/photometric_calib/cpp/";       break;
        case ModuleId::DRIZZLE:     sub = "lib/healpix_db/healpix_drizzle/"; break;
        default:                    sub = "";
    }
    if (lib_base_dir.empty()) {
        return sub;
    }
    // 用正斜杠拼接 (Windows 兼容)
    if (lib_base_dir.back() == '/' || lib_base_dir.back() == '\\') {
        return lib_base_dir + sub;
    }
    return lib_base_dir + "/" + sub;
}

// ============================================================================
// 构造 / 析构
// ============================================================================
DllLoader::DllLoader() {
    // 初始化 5 个模块的默认信息
    auto init = [this](ModuleId id) {
        ModuleInfo info;
        info.id = id;
        info.name = get_module_name(id);
        info.dll_filename = get_dll_filename(id);
        info.default_path = "";  // 实际路径在 load_module 时填充
        info.status = ModuleStatus::NOT_LOADED;
        info.handle = nullptr;
        info.error_msg = "";
        modules_[id] = info;
    };
    init(ModuleId::CALIBRATE);
    init(ModuleId::PLATESOLVE);
    init(ModuleId::PSF);
    init(ModuleId::PHOTOMETRIC);
    init(ModuleId::DRIZZLE);
}

DllLoader::~DllLoader() {
    unload_all();
}

// ============================================================================
// load_module - 加载单个模块 DLL
// 步骤:
//   1. 构建完整路径 = lib_base_dir + "/" + default_path + "/" + dll_filename
//   2. 检查文件是否存在 (std::ifstream)
//   3. 调用 LoadLibraryA 加载
//   4. 如失败, 设置 status=LOAD_FAILED, 记录 error_msg
//   5. 如成功, 设置 status=LOADED
// ============================================================================
bool DllLoader::load_module(ModuleId id, const std::string& lib_base_dir) {
    auto it = modules_.find(id);
    if (it == modules_.end()) {
        std::cerr << "[dll_loader] 错误: 未知模块 ID" << std::endl;
        return false;
    }

    ModuleInfo& info = it->second;

    // 如已加载, 先卸载
    if (info.status == ModuleStatus::LOADED && info.handle != nullptr) {
        std::cerr << "[dll_loader] 模块 " << info.name
                  << " 已加载, 先卸载旧实例" << std::endl;
        free_library(info.handle);
        info.handle = nullptr;
        info.status = ModuleStatus::NOT_LOADED;
    }

    // 构建完整路径
    std::string dir = get_default_path(id, lib_base_dir);
    info.default_path = dir;
    std::string full_path;
    if (dir.empty()) {
        full_path = info.dll_filename;
    } else if (dir.back() == '/' || dir.back() == '\\') {
        full_path = dir + info.dll_filename;
    } else {
        full_path = dir + "/" + info.dll_filename;
    }

    std::cerr << "[dll_loader] 加载模块 " << info.name
              << ": " << full_path << std::endl;

    // 检查文件是否存在
    std::ifstream ifs(full_path, std::ios::binary);
    if (!ifs.is_open()) {
        info.status = ModuleStatus::NOT_FOUND;
        info.error_msg = "DLL 文件不存在: " + full_path;
        std::cerr << "[dll_loader] [错误] " << info.error_msg << std::endl;
        return false;
    }
    ifs.close();

    // 调用 LoadLibraryA 加载
    HMODULE h = load_library(full_path);
    if (h == nullptr) {
        info.status = ModuleStatus::LOAD_FAILED;
        info.error_msg = "LoadLibraryA 失败: " + get_last_error();
        std::cerr << "[dll_loader] [错误] " << info.error_msg << std::endl;
        return false;
    }

    info.handle = h;
    info.status = ModuleStatus::LOADED;
    info.error_msg = "";
    std::cerr << "[dll_loader] 模块 " << info.name << " 加载成功 (handle="
              << h << ")" << std::endl;
    return true;
}

// ============================================================================
// load_all - 加载所有 5 个模块 DLL
// 返回: 全部加载成功返回 true, 任一失败返回 false
// ============================================================================
bool DllLoader::load_all(const std::string& lib_base_dir) {
    std::cerr << "[dll_loader] 开始加载所有模块 (lib_base_dir="
              << lib_base_dir << ")" << std::endl;

#ifdef _WIN32
    // 预加载公共依赖 DLL: astro_image_io.dll (位于 lib/astro_image_io/)
    // 多个模块 (healpix_drizzle, healpix_lod) 依赖 astro_image_io.dll,
    // 但它们各自在不同目录, LOAD_WITH_ALTERED_SEARCH_PATH 只能找同目录的依赖,
    // 所以需要预先把 astro_image_io.dll 加载到进程地址空间, 后续加载的 DLL
    // 在解析依赖时会直接复用已加载的 astro_image_io.dll
    std::string aio_dir;
    if (lib_base_dir.empty()) {
        aio_dir = "lib/astro_image_io/";
    } else if (lib_base_dir.back() == '/' || lib_base_dir.back() == '\\') {
        aio_dir = lib_base_dir + "lib/astro_image_io/";
    } else {
        aio_dir = lib_base_dir + "/lib/astro_image_io/";
    }
    std::string aio_path = aio_dir + "astro_image_io.dll";
    std::ifstream aio_ifs(aio_path, std::ios::binary);
    if (aio_ifs.is_open()) {
        aio_ifs.close();
        HMODULE aio_h = LoadLibraryExA(aio_path.c_str(), nullptr,
                                        LOAD_WITH_ALTERED_SEARCH_PATH);
        if (aio_h != nullptr) {
            std::cerr << "[dll_loader] 预加载公共依赖 astro_image_io.dll 成功 (handle="
                      << aio_h << ")" << std::endl;
        } else {
            std::cerr << "[dll_loader] [警告] 预加载 astro_image_io.dll 失败, "
                      << "依赖模块 (DRIZZLE) 可能无法加载" << std::endl;
        }
    } else {
        std::cerr << "[dll_loader] [提示] astro_image_io.dll 未找到 ("
                  << aio_path << "), 依赖模块可能无法加载" << std::endl;
    }
#endif

    bool all_ok = true;
    all_ok = load_module(ModuleId::CALIBRATE,   lib_base_dir) && all_ok;
    all_ok = load_module(ModuleId::PLATESOLVE,  lib_base_dir) && all_ok;
    all_ok = load_module(ModuleId::PSF,         lib_base_dir) && all_ok;
    all_ok = load_module(ModuleId::PHOTOMETRIC, lib_base_dir) && all_ok;
    all_ok = load_module(ModuleId::DRIZZLE,     lib_base_dir) && all_ok;

    if (all_ok) {
        std::cerr << "[dll_loader] 全部 5 个模块加载成功" << std::endl;
    } else {
        std::cerr << "[dll_loader] 部分模块加载失败, 详见各模块状态" << std::endl;
    }
    return all_ok;
}

// ============================================================================
// unload_module / unload_all
// ============================================================================
void DllLoader::unload_module(ModuleId id) {
    auto it = modules_.find(id);
    if (it == modules_.end()) return;
    ModuleInfo& info = it->second;
    if (info.handle != nullptr) {
        std::cerr << "[dll_loader] 卸载模块 " << info.name << std::endl;
        free_library(info.handle);
        info.handle = nullptr;
    }
    info.status = ModuleStatus::NOT_LOADED;
    info.error_msg = "";
}

void DllLoader::unload_all() {
    std::cerr << "[dll_loader] 卸载所有模块" << std::endl;
    unload_module(ModuleId::CALIBRATE);
    unload_module(ModuleId::PLATESOLVE);
    unload_module(ModuleId::PSF);
    unload_module(ModuleId::PHOTOMETRIC);
    unload_module(ModuleId::DRIZZLE);
}

// ============================================================================
// 状态查询
// ============================================================================
bool DllLoader::is_loaded(ModuleId id) const {
    auto it = modules_.find(id);
    if (it == modules_.end()) return false;
    return it->second.status == ModuleStatus::LOADED;
}

ModuleStatus DllLoader::get_status(ModuleId id) const {
    auto it = modules_.find(id);
    if (it == modules_.end()) return ModuleStatus::NOT_LOADED;
    return it->second.status;
}

std::string DllLoader::get_error(ModuleId id) const {
    auto it = modules_.find(id);
    if (it == modules_.end()) return "未知模块 ID";
    return it->second.error_msg;
}

ModuleInfo DllLoader::get_info(ModuleId id) const {
    auto it = modules_.find(id);
    if (it == modules_.end()) {
        ModuleInfo empty;
        empty.id = id;
        empty.name = "UNKNOWN";
        empty.status = ModuleStatus::NOT_LOADED;
        empty.handle = nullptr;
        return empty;
    }
    return it->second;
}

// ============================================================================
// get_version - 获取模块版本号
//   - CALIBRATE: 调用 ac_version()
//   - 其他模块: 暂返回 "unknown" (后续 Task 补充接口)
// ============================================================================
std::string DllLoader::get_version(ModuleId id) {
    if (!is_loaded(id)) {
        return "[未加载]";
    }
    switch (id) {
        case ModuleId::CALIBRATE: {
            // ac_version 返回 const char*
            using FuncType = const char* (*)();
            FuncType fn = get_function<FuncType>(id, "ac_version");
            if (fn == nullptr) {
                return "[无 ac_version 函数]";
            }
            const char* v = fn();
            if (v == nullptr) return "[null]";
            return std::string(v);
        }
        case ModuleId::PLATESOLVE:
            return "unknown";  // ipv_solver 暂无 version 函数
        case ModuleId::PSF:
            return "unknown";  // dynamic_psf 暂无 version 函数
        case ModuleId::PHOTOMETRIC:
            return "unknown";  // photometric_calib 暂无 version 函数
        case ModuleId::DRIZZLE:
            return "unknown";  // healpix_drizzle 暂无 version 函数
        default:
            return "unknown";
    }
}

// ============================================================================
// set_num_threads - 设置模块 OpenMP 线程数
//   - CALIBRATE: 调用 ac_set_num_threads(int)
//   - 其他模块: 暂返回 false (后续 Task 补充接口)
// ============================================================================
bool DllLoader::set_num_threads(ModuleId id, int n) {
    if (!is_loaded(id)) {
        return false;
    }
    if (n < 1) {
        std::cerr << "[dll_loader] [警告] 线程数非法: " << n << " (应>=1)" << std::endl;
        return false;
    }
    switch (id) {
        case ModuleId::CALIBRATE: {
            using FuncType = void (*)(int);
            FuncType fn = get_function<FuncType>(id, "ac_set_num_threads");
            if (fn == nullptr) {
                std::cerr << "[dll_loader] [警告] CALIBRATE 无 ac_set_num_threads 函数" << std::endl;
                return false;
            }
            fn(n);
            std::cerr << "[dll_loader] CALIBRATE 线程数设置为 " << n << std::endl;
            return true;
        }
        case ModuleId::PLATESOLVE:
            return false;  // 暂无 set_num_threads 接口
        case ModuleId::PSF:
            return false;  // 暂无 set_num_threads 接口
        case ModuleId::PHOTOMETRIC:
            return false;  // 暂无 set_num_threads 接口
        case ModuleId::DRIZZLE:
            return false;  // 暂无 set_num_threads 接口
        default:
            return false;
    }
}

// ============================================================================
// Windows API 封装
// ============================================================================
#ifdef _WIN32
HMODULE DllLoader::load_library(const std::string& path) {
    // LoadLibraryEx 配合 LOAD_WITH_ALTERED_SEARCH_PATH:
    //   当传入绝对路径时, 依赖的 DLL 会优先从该路径所在目录查找
    //   解决 ipv_solver/dynamic_psf/healpix_drizzle 依赖 astro_image_io 等
    //   其他目录 DLL 找不到的问题 (错误码 126 ERROR_MOD_NOT_FOUND)
    return LoadLibraryExA(path.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
}

void* DllLoader::get_proc_address(HMODULE handle, const std::string& func_name) {
    if (handle == nullptr) return nullptr;
    return reinterpret_cast<void*>(GetProcAddress(handle, func_name.c_str()));
}

void DllLoader::free_library(HMODULE handle) {
    if (handle != nullptr) {
        FreeLibrary(handle);
    }
}

std::string DllLoader::get_last_error() {
    DWORD err = GetLastError();
    if (err == 0) return "无错误";

    LPSTR buf = nullptr;
    DWORD len = FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
            FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        reinterpret_cast<LPSTR>(&buf), 0, nullptr);

    std::string msg;
    if (len > 0 && buf != nullptr) {
        msg.assign(buf, len);
        LocalFree(buf);
        // 去除末尾换行
        while (!msg.empty() && (msg.back() == '\r' || msg.back() == '\n' || msg.back() == ' ')) {
            msg.pop_back();
        }
        msg = "code=" + std::to_string(err) + " (" + msg + ")";
    } else {
        msg = "code=" + std::to_string(err) + " (FormatMessageA 无描述)";
    }
    return msg;
}
#else
// 非 Windows 平台占位 (本编排器仅支持 Windows)
HMODULE DllLoader::load_library(const std::string& /*path*/) { return nullptr; }
void* DllLoader::get_proc_address(HMODULE /*handle*/, const std::string& /*func_name*/) { return nullptr; }
void DllLoader::free_library(HMODULE /*handle*/) {}
std::string DllLoader::get_last_error() { return "非 Windows 平台不支持 DLL 加载"; }
#endif
