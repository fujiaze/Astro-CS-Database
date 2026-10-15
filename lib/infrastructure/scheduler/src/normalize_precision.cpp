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
#include "snr_estimator.h"            // snr_moffat4_profile_f64（归一化/ΣP² 统计）

namespace acsd::phase1::precision {

using acsd::normalize::ReferenceDefinition;
using acsd::normalize::ReferenceKind;
using acsd::normalize::PlanarPatchWindow;
namespace {

constexpr double kPi = 3.14159265358979323846;

// Moffat4 β=4 离散轮廓（PSF.md §2.1/§2.3 闭式）: I(r)=1/(1+r²/(2σ²))⁴,
// P_i = I_i/Σ_j I_j（严格 ΣP=1）。ΣP² 与 snr_moffat4_profile_f64 对拍校验
//（同一 closed form 的库统计; 不一致 ⇒ 数据错误, 不静默）。
bool moffat4_profile(double sigma_px, double cx, double cy,
                     const std::vector<std::int64_t>& pix, int width,
                     std::vector<double>* out_p, double* out_sum_p2) {
  if (!(sigma_px > 0.0) || !std::isfinite(sigma_px)) return false;
  const double alpha2 = 2.0 * sigma_px * sigma_px;
  double sum = 0.0;
  std::vector<double> vals(pix.size(), 0.0);
  for (std::size_t k = 0; k < pix.size(); ++k) {
    const std::int64_t idx = pix[k];
    const double px = static_cast<double>(idx % width);
    const double py = static_cast<double>(idx / width);
    const double dx = px - cx;
    const double dy = py - cy;
    const double t = 1.0 + (dx * dx + dy * dy) / alpha2;
    const double t2 = t * t;
    const double v = 1.0 / (t2 * t2);
    vals[k] = v;
    sum += v;
  }
  if (!(sum > 0.0)) return false;
  // **不随观测域重归一**（NOISE_SNR §11.2「不能随窗口缩小重新归一 P 并仍沿用总
  // 通量 F_ref」）: out_p 保持单位总通量的像元积分响应, ΣP = 观测域包络的通量份额
  // ≤ 1; 窗外份额由 f_out 闭式在 support 模型登记。
  for (std::size_t k = 0; k < pix.size(); ++k) (*out_p)[k] = vals[k];
  *out_sum_p2 = sum;   // ΣP（包络份额）, 非 ΣP²
  return true;
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
    // unavailable（不伪造绝对零点）。
    def.reason = "sed_response requires evaluable response integration "
                 "(SED curve + zero point); not wired in this build ⇒ "
                 "precision unavailable, signal unaffected";
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
  // 记录实际使用的模板足迹半径（support 模型据它估算窗外通量份额 f_out; 不影响
  // R 与方差域 —— 观测域由归属叶的完整贡献决定, 见下）。
  double& radius_for_note = const_cast<double&>(req.window_radius_px);
  radius_for_note = precision_window_radius_px(req);
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

  // ── 每窗口: 平面模板 P（Moffat4 闭式, 库统计对拍）+ 条件方差 v_j ──────────
  std::vector<acsd::normalize::PlanarPatchWindow> windows = windows_in;
  bool psf_ok = req.have_psf;
  double sum_p2_lib = 0.0;
  if (psf_ok) {
    // noise 库对同一 closed form 的归一化 ΣP²（半平面解析值）。
    snr_moffat4_profile_f64(/*fwhm_px=*/0.0, req.psf_sigma_px, /*half_px=*/0,
                            &sum_p2_lib, nullptr);
  }
  double profile_enclosed = 1.0;   // ΣP（观测域包络的点源通量份额; ≤1）
  for (auto& w : windows) {
    const std::size_t m = w.pix_index.size();
    if (m == 0) continue;
    if (psf_ok) {
      double enclosed = 0.0;
      if (!moffat4_profile(req.psf_sigma_px, w.cx_px, w.cy_px, w.pix_index,
                           req.width, &w.profile, &enclosed)) {
        psf_ok = false;
        break;
      }
      // 同一 closed form 的形状统计对拍: 把本窗口的归一化 ΣP² 与库的半平面解析值
      // 比较。覆盖不足时归一化 ΣP² 只会**偏大**（形状更集中）⇒ 要求不小于解析值
      // 减去容差; 偏小说明模板与库的 closed form 不一致 ⇒ 数据错误, 显式失败。
      double np2 = 0.0;
      for (double pv : w.profile) np2 += pv * pv;
      const double enclosed2 = enclosed * enclosed;
      const double norm_np2 = enclosed2 > 0.0 ? np2 / enclosed2 : 0.0;
      if (sum_p2_lib > 0.0 && m > 64 &&
          norm_np2 + 0.01 * sum_p2_lib < sum_p2_lib) {
        out.error = "moffat4 profile statistics mismatch vs noise library (sigma=" +
                    std::to_string(req.psf_sigma_px) + ")";
        return out;
      }
      if (std::isfinite(enclosed) && enclosed > 0.0) {
        profile_enclosed = std::min(profile_enclosed, enclosed);
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
  cfg.pixel_scale = req.pixel_scale;
  cfg.gain_provenance = req.gain_provenance;
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
      {  // psf: h = R P（长度 nl; 单位 1/sr）
        auto q = base();
        q.kind = acsd::normalize::LocalModelKind::kPsf;
        q.unit = "sr^-1";
        q.values = lw.h_sphere;
        q.matrix_rows = nl;
        q.matrix_cols = 1;
        model_payloads->push_back(q);
      }
      {  // covariance: C_sphere（nl×nl row-major; unit=linear_unit²/sr²）+ Z + 秩
        auto q = base();
        q.kind = acsd::normalize::LocalModelKind::kCovariance;
        q.unit = ref_def.flux_unit.empty() ? std::string("adu^2/sr^2")
                                           : ref_def.flux_unit + "^2/sr^2";
        if (lw.covariance_explicit) {
          q.values = lw.covariance;                 // 显式 nl×nl row-major
          q.matrix_rows = nl;
          q.matrix_cols = nl;
        } else {
          // 落盘预算之外的显式矩阵: 只写精确因子（v_j, 长度 m）并把
          // representation 记为 local-factor/v1。C = R diag(v) Rᵀ 可由本文件的
          // v_j 与同控制点的 drizzle_operator CSR **精确**重算, 不是近似; 但
          // 不在"显式矩阵"标签下塞因子（合同 §7.1）。nl 与理由写入 support_note。
          q.values = lw.source_variance;
          q.matrix_rows = m;
          q.matrix_cols = 1;
          q.support_note =
              "representation=acsd.normalize.local-factor/v1; explicit nl x nl "
              "matrix exceeds covariance_matrix_budget (" +
              std::to_string(cfg.covariance_matrix_budget) + "); C = R diag(v) R^T "
              "with v = values[] and R = the drizzle_operator CSR of the same "
              "control; nl=" + std::to_string(nl) + " m=" + std::to_string(m);
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
        // 模板截断份额（各向同性 Moffat4 闭式 f_out=(1+r²/(2σ²))^{-3}）: 观测域
        // 包络之外的点源通量份额; P **不**随域重归一 ⇒ ΣP = 包络份额, F_ref 仍是
        // 总通量口径（NOISE_SNR §11.2）。
        char note[512];
        std::snprintf(note, sizeof(note),
                      "patch-center deterministic grid; leaf_order=%d; template"
                      " footprint radius=%.3f px; template truncation f_out=%.3e;"
                      " enclosed profile sum=%.6f; operator coverage of N_p=%.6f",
                      windows[i].leaf_order, req.window_radius_px,
                      req.have_psf && req.psf_sigma_px > 0.0
                          ? std::pow(1.0 + req.window_radius_px *
                                              req.window_radius_px /
                                              (2.0 * req.psf_sigma_px *
                                               req.psf_sigma_px),
                                      -3.0)
                          : 0.0,
                      req.have_psf && req.psf_sigma_px > 0.0 ? profile_enclosed
                                                             : 0.0,
                      lw.operator_coverage_fraction);
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
        // covariance 在预算外改用版本化因子表示; 其余 kind 恒为显式表示。
        ref.representation =
            (kind == acsd::normalize::LocalModelKind::kCovariance &&
             !lw.covariance_explicit)
                ? "acsd.normalize.local-factor/v1"
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
