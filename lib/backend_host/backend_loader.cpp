// lib/backend_host/backend_loader.cpp — manifest 解析/预检/受限加载 (05 §3/§6/§7) — ABI-002
#include "backend_loader.h"

#include <cstdio>

#include <nlohmann/json.hpp>

#include "cpu_features.h"
#include "sha256.h"

#if defined(_WIN32)
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace fs = std::filesystem;

namespace astrocs::backend_host {

namespace {

bool is_bare_filename(const std::string& name) {
    if (name.empty() || name.size() > 128) return false;
    if (name == "." || name == "..") return false;
    if (name.find('/') != std::string::npos) return false;    // 路径分隔符
    if (name.find('\\') != std::string::npos) return false;
    if (name.find(':') != std::string::npos) return false;    // 盘符/流
    if (name[0] == '.') return false;                          // 隐藏/相对
    return true;
}

std::string join_private(const std::string& dir, const std::string& bare) {
    std::string p = dir;
    if (!p.empty() && p.back() != '/' && p.back() != '\\') p += "/";
    p += bare;
    return p;
}

}  // namespace

std::string file_sha256_hex(const std::string& u8path) {
    std::FILE* f = std::fopen(u8path.c_str(), "rb");
    if (!f) return {};
    crypto::Sha256 h;
    char buf[65536];
    size_t n = 0;
    while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0) h.update(buf, n);
    std::fclose(f);
    return h.final_hex();
}

bool parse_backends_manifest(const std::string& json_text,
                             std::vector<ManifestEntry>* out, std::string* err) {
    nlohmann::json doc;
    try {
        doc = nlohmann::json::parse(json_text);
    } catch (const nlohmann::json::parse_error& e) {
        if (err) *err = std::string("malformed JSON: ") + e.what();
        return false;
    }
    if (!doc.is_object() || doc.value("kind", std::string()) != "astrocs_backends_manifest" ||
        doc.value("schema_version", std::string()) != "1" || !doc.contains("backends") ||
        !doc["backends"].is_array()) {
        if (err) *err = "not a v1 astrocs_backends_manifest document";
        return false;
    }
    for (const auto& b : doc["backends"]) {
        ManifestEntry e;
        e.file = b.value("file", "");
        e.backend_id = b.value("backend_id", "");
        e.sha256 = b.value("sha256", "");
        e.abi_version = b.value("abi_version", 0u);
        e.required_features = b.value("required_features_bits", 0ull);
        if (e.file.empty() || e.backend_id.empty() || e.sha256.size() != 64 ||
            e.abi_version != ACS_ABI_VERSION_V1) {
            if (err) *err = "manifest entry incomplete: " + e.file;
            return false;
        }
        out->push_back(std::move(e));
    }
    return true;
}

LoadResult preflight_entry(const std::string& backends_dir, const ManifestEntry& e,
                           uint64_t detected_features, std::string* reason) {
    auto fail = [&](LoadResult::Decision d, const std::string& m) {
        if (reason) *reason = m;
        return LoadResult{d, m};
    };
    // ① 安全白名单: 只认私有目录内的裸文件名(禁 PATH/LD_LIBRARY_PATH 注入)
    if (!is_bare_filename(e.file))
        return fail(LoadResult::REJECT_SECURITY, "entry is not a bare filename: " + e.file);
    // ② ABI 版本
    if (e.abi_version != ACS_ABI_VERSION_V1)
        return fail(LoadResult::FALLBACK_BASELINE,
                    "abi_version mismatch: " + std::to_string(e.abi_version));
    // ③ 文件存在(仅在私有 backends 目录内解析)
    const std::string path = join_private(backends_dir, e.file);
    std::error_code ec;
    if (!fs::exists(fs::u8path(path), ec))
        return fail(LoadResult::FALLBACK_BASELINE, "backend file missing: " + e.file);
    // ④ hash 实测
    const std::string got = file_sha256_hex(path);
    if (got.empty())
        return fail(LoadResult::FALLBACK_BASELINE, "backend file unreadable: " + e.file);
    if (got != e.sha256)
        return fail(LoadResult::FALLBACK_BASELINE, "hash mismatch: " + e.file);
    // ⑤ required ⊆ detected(CPUID+OSXSAVE+XGETBV 实测; 不支持绝不尝试执行)
    if ((detected_features & e.required_features) != e.required_features)
        return fail(LoadResult::FALLBACK_BASELINE,
                    "unsupported ISA for " + e.file + " (required=" +
                        std::to_string(e.required_features) + " detected=" +
                        std::to_string(detected_features) + ")");
    return {LoadResult::OK, "preflight ok"};
}

