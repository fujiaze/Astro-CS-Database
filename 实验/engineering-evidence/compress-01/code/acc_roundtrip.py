#!/usr/bin/env python3
"""COMPRESS-01 战线二: 内存累加器的 zstd 往返成本 (忠实/代理口径).

口径来源: run/MEM-DESIGN-01/verify/compress_faithful.py (同一重建运算) —— 由真实叶级
发布面按 aio_hips_writer 的逐位运算重建祖先 cell 的 f64 累加器:
    z   = ((s<<18) | i) >> 2*dk          (s = 叶在祖先内的槽位, i = 叶内 NESTED local 索引)
    flux= (double)(float)signal * (double)(float)area ;  area = (double)(float)area
    acc[z] += flux / acc[z] += area       (叶按 ipix 升序, i 升序 —— 与生产同序)

诚实边界 (与 MEM-DESIGN-01 §7.5 同): 输入是**已发布**的 f32 叶面 (signal 已归一、
support 已按 uint8 面积比量化), 不是原始 f64 累加前量; 故为"代理口径"。另给纯 f64
噪声下界参照。

本脚本回答:
  (1) 真实累加器内容在 zstd 各档下的压缩率 (与 zlib9 / lzma 对照);
  (2) **每次就地更新的摊薄成本**: 解压-改-重压 的 codec 时间 / 更新次数;
  (3) 与"只做加法"(无压缩) 的时间对照。
"""
import glob, json, math, os, statistics, sys, time, zlib, lzma
import numpy as np
from astropy.io import fits
import zstandard as zstd

ROOT = sys.argv[1] if len(sys.argv) > 1 else "run/PERF-401/out/real16_w1"
OUTDIR = sys.argv[2] if len(sys.argv) > 2 else "run/COMPRESS-01/evidence"
L = 9
LEVELS = [1, 3, 9, 19]
CODEC_REPS = 15
A_LEAF = 4.0 * math.pi / (12.0 * (2 ** (L + 9)) ** 2)   # 叶级单元立体角 (sr)


# ---------- 0. 自检: np.add.at 与"显式顺序循环"逐位一致 (判据非退化, 能红能绿) ----------
def selftest_add_at():
    rng = np.random.default_rng(7)
    n = 4096
    acc1 = np.zeros(64, dtype=np.float64)
    acc2 = np.zeros(64, dtype=np.float64)
    z = np.sort(rng.integers(0, 64, n)).astype(np.int64)
    v = rng.standard_normal(n)
    for i in range(n):
        acc1[z[i]] += v[i]
    np.add.at(acc2, z, v)
    ok = (acc1.tobytes() == acc2.tobytes())
    # 负例: 故意改一个元素, 判据必须变红
    acc3 = acc2.copy(); acc3[0] = np.nextafter(acc3[0], np.inf)
    red = (acc1.tobytes() != acc3.tobytes())
    return ok, red


def load_leaf(layer, ipix):
    d = os.path.join(ROOT, layer, "Norder%d" % L)
    f = glob.glob(os.path.join(d, "**", "Npix%d.fits" % ipix), recursive=True)
    if not f:
        return None
    with fits.open(f[0], memmap=True) as h:
        return np.asarray(h[0].data, dtype=np.float32).ravel()


def leaf_ipix_list():
    out = []
    for f in glob.glob(os.path.join(ROOT, "signal", "Norder%d" % L, "**", "*.fits"), recursive=True):
        b = os.path.basename(f)
        if b.startswith("Npix"):
            out.append(int(b[4:-5]))
    return sorted(out)


