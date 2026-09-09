// ============================================================================
// P1-PSF-TEST · core 可执行入口 (对齐 p1hips/p1star 先例: main 单独 TU)
// ----------------------------------------------------------------------------
// 单跑: ./p1psf_tests units|properties|oracle|negative|boundary|all
// CTest: p1psf_units / p1psf_properties / p1psf_oracle / p1psf_negative /
//        p1psf_boundary
// ============================================================================
#include <cstdio>

int p1psf_run_core_groups(int argc, char** argv);

int main(int argc, char** argv) {
    return p1psf_run_core_groups(argc, argv);
}
