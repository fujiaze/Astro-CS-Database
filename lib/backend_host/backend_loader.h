// lib/backend_host/backend_loader.h — backend manifest/预检/安全加载 (05 §3/§6/§7) — ABI-002
// 策略: 预检失败→回退 baseline; 计算 stage 不静默换 backend(05 §6); 安全路径白名单。
#ifndef ASTROCS_BACKEND_LOADER_H
#define ASTROCS_BACKEND_LOADER_H

#include <string>
#include <vector>

#include "astrocs/common_abi_v1.h"

namespace astrocs::backend_host {

struct ManifestEntry {
    std::string file;                 // 裸文件名(禁路径分隔符/../盘符)
    std::string backend_id;
    std::string sha256;               // 小写 hex(64)
    uint32_t abi_version = 0;
    uint64_t required_features = 0;   // ACS_FEAT_* 位或
};

struct LoadResult {
    enum Decision { OK = 0, FALLBACK_BASELINE = 1, REJECT_SECURITY = 2 } decision;
    std::string reason;
};

/* backends.manifest.json 解析:
 * {"schema_version":"1","kind":"astrocs_backends_manifest",
 *  "backends":[{"file":"...","backend_id":"...","sha256":"...","abi_version":1,
 *               "required_features_bits":N}]}
 * 结构非法→err 非空, 不猜。reentrant=yes; threadsafe=yes。 */
bool parse_backends_manifest(const std::string& json_text,
                             std::vector<ManifestEntry>* out, std::string* err);

/* 预检(05 §3 六查的文件/ISA 面; 纯读, 不执行任何 backend 指令):
 * ①file 为裸文件名(安全白名单) ②abi_version==ACS_ABI_VERSION_V1
 * ③文件存在 ④sha256 实测匹配 ⑤required ⊆ detected(CPUID+XGETBV)。
 * 任一失败→非 OK+理由; 不产生部分状态。 */
LoadResult preflight_entry(const std::string& backends_dir, const ManifestEntry& e,
                           uint64_t detected_features, std::string* reason);

/* 加载(dlopen/受限 LoadLibrary)+handshake+self_test; 预检先行, 任一步失败即回退。
 * handle_out 供 dlclose(调用方唯一释放对)。reentrant=yes; threadsafe=no(句柄级)。 */
LoadResult load_backend(const std::string& backends_dir, const ManifestEntry& e,
                        const astrocs_host_services_v1* host,
                        astrocs_backend_api_v1* out_api, void** handle_out,
                        std::string* reason);

void close_backend(void* handle);

/* manifest 文件 sha256 实测(供 gen 工具/测试与加载前快速比对); 失败返空串。 */
std::string file_sha256_hex(const std::string& u8path);

}  // namespace astrocs::backend_host

#endif  // ASTROCS_BACKEND_LOADER_H
