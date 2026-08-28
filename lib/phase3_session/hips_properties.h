// lib/phase3_session/hips_properties.h — HiPS properties 严格解析/校验 (ALG-P3-001) — P3-001
// 原则(06/TASK): 无 silent default —— 必需键缺失/值非法/越界一律拒绝;
// 安全路径: 拒 ".." / 空段 / NUL / 符号逃逸; 缺 tile 探测 = 声明叶级至少 1 tile 可读路径。
#ifndef ASTROCS_HIPS_PROPERTIES_H
#define ASTROCS_HIPS_PROPERTIES_H

#include <cstddef>
#include <string>

namespace astrocs::phase3 {

struct HipsProperties {
    int order = -1;              // hips_order(必需, 0..kMaxOrder)
    int tile_width = 0;          // hips_tile_width(必需, 恒 512)
    std::string tile_format;     // hips_tile_format(必需, 须含 fits)
    std::string frame;           // hips_frame(必需, equatorial|icrs)
    std::string dataproduct_type;// dataproduct_type(必需, image)
    std::string creator_did;     // 可缺(诊断)
    std::string obs_title;       // 可缺(诊断)
};

static constexpr int kMaxOrder = 20;   // ARCH-P3 §3 内存守卫(2^(2*20) leaf 上限之外拒)
static constexpr int kMinOrder = 0;
static constexpr int kHipsTileWidth = 512;

/* 解析 properties 文本(键=值 行, #/空行忽略; 重复键=错误; 未知键保留忽略)。
 * 必需键缺失/值非法 → false + err。无 silent default。 */
bool hips_properties_parse(const std::string& text, HipsProperties* out,
                           std::string* err);

/* 校验一个 HiPS 子产品目录(如 <root>/signal):
 * 1) 路径安全(拒绝 .. / 空 / 绝对越权组合);
 * 2) properties 存在且解析通过;
 * 3) order/tile_width/format/frame 值域(512/fits/equatorial|icrs);
 * 4) 缺 tile 探测: Norder<order>/Dir* 下至少 1 个 Npix*.fits 存在。
 * 全过 → true。 */
bool hips_product_validate(const std::string& product_dir, HipsProperties* out,
                           std::string* err);

/* 路径安全检查(共享): 拒空/NUL/反斜杠 Windows 盘符/".." 段。
 * 返回规范化的 POSIX 路径(不改文件系统)。 */
bool path_is_safe(const std::string& p, std::string* err);

}  // namespace astrocs::phase3

#endif  // ASTROCS_HIPS_PROPERTIES_H
