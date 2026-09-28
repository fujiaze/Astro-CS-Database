#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""EXP-11（P2-M5 闭环）：**冻结默认重建算子**在**真实控制点网格**上的传递检查。

对抗审查 finding P2-M5
（run/FINAL-07/审核包/科研审查/SCI-702_P2_跨帧绝对SNR_审查报告.md:215）
指出 exp04_refmag_chain.py 的 H3「控制点精度约定（1.5%）完整穿过 P4 重建接口」有三处削弱：
  ① 它用了 IDW(power=2,k=8) —— 既不是冻结默认算子 natural_bicubic_spline_clip_v1，
     也不在冻结词表内（docs/plugins/algorithms_phase1/07_noise_snr.md §4.5、
     eng/contracts/schemas/unified/sparse_snr_layer.schema.json）；
  ② 无噪声臂 IDW 自身的离散化相对 RMS 已达 0.2287 ⇒ 1.5% 噪声只贡献 1.06× 通胀，
     该读数主要由**算子离散化**决定，而不是由控制点噪声决定；
  ③ 控制点仅 64 个（512²/Δ64 合成场），未覆盖生产域（真实帧 4096²、Δ=64 ⇒ 4096 控制点）。

本脚本是 P2-M5 的唯一闭环动作：**不重写任何重建算子**——全部逐点重建由生产实现
astrocs::v6::p2weight::SparseSnrReconstructor / reconstruct_sparse_snr
（lib/algorithms/integration/phase2_integrate/src/weight_chain.cpp，只读编译链接为独立驱动）
给出；IDW 只作为**代理算子对照行**保留在 H1，用于解释 exp04 那个 0.2287 / 1.06× 的出处。

  H0  驱动完整性：分带 σ 场逐位等价、常数网格归零负例、真值估计器相对标准误（MC 实测）
  H1  exp04 同几何：IDW 代理复现（度量定义可比性门）+ 冻结算子同网格读数
  H2  真实 M42 帧 × 真实生产控制网格（4096²；Δ=64 冻结默认 4096 控制点、Δ=32 加密 16384）
      × 4 个冻结 token × {无噪, +1.5% 乘性, +1.5% 加性}
      × {同族真值, 1px 棋盘 hold-out 真值（主帧）} + 正齐次性检查
  H3  fail-closed 负例（越界求值 / 未知 token（含 IDW token）/ 最近点算子用在规则网格 /
      非正控制值 / 角点锚定网格 / nx<2 / 全 NaN 网格 / 部分 NaN 填充正例）
  H4  值域钳制核验（钳制边界是否真正触发、能否单独关闭）
  H5  脉冲权重范数 Σ_k w_k(q)²（由生产算子对单位脉冲的响应直接测出）+ 解析闭式门
      + 线性叠加检验（真网格 1024² 全节点窗口）

固定 seed 20260926。成本纪律：单轮墙钟 < 900 s（实测约 420 s），全程 < 4 GB RSS。
复现（重计算必须套内存看门狗，禁止外层 timeout）：
  bash 实验/absolute-snr/code/exp11_build_driver.sh
  python3 eng/tools/monitoring/mem_guard.py --max-rss-gb 4 --timeout 900 -- \
      python3 实验/absolute-snr/code/audit/route3/exp11_frozen_operator_transfer.py
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
import struct
import subprocess
import sys
import time

import numpy as np

_HERE = os.path.dirname(os.path.abspath(__file__))
_UNIT_CODE = os.path.abspath(os.path.join(_HERE, "..", ".."))        # 实验/absolute-snr/code
_UNIT = os.path.abspath(os.path.join(_HERE, "..", "..", ".."))       # 实验/absolute-snr
_REPO = os.path.abspath(os.path.join(_UNIT, "..", ".."))             # 仓库根
sys.path.insert(0, _UNIT_CODE)
import sci_b_common as C  # noqa: E402  （实验侧公共模块，不是 lib/ 生产代码）

SEED = 20260926
CTRL_NOISE_REL = 0.015            # P-CST-08 控制点精度约定（SE/sigma @ N_sky=9216）
P_TRUTH = 16                      # 稠密参考 σ 场的 patch 边长 [px]
DELTA_LIST = [64, 32]             # 64 = sparse_snr.spacing_px 冻结默认，32 = 加密对照
FRAME_SIZE = 4096                 # 生产帧尺寸（testdata M42 Red 300 s）
Q_STRIDE_H5A = 4                  # H5a 查询点抽样步长（512² 像素 → 128²=16384 点）
H5_WINDOW_N = 16                  # H5b 真实帧脉冲窗口 = 16×16 个 Δ=64 单元（1024² px）
NOISE_REALIZATIONS = 8            # H5a 多实现加性噪声增益的重复数
M42 = [os.path.join(C.TESTDATA, p) for p in [
    "M42_T2T3_mosaic_Flying_dutchman/T2/M1/M42_M1_T2_flying_dutchman-20251212@012404-300S-Red.fts",
    "M42_T2T3_mosaic_Flying_dutchman/T2/M2/M42_M2_T2_flying_dutchman-20251212@020002-300S-Red.fts",
    "M42_T2T3_mosaic_Flying_dutchman/T2/M4/M42_M4_T2_flying_dutchman-20251224@045919-300S-Red.fts"]]

OP_SPLINE = "natural_bicubic_spline_clip_v1"
OP_MESH = "natural_bicubic_spline_clip_mesh_median_v1"
OP_BILINEAR = "bilinear_regular_grid_v1"
OP_NEAREST = "nearest_control_point_v1"
REGULAR_TOKENS = [OP_SPLINE, OP_MESH, OP_BILINEAR]
ALL_TOKENS = REGULAR_TOKENS + [OP_NEAREST]

PROD_SRC = os.path.join(_REPO, "lib", "algorithms", "integration", "phase2_integrate")
EXP04_JSON = os.path.join(_HERE, "..", "results", "route3", "exp04_refmag_chain.json")
OUT_JSON = os.path.join(_HERE, "..", "results", "route3", "exp11_frozen_operator_transfer.json")
EVID = os.path.join(_REPO, "run", "FINAL-07", "审核包", "科研审查", "P2_订正", "evidence", "prod")
DRIVER = os.path.join(EVID, "exp11_recon_driver")
BUILD_SH = os.path.join(_UNIT_CODE, "exp11_build_driver.sh")
WORK = os.path.join(EVID, "exp11_work")
MAX_RADIUS_PX = 100.0             # 散点模式半径：覆盖 Δ=64 半 cell 对角 45.3 px

_T0 = time.perf_counter()


def log(msg):
    print("[%6.1fs] %s" % (time.perf_counter() - _T0, msg), flush=True)


# ---------------------------------------------------------------------------
# σ 场（真实帧稠密参考 / 控制点值）——与 B3/EXP-205 同一起始估计器
# ---------------------------------------------------------------------------
def sigma_field_banded(img, P, band_cells=16):
    """按 cell 行分带调用 sci_b_common.sigma_field_fast（逐 cell 独立 ⇒ 与整幅逐位等价）。

    分带的唯一目的是把 4096² 单次 reshape 的峰值内存压到百 MB 级；数学上完全一致，
    H0 内有逐位等价性门（含 NaN 与非 NaN 两种输入）。
    """
    h, w = img.shape
    ny, nx = h // P, w // P
    out = np.empty((ny, nx), np.float64)
    for j0 in range(0, ny, band_cells):
        j1 = min(j0 + band_cells, ny)
        out[j0:j1, :] = C.sigma_field_fast(img[j0 * P:j1 * P, :nx * P], P)
    return out


def cell_centers(n, d):
    """生产几何：cell i 覆盖像素 [i*d, (i+1)*d-1]，控制点（节点）在 cell 中心 i*d+(d-1)/2。"""
    return np.arange(n, dtype=np.float64) * d + (d - 1.0) / 2.0


def grid_queries(nx, ny, dx, dy, origin=0.0):
    gx, gy = np.meshgrid(cell_centers(nx, dx) + origin, cell_centers(ny, dy) + origin)
    return np.stack([gx.ravel(), gy.ravel()], axis=1)


def dense_queries(n, P):
    """P×P cell 中心查询点（稠密参考分辨率）⇒ 返回 (n²,2)（行主序，与 σ 场 ravel 一致）。"""
    gx, gy = np.meshgrid(cell_centers(n, P), cell_centers(n, P))
    return np.stack([gx.ravel(), gy.ravel()], axis=1)


def flat_ctrl(field):
    return np.ascontiguousarray(field, np.float64).ravel()


# ---------------------------------------------------------------------------
# IDW 代理算子（exp04 逐字版 + 向量化版，后者先与前者逐点对齐再用于全幅）
# ---------------------------------------------------------------------------
def idw_reconstruct(pts, v, grid_xx, grid_yy, power=2.0, k=8):
    """IDW 代理算子：逐字取自 route3/exp04_refmag_chain.py（不做任何"改进"）。"""
    out = np.empty(grid_xx.shape)
    flat_g = np.stack([grid_xx.ravel(), grid_yy.ravel()], axis=1)
    for i, g in enumerate(flat_g):
        d2 = ((pts - g) ** 2).sum(axis=1)
        order = np.argsort(d2)[:k]
        w = 1.0 / np.maximum(d2[order], 1e-12) ** (power / 2.0)
        out.ravel()[i] = float((w * v[order]).sum() / w.sum())
    return out


def idw_reconstruct_vec(pts, v, qs, power=2.0, k=8, chunk=16384):
    """同定义的向量化实现（仅用于全幅 512² 复算；正确性由与逐字版的逐点比对锁定）。"""
    out = np.empty(qs.shape[0], np.float64)
    for s in range(0, qs.shape[0], chunk):
        e = min(s + chunk, qs.shape[0])
        qq = qs[s:e]
        d2 = ((qq[:, None, :] - pts[None, :, :]) ** 2).sum(axis=2)
        order = np.argsort(d2, axis=1)[:, :k]
        dd = np.take_along_axis(d2, order, axis=1)
        w = 1.0 / np.maximum(dd, 1e-12) ** (power / 2.0)
        out[s:e] = (w * v[order]).sum(axis=1) / w.sum(axis=1)
    return out


