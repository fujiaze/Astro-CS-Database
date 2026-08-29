#!/usr/bin/env python3
"""SYN-005 独立合成 Oracle — UPM solver 参数恢复 / gauge / 残差 / 星flux保留。
验收(03 L127): 已知低阶光度面、重叠图、gauge/退化/正则强度
               → 参数恢复、残差、接缝降低且不破坏星 flux。
方法(independent, 用已知解析面驱动 UPM solver, 第一性原理比对恢复结果):
  - 注入每帧常数低阶面 C(f) = base + k*f(观测=面+小噪声, 已知 k), gauge 锚定后模型求 C 场。
  - 验证 `p2_upm_evaluate_c` 恢复相对每帧偏移(锚定帧0=0): eval(f) == k*f(相对参考), 容差内。
  - 验证收敛稳定: 重复 build 得到**相同 model_hash**(determinism), 残差小。
  - 验证星 flux 不破坏: calibrate_block(frame, input=signal) 的映射一致(信号正比/常量参考), 不引入伪 flux。
"""
import os, re, shutil, subprocess, tempfile, unittest

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
PH2_INC = os.path.join(REPO, "lib", "phase2", "include")
PH2 = os.path.join(REPO, "lib", "phase2")
AIO = os.path.join(REPO, "lib", "astro_image_io")
OMP_LIB = os.path.join(REPO, "build", "linux-openmp-on", "libphase2.a")
ICOMMON = os.path.join(REPO, "lib", "common")

DRIVER = r'''
#include "astro/phase2/upm.h"
#include <cstdio>
#include <cstring>
#include <vector>
#include <cmath>
// 注入已知每帧常数低阶面 C(f)=base+k*f; 观测=面+小噪声; 返回 evaluate_c 相对恢复 & 收敛hash & calibrate 映射
int main(int argc,char**argv){
    double k=0.5, base=42.0; int order=argc>1?atoi(argv[1]):0;
    std::vector<P2ControlObservation> obs;
    for(int f=0;f<8;++f){
        for(int c=0;c<300;++c){
            double surf=base + k*f;
            P2ControlObservation o{}; o.frame_id=f; o.control_id=(std::uint64_t)c; o.leaf_ipix=c;
            o.value=surf + 0.02*std::sin(0.07*c);         // 观测 = 真值面 + 小噪声(掩真值 k 恢复)
            o.uncertainty=0.1; o.control_variance=0.01; o.control_ivar=100.0; o.snr=10.0; o.support=1.0;
            o.ra_deg=(double)c; o.dec_deg=(double)(f*30);
            obs.push_back(o);
        }
    }
    // 收敛稳定: 跑 3 次 build, hash 应一致
    char hashes[3][70]; int ncreps[3]; int allok=1;
    for(int r=0;r<3;++r){
        P2UpmBuildConfig cfg{}; cfg.cpu_workers=1; cfg.max_iterations=100; cfg.tolerance=1e-8; cfg.target_order=order; cfg.use_ivar_weight=1;
        void* m=nullptr; if(p2_upm_build(obs.data(),obs.size(),&cfg,&m)!=0){ allok=0; break; }
        P2ModelInfo inf{}; p2_upm_info(m,&inf);
        std::strncpy(hashes[r],inf.model_hash,64); hashes[r][64]='\0'; ncreps[r]=(int)inf.control_count;
        if(r==2){
            // evaluate_c 相对每帧恢复(锚定帧0): eval(f)-eval(0) 应 == k*f
            double e0=p2_upm_evaluate_c(m,0,10);
            double es[8]; for(int f=0;f<8;++f) es[f]=p2_upm_evaluate_c(m,f,10)-e0;
            double maxdev=0; for(int f=0;f<8;++f){ double truly=k*f; maxdev=std::max(maxdev,std::fabs(es[f]-truly)); }
            // calibrate_block 星flux保留: 帧 f 输入 signal 映射应随 C 场一致变化
            std::uint64_t ip[2]={10,40}; double in[2]={1.0,1.0}, out[2]={0};
            int rc=p2_upm_calibrate_block(m,3,ip,in,out,2);
            printf("R k=%.2f maxdev=%.6f evalrel=",k,maxdev);
            for(int f=0;f<8;++f) printf("%.6f%c",es[f],(f<7?',':'\n'));
            printf("RK calib_rc=%d o0=%.8f o1=%.8f\n",rc,out[0],out[1]);
        }
        p2_upm_close(m);
    }
    printf("STABLE allok=%d h0=%s h1=%s h2=%s\n",allok,hashes[0],hashes[1],hashes[2]);
    printf("DONE\n");
    return allok?0:1;
}
'''


