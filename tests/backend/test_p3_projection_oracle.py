#!/usr/bin/env python3
"""P3-001 独立投影 Oracle — 版本化 projection registry 与冻结四投影
(TAN/SIN/CAR/AIT) Python 侧对拍面 (tests/backend)。

合同锚: ALG-P3-PROJ-IMPL-001 §15 (docs/algorithms/PHASE3_PROJ_IMPL.md,
P3-001 新 claim) + SCI-P3-001 (FROZEN, TAN 往返容差 1e-6 px 零改动) +
宪章 §7.3/§18.1 (首批四投影 registry 冻结)。

方法 (independent, 不调生产实现复算):
  A) C++ driver 内联编译 lib/phase3_proj/p3_projection.cpp (生产同源,
     registry 直调), 网格输出每采样点 (x,y,ra,dec) 与往返 (x2,y2) 文本。
  B) Python/numpy 侧**第一性独立实现**四投影正反映射:
     TAN/SIN = 3D 单位向量法 (CRVAL 正交基切平面重建/透视除法, 与生产
     球面三角公式完全不同路径); CAR/AIT = θ₀=+90° 恒等旋转 + Paper II
     反演式独立书写; world2pix = 独立 CD⁻¹ 解析逆。
  C) 对拍容差全部预冻结写死: 解析比对 <1e-9 deg (FP64 机器精度量级)、
     往返 <1e-6 px (SCI §7 冻结值, 禁放宽)。
  D) 确定性: driver 全网格输出 sha256, 两次独立运行必须逐字节一致。

仅新增测试文件, 不修改生产代码。
"""
import hashlib
import math
import os
import re
import shutil
import subprocess
import tempfile
import unittest

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
P3PROJ_INC = os.path.join(REPO, "lib", "phase3_proj")

# ---------- 预冻结常量(写死, 不事后放宽) ----------
ROUNDTRIP_TOL_PX = 1e-6      # SCI-P3-001 §7 冻结往返容差
ORACLE_TOL_DEG = 1e-9        # 解析解比对(FP64 机器精度量级)

# ---------- C++ driver(内联编译生产源, 文本协议) ----------
PROJ_DRIVER = r'''
#include "p3_projection.h"
#include <cstdio>
#include <cstring>
#include <string>
using namespace astrocs::phase3proj;
int main(int argc, char** argv) {
    if (argc < 7) { std::fprintf(stderr, "usage: <code> <ra0> <dec0> <scale> <W> <H>\n"); return 2; }
    const char* code = argv[1];
    const double ra0 = atof(argv[2]), dec0 = atof(argv[3]), scale = atof(argv[4]);
    const int W = atoi(argv[5]), H = atoi(argv[6]);
    const P3ProjectionSpec* spec = p3_projection_registry_find(code);
    if (!spec) { std::fprintf(stderr, "unknown projection code\n"); return 2; }
    P3ProjectionDescriptor d;
    const P3ProjectionStatus st = p3_projection_make(
        spec->id, ra0, dec0, scale, W, H, "east_left", 0.0, &d);
    if (st != P3ProjectionStatus::P3_PROJ_OK) { std::fprintf(stderr, "make rc=%d\n", (int)st); return 3; }
    std::printf("SELFCHK %d\n", p3_projection_registry_selfcheck());
    std::printf("DESC %.17g %.17g %.17g %.17g %.17g %.17g %.17g %.17g\n",
                d.crpix_x, d.crpix_y, d.cd[0][0], d.cd[0][1], d.cd[1][0], d.cd[1][1],
                (double)d.width_px, (double)d.height_px);
    std::printf("KW %s\n", p3_projection_fits_keywords(&d).c_str());
    const int NX = W < 23 ? W : 23, NY = H < 17 ? H : 17;
    for (int j = 0; j < NY; ++j)
        for (int i = 0; i < NX; ++i) {
            const double x = (W - 1) * (double)i / (double)(NX - 1);
            const double y = (H - 1) * (double)j / (double)(NY - 1);
            double ra, dec, x2, y2;
            const P3ProjectionStatus s1 = p3_projection_pix2world(&d, x, y, &ra, &dec);
            if (s1 != P3ProjectionStatus::P3_PROJ_OK) { std::printf("PT %.17g %.17g SKIP %d\n", x, y, (int)s1); continue; }
            if (p3_projection_world2pix(&d, ra, dec, &x2, &y2) != P3ProjectionStatus::P3_PROJ_OK) {
                std::printf("PT %.17g %.17g %.17g %.17g SKIP2 %d\n", x, y, ra, dec, 3);
                continue;
            }
            std::printf("PT %.17g %.17g %.17g %.17g %.17g %.17g %d %d\n",
                        x, y, ra, dec, x2, y2, (int)s1, 0);
        }
    return 0;
}
'''


