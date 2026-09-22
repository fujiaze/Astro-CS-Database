// p5_extent.cpp — V0 极点破门区间的半径依赖（极坐标环扫描）
#include "polar_common.h"
#include "variants.h"
#include "spherical_overlap.h"
#include "healpix_core.h"
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <algorithm>
using namespace pp;
static double rel(double a, double b){ return b>0? std::fabs(a-b)/b : 0.0; }
static void ipix_to_fij(uint64_t ip, uint32_t Ns, int& f, uint32_t& i, uint32_t& j){
    uint64_t per=(uint64_t)Ns*Ns; f=(int)(ip/per); uint64_t r=ip%per; uint32_t x=0,y=0;
    for(int b=0;b<32;++b){x|=(uint32_t)((r>>(2*b))&1ull)<<b;y|=(uint32_t)((r>>(2*b+1))&1ull)<<b;} i=x;j=y;
}
int main(int argc,char**argv){
    uint32_t Ns=2097152u; double scale=0.2;
    const char* csv = (argc>1)? argv[1] : nullptr;
    healpix::HealpixCore hp((int)Ns);
    double hp_res=hp.pixelResolutionArcsec()*kArcsec2Rad;
    TanWcs w=make_wcs(0.0,90.0,scale,23.5);
    FILE* fp = csv? std::fopen(csv,"w"):nullptr;
    if(fp) std::fprintf(fp,"r_px,az_deg,ra_deg,dec_deg,v0,rec1,oracle,seg_per_leaf\n");
    printf("## T17 极点破门半径依赖 (nside=%u, %.2f\"/px)\n",Ns,scale);
    printf("  %-8s %-8s %-13s %-13s %-13s %-13s %-10s\n","r(px)","r(\")","V0最坏","V0中位","REC-1最坏","oracle最坏","段/叶");
    for (double rp : {0.0,0.25,0.5,1.0,2.0,4.0,8.0,12.0,16.0,20.0,24.0,32.0,64.0,128.0,256.0,512.0,1024.0}) {
        double w0=0,w1=0,wo=0; std::vector<double> v0s; long long seg=0,nl=0;
        int naz = (rp==0.0)?1:16;
        for(int k=0;k<naz;++k){
            double az = 2*kPi*k/naz;
            double dx = rp*std::cos(az), dy = rp*std::sin(az);
            std::vector<V3> d; if(!build_drop(w,dx,dy,1.0,1,d)) continue;
            DropGeom g; build_drop_geom(d,g);
            double ad=vos_area_rotated(d);
            std::vector<spherical::Vec3> dd; for(auto&p:d) dd.push_back({p.x,p.y,p.z});
            std::vector<uint64_t> c; spherical::query_candidate_pixels<double>(dd,hp,c);
            double s0=0,s1=0,so=0;
            for(uint64_t ip:c){ int f;uint32_t i,j; ipix_to_fij(ip,Ns,f,i,j);
                s0+=overlap_variant(g,Ns,f,i,j,0,1,hp_res,true);
                AdaptiveStat st; s1+=overlap_adaptive(g,Ns,f,i,j,1e-6*ad,18,&st); seg+=st.n_seg; ++nl;
                so+=oracle_overlap(f,Ns,i,j,d,96,1); }
            double r0=rel(s0,ad), r1=rel(s1,ad), ro=rel(so,ad);
            w0=std::max(w0,r0); w1=std::max(w1,r1); wo=std::max(wo,ro); v0s.push_back(r0);
            double ra,dec; V3 cen{0,0,0}; for(auto&p:d){cen.x+=p.x;cen.y+=p.y;cen.z+=p.z;} vec_to_radec(normalize3(cen),ra,dec);
            if(fp) std::fprintf(fp,"%.4f,%.1f,%.8f,%.8f,%.6e,%.6e,%.6e,%.1f\n",rp,az*kRad2Deg,ra,dec,r0,r1,ro,(double)seg/(double)std::max(1LL,nl));
        }
        std::sort(v0s.begin(),v0s.end());
        double med = v0s.empty()?0.0:v0s[v0s.size()/2];
        printf("  %-8.2f %-8.3f %-13.3e %-13.3e %-13.3e %-13.3e %-10.1f\n", rp, rp*scale, w0, med, w1, wo,
               (double)seg/(double)std::max(1LL,nl));
    }
    if(fp) std::fclose(fp);
    return 0;
}