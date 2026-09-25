"""V3: what does additive_mode="c" actually subtract? Algebra recompute.

Pinned source facts (read at HEAD c8f64e9a, no repo import):
  lib/algorithms/coverage/src/upm.cpp:1907-1942  p2_upm_calibrate_block:
        output = input - c - g,  c = evaluate_c_field(m, fi, ...) = the FRAME row theta_f,
        g = m->gauge row (empty => 0.0 when cfg.final_gauge == 0, upm.cpp:283/304)
  lib/algorithms/coverage/include/astro/phase2/upm.h:128-135  p2_upm_evaluate_c:
        "c_i = theta[i] 只是帧分量... 不含公共残差场 G... m_full_frame=1 时参考帧 c_row == 0"
  lib/infrastructure/scheduler/src/module_adapters.cpp:10524-10549  apply node:
        v = calibrate_block(raw)            # mode c keeps this
        if (!sub_c) v += evaluate_c(...)    # delta/both: add frame row back
        if (sub_delta) v -= sky_plane_eval_delta(...)   # delta: subtract delta_k
  lib/algorithms/coverage/include/astro/phase2/sky_plane.h:473-479:
        delta_k = b_k - B_ref ; corrected = raw - delta_k  <=>  raw - b_k + B_ref
  science 正本 docs/science/PHASE2_UPM.md:64-69 (S 5) and :129 (:7 常量场不变量)
        calibrated_f = raw_f - C_f, C_f = bilinear(theta_f);
        constant common input => M = C and C_f = 0 ("不写 C_f = C")

This script fits y_ik = M_k + C_ik under the shipped min-frame_id gauge and evaluates the
three subtractions, to test the comment "c = raw - C_k (全减, 含 B_ref => 背景被剪掉)".
"""
import numpy as np

FRAMES = ["A", "B", "C"]          # A = smallest frame_id => gauge reference frame
K = 6                             # control cells

rng = np.random.default_rng(20260925)
B_ref = np.array([280.0, 292.5, 305.0, 318.5, 297.0, 285.5])   # common sky plane (ADU)
# per-frame deviations (多退少补 target): delta_k
delta = {"A": np.array([0.0, 0.0, 0.0, 0.0, 0.0, 0.0]) + 1.0,
         "B": np.array([2.0, -1.0, 3.0, 0.5, -2.0, 1.5]) + 4.0,
         "C": np.array([-3.0, 1.5, 2.0, -2.5, 0.0, 3.0]) + 7.0}
# true sources (frame-independent astrophysical signal)
src = np.array([15.0, 0.0, 40.0, 3.0, 0.0, 22.0])

# observations y_ik = B_ref + delta_k(frame) + src  (noiseless => exact LS)
y = {f: B_ref + delta[f] + src for f in FRAMES}

# --- fit y_ik = M_k + C_ik with gauge C_ref,k = 0 (min frame_id = A) -----------
M = y["A"].copy()                          # reference frame absorbs the level
Cfit = {f: y[f] - M for f in FRAMES}       # frame component = delta_f - delta_A (level-free)

print("fit under shipped gauge (ref frame A has C_A == 0):")
print("  M_k  = %s   (= B_ref + delta_A + src: 公共面携带背景与天体结构)"
      % np.array2string(M, precision=3))
for f in FRAMES:
    print("  C_%s = %s   mean=%.3e (纯帧间偏差，不含天光水平)"
          % (f, np.array2string(Cfit[f], precision=3), float(np.mean(Cfit[f]))))

# --- §7 常量场不变量 check: constant common input (no per-frame offsets) ------
y2 = {f: np.full(K, 300.0) + src for f in FRAMES}
M2 = y2["A"]
C2 = {f: y2[f] - M2 for f in FRAMES}
print("\nSCI-UPM 常量场不变量 (各帧同值公共输入 300 ADU):")
print("  M = 300 + src (公共面携带背景): min(M)=%.3f  max(M)=%.3f" % (M2.min(), M2.max()))
print("  C_f == 0 for every frame:      %s" % all(np.allclose(C2[f], 0.0) for f in FRAMES))

# --- mode products as implemented in module_adapters.cpp ----------------------
b_total = {f: B_ref + delta[f] for f in FRAMES}      # sky_plane_eval 的全量 b_k
G = np.zeros(K)                                       # final_gauge default 0 (upm.cpp:283)
prod = {}
prod["mode c  (raw - C - G)"] = {f: y[f] - Cfit[f] - G for f in FRAMES}
prod["mode delta (raw - delta_k)"] = {f: y[f] - delta[f] for f in FRAMES}
prod["arm raw - b_k (全减, 无产路)"] = {f: y[f] - b_total[f] for f in FRAMES}

print("\nproducts (per-cell level; src is the astrophysical signal, B_ref = common sky):")
for name, d in prod.items():
    lv = np.mean(np.stack([d[f] for f in FRAMES]), axis=0)
    bg_component = lv - src            # what survives besides the sources
    print("  %-30s 帧间均值面 = %s" % (name, np.array2string(lv, precision=3)))
    print("  %-30s 扣除后残留的天光水平 = %s -> %s"
          % ("", np.array2string(bg_component, precision=3),
             "背景剪掉(≈0)" if np.allclose(bg_component, 0.0, atol=1e-9)
             else "背景保留(非零)"))
    stack = np.stack([d[f] for f in FRAMES])
    ptp = float(np.max(np.ptp(stack, axis=0)))
    print("  %-30s 帧间极差(接缝量级) = %s"
          % ("", "0 (无缝)" if ptp < 1e-9 else "%.4f ADU" % ptp))

print("\nkey arithmetic:")
print("  mode c on the REFERENCE frame: C_A == 0 => product_A = raw_A (整帧未动) => 背景不可能被剪掉")
print("  mode c median over all cells/frames = %.4f ADU"
      % float(np.median([v for f in FRAMES for v in prod["mode c  (raw - C - G)"][f]])))
print("  B_ref median = %.4f ADU ; 全减臂 median = %.4f ADU"
      % (float(np.median(B_ref)),
         float(np.median([v for f in FRAMES for v in prod["arm raw - b_k (全减, 无产路)"][f]]))))
print("  实验实测同号口径 (PHASE2_UPM.md:352-353): 保留 B_ref 臂产品中位 299.17 e- ≈ B_ref 297.33 e-;"
      " 全减臂 0.845 e-")
