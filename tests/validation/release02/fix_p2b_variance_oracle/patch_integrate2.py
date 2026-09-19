#!/usr/bin/env python3
# Atomic patch #2: P2b-2 frame-SNR chain gain pairing in p2_op_integrate.
import io, sys
path = "lib/infrastructure/scheduler/src/module_adapters.cpp"
s = io.open(path, encoding="utf-8").read()
orig = s
reps = []
reps.append((
"""      std::vector<FrameWeightInput> winputs(frames.size());
      double ref_flux = 0.0;
      bool ref_flux_set = false;
      std::string ref_err;""",
"""      std::vector<FrameWeightInput> winputs(frames.size());
      // P2b-2: 归一化含乘性 /g_k 时，帧级链须 w = SNR²/F_ref²·g_k²
      // （weight_chain FrameWeightInput.gain 可空；缺一 fail-closed）。
      // 生产方案 B（加性-only, g≡1）不置 multiplicative_gain_applied ⇒
      // gain=nullptr、require_frame_gain=false，行为与旧口径逐位一致。
      const bool require_gain = cor_doc.value("multiplicative_gain_applied", false);
      std::vector<double> frame_gain(frames.size(), 1.0);
      double ref_flux = 0.0;
      bool ref_flux_set = false;
      std::string ref_err;"""))
reps.append((
"""        in.sparse = nullptr;   // 稀疏 SNR 层尚未接入生产数据面
        in.x = 0.0;
        in.y = 0.0;""",
"""        in.sparse = nullptr;   // 稀疏 SNR 层尚未接入生产数据面
        in.x = 0.0;
        in.y = 0.0;
        in.gain = nullptr;
        if (require_gain) {
          frame_gain[f] = frames[f].value("frame_gain", 1.0);
          in.gain = &frame_gain[f];
        }"""))
reps.append((
"""      const WeightChainResult wres =
          astrocs::v6::p2weight::compute_inverse_variance_weights(
              winputs, ref_flux_set ? ref_flux : 0.0);""",
"""      astrocs::v6::p2weight::WeightChainPolicy wpolicy;
      wpolicy.require_frame_gain = require_gain;
      const WeightChainResult wres =
          astrocs::v6::p2weight::compute_inverse_variance_weights(
              winputs, ref_flux_set ? ref_flux : 0.0, wpolicy);"""))
for old, new in reps:
    n = s.count(old)
    if n != 1:
        sys.stderr.write("ANCHOR COUNT %d for: %r\n" % (n, old[:90]))
        sys.exit(2)
    s = s.replace(old, new)
io.open(path, "w", encoding="utf-8").write(s)
print("PATCH2_OK bytes %d -> %d" % (len(orig), len(s)))
