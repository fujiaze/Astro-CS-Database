#!/usr/bin/env python3
"""从 FITS 定义独立构造坐标真值；不读取旧 oracle。运行后写 JSON 证据。

依赖 numpy/astropy；可选 --production-probe 编译未经修改的 IPV/Drizzle/photometry。
像元坐标均为整数中心 0 基；头 CRPIX 始终为 FITS 1 基。
"""
from __future__ import annotations
import argparse
import copy
import hashlib
import json
import math
import os
from pathlib import Path
import platform
import shutil
import subprocess
import tempfile

import astropy
from astropy.wcs import WCS, Sip
from astropy.wcs import _wcs
import numpy as np

ROOT = Path(__file__).resolve().parents[3]
DEFAULT = Path(__file__).resolve().parents[1] / "evidence/coordinates_independent.json"


def make_wcs(center=(359.9993, -12.3), order=4):
    w = WCS(naxis=2)
    w.wcs.crpix = [47.381, 39.127]  # 非整数且不居中，禁止依赖中心巧合
    w.wcs.crval = center
    w.wcs.cd = np.array([[-1.9e-4, 6.7e-5], [4.1e-5, 2.3e-4]])
    w.wcs.ctype = ["RA---TAN-SIP", "DEC--TAN-SIP"]
    a, b = np.zeros((order + 1, order + 1)), np.zeros((order + 1, order + 1))
    for i in range(order + 1):
        for j in range(order + 1 - i):
            k = i + j
            if k < 2:
                continue
            a[i, j] = (-1)**i * (i + 2*j + 1) * 2e-5 / 70**(k - 2)
            b[i, j] = (-1)**j * (3*i - j + 2) * 1.3e-5 / 70**(k - 2)
    w.sip = Sip(a, b, None, None, w.wcs.crpix)
    return w


def poly(c, p):
    """独立的显式单项式求值，不调用 Astropy SIP。"""
    u, v = p.T
    return sum(c[i, j] * u**i * v**j
               for i in range(c.shape[0]) for j in range(c.shape[1] - i))


def sky_vectors(lonlat):
    lon, lat = np.deg2rad(np.asarray(lonlat)).T
    return np.column_stack([np.cos(lat)*np.cos(lon), np.cos(lat)*np.sin(lon), np.sin(lat)])


def angular_error(a, b):
    """atan2 叉积/内积在小角及极点保持稳定，单位 arcsec。"""
    aa, bb = sky_vectors(a), sky_vectors(b)
    return np.rad2deg(np.arctan2(np.linalg.norm(np.cross(aa, bb), axis=1),
                                np.sum(aa*bb, axis=1))) * 3600


def forward_independent(w, p):
    """向量形式的 TAN 逆：归一化 n0 + xi*east + eta*north。"""
    uv = np.asarray(p) + 1 - w.wcs.crpix
    if w.sip is not None:
        uv = uv + np.column_stack([poly(w.sip.a, uv), poly(w.sip.b, uv)])
    plane = np.deg2rad(uv @ w.wcs.cd.T)
    lon, lat = np.deg2rad(w.wcs.crval)
    n0 = np.array([np.cos(lat)*np.cos(lon), np.cos(lat)*np.sin(lon), np.sin(lat)])
    east = np.array([-np.sin(lon), np.cos(lon), 0])
    north = np.array([-np.sin(lat)*np.cos(lon), -np.sin(lat)*np.sin(lon), np.cos(lat)])
    vec = n0 + plane[:, :1]*east + plane[:, 1:]*north
    vec /= np.linalg.norm(vec, axis=1)[:, None]
    return np.column_stack([np.rad2deg(np.arctan2(vec[:, 1], vec[:, 0])) % 360,
                            np.rad2deg(np.arctan2(vec[:, 2], np.hypot(vec[:, 0], vec[:, 1])))])


def affine_wcs(w, matrix, offset):
    """旧坐标 p=M q+t。限定轴置换/翻转/对角分箱，精确变换 SIP。

    c_old=CRPIX_old−1；c_new=M^-1(c_old−t)；CD_new=CD_old M；
    D_new(z)=M^-1 D_old(Mz)。M 每行只有一个非零项。
    """
    m, t = np.asarray(matrix, float), np.asarray(offset, float)
    inv = np.linalg.inv(m)
    out = copy.deepcopy(w)
    out.wcs.crpix = inv @ (w.wcs.crpix - 1 - t) + 1
    out.wcs.cd = w.wcs.cd @ m
    axis = np.argmax(np.abs(m), axis=1)
    assert len(set(axis)) == 2 and np.count_nonzero(m) == 2
    n = w.sip.a.shape[0]
    new = np.zeros((2, n, n))
    for i in range(n):
        for j in range(n - i):
            powers = np.zeros(2, dtype=int)
            powers[axis[0]] += i
            powers[axis[1]] += j
            coef = inv @ np.array([w.sip.a[i,j], w.sip.b[i,j]])
            new[:, powers[0], powers[1]] += coef*m[0,axis[0]]**i*m[1,axis[1]]**j
    out.sip = Sip(new[0], new[1], None, None, out.wcs.crpix)
    return out


