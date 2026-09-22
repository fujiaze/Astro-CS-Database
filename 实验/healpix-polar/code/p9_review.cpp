// p9_review.cpp — 对独立复核 M1/M2 的自验证
//  T23 极点栅格: use_fast ∈ {0,1} × {A0,A1,A2,A3} + 快路径命中计数
//  T24 u+v=1 接缝中点扫描: use_fast ∈ {0,1}
//  T25 xyf2ang_replica 在 u+v=1 两侧的连续性
#include "polar_common.h"
#include "variants.h"
#include "spherical_overlap.h"
#include "healpix_core.h"
#include <cstdio>
#include <cstring>
#include <algorithm>
using namespace pp;
using namespace spherical;

static uint32_t NS = 2097152u;
static double HPRES = 0.0;

static void ipix_to_fij(uint64_t ipix, uint32_t Ns, int& face, uint32_t& i, uint32_t& j) {
    uint64_t per = (uint64_t)Ns * (uint64_t)Ns; face = (int)(ipix / per); uint64_t rem = ipix % per;
    uint32_t xv=0,yv=0; for (int b=0;b<32;++b){xv|=(uint32_t)((rem>>(2*b))&1ull)<<b;yv|=(uint32_t)((rem>>(2*b+1))&1ull)<<b;}
    i=xv;j=yv;
}

struct Row { int f0=0,f1=0,f2=0,f3=0; double w0=0,w1=0,w2=0,w3=0; long long nf=0,nq=0,nsh=0; int n=0; };

static Row scan(double ra0,double dec0,double lo,double hi,double stp,bool fast){
    Row R; TanWcs w=make_wcs(ra0,dec0,0.2,23.5);
    healpix::HealpixCore hp((int)NS);
    for(double x=lo;x<=hi+1e-9;x+=stp) for(double y=lo;y<=hi+1e-9;y+=stp){
        std::vector<V3> d; if(!build_drop(w,x,y,1.0,1,d)) continue;
        DropGeom g; build_drop_geom(d,g);
        double ad=vos_area_rotated(d);
        std::vector<spherical::Vec3> dd; for(auto&p:d) dd.push_back({p.x,p.y,p.z});
        std::vector<uint64_t> c; spherical::query_candidate_pixels<double>(dd,hp,c);
        double s[4]={0,0,0,0}; VariantStat vs;
        for(uint64_t ip:c){ int f;uint32_t i,j; ipix_to_fij(ip,NS,f,i,j);
            s[0]+=overlap_variant(g,NS,f,i,j,0,1,HPRES,fast,nullptr);
            s[1]+=overlap_variant(g,NS,f,i,j,1,1,HPRES,fast,nullptr);
            s[2]+=overlap_variant(g,NS,f,i,j,2,128,HPRES,fast,nullptr);
            s[3]+=overlap_variant(g,NS,f,i,j,3,128,HPRES,fast,&vs); }
        double r[4]; for(int k=0;k<4;++k) r[k]=std::fabs(s[k]-ad)/ad;
        ++R.n; if(r[0]>1e-6)++R.f0; if(r[1]>1e-6)++R.f1; if(r[2]>1e-6)++R.f2; if(r[3]>1e-6)++R.f3;
        R.w0=std::max(R.w0,r[0]);R.w1=std::max(R.w1,r[1]);R.w2=std::max(R.w2,r[2]);R.w3=std::max(R.w3,r[3]);
        R.nf+=vs.n_full; R.nq+=vs.n_quick; R.nsh+=vs.n_sh;
    }
    return R;
}
static void show(const char* tag,const Row&R){
    printf("  %-22s n=%-5d | A0 %3d/%-4d %.3e | A1 %3d/%-4d %.3e | A2 %3d/%-4d %.3e | A3 %3d/%-4d %.3e | full=%lld quick=%lld sh=%lld\n",
        tag,R.n,R.f0,R.n,R.w0,R.f1,R.n,R.w1,R.f2,R.n,R.w2,R.f3,R.n,R.w3,R.nf,R.nq,R.nsh);
}

