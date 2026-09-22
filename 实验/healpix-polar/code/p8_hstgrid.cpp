// p8: 定位 HST 全帧栅格上 drop 面积比最坏出现在哪一行
#include "polar_common.h"
#include <cstdio>
#include <algorithm>
using namespace pp;
int main(int argc, char** argv){
    uint32_t Ns = (argc>1)? (uint32_t)strtoul(argv[1],nullptr,10) : 8388608u;
    double sc=0.04, rot=-35.0, ra0=0.0, dec0=90.0;
    TanWcs w = make_wcs(ra0,dec0,sc,rot);
    TanWcs3D w3 = make_wcs3d(ra0,dec0,sc,rot);
    struct R{double x,y,rpole,ratio;};
    std::vector<R> rs;
    for (double x=-4000;x<=4000;x+=1000) for(double y=-4000;y<=4000;y+=1000){
        std::vector<V3> d,d3;
        if(!build_drop(w,x,y,1.0,1,d)) continue;
        build_drop3d(w3,x,y,1.0,1,d3);
        double a=vos_area_rotated(d), a3=vos_area_rotated(d3);
        double rp=1e9; for(auto&p:d3) rp=std::min(rp,std::atan2(std::hypot(p.x,p.y),std::fabs(p.z)));
        rs.push_back({x,y,rp*kRad2Arcsec, std::fabs(a/a3-1.0)});
    }
    std::sort(rs.begin(),rs.end(),[](const R&a,const R&b){return a.ratio>b.ratio;});
    printf("## T22 HST 全帧 9x9 栅格 drop 面积比排序 (nside 无关, %s)\n", "0.04\"/px rot=-35, 切点=北极");
    printf("  %-10s %-10s %-14s %-14s\n","x(px)","y(px)","离极点\"","drop面积比");
    for(size_t k=0;k<rs.size() && k<12;++k)
        printf("  %-10.1f %-10.1f %-14.4f %-14.3e\n", rs[k].x, rs[k].y, rs[k].rpole, rs[k].ratio);
    printf("  ... 共 %zu 行, 最小 %.3e\n", rs.size(), rs.back().ratio);
    return 0;
}