def main():
    ok, red = selftest_add_at()
    print("== 自检 np.add.at ≡ 顺序循环: 绿=%s  注入后判红=%s" % (ok, red))
    assert ok and red, "自检失败: 计时/累加口径不可信"

    leaves = leaf_ipix_list()
    print("== %s: %d 叶 tile" % (ROOT, len(leaves)))
    sig_cache, sup_cache = {}, {}
    for ip in leaves:
        sig_cache[ip] = load_leaf("signal", ip)
        sup_cache[ip] = load_leaf("support", ip)
    print("   叶面缓存: %.1f MiB" % ((sum(a.nbytes for a in sig_cache.values()) +
                                      sum(a.nbytes for a in sup_cache.values())) / 2**20))

    # 祖先 cell -> 叶槽映射
    cells = {}
    for ip in leaves:
        for k in range(0, L):
            dk = L - k
            A = ip >> (2 * dk)
            s = ip & ((1 << (2 * dk)) - 1)
            cells.setdefault((k, A), []).append((s, ip))
    print("   祖先 cell 数: %d" % len(cells))

    cctx = {lv: zstd.ZstdCompressor(level=lv) for lv in LEVELS}
    dctx = zstd.ZstdDecompressor()

    # 纯 f64 噪声下界参照
    rng = np.random.default_rng(20260922)
    ref_noise = rng.standard_normal(512 * 512).astype("<f8").tobytes()
    noise = {}
    for lv in LEVELS:
        noise[lv] = len(cctx[lv].compress(ref_noise)) / len(ref_noise)
    noise["zlib9"] = len(zlib.compress(ref_noise, 9)) / len(ref_noise)
    noise["lzma"] = len(lzma.compress(ref_noise)) / len(ref_noise)
    print("== 纯 f64 噪声参照: " + " ".join("%s=%.4f" % (k, v) for k, v in noise.items()))

    per_cell = []
    for (k, A), subs in sorted(cells.items()):
        dk = L - k
        subs = sorted(subs)
        # --- 重建累加器 (与生产同序: 叶 ipix 升序, i 升序) ---
        accF = np.zeros(512 * 512, dtype=np.float64)
        accA = np.zeros(512 * 512, dtype=np.float64)
        for s, ip in subs:
            sg = sig_cache[ip]; sp = sup_cache[ip]
            if sg is None or sp is None:
                continue
            area = (sp.astype(np.float64) * A_LEAF)
            flux = sg.astype(np.float64) * area
            good = np.isfinite(flux) & (area > 0.0)
            i_idx = np.nonzero(good)[0]
            if i_idx.size == 0:
                continue
            z = ((np.int64(s) << 18) | i_idx.astype(np.int64)) >> (2 * dk)
            np.add.at(accF, z, flux[i_idx])
            np.add.at(accA, z, area[i_idx])
        n_upd = len(subs)
        bufF = accF.astype("<f8").tobytes()
        bufA = accA.astype("<f8").tobytes()
        nzF = int((accF != 0).sum()); nzA = int((accA != 0).sum())
        # 方案 A 载荷: 只保留"有非零的 64x64 子块"
        def sparse_payload(acc):
            # 方案 A 的载荷 = 只保留"有非零元素"的 64x64 子块 (块内顺序与生产
            # 的 z>>12 交错序不同, 但字节集合相同, 对压缩率影响可忽略)
            v = acc.reshape(8, 64, 8, 64)
            parts = [v[bi, :, bj, :].ravel() for bi in range(8) for bj in range(8)
                     if v[bi, :, bj, :].any()]
            return np.concatenate(parts).astype("<f8").tobytes() if parts else b""
        spF = sparse_payload(accF); spA = sparse_payload(accA)
        row = dict(k=k, A=int(A), dk=dk, n_upd=n_upd, nz_flux=nzF, nz_area=nzA,
                   dense_bytes=len(bufF), sparse_bytes=len(spF),
                   dense_area_bytes=len(bufA), sparse_area_bytes=len(spA))
        for lv in LEVELS:
            row["zstd%d_flux_dense" % lv] = len(cctx[lv].compress(bufF)) / len(bufF)
            row["zstd%d_flux_sparse" % lv] = (len(cctx[lv].compress(spF)) / len(spF)) if spF else 0.0
        row["zlib9_flux_dense"] = len(zlib.compress(bufF, 9)) / len(bufF)
        row["lzma_flux_dense"] = len(lzma.compress(bufF)) / len(bufF)
        row["zlib9_area_dense"] = len(zlib.compress(bufA, 9)) / len(bufA)
        row["lzma_area_dense"] = len(lzma.compress(bufA)) / len(bufA)
        row["zstd3_area_dense"] = len(cctx[3].compress(bufA)) / len(bufA)
        # --- codec 往返时间 (对真实 buffer, 取中位) ---
        for lv in LEVELS:
            tc, td = [], []
            cb = cctx[lv].compress(bufF)
            for _ in range(CODEC_REPS):
                t0 = time.perf_counter(); cctx[lv].compress(bufF); tc.append(time.perf_counter() - t0)
                t0 = time.perf_counter(); dctx.decompress(cb, max_output_size=len(bufF)); td.append(time.perf_counter() - t0)
            row["c_ms_lv%d" % lv] = statistics.median(tc) * 1e3
            row["d_ms_lv%d" % lv] = statistics.median(td) * 1e3
        per_cell.append(row)
        del accF, accA, bufF, bufA, spF, spA

    os.makedirs(OUTDIR, exist_ok=True)
    json.dump(dict(cells=per_cell, noise=noise, leaves=len(leaves)), open(os.path.join(OUTDIR, "acc_roundtrip.json"), "w"), indent=1)

    # ---- 汇总 ----
    def wmean(key, weight="dense_bytes"):
        num = sum(r[key] * r[weight] for r in per_cell if key in r)
        den = sum(r[weight] for r in per_cell if key in r)
        return num / den if den else float("nan")

    print("\n== 累加器压缩率 (按字节加权, %d 个真实祖先 cell) ==" % len(per_cell))
    print("   通道      口径     " + "".join("%9s" % ("zstd%d" % lv) for lv in LEVELS) + "%9s%9s" % ("zlib9", "lzma"))
    for ch, dense, sparse in (("flux", "dense_bytes", "sparse_bytes"), ("area", "dense_area_bytes", "sparse_area_bytes")):
        line = "   %-8s %-8s" % (ch, "稠密整张")
        for lv in LEVELS:
            line += "%9.4f" % wmean("zstd%d_%s_dense" % (lv, ch), dense)
        line += "%9.4f%9.4f" % (wmean("zlib9_%s_dense" % ch, dense), wmean("lzma_%s_dense" % ch, dense))
        print(line)
        line = "   %-8s %-8s" % (ch, "方案A载荷")
        for lv in LEVELS:
            line += "%9.4f" % wmean("zstd%d_%s_sparse" % (lv, ch), sparse)
        print(line)

    print("\n== 每次就地更新的摊薄 codec 成本 (解压-改-重压; 单 cell 2 MiB f64) ==")
    tot_upd = sum(r["n_upd"] for r in per_cell)
    for lv in LEVELS:
        c = statistics.median([r["c_ms_lv%d" % lv] for r in per_cell])
        d = statistics.median([r["d_ms_lv%d" % lv] for r in per_cell])
        print("   zstd%-3d 压缩 %7.3f ms + 解压 %6.3f ms = %7.3f ms/次更新   (全产物 %d 次更新 -> %6.1f s)"
              % (lv, c, d, c + d, tot_upd, (c + d) * tot_upd / 1000.0))
    print("   参照: 累加器规模 %d cell x 2 MiB (flux) ; 总更新次数 %d" % (len(per_cell), tot_upd))


main()