# ---------------------------------------------------------------------------
# 生产驱动封装（规格见 exp11_recon_driver.cpp 头部注释）
# ---------------------------------------------------------------------------
def write_spec(path, token, declare, regular, nx, ny, dx, dy, origin, values,
               queries, max_radius=-1.0, impulse_eps=0.0, impulse_count=0,
               pts=None, grid_origin=None, cell_center_offset=1):
    # origin：标量或 (ox, oy)。网格定义域 = [ox-0.5, ox+nx*dx-0.5] × [oy-0.5, oy+ny*dy-0.5]，
    # 控制点（节点）在 cell 中心 ox + (dx-1)/2 + i*dx（cell_center_offset=1）。
    ox, oy = (origin if isinstance(origin, (tuple, list)) else (origin, origin))
    if grid_origin is not None:
        ox = oy = grid_origin
    with open(path, "wb") as f:
        f.write(b"E11SP1\x00\x00")
        f.write(struct.pack("<ii", 1 if regular else 0, 1 if declare else 0))
        f.write((token or "").encode("utf-8").ljust(64, b"\x00")[:64])
        f.write(struct.pack("<ii", int(nx), int(ny)))
        dxc = (dx - 1.0) / 2.0 if cell_center_offset else 0.0
        dyc = (dy - 1.0) / 2.0 if cell_center_offset else 0.0
        f.write(struct.pack("<8d", ox + dxc, oy + dyc, float(dx), float(dy), float(ox),
                            float(oy), float(max_radius), 1e-6))
        v = np.asarray(values, np.float64).ravel()
        q = np.asarray(queries, np.float64).reshape(-1, 2)
        f.write(struct.pack("<qqq", v.size, 0 if regular else 1, q.shape[0]))
        f.write(struct.pack("<dq", float(impulse_eps), int(impulse_count)))
        if regular:
            f.write(v.astype("<f8").tobytes())
        else:
            p = np.asarray(pts, np.float64).reshape(-1, 2)
            f.write(np.stack([p[:, 0], p[:, 1], v], axis=1).astype("<f8").tobytes())
        f.write(q.astype("<f8").tobytes())


def run_driver(tag, **kw):
    """跑一次生产驱动 ⇒ (诊断字典, 逐点值 | None, 逐点成功掩膜 | None)。rc=3 表示 prepare 拒绝。"""
    os.makedirs(WORK, exist_ok=True)
    spec = os.path.join(WORK, tag + ".spec")
    out = os.path.join(WORK, tag + ".bin")
    write_spec(spec, **kw)
    p = subprocess.run([DRIVER, spec, out], capture_output=True, text=True)
    diag = {}
    for line in p.stdout.splitlines():
        k, _, val = line.partition(" ")
        diag[k] = val
    if p.returncode not in (0, 3):
        raise RuntimeError("driver rc=%d tag=%s stderr=%s" % (p.returncode, tag, p.stderr[:300]))
    if p.returncode == 3:
        os.remove(spec)
        return diag, None, None
    vals = np.fromfile(out, dtype="<f8")
    mask = np.fromfile(out + ".mask", dtype=np.uint8).astype(bool)
    for f in (spec, out, out + ".mask"):
        os.remove(f)
    return diag, vals, mask


def _f(diag, k, default=float("nan")):
    try:
        return float(diag.get(k, default))
    except (TypeError, ValueError):
        return default


def prod_diag(diag):
    return dict(operator_id=diag.get("operator_id", ""),
                clipped=int(_f(diag, "clipped", 0)),
                clip_low=_f(diag, "clip_low"), clip_high=_f(diag, "clip_high"),
                mesh_median=int(_f(diag, "mesh_median", 0)),
                n_filled=int(_f(diag, "n_filled", 0)),
                cell_center_offset=_f(diag, "cell_center_offset"),
                node_residual=_f(diag, "node_residual"),
                n_ok=int(_f(diag, "n_ok", 0)), n_err=int(_f(diag, "n_err", 0)),
                n_out_of_domain=int(_f(diag, "n_out_of_domain", 0)),
                min_v=_f(diag, "min_v"), max_v=_f(diag, "max_v"), mean_v=_f(diag, "mean_v"))


def metrics(v_clean, v_noisy, ref, m_clean, m_noisy, ctrl_rel=None):
    """相对 RMS 度量定义（与 exp04 H3 同名同义；H1 的 IDW 复现门锁定可比性）。

    disc_rel_rms  = RMS(R_clean - truth)/std(truth)     ← 网格+算子（+真值估计噪声）
    noise_rel_rms = RMS(R_noisy - R_clean)/mean(R_clean) ← 与真值无关
    gain          = noise_rel_rms / 0.015                ← 控制点 1.5% 噪声的传递增益
    inflation     = RMS(R_noisy - truth)/RMS(R_clean - truth)
    """
    m = m_clean & m_noisy & np.isfinite(ref) & np.isfinite(v_clean) & np.isfinite(v_noisy)
    n = int(m.sum())
    if n == 0:
        return dict(n=0)
    r, v0, v1 = ref[m], v_clean[m], v_noisy[m]
    sd = float(np.std(r))
    lvl = float(np.mean(v0))
    e0 = float(np.sqrt(np.mean((v0 - r) ** 2)))
    e1 = float(np.sqrt(np.mean((v1 - r) ** 2)))
    dn = float(np.sqrt(np.mean((v1 - v0) ** 2)))
    nr = dn / lvl if lvl > 0 else float("nan")
    out = dict(n=n, disc_rel_rms=(e0 / sd if sd > 0 else float("nan")),
               disc_rel_mean=(e0 / lvl if lvl > 0 else float("nan")),
               noise_rel_rms=nr, noise_abs_rms=dn,
               gain=(nr / CTRL_NOISE_REL if lvl > 0 else float("nan")),
               inflation_ratio=(e1 / e0 if e0 > 0 else float("nan")),
               rms_clean_vs_truth=e0, rms_noisy_vs_truth=e1, mean_recon=lvl, std_ref=sd)
    if ctrl_rel is not None and ctrl_rel > 0:
        out["ctrl_noise_rel_actual"] = ctrl_rel
        out["gain_selfnormalized"] = nr / ctrl_rel
    return out


def _nan_mean(x):
    x = np.asarray(x, float)
    m = np.isfinite(x)
    return float(np.mean(x[m])) if m.any() else float("nan")


def _nan_rms(x):
    x = np.asarray(x, float)
    m = np.isfinite(x)
    return float(np.sqrt(np.mean(x[m] ** 2))) if m.any() else float("nan")


def chk(tag, res):
    """驱动调用自诊断：prepare 被拒时必须带 tag 与生产错误文本抛出，不静默变 NaN。"""
    d, a, m = res
    if m is None:
        raise RuntimeError("driver prepare failed [%s]: %s" % (tag, d.get("prepare_err", "?")))
    return res


def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def analytic_bilinear_sumw2(qx, qy, n, dx, origin=0.0):
    """双线性规则的 sqrt(mean_q Σ_k w_k(q)²) 解析闭式（按公开权重公式推导）。

    生产 eval 双线性分支：cx = clip((x-x0)/dx, 0, nx-1)，i0 = clip(floor(cx), 0, nx-2)，
    fx = cx - i0，四点权重 (1-fx)(1-fy), fx(1-fy), (1-fx)fy, fx·fy ⇒
        Σ_k w_k(q)² = [(1-fx)²+fx²]·[(1-fy)²+fy²]     （2D 可分离）
    x0 = origin + (dx-1)/2（cell 中心锚定）。注意两端各半个 cell 的查询点被 cx 钳到端点
    ⇒ fx ≡ 0/1 ⇒ Σw² ≡ 1；漏掉这两条带会把闭式算小（512²/Δ64 上 0.6666 vs 真值 0.7083）。
    """
    x0 = origin + (dx - 1.0) / 2.0
    cx = np.clip((np.asarray(qx, float) - x0) / dx, 0.0, n - 1.0)
    cy = np.clip((np.asarray(qy, float) - x0) / dx, 0.0, n - 1.0)
    fx = cx - np.clip(np.floor(cx).astype(np.int64), 0, n - 2)
    fy = cy - np.clip(np.floor(cy).astype(np.int64), 0, n - 2)
    w2 = ((1.0 - fx) ** 2 + fx ** 2) * ((1.0 - fy) ** 2 + fy ** 2)
    return float(np.sqrt(np.mean(w2))), dict(
        n_edge_queries=int(np.sum((fx <= 0) | (fx >= 1) | (fy <= 0) | (fy >= 1))),
        n_query=int(w2.size), interior_mean_w2=float(np.mean(w2[(fx > 0) & (fx < 1) & (fy > 0) & (fy < 1)])))


# ---------------------------------------------------------------------------
# H0 驱动完整性 + 真值估计器标准误
# ---------------------------------------------------------------------------
def h0_driver_integrity():
    out = {}
    sub, p16 = 2048, 16
    rng = np.random.default_rng(SEED + 91)
    img = rng.normal(15.0, 3.0, size=(sub, sub))
    a = sigma_field_banded(img, p16, band_cells=16)
    b = C.sigma_field_fast(img, p16)
    fn = np.where((np.mgrid[0:sub, 0:sub].sum(axis=0)) % 2 == 1, np.nan, img)
    a2 = sigma_field_banded(fn, p16, band_cells=8)
    b2 = C.sigma_field_fast(fn, p16)
    out["banded_sigma"] = dict(
        size=[sub, sub], finite_input_bitwise_equal=bool(np.array_equal(a, b)),
        nan_input_bitwise_equal=bool(np.array_equal(a2, b2)),
        max_abs_diff=float(max(np.max(np.abs(a - b)), np.max(np.abs(a2 - b2)))))

    frame, n, D = 512, 8, 64
    yy, xx = np.mgrid[0:frame, 0:frame].astype(float)
    q_all = np.stack([xx.ravel(), yy.ravel()], axis=1)
    truth = np.full(q_all.shape[0], 20.0)
    rows = {}
    for tok in REGULAR_TOKENS:
        d, v, m = run_driver("h0_const_" + tok, token=tok, declare=True, regular=True,
                             nx=n, ny=n, dx=D, dy=D, origin=0.0,
                             values=np.full(n * n, 20.0), queries=q_all)
        mm = metrics(v, v, truth, m, m)
        rows[tok] = dict(mode="regular_grid", prod=prod_diag(d),
                         max_abs_dev_from_const=float(np.max(np.abs(v[m] - 20.0))),
                         disc_rel_mean=mm["disc_rel_mean"], n=mm["n"], n_query=int(q_all.shape[0]))
    pts = grid_queries(n, n, D, D)
    d, v, m = run_driver("h0_const_nearest", token=OP_NEAREST, declare=True, regular=False,
                         nx=n, ny=n, dx=D, dy=D, origin=0.0,
                         values=np.full(n * n, 20.0), queries=q_all, pts=pts,
                         max_radius=MAX_RADIUS_PX)
    mm = metrics(v, v, truth, m, m)
    rows[OP_NEAREST] = dict(mode="scattered", prod=prod_diag(d),
                            max_abs_dev_from_const=float(np.max(np.abs(v[m] - 20.0))),
                            disc_rel_mean=mm["disc_rel_mean"], n=mm["n"], n_query=int(q_all.shape[0]))
    out["constant_grid_zero_negative"] = rows
    return out


