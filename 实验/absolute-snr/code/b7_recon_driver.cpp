/* b7_recon_driver.cpp — SCI-RECON-SNR-01 生产链路驱动（只读链接生产源，不改一行）
 *
 * 目的：把「逐像素绝对 SNR 重建」的四个分量面**全部由本仓生产函数在进程内算出**，
 *       交给 Python 侧只做代数组合与判据评估，避免在 Python 里另写一套模型。
 *
 * 直调的生产函数（逐个标注权威落点）：
 *   1. snr_noise_model_v1_f64 / _fill / _free            lib/algorithms/noise_snr/cpp/src/noise_model.cpp
 *      —— 背景方差面（NOISE_MODEL §5）：source-masked blank-sky 稳健方差 + 稀疏控制点 + 平面场
 *   2. snr_noise_gain_variance                           同文件
 *      —— 加权方差面的源项（NOISE_MODEL §5c）：var = max(signal,0)/gain + (rn/gain)^2
 *   3. snr_moffat4_profile_f64                            lib/algorithms/noise_snr/cpp/src/snr_science.cpp
 *      —— 离散归一化 Moffat4 轮廓统计（oracle 锚，用于校验本驱动的轮廓复现）
 *   4. snr_source_snr_f64                                同文件
 *      —— 逐源科学 SNR（Horne 1986 最优提取）
 *   5. astrocs::v6::p1psfw::w_info_solve                 lib/algorithms/noise_snr/cpp/src/information_weight.cpp
 *      —— FZ-FORMULA-WINFO/Q：W=a^2 P^T C^-1 P、Q=a P^T C^-1 d、F_hat=Q/W
 *   6. astrocs::v6::p2weight::SparseSnrReconstructor      lib/algorithms/integration/v6/src/weight_chain.cpp
 *      —— sparse_snr_layer 冻结重建算子（bilinear_regular_grid_v1 / natural_bicubic_spline_clip_v1）
 *   7. p2_star_mask_caps / p2_star_mask_contains /
 *      p2_sky_patch_estimate / p2_sky_plane_build /
 *      p2_sky_plane_eval_block / p2_sky_plane_info /
 *      p2_reject_plan_resolve / p2_reject_stack / p2_rejection_semantic_id
 *                                                       lib/algorithms/coverage/src/{sky_plane,rejection}.cpp
 *      —— 天光面（加性天光稀疏样条表示）与排异积分层次
 *
 * 编译：code/b7_build_driver.sh（产物落 run/SCI-RECON-SNR-01/bin/，不入库）
 *
 * 输入协议（文本头 + 定序 float64 原始块；全部 little-endian）：
 *   头：每行 "<key> <value>"，'#' 起注释；需要 h w gain rn_e dark_e n_frames
 *       pix_scale_deg node_spacing_deg sky_box_px psf_sigma_px profile_half
 *       use_star_prior n_stars
 *   块（顺序固定，float64）：
 *       data[(n_frames) x h x w]     第 0 帧 = 科学帧
 *       stars[n_stars x 4]           x, y, flux_adu, fwhm_px
 *       src_true[h x w]              （仅用于对照，不进入估计）
 *       var_slow_true[h x w]         （仅用于对照，不进入估计）
 *
 * 输出协议（文本头 + 定序 float64 原始块）：见 write_output()
 */
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <map>
#include <string>
#include <vector>

#include "snr_estimator.h"
#include "star_detector.h"
#include "astrocs/v6/information_weight.h"
#include "astrocs/v6/weight_chain.h"
#include "astro/phase2/sky_plane.h"
#include "astro/phase2/rejection.h"

namespace {

constexpr double kGaussFwhmFactor = 2.3548200450309493;  /* snr_science.cpp:50-54 */
constexpr double kMoffat4FwhmFactor = 1.230310;

struct Header {
  std::map<std::string, double> kv;
  double get(const std::string& k, double d = 0.0) const {
    auto it = kv.find(k);
    return it == kv.end() ? d : it->second;
  }
};

bool read_header(std::istream& in, Header* h) {
  std::string line;
  while (std::getline(in, line)) {
    const std::size_t hash = line.find('#');
    if (hash != std::string::npos) line = line.substr(0, hash);
    while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) line.pop_back();
    if (line.empty()) continue;
    if (line == "BEGIN_DATA") return true;
    std::size_t sp = line.find(' ');
    if (sp == std::string::npos) continue;
    h->kv[line.substr(0, sp)] = std::atof(line.c_str() + sp + 1);
  }
  return false;
}

/* 逐位复现 snr_science.cpp moffat4Discrete（:70-94）：P_i = t^-4 / sum_j t_j^-4，
 * 网格 half 由调用方给定。等价性由 b7_profile_selfcheck 对 snr_moffat4_profile_f64
 * 逐位核对（容差 1e-15）。 */
