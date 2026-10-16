// ACSD Normalize — 球面参考精度生产接线实现（不含科学公式; 数学在 noise 库）
#include "acsd/normalize_precision.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <map>
#include <set>
#include <sstream>

#include "acsd/io/aio_abi_v1.h"       // aio_content_hash_buffer_v1（sha256 唯一 C ABI）
#include "healpix/healpix_core.h"     // acsd::healpix::ang2pix_nest（唯一实现）
#include "acsd/sparse_reference.h"    // noise 库 GLS 完成
#include "snr_estimator.h"            // legacy 诊断通道（不参与 canonical 模板）
#include "acsd/pixel_integrated_psf.h" // 单位总通量像元积分（PSF 库唯一实现）

namespace acsd::phase1::precision {

using acsd::normalize::ReferenceDefinition;
using acsd::normalize::ReferenceKind;
using acsd::normalize::PlanarPatchWindow;
namespace {

constexpr double kPi = 3.14159265358979323846;

// ── 单位总通量像元积分 PSF 模板（PSF.md §4 裁决 / detail §7.4B）───────────
// 数学**唯一**实现在 PSF 库（acsd/pixel_integrated_psf.h）:
//   P_j = (3/(2π σ²)) ∫_{E_j} (1 + |r-r₀|²/(2σ²))^{-4} dA,  r₀ = 亚像元相位
// 本单元不再自行维护裸中心值模板（那正是第一轮被裁定为
// `center_sample_unnormalized_legacy`、不得作为 analytic_total canonical 输入的实现）。
// 旧实现保留在 lib/algorithms/noise_snr 之外的历史实验中; 生产路径只消费 PSF 库。
bool pixel_integrated_profile(double sigma_px, double cx, double cy,
                              const std::vector<std::int64_t>& pix, int width,
                              std::vector<double>* out_p,
                              acsd::psf::PixelIntegrationDiagnostics* diag) {
  if (!(sigma_px > 0.0)) return false;
  const std::size_t m = pix.size();
  if (m == 0) return false;
  std::vector<double> px(m), py(m);
  for (std::size_t k = 0; k < m; ++k) {
    px[k] = static_cast<double>(pix[k] % width);
    py[k] = static_cast<double>(pix[k] / width);
  }
  acsd::psf::PixelIntegrationRequest r;
  r.sx = sigma_px;              // Moffat4 σ 口径（PSF.md §3.1 唯一口径）
  r.sy = sigma_px;
  r.theta = 0.0;                // 当前模型各向同性; 椭圆/角度走 PSF 空间模型
  r.cx = cx;                    // 亚像元参考中心相位（0-based）
  r.cy = cy;
  r.pixel_x = px.data();
  r.pixel_y = py.data();
  r.n_pixels = m;
  r.order = 10;                 // 每维 10 节点 ⇒ 100 点/像元; 阶自检误差
  return acsd::psf::pixel_integrated_moffat4_profile(r, out_p, diag);
}

}  // namespace

// ── 运行身份/输入哈希（sidecar 身份字段; 缺项显式空串, 不编造）──────────────
bool load_run_identity(const std::string& output_dir, std::string* run_id,
                       std::string* software_sha, std::string* source_sha) {
  *run_id = *software_sha = *source_sha = std::string();
  const std::string path = output_dir + "/run_context.json";
  std::ifstream f(path);
  if (!f) return false;
  std::ostringstream ss;
  ss << f.rdbuf();
  try {
    const Json j = Json::parse(ss.str());
    *run_id = j.value("run_id", std::string());
    *software_sha = j.value("source_sha", std::string());
    *source_sha = j.value("source_sha", std::string());
    if (software_sha->empty()) *software_sha = j.value("build_head_sha", std::string());
    return true;
  } catch (const std::exception&) {
    return false;
  }
}

bool file_sha256_hex(const std::string& path, std::string* out) {
  std::ifstream f(path, std::ios::binary);
  if (!f) return false;
  std::ostringstream ss;
  ss << f.rdbuf();
  const std::string bytes = ss.str();
  char hex[65] = {0};
  if (aio_content_hash_buffer_v1(bytes.data(), bytes.size(), hex) != 0) return false;
  *out = std::string(hex);
  return true;
}

ReferenceDefinition parse_reference_config(const Json& snr_cfg, std::string* why) {
  ReferenceDefinition def;
  def.kind = ReferenceKind::kUnavailable;
  def.reason = "no snr.reference configured (canonical reference precision "
               "unavailable; legacy F0 path stays a named diagnostic)";
  if (!snr_cfg.is_object() || !snr_cfg.contains("reference") ||
      !snr_cfg["reference"].is_object()) {
    if (why) *why = def.reason;
    return def;
  }
  const Json& r = snr_cfg["reference"];
  const std::string kind = r.value("kind", std::string());
  if (kind == "explicit_linear_flux") {
    def.kind = ReferenceKind::kExplicitLinearFlux;
    def.reference_id = r.value("reference_id", std::string());
    def.flux_unit = r.value("flux_unit", std::string());
    def.normalization = r.value("normalization", std::string());
    if (r.contains("flux") && r["flux"].is_number()) {
      def.flux = r["flux"].get<double>();
    }
    if (def.reference_id.empty() || def.flux_unit.empty() ||
        def.normalization.empty() || !def.flux.has_value() ||
        !std::isfinite(*def.flux) || !(*def.flux > 0.0)) {
      def.kind = ReferenceKind::kUnavailable;
      def.reason = "explicit_linear_flux reference requires positive finite flux, "
                   "reference_id, flux_unit and normalization "
                   "(NORMALIZE_DATA §5)";
    } else {
      def.reason.clear();
    }
  } else if (kind == "sed_response") {
    def.kind = ReferenceKind::kSedResponse;
    def.sed_id = r.value("sed_id", std::string());
    def.response_id = r.value("response_id", std::string());
    def.flux_unit = r.value("flux_unit", std::string());
    def.normalization = r.value("normalization", std::string());
    // sed_response 需要可求值的响应积分（谱曲线 + 归一/零点）; 未接线时显式
    // unavailable（不伪造绝对零点）。缺失输入**逐项列出**（合同与裁决点见
    // 实验/normalize-precision-p5/ROUND9_SED_RESPONSE_CONTRACT.md §4）:
    //   (1) 参考 SED 身份/解析路径与按星 flux_mul/flux_min、波长网格;
    //   (2) 响应曲线 R_k 数值本体是否落盘为产品（现仅落身份）;
    //   (3) 零点/归一约定（A 帧相对 F_ref=a_k·m_k(u)·F_syn_ref, 或 B 绝对零点）
    //       与 reference.normalization 取值;
    //   (4) 参考 SED 外定标误差与拟合 a_k 参数协方差是否显式进入 V_F;
    //   (5) 星等→通量换算所用零点常数的适用范围。
    def.reason = "sed_response requires evaluable response integration; "
                 "not wired in this build. Missing inputs (see "
                 "ROUND9_SED_RESPONSE_CONTRACT.md §4): (1) reference SED "
                 "identity/resolution path and per-star flux_mul/flux_min + "
                 "wavelength grid; (2) whether R_k curve values are published "
                 "as a product (only identity is recorded today); "
                 "(3) zero-point/normalization convention (A frame-relative "
                 "F_ref=a_k*m_k(u)*F_syn_ref, or B absolute zero point) and "
                 "reference.normalization; (4) reference-SED external "
                 "calibration error and fitted a_k covariance propagation "
                 "into V_F; (5) applicability of any magnitude-to-flux zero "
                 "point constant. explicit_linear_flux remains the only "
                 "canonical path; precision unavailable, signal unaffected";
  } else {
    def.reason = "unknown snr.reference.kind='" + kind + "' (expected "
                 "explicit_linear_flux|sed_response) ⇒ unavailable";
  }
  if (why) *why = def.reason;
  return def;
}

bool frame_psf_sigma_px(const Json& sources_doc, const std::string& frame_key,
                        double* out_sigma_px, std::string* why) {
  if (out_sigma_px) *out_sigma_px = 0.0;
  if (!sources_doc.is_object() || !sources_doc.contains("frames") ||
      !sources_doc["frames"].is_array()) {
    if (why) *why = "p1_sources.json missing frames[]";
    return false;
  }
  for (const auto& fr : sources_doc["frames"]) {
    if (!fr.is_object()) continue;
    // frames[].file = <节点前缀>_<base>.<ext>; frame_key = <base>（与 drizzle 节点
    // nm_stem 同一口径: 去 cleaned_/calibrated_/photoapplied_ 前缀 + 去扩展名）。
    std::string file = fr.value("file", std::string());
    for (const char* pfx : {"cleaned_", "calibrated_", "photoapplied_"}) {
      const std::size_t n = std::strlen(pfx);
      if (file.rfind(pfx, 0) == 0) { file = file.substr(n); break; }
    }
    const std::size_t dot = file.find_last_of('.');
    if (dot != std::string::npos) file = file.substr(0, dot);
    // 与 p1_frame_key 同一字符白名单（frame_key 已净化; 原始基名可能含 '@' 等）
    std::string sane;
    for (char c : file) {
      const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                      (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.';
      sane.push_back(ok ? c : '_');
    }
    if (sane != frame_key) continue;
    if (!fr.contains("psf_params") || !fr["psf_params"].is_array()) {
      if (why) *why = "frame " + frame_key + " has no psf_params";
      return false;
    }
    std::vector<double> sx, sy;
    for (const auto& p : fr["psf_params"]) {
      if (!p.is_object()) continue;
      if (p.contains("sx") && p["sx"].is_number()) sx.push_back(p["sx"].get<double>());
      if (p.contains("sy") && p["sy"].is_number()) sy.push_back(p["sy"].get<double>());
    }
    if (sx.empty() || sy.empty()) {
      if (why) *why = "frame " + frame_key + " psf_params have no sx/sy";
      return false;
    }
    std::sort(sx.begin(), sx.end());
    std::sort(sy.begin(), sy.end());
    const double mx = sx[sx.size() / 2];
    const double my = sy[sy.size() / 2];
    if (!(mx > 0.0) || !(my > 0.0) || !std::isfinite(mx) || !std::isfinite(my)) {
      if (why) *why = "frame " + frame_key + " psf sigma non-physical";
      return false;
    }
    if (out_sigma_px) *out_sigma_px = std::sqrt(mx * my);
    if (why) *why = "";
    return true;
  }
  if (why) *why = "frame " + frame_key + " not found in p1_sources.json";
  return false;
}

double precision_window_radius_px(const PrecisionFrameRequest& req) {
  const double patch_radius =
      0.5 * std::min(static_cast<double>(req.width) / std::max(1, req.patch_grid_x),
                     static_cast<double>(req.height) / std::max(1, req.patch_grid_y));
  constexpr double kWindowPixelBudget = 1024.0;
  const double psf_radius =
      req.have_psf ? std::max(4.0, 6.0 * req.psf_sigma_px) : patch_radius;
  const double budget_radius = std::sqrt(kWindowPixelBudget / M_PI);
  return std::max(4.0, std::min(patch_radius, std::min(psf_radius, budget_radius)));
}

std::unordered_map<uint64_t, drizzle::DrizzleEngine::OverlapTarget>
build_capture_targets(
    const PrecisionFrameRequest& req,
    const std::function<void(double, double, double*, double*)>& pixel_to_sky_fits1) {
  std::unordered_map<uint64_t, drizzle::DrizzleEngine::OverlapTarget> targets;
  if (req.nside <= 0 || req.ctrl_x.size() != req.ctrl_y.size()) return targets;
  // 与 build_patch_windows **同一**半径推导（窗口/捕获候选必须一致; 曾因两处
  // 公式不一致导致候选叶集 19M、目标计数爆炸）。
  const double radius = precision_window_radius_px(req);
  // 目标叶集 = 每个控制点窗口圆盘在球面覆盖的**全部**叶（query_disc 四叉树下钻）,
  // 而不是单一中心叶 —— 否则窗口只拿到目标叶上的算子项, 协方差结构不完整。
  for (std::size_t i = 0; i < req.ctrl_x.size(); ++i) {
    double ra = 0.0, dec = 0.0;
    // FITS 1-based 桥接（内部 0-based 中心 +1; SIP 由回调统一施加）
    pixel_to_sky_fits1(req.ctrl_x[i] + 1.0, req.ctrl_y[i] + 1.0, &ra, &dec);
    if (!std::isfinite(ra) || !std::isfinite(dec)) continue;
    drizzle::DrizzleEngine::OverlapTarget t;
    t.cx = req.ctrl_x[i];
    t.cy = req.ctrl_y[i];
    t.radius = radius;
    try {
      // 圆盘半径（角秒）: 取像元角尺度的保守估计（query_disc 只要覆盖足量叶;
      // 精确边界由捕获时的像元—半径判定保证, 这里只决定候选叶集）。
      const double res_arcsec = acsd::healpix::pixel_resolution_arcsec(
          static_cast<std::uint32_t>(req.nside));
      const double disc_radius_arcsec =
          std::max(res_arcsec * 2.0, radius * req.pixel_scale_arcsec);
      const std::vector<std::uint64_t> leaves = acsd::healpix::query_disc(
          static_cast<std::uint32_t>(req.nside), ra, dec, disc_radius_arcsec);
      for (const std::uint64_t ipix : leaves) targets.emplace(ipix, t);
    } catch (const std::exception&) {
      continue;  // nside 非法等 ⇒ 该控制点无目标叶（调用方按缺项处理）
    }
  }
  return targets;
}

std::vector<acsd::normalize::PlanarPatchWindow> build_patch_windows(
    const PrecisionFrameRequest& req,
    const std::vector<drizzle::DrizzleEngine::SourceLeafOverlap>& overlaps,
    const std::function<void(double, double, double*, double*)>& pixel_to_sky_fits1,
    double omega_leaf,
    const std::unordered_map<std::uint64_t, drizzle::DrizzleEngine::OverlapTarget>&
        targets) {
  std::vector<acsd::normalize::PlanarPatchWindow> windows;
  if (req.ctrl_x.size() != req.ctrl_y.size()) return windows;
  // 模板足迹半径是**局部计算值**（不再通过 const_cast 写回 const 请求对象 ——
  // 那会对并发共享的 req 造成未定义行为; 见 NORMALIZE_PIPELINE §9）。
  const double template_radius = precision_window_radius_px(req);
  int leaf_order = 0;
  {
    int k = 0;
    while ((1u << k) < static_cast<unsigned>(req.nside)) ++k;
    leaf_order = k;   // 实际叶阶 log2(nside)（detail §7.4: 不是 tile 阶）
  }

  for (std::size_t ci = 0; ci < req.ctrl_x.size(); ++ci) {
    acsd::normalize::PlanarPatchWindow w;
    w.control_id = "p" + std::to_string(ci / std::max(1, req.patch_grid_y)) + "_r" +
                   std::to_string(ci % std::max(1, req.patch_grid_y));
    w.cx_px = req.ctrl_x[ci];
    w.cy_px = req.ctrl_y[ci];
    w.leaf_order = leaf_order;
    w.omega_leaf = omega_leaf;
    {  // ICRS（FITS 1-based 桥接 + SIP; pix2sky 单一实现由调用方注入）
      double ra = 0.0, dec = 0.0;
      pixel_to_sky_fits1(w.cx_px + 1.0, w.cy_px + 1.0, &ra, &dec);
      if (std::isfinite(ra) && std::isfinite(dec)) {
        w.ra_deg = ra;
        w.dec_deg = dec;
      }
    }
    // 目标叶 = 控制点 sky 的统一 NESTED ang2pix（F06: 不用 D_l 最大的叶; 不用
    // target_leaf==0 当缺失判据）。ang2pix 由调用方经 pixel_to_sky 提供 ICRS, 这里
    // 直接复用同一 healpix 实现（唯一实现, 不在本单元重写）。
    if (req.nside > 0 && std::isfinite(w.ra_deg) && std::isfinite(w.dec_deg)) {
      // 统一 HEALPix NESTED 实现（唯一, 不在本单元重写）; 合法编号 0 与其它编号
      // 同权, 由 has_target_leaf 表达"是否解出"。
      w.has_target_leaf = true;
      w.target_leaf = acsd::healpix::ang2pix_nest(
          static_cast<std::uint32_t>(req.nside), w.ra_deg, w.dec_deg);
    }

    // ── 观测域 = 归属本控制点的目标叶 + 给这些叶贡献的**全部**源像元 ──────────
    // （NOISE_SNR §11.2: 所选观测叶的完整有效样本; 不在小窗口内重新归一）。
    // 归属判定用 targets 的 cx/cy（capture 以叶为键, 同叶只归属第一个控制点）。
    std::map<std::int64_t, int> pix_slot;
    std::map<std::uint64_t, int> leaf_slot;
    for (const auto& o : overlaps) {
      const auto tit = targets.find(o.leaf_ipix);
      if (tit == targets.end()) continue;
      if (tit->second.cx != w.cx_px || tit->second.cy != w.cy_px) continue;
      const std::int64_t idx =
          static_cast<std::int64_t>(o.py) * req.width + o.px;
      const auto pit = pix_slot.find(idx);
      int pslot;
      if (pit == pix_slot.end()) {
        pslot = static_cast<int>(pix_slot.size());
        pix_slot.emplace(idx, pslot);
      } else {
        pslot = pit->second;
      }
      const auto lit = leaf_slot.find(o.leaf_ipix);
      int lslot;
      if (lit == leaf_slot.end()) {
        lslot = static_cast<int>(leaf_slot.size());
        leaf_slot.emplace(o.leaf_ipix, lslot);
      } else {
        lslot = lit->second;
      }
      w.op_pix.push_back(pslot);
      w.op_leaf.push_back(lslot);
      w.op_c.push_back(0.0);   // 占位; 汇总后按完整 N_l 回填
      w.op_raw_a.push_back(o.a_jp);
      w.op_raw_adrop.push_back(o.a_drop);
      w.op_raw_apixel.push_back(o.a_pixel);
      w.op_raw_leaf.push_back(lslot);
      w.op_raw_pix.push_back(pslot);
    }
    const int m = static_cast<int>(pix_slot.size());
    const int nl = static_cast<int>(leaf_slot.size());
    w.pix_index.resize(m);
    for (const auto& kv : pix_slot) w.pix_index[kv.second] = kv.first;
    w.leaf_ipix.resize(nl);
    for (const auto& kv : leaf_slot) w.leaf_ipix[kv.second] = kv.first;
    w.profile.assign(m, 0.0);
    w.variance.assign(m, 0.0);
    w.src_x.assign(m, 0.0);
    w.src_y.assign(m, 0.0);
    w.pix_omega.assign(m, 0.0);
    w.pix_a_drop.assign(m, 0.0);
    for (int j = 0; j < m; ++j) {
      w.src_x[j] = static_cast<double>(w.pix_index[j] % req.width);
      w.src_y[j] = static_cast<double>(w.pix_index[j] / req.width);
    }
    // 每像元 Ω/A_drop（同一捕获记录; 与 pix_index 对齐）
    for (std::size_t k = 0; k < w.op_raw_pix.size(); ++k) {
      const int p = w.op_raw_pix[k];
      if (p < 0 || p >= m) continue;
      w.pix_omega[static_cast<std::size_t>(p)] = w.op_raw_apixel[k];
      w.pix_a_drop[static_cast<std::size_t>(p)] = w.op_raw_adrop[k];
    }
    // D_l = Σ_j a_lj（全部贡献像元）;  N_l = Σ_j (a_lj/A_drop,j)·Ω_j
    // = 同次生产该叶的**完整**面亮度归一分母（与 sink 的 sumNorm 同一口径）。
    w.leaf_dp.assign(nl, 0.0);
    w.leaf_n_full.assign(nl, 0.0);
    w.leaf_n_covered.assign(nl, 0.0);
    for (std::size_t k = 0; k < w.op_raw_a.size(); ++k) {
      const int l = w.op_raw_leaf[k];
      if (l < 0 || l >= nl) continue;
      if (!(w.op_raw_adrop[k] > 0.0)) continue;
      w.leaf_dp[static_cast<std::size_t>(l)] += w.op_raw_a[k];
      w.leaf_n_full[static_cast<std::size_t>(l)] +=
          w.op_raw_a[k] * (w.op_raw_apixel[k] / w.op_raw_adrop[k]);
    }
    // 回填 c_lj = (a_lj/A_drop,j)/N_l  —— 生产 signal 的 R=w/N（NOISE_SNR §11.2;
    // 写盘前 flux 载荷的 w·D_p/N_p 仍缺一次除以 D_p, 不是这个 R）。
    for (std::size_t k = 0; k < w.op_raw_a.size(); ++k) {
      const int l = w.op_raw_leaf[k];
      if (l < 0 || l >= nl) continue;
      if (!(w.op_raw_adrop[k] > 0.0)) continue;
      const double nlz = w.leaf_n_full[static_cast<std::size_t>(l)];
      if (!(nlz > 0.0)) continue;
      const double c = (w.op_raw_a[k] / w.op_raw_adrop[k]) / nlz;
      w.op_c[k] = c;
      // 观测域按构造完整 ⇒ covered == full（留待将来引入显式子域时区分）
      w.leaf_n_covered[static_cast<std::size_t>(l)] =
          w.leaf_n_full[static_cast<std::size_t>(l)];
    }
    windows.push_back(std::move(w));
  }
  return windows;
}

StagedPrecision stage_frame_precision(
    const PrecisionFrameRequest& req,
    const std::vector<acsd::normalize::PlanarPatchWindow>& windows_in,
    acsd::normalize::FrameScienceDocument doc_template,
    const std::string& frame_key,
    const std::vector<float>* var_plane,
    std::vector<acsd::normalize::LocalModelPayload>* model_payloads) {
  StagedPrecision out;

  // ── 参考通量的单位配对（合同 §3: reference_flux 用与其模板 h 配对的积分通量尺度）──
  // 输入面 = photoapplied（像元值 = α·原生 ADU）: 原生参考通量 F 在该面上表现为
  // α·F; 反向声明则除 α。变换只是线性单位换算（SNR 在确定性缩放下不变）, 结果
  // 记入 reference.reason, 不静默、不伪造绝对零点。
  double f_ref_scale = 1.0;
  {
    const double alpha = req.applied_scale;
    if (!(alpha > 0.0) || !std::isfinite(alpha)) {
      out.error = "applied_scale must be positive finite";
      return out;
    }
    if (req.photometry_applied &&
        req.reference.normalization == "native_calibrated") {
      f_ref_scale = alpha;
    } else if (!req.photometry_applied &&
               req.reference.normalization == "photo_applied") {
      f_ref_scale = 1.0 / alpha;
    }
  }
  acsd::normalize::ReferenceDefinition ref_def = req.reference;
  // 未配置参考时 unit/normalization 继承调用方（drizzle 节点）按输入面真实状态
  // 填的值 —— schema 要求非空, 但不能因此伪造参考身份。
  if (ref_def.flux_unit.empty()) ref_def.flux_unit = doc_template.reference.flux_unit;
  if (ref_def.normalization.empty())
    ref_def.normalization = doc_template.reference.normalization;
  if (f_ref_scale != 1.0 && ref_def.flux.has_value()) {
    ref_def.flux = *ref_def.flux * f_ref_scale;
    ref_def.reason = "reference_flux converted to the drizzle input plane scale "
                     "(applied k_photo=" + std::to_string(req.applied_scale) + ")";
  }
  doc_template.reference = ref_def;

  // ── 每窗口: 单位总通量像元积分模板 P（PSF 库）+ 条件方差 v_j ──────────────
  // flux_estimator = analytic_total（PSF.md §4 默认且唯一 canonical 口径）:
  //   P_j 由 PSF 库对**实际源像元足迹** E_j 与亚像元相位求积, ΣP 不重归一,
  //   f_enc = ΣP, f_out = 1 - ΣP 均由**实际像元集合**得到（不用理想圆闭式代替,
  //   也不用 min(1,Σ) 把噪声粉饰成 100% 包络）。
  std::vector<acsd::normalize::PlanarPatchWindow> windows = windows_in;
  // 模板足迹半径: 本单元局部推导（与 build_patch_windows 同一函数, 同一口径）。
  const double template_radius = precision_window_radius_px(req);
  bool psf_ok = req.have_psf;
  // 逐控制点求积诊断（R2-F01 审查修正: 每个控制点记录**自己的** f_enc/f_out 与
  // 求积误差; 帧级 extrema 只作摘要, 不把单一最小值写给每个控制点）。
  std::vector<acsd::psf::PixelIntegrationDiagnostics> win_diag(windows.size());
  // 每控制点使用的 PSF 模型身份与三角定位诊断（PSF.md §4 空间模型 vs 帧常近似）。
  std::vector<std::string> win_psf_model(windows.size());
  std::vector<acsd::psf::SpatialResponseDiagnostics> win_spatial(windows.size());
  std::string flux_estimator = req.have_psf ? "analytic_total" : "unavailable";
  // ── 空间 Moffat4 模型（PSF.md §4 基础完成模型）────────────────────────────
  // 顶点 = 本帧有效星（p1_sources.json psf_params 行; 模块内做 sx>=sy 轴约定与
  // theta mod π 归约）。剖分失败或顶点不足 ⇒ 全帧回退帧常 σ 具名近似（登记,
  // 不冒充逐位置模型）; 凸包外的控制点逐点回退并登记（凸包外不外推）。
  acsd::psf::DelaunayTriangulation spatial_model;
  bool spatial_ok = false;
  if (!req.psf_vertices.empty()) {
    spatial_ok = acsd::psf::build_spatial_model(req.psf_vertices, &spatial_model);
  }
  for (std::size_t wi = 0; wi < windows.size(); ++wi) {
    auto& w = windows[wi];
    const std::size_t m = w.pix_index.size();
    if (m == 0) continue;
    if (psf_ok) {
      // 逐位置模型优先: 空间三角剖分重心凸组合（凸包内）; 否则帧常 σ 具名近似。
      bool used_spatial = false;
      if (spatial_ok) {
        std::vector<double> px(m), py(m);
        for (std::size_t k = 0; k < m; ++k) {
          px[k] = static_cast<double>(w.pix_index[k] % req.width);
          py[k] = static_cast<double>(w.pix_index[k] / req.width);
        }
        acsd::psf::SpatialResponseRequest sr;
        sr.cx = w.cx_px;
        sr.cy = w.cy_px;
        sr.pixel_x = px.data();
        sr.pixel_y = py.data();
        sr.n_pixels = m;
        sr.order = 10;
        acsd::psf::SpatialResponseDiagnostics sd;
        if (acsd::psf::spatial_pixel_response(spatial_model, sr, &w.profile,
                                              &win_diag[wi], &sd) &&
            sd.ok) {
          used_spatial = true;
          win_spatial[wi] = sd;
          win_psf_model[wi] = "spatial_delaunay_barycentric";
        } else {
          win_psf_model[wi] =
              "frame_constant_median_sigma_fallback_" + sd.reject;
        }
      } else {
        win_psf_model[wi] = "frame_constant_median_sigma_no_spatial_support";
      }
      if (!used_spatial &&
          !pixel_integrated_profile(req.psf_sigma_px, w.cx_px, w.cy_px,
                                    w.pix_index, req.width, &w.profile,
                                    &win_diag[wi])) {
        // 求积失败 ⇒ 该帧没有合法单位总通量模板 ⇒ **不得**退化为中心值近似;
        // 控制点由 GLS 侧记 unavailable（不用未归一模板冒充 analytic_total）。
        psf_ok = false;
        break;
      }
    } else {
      // 无 PSF ⇒ 模板缺失 ⇒ 该控制点由 GLS 侧记 unavailable（不用虚构 P）
      w.profile.assign(m, 0.0);
    }
    for (std::size_t j = 0; j < m; ++j) {
      double v = 0.0;
      if (var_plane && w.pix_index[j] >= 0 &&
          static_cast<std::size_t>(w.pix_index[j]) < var_plane->size()) {
        v = static_cast<double>((*var_plane)[static_cast<std::size_t>(w.pix_index[j])]);
      }
      if (!std::isfinite(v) || v < 0.0) v = 0.0;  // 不可用/损坏 ⇒ 0（GLS 具名拒绝）
      w.variance[j] = v;
    }
  }
  // 帧级 PSF 求积摘要（R2-F01: extrema 只作**帧摘要**; 逐控制点自己的
  // f_enclosed/f_out/求积误差已在上方各自窗口落盘, 不用单一最小值冒充逐点值）
  {
    double fmin = 1.0, fmax = 0.0, fsum = 0.0, emax = 0.0;
    int n = 0;
    bool err_known = false;
    for (const auto& d : win_diag) {
      if (std::isfinite(d.f_enclosed)) {
        fmin = std::min(fmin, d.f_enclosed);
        fmax = std::max(fmax, d.f_enclosed);
        fsum += d.f_enclosed;
        ++n;
      }
      if (d.estimated_error >= 0.0) {
        emax = std::max(emax, d.estimated_error);
        err_known = true;
      }
    }
    char sbuf[320];
    std::snprintf(sbuf, sizeof(sbuf),
                  "psf_pixel_integration frame summary: n_controls=%d "
                  "f_enclosed[min,mean,max]=[%.9f,%.9f,%.9f] "
                  "max_pixel_integration_error=%s",
                  n, fmin, n > 0 ? fsum / static_cast<double>(n) : 0.0, fmax,
                  err_known ? std::to_string(emax).c_str() : "not_available");
    out.psf_integration_summary = sbuf;
  }

  // ── GLS 完成（数学唯一实现在 noise 库）────────────────────────────────────
  acsd::normalize::SphericalCompletionConfig cfg;
  cfg.reference = ref_def;
  cfg.background = req.background_constant_fit
                       ? acsd::normalize::BackgroundNuisance::kConstantFit
                       : acsd::normalize::BackgroundNuisance::kKnown;
  cfg.gain_e_per_adu = req.gain_e_per_adu;      // **原生** ADU 面 gain（设备来源）
  cfg.master_uncertainty_modelled = req.master_uncertainty_modelled;
  cfg.template_fixed = psf_ok;  // 无 PSF ⇒ 模板不固定 ⇒ 不得称 upper_bound
  // NOISE_SNR §11.3 / PHOTOMETRY §4: applied 面源泊松 = α_j²·F_native·P_j/g。
  // 原生 gain 的 provenance 单独保留, 不被有效 gain 覆盖。
  cfg.plane_scale = req.applied_scale;
  cfg.gain_provenance = req.gain_provenance;
  // 空间 alpha 接线（NOISE_SNR §2 总通量口径, P5 空间 alpha）: req.pixel_alpha_at 是
  // 帧级求值器（输入源像元下标; m() 的唯一实现在 calibration::photo_spatial_m,
  // 由 drizzle 节点按 p1_phot.json 的 spatial_gain 场与 k_photo 组装）。GLS 侧
  // 的空间 alpha 是**逐窗口**量（PlanarPatchWindow::pixel_alpha, 与 pix_index
  // 对齐）——每个窗口在此就地求值映射, 不要求调用方预生成整帧 α 数组
  // （4096² 帧省 134 MB）。旧口径 cfg.pixel_scale 保留给单窗口调用方。
  //
  // α_*(u)（参考源位置 u=(cx_px,cy_px) 的声明标度, NOISE_SNR §2）:
  //   * 有连续求值器 alpha_at_xy ⇒ 直接在 u 求值（α 场是连续低阶多项式,
  //     无近似）;
  //   * 否则只有 pixel_alpha_at ⇒ 最近像元近似（误差 O(|m''|/8) px², 在
  //     support note 具名登记 nearest_pixel_alpha_star）;
  //   * 无空间场 ⇒ 常量 req.applied_scale（与 α_j ≡ α_* 标量模型一致）。
  //   非正/非有限 ⇒ fail_point（reference_alpha_star_invalid）, 不猜。
  const double alpha_star_const = req.applied_scale;
  // native_calibrated 声明 + 已施加测光 + 空间场: 参考源按**原生总通量**声明,
  // 在位置 u 的 applied 总通量 = α_*(u)·F_native = f_ref·(α_*/k_photo)
  // （f_ref 已由上方换算到输入面的帧级标度; NORMALIZE_DATA §5「每个控制点
  // 分别保存 F_ref」/ NOISE_SNR §2）。photo_applied 声明 ⇒ applied 面常量
  // 参考通量（另一种自洽口径, 由配置选择; 此时 reference_flux 保持帧级常量）。
  const bool per_point_reference_flux =
      req.pixel_alpha_at != nullptr && req.photometry_applied &&
      req.reference.normalization == "native_calibrated";
  auto alpha_star_at = [&](const acsd::normalize::PlanarPatchWindow& w) -> double {
    if (!req.pixel_alpha_at && !req.alpha_at_xy) return alpha_star_const;
    const double cx = w.cx_px, cy = w.cy_px;
    if (req.alpha_at_xy) {
      const double a = req.alpha_at_xy(cx, cy);
      if (std::isfinite(a) && a > 0.0) return a;
      return 0.0;   // 求值失败 ⇒ 调用方具名拒绝
    }
    // 最近像元近似（下限 0 ⇒ 调用方判非法; GLS 侧仍会复核正性）
    const long long ix = static_cast<long long>(std::floor(cx + 0.5));
    const long long iy = static_cast<long long>(std::floor(cy + 0.5));
    const long long idx = iy * static_cast<long long>(req.width) + ix;
    if (ix < 0 || iy < 0 || ix >= req.width || iy >= req.height) return 0.0;
    return req.pixel_alpha_at(idx);
  };
  bool alpha_star_ok = true;
  std::string alpha_star_reason;
  if (req.pixel_alpha_at) {
    for (auto& w : windows) {
      w.pixel_alpha.resize(w.pix_index.size());
      for (std::size_t j = 0; j < w.pix_index.size(); ++j) {
        const std::int64_t idx = w.pix_index[j];
        // 下标非法 ⇒ 0（非正 ⇒ GLS 具名拒绝 nonfinite_pixel_scale, 不猜）。
        w.pixel_alpha[j] =
            (idx >= 0 &&
             static_cast<std::int64_t>(idx) <
                 static_cast<std::int64_t>(req.width) *
                     static_cast<std::int64_t>(req.height))
                ? req.pixel_alpha_at(idx)
                : 0.0;
      }
      // α_*(u): 参考源位置声明标度（NOISE_SNR §2 总通量口径）。
      w.reference_alpha_star = alpha_star_at(w);
      if (!(w.reference_alpha_star > 0.0) || !std::isfinite(w.reference_alpha_star)) {
        alpha_star_ok = false;
        alpha_star_reason = "reference_alpha_star_invalid";
        continue;
      }
      // F_ref(u) = α_*·F_native（仅 native_calibrated + 空间场口径）;
      // f_ref/k_photo = F_native（k_photo = req.applied_scale）。
      if (per_point_reference_flux && ref_def.flux.has_value() &&
          std::isfinite(req.applied_scale) && req.applied_scale > 0.0) {
        const double f_native = *ref_def.flux / req.applied_scale;
        w.reference_flux = w.reference_alpha_star * f_native;
      }
    }
    if (!alpha_star_ok) {
      out.error = alpha_star_reason;
      return out;
    }
  }
  const auto comp = acsd::normalize::complete_spherical_reference(windows, cfg);

  doc_template.precision = comp.precision;
  doc_template.precision.header.frame_id = doc_template.frame_id;
  doc_template.precision.header.producer =
      "acsd.phase1.noise_snr.complete_spherical_reference";
  if (!psf_ok &&
      doc_template.precision.status.availability ==
          acsd::normalize::PrecisionAvailability::kAvailable) {
    // 无 PSF ⇒ 模板缺项 ⇒ 至多 approximate（不得称 conditional_complete/upper_bound）
    doc_template.precision.status.accuracy =
        acsd::normalize::PrecisionAccuracy::kApproximate;
    doc_template.precision.status.omitted_terms.push_back("psf_model");
  }

  // ── 每控制点局部模型 payload（§7.1 四类; 序列化唯一在 normalize_science）──
  if (model_payloads) {
    for (std::size_t wi = 0; wi < comp.windows.size(); ++wi) {
      const auto& lw = comp.windows[wi];
      // comp.windows 只含 available 控制; 用其 window_index 找回**输入**窗口
      // （否则不可用控制点会让两侧下标错位 ⇒ 越界访问）。
      if (lw.window_index >= windows.size()) break;
      const std::size_t i = lw.window_index;
      const auto& ctrl = comp.precision.controls[i];
      if (ctrl.status.availability !=
          acsd::normalize::PrecisionAvailability::kAvailable) {
        continue;
      }
      const int m = static_cast<int>(lw.pix_index.size());
      const int nl = static_cast<int>(windows[i].leaf_ipix.size());
      auto base = [&]() {
        acsd::normalize::LocalModelPayload q;
        q.control_id = lw.control_id;
        q.domain = acsd::normalize::LocalModelDomain::kDrizzledSphere;
        q.source_pixel_index = lw.pix_index;
        q.leaf_ipix = windows[i].leaf_ipix;
        return q;
      };
      {  // psf: h = R·(α/α_*)·P（长度 nl; 单位 1/sr）
        auto q = base();
        q.kind = acsd::normalize::LocalModelKind::kPsf;
        q.unit = "sr^-1";
        q.values = lw.h_sphere;
        q.matrix_rows = nl;
        q.matrix_cols = 1;
        // 空间标度合同（NOISE_SNR §2 总通量口径）: 平面模板
        // h_j = (α_j/α_*)·P_j 与源泊松 Var_src,j = F_ref·α_j²·P_j/(g·α_*)
        // 共用同一 α 场。逐像元 α_j、参考位置标度 α_* 与平面模板随 h 落盘,
        // 使独立读者可复核 h_sphere = R·planar_template 且 α_j = α(u_j)、
        // α_* = α(u_c) 与 photometry 的 spatial_gain 记录一致。
        q.source_pixel_alpha = lw.source_pixel_alpha;
        q.reference_alpha_star = lw.reference_alpha_star;
        q.planar_template = lw.planar_template;
        model_payloads->push_back(q);
      }
      {  // covariance: 精确因子 v_j + CSR(R) ⇒ C=R diag(v)ᵀ（local-factor/v2）
        auto q = base();
        q.kind = acsd::normalize::LocalModelKind::kCovariance;
        // 单位分工（NORMALIZE_DATA §7.1 裁决, 三者不可共用一个 BUNIT）:
        //   factor v_j            : [线性被估计量]^2（即 reference flux unit^2）
        //   算子 R                : sr^-1
        //   派生 C = R diag(v) Rᵀ : [线性被估计量]^2 / sr^2
        q.factor_variance_unit = ref_def.flux_unit.empty() ? std::string("adu^2")
                                                            : ref_def.flux_unit + "^2";
        q.derived_covariance_unit =
            (ref_def.flux_unit.empty() ? std::string("adu^2")
                                       : ref_def.flux_unit + "^2") + "/sr^2";
        q.operator_unit = "sr^-1";
        q.covariance_condition = "reference_source";
        q.background_basis_unit = "1/sr";   // Z_s = R Z_planar; β 与 Z 成对
        q.background_basis_definition =
            lw.n_bg == 0 ? "known_background"
                         : (lw.n_bg == 1 ? "constant_fit_unit_column"
                                         : "plane_fit_unit_x_y_columns");
        q.assumptions = {"diagonal_planar_condition"};
        q.operator_rows = nl;
        q.operator_cols = m;
        q.logical_covariance_rows = nl;
        q.logical_covariance_cols = nl;
        // 仅当模型内部精确（planar diagonal 且所有 v_j>0）才可声明;
        // 未建模 master/scale 相关项仍在 omitted_terms, 不因此变成"全协方差已建模"。
        q.exact_factorization_of_model = true;
        q.solver = lw.solver;
        q.solver_relative_tol = lw.solver_relative_tol;
        q.solver_rank = lw.solver_rank;
        q.condition_estimate_method = lw.solver_condition_estimate_method;
        q.solver_condition_number = lw.solver_condition_number;
        q.background_basis_rank = lw.background_basis_rank;
        q.flux_information = lw.flux_information;
        q.flux_response_norm = lw.flux_response_norm;
        q.flux_residual_norm = lw.flux_residual_norm;
        q.unit = q.derived_covariance_unit;   // 顶层 unit = 派生 C 的单位
        if (lw.covariance_explicit) {
          q.values = lw.covariance;                 // 显式 nl×nl row-major
          q.covariance = lw.covariance;             // 供读侧重算对拍
          q.matrix_rows = nl;
          q.matrix_cols = nl;
        } else {
          // 落盘预算之外的显式矩阵: 只写精确因子（v_j, 长度 m）。C = R diag(v) Rᵀ
          // 可由本文件的 v_j 与同控制点的 drizzle_operator CSR **精确**重算, 不是
          // 近似。representation 必须是真实写出的 local-factor/v2（R2-F02 审查修正:
          // 此前 note 写 v1 而 writer 发布 v2, 同一模型两个版本声明互相矛盾）;
          // v1 只保留给历史兼容的 reader 适配, 不作为新生产语义。
          q.values = lw.source_variance;
          q.matrix_rows = m;
          q.matrix_cols = 1;
          q.support_note =
              "representation=acsd.normalize.local-factor/v2; explicit nl x nl "
              "matrix exceeds covariance_matrix_budget (" +
              std::to_string(cfg.covariance_matrix_budget) + "); C = R diag(v) R^T "
              "with v = values[] and R = the drizzle_operator CSR of the same "
              "control; nl=" + std::to_string(nl) + " m=" + std::to_string(m) +
              "; factor_variance_unit=" + q.factor_variance_unit +
              "; derived_covariance_unit=" + q.derived_covariance_unit +
              "; operator_unit=sr^-1";
        }
        q.covariance_rank = ctrl.covariance_rank;
        q.condition_number = ctrl.condition_number;
        q.background_basis = lw.z_sphere;
        q.n_bg = lw.n_bg;
        q.random_components = lw.random_components;
        q.omitted_terms = lw.omitted_terms;
        model_payloads->push_back(q);
      }
      {  // drizzle_operator: R 的 CSR（行=叶, 列=像元, 系数=c_lj; shape=nl×m）
        auto q = base();
        q.kind = acsd::normalize::LocalModelKind::kDrizzleOperator;
        q.unit = "sr^-1";
        // 按叶分组的 CSR: 行 = 叶槽位, 列 = 像元槽位, 系数 = c_lj = w_lj/N_l
        std::vector<std::vector<std::pair<int, double>>> rows(
            static_cast<std::size_t>(nl));
        for (std::size_t k = 0; k < lw.op_nnz; ++k) {
          rows[static_cast<std::size_t>(lw.op_leaf[k])].emplace_back(
              lw.op_pix[k], lw.op_c[k]);
        }
        q.csr_row_offsets.assign(static_cast<std::size_t>(nl) + 1, 0);
        for (int l = 0; l < nl; ++l) {
          for (const auto& e : rows[static_cast<std::size_t>(l)]) {
            q.csr_column_indices.push_back(e.first);
            q.csr_coefficients.push_back(e.second);
          }
          q.csr_row_offsets[static_cast<std::size_t>(l) + 1] =
              static_cast<std::int64_t>(q.csr_coefficients.size());
        }
        q.source_omega = lw.pix_omega;
        q.source_a_drop = lw.pix_a_drop;
        // 每叶完整 N_l（同次生产叶的完整有效样本; 使 R 的归一可被独立复核）
        q.source_n_leaf = windows[i].leaf_n_full;
        q.matrix_rows = nl;
        q.matrix_cols = m;
        model_payloads->push_back(q);
      }
      {  // support: 每叶 D_l（真实 drop 命中面积; 不重复计面积的上界 = Ω_leaf）
        auto q = base();
        q.kind = acsd::normalize::LocalModelKind::kSupport;
        q.unit = "sr";
        q.values = lw.leaf_dp;
        // PSF.md §4 要求的落盘项: flux_estimator、psf_family/参数/子像元相位、
        // pixel_integration_method/order/估计误差、normalization_basis、
        // f_enclosed/f_out。f_enc/f_out 是**本控制点自己**的实际像元集合求和
        // （R2-F01 审查修正: 帧级 extrema 只作摘要, 不冒充逐点实测）; 理想圆
        // 闭式值单独作为参考求积并列写出, 不替代真实 f_out（PSF.md §4 明示）。
        const auto& pd = win_diag[static_cast<std::size_t>(i)];
        // 逐控制点 PSF 模型身份（PSF.md §4）: 空间三角重心凸组合 vs 帧常 σ
        // 具名近似（含回退原因, 不把近似冒充逐位置模型）。
        const std::string& psf_model =
            (win_psf_model[i].empty() ? "unavailable" : win_psf_model[i]);
        // 空间定位明细只在该控制点真的用了空间模型时落盘（回退点不写
        // 误导性的三角/权重字段; star_ids 是顶点下标, 回退时无意义）。
        std::string spatial_extra;
        if (win_psf_model[i] == "spatial_delaunay_barycentric") {
          char sb[256];
          std::snprintf(sb, sizeof(sb),
                        " spatial_triangle_stars=(%llu,%llu,%llu);"
                        " spatial_lambda=(%.4f,%.4f,%.4f);"
                        " spatial_vertex_sigma_px=[%.4f,%.4f];",
                        static_cast<unsigned long long>(
                            win_spatial[i].star_ids[0]),
                        static_cast<unsigned long long>(
                            win_spatial[i].star_ids[1]),
                        static_cast<unsigned long long>(
                            win_spatial[i].star_ids[2]),
                        win_spatial[i].lambda[0], win_spatial[i].lambda[1],
                        win_spatial[i].lambda[2],
                        win_spatial[i].min_vertex_sigma,
                        win_spatial[i].max_vertex_sigma);
          spatial_extra = sb;
        }
        char note[1280];
        std::snprintf(note, sizeof(note),
                      "patch-center deterministic grid; leaf_order=%d;"
                      " flux_estimator=%s; psf_family=moffat4;"
                      " psf_model=%s;"
                      " psf_sigma_px=%.6f; subpixel_phase=(%.6f,%.6f);"
                      " pixel_integration_method=gauss_legendre_tensor;"
                      " pixel_integration_order=10;"
                      " pixel_integration_error=%s;"
                      " normalization_basis=analytic_total;"
                      " f_enclosed=%.9f; f_out=%.9f;"
                      " ideal_disc_f_out_ref=%.6e;"
                      "%s"
                      " operator_coverage_of_N_p=%.6f;"
                      " alpha_convention=applied_total_flux_at_u;"
                      " reference_alpha_star=%.9g;"
                      " alpha_window_scale=%.9g;"
                      " per_point_reference_flux=%s",
                      windows[i].leaf_order,
                      req.have_psf ? flux_estimator.c_str() : "unavailable",
                      psf_model.c_str(),
                      req.psf_sigma_px, windows[i].cx_px, windows[i].cy_px,
                      pd.estimated_error >= 0.0 ? "reported_below"
                                                 : "not_available",
                      std::isfinite(pd.f_enclosed) ? pd.f_enclosed : 0.0,
                      std::isfinite(pd.f_enclosed) ? 1.0 - pd.f_enclosed : 0.0,
                      (req.have_psf && req.psf_sigma_px > 0.0
                           ? std::pow(1.0 + template_radius * template_radius /
                                                (2.0 * req.psf_sigma_px *
                                                 req.psf_sigma_px),
                                      -3.0)
                           : 0.0),
                      spatial_extra.c_str(),
                      lw.operator_coverage_fraction,
                      lw.reference_alpha_star,
                      lw.reference_window_scale,
                      (lw.reference_alpha_star > 0.0 &&
                       windows[i].reference_flux > 0.0)
                          ? "true"
                          : "false");
        // 求积误差单独一个可机读字段（不进自由文本 note）; 负值 = 未能估计
        q.pixel_integration_error = pd.estimated_error;
        q.support_note = note;
        q.matrix_rows = nl;
        q.matrix_cols = 1;
        model_payloads->push_back(q);
      }
      // sidecar models[] 登记（sha256 由发布方写实际文件后回填）
      auto reg = [&](acsd::normalize::LocalModelKind kind) {
        acsd::normalize::ModelFileRef ref;
        ref.model_id = lw.control_id + "." + acsd::normalize::to_string(kind);
        ref.kind = kind;
        ref.domain = acsd::normalize::LocalModelDomain::kDrizzledSphere;
        // representation 必须与**实际写出的模型文件**一致（NORMALIZE_DATA §7.1:
        // 读者按 version 判定语义, 不猜）。covariance 在预算外写 local-factor/v2
        //（v_j + CSR ⇒ C=R diag(v)Rᵀ, 单位/shape 分列声明）; 预算内写显式
        // local-model/v1。其余 kind 恒为 local-model/v1。
        ref.representation =
            (kind == acsd::normalize::LocalModelKind::kCovariance &&
             !lw.covariance_explicit)
                ? "acsd.normalize.local-factor/v2"
                : "acsd.normalize.local-model/v1";
        ref.path = "models/" + lw.control_id + "." +
                   acsd::normalize::to_string(kind) + ".json";
        ref.sha256.clear();
        ref.status.availability = acsd::normalize::PrecisionAvailability::kAvailable;
        ref.status.accuracy = ctrl.status.accuracy;
        ref.status.omitted_terms = ctrl.status.omitted_terms;
        doc_template.models.push_back(ref);
      };
      reg(acsd::normalize::LocalModelKind::kPsf);
      reg(acsd::normalize::LocalModelKind::kCovariance);
      reg(acsd::normalize::LocalModelKind::kDrizzleOperator);
      reg(acsd::normalize::LocalModelKind::kSupport);
    }
  }

  // 控制点模型引用回填（sidecar schema 要求 string; 引用不悬空 —— 模型文件按
  // control_id 命名, 由发布方写实际文件并复核 hash）
  for (auto& c : doc_template.precision.controls) {
    if (c.control_id.empty()) continue;
    if (c.support_id.empty()) c.support_id = c.control_id + ".support";
    if (c.psf_model_id.empty()) c.psf_model_id = c.control_id + ".psf";
    if (c.covariance_model_id.empty())
      c.covariance_model_id = c.control_id + ".covariance";
  }
  // 帧摘要（合同 §5 唯一定义: 支持面积加权 log SNR 中位数取 exp）
  doc_template.precision.frame_summary =
      acsd::normalize::summarize_frame_reference(comp.precision.controls);
  if (doc_template.precision.status.availability ==
          acsd::normalize::PrecisionAvailability::kAvailable &&
      doc_template.precision.support_model_id.empty()) {
    doc_template.precision.support_model_id = frame_key + ".support-set";
  }

  for (const auto& c : comp.precision.controls) {
    if (c.status.availability ==
        acsd::normalize::PrecisionAvailability::kAvailable) {
      ++out.n_available_controls;
    }
  }
  out.document = doc_template;
  out.ok = true;
  return out;
}

}  // namespace acsd::phase1::precision
