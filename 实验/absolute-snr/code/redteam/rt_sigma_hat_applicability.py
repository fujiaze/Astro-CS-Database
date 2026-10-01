"""帧级 `sigma_sky` 估计器的适用域反例（逐条镜像生产实现的裁剪循环）。

不调用生产二进制、不编译；按 `lib/algorithms/star_detection/wrapper_phase1/star_detector.cpp`
中 `estimate_background` 的两轮 `median ± 3·1.4826·MAD` 裁剪 → 末轮 RMS 的语义
逐行重写（裁剪窗地板 `3.0·(s>0 ? s : 1e-9)` = `star_detector.cpp:108`，仍属现行行为）。

**末端分支是反例夹具，不是现行行为。** 本脚本复现的是 CLEAN-401 之前那版被退役的静默
置地板门 `if (!(*sigma > 0.0)) *sigma = 1e-9;`——生产码把该写法逐字判为静默降级
（见 `star_detector.cpp:129`），保留它只为把该门造成的 `SNR_frame` 高估量级测出来。
现行生产末端是 `if (!(*sigma > 0.0)) return false;`（`star_detector.cpp:139`）：σ == 0
与 NaN 同处置为**显式失败**，`detect()` 以 `ErrorDomain::DATA` fail-closed
（`star_detector.cpp:149-152`）⇒ 现行行为下该帧根本不产出 `SNR_frame`，也不存在
2×10¹⁰ 倍高估。口径正本见 `实验/absolute-snr/docs/frame-snr-canon.md` §2.7。

检验两件事：
  (A) 结构/噪声比 `r = sigma_struct / sigma_sky` 对 `sigma_hat` 对天光灵敏度
      （`d ln sigma_hat / d ln B`）的衰减——决定「sigma_hat 对天光单调」这条判据
      的定量成立域；
  (B) 常量（掩膜 / 零填充 / 过曝置零）像素占比对 `sigma_hat` 的影响，以及
      **若** `sigma_hat` 走那条被退役的静默置地板门，`SNR_frame` 会被高估的倍数。
输出只到 stdout，**不写 results/**。
"""
import numpy as np

MAD_TO_SIGMA = 1.482602218505602
NX = 256


def sigma_hat(img):
    """`StarDetector::estimate_background` 的两轮裁剪 + 末轮残差 RMS（逐条镜像）。

    末端的置地板分支是**反例夹具**：镜像的是被退役的静默降级写法；现行生产在该处
    直接 `return false`（`star_detector.cpp:139`）。改动此分支会改变 §2.7 的实测数字。
    """
    keep = np.asarray(img, float).copy()
    for _ in range(2):
        med = np.median(keep)
        mad = np.median(np.abs(keep - med))
        s = MAD_TO_SIGMA * mad
        keep = keep[np.abs(keep - med) <= 3.0 * (s if s > 0 else 1e-9)]
        if keep.size == 0:
            break
    bg = np.median(keep)
    sig = float(np.sqrt(np.sum((keep - bg) ** 2) / max(keep.size, 1)))
    if not (sig > 0.0):
        sig = 1e-9                       # 反例量级读法：复现已退役的静默置地板门（量级 1e-9 供 §2.7 的高估倍数换算）；现行 star_detector.cpp:139 为显式失败
    return sig


def sky_frame(background, structure_rms, seed=5):
    rng = np.random.default_rng(seed)
    img = rng.normal(background, np.sqrt(background + 100.0), (NX, NX))
    if structure_rms > 0:
        t = np.linspace(0.0, 2.0 * np.pi, NX)
        shape = np.sin(t)[None, :] * np.sin(2.0 * t)[:, None]
        img = img + structure_rms * shape / np.sqrt(np.mean(shape ** 2))
    return img


def structure_sweep():
    print("== A: sigma_hat 对天光的灵敏度 d ln(sigma_hat) / d ln(B) ==")
    print(f"  {'struct_rms':>10} {'sigma(1e3)':>11} {'sigma(1e4)':>11} "
          f"{'sigma(1e5)':>11} {'slope':>8} {'sigma/sig_true':>14} {'r=s/sigma_n':>13}")
    for s_rms in (0, 100, 300, 1000, 3000):
        row = [sigma_hat(sky_frame(b, s_rms)) for b in (1e3, 1e4, 1e5)]
        slope = (np.log(row[2]) - np.log(row[0])) / (np.log(1e5) - np.log(1e3))
        print(f"  {s_rms:10} {row[0]:11.2f} {row[1]:11.2f} {row[2]:11.2f} "
              f"{slope:8.4f} {row[1] / np.sqrt(1e4):14.3f} {s_rms / np.sqrt(1e4):13.2f}")


def constant_fraction_sweep():
    base = np.random.default_rng(0).normal(300.0, 20.0, (NX, NX))
    true_sigma = float(np.sqrt(np.mean((base - 300.0) ** 2)))
    print("\n== B: 常量（掩膜 / 零填充 / 过曝置零）像素占比 ==")
    print(f"  {'const_frac':>10} {'sigma_hat':>12} {'sigma/true':>12} {'SNR overestimate':>18}")
    for p in (0.0, 0.10, 0.30, 0.35, 0.38, 0.40, 0.50):
        img = base.copy().ravel()
        idx = np.random.default_rng(3).choice(img.size, int(round(p * img.size)),
                                              replace=False)
        img[idx] = 300.0
        sh = sigma_hat(img.reshape(NX, NX))
        print(f"  {p:10} {sh:12.4g} {sh / true_sigma:12.4g} "
              f"{1.0 / (sh / true_sigma):18.4g}")


if __name__ == "__main__":
    structure_sweep()
    constant_fraction_sweep()