// ---------------------------------------------------------------------------
// T27 生产 V0 直调 vs 本文件 A0 复刻（任意位置）——用于给 §4.7.1 补生产证据
// ---------------------------------------------------------------------------
static int t27_prod(double ra0,double dec0,double lo,double hi,double stp,const char* tag){
    healpix::HealpixCore hp((int)NS);
    double hp_res_rad = hp.pixelResolutionArcsec()*kArcsec2Rad;
    TanWcs w = make_wcs(ra0,dec0,0.2,23.5);
    spherical::TargetGeomCache cache(8192);
    int n=0,nfp=0,nfa=0; double wp=0,wa=0,wd=0;
    for(double x=lo;x<=hi+1e-9;x+=stp) for(double y=lo;y<=hi+1e-9;y+=stp){
        std::vector<V3> drop; if(!build_drop(w,x,y,1.0,1,drop)) continue;
        DropGeom g; build_drop_geom(drop,g);
        const double adrop = vos_area_rotated(drop);
        std::vector<spherical::Vec3> d,dd;
        for(const auto& p:drop) d.push_back({p.x,p.y,p.z});
        dd=d;
        spherical::DropGeometryT<double> pg;
        spherical::build_drop_geometry_into<double>(pg,d,&dd);
        std::vector<uint64_t> cands;
        spherical::query_candidate_pixels<double>(dd,hp,cands);
        double sp=0.0;
        for(uint64_t ipix:cands) sp += spherical::compute_overlap_area_g_ctx_cached<double>(pg,hp,ipix,hp_res_rad,cache);
        double sa=0.0;
        for(uint64_t ipix:cands){ int f;uint32_t i,j; ipix_to_fij(ipix,NS,f,i,j);
            sa += overlap_variant(g,NS,f,i,j,0,1,hp_res_rad,true,nullptr); }
        ++n;
        double rp=std::fabs(sp-adrop)/adrop, ra=std::fabs(sa-adrop)/adrop;
        if(rp>1e-6)++nfp; if(ra>1e-6)++nfa;
        wp=std::max(wp,rp); wa=std::max(wa,ra); wd=std::max(wd,std::fabs(sp-sa)/adrop);
    }
    printf("  %-30s n=%-5d | 生产V0 破门=%d/%d 最坏=%.4e | A0复刻 破门=%d/%d 最坏=%.4e | |Δ|/A_drop=%.3e\n",
           tag,n,nfp,n,wp,nfa,n,wa,wd);
    return 0;
}


