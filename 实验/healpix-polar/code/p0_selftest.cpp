#include "polar_common.h"
#include <cstdio>
using namespace pp;
int main(){
    printf("## rotate_to_z 自检: R(c) 是否 == z\n");
    double worst=0;
    for (int i=0;i<12;++i){ for(int j=-80;j<=80;j+=20){
        double ra=i*30, dec=j;
        V3 c=radec_to_vec(ra,dec); V3 r=rotate_to_z(c,c);
        double d=std::atan2(norm3(cross3(r,{0,0,1})), dot3(r,{0,0,1}));
        worst=std::max(worst,d);
        if(i==0) printf("  dec=%+4d  |R(c)-z|角距=%.3e\n", j, d);
    }}
    printf("  全 12x9 组最坏 |R(c)-z| = %.3e rad  -> %s\n", worst, worst<1e-14?"PASS":"FAIL");
    printf("\n## vos_area_rotated vs 解析 drop 面积 4*asin(h^2/(1+h^2))\n");
    double h=0.5*0.2*kArcsec2Rad; double exact=4.0*std::asin(h*h/(1.0+h*h));
    for (auto pr : {std::pair<double,double>{0,90},{0,41.8103},{0,0},{200,-60},{17,0},{274.72,-13.84}}) {
        TanWcs w=make_wcs(pr.first,pr.second,0.2,23.5);
        std::vector<V3> d; build_drop(w,0,0,1.0,1,d);
        double a=vos_area_rotated(d);
        printf("  ra=%7.2f dec=%+8.4f  A=%.15e  相对差=%+.3e\n", pr.first, pr.second, a, a/exact-1);
    }
    return 0;
}