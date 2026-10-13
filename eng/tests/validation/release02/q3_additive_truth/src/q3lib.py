"""Q3 ADDITIVE-TRUTH shared library.
Frame registry + solved-geometry loading (**from the product**: per-frame HiPS
manifest's additive `geometry`; the intermediate `p1_wcs.json` lives in the run's
workspace, not in the product face) + bilinear sampling.
Read-only: never writes outside run/RELEASE-02/q3-additive-truth/.
"""
import glob, json, os, re
import numpy as np
from astropy.io import fits
from astropy.wcs import WCS
from astropy.wcs import Sip

REPO = '/workspace/Astro CS Database'
L4 = os.path.join(REPO, 'run/RELEASE-02/L4-rebuild')

def registry():
    """帧集来自**产品清单** `p1_products.json` 的 `frames[]`（不再枚举 `calibrated_*.fts`）。

    来源口径：
      · 几何与观测元数据取自 `frames[].input_light` 的**头部**（原始输入亮场）。
        **为何不取自产品**：产品侧不承载几何——`frames[]` 无 `crval/crpix/cd/wcs` 键，
        HiPS `properties` 无 `CRVAL`，tile 头只有 `NSIDE`。
        **为何可以这样取**：`calibrated_*.fts` 头部 WCS 与 `input_light` 头部 WCS
        **逐位相同**（4/4 帧实测）⇒ 换来源数值不变；且 `input_light` 是输入，
        不是中间产物、不是调试输出。
      · 帧键有**两套拼写**：`input_light` basename 保 `@`（`…-20251212@020002-…`），
        `frame_id` 用 `_`（`…-20251212_020002-…`）。正向 `stem.replace('@','_')` 安全；
        **反向严禁**对 `frame_id` 朴素 `replace('_','@')`（`frame_id` 含多个 `_`，
        会过度替换）⇒ 反查**必须**经 `frames[].input_light`。
    """
    out = []
    for pj in sorted(glob.glob(os.path.join(L4, 'norm', '*', 'p1_products.json'))):
        grp = os.path.basename(os.path.dirname(pj))
        prods = json.load(open(pj))
        # 中间产物（p1_phot/p1_flux 等）落在**工作区**，其实际路径记在运行级记账面
        # <out>/run_context.json#work_dir（位置可被配置键 work_dir 覆盖 ⇒ **不得**
        # 自行推导 <out 基名>.p1-work）。读取该字段即"从记录读"。
        # **要求 keep_intermediates=true**（缺省即 true）；显式关掉后这些中间产物
        # 在运行收尾被清理 ⇒ 本脚本不可用。
        work_dir = load_work_dir(os.path.dirname(pj))
        for fr in prods.get('frames', []):
            light = fr['input_light']
            stem = os.path.basename(light).rsplit('.', 1)[0]   # '@' 形态（权威来源）
            key = fr['frame_id']                               # '_' 形态
            fdir = fr['hips_path']
            m = re.match(r'M42_(M\d)_(T\d)_flying_dutchman-(\d{8})@(\d{6})', stem)
            panel, tel, date, tm = m.groups()
            h = fits.getheader(light)                          # 输入亮场头（EXPTIME/AIRMASS/DATE-OBS）
            # 几何/SIP **取自产品**（AGENTS §13.1）：逐帧 HiPS 产品集清单
            # <fdir>/manifest.json 的加性 `geometry` 对象（口径正本 =
            # docs/engineering/contracts/HIPS_STORAGE_FORM.md）。原先读
            # <fdir>/p1_wcs.json —— 该文件是中间产物，已移入工作区，产品面内不再存在。
            geo = load_geometry(fdir)
            out.append(dict(path=light, group=grp, stem=stem, key=key, fdir=fdir,
                            panel=panel, tel=tel, date=date, time=tm,
                            geometry_json=os.path.join(fdir, 'manifest.json'),
                            work_dir=work_dir,
                            phot_json=os.path.join(work_dir, 'p1_phot.json'),
                            flux_json=os.path.join(work_dir, 'p1_flux.json'),
                            crval1=geo['crval1'], crval2=geo['crval2'],
                            exptime=h.get('EXPTIME'), airmass=h.get('AIRMASS'),
                            dateobs=h.get('DATE-OBS')))
    return out

def load_work_dir(out_dir):
    """中间产物工作区的**实际路径**：从运行级记账面 <out_dir>/run_context.json 的
    `work_dir` 字段读（阶段一 CLI 写入；唯一实现 = acsd::core::resolve_work_dir）。
    **不自行推导**命名规则——位置可被配置键 work_dir 覆盖。
    **这些中间产物要求 keep_intermediates=true**（缺省即 true）。"""
    with open(os.path.join(out_dir, 'run_context.json')) as f:
        rc = json.load(f)
    w = rc.get('work_dir')
    if not w:
        raise KeyError('run_context.json 无 work_dir 字段（本运行未记录工作区；'
                       '该运行可能早于工作区落地或非阶段一）: ' + out_dir)
    return w