// ---------------------------------------------------------------------------
// T28 接缝区亏空的尺度标定：drop 放大 1x/2x/4x/8x，看亏空是否按 1/尺寸 衰减
//     （若按 1/尺寸 衰减 ⇒ 是"交点固定绝对误差"通道，可用 long double/解析求交修）
// ---------------------------------------------------------------------------
static void t28_scale(double ra0,double dec0,const char* tag){
    healpix::HealpixCore hp((int)NS);
    printf("\n## T28 接缝亏空的尺度标定: %s (ra=%.4f dec=%.6f, nside=2^21)\n", tag, ra0, dec0);
    printf("  %-8s %-12s %-14s %-14s %-14s\n","drop倍数","drop角半径\"","A0 最坏闭合","oracle 最坏","亏空(有符号中位)");
    for (double k : {1.0,2.0,4.0,8.0}) {
        double sc = 0.2*k;
        TanWcs w = make_wcs(ra0,dec0,sc,23.5);
        double HPR = hp.pixelResolutionArcsec()*kArcsec2Rad;
        double w0=0, wo=0; std::vector<double> signed_rel;
        double rdrop=0;
        for(double x=-8;x<=8+1e-9;x+=1.0) for(double y=-8;y<=8+1e-9;y+=1.0){
            std::vector<V3> d; if(!build_drop(w,x,y,1.0,1,d)) continue;
            DropGeom g; build_drop_geom(d,g);
            double ad=vos_area_rotated(d);
            double rr=0; for(auto&p:d) rr=std::max(rr,std::atan2(norm3(cross3(p,g.center_d)),dot3(p,g.center_d)));
            rdrop=std::max(rdrop,rr);
            std::vector<spherical::Vec3> dd; for(auto&p:d) dd.push_back({p.x,p.y,p.z});
            std::vector<uint64_t> c; spherical::query_candidate_pixels<double>(dd,hp,c);
            double s0=0, so=0;
            for(uint64_t ip:c){ int f;uint32_t i,j; ipix_to_fij(ip,NS,f,i,j);
                s0 += overlap_variant(g,NS,f,i,j,0,1,HPR,false,nullptr);
                so += oracle_overlap(f,NS,i,j,d,96,1); }
            double r0=(s0-ad)/ad, ro=(so-ad)/ad;
            w0=std::max(w0,std::fabs(r0)); wo=std::max(wo,std::fabs(ro));
            signed_rel.push_back(r0);
        }
        std::sort(signed_rel.begin(),signed_rel.end());
        double med = signed_rel.empty()?0.0:signed_rel[signed_rel.size()/2];
        printf("  %-8.1fx %-12.4f %-14.3e %-14.3e %+.3e\n", k, rdrop*kRad2Arcsec, w0, wo, med);
    }
}


// ---------------------------------------------------------------------------
// T29 接缝亏空的 HST 尺度外推检验：0.04"/px + nside=2^23（复核对 T28 的预测 ~4e-3）
//     同时给 0.2"/px + nside=2^21 作对照
// ---------------------------------------------------------------------------
static void t29_hst(double ra0,double dec0,const char* tag,uint32_t Ns,double sc){
    healpix::HealpixCore hp((int)Ns);
    double HPR = hp.pixelResolutionArcsec()*kArcsec2Rad;
    TanWcs w = make_wcs(ra0,dec0,sc,23.5);
    spherical::TargetGeomCache cache(8192);
    double w0=0, wp=0, wd=0; int n=0;
    double adrop1=0;
    for(double x=-8;x<=8+1e-9;x+=1.0) for(double y=-8;y<=8+1e-9;y+=1.0){
        std::vector<V3> d; if(!build_drop(w,x,y,1.0,1,d)) continue;
        DropGeom g; build_drop_geom(d,g);
        double ad=vos_area_rotated(d); adrop1=ad;
        std::vector<spherical::Vec3> dd; for(auto&p:d) dd.push_back({p.x,p.y,p.z});
        std::vector<uint64_t> c; spherical::query_candidate_pixels<double>(dd,hp,c);
        double s0=0; for(uint64_t ip:c){ int f;uint32_t i,j; ipix_to_fij(ip,Ns,f,i,j);
            s0 += overlap_variant(g,Ns,f,i,j,0,1,HPR,false,nullptr); }
        spherical::DropGeometryT<double> pg;
        std::vector<spherical::Vec3> dd2=dd;
        spherical::build_drop_geometry_into<double>(pg,dd,&dd2);
        double sp=0; for(uint64_t ip:c) sp += spherical::compute_overlap_area_g_ctx_cached<double>(pg,hp,ip,HPR,cache);
        double r0=std::fabs(s0-ad)/ad, rp=std::fabs(sp-ad)/ad;
        w0=std::max(w0,r0); wp=std::max(wp,rp); wd=std::max(wd,std::fabs(s0-sp)/ad);
        ++n;
    }
    printf("  %-26s %-6.2f\"/px nside=2^%-2d n=%-4d | A0 最坏=%.3e (绝对 %.3e sr) | 生产V0 最坏=%.3e | |Δ|/A=%.3e\n",
           tag, sc, (int)std::log2((double)Ns), n, w0, w0*adrop1, wp, wd);
}

