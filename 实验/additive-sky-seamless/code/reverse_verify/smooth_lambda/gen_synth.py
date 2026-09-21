#!/usr/bin/env python3
"""RELEASE-02 A5 合成场景生成器（物理噪声链）。

真值：公共天文场景 M_true(k)（含 M42/M16 式突兀亮度峰值）+ 逐帧加性天光 B_f(k)。
观测：对每个 (frame, control) 生成 64x64 patch 的**物理**像素：
    I(p)   = [ (M_base + Core_f)(k) * t_f + B_f(k) ] * (1 + tex(p))     [场景单位]
    mu_e   = I(p) * gain                                                [电子]
    n_e    = Poisson(mu_e) + Poisson(dark_e) + Normal(0, read_e)
    adu    = n_e / gain
  control estimator = patch median；sigma_bg = 1.4826*MAD；
  control_variance = k_corr*(pi/2)*sigma_bg^2/N_retained；control_ivar = 1/cv。

几何/覆盖 = **真实 L4 p2_samples.json**（control 位置、tile、leaf_ipix、逐帧覆盖子集全部照搬）。
输出：UPMB v1 二进制（喂给链接真实 upm.cpp 的 upm_sweep）+ 真值 npz。
"""
import argparse, json, math, os, sys
import numpy as np

ROOT = "/workspace/Astro CS Database"
REAL = os.path.join(ROOT, "run/RELEASE-02/L4-rebuild/mosaic_out/p2_samples.json")
OUTDIR = os.path.join(ROOT, "run/reverse_verify/smooth_lambda")
K_CORR = 1.4
PATCH = 64

def load_geometry():
    d = json.load(open(REAL))
    ctrls = d["controls"]
    obs = d["observations"]
    frames = list(d["frame_ids"])
    fidx = {f: i for i, f in enumerate(frames)}
    K = len(ctrls)
    cid2k = {c["control_id"]: i for i, c in enumerate(ctrls)}
    ra = np.array([c["ra_deg"] for c in ctrls])
    dec = np.array([c["dec_deg"] for c in ctrls])
    tile = np.array([c["tile_ipix"] for c in ctrls], np.uint64)
    gx = np.array([c["gx"] for c in ctrls], np.int32)
    gy = np.array([c["gy"] for c in ctrls], np.int32)
    leaf = np.array([c["leaf_ipix"] for c in ctrls], np.uint64)
    o_f = np.array([fidx[o["frame_id"]] for o in obs], np.int32)
    o_k = np.array([cid2k[o["control_id"]] for o in obs], np.int32)
    return dict(frames=frames, ctrls=ctrls, K=K, ra=ra, dec=dec, tile=tile,
                gx=gx, gy=gy, leaf=leaf, o_f=o_f, o_k=o_k, n_obs=len(obs),
                F=len(frames))

def sky_surfaces(G, args, rng):
    """真值面：公共场景 M_base + Core（可逐帧变化）+ 逐帧加性天光 B_f。"""
    ra, dec, K, F = G["ra"], G["dec"], G["K"], G["F"]
    # 归一化坐标（覆盖足迹 [-1,1]）
    x = (ra - ra.mean()) / (0.5 * (ra.max() - ra.min()))
    y = (dec - dec.mean()) / (0.5 * (dec.max() - dec.min()))
    # 公共大尺度背景（低阶）
    M_base = args.sky_base * (1.0 + 0.35 * x - 0.20 * y + 0.15 * x * y
                              - 0.10 * x * x + 0.08 * y * y)
    # 突兀亮度峰值（M16/M42 核心式）：窄高斯，幅度 A_core，宽度 sigma_core(deg)
    r0, d0 = args.core_ra, args.core_dec
    dra = (ra - r0) * math.cos(math.radians(d0))
    ddec = dec - d0
    r2 = (dra ** 2 + ddec ** 2)
    core_prof = np.exp(-0.5 * r2 / (args.core_sigma ** 2))
    # 逐帧乘性透过率（全局，UPM 纯加性模型无法吸收 → 真实压力源）
    tau = rng.normal(0.0, args.tau_sigma, F) if args.tau_sigma > 0 else np.zeros(F)
    # 核心的逐帧变化（负责人观察：各帧一致 vs 帧间不同）
    if args.core_mode == "consistent":
        eps = np.zeros(F); eta = np.zeros(F)
    else:
        eps = rng.normal(0.0, args.core_var, F)
        eta = rng.normal(0.0, args.core_var, F)
    # 逐帧加性天光：低阶面（offset + 梯度 + 二阶）
    boff = rng.normal(0.0, args.sky_off_sigma, F)
    bgx = rng.normal(0.0, args.sky_grad_sigma, F)
    bgy = rng.normal(0.0, args.sky_grad_sigma, F)
    bq = rng.normal(0.0, args.sky_grad_sigma, F)
    B = (boff[:, None] + bgx[:, None] * x[None, :] + bgy[:, None] * y[None, :]
         + bq[:, None] * (x[None, :] ** 2 - y[None, :] ** 2))
    if args.no_sky:
        B = np.zeros_like(B)
    # 每帧每 control 的场景电平
    Core = np.zeros((F, K))
    for f in range(F):
        Core[f] = args.core_amp * (1.0 + eps[f]) * np.exp(
            -0.5 * r2 / (args.core_sigma * (1.0 + eta[f])) ** 2)
    S_f = (M_base[None, :] + Core) * (1.0 + tau[:, None])
    M_true = M_base + (args.core_amp * core_prof if not args.no_core else 0.0)
    M_target = S_f + B                    # 每帧真值观测面
    return dict(M_base=M_base, M_true=M_true, Core=Core, S_f=S_f, B=B,
                M_target=M_target, x=x, y=y, tau=tau, core_prof=core_prof)

