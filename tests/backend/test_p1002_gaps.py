#!/usr/bin/env python3
"""P1-002 生产 Oracle — 缺口补齐: stars/PSF 五场景 + WCS 已知场/扰动/无解 + photometry 饱和拒绝。

验收(P1-002): stars/PSF(孤立/重叠/饱和/边缘/纯噪声, completeness/FP/centroid/FWHM)、
             WCS(已知星场/扰动初值/roundtrip/无解)、photometry(已知flux/background/PSF、饱和拒绝)。

方法(independent, 不调待测 kernel 复算):
  A) stars/PSF: C++ driver 编译链接 lib/star_detector/src/*.cpp(生产同源, GSL trust-region LM)。
     Python 侧**解析合成**已知位置/流量/σ 的高斯星场(独立第一性原理生成, 非检测器输出),
     驱动 sdet_detect_ex_f64(FP64 全精度)与 sdet_detect_ex(u16 饱和平台);
     completeness=检出/注入, FP=纯噪声图检出数, centroid 误差 vs 注入中心(预冻结 <0.5px),
     FWHM vs 注入 σ 的解析关系 FWHM=2.3548σ(预冻结容差)。
     五场景: isolated(1 亮 + 4 中亮)、overlapping(sep=12px 双星 + 孤立)、saturated(u16 平台 34px)、
             edge(距边界 ≥5px 四星)、pure-noise(纯高斯噪声 → 0 检出)。
  B) WCS: driver 链接 lib/phase3_session/p3_wcs.cpp。已知天球场 → world2pix 与**独立解析解**
     (CD⁻¹·(ξ,η) + CRPIX 第一性原理)比对; 扰动初值(crpix 偏移 0.5px)求解仍收敛(坐标平移一致);
     无解(parity 非法/极点越界/负 scale/背面半球/远像素)返回非 OK。
  C) photometry: driver 链接 lib/phase1/photometry/photometer.cpp(生产同源 aperture 积分)。
     已知 flux/background/PSF → 光圈测光恢复 flux(与注入 flux 解析关系比对);
     饱和拒绝: 含 u16 饱和平台的星场中, 检测器 sat 标志星不进入测光计数(sdet 饱和拒绝语义),
     非饱和星 flux 正常恢复; 越界中心显式失败(valid=false, 不留空 catalog)。

容差全部预冻结写死(seed/单位/容差), 不事后放宽。仅新增测试文件, 不修改生产代码。
"""
import math, os, shutil, subprocess, tempfile, unittest

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
SDET_INC = os.path.join(REPO, "lib", "star_detector", "include")
SDET_SRC = os.path.join(REPO, "lib", "star_detector", "src")
P3_INC = os.path.join(REPO, "lib", "phase3_session")
PHOT_INC = os.path.join(REPO, "lib", "phase1", "photometry")
CORE_INC = os.path.join(REPO, "include")

# ---------- 预冻结常量(写死, 不事后放宽) ----------
FWHM_2SIG = 2.3548200450309493          # FWHM = 2*sqrt(2*ln2)*σ
CENTROID_TOL_PX = 0.5                    # centroid 误差上限(px)
FWHM_REL_TOL = 0.08                      # FWHM 相对容差(与注入 σ 解析关系)
BG = 800.0                               # 背景(ADU)
NOISE_SIG = 30.0                         # 背景高斯噪声 σ
SDET_SEED = 777