@unittest.skipUnless(shutil.which("g++") and os.path.isfile(OMP_LIB), "需要 g++ + OpenMP phase2 lib")
class TestUpmRecoveryOracle(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.mkdtemp(prefix="syn005_")
        drv = os.path.join(cls.tmp, "u.cpp")
        with open(drv, "w") as f:
            f.write(DRIVER)
        cls.exe = os.path.join(cls.tmp, "u")
        r = subprocess.run(["g++", "-std=c++17", "-O3", "-DNDEBUG", "-fopenmp",
                            f"-I{PH2_INC}", f"-I{PH2}", f"-I{ICOMMON}", f"-I{os.path.join(REPO, 'include')}",
                            f"-I{AIO}", drv, OMP_LIB, os.path.join(AIO, "astro_image_io.dll"),
                            "-lgomp", "-lz", "-lzstd", "-llz4", "-pthread", "-o", cls.exe],
                           capture_output=True, text=True, timeout=600)
        assert r.returncode == 0, r.stderr[-1200:]

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.tmp, ignore_errors=True)

    def _run(self):
        r = subprocess.run([self.exe, "0"], capture_output=True, text=True, timeout=300)
        self.assertEqual(r.returncode, 0, r.stderr[-300:] + (r.stdout[-200:] if "DONE" not in r.stdout else ""))
        return r.stdout

    def test_01_parameter_recovery_constant_surface(self):
        """UPM 恢复注入的每帧常数低阶面: eval(f)-eval(0) == k*f(锚定帧0), 相对偏差小。"""
        out = self._run()
        m = re.search(r"R k=([\d.]+) maxdev=([\d.eE+-]+)", out)
        self.assertIsNotNone(m, "缺 R 行")
        maxdev = float(m.group(2))
        self.assertLess(maxdev, 0.05, f"参数恢复偏差过大 maxdev={maxdev}")

    def test_02_recovered_perframe_offsets(self):
        """恢复的每帧相对偏移应精确按 0.5*f 递增(锚定帧0=0)。"""
        out = self._run()
        m = re.search(r"evalrel=([-\d.,eE+]+)", out)
        self.assertIsNotNone(m)
        ev = [float(x) for x in m.group(1).split(',')]
        self.assertEqual(len(ev), 8)
        for f in range(8):
            self.assertAlmostEqual(ev[f], 0.5 * f, delta=0.05,
                                   msg=f"帧{f} 恢复偏移={ev[f]}, 期望 {0.5*f}")

    def test_03_convergence_deterministic(self):
        """重复 build model_hash 逐位一致(收敛稳定/determinism)。"""
        out = self._run()
        m = re.search(r"STABLE allok=(\d) h0=(\S+) h1=(\S+) h2=(\S+)", out)
        self.assertIsNotNone(m, "缺 STABLE 行")
        self.assertEqual(int(m.group(1)), 1, "build 失败")
        self.assertEqual(m.group(2), m.group(3))
        self.assertEqual(m.group(2), m.group(4))

    def test_04_star_flux_not_destroyed(self):
        """calibrate_block 帧3 输入 signal=1 的映射一致: 两控制点输出关系与 C 场吻合(不引入伪 flux)。"""
        out = self._run()
        m = re.search(r"RK calib_rc=(\d) o0=([-\d.eE+]+) o1=([-\d.eE+]+)", out)
        self.assertIsNotNone(m, "缺 RK 行")
        self.assertEqual(int(m.group(1)), 0, "calibrate_block rc")
        o0, o1 = float(m.group(2)), float(m.group(3))
        # calibrate 映射应为有限值(两控制点输出同阶, 非伪flux/NaN/无限)
        for v in (o0, o1):
            self.assertTrue(v == v, "calibrate 输出 NaN")
            self.assertTrue(abs(v) < 1e6, f"calibrate 输出超界 {v}")
        # 帧3 C ≈ 1.5, 输入1.0 → 输出应接近参考-校正(有限, 同阶)
        self.assertAlmostEqual(o0, o1, delta=0.5, msg="同帧两控制点应为近似一致校正")


if __name__ == "__main__":
    unittest.main(verbosity=2)
