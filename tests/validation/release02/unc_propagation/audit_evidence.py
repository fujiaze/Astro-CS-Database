#!/usr/bin/env python3
"""UNC-PROP evidence: (1) code-path facts, (2) real-frame SNR/ref-flux pairing,
(3) the high/low-SNR weight criterion on production data.

Read-only. No production code changed. Writes only under run/RELEASE-02/unc-prop/.
"""
import json, os, re, sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))
L4 = os.path.join(ROOT, "run/RELEASE-02/L4-rebuild")
OUT = []

def say(s=""):
    OUT.append(s)
    print(s)

# ---------------------------------------------------------------- 1. code facts
say("=" * 72)
say("1. CODE-PATH FACTS (grep-level, read-only)")
say("=" * 72)

def grep(pattern, path, label):
    hits = []
    p = os.path.join(ROOT, path)
    if not os.path.exists(p):
        say("  [MISS] %s (%s)" % (path, label)); return hits
    for i, line in enumerate(open(p, encoding="utf-8", errors="replace"), 1):
        if re.search(pattern, line):
            hits.append((i, line.rstrip("\n")))
    say("  %-46s %s -> %d hit(s)" % (label, path, len(hits)))
    for i, l in hits[:6]:
        say("      %s:%d  %s" % (os.path.basename(path), i, l.strip()[:110]))
    return hits

grep(r"p2_sky_plane_eval_delta_block", "lib/infrastructure/scheduler/src/module_adapters.cpp", "upm-apply applies delta (value only)")
grep(r"out_v\[static_cast<size_t>\(k\)\] -= dvals", "lib/infrastructure/scheduler/src/module_adapters.cpp", "upm-apply: corrected -= delta (value only)")
grep(r"local_ivar_map\.find|local_snr_map\.find|snr_v = frame_snr", "lib/algorithms/coverage/tools/stage2.cpp", "stage2 weight sources (gain-independent)")
grep(r"p2_upm_ma_param_cov", "lib/algorithms/integration/v6/src/phase2_integrate.cpp", "param cov in v6 diagnostic route")
grep(r"p2_upm_ma_param_cov|p2_upm_ma_c_out", "lib/algorithms/coverage/tools/stage2.cpp", "param cov in reference path")
grep(r"p2_upm_build_geo", "lib/infrastructure/scheduler/src/module_adapters.cpp", "production builds W2 model only (no cov API)")
grep(r"out_v\[i\] /= gain", "lib/algorithms/coverage/tools/stage2.cpp", "stage2: corrected /= g_k (value only)")
grep(r"aio_hips_open\(p.c_str\(\), AIO_HIPS_RD_IVAR\)", "lib/infrastructure/scheduler/src/module_adapters.cpp", "integrate reads RAW Phase1 ivar")
grep(r"snr_weights\[it.slot\[d\]\]", "lib/infrastructure/scheduler/src/module_adapters.cpp", "integrate uses frame-SNR weight verbatim")
grep(r"p2_reject_stack_ex", "lib/infrastructure/scheduler/src/module_adapters.cpp", "production reject kernel = stack_ex (MAD)")
grep(r"p2_reject_classify", "lib/infrastructure/scheduler/src/module_adapters.cpp", "p2_reject_classify (sigma_eff path) in production")
grep(r"p2_upm_ma_param_cov", "lib/infrastructure/scheduler/src/module_adapters.cpp", "param cov called in production chain")
grep(r"variance_out|out->variance", "lib/algorithms/coverage/src/integrate.cpp", "p2_integrate_pixel variance output")

say()
say("  upm-apply writes corrected VALUES only; no variance/ivar/sigma key is emitted:")
cor = json.load(open(os.path.join(L4, "upmfix_out/p2_corrected.json")))
say("      p2_corrected.json keys = %s" % sorted(cor.keys()))
say("      sky_plane_mode=%r sky_plane_applied=%r" % (cor.get("sky_plane_mode"), cor.get("sky_plane_applied")))
integ = json.load(open(os.path.join(L4, "upmfix_out/p2_integrated.json")))
say("      p2_integrated.json: weight_mode=%r weight_basis=%r weight_source=%r uncertainty_available=%r"
    % (integ.get("weight_mode"), integ.get("weight_basis"), integ.get("weight_source"),
       integ.get("uncertainty_available")))
fin = json.load(open(os.path.join(L4, "upmfix_out/p2_final.json")))
say("      p2_final.json: products=%r uncertainty_available=%r" % (fin.get("products"), fin.get("uncertainty_available")))
rej = json.load(open(os.path.join(L4, "upmfix_out/p2_rejection.json")))
say("      p2_rejection.json entry=%r plan=%s" % (rej.get("entry"), json.dumps(rej.get("plan"))[:160]))

say()
say("  Phase1 input frames carry NO variance/ivar product (so mode-2 has no source):")
p1 = json.load(open(os.path.join(L4, "norm/t2_m1_red/M42_M1_T2_flying_dutchman-20251212_012404-300S-Red/p1_final.json")))
say("      n_ivar_tiles=%r n_variance_tiles=%r products=%r uncertainty_available=%r"
    % (p1.get("n_ivar_tiles"), p1.get("n_variance_tiles"), p1.get("products"), p1.get("uncertainty_available")))