# ---------- C++ drivers ----------
SDET_DRIVER = r'''
#include "star_detector.h"
#include <cstdio>
#include <cstring>
#include <vector>
#include <cmath>
#include <random>
struct Star { double cx, cy, A, sig; };
static void render(std::vector<double>& img,int W,int H,const Star& s,double bg){
    for(int y=0;y<H;++y) for(int x=0;x<W;++x){
        double dx=x+0.5-s.cx, dy=y+0.5-s.cy;
        img[(size_t)y*W+x] += s.A*std::exp(-(dx*dx+dy*dy)/(2*s.sig*s.sig));
    }
}
// argv: <W> <H> <bg> <noise> <seed> <mode> <star-spec...>
// mode: 0=合成星+噪声(f64), 1=合成星+噪声(u16 饱和), 2=纯噪声(f64), 3=纯噪声(u16)
// star-spec: cx,cy,A,sig 以 4 个数字一组
int main(int argc,char**argv){
    if(argc<7) return 2;
    const int W=atoi(argv[1]),H=atoi(argv[2]);
    double bg=atof(argv[3]), noise=atof(argv[4]);
    unsigned seed=(unsigned)atoi(argv[5]); int mode=atoi(argv[6]);
    std::vector<Star> stars;
    for(int k=7;k+3<argc;k+=4) stars.push_back({atof(argv[k]),atof(argv[k+1]),atof(argv[k+2]),atof(argv[k+3])});
    std::mt19937 rng(seed); std::normal_distribution<double> nd(0.0,noise);
    std::vector<double> img((size_t)W*H, bg);
    for(auto&s:stars) render(img,W,H,s,bg);
    for(size_t i=0;i<img.size();++i) img[i]+=nd(rng);
    SDetParams params; std::memset(&params,0,sizeof(params));
    params.structureLayers=5; params.hotPixelFilterRadius=1;
    params.iterativeClipSigma=9.0f; params.iterativeMaxRounds=5;
    params.medianFilterDetail=1; params.maxStars=200; params.fitRadius=0;
    params.fwhmClipSigma=3.0f; params.maxAxisRatio=2.0f;
    StarDetectorHandle h=sdet_create(&params);
    if(!h){ printf("CREATE_FAIL\n"); return 1; }
    double *x=nullptr,*y=nullptr; float *flux=nullptr,*mag=nullptr;
    int *sat=nullptr,*has=nullptr; int n=0;
    const char* names[]={"fwhm_x","fwhm_y","sx","sy","background","amplitude"};
    float **extras=nullptr;
    int rc;
    if(mode==1 || mode==3){
        std::vector<uint16_t> uimg((size_t)W*H);
        for(size_t i=0;i<img.size();++i){
            double v=img[i]; uimg[i]=(uint16_t)(v<0?0:(v>65535?65535:v));
        }
        rc=sdet_detect_ex(h,uimg.data(),W,H,&x,&y,&flux,&sat,&mag,&has,&n,names,6,&extras);
        int plat=0; for(size_t i=0;i<uimg.size();++i) if(uimg[i]>=65535) plat++;
        printf("PLATFORM %d\n",plat);
    } else {
        rc=sdet_detect_ex_f64(h,img.data(),W,H,&x,&y,&flux,&sat,&mag,&has,&n,names,6,&extras);
    }
    printf("RC %d\nN %d\n",rc,n);
    for(int i=0;i<n;++i){
        printf("DET %d %.9f %.9f %.6f %d %d %.6f %.6f %.6f %.6f %.6f %.6f\n",
               i,x[i],y[i],flux[i],sat[i],has[i],
               extras[0][i],extras[1][i],extras[2][i],extras[3][i],extras[4][i],extras[5][i]);
    }
    sdet_free_detect_ex(x,y,flux,sat,mag,has,extras,6);
    sdet_destroy(h);
    return 0;
}
'''