LoadResult load_backend(const std::string& backends_dir, const ManifestEntry& e,
                        const astrocs_host_services_v1* host,
                        astrocs_backend_api_v1* out_api, void** handle_out,
                        std::string* reason) {
    std::string why;
    LoadResult pre = preflight_entry(backends_dir, e, astrocs_cpu_detect_features_v1(), &why);
    if (pre.decision != LoadResult::OK) {
        if (reason) *reason = why;
        return pre;
    }
    const std::string path = join_private(backends_dir, e.file);
    void* handle = nullptr;
    using GetApiFn = int (*)(uint32_t, uint32_t, const astrocs_host_services_v1*,
                             astrocs_backend_api_v1*);
    GetApiFn get_api = nullptr;
#if defined(_WIN32)
    // 受限 DLL 搜索(禁 PATH 注入; 05 §3); LOAD_LIBRARY_SEARCH_* 仅当 _WIN32_WINNT>=0x0602 定义
    HMODULE mod;
#if defined(LOAD_LIBRARY_SEARCH_APPLICATION_DIR) && defined(LOAD_LIBRARY_SEARCH_DEPENDENCIES)
    mod = LoadLibraryExA(path.c_str(), nullptr,
                         LOAD_LIBRARY_SEARCH_APPLICATION_DIR |
                             LOAD_LIBRARY_SEARCH_DEPENDENCIES);
#else
    mod = LoadLibraryA(path.c_str());
#endif
    handle = static_cast<void*>(mod);
    if (handle)
        get_api = reinterpret_cast<GetApiFn>(
            static_cast<void*>(GetProcAddress(mod, "astrocs_backend_get_api_v1")));
#else
    handle = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (handle)
        get_api = reinterpret_cast<GetApiFn>(dlsym(handle, "astrocs_backend_get_api_v1"));
#endif
    if (!handle) {
        if (reason) *reason = "dlopen/LoadLibrary failed: " + e.file;
        return {LoadResult::FALLBACK_BASELINE, reason ? *reason : ""};
    }
    if (!get_api) {
        if (reason) *reason = "entry symbol missing: astrocs_backend_get_api_v1";
        close_backend(handle);
        return {LoadResult::FALLBACK_BASELINE, reason ? *reason : ""};
    }
    const int rc = get_api(ACS_ABI_VERSION_V1,
                           static_cast<uint32_t>(sizeof(astrocs_host_services_v1)),
                           host, out_api);
    if (rc != ACS_OK) {
        if (reason) *reason = "handshake failed in backend: " + e.file;
        close_backend(handle);
        return {LoadResult::FALLBACK_BASELINE, reason ? *reason : ""};
    }
    if (out_api->self_test && out_api->self_test(host) != ACS_OK) {
        if (reason) *reason = "backend self_test failed: " + e.file;
        close_backend(handle);
        return {LoadResult::FALLBACK_BASELINE, reason ? *reason : ""};
    }
    if (handle_out) *handle_out = handle;
    if (reason) *reason = "loaded";
    return {LoadResult::OK, "loaded"};
}

void close_backend(void* handle) {
    if (!handle) return;
#if defined(_WIN32)
    FreeLibrary(static_cast<HMODULE>(handle));
#else
    dlclose(handle);
#endif
}

}  // namespace astrocs::backend_host
