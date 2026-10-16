#include <cstdio>
#include <cmath>
#include <vector>
#include <string>
#include "acsd/normalize_science.h"
using namespace acsd::normalize;
static int fails=0;
static void chk(bool ok,const char*w){std::printf("  [%s] %s\n",ok?"PASS":"FAIL",w);if(!ok)++fails;}
int main(){
  // 构造一份 factor/v2 covariance + 同控制点 operator, 走 write → load → verify.
  const std::string root="/tmp/acsd_f2_product";
  LocalModelPayload cov, op;
  cov.kind=LocalModelKind::kCovariance; cov.control_id="p0_r0";
  cov.domain=LocalModelDomain::kDrizzledSphere;
  cov.source_pixel_index={0,1,2,3}; cov.leaf_ipix={10,11,12,13};
  cov.values={4.0,9.0,16.0,25.0};              // v_j ([linear]^2)
  cov.factor_variance_unit="adu^2";            // sr^0
  cov.derived_covariance_unit="adu^2/sr^2";    // sr^-2
  cov.operator_unit="sr^-1";                   // sr^-1
  cov.covariance_condition="reference_source";
  cov.background_basis_unit="1/sr";
  cov.background_basis_definition="constant_fit_unit_column";
  cov.assumptions={"diagonal_planar_condition"};
  cov.operator_rows=4; cov.operator_cols=4;
  cov.logical_covariance_rows=4; cov.logical_covariance_cols=4;
  cov.exact_factorization_of_model=true;
  cov.solver="pivoted_householder_qr_rowspace"; cov.solver_relative_tol=1e-10;
  cov.solver_rank=4; cov.solver_condition_number=1234.5;
  cov.condition_estimate_method="pivot_norm_ratio_squared";
  cov.background_basis_rank=1; cov.flux_information=2.5e-4;
  cov.flux_response_norm=0.05; cov.flux_residual_norm=std::sqrt(2.5e-4);
  cov.n_bg=1; cov.random_components={"background"}; cov.omitted_terms={"master_uncertainty"};
  cov.unit="adu^2/sr^2";
  op.kind=LocalModelKind::kDrizzleOperator; op.control_id="p0_r0";
  op.domain=LocalModelDomain::kDrizzledSphere;
  op.source_pixel_index={0,1,2,3}; op.leaf_ipix={10,11,12,13};
  op.csr_row_offsets={0,2,4,6,8}; op.csr_column_indices={0,1,1,2,2,3,3,0};
  op.csr_coefficients={0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5};
  op.matrix_rows=4; op.matrix_cols=4;
  op.source_n_leaf={8.0,8.0,8.0,8.0};
  ModelWriteResult w1,w2;
  const bool ok1=write_local_model_file(root,cov,&w1);
  const bool ok2=write_local_model_file(root,op,&w2);
  std::printf("  write cov=%d op=%d\n",(int)ok1,(int)ok2);
  chk(ok1&&ok2,"write_local_model_file both models");
  const std::string rel="models/p0_r0.covariance.json";
  auto lc=load_local_model_payload(root,rel);
  auto lo=load_local_model_payload(root,"models/p0_r0.drizzle_operator.json");
  chk(lc.ok&&lo.ok,"load both model payloads");
  std::printf("  loaded: factor_unit=%s derived=%s op_unit=%s repr-in-file check\n",
              lc.payload.factor_variance_unit.c_str(),lc.payload.derived_covariance_unit.c_str(),
              lc.payload.operator_unit.c_str());
  chk(lc.payload.factor_variance_unit=="adu^2" &&
      lc.payload.derived_covariance_unit=="adu^2/sr^2" &&
      lc.payload.operator_unit=="sr^-1" &&
      lc.payload.covariance_condition=="reference_source" &&
      lc.payload.solver=="pivoted_householder_qr_rowspace" &&
      lc.payload.solver_rank==4 &&
      lc.payload.condition_estimate_method=="pivot_norm_ratio_squared" &&
      lc.payload.flux_information>0.0 &&
      lc.payload.flux_response_norm>0.0 &&
      lc.payload.flux_residual_norm>0.0 &&
      std::abs(lc.payload.flux_residual_norm*lc.payload.flux_residual_norm-
               lc.payload.flux_information)<1e-15,
      "v2 unit/shape/solver fields restored without confusion");
  chk(lc.payload.logical_covariance_rows==4 && lc.payload.logical_covariance_cols==4,
      "logical_covariance_shape = [nl,nl] (not [m,1])");
  auto rec=reconstruct_factor_covariance(lc.payload,lo.payload,1u<<20,"adu");
  chk(rec.ok,"factor reconstruction ok");
  std::printf("  rec rows=%d cols=%d sr(v)=%g sr(R)=%g sr(C)=%g\n",rec.rows,rec.cols,
              rec.factor_unit_sr_power,rec.operator_unit_sr_power,rec.derived_unit_sr_power);
  chk(rec.factor_unit_sr_power==0 && rec.operator_unit_sr_power==-1 &&
      rec.derived_unit_sr_power==-2, "sr powers 0/-1/-2 self-consistent");
  chk(rec.linear_unit_checked && rec.factor_linear_unit=="adu^2" &&
      rec.derived_linear_unit=="adu^2",
      "linear base units checked against frame signal unit (adu)");
  // 独立重算 C = R diag(v) Rᵀ（第三条路径, 直接由 CSR 三元组构造）
  std::vector<double> C(16,0.0);
  for (int l=0;l<4;++l){
    // 该叶的非零
    std::vector<std::pair<int,double>> e;
    for (int k=lo.payload.csr_row_offsets[l];k<lo.payload.csr_row_offsets[l+1];++k)
      e.emplace_back(lo.payload.csr_column_indices[k],lo.payload.csr_coefficients[k]);
    for (auto&a:e) C[l*4+l]+=a.second*cov.values[a.first]*a.second;
    for (std::size_t i=0;i<e.size();++i)
      for (std::size_t j=i+1;j<e.size();++j){
        // 共享像元对由 per-pixel 处理; 同叶不同像元不产生非对角
      }
  }
  // 非对角（共享源像元）
  std::vector<std::vector<std::pair<int,double>>> pp(4);
  for (int l=0;l<4;++l)
    for (int k=lo.payload.csr_row_offsets[l];k<lo.payload.csr_row_offsets[l+1];++k)
      pp[lo.payload.csr_column_indices[k]].emplace_back(l,lo.payload.csr_coefficients[k]);
  for (int j=0;j<4;++j) for (std::size_t i=0;i<pp[j].size();++i)
    for (std::size_t q=i+1;q<pp[j].size();++q){
      double t=pp[j][i].second*cov.values[j]*pp[j][q].second;
      C[pp[j][i].first*4+pp[j][q].first]+=t;
      C[pp[j][q].first*4+pp[j][i].first]+=t;
    }
  double worst=0; for (int i=0;i<16;++i) worst=std::max(worst,std::abs(C[i]-rec.c_row_major[i]));
  std::printf("  independent C vs reconstruction max|diff| = %.3e\n",worst);
  chk(worst==0.0,"reconstructed C == independent R diag(v) R^T (bit-identical)");
  // C[l][l] = Σ_j c_lj² v_j; 本测试每叶两个 c=0.5 ⇒ = v_0*0.25 + v_1*0.25（叶0）
  chk(std::abs(rec.c_row_major[0]-(4.0*0.25+9.0*0.25))<1e-15 &&
      std::abs(rec.c_row_major[5]-(9.0*0.25+16.0*0.25))<1e-15 &&
      std::abs(rec.c_row_major[10]-(16.0*0.25+25.0*0.25))<1e-15 &&
      std::abs(rec.c_row_major[15]-(25.0*0.25+4.0*0.25))<1e-15,
      "C diagonal = sum_j c_lj^2 v_j (analytic, bit-exact)");
  // 负例: 单位不自洽 ⇒ 必须拒绝
  {
    LocalModelPayload bad=lc.payload; bad.factor_variance_unit="adu^2/sr^2";
    auto r2=reconstruct_factor_covariance(bad,lo.payload,1u<<20,"adu");
    std::printf("  bad-unit rec ok=%d err=%s\n",(int)r2.ok,r2.error.c_str());
    chk(!r2.ok,"wrong factor unit rejected with a named error");
  }
  // 负例（NORMALIZE_DATA §7.1B）: sr 幂次 0/-1/-2 全部正确, 但线性单位与帧
  // signal 单位不是同一个 L（electron^2 vs adu）⇒ 必须拒绝（旧 reader 会放过）
  {
    LocalModelPayload bad=lc.payload;
    bad.factor_variance_unit="electron^2";
    bad.derived_covariance_unit="electron^2/sr^2";
    auto r2=reconstruct_factor_covariance(bad,lo.payload,1u<<20,"adu");
    std::printf("  linear-unit-mismatch rec ok=%d err=%s\n",(int)r2.ok,r2.error.c_str());
    chk(!r2.ok && r2.error.find("linear")!=std::string::npos,
        "sr powers correct but base linear unit mismatch rejected");
  }
  // 负例: 帧 signal 线性单位未知 ⇒ 不许仅凭 sr 指数接纳
  {
    auto r2=reconstruct_factor_covariance(lc.payload,lo.payload,1u<<20,"");
    std::printf("  unknown-frame-unit rec ok=%d err=%s\n",(int)r2.ok,r2.error.c_str());
    chk(!r2.ok,"unknown frame linear unit rejected (sr exponent alone insufficient)");
  }
  // 负例: CSR 重复贡献
  {
    LocalModelPayload badop=lo.payload;
    badop.csr_column_indices={0,0,1,2,2,3,3,0};
    auto r2=reconstruct_factor_covariance(lc.payload,badop,1u<<20,"adu");
    std::printf("  dup-csr rec ok=%d err=%s\n",(int)r2.ok,r2.error.c_str());
    chk(!r2.ok,"duplicate CSR (row,column) rejected");
  }
  // 帧级 verify: 正常产品不得产生 findings
  {
    FrameScienceDocument doc;
    ModelFileRef a; a.model_id="p0_r0.covariance"; a.kind=LocalModelKind::kCovariance;
    a.path="models/p0_r0.covariance.json";
    ModelFileRef b; b.model_id="p0_r0.drizzle_operator"; b.kind=LocalModelKind::kDrizzleOperator;
    b.path="models/p0_r0.drizzle_operator.json";
    doc.models={a,b};
    std::vector<std::string> f;
    verify_frame_science_factor_models(root,doc,&f);
    std::printf("  verify findings=%zu\n",f.size());
    for (auto&s:f) std::printf("    - %s\n",s.c_str());
    chk(f.empty(),"well-formed factor/v2 product passes reader verification");
  }
  // v1 拒绝为 canonical（前台裁决: 不悄悄升级单位）
  {
    FrameScienceDocument doc;
    ModelFileRef a; a.model_id="p0_r0.covariance"; a.kind=LocalModelKind::kCovariance;
    a.path="models/p0_r0.covariance.json"; a.representation="acsd.normalize.local-factor/v1";
    doc.models={a};
    std::vector<std::string> f;
    verify_frame_science_factor_models(root,doc,&f);
    std::printf("  v1 findings=%zu\n",f.size());
    chk(!f.empty(),"local-factor/v1 rejected as canonical without explicit adapter");
  }
  std::printf("\n%s: %d failure(s)\n",fails?"FAILED":"ALL PASS",fails);
  return fails?1:0;
}