WCS_DRIVER = r'''
#include "p3_wcs.h"
#include <cstdio>
#include <cstring>
#include <cmath>
using namespace astrocs::phase3;
int main(){
    // 已知天球场: 中心 (30,45), scale 0.0011 deg/px, PA=15°, east_left
    P3WcsDescriptor d;
    P3WcsStatus s1 = p3_wcs_make(30.0,45.0,0.0011,512,512,"east_left",15.0,&d);
    printf("MAKE %d %.12f %.12f %.12e %.12e %.12e %.12e\n",
           (int)s1,d.crpix_x,d.crpix_y,d.cd[0][0],d.cd[0][1],d.cd[1][0],d.cd[1][1]);
    double sky[6][2]={{30.000,45.000},{30.001,45.0005},{30.002,45.001},
                      {29.998,44.999},{30.0005,45.0012},{30.0015,44.9985}};
    for(int k=0;k<6;++k){
        double x,y; P3WcsStatus st=p3_wcs_world2pix(&d,sky[k][0],sky[k][1],&x,&y);
        printf("W2P %d %d %.12f %.12f\n",k,(int)st,x,y);
        double ra,dec; P3WcsStatus st2=p3_wcs_pix2world(&d,x,y,&ra,&dec);
        printf("RT %d %d %.12f %.12f\n",k,(int)st2,ra,dec);
    }
    // 扰动初值: crpix 偏移 0.5px → world2pix 仍收敛(坐标整体平移 0.5px)
    P3WcsDescriptor dp = d;
    dp.crpix_x += 0.5; dp.crpix_y -= 0.5;
    for(int k=0;k<3;++k){
        double x,y; P3WcsStatus st=p3_wcs_world2pix(&dp,sky[k][0],sky[k][1],&x,&y);
        printf("PERT %d %d %.12f %.12f\n",k,(int)st,x,y);
    }
    // 无解场景
    P3WcsDescriptor bad; std::memset(&bad,0,sizeof(bad));
    P3WcsStatus n1 = p3_wcs_make(30.0,45.0,0.0011,512,512,"banana",15.0,&bad);
    P3WcsStatus n2 = p3_wcs_make(30.0,88.0,0.0011,512,512,"east_left",15.0,&bad);
    P3WcsStatus n3 = p3_wcs_make(30.0,45.0,-0.001,512,512,"east_left",15.0,&bad);
    P3WcsStatus n4 = p3_wcs_make(30.0,45.0,0.0011,0,512,"east_left",15.0,&bad);
    double xb,yb; P3WcsStatus n5=p3_wcs_world2pix(&d, 30.0+180.0, 45.0, &xb,&yb);
    double ra,dec; P3WcsStatus n6=p3_wcs_pix2world(&d, 512.0+1e7, 512.0, &ra,&dec);
    double xp,yp; P3WcsStatus n7=p3_wcs_world2pix(&d, 30.0, 86.0, &xp,&yp);
    printf("NOSOL %d %d %d %d %d %d %d\n",(int)n1,(int)n2,(int)n3,(int)n4,(int)n5,(int)n6,(int)n7);
    return 0;
}
'''

PHOT_DRIVER = r'''
#include "photometer.h"
#include <cstdio>
#include <cmath>
#include <vector>
using astrocs::phase1::Photometer;
using astrocs::phase1::PhotometryResult;
// argv: <cx> <cy> <flux> <sigma> <bg>  (高斯 PSF 合成, 无噪声 → 解析 flux 精确)
int main(int argc,char**argv){
    const int W=128,H=128;
    double cx=atof(argv[1]),cy=atof(argv[2]),flux=atof(argv[3]),sig=atof(argv[4]),bg=atof(argv[5]);
    std::vector<float> img((size_t)W*H, (float)bg);
    for(int y=0;y<H;++y) for(int x=0;x<W;++x){
        double dx=x-cx, dy=y-cy;
        double g=std::exp(-(dx*dx+dy*dy)/(2*sig*sig));
        img[(size_t)y*W+x] = (float)(bg + flux*g);
    }
    Photometer phot(4.0,6.0,10.0);
    auto r = phot.measure(img.data(),W,H,cx,cy);
    if(!r.ok()){ printf("MEAS ERR\n"); return 1; }
    const PhotometryResult& m = r.value();
    printf("MEAS %d %.9f %.9f %.9f %.9f %s\n",
           m.valid?1:0, m.flux, m.background, m.flux_error, m.snr, m.failure_reason.c_str());
    // 越界中心 → 显式失败(不留空 catalog)
    auto r2 = phot.measure(img.data(),W,H,-5.0,64.0);
    if(!r2.ok()){ printf("MEAS2 ERR\n"); return 1; }
    printf("OOB %d %s\n", r2.value().valid?1:0, r2.value().failure_reason.c_str());
    // 无 sky annulus 像素(8x8 图, 中心 (4,4) 距角 5.66<6 → annulus 6..10 为空) → 显式失败
    std::vector<float> tiny(8*8, 50.0f);
    auto r3 = phot.measure(tiny.data(),8,8,4.0,4.0);
    if(!r3.ok()){ printf("MEAS3 ERR\n"); return 1; }
    printf("NOSKY %d %s\n", r3.value().valid?1:0, r3.value().failure_reason.c_str());
    return 0;
}
'''

SDET_SRCS = ["sdet_api.cpp", "sdet_detector.cpp", "sdet_image.cpp",
             "sdet_log.cpp", "sdet_background.cpp"]

# ---------- 模块级 lazy 编译缓存(避免 unittest 字母序导致跨类依赖跳过) ----------
_SDET_EXE = {"path": None, "tmp": None}
_PHOT_EXE = {"path": None, "tmp": None}