def simulate(G, T, args, rng):
    """物理像素模拟 → value/uncertainty/control_ivar（逐 obs）。"""
    F, K, n_obs = G["F"], G["K"], G["n_obs"]
    o_f, o_k = G["o_f"], G["o_k"]
    # 固定纹理场（公共结构，所有帧同一实现）——决定 patch 空间 MAD
    tex = rng.normal(0.0, 1.0, (PATCH, PATCH))
    tex = tex - np.median(tex)
    tex = tex / (1.4826 * np.median(np.abs(tex - np.median(tex))))
    tex = tex * args.tex_mad
    tex = tex.ravel()
    value = np.zeros(n_obs); unc = np.zeros(n_obs)
    cv = np.zeros(n_obs); civ = np.zeros(n_obs); nret = np.zeros(n_obs, np.int32)
    lvl = T["M_target"][o_f, o_k]           # 逐 obs 的期望电平（场景单位）
    CH = args.chunk
    for s in range(0, n_obs, CH):
        e = min(n_obs, s + CH)
        L = lvl[s:e, None] * (1.0 + tex[None, :])          # (m, P)
        mu = L * args.gain
        n_e = rng.poisson(np.maximum(mu, 0.0))
        if args.dark_e > 0:
            n_e = n_e + rng.poisson(args.dark_e, size=n_e.shape)
        if args.read_e > 0:
            n_e = n_e + rng.normal(0.0, args.read_e, size=n_e.shape)
        adu = n_e / args.gain
        med = np.median(adu, axis=1)
        mad = np.median(np.abs(adu - med[:, None]), axis=1)
        sig = 1.4826 * mad
        value[s:e] = med
        # SCI-UPM-WEIGHT-001 合同公式：N_retained = 整个 patch（PATCH×PATCH）
        cvar = K_CORR * (math.pi / 2.0) * sig ** 2 / (PATCH * PATCH)
        cvar = np.maximum(cvar, 1e-300)
        cv[s:e] = cvar; civ[s:e] = 1.0 / cvar
        unc[s:e] = np.sqrt(cvar)
        nret[s:e] = PATCH * PATCH  # N_retained
    return dict(value=value, uncertainty=unc, control_variance=cv,
                control_ivar=civ, n_retained=nret)

