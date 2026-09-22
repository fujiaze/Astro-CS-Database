// 校验: 直接 3D 构造 (gnomonic, 无 asin) vs pixelToSky -> radec_to_vec
#include "polar_common.h"
#include <cstdio>
#include <algorithm>
using namespace pp;
int main(){
    struct C { const char* n; double ra, dec; };
    C cs[] = {{"极点",0,90},{"极点邻域",0,89.9999},{"三角点",0,41.8103},{"赤道",0,0},{"赤道一般",17,0},{"南天",200,-60}};
    printf("## T20 3D 直接构造 vs (ra,dec) 路径：角距与 drop 面积对照\n");
    printf("  %-12s %-14s %-14s %-14s %-14s\n","位置","最大角距(rad)","最大角距/偏移","A_3D/A_radec-1","A_radec/A_3D-1");
    for (auto& c : cs) {
        TanWcs w2 = make_wcs(c.ra, c.dec, 0.2, 23.5);
        TanWcs3D w3 = make_wcs3d(c.ra, c.dec, 0.2, 23.5);
        double worst=0, worstrel=0, worst_off=0;
        for (double off : {0.0, 0.5, 2.0, 8.0, 32.0, 128.0, 512.0}) {
            for (int k=0;k<8;++k){
                double az=2*kPi*k/8; double dx=off*std::cos(az), dy=off*std::sin(az);
                double half=0.5; double q[4][2]={{dx-half,dy-half},{dx+half,dy-half},{dx+half,dy+half},{dx-half,dy+half}};
                for(int e=0;e<4;++e){
                    double ra,dec; w2.pixelToSky(q[e][0],q[e][1],ra,dec);
                    V3 a=radec_to_vec(ra,dec), b=wcs3d_sky(w3,q[e][0],q[e][1]);
                    double d=std::atan2(norm3(cross3(a,b)),dot3(a,b));
                    double r=std::atan2(std::hypot(b.x,b.y),std::fabs(b.z));
                    worst=std::max(worst,d); worst_off=std::max(worst_off,r);
                    if(r>0) worstrel=std::max(worstrel,d/r);
                }
            }
        }
        std::vector<V3> d2,d3; build_drop(w2,0,0,1.0,1,d2); build_drop3d(w3,0,0,1.0,1,d3);
        double a2=vos_area_rotated(d2), a3=vos_area_rotated(d3);
        printf("  %-12s %-14.3e %-14.3e %-14.3e %-14.3e\n", c.n, worst, worstrel, a3/a2-1.0, a2/a3-1.0);
    }
    printf("\n## T21 修正后 T14/T15 复算：切点在极点, drop 移开\n");
    printf("  %-10s %-14s %-14s %-14s\n","中心偏移px","drop角半径","|A_radec/A_3D-1|","模型 2.2e-16/r^2");
    for (double off : {0.0,0.25,0.5,1.0,2.0,4.0,8.0,16.0,32.0,64.0,128.0,256.0}) {
        TanWcs w2=make_wcs(0.0,90.0,0.2,23.5); TanWcs3D w3=make_wcs3d(0.0,90.0,0.2,23.5);
        std::vector<V3> d2,d3; build_drop(w2,off,0,1.0,1,d2); build_drop3d(w3,off,0,1.0,1,d3);
        double r=0; for(auto&p:d3) r=std::max(r,std::atan2(std::hypot(p.x,p.y),p.z));
        double a2=vos_area_rotated(d2), a3=vos_area_rotated(d3);
        printf("  %-10.2f %-14.4e %-14.3e %-14.3e\n", off, r, std::fabs(a2/a3-1.0), 2.2e-16/(r*r));
    }
    return 0;
}