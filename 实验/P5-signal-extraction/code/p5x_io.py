"""P5 信号提取：只读产物 IO、逐帧原始数据还原、叶像素几何与覆盖率。

产物根目录给出联合加性模型（`p2_sky_plane.bin`）、逐帧**已校正**瓦片与其方差
（`p2_corrected*.bin`）、天光样点（`p2_samples.json`）、覆盖率与积分数组
（`p2_integrated_*.bin`）。本模块把这些装进统一的并集网格：

  · 并集网格 = `p2_integrated.json` 的 40 个 order-12 瓦片（每瓦片 512²，叶像素 nside 2²¹）
  · 每帧的紧凑瓦片列表（26/26/26/25）映射到并集网格，帧外为 NaN
  · 原始数据由 `raw_k = corrected_k + δ_k` 还原，δ_k = δ_poly,k(u,v) + F_k(u,v)
    （生产施加语义 `corrected = raw − δ_k` 的逆；δ_poly 取 `deltas[k]`，F_k 取逐帧 own 网格双线性）
"""

from __future__ import annotations

import json
import os

import numpy as np

TILE = 512
LEAF_PER_TILE = TILE * TILE
LEAF_NSIDE = 1 << 21          # HiPS 叶像素 nside（signal/properties 的 hips_order=12 + 9）
TILE_SHIFT = 18               # 每瓦片叶像素数 = 4^9


# ---------------------------------------------------------------- 基础
def read_bin(path, dtype=np.float32):
    return np.fromfile(path, dtype=dtype)


def gnomonic(ra0, dec0, ra, dec):
    """球面 → 切平面 (u, v)（度），与生产信号面同一投影约定。"""
    ra = np.radians(ra)
    dec = np.radians(dec)
    ra0 = np.radians(ra0)
    dec0 = np.radians(dec0)
    cosc = np.sin(dec0) * np.sin(dec) + np.cos(dec0) * np.cos(dec) * np.cos(ra - ra0)
    u = np.degrees(np.cos(dec) * np.sin(ra - ra0) / cosc)
    v = np.degrees((np.cos(dec0) * np.sin(dec) -
                    np.sin(dec0) * np.cos(dec) * np.cos(ra - ra0)) / cosc)
    return u, v


def delta_basis(u, v, uc, vc, us, vs, order):
    """逐帧低阶项基（与生产 `delta_order` 同族），列序按总阶枚举：order=1 → [1, η, ξ]。"""
    xi = (np.asarray(u, dtype=float) - uc) / us
    eta = (np.asarray(v, dtype=float) - vc) / vs
    cols = []
    for total in range(order + 1):
        for a in range(total + 1):
            b = total - a
            cols.append(xi ** a * eta ** b)
    return np.stack(cols, axis=1)


def eval_own_grid(nu, nv, step, u0, v0, vals, u, v):
    """逐帧 own 网格双线性求值；布局 idx = iv*nu + iu（u 最快）。

    索引越界时钳到最近格点（边界值外延），与正本「格点必须平滑外延、不得留空判边界」一致；
    只有格点值本身非有限时才返回 NaN。"""
    g = np.asarray(vals, dtype=float).reshape(nv, nu)
    fu = (np.asarray(u, dtype=float) - u0) / step
    fv = (np.asarray(v, dtype=float) - v0) / step
    iu = np.floor(fu).astype(np.int64)
    iv = np.floor(fv).astype(np.int64)
    iu0, iv0 = iu.copy(), iv.copy()
    ok = (iu0 >= 0) & (iu0 < nu - 1) & (iv0 >= 0) & (iv0 < nv - 1)
    iu = np.clip(iu, 0, nu - 2)
    iv = np.clip(iv, 0, nv - 2)
    tu = fu - iu
    tv = fv - iv
    g00 = g[iv, iu]
    g10 = g[iv, iu + 1]
    g01 = g[iv + 1, iu]
    g11 = g[iv + 1, iu + 1]
    val = ((1 - tv) * ((1 - tu) * g00 + tu * g10) +
           tv * ((1 - tu) * g01 + tu * g11))
    finite = np.isfinite(g00) & np.isfinite(g10) & np.isfinite(g01) & np.isfinite(g11)
    # 网格外按边界值外延（正本要求平滑外延，不得留空判边界）
    return np.where(finite, val, np.nan)


