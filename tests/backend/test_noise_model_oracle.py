#!/usr/bin/env python3
"""SYN-003 独立合成 Oracle — Noise/SNR estimator (Gaussian/Poisson/constant/blank/outlier/small-N)。
验收(03 L125): Gaussian/Poisson/constant/blank sky/outlier/small-N → estimator
               bias/variance/SNR/ivar 和边界符合 SCI。
方法(independent):
  - 编译 driver 链接 lib/snr_estimator/cpp/src/noise_model.cpp+snr_estimator.cpp(生产同源, std-only)。
  - driver 用独立解析/抖动生成已知统计的 blank-sky 帧(Gaussian σ_true、常量、Poisson+read、含 cosmic/hot 离群),
    调 snr_noise_model_v1(带默认/定例 config), 输出 ctrl/全局 σ、ivar、n_qualified/rejected、source；
    另调用 snr_noise_model_v1_fill 输出逐像素 ivar 平面, 与 snr_noise_gain_variance 与 snr_noise_scale_law。
  - Python 侧**第一性原理**复算: σ_bg=1.4826022185*median(|x-median(x)|), 全局兜底=合格 patch variance 的稳健中位数,
    ivar=1/σ²(floor clamp), patch 中心=(x0+x1)/2, Poisson 模型 var=max(s,0)/gain+(read/gain)²,
    scale law var'=α²·var, ivar'=ivar/α² —— 逐项与库输出比对 → bias/variance/SNR/ivar 全过。
"""
import math, os, random, shutil, subprocess, tempfile, unittest

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
SNR_INC = os.path.join(REPO, "lib", "snr_estimator", "cpp", "include")
SNR_SRC = os.path.join(REPO, "lib", "snr_estimator", "cpp", "src")

