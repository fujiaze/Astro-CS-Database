// P1-PHOT-TEST · gaia_client 测试桩 (fake client, 外部域 CAT-GAIA 替身)
//
// 被测域 lib/photometric_calib 生产源 (pc_api.cpp with_gaia 系列) 经 C ABI
// 调用外部域符号 gaia_client_cone_search_with_spectrum /
// gaia_client_get_spectrum_params (lib/gaia_xpsd_client, CAT-GAIA 域)。
// 本桩仅替代外部域依赖 (被测域生产源零修改、零重编译差异), 由测试注入
// 固定合成星表/光谱或故障模式:
//   fail_mode=0 → 返回注册的合成星表 (F1/F4/F5 正常路径);
//   fail_mode=1 → 返回非 0 (锥形搜索失败 → 被测 rc=-3 注入点);
//   返回 0 星   → 被测退化路径 (scale=1.0, records reason=5)。
// 桩内不做任何被测域算法 (无积分/匹配/IRLS 逻辑)。
//
// pc_api.cpp 对 out_stars/out_spectra 使用 free() 释放 → 桩用 malloc 分配,
// 与 gaia_client.h 的 Ownership 注释一致。
#ifndef P1PHOT_GAIA_STUB_HPP
#define P1PHOT_GAIA_STUB_HPP

#include <cstdint>
#include <vector>

// gaia_client.h 提供 GaiaClient opaque typedef + GaiaSpectrumStar 完整定义
// (他域头, 只读引用)
#include "gaia_client.h"

namespace p1phot {
namespace stub {

// 与全局 opaque typedef 同一类型 (测试 TU 经 stub::GaiaClient 引用)
using GaiaClient = ::GaiaClient;

// 测试侧 fake client 状态 (与桩 cpp 中 GaiaClient 完整定义一致布局由桩管理;
// 测试经 create/destroy/以下 setter 操作, 不直接触 GaiaClient 字段)
struct FakeStar {
    double ra, dec, magG;
    float flux_min, flux_mul;
    std::vector<std::uint8_t> spectrum;  // n 点, n=343 (与 wl_start/step 匹配)
};

struct FakeClientConfig {
    std::vector<FakeStar> stars;
    int fail_mode = 0;      // 0=ok, 1=cone search 失败注入
    int wl_start = 336;     // 光谱参数 (pc_api 经 get_spectrum_params 取)
    int wl_step = 2;
    int wl_count = 343;
};

// 创建/销毁 (桩内 new/delete GaiaClient; handle 语义=opaque borrow)
GaiaClient* create(const FakeClientConfig& cfg);
void destroy(GaiaClient* c);

// 用例中途切换注入模式 (负面组免重建)
void set_fail_mode(GaiaClient* c, int fail_mode);

}  // namespace stub
}  // namespace p1phot

#endif  // P1PHOT_GAIA_STUB_HPP
