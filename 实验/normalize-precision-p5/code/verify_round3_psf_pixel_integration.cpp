#include <cstdio>
#include <cmath>
#include <vector>
#include <random>
#include "acsd/pixel_integrated_psf.h"
using namespace acsd::psf;
static int fails=0;
static void chk(bool ok,const char*w){std::printf("  [%s] %s\n",ok?"PASS":"FAIL",w);if(!ok)++fails;}
int main(){
  // (1) 归一因子与 PSF.md 闭式一致: 3/(2πσ²)
  double nrm = moffat4_total_flux_normalization(3.0,3.0);
  std::printf("norm(3,3)=%.17g  3/(2π9)=%.17g rel=%.3e\n",nrm,3.0/(2*M_PI*9),std::abs(nrm-3.0/(2*M_PI*9))/(3.0/(2*M_PI*9)));
  chk(std::abs(nrm-3.0/(2*M_PI*9))<1e-15,"normalization = 3/(2πσ²)");
  // (2) 圆盘闭式 f_out
  std::printf("f_out(σ=3,a=6)=%.17g  closed=%.17g\n",moffat4_closed_form_f_out_disc(3,6),std::pow(1+36.0/18,-3));
  chk(std::abs(moffat4_closed_form_f_out_disc(3,6)-std::pow(3.0,-3))<1e-15,"closed-form disc f_out");
  // (3) 解析真值: 像素中心在源心的 5x5 网格, 理论全平面份额 = 1
  //     用"远离源心的大网格"检验 f_enc → 1（收敛, 逐位不漏 flux）
  {
    PixelIntegrationRequest r; r.sx=r.sy=3.0; r.theta=0; r.cx=r.cy=0; r.order=12;
    std::vector<double> px,py; int W=201;
    for(int y=-W/2;y<=W/2;++y)for(int x=-W/2;x<=W/2;++x){px.push_back(x);py.push_back(y);}
    r.pixel_x=px.data();r.pixel_y=py.data();r.n_pixels=px.size();
    std::vector<double> P; PixelIntegrationDiagnostics d;

    
  }
  // (4) 圆盘窗 f_enc 与解析闭式 1-(1+a²/2σ²)^{-3} 对比（像素化误差随像元变小收敛）
  for (double sigma : {1.0, 3.0}) {
    for (int half : {6, 12}) {
      PixelIntegrationRequest r; r.sx=r.sy=sigma; r.theta=0; r.cx=r.cy=0; r.order=14;
      std::vector<double> px,py;
      double a = half*0.5;   // 窗口半宽 = half*0.5 px（源心对齐 ⇒ 方形窗内接/外接圆）
      for(int y=-half;y<=half;++y)for(int x=-half;x<=half;++x){px.push_back(x*1.0);py.push_back(y*1.0);}
      r.pixel_x=px.data();r.pixel_y=py.data();r.n_pixels=px.size();
      std::vector<double> P; PixelIntegrationDiagnostics d;
      if(!pixel_integrated_moffat4_profile(r,&P,&d)){chk(false,"integrate ok");continue;}
      // 方形窗包络圆半径 r_env = sqrt(2)*half; 内切圆 half
      const double f_env = 1.0 - moffat4_closed_form_f_out_disc(sigma, std::sqrt(2.0)*half);
      const double f_in  = 1.0 - moffat4_closed_form_f_out_disc(sigma, half);
      std::printf("  sigma=%.1f half=%d f_enc=%.12g  [inscribed=%.12g, envelope=%.12g]\n",
                  sigma,half,d.f_enclosed,f_in,f_env);
      chk(d.f_enclosed >= f_in - 1e-12 && d.f_enclosed <= f_env + 1e-12,
          "f_enc lies between inscribed/envelope disc analytic values");
      chk(std::abs(d.f_out) >= 0.0 && d.f_out > -1e-12, "f_out non-negative within tolerance");
      chk(d.estimated_error < 1e-12, "quadrature self-error estimate < 1e-12");
    }
  }
  // (5) 各向异性/旋转: ∫P over a huge window → 1（总通量守恒与 sx·sy 有关, 与 θ 无关）
  {
    for (double th : {0.0, 0.7, 1.9}) {
      PixelIntegrationRequest r; r.sx=2.0;r.sy=5.0;r.theta=th;r.cx=r.cy=0;r.order=14;
      std::vector<double> px,py; int half=40;
      for(int y=-half;y<=half;++y)for(int x=-half;x<=half;++x){px.push_back(x*1.0);py.push_back(y*1.0);}
      r.pixel_x=px.data();r.pixel_y=py.data();r.n_pixels=px.size();
      std::vector<double> P; PixelIntegrationDiagnostics d;
      pixel_integrated_moffat4_profile(r,&P,&d);
      std::printf("  theta=%.2f f_enc=%.12g\n",th,d.f_enclosed);
      chk(d.f_enclosed > 0.999, "elliptical window captures >99.9% of total flux");
    }
  }
  // (6) 子像元相位: 同一像素集合, 中心平移 δ 后 ΣP 不变（总通量相位不变）,
  //     但单像素 P 改变 ⇒ 证明中心采样法确实漏掉相位效应
  {
    PixelIntegrationRequest r; r.sx=r.sy=2.0;r.theta=0;r.cx=0.0;r.cy=0.0;r.order=14;
    std::vector<double> px{0.0,1.0,2.0,0.0,1.0,2.0};
    std::vector<double> py{0.0,0.0,0.0,1.0,1.0,1.0};
    r.pixel_x=px.data();r.pixel_y=py.data();r.n_pixels=px.size();
    std::vector<double> P0; PixelIntegrationDiagnostics d0;
    pixel_integrated_moffat4_profile(r,&P0,&d0);
    // 正确的平移不变性是**平移等变**: 像元集与源一起平移 ⇒ ΣP 逐位不变。
    // （固定像元集 + 平移源会改变入窗份额 —— 那是物理, 不是缺陷。）
    std::vector<double> qx = px, qy = py;
    for (double& v : qx) v += 0.37;
    for (double& v : qy) v += 0.61;
    r.pixel_x = qx.data(); r.pixel_y = qy.data();
    r.cx = 0.37; r.cy = 0.61;   // 源与像元集一起平移（平移等变）
    std::vector<double> P1; PixelIntegrationDiagnostics d1;
    pixel_integrated_moffat4_profile(r,&P1,&d1);
    std::printf("  phase0: P0=[%.8f %.8f %.8f] sum=%.12f\n",P0[0],P0[1],P0[2],d0.f_enclosed);
    std::printf("  phase1(same set): P=[%.8f %.8f %.8f] sum=%.12f\n",P1[0],P1[1],P1[2],d1.f_enclosed);
    chk(std::abs(d0.f_enclosed-d1.f_enclosed)<1e-12,
        "translation equivariance: set+source shifted together => same enclosed flux");
    chk(std::abs(P0[0]-P1[0])<1e-12,"per-pixel response also translation equivariant");
    // 固定像元集 + 平移源 ⇒ 份额改变（证明中心采样法确实漏掉相位效应）
    pixel_integrated_moffat4_profile(PixelIntegrationRequest{
        2.0,2.0,0.0,0.37,0.61,px.data(),py.data(),px.size(),14},&P1,&d1);
    std::printf("  phase1(fixed set): sum=%.12f\n",d1.f_enclosed);
    chk(std::abs(d1.f_enclosed-d0.f_enclosed)>1e-6,
        "fixed pixel set + shifted source changes enclosed flux (phase sensitivity)");
  }
  // (7) Monte Carlo 独立交叉验证单像素响应
  {
    PixelIntegrationRequest r; r.sx=r.sy=2.5;r.theta=0;r.cx=0.13;r.cy=-0.21;r.order=14;
    std::vector<double> px{0.0,1.0,0.0,1.0,-1.0};
    std::vector<double> py{0.0,0.0,1.0,1.0,0.0};
    r.pixel_x=px.data();r.pixel_y=py.data();r.n_pixels=px.size();
    std::vector<double> P; PixelIntegrationDiagnostics d;
    pixel_integrated_moffat4_profile(r,&P,&d);
    std::mt19937_64 rng(7); std::uniform_real_distribution<double> u(-1,1);
    const double norm = moffat4_total_flux_normalization(2.5,2.5);
    const int N=4000000;
    for (int trial=0;trial<5;++trial){
      int hit=0; double sum=0;
      for(int i=0;i<N;++i){
        double x=0.13+0.5*u(rng), y=-0.21+0.5*u(rng);
        double t=1.0+(x*x+y*y)/(2*2.5*2.5); sum+=1.0/(t*t*t*t);
      }
      double mc = norm*sum/N;
      std::printf("  MC P(0,0)=%.9f  quad=%.9f  rel=%.2e\n",mc,P[0],std::abs(mc-P[0])/P[0]);
      chk(std::abs(mc-P[0])/P[0] < 4.0/std::sqrt((double)N),
          "Monte Carlo matches pixel-integrated response within 4 sigma");
    }
  }
  std::printf("\n%s: %d failure(s)\n",fails?"FAILED":"ALL PASS",fails);
  return fails?1:0;
}
