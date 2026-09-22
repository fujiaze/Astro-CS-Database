// p10_seam.cpp — M2 机制定位：u+v=1 接缝中点为何破门（与快路径/弦表示无关）
#include "polar_common.h"
#include "variants.h"
#include <cstdio>
#include <algorithm>
using namespace pp;
using namespace spherical;
static const uint32_t NS = 2097152u;
static void ipix_to_fij(uint64_t ipix, uint32_t Ns, int& face, uint32_t& i, uint32_t& j) {
    uint64_t per=(uint64_t)Ns*Ns; face=(int)(ipix/per); uint64_t rem=ipix%per;
    uint32_t xv=0,yv=0; for(int b=0;b<32;++b){xv|=(uint32_t)((rem>>(2*b))&1ull)<<b;yv|=(uint32_t)((rem>>(2*b+1))&1ull)<<b;}
    i=xv;j=yv;
}
int main(int argc,char**argv){
    healpix::HealpixCore hp((int)NS);
    double HPRES = hp.pixelResolutionArcsec()*kArcsec2Rad;
    double ra0 = (argc>1)? atof(argv[1]) : 45.0;
    double dec0= (argc>2)? atof(argv[2]) : 41.810315;
    TanWcs w=make_wcs(ra0,dec0,0.2,23.5);
    printf("## T26 接缝点 ra=%.6f dec=%.6f 的逐叶分解\n", ra0, dec0);
    for (double off : {0.0, 1.0}) {
      std::vector<V3> d; if(!build_drop(w,off,0,1.0,1,d)) { printf("  build_drop 失败\n"); continue; }
      DropGeom g; build_drop_geom(d,g);
      double ad=vos_area_rotated(d);
      std::vector<spherical::Vec3> dd; for(auto&p:d) dd.push_back({p.x,p.y,p.z});
      std::vector<uint64_t> c; spherical::query_candidate_pixels<double>(dd,hp,c);
      printf("\n  偏移 %.1fpx  A_drop=%.12e  候选叶=%zu  drop角半径=%.4f\"\n", off, ad, c.size(),
             g.max_angle*kRad2Arcsec);
      printf("  %-22s %-13s %-13s %-13s %-13s %-13s\n","(f,i,j)","A0","A2K128","oracleK96","max_angle","drop_in");
      double s0=0,s2=0,so=0;
      for(uint64_t ip:c){ int f;uint32_t i,j; ipix_to_fij(ip,NS,f,i,j);
        double a0=overlap_variant(g,NS,f,i,j,0,1,HPRES,false,nullptr);
        double a2=overlap_variant(g,NS,f,i,j,2,128,HPRES,false,nullptr);
        double ao=oracle_overlap(f,NS,i,j,d,96,1);
        s0+=a0;s2+=a2;so+=ao;
        if(a0>0||a2>0||ao>0){
          // 该叶的角尺寸
          double uv[4][2]; leaf_corner_uv(NS,i,j,uv);
          double emax=0; for(int e=0;e<4;++e){int e2=(e+1)%4;
            V3 A=chart_uv_to_xyz(f,uv[e][0],uv[e][1],1),B=chart_uv_to_xyz(f,uv[e2][0],uv[e2][1],1);
            emax=std::max(emax,std::atan2(norm3(cross3(A,B)),dot3(A,B)));}
          printf("  (%2d,%7u,%7u)  %-13.6e %-13.6e %-13.6e %-13.3e %-13.3e\n",f,i,j,a0,a2,ao,emax*kRad2Arcsec,
                 std::atan2(norm3(cross3(leaf_center_xyz(f,NS,i,j),d[0])),dot3(leaf_center_xyz(f,NS,i,j),d[0]))*kRad2Arcsec);
        }
      }
      printf("  合计 A0=%.12e (rel %+.3e)  A2K128=%.12e (rel %+.3e)  oracleK96=%.12e (rel %+.3e)\n",
             s0,s0/ad-1,s2,s2/ad-1,so,so/ad-1);
      // 暴力候选：遍历 drop 附近 3 圈
      int f0;uint32_t i0,j0; ipix_to_fij(c.empty()?0:c[0],NS,f0,i0,j0);
      long long extra=0; double sx=0;
      for(int df=-3;df<=3;++df) for(int di=-3;di<=3;++di) for(int dj=-3;dj<=3;++dj){
        int f=f0+df; if(f<0||f>11) continue; long long ii=(long long)i0+di, jj=(long long)j0+dj;
        if(ii<0||jj<0||ii>=(long long)NS||jj>=(long long)NS) continue;
        uint64_t ip=((uint64_t)f*NS*NS)+((uint64_t)jj<<1)*NS+((uint64_t)ii<<0)*1;
        uint64_t per=(uint64_t)NS*NS; uint64_t rem=(uint64_t)jj; // nest: i 低位, j 高位
        uint64_t pix=(uint64_t)f*per; for(int b=0;b<32;++b){pix|=((uint64_t)((ii>>b)&1))<<(2*b); pix|=((uint64_t)((jj>>b)&1))<<(2*b+1);}
        if(std::find(c.begin(),c.end(),pix)!=c.end()) continue;
        double a0=overlap_variant(g,NS,f,(uint32_t)ii,(uint32_t)jj,0,1,HPRES,false,nullptr);
        if(a0>0){ ++extra; sx+=a0; }
      }
      printf("  候选集合外仍有交叠的叶: %lld 个, 面积合计 %.3e (占 A_drop %.3e)\n", extra, sx, sx/ad);
    }
    return 0;
}