// ============================================================================
// aio_abi_layout_probe.cpp — AIO HiPS 跨边界结构 C ABI 布局探针 (V11-N-01)
// ----------------------------------------------------------------------------
// 只读打印公共 C 结构体的 sizeof / alignof / 逐字段 offsetof+sizeof 为 JSON,
// 供 aio_abi_layout_lock.py 与唯一 ctypes 镜像
// (lib/infrastructure/aio/tools/aio_abi_mirror.py) 逐字段机器对账。
//
// 依赖面: 仅 aio_hips.h (无 windows.h / 不链接生产库) => Linux amd64 控制节点
// 可直接编译。
//
// 同时以 static_assert 锁定跨边界结构的 ABI 头部契约 (ASTROCS_DESIGN §7.3):
// struct_size 在偏移 0, abi_version 在偏移 4, 且二者均为 uint32_t。
// ============================================================================
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <type_traits>

#include "aio_hips.h"

// --- ABI 头部契约 (编译期锁; 任一结构漂移即编译失败) ---
#define AIO_ABI_HEAD_ASSERT(T)                                                \
    static_assert(offsetof(T, struct_size) == 0,                              \
                  #T ".struct_size 必须在偏移 0 (ASTROCS_DESIGN 7.3)");       \
    static_assert(offsetof(T, abi_version) == 4,                              \
                  #T ".abi_version 必须在偏移 4 (ASTROCS_DESIGN 7.3)");       \
    static_assert(std::is_same<decltype(T::struct_size), uint32_t>::value,    \
                  #T ".struct_size 必须为 uint32_t");                          \
    static_assert(std::is_same<decltype(T::abi_version), uint32_t>::value,    \
                  #T ".abi_version 必须为 uint32_t")

AIO_ABI_HEAD_ASSERT(AstroSphereTileView);
AIO_ABI_HEAD_ASSERT(AioHipsSnrPoint);
AIO_ABI_HEAD_ASSERT(AioHipsDiagTileView);
AIO_ABI_HEAD_ASSERT(AioHipsTile);

// --- 头/镜像一致性: 初始化器写入的版本号必须等于头常量 ---
static_assert(AIO_HIPS_TILE_VIEW_ABI_VERSION == 1u, "tile view ABI 版本常量漂移");
static_assert(AIO_HIPS_SNR_POINT_ABI_VERSION == 1u, "snr point ABI 版本常量漂移");
static_assert(AIO_HIPS_DIAG_TILE_VIEW_ABI_VERSION == 1u, "diag view ABI 版本常量漂移");
static_assert(AIO_HIPS_TILE_ABI_VERSION == 1u, "legacy tile ABI 版本常量漂移");

namespace {

#define AIO_OFF(T, f) offsetof(T, f)
#define AIO_SZ(T, f) sizeof(((T*)0)->f)

template <typename T>
void emit(const char* name, const char* const* names, const std::size_t* offsets,
          const std::size_t* sizes, int n, unsigned abi_value, bool last) {
    std::printf("  \"%s\": {\n", name);
    std::printf("    \"sizeof\": %zu,\n", sizeof(T));
    std::printf("    \"alignof\": %zu,\n", alignof(T));
    std::printf("    \"abi_version_value\": %u,\n", abi_value);
    std::printf("    \"fields\": [");
    for (int i = 0; i < n; ++i) {
        std::printf("%s\n      {\"name\": \"%s\", \"offset\": %zu, \"size\": %zu}",
                    i == 0 ? "" : ",", names[i], offsets[i], sizes[i]);
    }
    std::printf("\n    ]\n  }%s\n", last ? "" : ",");
}

}  // namespace

int main() {
    std::printf("{\n");

    {
        const char* names[] = {"struct_size", "abi_version", "parent_ipix",
                               "leaf_order", "width", "data_type", "flux_sum",
                               "covered_area", "valid_mask", "var_num_sum"};
        const std::size_t offsets[] = {
            AIO_OFF(AstroSphereTileView, struct_size),
            AIO_OFF(AstroSphereTileView, abi_version),
            AIO_OFF(AstroSphereTileView, parent_ipix),
            AIO_OFF(AstroSphereTileView, leaf_order),
            AIO_OFF(AstroSphereTileView, width),
            AIO_OFF(AstroSphereTileView, data_type),
            AIO_OFF(AstroSphereTileView, flux_sum),
            AIO_OFF(AstroSphereTileView, covered_area),
            AIO_OFF(AstroSphereTileView, valid_mask),
            AIO_OFF(AstroSphereTileView, var_num_sum)};
        const std::size_t sizes[] = {
            AIO_SZ(AstroSphereTileView, struct_size),
            AIO_SZ(AstroSphereTileView, abi_version),
            AIO_SZ(AstroSphereTileView, parent_ipix),
            AIO_SZ(AstroSphereTileView, leaf_order),
            AIO_SZ(AstroSphereTileView, width),
            AIO_SZ(AstroSphereTileView, data_type),
            AIO_SZ(AstroSphereTileView, flux_sum),
            AIO_SZ(AstroSphereTileView, covered_area),
            AIO_SZ(AstroSphereTileView, valid_mask),
            AIO_SZ(AstroSphereTileView, var_num_sum)};
        emit<AstroSphereTileView>("AstroSphereTileView", names, offsets, sizes, 10,
                                  (unsigned)AIO_HIPS_TILE_VIEW_ABI_VERSION, false);
    }
    {
        const char* names[] = {"struct_size", "abi_version", "ra_deg", "dec_deg",
                               "snr", "star_id", "quality_flags",
                               "photometric_status"};
        const std::size_t offsets[] = {
            AIO_OFF(AioHipsSnrPoint, struct_size), AIO_OFF(AioHipsSnrPoint, abi_version),
            AIO_OFF(AioHipsSnrPoint, ra_deg), AIO_OFF(AioHipsSnrPoint, dec_deg),
            AIO_OFF(AioHipsSnrPoint, snr), AIO_OFF(AioHipsSnrPoint, star_id),
            AIO_OFF(AioHipsSnrPoint, quality_flags),
            AIO_OFF(AioHipsSnrPoint, photometric_status)};
        const std::size_t sizes[] = {
            AIO_SZ(AioHipsSnrPoint, struct_size), AIO_SZ(AioHipsSnrPoint, abi_version),
            AIO_SZ(AioHipsSnrPoint, ra_deg), AIO_SZ(AioHipsSnrPoint, dec_deg),
            AIO_SZ(AioHipsSnrPoint, snr), AIO_SZ(AioHipsSnrPoint, star_id),
            AIO_SZ(AioHipsSnrPoint, quality_flags),
            AIO_SZ(AioHipsSnrPoint, photometric_status)};
        emit<AioHipsSnrPoint>("AioHipsSnrPoint", names, offsets, sizes, 8,
                              (unsigned)AIO_HIPS_SNR_POINT_ABI_VERSION, false);
    }
    {
        const char* names[] = {"struct_size", "abi_version", "parent_ipix",
                               "leaf_order", "width", "nused", "nrej"};
        const std::size_t offsets[] = {
            AIO_OFF(AioHipsDiagTileView, struct_size),
            AIO_OFF(AioHipsDiagTileView, abi_version),
            AIO_OFF(AioHipsDiagTileView, parent_ipix),
            AIO_OFF(AioHipsDiagTileView, leaf_order),
            AIO_OFF(AioHipsDiagTileView, width),
            AIO_OFF(AioHipsDiagTileView, nused),
            AIO_OFF(AioHipsDiagTileView, nrej)};
        const std::size_t sizes[] = {
            AIO_SZ(AioHipsDiagTileView, struct_size),
            AIO_SZ(AioHipsDiagTileView, abi_version),
            AIO_SZ(AioHipsDiagTileView, parent_ipix),
            AIO_SZ(AioHipsDiagTileView, leaf_order),
            AIO_SZ(AioHipsDiagTileView, width),
            AIO_SZ(AioHipsDiagTileView, nused),
            AIO_SZ(AioHipsDiagTileView, nrej)};
        emit<AioHipsDiagTileView>("AioHipsDiagTileView", names, offsets, sizes, 7,
                                  (unsigned)AIO_HIPS_DIAG_TILE_VIEW_ABI_VERSION, false);
    }
    {
        const char* names[] = {"struct_size", "abi_version", "parent_ipix",
                               "depth", "signal", "support"};
        const std::size_t offsets[] = {
            AIO_OFF(AioHipsTile, struct_size), AIO_OFF(AioHipsTile, abi_version),
            AIO_OFF(AioHipsTile, parent_ipix), AIO_OFF(AioHipsTile, depth),
            AIO_OFF(AioHipsTile, signal), AIO_OFF(AioHipsTile, support)};
        const std::size_t sizes[] = {
            AIO_SZ(AioHipsTile, struct_size), AIO_SZ(AioHipsTile, abi_version),
            AIO_SZ(AioHipsTile, parent_ipix), AIO_SZ(AioHipsTile, depth),
            AIO_SZ(AioHipsTile, signal), AIO_SZ(AioHipsTile, support)};
        emit<AioHipsTile>("AioHipsTile", names, offsets, sizes, 6,
                          (unsigned)AIO_HIPS_TILE_ABI_VERSION, true);
    }

    std::printf("}\n");
    return 0;
}