def build_driver(workdir: str) -> str:
    src = os.path.join(workdir, "p3_projection_driver.cpp")
    exe = os.path.join(workdir, "p3_projection_driver")
    with open(src, "w") as f:
        f.write(PROJ_DRIVER)
    cmd = ["g++", "-std=c++17", "-O2", "-I", P3PROJ_INC, src,
           os.path.join(REPO, "lib", "phase3_proj", "p3_projection.cpp"),
           "-o", exe]
    r = subprocess.run(cmd, capture_output=True, text=True, timeout=180)
    if r.returncode != 0:
        raise RuntimeError(f"driver build failed: {r.stderr}")
    return exe


def run_driver(exe: str, code: str, ra0: float, dec0: float, scale: float,
               w: int, h: int) -> str:
    r = subprocess.run([exe, code, str(ra0), str(dec0), str(scale),
                        str(w), str(h)], capture_output=True, text=True, timeout=120)
    if r.returncode != 0:
        raise RuntimeError(f"driver run failed rc={r.returncode}: {r.stderr}")
    return r.stdout


# ---------- numpy 独立 oracle(第一性, 不调生产) ----------
def sky_vec(ra_deg, dec_deg):
    a = math.radians(ra_deg)
    d = math.radians(dec_deg)
    return (math.cos(d) * math.cos(a), math.cos(d) * math.sin(a), math.sin(d))


def cross(a, b):
    return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2],
            a[0] * b[1] - a[1] * b[0])


def basis(ra0, dec0):
    up = sky_vec(ra0, dec0)
    east = (-math.sin(math.radians(ra0)), math.cos(math.radians(ra0)), 0.0)
    north = cross(up, east)
    return up, east, north


def norm_wrap(ra):
    ra = math.fmod(ra, 360.0)
    return ra + 360.0 if ra < 0 else ra


def oracle_pix2world(code, ra0, dec0, x_deg, y_deg):
    """独立正反映射: x_deg,y_deg = CD 平面中间坐标 (deg)。返回 (ra,dec) 或 None。"""
    if code == "TAN":
        # gnomonic 第一性: 切平面点(rad) → 球心连线交球面方向
        up, east, north = basis(ra0, dec0)
        x, y = math.radians(x_deg), math.radians(y_deg)
        w = tuple(up[i] + x * east[i] + y * north[i] for i in range(3))
        wn = math.sqrt(sum(v * v for v in w))
        dec = math.degrees(math.asin(max(-1.0, min(1.0, w[2] / wn))))
        dra = math.atan2(w[1], w[0]) - math.atan2(up[1], up[0])
        dra = (dra + math.pi) % (2.0 * math.pi) - math.pi
        return norm_wrap(ra0 + math.degrees(dra)), dec
    if code == "SIN":
        # orthographic: 球面点 = zu·up + x·east + y·north (zu=√(1−x²−y²))
        up, east, north = basis(ra0, dec0)
        x, y = math.radians(x_deg), math.radians(y_deg)
        r2 = x * x + y * y
        if r2 > 1.0:
            return None
        zu = math.sqrt(max(0.0, 1.0 - r2))
        w = tuple(zu * up[i] + x * east[i] + y * north[i] for i in range(3))
        dec = math.degrees(math.asin(max(-1.0, min(1.0, w[2]))))
        dra = math.atan2(w[1], w[0]) - math.atan2(up[1], up[0])
        dra = (dra + math.pi) % (2.0 * math.pi) - math.pi
        return norm_wrap(ra0 + math.degrees(dra)), dec
    if code == "CAR":
        # θ₀=+90° 恒等旋转: x=φ, y=−θ
        ra = norm_wrap(ra0 + x_deg)
        dec = -y_deg
        return (ra, dec) if abs(dec) <= 90.0 else None
    if code == "AIT":
        # Paper II 反演独立式: D²=2−X²/4−Y², sinθ=Y·D, φ=2·atan2(X·D/2, D²−1)
        X, Y = math.radians(x_deg), math.radians(y_deg)
        dsq = 2.0 - (X * X / 4.0 + Y * Y)
        if dsq <= 0.0:
            return None
        dq = math.sqrt(dsq)
        sth = Y * dq
        if abs(sth) > 1.0:
            return None
        dec = math.degrees(math.asin(sth))
        phi = 2.0 * math.atan2(X * dq / 2.0, dsq - 1.0)
        return norm_wrap(ra0 + math.degrees(phi)), dec
    raise AssertionError(f"unknown code {code}")