int main(int argc,char**argv){
    const char* which=(argc>1)?argv[1]:"all";
    healpix::HealpixCore hp((int)NS);
    HPRES = hp.pixelResolutionArcsec()*kArcsec2Rad;
    if(!std::strcmp(which,"t23")||!std::strcmp(which,"all")){
        printf("## T23 极点栅格 ±8px/0.5px (nside=2^21, 0.2\"/px)\n");
        show("use_fast=1 (生产)", scan(0,90,-8,8,0.5,true));
        show("use_fast=0",        scan(0,90,-8,8,0.5,false));
    }
    if(!std::strcmp(which,"t24")||!std::strcmp(which,"all")){
        printf("\n## T24 u+v=1 接缝中点 ra=45 dec=41.810315 ±8px/1px\n");
        show("use_fast=1", scan(45.0,41.810315,-8,8,1.0,true));
        show("use_fast=0", scan(45.0,41.810315,-8,8,1.0,false));
        printf("\n## T24b 对照: 同纬度三角点 ra=0 dec=41.810315\n");
        show("use_fast=1", scan(0.0,41.810315,-8,8,1.0,true));
        printf("\n## T24c 对照: 赤道 ra=45 dec=0\n");
        show("use_fast=1", scan(45.0,0.0,-8,8,1.0,true));
    }
    if(!std::strcmp(which,"t29")||!std::strcmp(which,"all")){
        printf("\n## T29 接缝亏空的 HST 尺度外推检验（预测：0.04\"/px 时应达 ~4e-3）\n");
        t29_hst(45.0,41.810315,"u+v=1 接缝中点",2097152u,0.20);
        t29_hst(45.0,41.810315,"u+v=1 接缝中点",8388608u,0.04);
        t29_hst(45.0,41.810315,"u+v=1 接缝中点",4194304u,0.08);
        t29_hst( 0.0,41.810315,"同纬度三角点(对照)",8388608u,0.04);
        t29_hst(45.0, 0.0,     "赤道 ra=45(对照)",  8388608u,0.04);
    }
    if(!std::strcmp(which,"t28")||!std::strcmp(which,"all")){
        t28_scale(45.0,41.810315,"u+v=1 接缝中点");
        t28_scale( 0.0,41.810315,"同纬度三角点(对照)");
        t28_scale(45.0, 0.0,     "赤道 ra=45(对照)");
    }
    if(!std::strcmp(which,"t27")||!std::strcmp(which,"all")){
        printf("\n## T27 生产 V0 直调 vs A0 复刻（T7 口径，扩展到接缝/三角点/极点）\n");
        t27_prod(45.0,41.810315,-8,8,1.0,"u+v=1 接缝中点");
        t27_prod( 0.0,41.810315,-8,8,1.0,"同纬度三角点");
        t27_prod(45.0, 0.0,     -8,8,1.0,"赤道 ra=45");
        t27_prod( 0.0,90.0,     -8,8,0.5,"极点");
    }
    if(!std::strcmp(which,"t25")||!std::strcmp(which,"all")){
        printf("\n## T25 角点公式在 u+v=1 两侧的连续性 (face0, 角点法 vs 真实曲线法)\n");
        printf("  %-8s %-16s %-12s %-14s\n","u","v","u+v","两法角距");
        for(double u : {0.25,0.5,0.75}) for(double dv : {-1e-9,-1e-12,0.0,1e-12,1e-9}){
            double v=1.0-u+dv;
            V3 a=chart_uv_to_xyz(0,u,v,0), b=chart_uv_to_xyz(0,u,v,1);
            double d=std::atan2(norm3(cross3(a,b)),dot3(a,b));
            printf("  %-8.4f %-16.12f %-12.9f %-14.3e\n",u,v,u+v,d);
        }
    }
    return 0;
}