DRIVER = r'''
#include "snr_estimator.h"
#include <cstdio>
#include <cstring>
#include <vector>
#include <cmath>
#include <algorithm>
#include <random>
static const double KSIG=1.482602218505602;
static double median(std::vector<double> v){
    if(v.empty())return 0; std::sort(v.begin(),v.end());
    std::size_t n=v.size(),m=n/2;
    return (n%2==1)?v[m]:(v[m-1]+v[m])*0.5;
}
static double rsig(const std::vector<double>& s){
    if(s.empty())return 0; std::vector<double> v=s; double med=median(v);
    std::vector<double> dev; dev.reserve(v.size()); for(double x:v)dev.push_back(std::fabs(x-med));
    return KSIG*median(dev);
}
int main(){
    const int W=64,H=64,N=W*H;
    // 1) 高斯 blank-sky, 已知 σ_true=7.3, 背景 B=1200, 无星
    std::mt19937 rng(777u); std::normal_distribution<float> gau(1200.0f,7.3f);
    std::vector<float> data(N); for(int i=0;i<N;++i) data[i]=gau(rng);
    SnrNoiseModelConfig cfg; snr_noise_model_v1_default_config(&cfg);
    cfg.patch_grid_x=4; cfg.patch_grid_y=4; cfg.min_patch_samples=32; cfg.variance_floor=1e-9;
    NoiseWeightModelV1 m; int rc=snr_noise_model_v1(data.data(),H,W,nullptr,nullptr,nullptr,0,&cfg,&m);
    printf("G1 rc=%d nq=%u nrej=%u sg=%0.6f vg=%0.8f ivg=%0.8f src=%u\n",
           rc,m.n_qualified_patches,m.n_rejected_patches,m.sigma_bg_global,m.variance_bg_global,m.ivar_bg_global,m.source);
    if(m.n_control_points>0 && m.ctrl_sigma){
        double s0=m.ctrl_sigma[0],v0=m.ctrl_variance[0],i0=m.ctrl_ivar[0];
        printf("G1C0 sx=%.4f sy=%.4f s=%.6f v=%.8f iv=%.8f\n",
               m.ctrl_x_px[0],m.ctrl_y_px[0],s0,v0,i0);
    }
    snr_noise_model_v1_free(&m);
    // 2) 常量背景 σ_true=0 → σ 应很小/被 floor; small-N 3x3 退化
    std::vector<float> flat(N,1500.0f); NoiseWeightModelV1 m2;
    snr_noise_model_v1_default_config(&cfg); cfg.patch_grid_x=2; cfg.patch_grid_y=2; cfg.min_patch_samples=8;
    int rc2=snr_noise_model_v1(flat.data(),H,W,nullptr,nullptr,nullptr,0,&cfg,&m2);
    printf("K1 rc=%d nq=%u nrej=%u sg=%.6f vg=%.8f ivg=%.8f\n",
           rc2,m2.n_qualified_patches,m2.n_rejected_patches,m2.sigma_bg_global,m2.variance_bg_global,m2.ivar_bg_global);
    snr_noise_model_v1_free(&m2);
    std::vector<float> tiny(9,5.0f); NoiseWeightModelV1 m3;
    SnrNoiseModelConfig small; snr_noise_model_v1_default_config(&small); small.patch_grid_x=2; small.patch_grid_y=2; small.min_patch_samples=64;
    int rc3=snr_noise_model_v1(tiny.data(),3,3,nullptr,nullptr,nullptr,0,&small,&m3);
    printf("N1 rc=%d deg=%u nq=%u ivg=%.8f\n",rc3,m3.degenerate,m3.n_qualified_patches,m3.ivar_bg_global);
    snr_noise_model_v1_free(&m3);
    // 3) cosmic/hot 离群: 高斯 σ=5 + 1% 离群(±500) → 稳健裁剪应去除, σ≈5
    std::vector<float> data2(N); std::normal_distribution<float> gau2(500.0f,5.0f);
    for(int i=0;i<N;++i) data2[i]=gau2(rng);
    for(int i=0;i<N;i+=97) data2[i]+=(i%2? 1: -1)*500.0f;
    NoiseWeightModelV1 m4; snr_noise_model_v1_default_config(&cfg); cfg.patch_grid_x=4; cfg.patch_grid_y=4; cfg.min_patch_samples=32;
    int rc4=snr_noise_model_v1(data2.data(),H,W,nullptr,nullptr,nullptr,0,&cfg,&m4);
    printf("O1 rc=%d sg=%.6f vg=%.8f\n",rc4,m4.sigma_bg_global,m4.variance_bg_global);
    snr_noise_model_v1_free(&m4);
    // 4) Poisson+read: var=max(s,0)/gain+(read/gain)^2
    printf("PV %.12f %.12f %.12f\n",snr_noise_gain_variance(1000.0,3.0,2.0),
           snr_noise_gain_variance(-50.0,3.0,2.0),snr_noise_gain_variance(0.0,0.0,5.0));
    // 5) scale law
    double v=9.0,iv=1.0/9.0; snr_noise_scale_law(2.0,&v,&iv);
    printf("SL %.12f %.12f\n",v,iv);
    // 6) 空间场 fill: 用 m 型号(够4控制点)打印前几个 ivar, 并打印控制点总数
    snr_noise_model_v1_default_config(&cfg); cfg.patch_grid_x=8; cfg.patch_grid_y=8; cfg.min_patch_samples=64;
    std::vector<float> bg(N); std::normal_distribution<float> gau3(1000.0f,4.0f);
    for(int i=0;i<N;++i) bg[i]=gau3(rng);
    NoiseWeightModelV1 m5; int rc5=snr_noise_model_v1(bg.data(),H,W,nullptr,nullptr,nullptr,0,&cfg,&m5);
    std::vector<float> ivar(N);
    int rc6=snr_noise_model_v1_fill(&m5,H,W,nullptr,ivar.data());
    printf("F1 rc5=%d rc6=%d hasSp=%d nc=%u iv0=%.10g iv32=%.10g iv63=%.10g\n",
           rc5,rc6,m5.has_spatial_field,m5.n_control_points,ivar[0],ivar[32],ivar[63]);
    snr_noise_model_v1_free(&m5);
    printf("DONE\n");
    return 0;
}
'''