def h0b_truth_estimator_se():
    """MC 实测 sci_b_common σ 估计器的相对标准误（真值估计噪声的实际量级）。"""
    rng = np.random.default_rng(SEED + 92)
    res = {}
    for P, R in ((16, 24), (8, 24)):
        est = []
        for _ in range(R):
            patch = rng.normal(0.0, 1.0, size=(256, 256)) * 5.0 + 100.0
            f = C.sigma_field_fast(patch, P)
            est.append(f[np.isfinite(f)] / 5.0)
        e = np.concatenate(est)
        res["P%d_n%d" % (P, P * P)] = dict(
            n_samples_in_cell=P * P, n_repeats=R, n_estimates=int(e.size),
            rel_se=float(np.std(e, ddof=1)), rel_bias=float(np.mean(e) - 1.0),
            rel_se_of_se=float(np.std(e, ddof=1) / np.sqrt(2 * (e.size - 1))))
    r16 = res["P16_n256"]["rel_se"]
    r8 = res["P8_n64"]["rel_se"]
    res["ratio_se_64_over_256"] = r8 / r16
    res["theory_1_over_sqrt_N_ratio"] = 2.0
    res["use"] = ("P=16 cell（256 px）真值场的单 cell 相对标准误；用于把 disc_rel_rms 的"
                  "「估计器噪声污染」从读数里扣掉：disc_corrected = sqrt(max(disc²-se²,0))。"
                  "噪声传递读数（gain / noise_rel_rms）两臂共用同一真值 ⇒ 不受此污染。")
    return res