# weight_mode=2 production run actually fail-closes
run2 = json.load(open(os.path.join(L4, "mosaic_out/astrocs_run_212e46b74828.json")))
say()
say("  Production weight_mode=2 run status=%r" % run2.get("status"))
say("      summary=%s" % str(run2.get("summary"))[:400])

# ------------------------------------------------------- 2. real frame SNR/ref
say()
say("=" * 72)
say("2. REAL FRAME SNR / REFERENCE FLUX PAIRING (49 L4 frames)")
say("=" * 72)
cfg = json.load(open(os.path.join(L4, "mosaic_upmfix.json")))
rows = []
for p in cfg["hips_paths"]:
    prop = os.path.join(p, "signal", "properties")
    d = {}
    if os.path.exists(prop):
        for line in open(prop, encoding="utf-8", errors="replace"):
            if "=" in line:
                k, v = line.rstrip("\n").split("=", 1)
                d[k] = v
    rows.append((os.path.basename(p), d.get("ASTROCS_FRAME_SNR"), d.get("ASTROCS_REFERENCE_FLUX")))

miss = [r for r in rows if r[1] is None or r[2] is None]
vals = [(r[0], float(r[1]), float(r[2])) for r in rows if r[1] is not None and r[2] is not None]
snrs = [v[1] for v in vals]; refs = [v[2] for v in vals]
say("  n_frames=%d  missing ASTROCS_FRAME_SNR/ASTROCS_REFERENCE_FLUX=%d" % (len(rows), len(miss)))
for m in miss:
    say("      MISSING: %s" % m[0])
say("  frame_snr  min=%.4f max=%.4f median=%.4f  (max/min=%.4f)"
    % (min(snrs), max(snrs), sorted(snrs)[len(snrs)//2], max(snrs) / min(snrs)))
say("  ref_flux   min=%.4f max=%.4f median=%.4f  (max/min=%.4f)"
    % (min(refs), max(refs), sorted(refs)[len(refs)//2], max(refs) / min(refs)))
say("  weight-chain gate requires a GROUP-COMMON F_ref (rel tol 1e-9) -> never satisfiable;")
say("  3/49 frames miss the keys entirely -> unclosed_missing_frame_snr.")
say()
say("  If the per-frame F_ref were used in w = SNR^2/F_ref^2 (the write-side pairing")
say("  defect flagged in weight_chain.h:34-41), the COMMON-scale weight would instead be")
say("  w_correct = SNR^2/F_common^2, i.e. w_correct/w_used = (F_ref/F_common)^2:")
fcom = sorted(refs)[len(refs) // 2]
say("      F_common (median) = %.4f" % fcom)
say("      %-58s %8s %10s %10s" % ("frame", "SNR", "F_ref", "wcorr/wused"))
for v in sorted(vals, key=lambda x: -x[1])[:4] + sorted(vals, key=lambda x: x[1])[:4]:
    ratio = (v[2] / fcom) ** 2
    say("      %-58s %8.3f %10.1f %10.4f" % (v[0][:58], v[1], v[2], ratio))
hi = max(vals, key=lambda x: x[1]); lo = min(vals, key=lambda x: x[1])
say("  => highest-SNR frame gets w_correct/w_used = (%.0f/%.0f)^2 = %.3f (i.e. the"
    % (hi[2], fcom, (hi[2] / fcom) ** 2))
say("     per-frame-F_ref chain would suppress it by %.2fx relative to a paired chain)."
    % (1.0 / (hi[2] / fcom) ** 2))

# --------------------------------------------------- 3. high/low SNR criterion
say()
say("=" * 72)
say("3. HIGH-SNR vs LOW-SNR CRITERION (production data, current chain)")
say("=" * 72)
say("  Observable production state: weight_mode=1 (equal weight). Every accepted sample")
say("  gets w=1, so w_highSNR == w_lowSNR == 1 -> the criterion 'w_high >= w_low' holds")
say("  trivially, but ONLY because there is no variance surface at all (uncertainty_available")
say("  = false). There is nothing to propagate: the normalization emitted no variance and")
say("  the input frames have no ivar product.")
say()
say("  Consequence of the missing propagation on the SNR weight chain (if it were closed):")
for name, snr, ref in [("highest-SNR", hi[1], hi[2]), ("lowest-SNR", lo[1], lo[2])]:
    w_common = (snr / fcom) ** 2
    w_own = (snr / ref) ** 2
    say("      %-12s SNR=%.3f F_ref=%.1f  w(common F)=%.6e  w(own F)=%.6e  ratio=%.4f"
        % (name, snr, ref, w_common, w_own, w_common / w_own))
say("  -> using the frame's own F_ref collapses w to ~1/sigma_F^2 and silently reduces the")
say("     high-SNR frame's common-scale weight by a_f^2; the chain currently fail-closes")
say("     instead (safe), but any relaxation of the 1e-9 gate would expose the down-weighting.")

with open(os.path.join(os.path.dirname(__file__), "code_facts_and_realdata.log"), "w") as f:
    f.write("\n".join(OUT) + "\n")
print("\nwrote code_facts_and_realdata.log")