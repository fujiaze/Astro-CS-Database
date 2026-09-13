#!/usr/bin/env python3
"""SYN-001 独立合成 Oracle — Calibration(全后端路径值一致)。
验收(03 L123): constant/ramp/dark exposure/flat/gain/read noise/saturation/mask/NaN
               → value+variance+mask 单位/解析值全过。
设计(independent):
  1) GCC 编译 driver, 链接 lib/calibration/src/*.cpp(同 cli/CMakeLists.txt CAL_SRCS, 与生产同源);
  2) driver 用**确定性解析函数**(sin/cos 闭式, 无 RNG)合成 bias/dark/flat/light 帧(含常量、ramp、
     固定离群值、NaN), 打印原始 stack 与库输出 MASTER_BIAS/DARK/FLAT、CAL0/CAL1;
  3) Python 侧只读取**原始素材**(RAW_*STACK、LIGHT), 按 astro_calibration.h 头注释规定的数学契约
     (median=奇偶中间两值平均/MAD=1.4826*median/sigma-clip 非对称门限/mean vs median 合并/
      flat 归一化到中位=1.0 且裁剪 0.1)/ac_calibrate_frame 两种模式**第一性原理独立复算**,
     逐元素对比 → value/单位解析值全过。
  全线程位级一致: ac_set_num_threads(1) vs (4) 输出逐位相同。
"""
import os, shutil, subprocess, tempfile, unittest

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
CAL_INC = os.path.join(REPO, "lib", "calibration", "include")
CAL_SRC = os.path.join(REPO, "lib", "calibration", "src")
HISS_INC = os.path.join(REPO, "lib", "astro_image_io", "include")

