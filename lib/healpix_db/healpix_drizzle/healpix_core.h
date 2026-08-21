// DEPRECATED: B4-01 去重，唯一实现见 lib/common/healpix/healpix_core.h
// 本文件为兼容 shim，转发至 astrocs::healpix 权威实现。
// 禁止在此新增/修改任何数值算法；新增需求请在 common 增补充。
#ifndef HEALPIX_DRIZZLE_SHIM_H
#define HEALPIX_DRIZZLE_SHIM_H

#include "healpix/healpix_core.h"

#include <cmath>
#include <cstdint>
#include <vector>
#include <stdexcept>

namespace healpix {

// 兼容适配：历史 healpix::HealpixCore 类仅保留 NESTED 语义，
// 内部委托 astrocs::healpix。RING 构造直接拒绝（已在 drizzle 入口拒绝 RING）。
class HealpixCore {
public:
    HealpixCore(int nside, bool nested = true)
        : m_nside(nside), m_nested(nested) {
        if (!nested) {
            throw std::invalid_argument("RING not supported: use astrocs::healpix NESTED only");
        }
        if (nside <= 0 || (nside & (nside - 1)) != 0) {
            // 保持历史警告语义：不抛异常，仅容忍（调用方已校验），与 common 行为对齐
        }
    }

    int getNside() const { return m_nside; }
    bool isNested() const { return m_nested; }
    int64_t getNpix() const { return (int64_t)astrocs::healpix::npix((uint32_t)m_nside); }
    double pixelResolutionArcsec() const {
        return astrocs::healpix::pixel_resolution_arcsec((uint32_t)m_nside);
    }

    // theta: 0=北极, π=南极; phi: 0~2π  -> NESTED
    int64_t ang2pix(double theta_rad, double phi_rad) const {
        double dec = 90.0 - theta_rad * 180.0 / 3.14159265358979323846;
        double ra  = phi_rad * 180.0 / 3.14159265358979323846;
        if (ra < 0) ra += 360.0;
        if (ra >= 360.0) ra = std::fmod(ra, 360.0);
        uint64_t u = astrocs::healpix::ang2pix_nest((uint32_t)m_nside, ra, dec);
        return (int64_t)u;
    }
    void pix2ang(int64_t ipix, double* theta_rad, double* phi_rad) const {
        double ra, dec;
        astrocs::healpix::pix2ang_nest((uint32_t)m_nside, (uint64_t)ipix, ra, dec);
        *phi_rad = ra * 3.14159265358979323846 / 180.0;
        *theta_rad = (90.0 - dec) * 3.14159265358979323846 / 180.0;
    }

    int64_t radec2pix(double ra_deg, double dec_deg) const {
        return (int64_t)astrocs::healpix::ang2pix_nest((uint32_t)m_nside, ra_deg, dec_deg);
    }
    void pix2radec(int64_t ipix, double* ra_deg, double* dec_deg) const {
        astrocs::healpix::pix2ang_nest((uint32_t)m_nside, (uint64_t)ipix, *ra_deg, *dec_deg);
    }

    std::vector<int64_t> neighbors(int64_t ipix) const {
        auto v = astrocs::healpix::neighbors((uint32_t)m_nside, (uint64_t)ipix);
        std::vector<int64_t> out;
        out.reserve(v.size());
        for (auto x : v) out.push_back((int64_t)x);
        return out;
    }

    std::vector<int64_t> queryDisc(double ra_deg, double dec_deg, double radius_arcsec) const {
        auto v = astrocs::healpix::query_disc((uint32_t)m_nside, ra_deg, dec_deg, radius_arcsec);
        std::vector<int64_t> out;
        out.reserve(v.size());
        for (auto x : v) out.push_back((int64_t)x);
        return out;
    }

    // 仅保留兼容存根，drizzle 生产路径未使用（保留防编译破裂）
    int64_t pixelToCoarse(int64_t ipix_fine, int nside_coarse) const {
        if (nside_coarse <= 0) return ipix_fine;
        // NESTED 位运算父像素：shift = log2(nside_fine/nside_coarse)
        int a = 0, b = 0;
        int t = m_nside; while (t > 1) { t >>= 1; ++a; }
        t = nside_coarse; while (t > 1) { t >>= 1; ++b; }
        int shift = a - b;
        if (shift <= 0) return ipix_fine;
        return ipix_fine >> (2 * shift);
    }
    std::vector<int64_t> pixelToFine(int64_t ipix_coarse, int nside_fine) const {
        if (nside_fine <= m_nside) return {ipix_coarse};
        int a = 0, b = 0;
        int t = m_nside; while (t > 1) { t >>= 1; ++a; }
        t = nside_fine; while (t > 1) { t >>= 1; ++b; }
        int shift = b - a;
        int n = 1 << (2 * shift);
        std::vector<int64_t> out;
        out.reserve(n);
        int64_t base = ipix_coarse << (2 * shift);
        for (int i = 0; i < n; ++i) out.push_back(base | i);
        return out;
    }

private:
    int m_nside;
    bool m_nested;
};

} // namespace healpix

#endif // HEALPIX_DRIZZLE_SHIM_H