def load_geometry(fdir):
    """**几何取自产品**（AGENTS §13.1）：读逐帧 HiPS 产品集清单的加性 `geometry`
    对象（<fdir>/manifest.json）。这是解算几何（含 SIP）在产品面内的**唯一承载点**；
    中间产物 <fdir>/p1_wcs.json 已移入工作区、产品面内不再存在。
    口径正本 = docs/engineering/contracts/HIPS_STORAGE_FORM.md
    「逐帧 HiPS 产品集清单 manifest.json#geometry（加性）」一节。"""
    with open(os.path.join(fdir, 'manifest.json')) as f:
        m = json.load(f)
    g = m['geometry']                      # 加性对象；缺失即该帧未承载几何（fail-closed）
    return g

def load_wcs(frame):
    """Build astropy WCS (TAN-SIP) from the product geometry
    (<fdir>/manifest.json#geometry). 0-based array index in/out:
    all_world2pix returns 0-based (we subtract 1 from FITS xp)."""
    w = load_geometry(frame['fdir'])
    hdr = {}
    hdr['CTYPE1'] = w['ctype1']; hdr['CTYPE2'] = w['ctype2']
    hdr['CRPIX1'] = w['crpix1']; hdr['CRPIX2'] = w['crpix2']
    hdr['CRVAL1'] = w['crval1']; hdr['CRVAL2'] = w['crval2']
    hdr['CD1_1'] = w['cd11']; hdr['CD1_2'] = w['cd12']
    hdr['CD2_1'] = w['cd21']; hdr['CD2_2'] = w['cd22']
    hdr['NAXIS'] = 2; hdr['NAXIS1'] = 4096; hdr['NAXIS2'] = 4096
    hdr['RADESYS'] = 'ICRS'
    W = WCS(hdr)
    if w.get('sip'):
        s = w['sip']
        def _sq(v):
            a = np.array(v, dtype=np.float64)
            n = int(round(len(a) ** 0.5))
            return a.reshape(n, n)          # dense [p][q], p,q in 0..n-1
        W.sip = Sip(_sq(s['a']), _sq(s['b']), _sq(s['ap']), _sq(s['bp']), W.wcs.crpix)
    return W

def pix_to_sky(W, x0, y0):
    """x0,y0 = 0-based array indices. Returns RA,Dec deg."""
    ra, dec = W.all_pix2world(np.asarray(x0, dtype=np.float64),
                              np.asarray(y0, dtype=np.float64), 0)
    return ra, dec

def sky_to_pix(W, ra, dec):
    """Returns 0-based x,y array indices."""
    x, y = W.all_world2pix(np.asarray(ra, dtype=np.float64),
                           np.asarray(dec, dtype=np.float64), 0)
    return x, y

def sample_bilinear(arr, x, y):
    """Bilinear sample of 2-D array at 0-based float coords (arrays).
    Out-of-range -> NaN."""
    x = np.asarray(x, dtype=np.float64); y = np.asarray(y, dtype=np.float64)
    x0 = np.floor(x).astype(np.int64); y0 = np.floor(y).astype(np.int64)
    fx = x - x0; fy = y - y0
    H, Wd = arr.shape
    ok = (x0 >= 0) & (y0 >= 0) & (x0 < Wd - 1) & (y0 < H - 1)
    x0c = np.clip(x0, 0, Wd - 2); y0c = np.clip(y0, 0, H - 2)
    a = np.asarray(arr[y0c, x0c], dtype=np.float64)
    b = np.asarray(arr[y0c, x0c + 1], dtype=np.float64)
    c = np.asarray(arr[y0c + 1, x0c], dtype=np.float64)
    d = np.asarray(arr[y0c + 1, x0c + 1], dtype=np.float64)
    v = (a * (1 - fx) * (1 - fy) + b * fx * (1 - fy) + c * (1 - fx) * fy + d * fx * fy)
    v = np.where(ok, v, np.nan)
    return v

def load_flux(frame):
    """Per-frame source catalog (x,y,flux,snr,background) from p1_flux.json.

    p1_flux.json 是**中间产物**（工作区，非产品面）⇒ 路径取自运行级记账面记录的
    work_dir（`frame['flux_json']`），**要求 keep_intermediates=true**（缺省即 true）。
    订正：旧实现拼的是 `dirname(frame['path'])`，而 `frame['path']` 是**输入亮场**的
    路径 ⇒ 那个拼接本来指向输入数据目录（从来取不到 p1_flux.json）。"""
    d = json.load(open(frame['flux_json']))
    for fr in d['frames']:
        if fr['file'].replace('cleaned_', '').replace('.fts', '') == frame['stem']:
            res = fr['results']
            n = len(res)
            x = np.array([r['x'] for r in res], dtype=np.float64)
            y = np.array([r['y'] for r in res], dtype=np.float64)
            flux = np.array([r['flux'] for r in res], dtype=np.float64)
            snr = np.array([r['snr'] for r in res], dtype=np.float64)
            valid = np.array([r['valid'] for r in res], dtype=bool)
            bg = np.array([r['background'] for r in res], dtype=np.float64)
            return dict(x=x, y=y, flux=flux, snr=snr, valid=valid, background=bg)
    raise KeyError('frame not in flux json: ' + frame['stem'])
