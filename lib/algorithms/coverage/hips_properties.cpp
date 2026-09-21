// lib/phase3_session/hips_properties.cpp — HiPS properties 严格解析实现 (ALG-P3-001) — P3-001
#include "hips_properties.h"

#include <cstdio>
#include <cstring>

// CLEAN-403 (ASTROCS_DESIGN §10「aio 是文件级唯一 I/O 边界」): 目录枚举与
// 整文件读取一律经 aio 唯一实现 (aio_atomic::for_each_child / aio_file::read_all),
// 本 TU 不再 include <dirent.h>/<fstream> 自持 I/O 通道。
#include "aio_atomic_file.h"
#include "aio_file_io.h"

#include <sstream>

namespace astrocs::phase3 {

namespace {

std::string trim(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && (s[a] == ' ' || s[a] == '\t' || s[a] == '\r')) ++a;
    while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t' || s[b - 1] == '\r')) --b;
    return s.substr(a, b - a);
}

bool parse_int(const std::string& v, int* out) {
    if (v.empty()) return false;
    long acc = 0;
    size_t i = (v[0] == '-' || v[0] == '+') ? 1 : 0;
    if (i >= v.size()) return false;
    for (; i < v.size(); ++i) {
        if (v[i] < '0' || v[i] > '9') return false;
        acc = acc * 10 + (v[i] - '0');
        if (acc > 100000) return false;   // 防溢出/超界
    }
    *out = static_cast<int>(v[0] == '-' ? -acc : acc);
    return true;
}

}  // namespace

bool path_is_safe(const std::string& p, std::string* err) {
    const auto fail = [&](const char* m) { if (err) *err = m; return false; };
    if (p.empty()) return fail("path empty");
    if (p.find('\0') != std::string::npos) return fail("path contains NUL");
    if (p.find('\\') != std::string::npos) return fail("path contains backslash");
    if (p.find("//") != std::string::npos) return fail("path contains empty segment");
    // 逐段检查 ".."(拒路径上溯; Windows 盘符/绝对 UNC 不支持)
    size_t start = 0;
    while (start <= p.size()) {
        size_t end = p.find('/', start);
        if (end == std::string::npos) end = p.size();
        const std::string seg = p.substr(start, end - start);
        if (seg == "..") return fail("path contains '..' (traversal)");
        start = end + 1;
        if (end == p.size()) break;
    }
    return true;
}