# ---------------------------------------------------------------------------
# H1 exp04 同几何：度量可比性门 + 冻结算子读数
# ---------------------------------------------------------------------------
def h1_exp04_grid():
    frame, delta = 512, 64
    yy, xx = np.mgrid[0:frame, 0:frame].astype(float)

    def truth_at(px, py):
        return 20.0 * (1.0 + 0.3 * np.sin(2 * np.pi * px / frame) * np.cos(2 * np.pi * py / frame))

    true_field = truth_at(xx, yy)
    rng = np.random.default_rng(SEED + 7)
    c = np.arange(delta // 2, frame, delta, dtype=float)      # exp04 逐字几何 32+64k
    pts, vals = [], []
    for cy in c:
        for cx in c:
            pts.append((cx, cy))
            vals.append(float(truth_at(cx, cy) * (1.0 + rng.normal(0.0, CTRL_NOISE_REL))))
    pts_e = np.array(pts, float)
    vals_e = np.array(vals, float)
    v_clean_e = truth_at(pts_e[:, 0], pts_e[:, 1])

    # (1) 逐字版 vs 向量化版：随机 4096 个查询点逐点比对（正确性锁定）
    idx = np.random.default_rng(SEED + 8).choice(xx.size, 4096, replace=False)
    qx_s = np.stack([xx.ravel()[idx], yy.ravel()[idx]], axis=1)
    ref_n = idw_reconstruct(pts_e, vals_e, qx_s[:, 0].reshape(1, -1), qx_s[:, 1].reshape(1, -1)).ravel()
    ref_c = idw_reconstruct(pts_e, v_clean_e, qx_s[:, 0].reshape(1, -1), qx_s[:, 1].reshape(1, -1)).ravel()
    vec_n = idw_reconstruct_vec(pts_e, vals_e, qx_s)
    vec_c = idw_reconstruct_vec(pts_e, v_clean_e, qx_s)
    dev = float(max(np.max(np.abs(vec_n - ref_n) / np.maximum(np.abs(ref_n), 1e-12)),
                    np.max(np.abs(vec_c - ref_c) / np.maximum(np.abs(ref_c), 1e-12))))

    # (2) 全幅复算（向量化）→ 与 exp04 存档读数比对（度量定义可比性门）
    q_all = np.stack([xx.ravel(), yy.ravel()], axis=1)
    ref_flat = true_field.ravel()
    ones = np.ones(ref_flat.size, bool)
    rec_n = idw_reconstruct_vec(pts_e, vals_e, q_all)
    rec_c = idw_reconstruct_vec(pts_e, v_clean_e, q_all)
    mm = metrics(rec_c, rec_n, ref_flat, ones, ones)
    with open(EXP04_JSON, "r", encoding="utf-8") as f:
        arch = json.load(f)["H3_downstream_interface"]
    idw = dict(
        n_control_points=int(vals_e.size), node_convention="exp04 逐字 32+64k",
        vectorized_vs_verbatim_max_rel_dev=dev,
        vectorized_vs_verbatim_n_points=int(idx.size),
        dense_rel_rms_noiseless_negative=mm["disc_rel_rms"],
        dense_rel_rms_with_ctrl_noise=mm["rms_noisy_vs_truth"] / float(true_field.std()),
        noise_inflation_over_noiseless=mm["inflation_ratio"],
        archived_exp04={k: arch[k] for k in (
            "dense_rel_rms_with_ctrl_noise", "dense_rel_rms_noiseless_negative",
            "noise_inflation_over_noiseless")})
    idw["rel_dev_vs_archived"] = {
        "noiseless": abs(idw["dense_rel_rms_noiseless_negative"] / arch["dense_rel_rms_noiseless_negative"] - 1.0),
        "with_noise": abs(idw["dense_rel_rms_with_ctrl_noise"] / arch["dense_rel_rms_with_ctrl_noise"] - 1.0),
        "inflation": abs(idw["noise_inflation_over_noiseless"] / arch["noise_inflation_over_noiseless"] - 1.0)}

    # (3) 冻结算子在**生产几何**（节点在 cell 中心）上的同网格读数
    frozen, proxy = {}, {}
    for D in DELTA_LIST:
        n = frame // D
        nodes = grid_queries(n, n, D, D)
        v0 = truth_at(nodes[:, 0], nodes[:, 1])
        xi = np.random.default_rng(SEED + 70 + D).normal(0.0, 1.0, size=v0.size)
        v_mul = v0 * (1.0 + CTRL_NOISE_REL * xi)
        v_add = v0 + CTRL_NOISE_REL * float(np.mean(v0)) * xi
        rows = {}
        for tok in REGULAR_TOKENS:
            d0, a0, m0 = run_driver("h1_%d_%s_clean" % (D, tok), token=tok, declare=True,
                                    regular=True, nx=n, ny=n, dx=D, dy=D, origin=0.0,
                                    values=v0, queries=q_all)
            d1, a1, m1 = run_driver("h1_%d_%s_mul" % (D, tok), token=tok, declare=True,
                                    regular=True, nx=n, ny=n, dx=D, dy=D, origin=0.0,
                                    values=v_mul, queries=q_all)
            d2, a2, m2 = run_driver("h1_%d_%s_add" % (D, tok), token=tok, declare=True,
                                    regular=True, nx=n, ny=n, dx=D, dy=D, origin=0.0,
                                    values=v_add, queries=q_all)
            rows[tok] = dict(prod=prod_diag(d0), mode="regular_grid",
                             mul=metrics(a0, a1, ref_flat, m0, m1), add=metrics(a0, a2, ref_flat, m0, m2))
        d0, a0, m0 = run_driver("h1_%d_nearest_clean" % D, token=OP_NEAREST, declare=True,
                                regular=False, nx=n, ny=n, dx=D, dy=D, origin=0.0,
                                values=v0, queries=q_all, pts=nodes, max_radius=MAX_RADIUS_PX)
        d1, a1, m1 = run_driver("h1_%d_nearest_mul" % D, token=OP_NEAREST, declare=True,
                                regular=False, nx=n, ny=n, dx=D, dy=D, origin=0.0,
                                values=v_mul, queries=q_all, pts=nodes, max_radius=MAX_RADIUS_PX)
        d2, a2, m2 = run_driver("h1_%d_nearest_add" % D, token=OP_NEAREST, declare=True,
                                regular=False, nx=n, ny=n, dx=D, dy=D, origin=0.0,
                                values=v_add, queries=q_all, pts=nodes, max_radius=MAX_RADIUS_PX)
        rows[OP_NEAREST] = dict(prod=prod_diag(d0), mode="scattered",
                                mul=metrics(a0, a1, ref_flat, m0, m1),
                                add=metrics(a0, a2, ref_flat, m0, m2))
        frozen["delta_%d" % D] = dict(
            delta_px=D, n_control_points=int(n * n), geometry="节点在 cell 中心 %g+%d·k" % ((D - 1) / 2, D),
            rows=rows)
        # IDW 代理在**同几何同噪声实现**下的对照（用向量化版；用于解释 exp04 的读数归属）
        rr0 = idw_reconstruct_vec(nodes, v0, q_all)
        rr1 = idw_reconstruct_vec(nodes, v_mul, q_all)
        e0 = float(np.sqrt(np.mean((rr0 - ref_flat) ** 2)))
        e1 = float(np.sqrt(np.mean((rr1 - ref_flat) ** 2)))
        dn = float(np.sqrt(np.mean((rr1 - rr0) ** 2)))
        proxy["delta_%d" % D] = dict(
            delta_px=D, n_control_points=int(n * n),
            disc_rel_rms=e0 / float(true_field.std()),
            noise_rel_rms=dn / float(np.mean(rr0)), gain=(dn / float(np.mean(rr0))) / CTRL_NOISE_REL,
            inflation_ratio=e1 / e0,
            note="代理算子（不在冻结词表）——只用于说明 exp04 的 0.2287/1.06× 出自 IDW 自身离散化")
    return dict(metric_parity_idw_exp04=idw,
                frozen_operators_on_exp04_field=frozen,
                idw_proxy_on_production_geometry=proxy,
                field="20*(1+0.3*sin(2*pi*x/512)*cos(2*pi*y/512))",
                note="exp04 的 32+64k 节点几何只用于 IDW 复现门；冻结算子必须用生产几何"
                     "（cell 中心 31.5+64k / 15.5+32k），否则会被 cell 中心门 fail-closed 拒绝。")


# ---------------------------------------------------------------------------
# H5a 512²/Δ64 均匀场：脉冲 Σw² + 解析闭式 + 多实现实测增益
# ---------------------------------------------------------------------------
def h5a_impulse_analytic():
    frame, n = 512, 8
    xy = np.arange(0, frame, Q_STRIDE_H5A, dtype=float)
    gx, gy = np.meshgrid(xy, xy)
    q = np.stack([gx.ravel(), gy.ravel()], axis=1)
    c0 = 20.0
    v0 = np.full(n * n, c0)
    eps = 1e-6 * c0                       # 与噪声臂同量级 ⇒ 钳制活性面一致
    pred_bilinear, ab = analytic_bilinear_sumw2(q[:, 0], q[:, 1], n, 64, 0.0)
    out = dict(frame_px=frame, delta_px=64, query_stride=Q_STRIDE_H5A, n_query=int(q.shape[0]),
               query_set="512² 像素网格按步长 4 抽样（16384 点）",
               analytic=dict(bilinear_closed_form=pred_bilinear, nearest_closed_form=1.0,
                             bilinear_detail=ab,
                             derivation="Σw² = [(1-fx)²+fx²][(1-fy)²+fy²]（双线性四点权重，2D 可分离）"),
               rows={})
    # 最近点的解析值（1.0）也在同一查询集上核验
    pts = grid_queries(n, n, 64, 64)
    for tok, kw in [(OP_NEAREST, dict(regular=False, pts=pts, max_radius=MAX_RADIUS_PX))] + \
                   [(t, dict(regular=True)) for t in REGULAR_TOKENS]:
        d0, a0, m0 = run_driver("h5a_%s_base" % tok, token=tok, declare=True, nx=n, ny=n,
                                dx=64, dy=64, origin=0.0, values=v0, queries=q, **kw)
        di, pred, mi = run_driver("h5a_%s_imp" % tok, token=tok, declare=True, nx=n, ny=n,
                                  dx=64, dy=64, origin=0.0, values=v0, queries=q,
                                  impulse_eps=eps, impulse_count=n * n, **kw)
        row = dict(predicted_gain=float(np.sqrt(np.mean(pred[mi]))),
                   impulse_mean_sumw2=_f(di, "impulse_mean_sumw2"),
                   prediction_valid=bool(tok != OP_MESH),
                   prediction_note=("mesh_median 对单节点脉冲响应恒为 0（3×3 中值吃掉单点扰动）"
                                    "⇒ 脉冲/线性预测对该非线性算子无效，只记实测" if tok == OP_MESH else ""))
        g2 = []
        for r in range(NOISE_REALIZATIONS):
            xi = np.random.default_rng(SEED + 500 + r).normal(0.0, 1.0, size=v0.size)
            v1 = v0 + eps * xi
            _d, a1, m1 = run_driver("h5a_%s_n%d" % (tok, r), token=tok, declare=True, nx=n, ny=n,
                                    dx=64, dy=64, origin=0.0, values=v1, queries=q, **kw)
            k = m0 & m1
            g2.append((float(np.sqrt(np.mean((a1[k] - a0[k]) ** 2))) /
                       float(np.sqrt(np.mean((v1 - v0) ** 2)))) ** 2)
        row["measured_gain_mean_of_R"] = float(np.sqrt(np.mean(g2)))
        row["measured_gain_std_of_R"] = float(np.std(np.sqrt(g2), ddof=1))
        row["R_realizations"] = NOISE_REALIZATIONS
        row["ctrl_perturbation_rel"] = float(eps / c0)
        out["rows"][tok] = row
    return out


# ---------------------------------------------------------------------------
# H3 fail-closed 负例
# ---------------------------------------------------------------------------
def h3_fail_closed():
    n, D = 8, 64
    vals = np.full(n * n, 20.0)
    qs = np.array([[10.0, 10.0], [200.0, 200.0]])
    cases = {}

    def rec(name, expect, **kw):
        nq = int(np.asarray(kw["queries"]).reshape(-1, 2).shape[0])
        d, v, m = run_driver("h3_" + name, **kw)
        ok = (d.get("prepare_ok") == "1")
        if expect == "prepare_fail":
            keep = not ok
        elif expect == "all_query_fail":
            keep = ok and int(_f(d, "n_err", -1)) == nq and int(_f(d, "n_out_of_domain", -1)) == nq
        else:
            keep = ok and int(_f(d, "n_ok", -1)) == nq
        cases[name] = dict(expectation=expect, prepare_ok=ok, prepare_err=d.get("prepare_err", ""),
                           n_query=nq, n_ok=int(_f(d, "n_ok", 0)), n_err=int(_f(d, "n_err", 0)),
                           n_out_of_domain=int(_f(d, "n_out_of_domain", 0)),
                           first_err=d.get("first_err", ""), n_filled=int(_f(d, "n_filled", 0)),
                           keep_fail_closed=bool(keep))
        return cases[name]

    ood = np.array([[517.0, 10.0], [10.0, -3.0], [600.0, 600.0]])
    rec("out_of_domain_query_all", "all_query_fail", token=OP_SPLINE, declare=True, regular=True,
        nx=n, ny=n, dx=D, dy=D, origin=0.0, values=vals, queries=ood)
    edge = np.array([[511.4, 511.4], [0.0, 0.0], [-0.4, 300.0], [300.0, 511.4]])
    rec("in_domain_boundary_ok", "all_query_ok", token=OP_SPLINE, declare=True, regular=True,
        nx=n, ny=n, dx=D, dy=D, origin=0.0, values=vals, queries=edge)
    for tok in ["idw_power2_k8_v1", "idw", "NATURAL_BICUBIC_SPLINE_CLIP_V1", "kriging_v1"]:
        rec("unknown_token_%s" % tok, "prepare_fail", token=tok, declare=True, regular=True,
            nx=n, ny=n, dx=D, dy=D, origin=0.0, values=vals, queries=qs)
    rec("nearest_on_regular_grid", "prepare_fail", token=OP_NEAREST, declare=True, regular=True,
        nx=n, ny=n, dx=D, dy=D, origin=0.0, values=vals, queries=qs)
    v0 = vals.copy(); v0[0] = 0.0
    rec("control_value_zero", "prepare_fail", token=OP_SPLINE, declare=True, regular=True,
        nx=n, ny=n, dx=D, dy=D, origin=0.0, values=v0, queries=qs)
    vneg = vals.copy(); vneg[3] = -5.0
    rec("control_value_negative", "prepare_fail", token=OP_SPLINE, declare=True, regular=True,
        nx=n, ny=n, dx=D, dy=D, origin=0.0, values=vneg, queries=qs)
    rec("nx_too_small", "prepare_fail", token=OP_SPLINE, declare=True, regular=True,
        nx=1, ny=8, dx=D, dy=D, origin=0.0, values=np.full(8, 20.0), queries=qs)
    rec("all_nan_control_grid", "prepare_fail", token=OP_SPLINE, declare=True, regular=True,
        nx=n, ny=n, dx=D, dy=D, origin=0.0, values=np.full(n * n, np.nan), queries=qs)
    vnan = vals.copy(); vnan[[0, 5, 9]] = np.nan
    r8 = rec("partial_nan_filled", "all_query_ok", token=OP_SPLINE, declare=True, regular=True,
             nx=n, ny=n, dx=D, dy=D, origin=0.0, values=vnan, queries=qs)
    r8["expect_n_filled"] = 3
    r8["n_filled_ok"] = int(r8["n_filled"]) == 3
    # 角点锚定网格（x0 = origin，整场平移半 cell）⇒ cell 中心门必须 fail-closed
    d, v, m = run_driver("h3_corner_anchored_grid", token=OP_SPLINE, declare=True, regular=True,
                         nx=n, ny=n, dx=D, dy=D, origin=0.0, values=vals, queries=qs,
                         cell_center_offset=0)
    cases["corner_anchored_grid"] = dict(
        expectation="prepare_fail", prepare_ok=(d.get("prepare_ok") == "1"),
        prepare_err=d.get("prepare_err", ""), keep_fail_closed=(d.get("prepare_ok") != "1"))
    return cases


# ---------------------------------------------------------------------------
# H4 值域钳制核验
# ---------------------------------------------------------------------------
def h4_clip():
    n, D, frame = 8, 64, 512
    yy, xx = np.mgrid[0:frame, 0:frame].astype(float)
    q_all = np.stack([xx.ravel(), yy.ravel()], axis=1)
    rng = np.random.default_rng(SEED + 11)
    vals = 20.0 * np.exp(0.4 * rng.normal(0.0, 1.0, size=n * n))
    stress = vals.copy()
    stress[27] = float(np.min(vals)) * 0.05
    out = {}
    for tag, v in (("realistic_lognormal", vals), ("stress_deep_dip", stress)):
        rows = {}
        for tok in REGULAR_TOKENS:
            d, a, m = run_driver("h4_%s_%s" % (tag, tok), token=tok, declare=True, regular=True,
                                 nx=n, ny=n, dx=D, dy=D, origin=0.0, values=v, queries=q_all)
            pd = prod_diag(d)
            lo, hi, vv = pd["clip_low"], pd["clip_high"], a[m]
            tol = 1e-9 * max(1.0, abs(hi))
            rows[tok] = dict(value_range_clipped=pd["clipped"], clip_low=lo, clip_high=hi,
                             ctrl_min=float(np.min(v)), ctrl_max=float(np.max(v)),
                             recon_min=float(np.min(vv)), recon_max=float(np.max(vv)),
                             n_below_clip=int(np.sum(vv < lo - tol)),
                             n_above_clip=int(np.sum(vv > hi + tol)),
                             n_at_clip_low=int(np.sum(np.abs(vv - lo) <= tol)),
                             n_at_clip_high=int(np.sum(np.abs(vv - hi) <= tol)),
                             within_clip_range=bool(np.all(vv >= lo - tol) and np.all(vv <= hi + tol)))
        out[tag] = rows
    out["clip_can_be_disabled_independently"] = False
    out["evidence"] = ("weight_chain.h:117-118 sparse_recon_operator_clips_to_ctrl_range()：仅 "
                       "natural_bicubic_spline_clip_v1 / ..._mesh_median_v1 为真；"
                       "weight_chain.cpp:654-656 在 eval 内按 clips_ 施加；API 无独立开关。"
                       "故「关掉钳制」在生产不可表达，只能以 bilinear 无钳制档 + 值域界核验代替。")
    return out


# ---------------------------------------------------------------------------
# H2 真实帧 × 真实生产控制网格
# ---------------------------------------------------------------------------
def load_frame(path):
    from astropy.io import fits
    with fits.open(path, memmap=False) as h:
        raw = np.asarray(h[0].data, dtype=np.float64)
        hdr = h[0].header
    if raw.shape != (FRAME_SIZE, FRAME_SIZE):
        raise RuntimeError("帧尺寸非 4096²（fail-closed，禁止静默裁剪）: %s %s" % (path, raw.shape))
    return raw, dict(file=os.path.basename(path), exptime=float(hdr.get("EXPTIME", 0.0)),
                     filter=str(hdr.get("FILTER", "")),
                     naxis=[int(hdr["NAXIS1"]), int(hdr["NAXIS2"])])


def _arm_runs(fi, D, tag, v0, v_mul, v_add, truth, q_all, nodes, n, ctrl_rel_mul, ctrl_rel_add):
    """tag 同时用于驱动临时文件命名与失败归属（含 Δ 与标量表示）。"""
    rows = {}
    for tok in REGULAR_TOKENS:
        t = "h2_f%d_%s_%s" % (fi, tag, tok)
        d0, a0, m0 = chk(t + "_clean", run_driver(t + "_clean", token=tok,
                         declare=True, regular=True, nx=n, ny=n, dx=D, dy=D,
                         origin=0.0, values=v0, queries=q_all))
        d1, a1, m1 = chk(t + "_mul", run_driver(t + "_mul", token=tok,
                         declare=True, regular=True, nx=n, ny=n, dx=D, dy=D,
                         origin=0.0, values=v_mul, queries=q_all))
        if v_add is None:                       # 加性臂不可表示（扰动使控制值非正）
            rows[tok] = dict(mode="regular_grid", prod=prod_diag(d0),
                             mul=metrics(a0, a1, truth, m0, m1, ctrl_rel_mul),
                             add=dict(skipped="加性臂不可表示（见 additive_arm_note）"))
        else:
            d2, a2, m2 = chk(t + "_add", run_driver(t + "_add", token=tok, declare=True,
                             regular=True, nx=n, ny=n, dx=D, dy=D, origin=0.0,
                             values=v_add, queries=q_all))
            rows[tok] = dict(mode="regular_grid", prod=prod_diag(d0),
                             mul=metrics(a0, a1, truth, m0, m1, ctrl_rel_mul),
                             add=metrics(a0, a2, truth, m0, m2, ctrl_rel_add))
    t = "h2_f%d_%s_nearest" % (fi, tag)
    # 散点模式的生产语义与规则网格**不同**（weight_chain.cpp:416-421 vs :488-492）：
    # 规则网格把 NaN 当 schema 的 invalid 标记并按最近有效点填充；散点模式要求逐点
    # positive_finite，NaN 直接判 corrupt ⇒ 散点臂必须先剔除 invalid 点（同一掩膜用于
    # clean/mul/add 三臂，保证可比）。
    fm = np.isfinite(v0) & (v0 > 0.0)
    if not bool(fm.any()):
        raise RuntimeError("散点臂无有效控制点（fail-closed）: " + t)
    n_drop = int(fm.size - int(fm.sum()))
    nodes_u, v0u = nodes[fm], v0[fm]
    v_mul_u = None if v_mul is None else v_mul[fm]
    d0, a0, m0 = chk(t + "_clean", run_driver(t + "_clean", token=OP_NEAREST,
                     declare=True, regular=False, nx=n, ny=n, dx=D, dy=D, origin=0.0,
                     values=v0u, queries=q_all, pts=nodes_u, max_radius=MAX_RADIUS_PX))
    d1, a1, m1 = chk(t + "_mul", run_driver(t + "_mul", token=OP_NEAREST,
                     declare=True, regular=False, nx=n, ny=n, dx=D, dy=D, origin=0.0,
                     values=v_mul_u, queries=q_all, pts=nodes_u, max_radius=MAX_RADIUS_PX))
    if v_add is None:
        rows[OP_NEAREST] = dict(mode="scattered", prod=prod_diag(d0),
                                mul=metrics(a0, a1, truth, m0, m1, ctrl_rel_mul),
                                add=dict(skipped="加性臂不可表示（见 additive_arm_note）"),
                                n_scattered_points=int(fm.sum()), n_dropped_invalid=n_drop)
    else:
        d2, a2, m2 = chk(t + "_add", run_driver(t + "_add", token=OP_NEAREST,
                         declare=True, regular=False, nx=n, ny=n, dx=D, dy=D, origin=0.0,
                         values=v_add[fm], queries=q_all, pts=nodes_u, max_radius=MAX_RADIUS_PX))
        rows[OP_NEAREST] = dict(mode="scattered", prod=prod_diag(d0),
                                mul=metrics(a0, a1, truth, m0, m1, ctrl_rel_mul),
                                add=metrics(a0, a2, truth, m0, m2, ctrl_rel_add))
    rows[OP_NEAREST].update(n_scattered_points=int(fm.sum()), n_dropped_invalid=n_drop,
                            scattered_note="散点模式要求逐点 positive_finite（无 NaN 填充）⇒ "
                                           "invalid 点先剔除；同一掩膜用于三臂")
    return rows


def h2_real_grid(n_frames, x_frac):
    q_all = dense_queries(FRAME_SIZE // P_TRUTH, P_TRUTH)        # 256² = 65536 查询点
    out = dict(
        query_points=dict(n=int(q_all.shape[0]), patch_px=P_TRUTH,
                          coords="P=16 cell 中心 (16k+7.5, 16l+7.5)，k,l=0..255"),
        config=dict(delta_default=64, delta_extra=32, ctrl_noise_rel=CTRL_NOISE_REL, seed=SEED,
                    max_radius_px=MAX_RADIUS_PX,
                    frame_policy="主帧：Δ=64 与 Δ=32 × {同族真值, 1px 棋盘 hold-out 真值} + 正齐次性；"
                                 "其余帧：Δ=64 同族真值（覆盖生产全帧与多帧）"),
        frames=[])
    fam = (np.mgrid[0:FRAME_SIZE, 0:FRAME_SIZE].sum(axis=0)) % 2
    for fi in range(n_frames):
        path = M42[fi]
        if not os.path.exists(path):
            raise RuntimeError("testdata 缺失（fail-closed，禁止静默少面/替换）: " + path)
        img, meta = load_frame(path)
        t = time.perf_counter()
        fields = {"full_%d" % P_TRUTH: sigma_field_banded(img, P_TRUTH),
                  "full_64": sigma_field_banded(img, 64)}
        if fi == 0:
            fields["full_32"] = sigma_field_banded(img, 32)
            fields["fam1_%d" % P_TRUTH] = sigma_field_banded(np.where(fam == 1, img, np.nan), P_TRUTH)
            fields["fam0_64"] = sigma_field_banded(np.where(fam == 0, img, np.nan), 64)
        load_s = time.perf_counter() - t
        frow = dict(meta=meta, sigma_fields_s=load_s, n_valid_cells={k: int(np.isfinite(v).sum())
                                                                    for k, v in fields.items()},
                    arms={})
        # 层标量：P2 的 sparse_snr_value = F_ref,k/σ_F,c（正标量，∝ 1/σ）。
        # σ[ADU] 与 1/σ（等价于取 F_ref≡1 的 SNR）是同一层的两种标量表示，两种都报，
        # 并把「幅度尺度不影响相对读数」作为可假门（正齐次性）。
        reps = [("sigma", lambda a: a), ("inv_sigma_Fref1", lambda a: 1.0 / a)]
        for rep_name, rep_fn in reps:
            for D in (DELTA_LIST if fi == 0 else [64]):
                n = FRAME_SIZE // D
                nodes = grid_queries(n, n, D, D)
                variants = [("same_family", fields["full_%d" % D], fields["full_%d" % P_TRUTH])]
                if fi == 0 and D == 64:
                    variants.append(("holdout_1px", fields["fam0_64"], fields["fam1_%d" % P_TRUTH]))
                for variant, ctrl_field, truth_field in variants:
                    v0 = flat_ctrl(rep_fn(ctrl_field))
                    truth = flat_ctrl(rep_fn(truth_field))
                    bad = ~np.isfinite(v0) & ~np.isnan(v0)
                    if bad.any():
                        raise RuntimeError("控制网格含 ±inf（非法值，fail-closed）: rep=%s" % rep_name)
                    nan_ctrl = int(np.isnan(v0).sum())
                    # 控制网格里的 NaN 是 schema 的 invalid 标记（生产按最近有效点填充）；
                    # 噪声幅度必须用 nan-aware 均值，否则一个 NaN 会把整个加性臂变成全 NaN
                    mean_ctrl = _nan_mean(v0)
                    xi = np.random.default_rng(SEED + 1000 * fi + 10 * D).normal(0.0, 1.0, size=v0.size)
                    v_mul = v0 * (1.0 + CTRL_NOISE_REL * xi)
                    # 加性臂统一定义在 σ[ADU] 域：σ_add = σ + 1.5%·mean(σ)·ξ，再映射回本表示的层值。
                    # 理由：层标量 SNR ∝ 1/σ 可跨一到两个数量级，若直接在层值上加「1.5%·mean」的
                    # 全局偏移，最暗弱处会被推成非正值 —— 生产按 corrupt fail-closed（实测），
                    # 该扰动模型在那里根本不可表示。σ 域定义在两种表示下是同一个物理扰动。
                    sig0 = 1.0 / v0 if rep_name != "sigma" else v0
                    sig_add = sig0 + CTRL_NOISE_REL * _nan_mean(sig0) * xi
                    v_add = (1.0 / sig_add) if rep_name != "sigma" else sig_add
                    rel_mul = _nan_rms(v_mul - v0) / mean_ctrl
                    neg = int(np.sum(np.isfinite(v_add) & (v_add <= 0)))
                    v_add_arg, add_note = v_add, None
                    if neg:
                        add_note = ("加性臂不可表示：扰动使 %d/%d 个控制值非正"
                                    "（生产把非正控制值视为 corrupt 并 fail-closed）⇒ 该臂不跑。"
                                    % (neg, v_add.size))
                        v_add_arg = None
                    rel_add = (_nan_rms(v_add - v0) / mean_ctrl) if v_add_arg is not None else None
                    key = "delta_%d_%s_%s" % (D, variant, rep_name)
                    frow["arms"][key] = dict(
                        delta_px=D, truth_variant=variant, representation=rep_name,
                        n_control_points=int(n * n), n_invalid_control_points=nan_ctrl,
                        ctrl_stats=dict(mean=mean_ctrl, std=_nan_rms(v0 - mean_ctrl),
                                        min=float(np.nanmin(v0)), max=float(np.nanmax(v0))),
                        additive_model="σ 域加性：σ'=σ+1.5%·mean(σ)·ξ，再映射回本表示"
                                       "（两种表示下同一物理扰动；避免层值被推成非正）",
                        additive_arm_note=add_note,
                        rows=_arm_runs(fi, D, key, v0, v_mul, v_add_arg, truth, q_all, nodes, n,
                                       rel_mul, rel_add))
                    if fi == 0 and D == 64 and variant == "same_family" and rep_name == "inv_sigma_Fref1":
                        # 正齐次性（精确门）：R(K·v) = K·R(v)，且相对读数逐位不变。
                        # 这是「控制值整体幅度（含 F_ref 因子）不影响结论」的全部数学根据。
                        K = 1000.0
                        d0, a0, m0 = chk("homog_base", run_driver("h2_homog_base", token=OP_SPLINE,
                                         declare=True, regular=True, nx=n, ny=n, dx=D, dy=D,
                                         origin=0.0, values=v0, queries=q_all))
                        dK, aK, mK = chk("homog_scaled", run_driver("h2_homog_scaled", token=OP_SPLINE,
                                         declare=True, regular=True, nx=n, ny=n, dx=D, dy=D,
                                         origin=0.0, values=K * v0, queries=q_all))
                        g0 = _f(d0, "clip_low"), _f(d0, "clip_high")
                        gK = _f(dK, "clip_low"), _f(dK, "clip_high")
                        k = m0 & mK
                        pt_dev = float(np.max(np.abs(aK[k] - K * a0[k]) / np.abs(K * a0[k])))
                        mm_base = frow["arms"][key]["rows"][OP_SPLINE]["mul"]
                        mm_K = metrics(aK, aK, K * truth, mK, mK)
                        frow["homogeneity_check"] = dict(
                            statement="R(K·v) = K·R(v)（K=1000；含钳制：clip 界随网格等比缩放）",
                            pointwise_max_rel_dev=pt_dev,
                            clip_low_ratio=gK[0] / g0[0] if g0[0] else None,
                            clip_high_ratio=gK[1] / g0[1] if g0[1] else None,
                            disc_scaled_rel=mm_K["disc_rel_rms"],
                            disc_base_rel=mm_base["disc_rel_rms"],
                            rel_dev=float(abs(mm_K["disc_rel_rms"] / mm_base["disc_rel_rms"] - 1.0)),
                            note="此门可红：若算子非正齐次（或钳制界不随幅度缩放），"
                                 "控制值幅度（含 F_ref 因子）就会改变相对传递读数。")
        out["frames"].append(frow)
        if fi == 0 and len(out["frames"]) == 1:
            out["_frame0_fields"] = fields      # 供 H5b 用；落盘前移除
        del fields, img
    return out


# ---------------------------------------------------------------------------
# H5b 真实帧（主帧 1024² 全节点窗口）脉冲 + 线性叠加
# ---------------------------------------------------------------------------
def _finite_window(field64, n):
    """取 field64 内第一个 n×n 全有效的块（脉冲 Σw² 需要完整节点集且不能含 invalid 标记）。"""
    for j in range(0, field64.shape[0] - n + 1):
        for i in range(0, field64.shape[1] - n + 1):
            blk = field64[j:j + n, i:i + n]
            if np.all(np.isfinite(blk)):
                return blk, j, i
    raise RuntimeError("找不到 %d×%d 全有效控制块（fail-closed）" % (n, n))


def h5b_real(fields, x_frac):
    n = H5_WINDOW_N
    D = 64
    inv = lambda a: 1.0 / a                      # 层标量 = SNR ∝ 1/σ（F_ref≡1）
    f64 = inv(fields["full_64"])
    win, j0, i0 = _finite_window(f64, n)
    v0 = flat_ctrl(win)
    mean_v = _nan_mean(v0)
    eps = 1e-4 * mean_v
    m = n * 64 // P_TRUTH                                   # 窗口内 P=16 cell 数 = 64
    gx, gy = np.meshgrid(cell_centers(m, P_TRUTH) + i0 * D, cell_centers(m, P_TRUTH) + j0 * D)
    q = np.stack([gx.ravel(), gy.ravel()], axis=1)           # 窗口内 64²=4096 查询点
    truth = inv(fields["full_%d" % P_TRUTH][j0 * D // P_TRUTH:(j0 * D + n * D) // P_TRUTH,
                                             i0 * D // P_TRUTH:(i0 * D + n * D) // P_TRUTH]).ravel()
    org2 = (float(i0 * D), float(j0 * D))
    d, pred, mk = chk("h5b_imp", run_driver("h5b_imp", token=OP_SPLINE, declare=True, regular=True,
                     nx=n, ny=n, dx=D, dy=D, origin=org2, values=v0, queries=q,
                     impulse_eps=eps, impulse_count=n * n))
    n_ok_imp = int(_f(d, "n_ok", 0))
    out = dict(representation="inv_sigma_Fref1（层标量 SNR ∝ 1/σ）",
               window_px=n * D, window_origin_px=[int(i0 * D), int(j0 * D)],
               n_nodes=int(n * n), n_query=int(q.shape[0]), impulse_eps=eps,
               note="Σ_k w_k(q)² 需要**完整**节点集，故窗口取 16×16 个 Δ=64 单元（1024² px），"
                    "并选主帧上第一个**全有效**（无 invalid 控制点）的 16×16 块 ⇒ 窗口位置随帧数据而定"
                    "（window_origin_px 记录实际位置；脉冲分析与平移无关）。",
               default_operator=dict(predicted_gain=float(np.sqrt(np.mean(pred[mk]))),
                                     impulse_mean_sumw2=_f(d, "impulse_mean_sumw2"),
                                     impulse_n_ok=n_ok_imp, impulse_n_query=int(q.shape[0])),
               linearity={},
               linearity_note="线性叠加检验（同一窗口的完整节点集；含 NaN 的网格会让 NaN 填充"
                              "破坏「0.5(a+b)」的构造，故用全有效窗口）：")
    xi = np.random.default_rng(SEED + 4242).normal(0.0, 1.0, size=v0.size)
    vb = v0 * (1.0 + CTRL_NOISE_REL * xi)
    vm = 0.5 * (v0 + vb)
    org = (float(i0 * D), float(j0 * D))
    for tok in REGULAR_TOKENS:
        _d, va, ma = chk("lin_a", run_driver("h5b_lin_a_%s" % tok, token=tok, declare=True,
                         regular=True, nx=n, ny=n, dx=D, dy=D, origin=org, values=v0, queries=q))
        _d, vbb, mb = chk("lin_b", run_driver("h5b_lin_b_%s" % tok, token=tok, declare=True,
                          regular=True, nx=n, ny=n, dx=D, dy=D, origin=org, values=vb, queries=q))
        _d, vmm, mm2 = chk("lin_m", run_driver("h5b_lin_m_%s" % tok, token=tok, declare=True,
                           regular=True, nx=n, ny=n, dx=D, dy=D, origin=org, values=vm, queries=q))
        k = ma & mb & mm2
        out["linearity"][tok] = dict(
            max_rel_superposition_residual=float(np.max(np.abs(vmm[k] - 0.5 * (va[k] + vbb[k]))) /
                                                  np.mean(vmm[k])),
            n_query=int(k.sum()))
    # 窗口上的加性噪声实测增益 vs 脉冲预测
    v_add = v0 + CTRL_NOISE_REL * mean_v * np.random.default_rng(SEED + 4243).normal(0.0, 1.0, size=v0.size)
    _d0, a0, m0 = chk("add_clean", run_driver("h5b_add_clean", token=OP_SPLINE, declare=True,
                      regular=True, nx=n, ny=n, dx=D, dy=D, origin=org, values=v0, queries=q))
    _d1, a1, m1 = chk("add_noisy", run_driver("h5b_add_noisy", token=OP_SPLINE, declare=True,
                      regular=True, nx=n, ny=n, dx=D, dy=D, origin=org, values=v_add, queries=q))
    k = m0 & m1
    dn = float(np.sqrt(np.mean((a1[k] - a0[k]) ** 2)))
    out["default_operator"]["measured_additive_gain"] = dn / (CTRL_NOISE_REL * mean_v)
    out["default_operator"]["pred_vs_measured_rel_dev"] = abs(
        out["default_operator"]["measured_additive_gain"] -
        out["default_operator"]["predicted_gain"]) / out["default_operator"]["predicted_gain"]
    return out


# ---------------------------------------------------------------------------
# 主流程
# ---------------------------------------------------------------------------
def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--frames", type=int, default=3, help="真实帧数（1–3）")
    ap.add_argument("--skip-real", action="store_true", help="只跑 H0/H1/H5a/H3/H4")
    ap.add_argument("--out", default=OUT_JSON)
    a = ap.parse_args()

    if not os.path.exists(DRIVER):
        log("[build] " + BUILD_SH)
        subprocess.run(["bash", BUILD_SH], check=True)
    os.makedirs(WORK, exist_ok=True)
    for f in os.listdir(WORK):
        try:
            os.remove(os.path.join(WORK, f))
        except OSError:
            pass

    t = {}
    log("[H0] driver integrity + sigma estimator SE")
    h0 = h0_driver_integrity()
    log("    banded bitwise equal: %s/%s (diff=%.1e); const-grid dev=%s" % (
        h0["banded_sigma"]["finite_input_bitwise_equal"], h0["banded_sigma"]["nan_input_bitwise_equal"],
        h0["banded_sigma"]["max_abs_diff"],
        {k: "%.1e" % v["max_abs_dev_from_const"] for k, v in h0["constant_grid_zero_negative"].items()}))
    h0["truth_estimator_se"] = h0b_truth_estimator_se()
    log("    truth-estimator rel_se: P16/256px=%.4f  P8/64px=%.4f  ratio=%.3f (theory 2.0)" % (
        h0["truth_estimator_se"]["P16_n256"]["rel_se"], h0["truth_estimator_se"]["P8_n64"]["rel_se"],
        h0["truth_estimator_se"]["ratio_se_64_over_256"]))
    t["H0"] = time.perf_counter() - _T0

    log("[H1] exp04 grid: proxy parity + frozen operators")
    h1 = h1_exp04_grid()
    p = h1["metric_parity_idw_exp04"]
    log("    IDW parity rel_dev=%s (vec-vs-verbatim %.1e @%d pts)" % (
        {k: "%.1e" % v for k, v in p["rel_dev_vs_archived"].items()},
        p["vectorized_vs_verbatim_max_rel_dev"], p["vectorized_vs_verbatim_n_points"]))
    for D in DELTA_LIST:
        r = h1["frozen_operators_on_exp04_field"]["delta_%d" % D]["rows"]
        log("    Δ=%d " % D + " | ".join(
            "%s disc=%.4f gain=%.3f infl=%.3f" % (k[:26], r[k]["mul"]["disc_rel_rms"],
                                                  r[k]["mul"]["gain"], r[k]["mul"]["inflation_ratio"])
            for k in r))
    t["H1"] = time.perf_counter() - _T0

    log("[H5a] 512²/Δ64 impulse + analytic closed forms")
    h5a = h5a_impulse_analytic()
    log("    " + json.dumps({k: dict(pred=round(v["predicted_gain"], 4),
                                     meas=round(v["measured_gain_mean_of_R"], 4))
                              for k, v in h5a["rows"].items()}, ensure_ascii=False))
    t["H5a"] = time.perf_counter() - _T0

    log("[H3] fail-closed negatives")
    h3 = h3_fail_closed()
    log("    all fail-closed: %s" % all(v.get("keep_fail_closed") for v in h3.values()))
    log("[H4] clip verification")
    h4 = h4_clip()
    t["H3H4"] = time.perf_counter() - _T0

    h2 = h5b = None
    if not a.skip_real:
        log("[H2] real M42 frames on the production control grid")
        h2 = h2_real_grid(max(1, min(3, a.frames)), None)
        for f in h2["frames"]:
            for key, arm in f["arms"].items():
                r = arm["rows"]
                log("    %-30s %-22s " % (f["meta"]["file"][:30], key) + " | ".join(
                    "%s disc=%.3f T=%.3f/%.3f infl=%.4f" % (
                        k[:20], r[k]["mul"]["disc_rel_rms"],
                        (r[k]["mul"].get("gain_selfnormalized") or float("nan")),
                        ((r[k].get("add") or {}).get("gain_selfnormalized") or float("nan")),
                        r[k]["mul"]["inflation_ratio"]) for k in r))
            if "homogeneity_check" in f:
                log("    homogeneity rel_dev=%.2e" % f["homogeneity_check"]["rel_dev"])
        t["H2"] = time.perf_counter() - _T0
        f0_fields = h2.pop("_frame0_fields", None)
        if f0_fields is None:
            raise RuntimeError("主帧 σ 场缺失（H5b 需要，fail-closed）")
        log("[H5b] real-frame window impulse + superposition")
        h5b = h5b_real(f0_fields, None)
        log("    pred=%.4f measured=%.4f rel_dev=%.2e; linearity=%s" % (
            h5b["default_operator"]["predicted_gain"],
            h5b["default_operator"]["measured_additive_gain"],
            h5b["default_operator"]["pred_vs_measured_rel_dev"],
            {k: "%.1e" % v["max_rel_superposition_residual"] for k, v in h5b["linearity"].items()}))
        t["H5b"] = time.perf_counter() - _T0

    # ---------------- 判据门 ----------------
    g = {}
    cg = h0["constant_grid_zero_negative"]
    g["G0_banded_sigma_bitwise_equal"] = dict(
        green=bool(h0["banded_sigma"]["finite_input_bitwise_equal"] and
                   h0["banded_sigma"]["nan_input_bitwise_equal"]),
        value=h0["banded_sigma"], requirement="分带 σ 场与整幅计算逐位相等（含 NaN 输入）")
    g["G1_constant_grid_zero_negative"] = dict(
        green=bool(all(v["disc_rel_mean"] < 1e-12 and v["max_abs_dev_from_const"] < 1e-12
                       and v["n"] == v["n_query"] for v in cg.values())),
        value={k: dict(disc_rel_mean=v["disc_rel_mean"], dev=v["max_abs_dev_from_const"],
                       n=v["n"], n_query=v["n_query"]) for k, v in cg.items()},
        requirement="常数控制网格 ⇒ 重建场逐点=常数（4 个 token 全绿），512² 全部 262144 查询点在域内")
    g["G15_truth_estimator_se_model"] = dict(
        green=bool(abs(h0["truth_estimator_se"]["ratio_se_64_over_256"] - 2.0) < 0.15),
        value=h0["truth_estimator_se"],
        requirement="真值 σ 估计器的相对标准误随每 cell 像素数按 1/√N 缩放（实测 64px/256px 之比 ≈2）"
                    "⇒ 该 SE 可作为 disc_rel_rms 污染量的实测标定；此门可红")
    par = h1["metric_parity_idw_exp04"]
    g["G2_metric_definition_parity_with_exp04"] = dict(
        green=bool(max(par["rel_dev_vs_archived"].values()) < 1e-9 and
                   par["vectorized_vs_verbatim_max_rel_dev"] < 1e-12),
        value=dict(rel_dev_vs_archived=par["rel_dev_vs_archived"],
                   vectorized_vs_verbatim=par["vectorized_vs_verbatim_max_rel_dev"],
                   archived=par["archived_exp04"]),
        requirement="本脚本度量定义逐位复现 exp04 H3 存档的 IDW 读数（否则与 0.2287/1.06× 不可比）")
    a5 = h5a["rows"]
    g["G3_bilinear_closed_form"] = dict(
        green=bool(abs(a5[OP_BILINEAR]["predicted_gain"] -
                       h5a["analytic"]["bilinear_closed_form"]) < 1e-9),
        value=dict(measured=a5[OP_BILINEAR]["predicted_gain"],
                   closed_form=h5a["analytic"]["bilinear_closed_form"], detail=h5a["analytic"]["bilinear_detail"]),
        requirement="双线性 Σw² 的解析闭式（含两端半 cell 端点带）必须与生产算子脉冲响应一致")
    g["G4_nearest_identity"] = dict(
        green=bool(abs(a5[OP_NEAREST]["predicted_gain"] - 1.0) < 1e-8),
        value=a5[OP_NEAREST]["predicted_gain"],
        requirement="最近点算子 Σw²≡1 ⇒ 传递增益≡1（容差 1e-8 覆盖脉冲差商舍入）")
    g["G5_impulse_predicts_measured_gain"] = dict(
        green=bool(all(abs(a5[k]["measured_gain_mean_of_R"] - a5[k]["predicted_gain"]) <=
                       0.03 * a5[k]["predicted_gain"] for k in (OP_BILINEAR, OP_NEAREST))),
        value={k: dict(measured=a5[k]["measured_gain_mean_of_R"], predicted=a5[k]["predicted_gain"],
                       rel_dev=abs(a5[k]["measured_gain_mean_of_R"] - a5[k]["predicted_gain"]) /
                               a5[k]["predicted_gain"]) for k in (OP_BILINEAR, OP_NEAREST)},
        requirement="线性算子（bilinear/nearest）的 R 实现实测增益必须命中脉冲预测（3% 内）")
    g["G6_fail_closed_negatives"] = dict(
        green=bool(all(v.get("keep_fail_closed") for v in h3.values()) and
                   h3["partial_nan_filled"]["n_filled_ok"]),
        value={k: v.get("keep_fail_closed") for k, v in h3.items()},
        requirement="全部退化/未知/越界输入 fail-closed（含 IDW token 被拒）；部分 NaN 必须按最近有效点填充")
    clip_ok = all(not (r["n_below_clip"] or r["n_above_clip"])
                  for tag in ("realistic_lognormal", "stress_deep_dip")
                  for r in h4[tag].values())
    g["G7_clip_in_range_and_binding"] = dict(
        green=bool(clip_ok and h4["realistic_lognormal"][OP_SPLINE]["value_range_clipped"] == 1 and
                   h4["realistic_lognormal"][OP_BILINEAR]["value_range_clipped"] == 0 and
                   max(h4["stress_deep_dip"][k]["n_at_clip_low"] + h4["stress_deep_dip"][k]["n_at_clip_high"]
                       for k in (OP_SPLINE, OP_MESH)) > 0),
        value=dict(binding_stress={k: dict(low=v["n_at_clip_low"], high=v["n_at_clip_high"])
                                   for k, v in h4["stress_deep_dip"].items()},
                   clipped_flags={k: v["value_range_clipped"] for k, v in h4["realistic_lognormal"].items()}),
        requirement="钳制档重建值落在 [min,max] 有效控制值内、且深凹陷用例下钳制边界确实被触发；"
                    "bilinear 档 clipped=0（无钳制分支）")
    lens = []
    if h2 is not None:
        for f in h2["frames"]:
            for key, arm in f["arms"].items():
                for tok, r in arm["rows"].items():
                    lens.append(dict(frame=f["meta"]["file"][:34], arm=key, op=tok,
                                     disc=r["mul"]["disc_rel_rms"], gain=r["mul"]["gain"],
                                     gain_add=(r.get("add") or {}).get("gain"),
                                     infl=r["mul"]["inflation_ratio"],
                                     gain_self=(r.get("add") or {}).get("gain_selfnormalized")))
        g["G8_real_arm_positive_and_finite"] = dict(
            green=bool(all(np.isfinite(x["gain"]) and x["gain"] > 0 for x in lens)),
            value=dict(n_arms=len(lens), min_gain=float(min(x["gain"] for x in lens)),
                       max_gain=float(max(x["gain"] for x in lens))),
            requirement="真实帧 × 真实控制网格上 1.5% 控制噪声必须产生有限、正的传递读数")
        g["G9_metric_non_degenerate_on_real_data"] = dict(
            green=bool(all(x["disc"] > 0 for x in lens)),
            value=dict(n_disc_positive=sum(1 for x in lens if x["disc"] > 0), n_total=len(lens)),
            requirement="真实数据上离散化相对 RMS 必须 >0（否则度量退化）")
        g["G10_nearest_additive_gain_identity_real"] = dict(
            green=bool(all(abs(x["gain_self"] - 1.0) <= 0.05 for x in lens
                           if x["op"] == OP_NEAREST and x["gain_self"] is not None)),
            value=[dict(arm=x["arm"], gain_self=x["gain_self"]) for x in lens
                   if x["op"] == OP_NEAREST and x["gain_self"] is not None][:8],
            requirement="真实网格上最近点算子加性噪声自归一化增益≡1（±5% 采样误差）")
        hom = h2["frames"][0].get("homogeneity_check")
        g["G13_positive_homogeneity"] = dict(
            green=bool(hom and hom["pointwise_max_rel_dev"] < 1e-12 and hom["rel_dev"] < 1e-12
                       and abs(hom["clip_low_ratio"] - 1000.0) < 1e-9
                       and abs(hom["clip_high_ratio"] - 1000.0) < 1e-9),
            value=(hom or {}),
            requirement="R(K·v)=K·R(v) 逐点（K=1000，含钳制界等比缩放）且相对读数逐位不变 ⇒ "
                        "控制值幅度（含 F_ref 因子）不改变相对传递读数")
    if h5b is not None:
        lin = h5b["linearity"]
        g["G11_nonlinearity_is_detectable"] = dict(
            green=bool(lin[OP_MESH]["max_rel_superposition_residual"] > 1e-8),
            value={k: v["max_rel_superposition_residual"] for k, v in lin.items()},
            requirement="mesh_median 档叠加残差必须显著 >1e-8（否则说明该检验对非线性不敏感 ⇒ 判红）")
        g["G12_linear_operators_superpose"] = dict(
            green=bool(lin[OP_BILINEAR]["max_rel_superposition_residual"] <= 1e-12),
            value={k: lin[k]["max_rel_superposition_residual"] for k in lin},
            requirement="唯一无钳制/无滤波的线性算子（bilinear）叠加残差 ~1e-16；"
                        "钳制样条档 3e-3、mesh 档 7e-3 的残差即其非线性量级（按实测报告，不作门）")
        g["G14_impulse_predicts_gain_on_real_window"] = dict(
            green=bool(h5b["default_operator"]["pred_vs_measured_rel_dev"] <= 0.05),
            value=h5b["default_operator"],
            requirement="真实帧 1024² 全节点窗口上，默认算子的实测加性增益必须命中脉冲预测（5% 内）")

    res = dict(
        experiment="P2-M5 闭环：冻结默认算子在真实控制点网格上的传递检查（生产 SparseSnrReconstructor 直调）",
        finding="run/FINAL-07/审核包/科研审查/SCI-702_P2_跨帧绝对SNR_审查报告.md §6 P2-M5",
        seed=SEED, claimed_ctrl_noise_rel=CTRL_NOISE_REL, frozen_operator_tokens=ALL_TOKENS,
        production_link=dict(
            header=os.path.join(PROD_SRC, "include/astrocs/weight_chain.h"),
            source=os.path.join(PROD_SRC, "src/weight_chain.cpp"),
            sha256_source=sha256(os.path.join(PROD_SRC, "src/weight_chain.cpp")),
            sha256_header=sha256(os.path.join(PROD_SRC, "include/astrocs/weight_chain.h")),
            driver_source=os.path.join(_UNIT_CODE, "exp11_recon_driver.cpp"),
            driver_build=BUILD_SH, driver_binary=DRIVER,
            compile="g++ -O2 -std=c++17 -I <phase2_integrate>/include exp11_recon_driver.cpp "
                    "<phase2_integrate>/src/weight_chain.cpp -o exp11_recon_driver",
            api="astrocs::v6::p2weight::SparseSnrReconstructor::prepare/eval"
                "（等价单调用 reconstruct_sparse_snr）"),
        sections_s=dict(t), wall_s=time.perf_counter() - _T0,
        headline_primary_frame=(None if h2 is None else {
            "%s|%s" % (k, tok): dict(disc_rel_rms=r["mul"]["disc_rel_rms"], gain_mul=r["mul"]["gain"],
                                     gain_add=r["add"]["gain"], gain_mul_self=r["mul"].get("gain_selfnormalized"),
                                     inflation_ratio=r["mul"]["inflation_ratio"],
                                     n_control_points=arm["n_control_points"],
                                     n_invalid_control_points=arm["n_invalid_control_points"])
            for k, arm in h2["frames"][0]["arms"].items() for tok, r in arm["rows"].items()}),
        H0_driver_integrity=h0, H1_exp04_grid=h1, H5a_impulse_analytic=h5a,
        H3_fail_closed=h3, H4_clip=h4, H2_real_grid=h2, H5b_real=h5b, gates=g)
    all_green = all(bool(v["green"]) for v in g.values())
    if h2 is not None:
        se16 = h0["truth_estimator_se"]["P16_n256"]["rel_se"]
        rows = {}
        for f in h2["frames"]:
            for key, arm in f["arms"].items():
                for tok, r in arm["rows"].items():
                    m, ad = r["mul"], (r.get("add") or {})
                    cv = arm["ctrl_stats"]["std"] / arm["ctrl_stats"]["mean"]
                    # 真值估计器噪声折算到「以 std(truth) 为单位」：SE 是相对 cell 均值的相对量，
                    # disc 的分母是 std(truth)，两者差一个 CV(truth)。
                    cv_truth = m["std_ref"] / m["mean_recon"]
                    floor = se16 / cv_truth
                    rows["%s|%s|%s" % (f["meta"]["file"][:30], key, tok)] = dict(
                        disc_rel_rms=m["disc_rel_rms"],
                        cv_truth=cv_truth,
                        disc_estimator_floor=floor,
                        disc_corrected=float(np.sqrt(max(m["disc_rel_rms"] ** 2 - floor ** 2, 0.0))),
                        transfer_factor_mul=m.get("gain_selfnormalized"),
                        transfer_factor_add=ad.get("gain_selfnormalized"),
                        nominal_gain_mul=m["gain"], nominal_gain_add=ad.get("gain"),
                        ctrl_cv=cv, inflation_ratio=m["inflation_ratio"])
        res["transfer_summary"] = dict(
            truth_estimator_rel_se_P16=se16,
            transfer_factor_definition="RMS(R_noisy−R_clean)/RMS(控制值扰动) —— 与真值无关、与"
                                       "扰动模型无关的算子传递因子（= nominal_gain / ctrl_cv）",
            disc_correction="disc_estimator_floor = se(P16)/CV(truth)（把真值估计器的相对 SE "
                            "折算到以 std(truth) 为单位）⇒ disc_corrected = sqrt(max(disc² − floor², 0))。"
                            "注意控制点与真值场由**同一批像素**算出、其估计噪声正相关，故真实 disc 介于"
                            "未订正与全订正之间；disc/floor 同量级即说明真实帧 disc 主要不是算子误差。",
            rows=rows)
    res["gates_all_green"] = all_green
    res["red_gates"] = [k for k, v in g.items() if not v["green"]]
    res["honesty_boundaries"] = [
        "① 真实帧无逐像素真值：稠密参考是 P=16 稳健 σ 估计场（1.4826×MAD，256 px/cell），"
        "其单 cell 相对标准误由 H0b 的 MC 实测给出（约 6%）⇒ disc_rel_rms 含「参考估计噪声」上界；"
        "而 gain / noise_rel_rms / inflation 两臂共用同一参考 ⇒ 噪声传递读数不受该污染。",
        "② 查询点 = P=16 cell 中心（256²=65536），不是全部 4096² 像素；未做逐像素全帧扫描。",
        "③ Δ=32 只作加密对照（冻结默认是 sparse_snr.spacing_px=64，eng/packaging/config/defaults.json）。",
        "④ 4 个 token 全部实测，但 nearest_control_point_v1 在生产里是散点模式算子，在规则网格上被 "
        "fail-closed 拒绝（H3 已实测）⇒ 其传递读数取自「同一节点集的散点模式」，"
        "另以 H3 的拒绝行给出规则网格上的行为。",
        "⑤ 值域钳制不可单独关闭（生产把核/滤波/钳制绑成 token，weight_chain.h:88-118）⇒ "
        "「无钳制样条」在生产不可表达，只以 bilinear 无钳制档 + 值域界与钳制触发核验代替。",
        "⑥ H5b 的脉冲 Σw² 需要完整节点集，故只在主帧左上 1024²（16×16 个 Δ=64 单元）窗口上做；"
        "H5a 的脉冲在 512²/Δ64 全节点、16384 个抽样查询点上做。",
        "⑦ IDW 行是**代理算子**读数（不在冻结词表内），只用于解释 exp04 的 0.2287 与 1.06× 的出处，"
        "不作为本单元任何主张的依据。",
        "⑧ 控制值用 σ[ADU]（而非 SNR）：算子正齐次（含钳制与中值），H2 的同质性门验证了"
        "σ 与 SNR=K/σ 两种表示给出同一结论（相对偏差 <1e-9）。",
        "⑨ route3 其余审计脚本遵循「零仓库 import」；本脚本必须调用生产重建器，"
        "故 import 实验侧 sci_b_common + 编译生产源为独立只读驱动——这是本脚本与路线约定的已知偏差。",
    ]
    res["reproduce"] = dict(
        build="bash 实验/absolute-snr/code/exp11_build_driver.sh",
        run="python3 eng/tools/monitoring/mem_guard.py --max-rss-gb 4 --timeout 900 -- "
            "python3 实验/absolute-snr/code/audit/route3/exp11_frozen_operator_transfer.py",
        quick_repro="... 同前，附加 --skip-real（只跑 H0/H1/H5a/H3/H4，约 60 s）",
        results_json=os.path.abspath(a.out), driver_work_dir=WORK)
    os.makedirs(os.path.dirname(a.out), exist_ok=True)
    with open(a.out, "w", encoding="utf-8") as f:
        json.dump(res, f, indent=1, ensure_ascii=False)
    log("wrote %s  all_green=%s red=%s" % (a.out, all_green, res["red_gates"]))
    return res


if __name__ == "__main__":
    main()