void moffat4_grid(double sigma_px, int half, std::vector<double>* out_P,
                  double* out_sum_p2, double* out_p_center) {
  const double alpha2 = 2.0 * sigma_px * sigma_px;
  const int n = 2 * half + 1;
  out_P->assign(static_cast<std::size_t>(n) * n, 0.0);
  double sum = 0.0, sum2 = 0.0, center = 0.0;
  for (int j = -half; j <= half; ++j) {
    for (int i = -half; i <= half; ++i) {
      const double r2 = static_cast<double>(i) * i + static_cast<double>(j) * j;
      const double t = 1.0 + r2 / alpha2;
      const double t2 = t * t;
      const double v = 1.0 / (t2 * t2);
      (*out_P)[static_cast<std::size_t>(j + half) * n + (i + half)] = v;
      sum += v;
      sum2 += v * v;
      if (i == 0 && j == 0) center = v;
    }
  }
  for (std::size_t k = 0; k < out_P->size(); ++k) (*out_P)[k] /= sum;
  if (out_sum_p2) *out_sum_p2 = sum2 / (sum * sum);
  if (out_p_center) *out_p_center = center / sum;
}

int auto_half(double fwhm_eff) {
  int h = static_cast<int>(std::ceil(12.0 * fwhm_eff));
  if (h < 30) h = 30;
  if (h > 256) h = 256;
  return h;
}

