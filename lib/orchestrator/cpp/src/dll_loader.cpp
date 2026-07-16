// ============================================================================
// dll_loader.cpp - 动态 DLL 加载器实现
// 功能: 实现 DllLoader 类, 通过 Windows API 加载/卸载模块 DLL,
//       获取函数指针, 查询模块版本与状态。
// ============================================================================

#include "dll_loader.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <cstdlib>

#ifdef _WIN32
#include <windows.h>
#endif

// ============================================================================
// 辅助: 模块元数据
// ============================================================================
std::string DllLoader::get_module_name(ModuleId id) const {
    switch (id) {
        case ModuleId::AIO:             return "AIO";
        case ModuleId::CALIBRATE:       return "CALIBRATE";
        case ModuleId::PLATESOLVE:      return "PLATESOLVE";
        case ModuleId::PSF:             return "PSF";
        case ModuleId::PHOTOMETRIC:     return "PHOTOMETRIC";
        case ModuleId::GRADIENT_2D:     return "GRADIENT_2D";
        case ModuleId::SNR:             return "SNR";
        case ModuleId::DRIZZLE:         return "DRIZZLE";
        case ModuleId::GRADIENT_SPHERE: return "GRADIENT_SPHERE";
        case ModuleId::STACK:           return "STACK";
        default:                        return "UNKNOWN";
    }
}

std::string DllLoader::get_dll_filename(ModuleId id) const {
    switch (id) {
        case ModuleId::AIO:             return "astro_image_io.dll";
        case ModuleId::CALIBRATE:       return "astro_calibration.dll";
        case ModuleId::PLATESOLVE:      return "ipv_solver.dll";
        case ModuleId::PSF:             return "dynamic_psf.dll";
        case ModuleId::PHOTOMETRIC:     return "photometric_calib.dll";
        case ModuleId::GRADIENT_2D:     return "gradient_2d.dll";
        case ModuleId::SNR:             return "snr_estimator.dll";
        case ModuleId::DRIZZLE:         return "healpix_drizzle.dll";
        case ModuleId::GRADIENT_SPHERE: return "healpix_stack.dll";
        case ModuleId::STACK:           return "healpix_stack.dll";
        default:                        return "";
    }
}