def oracle_world2pix(code, ra0, dec0, desc, ra, dec):
    """独立正向: (ra,dec) → 像素 (0-based)。返回 (x,y) 或 None。"""
    crpix_x, crpix_y = desc[0], desc[1]
    (cd11, cd12, cd21, cd22) = desc[2], desc[3], desc[4], desc[5]
    det = cd11 * cd22 - cd12 * cd21
    if code in ("TAN", "SIN"):
        up, east, north = basis(ra0, dec0)
        v = sky_vec(ra, dec)
        xi = sum(v[i] * east[i] for i in range(3))      # = −cosθ sinφ
        eta = sum(v[i] * north[i] for i in range(3))    # = cosθ cosφ
        sin_th = sum(v[i] * up[i] for i in range(3))    # = sinθ
        if sin_th <= 0.0:
            return None
        x_rad = xi / sin_th if code == "TAN" else xi    # TAN 透视除法
        y_rad = eta / sin_th if code == "TAN" else eta
    elif code == "CAR":
        dra = math.fmod(ra - ra0, 360.0)
        if dra <= -180.0:
            dra += 360.0
        if dra > 180.0:
            dra -= 360.0
        x_rad, y_rad = math.radians(dra), math.radians(-dec)
    elif code == "AIT":
        dra = math.fmod(ra - ra0, 360.0)
        if dra <= -180.0:
            dra += 360.0
        if dra > 180.0:
            dra -= 360.0
        phi, th = math.radians(dra), math.radians(dec)
        dq = math.sqrt(1.0 + math.cos(th) * math.cos(phi / 2.0))
        x_rad = 2.0 * math.cos(th) * math.sin(phi / 2.0) / dq
        y_rad = math.sin(th) / dq
    else:
        raise AssertionError(code)
    xd, yd = math.degrees(x_rad), math.degrees(y_rad)
    dx = (cd22 * xd - cd12 * yd) / det
    dy = (-cd21 * xd + cd11 * yd) / det
    return dx + crpix_x - 1.0, dy + crpix_y - 1.0