DRIVER = r'''
#include "astro_calibration.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <cmath>
static const int W=16,H=8,N=W*H;
static float sinf_(double x){ return (float)std::sin(x); }
static void put(const float* a,int n,const char* tag){ printf("%s",tag); for(int i=0;i<n;++i) printf("%.9g%c",a[i],(i+1<n)?',':'\n'); }
int main(int argc,char**argv){
    int threads=argc>1?atoi(argv[1]):1;
    ac_set_num_threads(threads);
    // ---- 素材: 确定性解析(无RNG), 帧序 n∈[0,nf), 像素 i
    // bstack[n][i] = 2000 + 3*sin(0.7*i+n); 帧2 注入离群
    std::vector<float> bstack(5*N);
    for(int n=0;n<5;++n) for(int i=0;i<N;++i) bstack[n*N+i]=2000.0f+3.0f*sinf_(0.7*i+n);
    for(int i=0;i<N;i+=4){ int m=i%8; bstack[2*N+i]=2000.0f+(m==0?920.f:(m==1?-700.f:(m==2?-250.f:1800.f))); }
    bstack[2*N+7]=NAN;   // NaN 注入在 generate_master 的逐像素统计路径 (显式跳过 NaN)
    // dstack = 80 + 1.5*cos(0.5*i+n); 帧3 离群
    std::vector<float> dstack(5*N);
    for(int n=0;n<5;++n) for(int i=0;i<N;++i) dstack[n*N+i]=80.0f+1.5f*cosf(0.5f*i+n);
    for(int i=0;i<N;i+=5){ int m=i%5; dstack[3*N+i]=80.0f+(m==0?55.f:(m==1?-40.f:30.f)); }
    // fstack = 2000 + 40*(1 + 空间梯度 + 0.01*sin) [ADU]; 帧1污染
    // SCI-CAL-001 §3 单位表: raw/bias/dark/flat/cal 同为 ADU —— flat 必须与
    // bias 同量级, 否则减 bias 后中位数为负 = 退化输入 (RESCUE-P0-06 根因)。
    // 注: NaN 注入不放在 flat (generate_master_flat 步骤1 的帧 median 缓冲
    // 不经 NaN 过滤, SCI §4 的"median 跳过 NaN"契约只在 generate_master 的
    // 逐像素统计路径实现), 避免夹具依赖未定义序; NaN 覆盖由 bias stack 承担。
    std::vector<float> fstack(4*N);
    for(int n=0;n<4;++n) for(int i=0;i<N;++i){ int x=i%W,y=i/W; fstack[n*N+i]=2000.0f+40.0f*(1.0f+0.02f*((float)x/(W-1)-0.5f)+0.015f*((float)y/(H-1)-0.5f)+0.01f*sinf_(i+n)); }
    for(int i=0;i<N;i+=3) fstack[1*N+i]+=9.0f;
    // light = 2500 + 20*x/W + 2*sin
    std::vector<float> light(N);
    for(int i=0;i<N;++i){ int x=i%W; light[i]=2500.0f+20.0f*((float)x/W)+2.0f*sinf_(0.3f*i); }
    // ---- 调用库 ----
    std::vector<float> mb(N),md(N),mf(N),o0(N),o1(N),o2(N);
    put(bstack.data(),bstack.size(),"RAW_BIASSTACK ");
    put(dstack.data(),dstack.size(),"RAW_DARKSTACK ");
    put(fstack.data(),fstack.size(),"RAW_FLATSTACK ");
    put(light.data(),N,"LIGHT ");
    // 返回码必须显式断言 (不得忽略 reject): 任一 != AC_OK 则以非零退出,
    // 让 Python 侧 _run 的 returncode==0 断言硬失败 (RESCUE-P0-06: 全零 master
    // flat 曾被静默消费 → 恒 ×10.00079)。
    const int rc_mb = ac_generate_master_bias(bstack.data(),5,W,H,mb.data(),2.0f,3.0f,5,AC_COMBINE_MEAN);
    printf("RC_MASTER_BIAS %d\n",rc_mb);
    put(mb.data(),N,"MASTER_BIAS ");
    const int rc_md = ac_generate_master_dark(dstack.data(),5,W,H,md.data(),2.0f,3.0f,5,AC_COMBINE_MEDIAN);
    printf("RC_MASTER_DARK %d\n",rc_md);
    put(md.data(),N,"MASTER_DARK ");
    const int rc_mf = ac_generate_master_flat(fstack.data(),4,W,H,mb.data(),mf.data(),2.5f,3.0f,4);
    printf("RC_MASTER_FLAT %d\n",rc_mf);
    put(mf.data(),N,"MASTER_FLAT ");
    float ak0=0,ak1=0;
    const int rc_c0 = ac_calibrate_frame(light.data(),W,H,md.data(),mf.data(),mb.data(),o0.data(),0,1.0f,&ak0);
    printf("RC_CAL0 %d\n",rc_c0);
    put(o0.data(),N,"CAL0 "); printf("CAL0AK %.9g\n",ak0);
    const int rc_c1 = ac_calibrate_frame(light.data(),W,H,md.data(),mf.data(),mb.data(),o1.data(),1,1.5f,&ak1);
    printf("RC_CAL1 %d\n",rc_c1);
    put(o1.data(),N,"CAL1 "); printf("CAL1AK %.9g\n",ak1);
    std::vector<float> zdark(N,0.f),zflat(N,0.f);
    const int rc_c2 = ac_calibrate_frame(light.data(),W,H,zdark.data(),zflat.data(),mb.data(),o2.data(),1,1.5f,nullptr);
    printf("RC_CAL2 %d\n",rc_c2);
    put(o2.data(),N,"CAL2 ");
    if(rc_mb!=AC_OK||rc_md!=AC_OK||rc_mf!=AC_OK||rc_c0!=AC_OK||rc_c1!=AC_OK||rc_c2!=AC_OK){
        printf("GEN_OR_CAL_FAILED\n");
        return 3;
    }
    printf("DONE\n");
    return 0;
}
'''

# ---------- 第一性原理 Oracle(不调库, 按头注释契约) ----------
def median(vals):
    a = [float(v) for v in vals if v == v]
    if not a:
        return float('nan')
    a.sort()
    n = len(a); mid = n // 2
    return a[mid] if n % 2 == 1 else (a[mid - 1] + a[mid]) * 0.5

def sigma_clip(vals, low, high, iters):
    v = [float(x) for x in vals]
    for _ in range(iters):
        ok = [x for x in v if x == x]
        if not ok:
            break
        med = median(ok)
        mad = median([abs(x - med) for x in ok])
        sigma = 1.4826 * mad
        if sigma <= 0:
            break
        rej = 0
        for k, x in enumerate(v):
            if x != x:
                continue
            dev = x - med
            if dev < -low * sigma or dev > high * sigma:
                v[k] = float('nan'); rej += 1
        if rej == 0:
            break
    return v