def summarize(x):
    x = np.asarray(x, float)
    return {"min": float(np.min(x)), "median": float(np.median(x)), "max": float(np.max(x))}


def production_probe(w, p, world):
    """编译原始生产翻译单元；不复制其关键算法、不改其内容。"""
    compiler = shutil.which("g++")
    if compiler is None and Path("C:/msys64/mingw64/bin/g++.exe").exists():
        compiler = "C:/msys64/mingw64/bin/g++.exe"
    if compiler is None:
        return {"status": "UNAVAILABLE", "reason": "g++ absent"}
    ipv = ROOT / "lib/algorithms/platesolve/cpp/ipv"
    drizzle = ROOT / "lib/algorithms/drizzle/healpix_drizzle"
    photo = ROOT / "lib/algorithms/photometry/cpp/src"
    code = r'''
#include "ipv_wcs.h"
#include "wcs_sip.h"
#include "wcs_transform.h"
#include <iostream>
#include <iomanip>
int main() {
  ipv::WcsFitResult v{}; drizzle::WcsParams d{}; d.has_wcs=true;
  std::cin>>v.crval[0]>>v.crval[1]>>v.crpix[0]>>v.crpix[1];
  std::cin>>v.cd.cd11>>v.cd.cd12>>v.cd.cd21>>v.cd.cd22>>v.sip.order;
  d.crval[0]=v.crval[0];d.crval[1]=v.crval[1];d.crpix[0]=v.crpix[0];d.crpix[1]=v.crpix[1];
  d.cd[0]=v.cd.cd11;d.cd[1]=v.cd.cd12;d.cd[2]=v.cd.cd21;d.cd[3]=v.cd.cd22;d.sip.order=v.sip.order;
  for(int k=0;k<36;++k){std::cin>>v.sip.A[k]>>v.sip.B[k];d.sip.a[k]=v.sip.A[k];d.sip.b[k]=v.sip.B[k];}
  drizzle::WcsSip dr(d);
  pc::WcsTransform ph(v.crval[0],v.crval[1],v.crpix[0],v.crpix[1],v.cd.cd11,v.cd.cd12,v.cd.cd21,v.cd.cd22,v.sip.order,v.sip.A,v.sip.B,nullptr,nullptr);
  double x,y,ra,dec;std::cout<<std::setprecision(17);
  while(std::cin>>x>>y>>ra>>dec){
    auto a=ipv::wcs_sky_to_pixel_iterative(v,ra,dec);double ra1,dec1,x2,y2;
    dr.pixelToSky(x,y,ra1,dec1);ph.skyToPixel(ra,dec,x2,y2);
    std::cout<<a.converged<<" "<<a.x<<" "<<a.y<<" "<<ra1<<" "<<dec1<<" "<<x2<<" "<<y2<<"\n";
  }
}'''
    # 平移单位/接口编译失败是证据状态，绝不伪装成执行通过。
    with tempfile.TemporaryDirectory(prefix="acsd_s0_") as temp:
        source, exe = Path(temp)/"probe.cpp", Path(temp)/"probe.exe"
        source.write_text(code, encoding="utf-8")
        includes = [ipv/"include", ipv/"src", drizzle, photo, ROOT/"lib/infrastructure/aio/src"]
        cmd = [compiler, "-std=c++17", "-O2", "-flto", "-ffunction-sections", "-fdata-sections", str(source),
               str(ipv/"src/ipv_wcs.cpp"), str(drizzle/"wcs_sip.cpp"), str(photo/"wcs_transform.cpp"),
               "-Wl,--gc-sections", "-o", str(exe)] + ["-I"+str(i) for i in includes]
        env = dict(os.environ)
        env["PATH"] = str(Path(compiler).parent)+os.pathsep+env.get("PATH", "")
        built = subprocess.run(cmd, capture_output=True, text=True, encoding="utf-8", errors="replace", env=env)
        if built.returncode:
            return {"status":"COMPILE_FAILED", "stderr":built.stderr, "command":cmd}
        fields = [*w.wcs.crval, *w.wcs.crpix, *w.wcs.cd.ravel(), w.sip.a.shape[0]-1]
        for i in range(6):
            for j in range(6):
                fields += [w.sip.a[i,j] if i<w.sip.a.shape[0] and j<w.sip.a.shape[1] else 0,
                           w.sip.b[i,j] if i<w.sip.b.shape[0] and j<w.sip.b.shape[1] else 0]
        data = " ".join(map(str,fields))+"\n"+"\n".join(" ".join(map(str,[*pp,*ww])) for pp,ww in zip(p,world))
        run = subprocess.run([str(exe)], input=data, capture_output=True, text=True, encoding="utf-8", errors="replace", env=env, check=True)
        values = np.loadtxt(run.stdout.splitlines())
        antipode = [(w.wcs.crval[0]+180)%360, -w.wcs.crval[1]]
        back_data = " ".join(map(str,fields))+"\n0 0 "+" ".join(map(str,antipode))+"\n"
        back_run = subprocess.run([str(exe)], input=back_data, capture_output=True, text=True, encoding="utf-8", errors="replace", env=env, check=True)
        back = np.loadtxt(back_run.stdout.splitlines())
        return {"status":"EXECUTED", "compiler":subprocess.check_output([compiler,"--version"],text=True,env=env).splitlines()[0],
                "n":len(p), "ipv_converged":int(values[:,0].sum()),
                "ipv_minus_astropy_origin0_pixel": {"x":summarize(values[:,1]-p[:,0]),"y":summarize(values[:,2]-p[:,1])},
                "drizzle_forward_error_arcsec": summarize(angular_error(values[:,3:5],world)),
                "photometry_inverse_error_pixel":summarize(np.linalg.norm(values[:,5:7]-p,axis=1)),
                "antipodal_negative_case":{"world_deg":antipode,"expected":"TAN q<0: outside chosen tangent hemisphere; reject", "ipv_converged":bool(back[0]),"ipv_xy":back[1:3].tolist(),"photometry_xy":[float(v) if np.isfinite(v) else None for v in back[5:7]]},
                "source_sha256":{f.relative_to(ROOT).as_posix():hashlib.sha256(f.read_bytes()).hexdigest() for f in [ipv/"src/ipv_wcs.cpp", drizzle/"wcs_sip.cpp", photo/"wcs_transform.cpp"]}}