# ---------------------------------------------------------------- 产物装载
class Product:
    """一个 mosaic 产物目录在并集网格上的全部数组。"""

    def __init__(self, root):
        self.root = os.path.abspath(root)
        self.sp = json.load(open(os.path.join(self.root, "p2_sky_plane.bin")))
        self.integ = json.load(open(os.path.join(self.root, "p2_integrated.json")))
        self.corr = json.load(open(os.path.join(self.root, "p2_corrected.json")))
        self.samples = json.load(open(os.path.join(self.root, "p2_samples.json")))
        self.tiles = [int(t["tile_ipix"]) for t in self.integ["tiles"]]
        self.npix = len(self.tiles) * LEAF_PER_TILE
        self.tile_index = {tp: i for i, tp in enumerate(self.tiles)}
        self.frame_ids = [int(f) for f in self.sp["frame_ids"]]
        self.nframe = len(self.frame_ids)
        self._load_frames()
        self._geometry()
        self._delta()

    # -- 逐帧已校正瓦片与方差
    def _load_frames(self):
        nf = self.nframe
        self.corrected = np.full((nf, self.npix), np.nan, dtype=np.float64)
        self.var = np.full((nf, self.npix), np.nan, dtype=np.float64)
        for k, fr in enumerate(self.corr["frames"]):
            dat = read_bin(fr["data_file"], np.float64)
            var = read_bin(fr["var_file"], np.float64)
            for t in fr["tiles"]:
                tp = int(t["tile_ipix"])
                j = self.tile_index.get(tp)
                if j is None:
                    continue
                o = int(t["offset"])
                n = int(t["n_pixels"])
                sl = slice(j * LEAF_PER_TILE, (j + 1) * LEAF_PER_TILE)
                self.corrected[k, sl] = dat[o:o + n]
                self.var[k, sl] = var[o:o + n]
        # 合法样本：有限值 + 正方差
        self.valid = np.isfinite(self.corrected) & np.isfinite(self.var) & (self.var > 0)
        self.corrected = np.where(self.valid, self.corrected, np.nan)
        self.var = np.where(self.valid, self.var, np.nan)
        self.ncover = self.valid.sum(axis=0).astype(np.int16)
        # 覆盖帧集差异用的位掩码
        self.covmask = np.zeros(self.npix, dtype=np.int32)
        for k in range(self.nframe):
            self.covmask |= (self.valid[k].astype(np.int32) << k)

    # -- 叶像素几何（ra/dec → u/v）
    def _geometry(self):
        try:
            import healpy as hp
        except Exception as exc:  # pragma: no cover
            raise RuntimeError("需要 healpy 计算叶像素几何") from exc
        idx = np.empty(self.npix, dtype=np.int64)
        for j, tp in enumerate(self.tiles):
            base = tp * (1 << TILE_SHIFT)
            idx[j * LEAF_PER_TILE:(j + 1) * LEAF_PER_TILE] = base + np.arange(LEAF_PER_TILE)
        self.leaf_nest = idx
        ra, dec = hp.pix2ang(LEAF_NSIDE, idx, nest=True, lonlat=True)
        self.ra, self.dec = np.asarray(ra), np.asarray(dec)
        self.u, self.v = gnomonic(self.sp["ra0_deg"], self.sp["dec0_deg"], self.ra, self.dec)
        self.tile_of = np.repeat(np.arange(len(self.tiles)), LEAF_PER_TILE)
        self.local_of = np.tile(np.arange(LEAF_PER_TILE), len(self.tiles))
        # 瓦片内 2D 邻接（上下左右）
        r = (self.local_of // TILE).astype(np.int32)
        c = (self.local_of % TILE).astype(np.int32)
        self.row, self.col = r, c

    # -- δ_k 求值（δ_poly,k + F_k）
    def _delta(self):
        sp = self.sp
        Db = delta_basis(self.u, self.v, sp["uc"], sp["vc"], sp["us"], sp["vs"],
                         int(sp["delta_order"]))
        self.delta = np.zeros((self.nframe, self.npix), dtype=np.float64)
        for k in range(self.nframe):
            d = Db @ np.asarray(sp["deltas"][k], dtype=float)
            if sp.get("own_enabled") and sp.get("own"):
                g = sp["own"][k]
                vals = np.asarray(g["vals"], dtype=float)
                d = d + eval_own_grid(int(g["nu"]), int(g["nv"]), float(g["step"]),
                                      float(g["u0"]), float(g["v0"]), vals, self.u, self.v)
            self.delta[k] = d
        self.raw = self.corrected + self.delta
        self.raw = np.where(self.valid, self.raw, np.nan)

    # -- 积分侧数组（生产成品）
    def integrated(self):
        """生产成品：signal/support/wsum 为 float64，nused/nrej 为 int32（按文件尺寸定 dtype）。"""
        f = self.integ["files"]
        n = self.npix
        out = {}
        for k, path in f.items():
            sz = os.path.getsize(path)
            if sz == 8 * n:
                out[k] = read_bin(path, np.float64)
            elif sz == 4 * n:
                out[k] = np.fromfile(path, dtype=np.int32)
        return out

    # -- 天光样点的模型残差自检
    def sample_check(self):
        sp = self.sp
        s = self.samples
        obs = s["observations"]
        fid = {f: i for i, f in enumerate(self.frame_ids)}
        k = np.array([fid[int(o["frame_id"])] for o in obs], dtype=np.int64)
        u, v = gnomonic(sp["ra0_deg"], sp["dec0_deg"],
                        np.array([o["ra_deg"] for o in obs]),
                        np.array([o["dec_deg"] for o in obs]))
        val = np.array([o["value"] for o in obs], dtype=float)
        w = np.array([o["control_ivar"] for o in obs], dtype=float)
        Db = delta_basis(u, v, sp["uc"], sp["vc"], sp["us"], sp["vs"], int(sp["delta_order"]))
        model = np.zeros_like(val)
        for kk in range(self.nframe):
            m = k == kk
            if not m.any():
                continue
            d = Db[m] @ np.asarray(sp["deltas"][kk], dtype=float)
            if sp.get("own_enabled") and sp.get("own"):
                g = sp["own"][kk]
                d = d + eval_own_grid(int(g["nu"]), int(g["nv"]), float(g["step"]),
                                      float(g["u0"]), float(g["v0"]),
                                      np.asarray(g["vals"], dtype=float), u[m], v[m])
            model[m] = d
        r = val - model
        ok = np.isfinite(r)
        rms_w = float(np.sqrt(np.sum(w[ok] * r[ok] ** 2) / np.sum(w[ok])))
        rms_u = float(np.sqrt(np.mean(r[ok] ** 2)))
        info = sp.get("info", {})
        return {"rms_delta_only_unweighted": rms_u, "rms_delta_only_weighted": rms_w,
                "n_obs": int(ok.sum()),
                "product_info_rms_unweighted": info.get("rms_unweighted"),
                "product_info_rms_weighted": info.get("rms_weighted")}

    # -- 结构掩膜：由**未校正**堆栈冻结，供各路径共用
    def structure_mask(self, stack_raw, q=0.99, win=9, dilate=2):
        from scipy import ndimage
        hi = np.nanpercentile(stack_raw, q * 100)
        m = np.isfinite(stack_raw) & (stack_raw > hi)
        m = ndimage.binary_dilation(m, iterations=dilate)
        return m


def load(root):
    return Product(root)
