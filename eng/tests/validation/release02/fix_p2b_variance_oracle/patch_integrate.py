#!/usr/bin/env python3
# Atomic patch for module_adapters.cpp — RELEASE-02 FIX-P2b (P2b-2 integrate wiring).
import io, sys

path = "lib/infrastructure/scheduler/src/module_adapters.cpp"
s = io.open(path, encoding="utf-8").read()
orig = s
reps = []

reps.append((
"""  const bool allow_fallback = doc.value("legacy_allow_weight_fallback", false);

  // ivar 产品读取（weight_mode=2 必须）。ivar 缺失 → **不再等权降级**:""",
"""  const bool allow_fallback = doc.value("legacy_allow_weight_fallback", false);

  // ── RELEASE-02 P2b-2: 优先消费归一化逐像素方差 w = 1/Var(corrected) ──────
  // p2_corrected.json 报 uncertainty_available=true（方差完整传播：残差制造者
  // PΣPᵀ + 逐像素 Phase1 噪声 + 参数协方差项）时，权重面**优先**用逐像素
  // 1/Var(corrected)（weight_chain.h: weight_from_corrected_variance）；否则
  // 退回既有 ivar 面 / 帧级 SNR 链（后者须组内公共 F_ref 与 g_k 配对）。
  // 过渡期（方差未完整传播）不得声称逆方差加权（P2b-5）。
  std::vector<std::string> corr_var_file(frames.size());
  bool corr_var_ready = false;
  if (weight_mode == 2) {
    const bool cor_var_avail = cor_doc.value("variance_available", false);
    const bool cor_unc_avail = cor_doc.value("uncertainty_available", false);
    bool all_files = cor_var_avail && cor_unc_avail;
    for (size_t f = 0; f < frames.size() && all_files; ++f) {
      corr_var_file[f] = frames[f].value("var_file", std::string());
      if (corr_var_file[f].empty()) all_files = false;
    }
    corr_var_ready = all_files;
  }

  // ivar 产品读取（weight_mode=2 必须）。ivar 缺失 → **不再等权降级**:"""))

reps.append((
"""  std::string snr_chain_closure = "not_used";
  if (weight_mode == 2) {
    uint64_t ivar_missing = 0;""",
"""  std::string snr_chain_closure = "not_used";
  if (corr_var_ready) {
    // P2b-2 priority 1：逐像素归一化方差面（唯一科学正确的权重来源）。
    weight_basis = "per_pixel_corrected_variance";
    weight_source = "corrected_variance";
    uncertainty_available = true;
  }
  if (weight_mode == 2 && !corr_var_ready) {
    uint64_t ivar_missing = 0;"""))

reps.append((
"""  const bool need_ivar = (weight_mode == 2 && !fallback && !use_snr_chain);""",
"""  const bool need_ivar =
      (weight_mode == 2 && !fallback && !use_snr_chain && !corr_var_ready);"""))

reps.append((
"""    std::vector<const std::vector<double>*> tile_v;
    tile_v.reserve(tile_bufs.size());
    for (const auto& b : tile_bufs) tile_v.push_back(&b);""",
"""    // P2b-2: 逐像素归一化方差 tile（与 corrected tile 同布局/同 offset）
    std::vector<std::vector<double>> tile_cvar;
    if (corr_var_ready) {
      tile_cvar.reserve(static_cast<size_t>(depth));
      for (size_t d = 0; d < depth; ++d) {
        std::vector<double> buf;
        const std::string& vf = corr_var_file[it.slot[d]];
        if (vf.empty() ||
            !p2_read_bin_range<double>(vf, it.data_off[d], tile_span, &buf)) {
          t_errd[ti_s] = static_cast<int>(ErrorDomain::IO);
          t_err[ti_s] = "corrected variance bin read failed (tile " +
                        std::to_string(tip) + " frame " +
                        std::to_string(it.slot[d]) + ")";
          return;
        }
        tile_cvar.push_back(std::move(buf));
      }
    }
    std::vector<const std::vector<double>*> tile_v;
    tile_v.reserve(tile_bufs.size());
    for (const auto& b : tile_bufs) tile_v.push_back(&b);"""))

reps.append((
"""        double w = 1.0;
        if (weight_mode == 2 && !fallback) {
          if (use_snr_chain) {""",
"""        double w = 1.0;
        if (weight_mode == 2 && !fallback) {
          if (corr_var_ready) {
            // P2b-2 priority 1: w = 1/Var(corrected)（逐像素归一化方差）
            const double vv = tile_cvar[d][static_cast<size_t>(p)];
            if (!std::isfinite(vv) || !(vv > 0.0)) {
              t_errd[ti_s] = static_cast<int>(ErrorDomain::DATA);
              t_err[ti_s] = "corrected variance invalid (non-finite/<=0) at frame " +
                            std::to_string(it.slot[d]) + " tile " +
                            std::to_string(tip) + " pixel " + std::to_string(p) +
                            " (P2b-2: w=1/Var(corrected) fail-closed, no clamp)";
              return;
            }
            std::string verr;
            if (!astrocs::v6::p2weight::weight_from_corrected_variance(vv, &w,
                                                                       &verr)) {
              t_errd[ti_s] = static_cast<int>(ErrorDomain::DATA);
              t_err[ti_s] = "weight_from_corrected_variance failed at frame " +
                            std::to_string(it.slot[d]) + ": " + verr;
              return;
            }
          } else if (use_snr_chain) {"""))

reps.append((
"""                       {"weight_source", weight_source},
                       {"snr_chain_closure", snr_chain_closure},""",
"""                       {"weight_source", weight_source},
                       {"corrected_variance_used", corr_var_ready},
                       {"snr_chain_closure", snr_chain_closure},"""))

reps.append((
"""  (*man)["weight_source"] = weight_source;""",
"""  (*man)["weight_source"] = weight_source;
  (*man)["corrected_variance_used"] = corr_var_ready;"""))

for old, new in reps:
    n = s.count(old)
    if n != 1:
        sys.stderr.write("ANCHOR COUNT %d for: %r\n" % (n, old[:80]))
        sys.exit(2)
    s = s.replace(old, new)

io.open(path, "w", encoding="utf-8").write(s)
print("PATCH_OK bytes %d -> %d" % (len(orig), len(s)))