std::string DllLoader::get_default_path(ModuleId id, const std::string& lib_base_dir) const {
    std::string sub;
    switch (id) {
        case ModuleId::AIO:             sub = "lib/astro_image_io/";                          break;
        case ModuleId::CALIBRATE:       sub = "lib/calibration/";                            break;
        case ModuleId::PLATESOLVE:      sub = "lib/plate_solve/cpp/ipv/";                    break;
        case ModuleId::PSF:             sub = "lib/dynamic_psf/";                            break;
        case ModuleId::PHOTOMETRIC:     sub = "lib/photometric_calib/cpp/";                  break;
        case ModuleId::GRADIENT_2D:     sub = "lib/photometric_calib/cpp/gradient_2d/";      break;
        case ModuleId::SNR:             sub = "lib/snr_estimator/cpp/";                      break;
        case ModuleId::DRIZZLE:         sub = "lib/healpix_db/healpix_drizzle/";             break;
        case ModuleId::GRADIENT_SPHERE: sub = "lib/healpix_db/healpix_stack/";               break;
        case ModuleId::STACK:           sub = "lib/healpix_db/healpix_stack/";               break;
        default:                        sub = "";
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
    // 初始化所有模块的默认信息 (10 个, 对应 spec §2.3.2 两段流水线)
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
    init(ModuleId::AIO);
    init(ModuleId::CALIBRATE);
    init(ModuleId::PLATESOLVE);
    init(ModuleId::PSF);
    init(ModuleId::PHOTOMETRIC);
    init(ModuleId::GRADIENT_2D);
    init(ModuleId::SNR);
    init(ModuleId::DRIZZLE);
    init(ModuleId::GRADIENT_SPHERE);
    init(ModuleId::STACK);
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
// 辅助: 查找 MinGW bin 目录 (运行时 DLL 依赖搜索路径)
// 业务 DLL 动态链接 libgomp-1.dll/liblz4.dll/libzstd.dll/libstdc++-6.dll 等
// MinGW 运行时 DLL, 这些 DLL 位于 MSYS2 mingw64\bin 目录.
// orchestrator.exe 用 -static 编译自身不需要, 但加载的业务 DLL 需要.
// 策略: 1) 从 PATH 环境变量查找 mingw64\bin; 2) 回退默认 C:\msys64\mingw64\bin
// ============================================================================
#ifdef _WIN32
static std::string find_mingw_bin() {
    // 1. 从 PATH 环境变量查找
    const char* path_env = std::getenv("PATH");
    if (path_env != nullptr) {
        std::string path_str(path_env);
        size_t pos = 0;
        while (pos < path_str.size()) {
            size_t sep = path_str.find(';', pos);
            std::string entry = (sep == std::string::npos)
                ? path_str.substr(pos)
                : path_str.substr(pos, sep - pos);
            // 查找包含 mingw64\bin 的路径
            if (entry.find("mingw64\\bin") != std::string::npos ||
                entry.find("mingw64/bin") != std::string::npos) {
                // 验证路径下有 libgomp-1.dll (MinGW 运行时标志)
                std::string test = entry + "\\libgomp-1.dll";
                DWORD attr = GetFileAttributesA(test.c_str());
                if (attr != INVALID_FILE_ATTRIBUTES) {
                    return entry;
                }
            }
            if (sep == std::string::npos) break;
            pos = sep + 1;
        }
    }
    // 2. 回退默认路径
    std::string def = "C:\\msys64\\mingw64\\bin";
    std::string test = def + "\\libgomp-1.dll";
    DWORD attr = GetFileAttributesA(test.c_str());
    if (attr != INVALID_FILE_ATTRIBUTES) {
        return def;
    }
    return "";
}
#endif

// ============================================================================
// load_all - 加载所有模块 DLL (对应 spec §2.3.2 两段流水线 10 节点)
// 返回: 全部加载成功返回 true, 任一失败返回 false
// 加载顺序: 设置运行时搜索路径 -> AIO 预加载 -> 第一段 stage1 -> 第二段 stage2
// ============================================================================
bool DllLoader::load_all(const std::string& lib_base_dir) {
    std::cerr << "[dll_loader] 开始加载所有模块 (lib_base_dir="
              << lib_base_dir << ")" << std::endl;

    bool all_ok = true;

#ifdef _WIN32
    // 0. 添加 MinGW 运行时 DLL 搜索路径 (解决业务 DLL 依赖 libgomp/liblz4 等)
    //    SetDllDirectoryA 将指定目录加入 DLL 搜索路径 (仅次于应用目录)
    //    orchestrator.exe 用 -static 编译, 自身不需要运行时 DLL,
    //    但业务 DLL (astro_image_io.dll 等) 动态链接需要.
    {
        std::string mingw_bin = find_mingw_bin();
        if (!mingw_bin.empty()) {
            SetDllDirectoryA(mingw_bin.c_str());
            std::cerr << "[dll_loader] 设置运行时 DLL 搜索路径: " << mingw_bin << std::endl;
        } else {
            std::cerr << "[dll_loader] [警告] 未找到 MinGW bin 目录, 业务 DLL 可能加载失败" << std::endl;
        }
    }
#endif

    // 1. 预加载 AIO (公共依赖, 多个模块依赖 astro_image_io.dll)
    //    用 load_module 正式加载 (注册到 modules_ 中, 可供后续 get_function 调用)
    all_ok = load_module(ModuleId::AIO, lib_base_dir) && all_ok;

#ifdef _WIN32
    // 兼容旧逻辑: 即便 load_module 已加载 AIO, 再用 LOAD_WITH_ALTERED_SEARCH_PATH
    // 显式预加载一次, 保证后续 DLL 在解析依赖时能找到 astro_image_io.dll
    // (Windows DLL 搜索路径问题的双保险)
    {
        std::string aio_dir;
        if (lib_base_dir.empty()) {
            aio_dir = "lib/astro_image_io/";
        } else if (lib_base_dir.back() == '/' || lib_base_dir.back() == '\\') {
            aio_dir = lib_base_dir + "lib/astro_image_io/";
        } else {
            aio_dir = lib_base_dir + "/lib/astro_image_io/";
        }
        std::string aio_path = aio_dir + "astro_image_io.dll";
        HMODULE aio_h = LoadLibraryExA(aio_path.c_str(), nullptr,
                                        LOAD_WITH_ALTERED_SEARCH_PATH);
        if (aio_h != nullptr) {
            std::cerr << "[dll_loader] 预加载 astro_image_io.dll (依赖前置) handle="
                      << aio_h << std::endl;
        }
    }
#endif

    // 2. 第一段: 单帧预处理 stage1 模块 (stage 1-7, AIO=stage0 已加载)
    all_ok = load_module(ModuleId::CALIBRATE,   lib_base_dir) && all_ok;
    all_ok = load_module(ModuleId::PLATESOLVE,  lib_base_dir) && all_ok;
    all_ok = load_module(ModuleId::PSF,         lib_base_dir) && all_ok;

#ifdef _WIN32
    // 预加载 gaia_client.dll (PHOTOMETRIC 依赖, 位于 photometric_calib/cpp/ 目录)
    // photometric_calib.dll 依赖 gaia_client.dll, gaia_client.dll 又依赖 libgomp/zlib
    // 预加载到进程地址空间, 避免传递依赖解析失败 (code=126)
    {
        std::string gaia_dir;
        if (lib_base_dir.empty()) {
            gaia_dir = "lib/photometric_calib/cpp/";
        } else if (lib_base_dir.back() == '/' || lib_base_dir.back() == '\\') {
            gaia_dir = lib_base_dir + "lib/photometric_calib/cpp/";
        } else {
            gaia_dir = lib_base_dir + "/lib/photometric_calib/cpp/";
        }
        std::string gaia_path = gaia_dir + "gaia_client.dll";
        HMODULE gaia_h = LoadLibraryExA(gaia_path.c_str(), nullptr,
                                        LOAD_WITH_ALTERED_SEARCH_PATH);
        if (gaia_h != nullptr) {
            std::cerr << "[dll_loader] 预加载 gaia_client.dll (依赖前置) handle="
                      << gaia_h << std::endl;
        }
    }
#endif

    all_ok = load_module(ModuleId::PHOTOMETRIC, lib_base_dir) && all_ok;
    all_ok = load_module(ModuleId::GRADIENT_2D, lib_base_dir) && all_ok;
    all_ok = load_module(ModuleId::SNR,         lib_base_dir) && all_ok;
    all_ok = load_module(ModuleId::DRIZZLE,     lib_base_dir) && all_ok;

    // 3. 第二段: 多帧合并 stage2 模块 (stage 8-9, 共用 healpix_stack.dll)
    all_ok = load_module(ModuleId::GRADIENT_SPHERE, lib_base_dir) && all_ok;
    // STACK 与 GRADIENT_SPHERE 共用 healpix_stack.dll, 已加载则跳过
    if (!is_loaded(ModuleId::STACK)) {
        all_ok = load_module(ModuleId::STACK, lib_base_dir) && all_ok;
    } else {
        // 复用 GRADIENT_SPHERE 的 handle
        modules_[ModuleId::STACK].handle = modules_[ModuleId::GRADIENT_SPHERE].handle;
        modules_[ModuleId::STACK].status = ModuleStatus::LOADED;
    }

    if (all_ok) {
        std::cerr << "[dll_loader] 全部模块加载成功" << std::endl;
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
    // 先卸载业务模块, 最后卸载 AIO (避免依赖顺序问题)
    unload_module(ModuleId::CALIBRATE);
    unload_module(ModuleId::PLATESOLVE);
    unload_module(ModuleId::PSF);
    unload_module(ModuleId::PHOTOMETRIC);
    unload_module(ModuleId::GRADIENT_2D);
    unload_module(ModuleId::SNR);
    unload_module(ModuleId::DRIZZLE);
    // STACK 与 GRADIENT_SPHERE 共用 handle, 仅卸载一次
    // 标记 STACK 为已卸载, 但不调用 free_library (避免 double-free)
    if (modules_.count(ModuleId::STACK) &&
        modules_[ModuleId::STACK].handle != nullptr &&
        modules_.count(ModuleId::GRADIENT_SPHERE) &&
        modules_[ModuleId::STACK].handle == modules_[ModuleId::GRADIENT_SPHERE].handle) {
        modules_[ModuleId::STACK].handle = nullptr;
        modules_[ModuleId::STACK].status = ModuleStatus::NOT_LOADED;
    } else {
        unload_module(ModuleId::STACK);
    }
    unload_module(ModuleId::GRADIENT_SPHERE);
    unload_module(ModuleId::AIO);
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
        case ModuleId::AIO:
        case ModuleId::PLATESOLVE:
        case ModuleId::PSF:
        case ModuleId::PHOTOMETRIC:
        case ModuleId::GRADIENT_2D:
        case ModuleId::SNR:
        case ModuleId::DRIZZLE:
        case ModuleId::GRADIENT_SPHERE:
        case ModuleId::STACK:
            return "unknown";  // 暂无统一 version 函数, 后续补充
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
        case ModuleId::AIO:
        case ModuleId::PLATESOLVE:
        case ModuleId::PSF:
        case ModuleId::PHOTOMETRIC:
        case ModuleId::GRADIENT_2D:
        case ModuleId::SNR:
        case ModuleId::DRIZZLE:
        case ModuleId::GRADIENT_SPHERE:
        case ModuleId::STACK:
            return false;  // 暂无 set_num_threads 接口, 后续补充
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
