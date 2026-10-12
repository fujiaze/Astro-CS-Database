"""结构保留自检：A 相对 raw（同单位）——电平保留、非结构区高通噪声。"""
import sys, json, os
import numpy as np
from astropy.io import fits
from scipy import ndimage

fitsdir, outdir = sys.argv[1], sys.argv[2]
res = {}
for arm in ("cleaned", "photoapplied"):
    fr, fa = os.path.join(fitsdir, f"p5z_{arm}_raw.fits"), os.path.join(fitsdir, f"p5z_{arm}_A.fits")
    if not (os.path.exists(fr) and os.path.exists(fa)):
        continue
    raw = fits.getdata(fr).astype(float); A = fits.getdata(fa).astype(float)
    m = np.isfinite(raw) & np.isfinite(A)
    sm = m & (raw > np.nanpercentile(raw[m], 99.9)); sm = ndimage.binary_dilation(sm, iterations=1)
    ratio = (A[sm] / raw[sm]); ratio = ratio[np.isfinite(ratio)]
    hp = lambda im: np.nan_to_num(im) - ndimage.median_filter(np.nan_to_num(im), size=3)
    quiet = m & ~sm
    res[arm] = {"n_struct_px": int(sm.sum()),
                "level_retention_median": float(np.median(ratio)),
                "level_retention_p05": float(np.percentile(ratio, 5)),
                "noise_highpass_rms_raw": float(np.sqrt(np.mean(hp(raw)[quiet] ** 2))),
                "noise_highpass_rms_A": float(np.sqrt(np.mean(hp(A)[quiet] ** 2)))}
os.makedirs(outdir, exist_ok=True)
json.dump(res, open(os.path.join(outdir, "p5z_structure_selfconsistent.json"), "w"), indent=1)
print(json.dumps(res, indent=1))