bool hips_properties_parse(const std::string& text, HipsProperties* out,
                           std::string* err) {
    const auto fail = [&](const std::string& m) { if (err) *err = m; return false; };
    if (!out) return fail("null out");
    *out = HipsProperties{};
    bool has_order = false, has_width = false, has_fmt = false, has_frame = false,
         has_dpt = false;
    std::istringstream f(text);
    std::string line;
    while (std::getline(f, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#') continue;
        const size_t eq = line.find('=');
        if (eq == std::string::npos) return fail("malformed line (no '='): " + line);
        const std::string key = trim(line.substr(0, eq));
        const std::string val = trim(line.substr(eq + 1));
        if (key.empty()) return fail("empty key");
        // 重复键 = 错误(无 silent override)
        if (key == "hips_order") {
            if (has_order) return fail("duplicate key hips_order");
            if (!parse_int(val, &out->order)) return fail("hips_order not integer: " + val);
            has_order = true;
        } else if (key == "hips_tile_width") {
            if (has_width) return fail("duplicate key hips_tile_width");
            if (!parse_int(val, &out->tile_width)) return fail("hips_tile_width not integer");
            has_width = true;
        } else if (key == "hips_tile_format") {
            if (has_fmt) return fail("duplicate key hips_tile_format");
            if (val.empty()) return fail("hips_tile_format empty");
            out->tile_format = val;
            has_fmt = true;
        } else if (key == "hips_frame") {
            if (has_frame) return fail("duplicate key hips_frame");
            out->frame = val;
            has_frame = true;
        } else if (key == "dataproduct_type") {
            if (has_dpt) return fail("duplicate key dataproduct_type");
            out->dataproduct_type = val;
            has_dpt = true;
        } else if (key == "creator_did") {
            out->creator_did = val;
        } else if (key == "obs_title") {
            out->obs_title = val;
        } else if (key == "BUNIT" || key == "bunit") {
            out->bunit = val;   // 可缺; 缺省 ADU(SCI 合同), 绝不 Jy/beam 默认
        }
        // 未知键: 保留忽略(HiPS 允许扩展键), 但值仍不得含 NUL
        if (val.find('\0') != std::string::npos) return fail("value contains NUL");
    }
    // 必需键全查(无 silent default)
    if (!has_order) return fail("missing required key 'hips_order'");
    if (!has_width) return fail("missing required key 'hips_tile_width'");
    if (!has_fmt) return fail("missing required key 'hips_tile_format'");
    if (!has_frame) return fail("missing required key 'hips_frame'");
    if (!has_dpt) return fail("missing required key 'dataproduct_type'");
    // 值域
    if (out->order < kMinOrder || out->order > kMaxOrder)
        return fail("hips_order out of range [0," + std::to_string(kMaxOrder) + "]: " +
                    std::to_string(out->order));
    if (out->tile_width != kHipsTileWidth)
        return fail("hips_tile_width must be 512 (got " + std::to_string(out->tile_width) + ")");
    if (out->tile_format.find("fits") == std::string::npos)
        return fail("hips_tile_format must include 'fits' (got " + out->tile_format + ")");
    if (out->frame != "equatorial" && out->frame != "icrs")
        return fail("hips_frame must be equatorial|icrs (got " + out->frame + ")");
    if (out->dataproduct_type != "image")
        return fail("dataproduct_type must be image (got " + out->dataproduct_type + ")");
    return true;
}

bool hips_product_validate(const std::string& product_dir, HipsProperties* out,
                           std::string* err) {
    const auto fail = [&](const std::string& m) { if (err) *err = m; return false; };
    if (!path_is_safe(product_dir, err)) return false;
    const std::string props_path = product_dir + "/properties";
    std::string props_text;
    if (!aio_file::read_all(props_path.c_str(), &props_text))
        return fail("properties not found: " + props_path);
    if (!hips_properties_parse(props_text, out, err)) return false;
    // 缺 tile 探测: Norder<order>/Dir*/Npix*.fits 至少 1 (目录枚举经 aio)
    const std::string order_dir = product_dir + "/Norder" + std::to_string(out->order);
    int order_is_dir = 0;
    if (!aio_atomic::path_exists(order_dir, &order_is_dir) || !order_is_dir)
        return fail("order directory missing (no tiles): " + order_dir);
    bool found = false;
    auto base_name = [](const std::string& p) {
        const std::size_t s = p.find_last_of("/\\");
        return (s == std::string::npos) ? p : p.substr(s + 1);
    };
    int abort_rc = 0;
    const int rc = aio_atomic::for_each_child(
        order_dir,
        [&](const std::string& subdir, int kind) -> int {
            if (kind != 1) return 0;                 // 只下钻目录 (opendir 语义等价)
            if (base_name(subdir).rfind("Dir", 0) != 0) return 0;
            (void)aio_atomic::for_each_child(
                subdir,
                [&](const std::string& tile, int tkind) -> int {
                    if (tkind != 0) return 0;
                    const std::string t = base_name(tile);
                    if (t.rfind("Npix", 0) == 0 && t.size() > 5 &&
                        t.compare(t.size() - 5, 5, ".fits") == 0) {
                        found = true;
                        return 1;                    // 中止本层枚举
                    }
                    return 0;
                },
                nullptr);
            return found ? 1 : 0;                    // 已找到 ⇒ 中止外层
        },
        &abort_rc);
    if (rc != 0) return fail("order directory unreadable: " + order_dir);
    if (!found) return fail("no Npix*.fits tile under " + order_dir + " (missing tiles)");
    return true;
}

}  // namespace astrocs::phase3