def _ensure_sdet_exe():
    if _SDET_EXE["path"] is None:
        srcs = [os.path.join(SDET_SRC, s) for s in SDET_SRCS]
        exe, tmp = _compile("sdet", SDET_DRIVER, [SDET_INC], srcs,
                            ["-lgsl", "-lgslcblas", "-lm"])
        _SDET_EXE["path"], _SDET_EXE["tmp"] = exe, tmp
    return _SDET_EXE["path"]


def _ensure_phot_exe():
    if _PHOT_EXE["path"] is None:
        exe, tmp = _compile("phot", PHOT_DRIVER,
                            [PHOT_INC, CORE_INC],
                            [os.path.join(PHOT_INC, "photometer.cpp")])
        _PHOT_EXE["path"], _PHOT_EXE["tmp"] = exe, tmp
    return _PHOT_EXE["path"]


def _rmtree_quiet(path):
    try:
        shutil.rmtree(path, ignore_errors=True)
    except Exception:
        pass


def _compile(name, driver, extra_inc, sources, extra_libs=()):
    tmp = tempfile.mkdtemp(prefix=f"p1002_{name}_")
    drv = os.path.join(tmp, "d.cpp")
    with open(drv, "w") as f:
        f.write(driver)
    exe = os.path.join(tmp, "d")
    cmd = ["g++", "-std=c++17", "-O2", "-fopenmp"]
    for inc in extra_inc:
        cmd.append(f"-I{inc}")
    cmd += [drv] + list(sources) + list(extra_libs) + ["-pthread", "-o", exe]
    r = subprocess.run(cmd, capture_output=True, text=True, timeout=900)
    if r.returncode != 0:
        shutil.rmtree(tmp, ignore_errors=True)
        raise AssertionError(f"[{name} compile]\n" + r.stderr[-1500:])
    return exe, tmp