def main():
    parser=argparse.ArgumentParser()
    parser.add_argument("--output",type=Path,default=DEFAULT)
    parser.add_argument("--production-probe",action="store_true")
    args=parser.parse_args()
    rng=np.random.default_rng(11120261020)
    p=np.vstack([rng.uniform([-.5,-.5],[95.5,79.5],(1000,2)),[-.5,-.5],[95.5,79.5],[46.381,38.127]])
    result={"identity":{"seed":11120261020,"python":platform.python_version(),"numpy":np.__version__,"astropy":astropy.__version__,"wcslib":_wcs.__version__,"n_per_case":len(p),"script_sha256":hashlib.sha256(Path(__file__).read_bytes()).hexdigest()},
            "meaning":"INDEPENDENTLY_TESTED 是本脚本实际运行；静态结论与真实数据假设另列，非正式验收。",
            "forward":[],"affine":{},"counterexamples":{}}
    for center in [(359.9993,-12.3),(0.0002,89.95),(123.4,-89.94)]:
        for order in [2,3,4]:
            w=make_wcs(center,order)
            oracle=w.all_pix2world(p,0)
            error=angular_error(forward_independent(w,p),oracle)
            inverse=w.all_world2pix(oracle,0,tolerance=1e-12,maxiter=100,adaptive=False,detect_divergence=True)
            result["forward"].append({"center_deg":list(center),"order":order,"analytic_vs_astropy_arcsec":summarize(error),"astropy_inverse_error_pixel":summarize(np.linalg.norm(inverse-p,axis=1))})
            assert np.max(error)<1e-6
            assert np.max(np.linalg.norm(inverse-p,axis=1))<1e-6
    w=make_wcs()
    result["synthetic_header"]=dict(w.to_header(relax=True))
    # 三角不等式给连续整个矩形的导数范数上界；不是抽样的假精度。
    radius=np.max(np.abs(np.array([[-.5,-.5],[95.5,79.5]])+1-w.wcs.crpix),axis=0)
    bound=np.zeros((2,2))
    for row,c in enumerate([w.sip.a,w.sip.b]):
        for i in range(c.shape[0]):
            for j in range(c.shape[1]-i):
                if i: bound[row,0]+=abs(c[i,j])*i*radius[0]**(i-1)*radius[1]**j
                if j: bound[row,1]+=abs(c[i,j])*j*radius[0]**i*radius[1]**(j-1)
    lipschitz=float(np.max(np.sum(bound,axis=1)))
    assert lipschitz<1
    result["sip_injectivity_certificate"]={"classification":"DERIVED_BOUND_FOR_SYNTHETIC_DOMAIN", "domain_pixel":[[-.5,-.5],[95.5,79.5]],"uv_absolute_bounds":radius.tolist(),"derivative_absolute_matrix_bounds":bound.tolist(),"D_lipschitz_infinity_upper_bound":lipschitz,"inverse_lipschitz_upper_bound":1/(1-lipschitz),"statement":"L<1 and domain convex imply ||(I+D)(u)-(I+D)(v)||inf >= (1-L)||u-v||inf. Coefficient bound is ordinary float64 with >0.8 margin; not a machine outward-rounded interval proof."}
    matrices={"crop":(np.eye(2),[13,7]),"bin_2x3":(np.diag([2,3]),[.5,1]),
              "flip_y":(np.diag([1,-1]),[0,79]),"flip_x":(np.diag([-1,1]),[95,0]),
              "transpose":(np.array([[0,1],[1,0]]),[0,0]),
              "transpose_flip_bin":(np.array([[0,-3],[2,0]]),[95,.5])}
    q=rng.uniform([0,0],[20,20],(500,2))
    for name,(m,t) in matrices.items():
        transformed=affine_wcs(w,m,t)
        old=q@np.asarray(m).T+np.asarray(t)
        e=angular_error(w.all_pix2world(old,0),transformed.all_pix2world(q,0))
        result["affine"][name]={"old_from_new_matrix":np.asarray(m).tolist(),"offset":list(t),"new_crpix":transformed.wcs.crpix.tolist(),"error_arcsec":summarize(e)}
        assert np.max(e)<1e-6
    truth=w.all_pix2world(p,0)
    for name,shift in [("omit_fits_plus1",-1),("double_fits_plus1",1),("detector_half_pixel_unadapted",.5)]:
        result["counterexamples"][name]={"error_arcsec":summarize(angular_error(w.all_pix2world(p+shift,0),truth)),"pixel_offset_each_axis":shift}
    # 同一个错误原点的正逆互消，往返测试必过，绝对坐标必错。
    mistaken=w.all_pix2world(p,1)
    closed=w.all_world2pix(mistaken,1,tolerance=1e-12,maxiter=100)
    result["counterexamples"]["wrong_origin_roundtrip"]={"roundtrip_error_pixel":summarize(np.linalg.norm(closed-p,axis=1)),"absolute_error_arcsec":summarize(angular_error(mistaken,truth))}
    # 将 index+0.5 的 solver 域转回整数中心后，现有居中 CRPIX 恰好正确。
    det=p+.5
    solver_u=det-np.array([96,80])/2
    fits_u=p+1-(np.array([96,80])+1)/2
    result["solver_origin_identity"]={"max_abs_u_difference_pixel":float(np.max(np.abs(solver_u-fits_u))),"classification":"DERIVED_AND_INDEPENDENTLY_TESTED"}
    if args.production_probe:
        result["production_probe"]=production_probe(w,p,truth)
        result["centroid_probe"]=centroid_probe()
    args.output.parent.mkdir(parents=True,exist_ok=True)
    args.output.write_text(json.dumps(result,ensure_ascii=False,indent=2,allow_nan=False)+"\n",encoding="utf-8")
    print(json.dumps({"output":str(args.output),"max_forward_arcsec":max(i["analytic_vs_astropy_arcsec"]["max"] for i in result["forward"]),"production":result.get("production_probe",{}).get("status")},ensure_ascii=False))