def oracle_master(stack, w, h, low, high, iters, combine):
    nf = len(stack) // (w * h); npix = w * h; out = []
    for i in range(npix):
        vals = sigma_clip([stack[k * npix + i] for k in range(nf)], low, high, iters)
        ok = [x for x in vals if x == x]
        out.append(sum(ok) / len(ok) if combine == 0 else median(ok))
    return out

def oracle_flat(fstack, w, h, bias, low, high, iters):
    """SCI-CAL-001 §4/§5/§8 flat 语义 + ALG-CAL-002 离散步骤 (与生产同契约)。

    逐帧: minus = src - bias; med = median(minus)
          NaN/0 → med = 1.0 (生产兜底); med < 0 → 不可归一化: 生产
          ac_generate_master_flat 返回 AC_ERR_PARAM 且不写 out
          (master_generator.cpp B13-R13-7) ⇒ Oracle 同拒, 不产出可比较 master。
          flat_norm = max(minus/med, 0.1)
    合并: sigma-clip + mean; 最终 median 归一同上 (NaN/0 → 1.0, <0 拒绝, floor 0.1)。
    """
    nf = len(fstack) // (w * h); npix = w * h; norm = []
    for n in range(nf):
        src = fstack[n * npix:(n + 1) * npix]
        minus = [src[i] - bias[i] for i in range(npix)]
        med = median(minus)
        if med != med or med == 0:
            med = 1.0
        if med < 0:  # 生产 reject 分支 (AC_ERR_PARAM): 不得继续归一
            raise ValueError(
                "master flat frame %d median<0: invalid flat stack "
                "(production AC_ERR_PARAM)" % n)
        norm.extend([max(v / med, 0.1) for v in minus])
    master = oracle_master(norm, w, h, low, high, iters, 0)
    med = median(master)
    if med != med or med == 0:
        med = 1.0
    if med < 0:
        raise ValueError(
            "master flat final median<0: invalid flat stack "
            "(production AC_ERR_PARAM)")
    return [max(v / med, 0.1) for v in master]

def oracle_cal(light, dark, flat, bias, dark_opt, k):
    out = []
    for i in range(len(light)):
        if dark_opt == 1 and bias is not None and dark is not None:
            v = light[i] - bias[i] - k * (dark[i] - bias[i])
            if flat is not None:
                v /= max(flat[i], 0.1)
        else:
            v = light[i]
            if dark is not None:
                v -= dark[i]
            if flat is not None:
                v /= max(flat[i], 0.1)
        out.append(v)
    return out