def write_upmb(path, G, sim, quality=1):
    with open(path, "wb") as f:
        f.write(np.uint32(0x55504D31).tobytes()); f.write(np.uint32(1).tobytes())
        f.write(np.uint64(G["n_obs"]).tobytes()); f.write(np.uint64(G["K"]).tobytes())
        f.write(np.uint64(G["F"]).tobytes())
        f.write(np.array(G["frames"], np.uint64).tobytes())
        dt = np.dtype([("frame_id","<u8"),("control_id","<u8"),("leaf_ipix","<u8"),
                       ("ra","<f8"),("dec","<f8"),("value","<f8"),("unc","<f8"),
                       ("snr","<f8"),("ivar","<f8"),("cvar","<f8"),("civar","<f8"),
                       ("snrav","<i4"),("support","<f8"),("qflags","<u4")])
        # 逐 obs 的 leaf/ra/dec 取自 control
        o_k = G["o_k"]
        arr = np.zeros(G["n_obs"], dt)
        arr["frame_id"] = np.array(G["frames"], np.uint64)[G["o_f"]]
        arr["control_id"] = np.array([c["control_id"] for c in G["ctrls"]], np.uint64)[o_k]
        arr["leaf_ipix"] = G["leaf"][o_k]
        arr["ra"] = G["ra"][o_k]; arr["dec"] = G["dec"][o_k]
        arr["value"] = sim["value"]; arr["unc"] = sim["uncertainty"]
        arr["snr"] = 0.0; arr["ivar"] = 0.0
        arr["cvar"] = sim["control_variance"]; arr["civar"] = sim["control_ivar"]
        arr["snrav"] = 0; arr["support"] = 1.0; arr["qflags"] = quality
        f.write(arr.tobytes())
        nd = np.dtype([("control_id","<u8"),("tile_ipix","<u8"),("gx","<i4"),("gy","<i4"),
                       ("ra","<f8"),("dec","<f8"),("leaf_ipix","<u8")])
        na = np.zeros(G["K"], nd)
        na["control_id"] = np.array([c["control_id"] for c in G["ctrls"]], np.uint64)
        na["tile_ipix"] = G["tile"]; na["gx"] = G["gx"]; na["gy"] = G["gy"]
        na["ra"] = G["ra"]; na["dec"] = G["dec"]; na["leaf_ipix"] = G["leaf"]
        f.write(na.tobytes())
    return os.path.getsize(path)

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", required=True)
    ap.add_argument("--scene", default="base")
    ap.add_argument("--sky-base", type=float, default=1.0e4)
    ap.add_argument("--core-amp", type=float, default=3.0e5)
    ap.add_argument("--core-ra", type=float, default=83.82)
    ap.add_argument("--core-dec", type=float, default=-5.39)
    ap.add_argument("--core-sigma", type=float, default=0.10)
    ap.add_argument("--core-mode", default="consistent", choices=["consistent","varying"])
    ap.add_argument("--core-var", type=float, default=0.10)
    ap.add_argument("--tau-sigma", type=float, default=0.0)
    ap.add_argument("--sky-off-sigma", type=float, default=0.0)
    ap.add_argument("--sky-grad-sigma", type=float, default=0.0)
    ap.add_argument("--no-sky", action="store_true")
    ap.add_argument("--no-core", action="store_true")
    ap.add_argument("--tex-mad", type=float, default=0.0)
    ap.add_argument("--gain", type=float, default=2.0)
    ap.add_argument("--read-e", type=float, default=5.0)
    ap.add_argument("--dark-e", type=float, default=3.0)
    ap.add_argument("--chunk", type=int, default=1024)
    ap.add_argument("--seed", type=int, default=20260919)
    ap.add_argument("--scale", type=float, default=1.0, help="输出单位缩放（仅标签，物理不变）")
    args = ap.parse_args()
    os.makedirs(OUTDIR, exist_ok=True)
    rng = np.random.default_rng(args.seed)
    G = load_geometry()
    T = sky_surfaces(G, args, rng)
    sim = simulate(G, T, args, rng)
    # 单位缩放（纯标签：value×s、variance×s² ⇒ z=r/sigma 与归一化权重逐位不变）
    sim["value"] = sim["value"] * args.scale
    sim["control_variance"] = sim["control_variance"] * args.scale ** 2
    sim["uncertainty"] = np.sqrt(sim["control_variance"])
    sim["control_ivar"] = 1.0 / np.maximum(sim["control_variance"], 1e-300)
    p = os.path.join(OUTDIR, args.out + ".upmb")
    sz = write_upmb(p, G, sim)
    np.savez_compressed(os.path.join(OUTDIR, args.out + ".truth.npz"),
        M_true=T["M_true"], M_base=T["M_base"], Core=T["Core"], B=T["B"],
        M_target=T["M_target"], x=T["x"], y=T["y"], tau=T["tau"],
        value=sim["value"], uncertainty=sim["uncertainty"],
        control_ivar=sim["control_ivar"], o_f=G["o_f"], o_k=G["o_k"],
        ra=G["ra"], dec=G["dec"])
    st = dict(scene=args.scene, vars=vars(args), upmb=p, bytes=sz,
              value_median=float(np.median(sim["value"])),
              unc_median=float(np.median(sim["uncertainty"])),
              civar_median=float(np.median(sim["control_ivar"])))
    json.dump(st, open(os.path.join(OUTDIR, args.out + ".gen.json"), "w"), indent=1)
    print(json.dumps({k: st[k] for k in ("scene","upmb","bytes","value_median","unc_median","civar_median")}, indent=1))

if __name__ == "__main__":
    main()