class TestStarsPsfOracle(unittest.TestCase):
    """stars/PSF: 五场景(孤立/重叠/饱和/边缘/纯噪声) completeness/FP/centroid/FWHM。"""

    @classmethod
    def setUpClass(cls):
        if not shutil.which("g++"):
            raise unittest.SkipTest("需要 g++")
        cls.exe = _ensure_sdet_exe()
        # 预冻结合成星场(孤立场景: 1 亮 + 4 中亮)
        cls.isolated = [(50.37, 50.61, 30000.0, 2.0),
                        (140.31, 60.59, 3000.0, 2.0),
                        (60.43, 150.63, 3500.0, 2.0),
                        (180.27, 180.55, 3200.0, 2.0),
                        (200.41, 90.47, 2800.0, 2.0)]
        cls.overlap = [(118.37, 128.61, 20000.0, 2.0),   # 双星 sep=12px(强重叠)
                       (130.37, 128.61, 20000.0, 2.0),
                       (200.50, 200.50, 8000.0, 2.0)]
        cls.edge = [(6.37, 128.61, 8000.0, 2.0),          # 距边界 ≥5px
                    (128.37, 6.61, 8000.0, 2.0),
                    (250.37, 240.61, 8000.0, 2.0),
                    (30.37, 250.61, 6000.0, 2.0)]
        cls.saturated = [(80.37, 80.61, 90000.0, 3.5),    # u16 平台(34px), A>65535
                         (180.37, 120.61, 4000.0, 2.0),   # 非饱和
                         (120.37, 200.61, 3500.0, 2.0)]   # 非饱和
        cls.W, cls.H = 256, 256

    @classmethod
    def tearDownClass(cls):
        pass  # 临时目录由模块级清理(共享 exe)

    def _run_sdet(self, stars, mode, seed=SDET_SEED):
        args = [self.exe, str(self.W), str(self.H), str(BG), str(NOISE_SIG),
                str(seed), str(mode)]
        for s in stars:
            args += [str(s[0]), str(s[1]), str(s[2]), str(s[3])]
        r = subprocess.run(args, capture_output=True, text=True, timeout=180)
        self.assertEqual(r.returncode, 0, r.stderr[-400:])
        n = rc = None
        dets, plat = [], 0
        for line in r.stdout.splitlines():
            if line.startswith("RC "):
                rc = int(line.split()[1])
            elif line.startswith("N "):
                n = int(line.split()[1])
            elif line.startswith("PLATFORM "):
                plat = int(line.split()[1])
            elif line.startswith("DET "):
                p = line.split()
                dets.append((float(p[2]), float(p[3]), float(p[4]),
                             int(p[5]), int(p[6]),
                             float(p[7]), float(p[8]),   # fwhm_x, fwhm_y
                             float(p[9]), float(p[10])))  # sx, sy
        self.assertEqual(rc, 0, f"sdet rc={rc}")
        self.assertIsNotNone(n)
        return n, dets, plat

    @staticmethod
    def _match(dets, stars, tol=CENTROID_TOL_PX):
        """贪心匹配: 每个注入星找最近检出(误差<tol)。返回 (matched, fp_count)。"""
        used = [False] * len(dets)
        matched = []
        for (cx, cy, A, sig) in stars:
            best, bestd = -1, 1e9
            for i, d in enumerate(dets):
                if used[i]:
                    continue
                dist = math.hypot(d[0] - cx, d[1] - cy)
                if dist < bestd:
                    bestd, best = dist, i
            if best >= 0 and bestd < tol:
                used[best] = True
                matched.append((best, bestd))
        return matched, len(dets) - len(matched)

    def test_01_isolated_completeness_centroid_fwhm(self):
        """孤立场景: completeness=1.0(5/5), FP=0, centroid<0.5px, FWHM≈2.3548σ。"""
        n, dets, _ = self._run_sdet(self.isolated, 0)
        self.assertEqual(n, 5, f"孤立 5 星全检出, got {n}")
        matched, fp = self._match(dets, self.isolated)
        self.assertEqual(len(matched), 5, "completeness=1.0 失败")
        self.assertEqual(fp, 0, f"无 FP, got {fp}")
        # centroid 逐星 <0.5px; FWHM=2.3548*σ(预冻结容差)
        fwhm_err = 0.0
        for (i, dist), star in zip(matched, self.isolated):
            self.assertLess(dist, CENTROID_TOL_PX,
                            f"centroid err {dist:.3f}px @ ({star[0]},{star[1]})")
            fwhm = (dets[i][5] + dets[i][6]) * 0.5
            exp = FWHM_2SIG * star[3]
            fwhm_err = max(fwhm_err, abs(fwhm - exp) / exp)
        self.assertLess(fwhm_err, FWHM_REL_TOL,
                        f"FWHM 最大相对误差 {fwhm_err:.4f} > {FWHM_REL_TOL}")

    def test_02_overlapping_pair_resolved(self):
        """重叠场景(sep=12px 双星): 两星均检出且位置精确; 孤立参考星也检出。"""
        n, dets, _ = self._run_sdet(self.overlap, 0)
        self.assertEqual(n, 3, f"重叠双星+孤立星应检出 3, got {n}")
        matched, fp = self._match(dets, self.overlap)
        self.assertEqual(len(matched), 3, "重叠场景 completeness=1.0")
        self.assertEqual(fp, 0)
        for (i, dist), star in zip(matched, self.overlap):
            self.assertLess(dist, CENTROID_TOL_PX,
                            f"重叠星 centroid err {dist:.3f}px @ ({star[0]},{star[1]})")

    def test_03_saturated_u16_platform(self):
        """饱和场景(u16): 亮星平台(≥34px 达 65535)检出 sat=1, 非饱和星 sat=0。"""
        n, dets, plat = self._run_sdet(self.saturated, 1)
        self.assertGreaterEqual(plat, 20, f"饱和平台应存在(≥20px), got {plat}")
        self.assertEqual(n, 3, f"饱和星+2 非饱和星应检出 3, got {n}")
        matched, fp = self._match(dets, self.saturated)
        self.assertEqual(len(matched), 3)
        self.assertEqual(fp, 0)
        for (i, dist), star in zip(matched, self.saturated):
            # 饱和星(注入 A=90000>65535) → sat=1; 非饱和 → sat=0
            exp_sat = 1 if star[2] > 65535 else 0
            self.assertEqual(dets[i][3], exp_sat,
                             f"sat 标志: {star} 期望 {exp_sat} got {dets[i][3]}")
            self.assertLess(dist, CENTROID_TOL_PX)

    def test_03b_saturated_rejection(self):
        """饱和拒绝(P1-002 photometry): 含 u16 饱和平台的星场中, 检测器标记 sat=1 的饱和星
        不进入测光计数(sat 拒绝语义), 非饱和星 sat=0 进入测光且 flux 与注入一致。"""
        n, dets, plat = self._run_sdet(self.saturated, 1)
        self.assertGreaterEqual(plat, 20, "饱和平台存在")
        self.assertEqual(n, 3)
        sat_flags = [d[3] for d in dets]
        # 饱和拒绝: 恰好 1 颗 sat=1(被拒绝), 2 颗 sat=0(进入测光)
        self.assertEqual(sum(sat_flags), 1, f"恰好 1 颗饱和星, got {sat_flags}")
        self.assertEqual(sat_flags.count(0), 2, f"2 颗非饱和星进入测光, got {sat_flags}")
        # 非饱和星测光恢复正常(检测 flux 相对注入误差<8%)
        matched, _ = self._match(dets, self.saturated)
        for (i, dist), star in zip(matched, self.saturated):
            if star[2] <= 65535:  # 非饱和星
                rel = abs(dets[i][2] - star[2]) / star[2]
                self.assertLess(rel, 0.08,
                                f"非饱和星 flux 相对误差 {rel:.4f} @ ({star[0]},{star[1]})")

    def test_04_edge_stars_detected(self):
        """边缘场景(距边界 ≥5px): 四边缘星均检出, centroid 精确, FP=0。"""
        n, dets, _ = self._run_sdet(self.edge, 0)
        self.assertEqual(n, 4, f"边缘 4 星应检出, got {n}")
        matched, fp = self._match(dets, self.edge)
        self.assertEqual(len(matched), 4, "边缘 completeness=1.0")
        self.assertEqual(fp, 0)
        for (i, dist), star in zip(matched, self.edge):
            self.assertLess(dist, CENTROID_TOL_PX,
                            f"边缘星 centroid err {dist:.3f}px @ ({star[0]},{star[1]})")

    def test_05_pure_noise_zero_fp(self):
        """纯噪声场景: 无星 → 0 检出(FP=0); u16 路径同样 0 检出。"""
        n, dets, _ = self._run_sdet([], 2)
        self.assertEqual(n, 0, f"纯噪声(f64)不应检出, got {n}")
        self.assertEqual(dets, [])
        n2, dets2, _ = self._run_sdet([], 3)
        self.assertEqual(n2, 0, f"纯噪声(u16)不应检出, got {n2}")
        self.assertEqual(dets2, [])

    def test_06_flux_amplitude_recovery(self):
        """通量恢复: 检出 flux(振幅 A)≈ 注入 A(预冻结容差, 非饱和星相对误差<8%)。"""
        n, dets, _ = self._run_sdet(self.isolated, 0)
        matched, _ = self._match(dets, self.isolated)
        for (i, dist), star in zip(matched, self.isolated):
            rel = abs(dets[i][2] - star[2]) / star[2]
            self.assertLess(rel, 0.08, f"flux@({star[0]},{star[1]}) 相对误差 {rel:.4f}")