def centroid_probe():
    """整数中心合成图像，直接执行生产引导检测和 Moffat PSF API。

    仅将两种日志函数接至空 sink，科学代码、平滑及拟合均未经修改。
    先验位置是否加半像元分别试验，不把收敛吸收先验误差误当正确原点。
    """
    compiler=shutil.which("g++") or "C:/msys64/mingw64/bin/g++.exe"
    if not Path(compiler).exists() and shutil.which(compiler) is None:
        return {"status":"UNAVAILABLE"}
    sd=ROOT/"lib/algorithms/star_detection"
    psf=ROOT/"lib/algorithms/psf"
    code=r'''
#include "star_detector.h"
#include "dynamic_psf.h"
#include <vector>
#include <cmath>
#include <iostream>
#include <iomanip>
void sdet_log(int,const char*,const char*,...){}
void dpsf_log(int,const char*,const char*,...){}
int main(){
 const int W=64,H=64;const double cx=31.237,cy=30.671;
 std::vector<double> gauss(W*H),moffat(W*H);
 for(int y=0;y<H;++y)for(int x=0;x<W;++x){
   double dx=x-cx,dy=y-cy,c=std::cos(.23),s=std::sin(.23);
   double u=(c*dx+s*dy)/3.2,v=(-s*dx+c*dy)/2.6,q=(u*u+v*v)/2;
   gauss[y*W+x]=100+1000*std::exp(-q);moffat[y*W+x]=100+1000/std::pow(1+q,4);
 }
 auto h=sdet_create(nullptr);std::cout<<std::setprecision(17);
 for(double shift:{0.,.5}){
   double px=cx+shift,py=cy+shift,*x=nullptr,*y=nullptr;float *f=nullptr,*mag=nullptr;
   int *sat=nullptr,*hs=nullptr,n=0;SDetGuidedStats st{};
   int rc=sdet_detect_guided_ex_f64(h,gauss.data(),W,H,&px,&py,1,&x,&y,&f,&sat,&mag,&hs,&n,nullptr,0,nullptr,&st);
   std::cout<<"gaussian "<<shift<<" "<<rc<<" "<<n<<" "<<(n?x[0]:NAN)<<" "<<(n?y[0]:NAN)<<"\n";
   sdet_free_detect_ex(x,y,f,sat,mag,hs,nullptr,0);
 }
 sdet_destroy(h);
 double det[6]={cx+.5,cy+.5,1000,-7.5,0,0},out[9];int nv=0,st=0;
 int rc=dpsf_fit_batch_f64(moffat.data(),W,H,det,1,nullptr,out,&nv,&st);
 std::cout<<"moffat 0.5 "<<rc<<" "<<nv<<" "<<(nv?out[2]:NAN)<<" "<<(nv?out[3]:NAN)<<"\n";
}'''
    sources=[sd/"src/sdet_api.cpp",sd/"src/sdet_image.cpp",sd/"src/nls_lm.cpp",psf/"src/dpsf_psf.cpp",psf/"src/dpsf_image.cpp"]
    with tempfile.TemporaryDirectory(prefix="acsd_s0_centroid_") as tmp:
        src,exe=Path(tmp)/"centroid.cpp",Path(tmp)/"centroid.exe"
        src.write_text(code,encoding="utf8")
        cmd=[compiler,"-std=c++17","-O2","-flto","-fopenmp",str(src),*[str(f) for f in sources],"-I"+str(sd/"include"),"-I"+str(psf/"include"),"-o",str(exe)]
        env=dict(os.environ);env["PATH"]=str(Path(compiler).parent)+os.pathsep+env.get("PATH","");env["OMP_NUM_THREADS"]="1"
        built=subprocess.run(cmd,capture_output=True,text=True,encoding="utf8",errors="replace",env=env)
        if built.returncode:return {"status":"COMPILE_FAILED","stderr":built.stderr}
        run=subprocess.run([str(exe)],capture_output=True,text=True,encoding="utf8",errors="replace",env=env,check=True)
        rows=[]
        for line in run.stdout.splitlines():
            kind,shift,rc,n,x,y=line.split();x,y=float(x),float(y)
            rows.append({"model":kind,"prior_offset_pixel":float(shift),"rc":int(rc),"n_output":int(n),"reported_center_pixel":[x if math.isfinite(x) else None,y if math.isfinite(y) else None],"offset_from_integer_center_truth_pixel":[x-31.237 if math.isfinite(x) else None,y-30.671 if math.isfinite(y) else None]})
        return {"status":"EXECUTED","truth_integer_center_pixel":[31.237,30.671],"input":"64x64, background=100, amplitude=1000, axes=(3.2,2.6), theta=0.23 rad, point sampled; Gaussian and beta=4 Moffat separately generated", "logging":"sdet_log and dpsf_log are no-op; all scientific translation units unmodified","rows":rows,"source_sha256":{f.relative_to(ROOT).as_posix():hashlib.sha256(f.read_bytes()).hexdigest() for f in sources}}


if __name__=="__main__":
    main()
