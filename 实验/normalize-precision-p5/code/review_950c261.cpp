#include <cstdio>
#include <cmath>
#include "acsd/sparse_reference.h"
using namespace acsd::normalize;
int main(){
 PlanarPatchWindow w; w.control_id="heteroscedastic"; w.pix_index={0,1}; w.profile={0.6,0.4}; w.variance={1,9}; w.src_x={0,1}; w.src_y={0,0}; w.leaf_ipix={101,102,103}; w.leaf_order=9; w.omega_leaf=1; w.leaf_dp={1,1,1}; w.target_leaf=101;
 w.op_pix={0,1,0,1,1}; w.op_leaf={0,0,1,1,2}; w.op_c={0.8,0.2,0.2,0.8,1};
 SphericalCompletionConfig cfg; cfg.reference.kind=ReferenceKind::kExplicitLinearFlux; cfg.reference.reference_id="review"; cfg.reference.flux=10; cfg.reference.flux_unit="adu"; cfg.reference.normalization="native_calibrated";
 auto r=complete_spherical_reference({w},cfg); const auto &c=r.precision.controls[0];
 const double oracle=1/(0.6*0.6/1+0.4*0.4/9);
 printf("production_VF=%.17g oracle_VF=%.17g relative_error=%.17g\n",*c.flux_variance,oracle,*c.flux_variance/oracle-1);
 printf("m=2 nl=3 persisted_h_size=%zu persisted_variance_size=%zu n_bg=%d z_size=%zu reported_cov_rank=%llu\n",r.windows[0].h_sphere.size(),r.windows[0].variance.size(),r.windows[0].n_bg,r.windows[0].z_sphere.size(),(unsigned long long)*c.covariance_rank);
 cfg.background=BackgroundNuisance::kConstantFit; auto rb=complete_spherical_reference({w},cfg); printf("constant_fit_background_columns=%d persisted_background_size=%zu\n",rb.windows[0].n_bg,rb.windows[0].z_sphere.size());
 return std::abs(*c.flux_variance/oracle-1) < 1e-9 ? 0 : 1;
}