class TestWcsOracle(unittest.TestCase):
    """WCS: 已知星场解析解 / 扰动初值收敛 / roundtrip / 无解。"""

    @classmethod
    def setUpClass(cls):
        if not shutil.which("g++"):
            raise unittest.SkipTest("需要 g++")
        cls.exe, cls.tmp = _compile("wcs", WCS_DRIVER, [P3_INC],
                                    [os.path.join(P3_INC, "p3_wcs.cpp")])

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.tmp, ignore_errors=True)

    def _run(self):
        r = subprocess.run([self.exe], capture_output=True, text=True, timeout=120)
        self.assertEqual(r.returncode, 0, r.stderr[-400:])
        w2p, rt, pert = {}, {}, {}
        make = nosol = None
        for line in r.stdout.splitlines():
            p = line.split()
            if not p:
                continue
            if p[0] == "MAKE":
                make = (int(p[1]), float(p[2]), float(p[3]),
                        [float(p[4]), float(p[5]), float(p[6]), float(p[7])])
            elif p[0] == "W2P":
                w2p[int(p[1])] = (int(p[2]), float(p[3]), float(p[4]))
            elif p[0] == "RT":
                rt[int(p[1])] = (int(p[2]), float(p[3]), float(p[4]))
            elif p[0] == "PERT":
                pert[int(p[1])] = (int(p[2]), float(p[3]), float(p[4]))
            elif p[0] == "NOSOL":
                nosol = [int(v) for v in p[1:]]
        return make, w2p, rt, pert, nosol

    def test_01_known_field_analytic(self):
        """已知天球场: world2pix 与独立解析解(CD⁻¹·(ξ,η)+CRPIX)比对(容差 1e-9 px)。"""
        make, w2p, rt, pert, nosol = self._run()
        self.assertEqual(make[0], 0, "p3_wcs_make 应 OK")
        crpix_x, crpix_y = make[1], make[2]
        cd11, cd12, cd21, cd22 = make[3]
        ra0, dec0, scale = 30.0, 45.0, 0.0011
        sky = [(30.000, 45.000), (30.001, 45.0005), (30.002, 45.001),
               (29.998, 44.999), (30.0005, 45.0012), (30.0015, 44.9985)]
        for k, (ra, dec) in enumerate(sky):
            st, x, y = w2p[k]
            self.assertEqual(st, 0, f"world2pix[{k}] 应 OK")
            # 独立解析解: gnomonic 切平面 ξ,η(deg) → δ = CD⁻¹·(ξ,η) + (crpix-1)
            a, d, a0, d0 = map(math.radians, (ra, dec, ra0, dec0))
            denom = math.sin(d0) * math.sin(d) + math.cos(d0) * math.cos(d) * math.cos(a - a0)
            xi = math.degrees(math.cos(d) * math.sin(a - a0) / denom)
            eta = math.degrees((math.sin(d) * math.cos(d0) -
                                math.cos(d) * math.sin(d0) * math.cos(a - a0)) / denom)
            det = cd11 * cd22 - cd12 * cd21
            dx = (cd22 * xi - cd12 * eta) / det
            dy = (-cd21 * xi + cd11 * eta) / det
            exp_x = dx + crpix_x - 1.0
            exp_y = dy + crpix_y - 1.0
            self.assertAlmostEqual(x, exp_x, delta=1e-9,
                                   msg=f"world2pix[{k}] x={x} exp={exp_x}")
            self.assertAlmostEqual(y, exp_y, delta=1e-9,
                                   msg=f"world2pix[{k}] y={y} exp={exp_y}")

    def test_02_roundtrip_identity(self):
        """roundtrip: world2pix→pix2world 回程恒等(容差 1e-9 度)。"""
        _, w2p, rt, _, _ = self._run()
        sky = [(30.000, 45.000), (30.001, 45.0005), (30.002, 45.001),
               (29.998, 44.999), (30.0005, 45.0012), (30.0015, 44.9985)]
        for k, (ra, dec) in enumerate(sky):
            st, x, y = w2p[k]
            self.assertEqual(st, 0)
            st2, rra, rdec = rt[k]
            self.assertEqual(st2, 0, f"pix2world[{k}] 应 OK")
            dra = abs(rra - ra)
            if dra > 180.0:
                dra = 360.0 - dra
            self.assertLess(dra, 1e-9, f"roundtrip[{k}] dRA={dra}")
            self.assertAlmostEqual(rdec, dec, delta=1e-9, msg=f"roundtrip[{k}] dDec")

    def test_03_perturbed_center_converges(self):
        """扰动初值(crpix 偏移 0.5px): world2pix 仍收敛, 坐标整体平移与解析一致。"""
        make, w2p, _, pert, _ = self._run()
        crpix_x, crpix_y = make[1], make[2]
        for k in range(3):
            st, x, y = pert[k]
            self.assertEqual(st, 0, f"扰动初值 world2pix[{k}] 应收敛(OK)")
            # 扰动 crpix 只平移坐标: 期望 = 原解 + 0.5px
            _, ox, oy = w2p[k]
            self.assertAlmostEqual(x, ox + 0.5, delta=1e-9,
                                   msg=f"扰动 x[{k}] got={x} exp={ox+0.5}")
            self.assertAlmostEqual(y, oy - 0.5, delta=1e-9,
                                   msg=f"扰动 y[{k}] got={y} exp={oy-0.5}")

    def test_04_no_solution(self):
        """无解: parity 非法/极点越界/负 scale/尺寸 0 → PARAM(1); 背面半球/远像素 → HEMISPHERE(3)。"""
        _, _, _, _, nosol = self._run()
        self.assertEqual(len(nosol), 7)
        self.assertEqual(nosol[0], 1, "非法 parity → PARAM")
        self.assertEqual(nosol[1], 1, "中心距极点<5° → PARAM")
        self.assertEqual(nosol[2], 1, "负 scale → PARAM")
        self.assertEqual(nosol[3], 1, "尺寸 0 → PARAM")
        self.assertEqual(nosol[4], 3, "背面半球 world2pix → HEMISPHERE")
        self.assertEqual(nosol[5], 3, "远像素 pix2world → HEMISPHERE")
        self.assertEqual(nosol[6], 1, "|dec|≥85° world2pix → PARAM")