# ---------- 用例 ----------
class TestP3ProjectionOracle(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.workdir = tempfile.mkdtemp(prefix="p3_proj_oracle_")
        cls.exe = build_driver(cls.workdir)

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.workdir, ignore_errors=True)

    CASES = [
        ("TAN", 350.0, 30.0, 0.002, 97, 89),   # 跨 RA wrap
        ("SIN", 350.0, 30.0, 0.02, 97, 89),
        ("CAR", 350.0, 30.0, 0.02, 97, 89),
        ("AIT", 350.0, 30.0, 0.5, 97, 89),
        ("TAN", 10.0, -60.0, 0.001, 64, 64),
        ("SIN", 10.0, -60.0, 0.01, 64, 64),
        ("CAR", 10.0, -60.0, 0.01, 64, 64),
        ("AIT", 10.0, -60.0, 0.25, 64, 64),
        ("TAN", 0.0, 85.0, 0.001, 32, 32),     # 中心守卫边界内
        ("SIN", 0.0, 85.0, 0.01, 32, 32),
        ("CAR", 0.0, 85.0, 0.01, 32, 32),
        ("AIT", 0.0, 85.0, 0.05, 32, 32),
    ]

    def test_ctype_and_registry(self):
        """CTYPE 面经 registry 解析 + driver 自检全过。"""
        expect = {"TAN": ("RA---TAN", "DEC--TAN"), "SIN": ("RA---SIN", "DEC--SIN"),
                  "CAR": ("RA---CAR", "DEC--CAR"), "AIT": ("RA---AIT", "DEC--AIT")}
        for code, ra0, dec0, scale, w, h in self.CASES:
            out = run_driver(self.exe, code, ra0, dec0, scale, w, h)
            m = re.search(r"SELFCHK (\d+)", out)
            self.assertEqual(int(m.group(1)), 0, f"{code} registry selfcheck")
            self.assertIn(f"CTYPE1= '{expect[code][0]}'", out)
            self.assertIn(f"CTYPE2= '{expect[code][1]}'", out)
            for line in re.search(r"KW (.*)", out).group(1).split("\n"):
                self.assertLessEqual(len(line), 80, f"{code} FITS 行宽")

    def test_oracle_grid(self):
        """四投影全网格: 生产 vs numpy 独立 oracle(<1e-9 deg) + 往返(<1e-6 px)
        + 正向独立解析解。"""
        for code, ra0, dec0, scale, w, h in self.CASES:
            out = run_driver(self.exe, code, ra0, dec0, scale, w, h)
            desc = [float(t) for t in
                    re.search(r"DESC (.*)", out).group(1).split()]
            n_ok = n_skip = 0
            for line in out.splitlines():
                if not line.startswith("PT "):
                    continue
                tok = line.split()
                x, y = float(tok[1]), float(tok[2])
                if tok[3] == "SKIP":
                    n_skip += 1
                    continue
                ra, dec = float(tok[3]), float(tok[4])
                # 独立 oracle 正向(pix2world)
                o = oracle_pix2world(code, ra0, dec0,
                                     (x + 1 - desc[0]) * desc[2] + (y + 1 - desc[1]) * desc[3],
                                     (x + 1 - desc[0]) * desc[4] + (y + 1 - desc[1]) * desc[5])
                self.assertIsNotNone(o, f"{code} oracle 域内点 ({x},{y})")
                dra = abs(ra - o[0])
                if dra > 180.0:
                    dra = 360.0 - dra
                self.assertLess(dra, ORACLE_TOL_DEG,
                                f"{code} pix2world vs oracle ({x},{y})")
                self.assertLess(abs(dec - o[1]), ORACLE_TOL_DEG,
                                f"{code} dec vs oracle ({x},{y})")
                if tok[5] == "SKIP2":
                    n_skip += 1  # 往返 |dec|>85° 冻结边界拒(域外像素)
                    continue
                x2, y2 = float(tok[5]), float(tok[6])
                # 独立 oracle 反向(world2pix)
                p = oracle_world2pix(code, ra0, dec0, desc, ra, dec)
                self.assertIsNotNone(p, f"{code} oracle world2pix ({ra},{dec})")
                err = math.hypot(p[0] - x, p[1] - y)
                self.assertLess(err, ROUNDTRIP_TOL_PX,
                                f"{code} world2pix vs oracle ({ra},{dec})")
                # 往返(冻结 <1e-6 px)
                self.assertLess(math.hypot(x2 - x, y2 - y), ROUNDTRIP_TOL_PX,
                                f"{code} roundtrip ({x},{y})")
                n_ok += 1
            self.assertGreater(n_ok, 0, f"{code} 有效采样点")

    def test_determinism_two_runs(self):
        """两次独立运行 stdout sha256 逐字节一致(跨进程确定性)。"""
        for code, ra0, dec0, scale, w, h in self.CASES:
            h1 = hashlib.sha256(
                run_driver(self.exe, code, ra0, dec0, scale, w, h).encode()
            ).hexdigest()
            h2 = hashlib.sha256(
                run_driver(self.exe, code, ra0, dec0, scale, w, h).encode()
            ).hexdigest()
            self.assertEqual(h1, h2, f"{code} 跨进程确定性")


if __name__ == "__main__":
    unittest.main()