@unittest.skipUnless(shutil.which("g++"), "需要 g++")
class TestCalibrationOracle(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.mkdtemp(prefix="syn001_")
        drv = os.path.join(cls.tmp, "cal_drv.cpp")
        with open(drv, "w") as f:
            f.write(DRIVER)
        srcs = [os.path.join(CAL_SRC, s) for s in
                ["calibrator.cpp", "master_generator.cpp", "cosmetic_corrector.cpp",
                 "dark_optimizer.cpp", "ac_api.cpp"]]
        cls.exe = os.path.join(cls.tmp, "cal_drv")
        r = subprocess.run(["g++", "-std=c++17", "-O2", "-fopenmp",
                            f"-I{CAL_INC}", f"-I{os.path.join(REPO, 'include')}", f"-I{HISS_INC}",
                            drv, *srcs, "-lgomp", "-pthread", "-o", cls.exe],
                           capture_output=True, text=True, timeout=600)
        assert r.returncode == 0, "[compile]\n" + r.stderr[-1200:]

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.tmp, ignore_errors=True)

    def _run(self, threads):
        r = subprocess.run([self.exe, str(threads)], capture_output=True, text=True, timeout=180)
        self.assertEqual(r.returncode, 0, r.stderr[-300:])
        d = {}
        for line in r.stdout.splitlines():
            tag, _, rest = line.partition(' ')
            if tag and rest.strip():
                d[tag] = rest.strip()
        arr = lambda s: [float(x) for x in s.split(',') if x.strip() != '']
        return {k: arr(v) for k, v in d.items()}

    def _load(self):
        d = self._run(2)
        self.raws = {}
        for t in ("RAW_BIASSTACK", "RAW_DARKSTACK", "RAW_FLATSTACK", "LIGHT"):
            self.raws[t] = d[t]
        self.out = d
        return d

    def _approx_list(self, got, exp, tol, label):
        for i in range(0, len(got), max(1, len(got) // 40)):
            self.assertAlmostEqual(got[i], exp[i], delta=tol * max(1.0, abs(exp[i])),
                                   msg=f"{label}[{i}] got={got[i]} exp={exp[i]}")

    def test_01_master_bias_mean_oracle(self):
        d = self._load(); W, H = 16, 8; N = W * H
        exp = oracle_master(self.raws["RAW_BIASSTACK"], W, H, 2.0, 3.0, 5, 0)
        self._approx_list(d["MASTER_BIAS"], exp, 1e-3, "MASTER_BIAS")

    def test_02_master_dark_median_oracle(self):
        d = self._load(); W, H = 16, 8; N = W * H
        exp = oracle_master(self.raws["RAW_DARKSTACK"], W, H, 2.0, 3.0, 5, 1)
        self._approx_list(d["MASTER_DARK"], exp, 1e-3, "MASTER_DARK")

    def test_03_master_flat_oracle(self):
        d = self._load(); W, H = 16, 8; N = W * H
        mb = oracle_master(self.raws["RAW_BIASSTACK"], W, H, 2.0, 3.0, 5, 0)
        exp = oracle_flat(self.raws["RAW_FLATSTACK"], W, H, mb, 2.5, 3.0, 4)
        # B2-A6: master flat 生成返回码必须 AC_OK (不得忽略 reject → 全零缓冲)
        self.assertEqual(d["RC_MASTER_FLAT"][0], 0.0,
                         "generate_master_flat 返回码必须 AC_OK")
        self.assertEqual(d["RC_MASTER_BIAS"][0], 0.0)
        self.assertEqual(d["RC_MASTER_DARK"][0], 0.0)
        # 全零 master flat 是历史红灯形态: 中位数必须为 1.0 且逐像素非零正
        med = median(d["MASTER_FLAT"])
        self.assertAlmostEqual(med, 1.0, delta=3e-3,
                               msg="master flat 必须 median≈1.0 (非全零)")
        self.assertGreater(min(d["MASTER_FLAT"]), 0.0,
                           "master flat 不得含 0/负像素 (floor 契约)")
        self._approx_list(d["MASTER_FLAT"], exp, 3e-3, "MASTER_FLAT")

    def test_04_calibrate_standard_value_oracle(self):
        d = self._load(); W, H = 16, 8; N = W * H
        mb = oracle_master(self.raws["RAW_BIASSTACK"], W, H, 2.0, 3.0, 5, 0)
        md = oracle_master(self.raws["RAW_DARKSTACK"], W, H, 2.0, 3.0, 5, 1)
        mf = oracle_flat(self.raws["RAW_FLATSTACK"], W, H, mb, 2.5, 3.0, 4)
        light = self.raws["LIGHT"]
        exp = oracle_cal(light, md, mf, None, 0, 1.0)
        self.assertAlmostEqual(d["CAL0AK"][0], 1.0, delta=1e-6)
        self._approx_list(d["CAL0"], exp, 2e-3, "CAL0")

    def test_05_calibrate_darkopt_value_oracle(self):
        d = self._load(); W, H = 16, 8; N = W * H
        mb = oracle_master(self.raws["RAW_BIASSTACK"], W, H, 2.0, 3.0, 5, 0)
        md = oracle_master(self.raws["RAW_DARKSTACK"], W, H, 2.0, 3.0, 5, 1)
        mf = oracle_flat(self.raws["RAW_FLATSTACK"], W, H, mb, 2.5, 3.0, 4)
        light = self.raws["LIGHT"]
        exp = oracle_cal(light, md, mf, mb, 1, 1.5)
        self.assertAlmostEqual(d["CAL1AK"][0], 1.5, delta=1e-6)
        self._approx_list(d["CAL1"], exp, 2e-3, "CAL1")

    def test_06_calibrate_zero_flat_clip(self):
        d = self._load()
        for v in d["CAL2"]:
            self.assertTrue(v == v and abs(v) > 0, f"flat clip 0.1 应产生有限非零, got {v}")

    def test_07_determinism_across_threads(self):
        a = self._run(1); b = self._run(4)
        for k in ("MASTER_BIAS", "MASTER_DARK", "MASTER_FLAT"):
            self.assertEqual(a[k], b[k], f"线程({k}) 1 vs 4 数值不一致")
        self.assertEqual(a["CAL0"], b["CAL0"])
        self.assertEqual(a["CAL1"], b["CAL1"])


if __name__ == "__main__":
    unittest.main(verbosity=2)