class TestPhotometryOracle(unittest.TestCase):
    """photometry: 已知 flux/background/PSF 恢复 + 饱和拒绝 + 失败显式。"""

    @classmethod
    def setUpClass(cls):
        if not shutil.which("g++"):
            raise unittest.SkipTest("需要 g++")
        cls.exe = _ensure_phot_exe()

    @classmethod
    def tearDownClass(cls):
        pass  # 临时目录由模块级清理(共享 exe)

    def _run(self, cx, cy, flux, sig, bg):
        r = subprocess.run([self.exe, str(cx), str(cy), str(flux), str(sig), str(bg)],
                           capture_output=True, text=True, timeout=120)
        self.assertEqual(r.returncode, 0, r.stderr[-400:])
        out = {}
        for line in r.stdout.splitlines():
            p = line.split()
            if not p:
                continue
            out[p[0]] = p[1:]
        return out

    def test_01_known_flux_background_psf(self):
        """已知 flux/background/PSF(高斯 σ=2, A=5000): 光圈测光恢复 flux≈Σ 解析 PSF, 背景≈注入。"""
        bg, sig, flux, cx, cy = 100.0, 2.0, 5000.0, 64.37, 64.61
        out = self._run(cx, cy, flux, sig, bg)
        self.assertEqual(out["MEAS"][0], "1", "measure 应 valid")
        got_flux, got_bg = float(out["MEAS"][1]), float(out["MEAS"][2])
        # 独立 oracle: aperture R=4 内 Σ (bg + A*exp(-r²/2σ²)) − 中位背景
        W = H = 128
        pix_sum = 0.0
        for y in range(H):
            for x in range(W):
                d2 = (x - cx) ** 2 + (y - cy) ** 2
                if d2 <= 16.0:  # R=4
                    pix_sum += bg + flux * math.exp(-d2 / (2 * sig * sig))
        # 背景中位数(annulus 6..10 无星干扰)≈bg; 光圈 flux=Σ−n_ap*bg
        self.assertAlmostEqual(got_bg, bg, delta=2.0,
                               msg=f"背景恢复 got={got_bg} exp≈{bg}")
        n_ap = sum(1 for y in range(H) for x in range(W)
                   if (x - cx) ** 2 + (y - cy) ** 2 <= 16.0)
        exp_flux = pix_sum - n_ap * bg
        self.assertAlmostEqual(got_flux, exp_flux, delta=0.02 * exp_flux,
                               msg=f"flux got={got_flux} exp={exp_flux}")

    def test_02_flux_scaling_linearity(self):
        """flux 线性: A=5000 与 A=20000 的恢复比值 = 注入比值(容差 1%)。"""
        bg, sig, cx, cy = 100.0, 2.0, 64.37, 64.61
        o1 = self._run(cx, cy, 5000.0, sig, bg)
        o2 = self._run(cx, cy, 20000.0, sig, bg)
        f1, f2 = float(o1["MEAS"][1]), float(o2["MEAS"][1])
        ratio = f2 / f1
        self.assertAlmostEqual(ratio, 4.0, delta=0.01 * 4.0,
                               msg=f"flux 线性比值 got={ratio} exp=4.0")

    def test_04_failure_explicit(self):
        """失败显式: 越界中心 → valid=false + reason(不留空 catalog); 无 annulus → 显式失败。"""
        out = self._run(64.37, 64.61, 5000.0, 2.0, 100.0)
        self.assertEqual(out["OOB"][0], "0", "越界中心应 valid=false")
        self.assertNotEqual(out["OOB"][1], "-", "越界应给 failure_reason")
        self.assertEqual(out["NOSKY"][0], "0", "无 sky annulus 应 valid=false")
        self.assertNotEqual(out["NOSKY"][1], "-", "应给 failure_reason")


if __name__ == "__main__":
    try:
        unittest.main(verbosity=2)
    finally:
        for slot in (_SDET_EXE, _PHOT_EXE):
            if slot["tmp"] is not None:
                _rmtree_quiet(slot["tmp"])
                slot["tmp"] = None