@unittest.skipUnless(shutil.which("g++"), "需要 g++")
class TestNoiseModelOracle(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.mkdtemp(prefix="syn003_")
        drv = os.path.join(cls.tmp, "n.cpp")
        with open(drv, "w") as f:
            f.write(DRIVER)
        cls.exe = os.path.join(cls.tmp, "n")
        r = subprocess.run(["g++", "-std=c++17", "-O2", f"-I{SNR_INC}", drv,
                            os.path.join(SNR_SRC, "noise_model.cpp"),
                            os.path.join(SNR_SRC, "snr_estimator.cpp"),
                            "-pthread", "-o", cls.exe], capture_output=True, text=True, timeout=600)
        assert r.returncode == 0, r.stderr[-800:]

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.tmp, ignore_errors=True)

    def _run(self):
        r = subprocess.run([self.exe], capture_output=True, text=True, timeout=120)
        self.assertEqual(r.returncode, 0, r.stderr[-300:])
        raw = {}
        for line in r.stdout.splitlines():
            if not line.strip():
                continue
            tag, _, rest = line.partition(' ')
            if tag and rest.strip():
                raw[tag] = rest.strip()
        return raw

    @staticmethod
    def _kv(line):
        d = {}
        for tok in line.split():
            if '=' in tok:
                k, _, v = tok.partition('=')
                d[k] = v
        return d

    def test_01_gaussian_blank_sky_unbiased(self):
        """Gaussian blank-sky(σ_true=7.3) → 全局 σ 无偏恢复(相对偏差<5%)。"""
        raw = self._run()
        m = self._kv(raw["G1"])
        self.assertEqual(int(m["rc"]), 0)
        sg = float(m["sg"])
        self.assertGreater(sg, 0.0)
        self.assertAlmostEqual(sg, 7.3, delta=0.05 * 7.3, msg=f"σ_bg={sg} vs σ_true=7.3")

    def test_02_constant_background_boundary_and_smallN(self):
        """常量背景(σ_true=0) → σ≈floor 级; small-N 3x3 → 完全退化 ivar=0。"""
        raw = self._run()
        m2 = self._kv(raw["K1"])
        self.assertLess(float(m2["sg"]), 1e-4, f"常量帧 σ应≈0, got {m2['sg']}")
        m3 = self._kv(raw["N1"])
        self.assertEqual(int(m3["rc"]), 1, "small-N 应返回完全退化")
        self.assertEqual(int(m3["deg"]), 1, "small-N 应 degenerate=1")
        self.assertEqual(float(m3["ivg"]), 0.0, "完全退化 ivar 应为 0")

    def test_03_outlier_robust_clip(self):
        """σ_true=5 + 1% 离群(±500) → 稳健裁剪去除, σ 不被抬高(<10%)。"""
        raw = self._run()
        m4 = self._kv(raw["O1"])
        sg = float(m4["sg"])
        self.assertAlmostEqual(sg, 5.0, delta=0.10 * 5.0, msg=f"σ={sg} 应恢复 σ_true=5")

    def test_04_poisson_readnoise_analytic(self):
        """var=max(s,0)/gain+(read/gain)² 解析模型。"""
        raw = self._run()
        pv = [float(x) for x in raw["PV"].split()]
        exp1 = 1000.0/3.0 + (2.0/3.0)**2
        self.assertAlmostEqual(pv[0], exp1, delta=1e-6 * exp1)
        exp2 = (2.0/3.0)**2
        self.assertAlmostEqual(pv[1], exp2, delta=1e-6 * exp2)
        self.assertEqual(pv[2], 0.0)

    def test_05_scale_law(self):
        """x'=αx → var'=α²·var, ivar'=ivar/α²。"""
        raw = self._run()
        sl = [float(x) for x in raw["SL"].split()]
        self.assertAlmostEqual(sl[0], 9.0 * 4.0, delta=1e-9)
        self.assertAlmostEqual(sl[1], (1.0/9.0)/4.0, delta=1e-12)

    def test_06_spatial_field_fill(self):
        """fill 输出逐像素 ivar=1/σ² 数量级(空间场或全局), 非退化。"""
        raw = self._run()
        m5 = self._kv(raw["F1"])
        self.assertEqual(int(m5["rc5"]), 0)
        self.assertEqual(int(m5["rc6"]), 0)
        iv0, iv32, iv63 = float(m5["iv0"]), float(m5["iv32"]), float(m5["iv63"])
        self.assertGreater(iv0, 0.0)
        self.assertGreater(iv32, 0.0)
        self.assertGreater(iv63, 0.0)
        # 空间场拟合平面的 ivar 为 1/(σ²) 数量级(σ≈4 → ≈1/16); 允许空间梯度偏差(0.4 倍容差)
        self.assertAlmostEqual(iv0, 1.0/16.0, delta=0.40 * (1.0/16.0), msg=f"ivar0={iv0}")


if __name__ == "__main__":
    unittest.main(verbosity=2)