template <typename T>
void write_block(std::ostream& os, const std::vector<T>& v) {
  os.write(reinterpret_cast<const char*>(v.data()),
           static_cast<std::streamsize>(v.size() * sizeof(T)));
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 3) {
    std::fprintf(stderr, "usage: %s <in.bin> <out.bin>\n", argv[0]);
    return 2;
  }
  std::ifstream in(argv[1], std::ios::binary);
  if (!in) { std::fprintf(stderr, "cannot open %s\n", argv[1]); return 2; }

  Header hdr;
  if (!read_header(in, &hdr)) { std::fprintf(stderr, "bad header\n"); return 2; }

  const int H = static_cast<int>(hdr.get("h"));
  const int W = static_cast<int>(hdr.get("w"));
  const int n_frames = static_cast<int>(hdr.get("n_frames", 1));
  const int n_stars = static_cast<int>(hdr.get("n_stars"));
  const double gain = hdr.get("gain");
  const double rn_e = hdr.get("rn_e");
  const double dark_e = hdr.get("dark_e");
  const double saturation = hdr.get("saturation");
  const double pix_scale_deg = hdr.get("pix_scale_deg", 1.0 / 512.0);
  const double node_spacing_deg = hdr.get("node_spacing_deg", 0.125);
  const int sky_box = static_cast<int>(hdr.get("sky_box_px", 16));
  const double psf_sigma_px = hdr.get("psf_sigma_px", 1.5);
  const int profile_half = static_cast<int>(hdr.get("profile_half", 0));
  const int use_star_prior = static_cast<int>(hdr.get("use_star_prior", 1));
  const std::size_t npix = static_cast<std::size_t>(H) * W;

  std::vector<double> data(static_cast<std::size_t>(n_frames) * npix);
  in.read(reinterpret_cast<char*>(data.data()),
          static_cast<std::streamsize>(data.size() * sizeof(double)));
  std::vector<double> stars(static_cast<std::size_t>(n_stars) * 4);
  if (n_stars > 0) {
    in.read(reinterpret_cast<char*>(stars.data()),
            static_cast<std::streamsize>(stars.size() * sizeof(double)));
  }
  std::vector<double> src_true(npix), var_slow_true(npix);
  in.read(reinterpret_cast<char*>(src_true.data()),
          static_cast<std::streamsize>(npix * sizeof(double)));
  in.read(reinterpret_cast<char*>(var_slow_true.data()),
          static_cast<std::streamsize>(npix * sizeof(double)));
  if (!in) { std::fprintf(stderr, "short read\n"); return 2; }

  const double* frame0 = data.data();

  /* ---------------- 0a. 生产星点检测（sdet，诊断/初值路径） ----------------
   * 头键 use_sdet=1 时用生产检测结果**替换**入参星表的位置与 FWHM（通量仍由
   * 生产 GLS w_info_solve 在帧上重估）；=0 时直接用入参星表。
   * 依据：star_detector.h:69-76（全图盲检测为诊断/初值路径，星表引导为权威路径；
   * 本实验无星表，故走盲检测并显式登记）。 */
  int n_stars_eff = n_stars;
  std::vector<double> st_x(static_cast<std::size_t>(std::max(n_stars, 1)));
  std::vector<double> st_y(st_x.size()), st_flux(st_x.size()), st_fwhm(st_x.size());
  for (int i = 0; i < n_stars; ++i) {
    st_x[i] = stars[4 * i + 0];
    st_y[i] = stars[4 * i + 1];
    st_flux[i] = stars[4 * i + 2];
    st_fwhm[i] = stars[4 * i + 3];
  }
  int sdet_rc = -1, sdet_count = 0;
  if (static_cast<int>(hdr.get("use_sdet", 0)) == 1) {
    SDetParams sp;
    std::memset(&sp, 0, sizeof(sp));
    sp.structureLayers = 5;
    sp.hotPixelFilterRadius = 1;
    sp.iterativeClipSigma = 9.0f;
    sp.iterativeMaxRounds = 5;
    sp.medianFilterDetail = 1;
    sp.maxStars = 2000;
    sp.fitRadius = 0;
    sp.fwhmClipSigma = 3.0f;
    sp.maxAxisRatio = 2.0f;
    StarDetectorHandle sd = sdet_create(&sp);
    if (sd) {
      double* ox = nullptr; double* oy = nullptr; float* oflux = nullptr;
      int* osat = nullptr; float* omag = nullptr; int* ohs = nullptr;
      const char* extras[2] = {"fwhm_x", "fwhm_y"};
      float** ex = nullptr;
      sdet_rc = sdet_detect_ex_f64(sd, frame0, W, H, &ox, &oy, &oflux, &osat,
                                   &omag, &ohs, &sdet_count, extras, 2, &ex);
      if (sdet_rc == 0 && sdet_count > 0) {
        st_x.assign(sdet_count, 0.0);
        st_y.assign(sdet_count, 0.0);
        st_flux.assign(sdet_count, 0.0);
        st_fwhm.assign(sdet_count, 0.0);
        for (int i = 0; i < sdet_count; ++i) {
          st_x[i] = ox[i];
          st_y[i] = oy[i];
          st_flux[i] = (oflux ? static_cast<double>(oflux[i]) : 0.0);
          double fx = (ex && ex[0]) ? static_cast<double>(ex[0][i]) : 0.0;
          double fy = (ex && ex[1]) ? static_cast<double>(ex[1][i]) : 0.0;
          double fw = 0.5 * (fx + fy);
          if (!(fw > 0.0)) fw = psf_sigma_px * kGaussFwhmFactor;
          st_fwhm[i] = fw;
        }
        n_stars_eff = sdet_count;
      }
      sdet_free_detect_ex(ox, oy, oflux, osat, omag, ohs, ex, 2);
      sdet_destroy(sd);
    }
  }

  /* ---------------- 0. 轮廓自检：本驱动复现 vs 生产 oracle ---------------- */
  double sc_sum_p2 = 0.0, sc_p_center = 0.0, drv_sum_p2 = 0.0, drv_p_center = 0.0;
  {
    const int hh = (profile_half > 0) ? profile_half : auto_half(psf_sigma_px * kMoffat4FwhmFactor);
    snr_moffat4_profile_f64(0.0, psf_sigma_px, hh, &sc_sum_p2, &sc_p_center);
    std::vector<double> P;
    moffat4_grid(psf_sigma_px, hh, &P, &drv_sum_p2, &drv_p_center);
  }
  const double profile_selfcheck = std::max(
      std::fabs(drv_sum_p2 - sc_sum_p2) / std::max(sc_sum_p2, 1e-300),
      std::fabs(drv_p_center - sc_p_center) / std::max(sc_p_center, 1e-300));

  /* ---------------- 1. 背景方差面（生产噪声模型） ---------------- */
  const int n_stars_use = n_stars_eff;
  std::vector<double> sx(st_x), sy(st_y), sflux(st_flux), sfwhm(st_fwhm);

  SnrNoiseModelConfig cfg;
  std::memset(&cfg, 0, sizeof(cfg));
  snr_noise_model_v1_default_config(&cfg);
  cfg.gain_e_per_adu = gain;
  cfg.read_noise_e = rn_e;
  cfg.saturation_level = saturation;
  cfg.enable_spatial_field = 1;
  snr_noise_model_v1_abi_stamp_config(&cfg);

  NoiseWeightModelV1 model;
  std::memset(&model, 0, sizeof(model));
  const double* sp_f = use_star_prior ? sflux.data() : nullptr;
  const double* sp_w = use_star_prior ? sfwhm.data() : nullptr;
  const int rc_model = snr_noise_model_v1_f64(
      frame0, H, W, nullptr, sx.data(), sy.data(), sp_f, sp_w, n_stars_use,
      &cfg, &model);

  std::vector<float> var_plane(npix, 0.0f), ivar_plane(npix, 0.0f);
  int rc_fill = -999;
  if (rc_model == 0) {
    rc_fill = snr_noise_model_v1_fill(&model, H, W, var_plane.data(), ivar_plane.data());
  }

  /* 控制点 → 逐像素重建（生产 sparse_snr_layer 重建算子） */
  const int n_ctrl = (rc_model == 0) ? static_cast<int>(model.n_control_points) : 0;
  std::vector<double> ctrl_x(std::max(n_ctrl, 1)), ctrl_y(std::max(n_ctrl, 1));
  std::vector<double> ctrl_var(std::max(n_ctrl, 1)), ctrl_sigma(std::max(n_ctrl, 1));
  for (int i = 0; i < n_ctrl; ++i) {
    ctrl_x[i] = model.ctrl_x_px[i];
    ctrl_y[i] = model.ctrl_y_px[i];
    ctrl_var[i] = model.ctrl_variance[i];
    ctrl_sigma[i] = model.ctrl_sigma[i];
  }

  std::vector<double> var_recon_bilinear(npix, 0.0), var_recon_bicubic(npix, 0.0);
  double recon_node_resid_bilinear = -1.0, recon_node_resid_bicubic = -1.0;
  std::string recon_err_bilinear, recon_err_bicubic;
  int recon_ok_bilinear = 0, recon_ok_bicubic = 0;
  if (n_ctrl >= 4) {
    /* 控制点规则网格几何：由控制点坐标反解 (nx, ny, x0, y0, dx, dy)。
     * 生产噪声模型的 patch 中心在 cell 中心，规则网格。 */
    std::vector<double> ux, uy;
    for (int i = 0; i < n_ctrl; ++i) {
      bool seen = false;
      for (double v : ux) if (std::fabs(v - ctrl_x[i]) < 1e-6) { seen = true; break; }
      if (!seen) ux.push_back(ctrl_x[i]);
      seen = false;
      for (double v : uy) if (std::fabs(v - ctrl_y[i]) < 1e-6) { seen = true; break; }
      if (!seen) uy.push_back(ctrl_y[i]);
    }
    std::sort(ux.begin(), ux.end());
    std::sort(uy.begin(), uy.end());
    const int nx = static_cast<int>(ux.size());
    const int ny = static_cast<int>(uy.size());
    if (nx * ny == n_ctrl && nx >= 2 && ny >= 2) {
      auto run_recon = [&](const char* token, std::vector<double>* out,
                           double* node_resid, int* ok, std::string* err_out) {
        astrocs::v6::p2weight::SparseSnrLayer layer;
        layer.present = true;
        layer.regular_grid = true;
        layer.nx = nx;
        layer.ny = ny;
        layer.x0 = ux.front();
        layer.y0 = uy.front();
        layer.dx = (nx > 1) ? (ux.back() - ux.front()) / (nx - 1) : 1.0;
        layer.dy = (ny > 1) ? (uy.back() - uy.front()) / (ny - 1) : 1.0;
        /* cell 网格原点：生产噪声模型的 patch 中心在 (i+0.5)*Δ，而 sparse_snr_layer
         * 的 cell_center_v1 约定为 origin + i*Δ + (Δ-1)/2（weight_chain.cpp:473-474）。
         * 两者相差 0.5 px；不显式声明 origin=0.5 时生产重建算子**正确地 fail-closed**
         * （"control points are not at their cell centers (max offset 0.500000 px)"）。
         * 这里显式声明 origin=0.5 使 cell 中心 = (i+0.5)*Δ，与噪声模型控制点重合；
         * 该 0.5 px 相位差作为接口事实登记在报告中，不改任何生产代码。 */
        layer.grid_origin_x = 0.5;
        layer.grid_origin_y = 0.5;
        layer.reconstruction_operator = token;
        layer.points.resize(static_cast<std::size_t>(n_ctrl));
        for (int i = 0; i < n_ctrl; ++i) {
          /* 控制点值 = 背景方差面在该点的方差 [ADU^2]（>0，落在算子的正值域） */
          astrocs::v6::p2weight::SparseSnrPoint p;
          p.x = ctrl_x[i];
          p.y = ctrl_y[i];
          p.snr = ctrl_var[i];
          layer.points[i] = p;
        }
        astrocs::v6::p2weight::SparseSnrReconstructor rec;
        std::string err;
        if (!rec.prepare(layer, &err)) { *ok = 0; *err_out = err; return; }
        *node_resid = rec.node_reproduction_max_abs();
        astrocs::v6::p2weight::SparseReconstruction info;
        for (int yy = 0; yy < H; ++yy) {
          for (int xx = 0; xx < W; ++xx) {
            double v = 0.0;
            if (rec.eval(static_cast<double>(xx), static_cast<double>(yy), &v, &info, &err)) {
              (*out)[static_cast<std::size_t>(yy) * W + xx] = v;
            } else {
              (*out)[static_cast<std::size_t>(yy) * W + xx] = 0.0;
            }
          }
        }
        *ok = 1;
      };
      run_recon("bilinear_regular_grid_v1", &var_recon_bilinear,
                &recon_node_resid_bilinear, &recon_ok_bilinear, &recon_err_bilinear);
      run_recon("natural_bicubic_spline_clip_v1", &var_recon_bicubic,
                &recon_node_resid_bicubic, &recon_ok_bicubic, &recon_err_bicubic);
    }
  }

  /* ---------------- 2. 天光面（生产 sky_plane）+ 排异层次 ---------------- */
  /* 星点掩膜圆帽（生产 p2_star_mask_caps）：半径按 MASK-002 的 r_min 口径
   * r_min = max(1.5 px, 0.75*FWHM)，snr_threshold=0 ⇒ 全部星都掩。 */
  std::vector<double> ra_s(n_stars_use), dec_s(n_stars_use), snr_s(n_stars_use);
  for (int i = 0; i < n_stars_use; ++i) {
    ra_s[i] = 0.5 + sx[i] * pix_scale_deg;
    dec_s[i] = 0.5 + sy[i] * pix_scale_deg;
    snr_s[i] = 100.0;   /* 星点一律进掩膜（>0 即命中） */
  }
  std::vector<P2StarMaskCap> caps(std::max(n_stars_use, 1));
  std::uint64_t n_caps = 0;
  const double mask_radius_deg = 6.0 * pix_scale_deg;   /* 保守 6 px 掩膜半径 */
  if (n_stars_use > 0) {
    p2_star_mask_caps(ra_s.data(), dec_s.data(), snr_s.data(),
                      static_cast<std::uint64_t>(n_stars_use), 0.0, mask_radius_deg,
                      caps.data(), static_cast<std::uint64_t>(caps.size()), &n_caps);
  }

  /* 天空采样：sky_box x sky_box 盒子，盒内未被掩膜的像素 → 生产 patch 估计器 */
  P2SkyPatchConfig patch_cfg = p2_sky_patch_default_config();
  std::vector<P2SkySample> samples;
  std::vector<double> patch_sigma_used;      /* 逐盒 1.4826*MAD（诊断） */
  int n_patch_ok = 0, n_patch_rejected = 0;
  const int nbx = (W + sky_box - 1) / sky_box;
  const int nby = (H + sky_box - 1) / sky_box;
  std::vector<double> box_values(static_cast<std::size_t>(sky_box) * sky_box);
  std::vector<std::uint8_t> box_valid(box_values.size());
  for (int by = 0; by < nby; ++by) {
    for (int bx = 0; bx < nbx; ++bx) {
      std::size_t n = 0;
      double cx_sum = 0.0, cy_sum = 0.0;
      for (int dy = 0; dy < sky_box; ++dy) {
        for (int dx = 0; dx < sky_box; ++dx) {
          const int x = bx * sky_box + dx;
          const int y = by * sky_box + dy;
          if (x >= W || y >= H) continue;
          const double ra = 0.5 + x * pix_scale_deg;
          const double dec = 0.5 + y * pix_scale_deg;
          int hit = 0;
          if (n_caps > 0) hit = p2_star_mask_contains(caps.data(), n_caps, ra, dec);
          box_valid[n] = (hit == 1) ? 0 : 1;
          box_values[n] = frame0[static_cast<std::size_t>(y) * W + x];
          cx_sum += x;
          cy_sum += y;
          ++n;
        }
      }
      if (n < 8) continue;
      P2SkyPatchEstimate est;
      std::memset(&est, 0, sizeof(est));
      char err[256] = {0};
      const int rc = p2_sky_patch_estimate(box_values.data(), box_valid.data(), n,
                                           &patch_cfg, &est, err, sizeof(err));
      if (rc != P2_SKY_PATCH_OK) { ++n_patch_rejected; continue; }
      ++n_patch_ok;
      patch_sigma_used.push_back(est.sigma_mad);
      P2SkySample s;
      std::memset(&s, 0, sizeof(s));
      s.frame_id = 1;
      s.control_id = static_cast<std::uint64_t>(by) * nbx + bx;
      s.ra_deg = 0.5 + (cx_sum / n) * pix_scale_deg;
      s.dec_deg = 0.5 + (cy_sum / n) * pix_scale_deg;
      s.value = est.value;
      s.variance = est.variance;
      s.snr = 100.0;
      s.flags = 0;
      samples.push_back(s);
    }
  }

  /* 排异层次：解析 RejectionPlan（生产 p2_reject_plan_resolve）+ 候选栈排异 */
  P2RejectionPlanRequest req;
  std::memset(&req, 0, sizeof(req));
  req.request = P2_REJECT_AUTO;
  req.nominal_contributors = static_cast<std::uint32_t>(std::max(n_frames, 1));
  req.profile = "astrocs_adaptive";
  req.underdetermined_n = 0;
  P2RejectionPlan plan;
  std::memset(&plan, 0, sizeof(plan));
  char plan_err[512] = {0};
  const int rc_plan = p2_reject_plan_resolve(&req, &plan, plan_err, sizeof(plan_err));
  const char* plan_sem = (rc_plan == 0) ? p2_rejection_semantic_id(plan.method) : "unknown";

  /* 多帧候选栈：同一 sky control 跨帧 → 生产 p2_reject_stack */
  int rej_rc = -999, rej_n = 0;
  std::uint32_t rej_accepted = 0, rej_low = 0, rej_high = 0, rej_under = 0;
  int rej_status_last = -1;
  if (n_frames >= 2 && !samples.empty() && rc_plan == 0) {
    std::vector<double> stack_vals(static_cast<std::size_t>(n_frames));
    std::vector<std::uint8_t> stack_valid(static_cast<std::size_t>(n_frames), 1);
    std::vector<std::uint8_t> stack_acc(static_cast<std::size_t>(n_frames), 0);
    for (std::size_t si = 0; si < samples.size(); ++si) {
      const double ra = samples[si].ra_deg, dec = samples[si].dec_deg;
      const int x = static_cast<int>((ra - 0.5) / pix_scale_deg + 0.5);
      const int y = static_cast<int>((dec - 0.5) / pix_scale_deg + 0.5);
      if (x < 0 || x >= W || y < 0 || y >= H) continue;
      for (int f = 0; f < n_frames; ++f) {
        stack_vals[f] = data[static_cast<std::size_t>(f) * npix +
                             static_cast<std::size_t>(y) * W + x];
      }
      P2SampleStackView view;
      std::memset(&view, 0, sizeof(view));
      view.values = stack_vals.data();
      view.valid = stack_valid.data();
      view.count = static_cast<std::uint32_t>(n_frames);
      view.data_type = 1;                       /* fp64 */
      view.method = plan.method;
      view.sigma_low = plan.sigma.lower_sigma;
      view.sigma_high = plan.sigma.upper_sigma;
      view.max_iterations = plan.sigma.max_iterations;
      view.min_samples = plan.minimum_n;
      P2RejectionResult res;
      std::memset(&res, 0, sizeof(res));
      res.accepted = stack_acc.data();
      rej_rc = p2_reject_stack(&view, &res);
      ++rej_n;
      rej_accepted += res.accepted_count;
      rej_low += res.rejected_low;
      rej_high += res.rejected_high;
      rej_status_last = res.status;
    }
  }

  void* sky_model = nullptr;
  P2SkyPlaneConfig sky_cfg = p2_sky_plane_default_config();
  sky_cfg.spline_degree = 1;
  sky_cfg.node_spacing_deg = node_spacing_deg;
  sky_cfg.frame_gradient_order = 1;
  sky_cfg.max_extrapolation_deg = 2.0 * node_spacing_deg;
  sky_cfg.min_samples = 8;
  char sky_err[1024] = {0};
  const int rc_sky = samples.empty()
                         ? P2_SKY_PLANE_NO_USABLE_SAMPLES
                         : p2_sky_plane_build(samples.data(),
                                              static_cast<std::uint64_t>(samples.size()),
                                              &sky_cfg, &sky_model, sky_err, sizeof(sky_err));
  P2SkyPlaneInfo sky_info;
  std::memset(&sky_info, 0, sizeof(sky_info));
  if (sky_model) p2_sky_plane_info(sky_model, &sky_info);

  std::vector<double> sky_face(npix, 0.0);
  std::uint64_t n_sky_eval_out = 0;
  if (sky_model) {
    std::vector<double> ra_v(npix), dec_v(npix);
    for (int y = 0; y < H; ++y) {
      for (int x = 0; x < W; ++x) {
        const std::size_t k = static_cast<std::size_t>(y) * W + x;
        ra_v[k] = 0.5 + x * pix_scale_deg;
        dec_v[k] = 0.5 + y * pix_scale_deg;
      }
    }
    std::vector<std::uint8_t> st(npix, 0);
    p2_sky_plane_eval_block(sky_model, 1, ra_v.data(), dec_v.data(),
                            static_cast<std::uint64_t>(npix), sky_face.data(), st.data());
    for (std::size_t k = 0; k < npix; ++k) if (st[k] != P2_SKY_EVAL_OK) ++n_sky_eval_out;
  }

  /* ---------------- 3. 源分离：GLS 通量（生产 w_info_solve）+ PSF 轮廓 ---------------- */
  const int half = (profile_half > 0) ? profile_half
                                      : auto_half(psf_sigma_px * kMoffat4FwhmFactor);
  std::vector<double> ssrc_est(npix, 0.0);
  std::vector<double> flux_hat(std::max(n_stars_use, 1), 0.0);
  std::vector<double> flux_var(std::max(n_stars_use, 1), 0.0);
  int n_flux_ok = 0;
  for (int i = 0; i < n_stars_use; ++i) {
    /* 入参 fwhm 用 **PSF 块**约定 FWHM_moffat4 = 1.230310*sigma（snr_science.cpp:22），
     * 与生产噪声模型的调用点 psf 块 row[5] 同源（snr_estimator.h:184-186）。
     * 注意与**检测块**高斯约定 FWHM = 2.3548200450309493*sigma 相差
     * 2.3548200450309493/1.230310 = 1.914005 倍（DISP-STAR-007 禁止跨块混用）。 */
    const double fwhm_i = (sfwhm[i] > 0.0) ? sfwhm[i] : psf_sigma_px * kMoffat4FwhmFactor;
    const double sigma_i = fwhm_i / kMoffat4FwhmFactor;
    std::vector<double> P;
    moffat4_grid(sigma_i, half, &P, nullptr, nullptr);
    const int n = 2 * half + 1;
    const int x0 = static_cast<int>(std::lround(sx[i])) - half;
    const int y0 = static_cast<int>(std::lround(sy[i])) - half;
    std::vector<double> pv, dsub, var_i;
    pv.reserve(static_cast<std::size_t>(n) * n);
    dsub.reserve(pv.capacity());
    var_i.reserve(pv.capacity());
    std::size_t n_in = 0;
    for (int j = 0; j < n; ++j) {
      for (int ii = 0; ii < n; ++ii) {
        const int x = x0 + ii, y = y0 + j;
        if (x < 0 || x >= W || y < 0 || y >= H) continue;
        const std::size_t k = static_cast<std::size_t>(y) * W + x;
        const double Pi = P[static_cast<std::size_t>(j) * n + ii];
        double vs = var_plane[k];
        if (!(vs > 0.0)) vs = var_recon_bilinear[k];
        if (!(vs > 0.0)) continue;
        pv.push_back(Pi);
        dsub.push_back(frame0[k] - sky_face[k]);
        var_i.push_back(vs);
        ++n_in;
      }
    }
    if (n_in < 16) continue;
    /* 固定点两轮：C 含源泊松项（NOISE_MODEL §5c 的加权方差面），与生产
     * snr_source_snr_f64 的逐像素方差同式 sigma_i^2 = sigma_sky^2 + F*P_i/g。 */
    double F_prev = 0.0;
    double q = 0.0, w = 0.0;
    for (int iter = 0; iter < 2; ++iter) {
      std::vector<double> c2(n_in);
      for (std::size_t k = 0; k < n_in; ++k) {
        c2[k] = var_i[k] + snr_noise_gain_variance(F_prev * pv[k], gain, 0.0);
      }
      astrocs::v6::p1psfw::CovarianceView cov;
      cov.kind = astrocs::v6::p1psfw::CovarianceView::Kind::diagonal;
      cov.m = n_in;
      cov.sigma2 = c2.data();
      cov.sigma_declared = true;
      const astrocs::v6::p1psfw::PointEstimate pe =
          astrocs::v6::p1psfw::w_info_solve(cov, pv.data(), dsub.data(), 1.0);
      if (!pe.ok || !(pe.w_info > 0.0)) { F_prev = 0.0; w = 0.0; q = 0.0; break; }
      q = pe.q;
      w = pe.w_info;
      F_prev = q / w;
    }
    if (!(w > 0.0)) continue;
    flux_hat[i] = F_prev;
    flux_var[i] = 1.0 / w;
    ++n_flux_ok;
    /* 源模型：把该星的通量按 PSF 摊到像素上（只加源，不加天光） */
    for (int j = 0; j < n; ++j) {
      for (int ii = 0; ii < n; ++ii) {
        const int x = x0 + ii, y = y0 + j;
        if (x < 0 || x >= W || y < 0 || y >= H) continue;
        ssrc_est[static_cast<std::size_t>(y) * W + x] +=
            F_prev * P[static_cast<std::size_t>(j) * n + ii];
      }
    }
  }

  /* ---------------- 4. 源项方差（生产 snr_noise_gain_variance） ---------------- */
  std::vector<double> src_term(npix, 0.0), src_term_true(npix, 0.0);
  std::vector<double> var_slow_true_out(npix, 0.0);
  for (std::size_t k = 0; k < npix; ++k) {
    src_term[k] = snr_noise_gain_variance(ssrc_est[k], gain, 0.0);
    src_term_true[k] = snr_noise_gain_variance(src_true[k], gain, 0.0);
    var_slow_true_out[k] = var_slow_true[k];
  }

  /* ---------------- 5. 逐源科学 SNR（生产 snr_source_snr_f64） ---------------- */
  std::vector<double> per_star_snr(std::max(n_stars_use, 1), 0.0);
  std::vector<double> per_star_sigma_f(std::max(n_stars_use, 1), 0.0);
  int n_star_snr_ok = 0;
  for (int i = 0; i < n_stars_use; ++i) {
    if (!(flux_hat[i] > 0.0)) continue;
    const int x = static_cast<int>(std::lround(sx[i]));
    const int y = static_cast<int>(std::lround(sy[i]));
    if (x < 0 || x >= W || y < 0 || y >= H) continue;
    const std::size_t k = static_cast<std::size_t>(y) * W + x;
    double vs = var_plane[k];
    if (!(vs > 0.0)) vs = var_recon_bilinear[k];
    if (!(vs > 0.0)) continue;
    const double fwhm_i = (sfwhm[i] > 0.0) ? sfwhm[i] : psf_sigma_px * kMoffat4FwhmFactor;
    const double sigma_i_star = fwhm_i / kMoffat4FwhmFactor;
    SnrSourceParams sp;
    std::memset(&sp, 0, sizeof(sp));
    sp.flux_adu = flux_hat[i];
    sp.sigma_px = sigma_i_star;
    sp.sigma_sky_adu = std::sqrt(vs);
    sp.gain_e_per_adu = gain;
    sp.read_noise_e = 0.0;   /* sigma_sky 已是经验总 rms（含读噪）⇒ 不再加 (RN/g)^2 */
    sp.profile_half_px = half;
    sp.fwhm_px = 0.0;   /* 0 => 用 sigma_px（本块 Moffat4 尺度），不做跨块 FWHM 换算 */
    sp.sigma_sky_source = SNR_SIGMA_SKY_EMPIRICAL_TOTAL_RMS;
    SnrSourceResult sr;
    std::memset(&sr, 0, sizeof(sr));
    if (snr_source_snr_f64(&sp, &sr) == 0) {
      per_star_snr[i] = sr.snr_optimal;
      per_star_sigma_f[i] = sr.sigma_f_optimal_adu;
      ++n_star_snr_ok;
    }
  }

  /* ---------------- 输出 ---------------- */
  std::ofstream os(argv[2], std::ios::binary);
  if (!os) { std::fprintf(stderr, "cannot write %s\n", argv[2]); return 2; }
  std::vector<double> meta;
  auto push = [&meta](double v) { meta.push_back(v); };
  push(profile_selfcheck);
  push(static_cast<double>(rc_model));
  push(static_cast<double>(rc_fill));
  push(static_cast<double>(model.has_spatial_field));
  push(static_cast<double>(model.degenerate));
  push(static_cast<double>(model.mask_degraded));
  push(model.sigma_bg_global);
  push(model.variance_bg_global);
  push(static_cast<double>(model.n_qualified_patches));
  push(static_cast<double>(model.n_rejected_patches));
  push(model.mask_radius_p50);
  push(model.mask_frac);
  push(static_cast<double>(n_ctrl));
  push(recon_node_resid_bilinear);
  push(recon_node_resid_bicubic);
  push(static_cast<double>(recon_ok_bilinear));
  push(static_cast<double>(recon_ok_bicubic));
  push(static_cast<double>(rc_plan));
  push(static_cast<double>(plan.method));
  push(static_cast<double>(plan.nominal_n));
  push(static_cast<double>(n_patch_ok));
  push(static_cast<double>(n_patch_rejected));
  push(static_cast<double>(rc_sky));
  push(static_cast<double>(sky_info.n_samples));
  push(static_cast<double>(sky_info.n_used));
  push(static_cast<double>(sky_info.n_nodes));
  push(sky_info.rms_weighted);
  push(sky_info.chi2_red);
  push(static_cast<double>(sky_info.iterations));
  push(static_cast<double>(sky_info.n_rejected));
  push(static_cast<double>(n_sky_eval_out));
  push(static_cast<double>(n_flux_ok));
  push(static_cast<double>(n_star_snr_ok));
  push(static_cast<double>(sdet_rc));
  push(static_cast<double>(sdet_count));
  push(static_cast<double>(n_stars_use));
  push(static_cast<double>(rej_rc));
  push(static_cast<double>(rej_n));
  push(static_cast<double>(rej_accepted));
  push(static_cast<double>(rej_low));
  push(static_cast<double>(rej_high));
  push(static_cast<double>(rej_under));
  push(static_cast<double>(rej_status_last));
  write_block(os, meta);
  /* 文本头（可读诊断） */
  {
    std::string t;
    char buf[512];
    std::snprintf(buf, sizeof(buf),
                  "profile_selfcheck %.17g\nrc_model %d\nrc_fill %d\nn_ctrl %d\n"
                  "recon_bilinear_ok %d\nrecon_bicubic_ok %d\nrc_plan %d\nplan_method %d\n"
                  "plan_semantic %s\nn_patch_ok %d\nn_patch_rejected %d\nrc_sky %d\n"
                  "sky_n_nodes %llu\nsky_n_used %llu\nsky_rms_weighted %.17g\n"
                  "sky_chi2_red %.17g\nsky_n_rejected %llu\nn_sky_eval_out_of_domain %llu\n"
                  "n_flux_ok %d\nn_star_snr_ok %d\nrej_rc %d\nrej_n %d\n"
                  "recon_err_bilinear %s\nrecon_err_bicubic %s\n",
                  profile_selfcheck, rc_model, rc_fill, n_ctrl, recon_ok_bilinear,
                  recon_ok_bicubic, rc_plan, plan.method, plan_sem, n_patch_ok,
                  n_patch_rejected, rc_sky,
                  static_cast<unsigned long long>(sky_info.n_nodes),
                  static_cast<unsigned long long>(sky_info.n_used),
                  sky_info.rms_weighted, sky_info.chi2_red,
                  static_cast<unsigned long long>(sky_info.n_rejected),
                  static_cast<unsigned long long>(n_sky_eval_out),
                  n_flux_ok, n_star_snr_ok, rej_rc, rej_n,
                  recon_err_bilinear.c_str(), recon_err_bicubic.c_str());
    t = buf;
    const std::uint32_t tl = static_cast<std::uint32_t>(t.size());
    os.write(reinterpret_cast<const char*>(&tl), sizeof(tl));
    os.write(t.data(), tl);
  }
  write_block(os, ctrl_x);
  write_block(os, ctrl_y);
  write_block(os, ctrl_var);
  write_block(os, ctrl_sigma);
  std::vector<double> var_plane_d(var_plane.begin(), var_plane.end());
  write_block(os, var_plane_d);
  write_block(os, var_recon_bilinear);
  write_block(os, var_recon_bicubic);
  write_block(os, sky_face);
  write_block(os, ssrc_est);
  write_block(os, src_term);
  write_block(os, src_term_true);
  write_block(os, var_slow_true_out);
  write_block(os, st_x);
  write_block(os, st_y);
  write_block(os, st_flux);
  write_block(os, st_fwhm);
  write_block(os, flux_hat);
  write_block(os, flux_var);
  write_block(os, per_star_snr);
  write_block(os, per_star_sigma_f);
  os.close();
  if (sky_model) p2_sky_plane_close(sky_model);
  if (rc_model == 0) snr_noise_model_v1_free(&model);

  std::printf("profile_selfcheck=%.3e rc_model=%d rc_fill=%d n_ctrl=%d "
              "recon_bilinear_ok=%d recon_bicubic_ok=%d rc_sky=%d n_flux_ok=%d\n",
              profile_selfcheck, rc_model, rc_fill, n_ctrl, recon_ok_bilinear,
              recon_ok_bicubic, rc_sky, n_flux_ok);
  return 0;
}
