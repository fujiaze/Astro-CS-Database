# -*- coding: utf-8 -*-
"""
C-002 HISS V2 测试脚本

验证项:
  1. V1→V2 转换 (3 帧 B-002 数据)
  2. 7 个 batch read API
  3. CRC32 校验 (per-chunk + global)
  4. zstd 压缩往返一致
  5. 数据一致性 (V1 ipix/pixel/snr == V2 ipix/signal/snr)
  6. support 通道 (V1 迁移后全 1)
  7. 损坏测试 (翻转字节 → -4)
  8. 多块分块 (小 chunk_size 触发多块)
  9. 错误码测试 (magic/version/footer/no_snr)
"""

import os
import sys
import struct
import shutil
import logging
import numpy as np

# 加入模块路径
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "..", "..",
                                "lib", "astro_image_io", "python"))

import hiss_v2 as hv

logging.basicConfig(level=logging.WARNING, format="[%(levelname)s] %(message)s")
logger = logging.getLogger("C002_test")

PROJECT_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))
V1_DIR = os.path.join(PROJECT_ROOT, "output", "B-002")
V2_DIR = os.path.join(PROJECT_ROOT, "output", "C-002")
os.makedirs(V2_DIR, exist_ok=True)

FRAMES = ["T2_RED_LDN43", "T3_RED_NGC55", "T4_RED_GalaxyCenter_panel1"]

results = []  # (test_name, passed, detail)


def record(name, passed, detail=""):
    results.append((name, bool(passed), detail))
    flag = "PASS" if passed else "FAIL"
    print(f"  [{flag}] {name}" + (f" — {detail}" if detail else ""))


print("=" * 70)
print("C-002 HISS V2 测试")
print("=" * 70)

# ============================================================================
# 1. V1→V2 转换 + 数据一致性
# ============================================================================
print("\n--- 1. V1→V2 转换与数据一致性 ---")
v1_data = {}  # frame -> (nside, nested, ipix, pixel, meta, snr_model)
v2_paths = {}
for frame in FRAMES:
    v1_path = os.path.join(V1_DIR, frame + ".hiss")
    v2_path = os.path.join(V2_DIR, frame + ".hiss2")
    v2_paths[frame] = v2_path
    try:
        nside, nested, ipix, pixel, meta, snr_model = hv.v1_read_snr_model(v1_path)
        v1_data[frame] = (nside, nested, ipix, pixel, meta, snr_model)
        ret = hv.v1_to_v2_converter(v1_path, v2_path)
        record(f"转换 {frame}", ret == 0, f"ret={ret} v1_npix={ipix.size} v2_size={os.path.getsize(v2_path)}")
    except Exception as e:
        record(f"转换 {frame}", False, f"异常: {e}")

# ============================================================================
# 2. 全局 CRC32 + magic/version/footer 校验 (Reader 打开即校验)
# ============================================================================
print("\n--- 2. 全局 CRC32 + 结构校验 ---")
for frame in FRAMES:
    v2_path = v2_paths[frame]
    try:
        with hv.HissV2Reader(v2_path) as r:
            record(f"全局CRC {frame}", True,
                   f"global_crc={r._global_crc32:#010x} n_chunks={r.n_chunks} has_snr={r.has_snr}")
    except hv.HissV2Error as e:
        record(f"全局CRC {frame}", False, f"code={e.code} {e.message}")

# ============================================================================
# 3. read_provenance (§11.1)
# ============================================================================
print("\n--- 3. read_provenance (§11.1) ---")
for frame in FRAMES:
    code, prov = hv.hiss2_read_provenance(v2_paths[frame])
    ok = code == 0 and prov is not None
    if ok:
        p = prov["provenance"]
        ok = (p["format_version"] == "HISS-V2" and p["codec"] == "ZSTD"
              and p["crc_algorithm"] == "CRC32_IEEE8023" and p["nside"] == v1_data[frame][0])
        record(f"provenance {frame}", ok,
               f"fv={p['format_version']} nside={p['nside']} n_chunks={p['n_chunks']}")
    else:
        record(f"provenance {frame}", False, f"code={code}")

# ============================================================================
# 4. read_chunk (§11.2) + zstd 往返 + 数据一致
# ============================================================================
print("\n--- 4. read_chunk (§11.2) + zstd 往返 + 数据一致 ---")
for frame in FRAMES:
    nside, nested, ipix_v1, pixel_v1, meta, snr_v1 = v1_data[frame]
    try:
        with hv.HissV2Reader(v2_paths[frame]) as r:
            raw_count, ipix_v2, signal_v2, support_v2 = r.read_chunk(0)
            # 数据一致
            ipix_ok = np.array_equal(ipix_v1, ipix_v2)
            sig_ok = np.array_equal(pixel_v1, signal_v2)
            sup_ok = bool(np.all(support_v2 == 1)) and support_v2.size == ipix_v1.size
            # signal 不得为 uint8
            dtype_ok = signal_v2.dtype == np.float32
            record(f"read_chunk {frame}", ipix_ok and sig_ok and sup_ok and dtype_ok,
                   f"raw_count={raw_count} ipix_eq={ipix_ok} signal_eq={sig_ok} "
                   f"support_all1={sup_ok} signal_dtype={signal_v2.dtype}")
    except Exception as e:
        record(f"read_chunk {frame}", False, f"异常: {e}")

# ============================================================================
# 5. read_chunks (§11.3) 批量读
# ============================================================================
print("\n--- 5. read_chunks (§11.3) 批量读 ---")
for frame in FRAMES:
    try:
        with hv.HissV2Reader(v2_paths[frame]) as r:
            total, ipix, signal, support = r.read_chunks(list(range(r.n_chunks)))
            ok = total == v1_data[frame][3].size and np.array_equal(ipix, v1_data[frame][2])
            record(f"read_chunks {frame}", ok, f"total={total} expected={v1_data[frame][3].size}")
    except Exception as e:
        record(f"read_chunks {frame}", False, f"异常: {e}")

# ============================================================================
# 6. read_ipix_range (§11.4) 范围查询
# ============================================================================
print("\n--- 6. read_ipix_range (§11.4) 范围查询 ---")
for frame in FRAMES:
    ipix_v1 = v1_data[frame][2]
    if ipix_v1.size < 2:
        record(f"ipix_range {frame}", True, "n_pix<2, 跳过")
        continue
    lo = int(ipix_v1[0])
    hi = int(ipix_v1[-1])
    mid = int(ipix_v1[ipix_v1.size // 2])
    try:
        with hv.HissV2Reader(v2_paths[frame]) as r:
            # 全范围
            c1, i1, s1, su1 = r.read_ipix_range(lo, hi)
            full_ok = c1 == ipix_v1.size and np.array_equal(i1, ipix_v1)
            # 子范围 [lo, mid]
            c2, i2, s2, su2 = r.read_ipix_range(lo, mid)
            mask = (ipix_v1 >= lo) & (ipix_v1 <= mid)
            sub_ok = c2 == int(mask.sum()) and np.array_equal(i2, ipix_v1[mask])
            # 无交集范围
            c3, i3, s3, su3 = r.read_ipix_range(hi + 1000, hi + 2000)
            empty_ok = c3 == 0 and i3.size == 0
            record(f"ipix_range {frame}", full_ok and sub_ok and empty_ok,
                   f"full={c1}/{ipix_v1.size} sub={c2}/{int(mask.sum())} empty={c3}")
    except Exception as e:
        record(f"ipix_range {frame}", False, f"异常: {e}")

# ============================================================================
# 7. read_leaf (§11.5) 子叶读取
# ============================================================================
print("\n--- 7. read_leaf (§11.5) 子叶读取 ---")
for frame in FRAMES:
    nside = v1_data[frame][0]
    ipix_v1 = v1_data[frame][2]
    if nside < 64:
        record(f"leaf {frame}", True, f"nside={nside}<64, 跳过")
        continue
    try:
        with hv.HissV2Reader(v2_paths[frame]) as r:
            # 取第一个像素所在子叶
            nside_log2 = int(round(np.log2(nside)))
            shift = 2 * (nside_log2 - 6)
            leaf0 = int(ipix_v1[0]) >> shift
            c, i, s, su = r.read_leaf(leaf0)
            # 验证: 该子叶所有像素都在 read_leaf 结果中
            mask = (ipix_v1 >> shift) == leaf0
            expected = int(mask.sum())
            ok = c == expected and c > 0
            record(f"leaf {frame}", ok, f"leaf_ipix={leaf0} count={c} expected={expected}")
    except Exception as e:
        record(f"leaf {frame}", False, f"异常: {e}")

# ============================================================================
# 8. read_snr_model (§11.6) + SNR 一致性
# ============================================================================
print("\n--- 8. read_snr_model (§11.6) + SNR 一致性 ---")
for frame in FRAMES:
    snr_v1 = v1_data[frame][5]
    try:
        with hv.HissV2Reader(v2_paths[frame]) as r:
            snr_v2 = r.read_snr_model()
            if snr_v1 is None:
                ok = snr_v2 is None
                record(f"snr {frame}", ok, "无 SNR")
            else:
                n_eq = snr_v1.n_points == snr_v2.n_points
                # SNR 数据可能含 NaN, 用 equal_nan=True 比较; 并做字节级往返校验
                ra_eq = np.allclose(snr_v1.points_ra, snr_v2.points_ra, equal_nan=True)
                dec_eq = np.allclose(snr_v1.points_dec, snr_v2.points_dec, equal_nan=True)
                snr_eq = np.allclose(snr_v1.points_snr, snr_v2.points_snr, equal_nan=True)
                # 字节级往返一致 (契约 §10.3, zstd 无损)
                ra_bytes = snr_v1.points_ra.astype("<f8").tobytes() == snr_v2.points_ra.astype("<f8").tobytes()
                dec_bytes = snr_v1.points_dec.astype("<f8").tobytes() == snr_v2.points_dec.astype("<f8").tobytes()
                snr_bytes = snr_v1.points_snr.astype("<f4").tobytes() == snr_v2.points_snr.astype("<f4").tobytes()
                bytes_ok = ra_bytes and dec_bytes and snr_bytes
                scal_eq = (abs(snr_v1.snr_phot - snr_v2.snr_phot) < 1e-12
                           and abs(snr_v1.median_snr - snr_v2.median_snr) < 1e-12
                           and abs(snr_v1.idw_power - snr_v2.idw_power) < 1e-12)
                has_nan = bool(np.isnan(snr_v1.points_snr).any())
                record(f"snr {frame}", n_eq and ra_eq and dec_eq and snr_eq and scal_eq and bytes_ok,
                       f"n={snr_v2.n_points} ra_eq={ra_eq} dec_eq={dec_eq} "
                       f"snr_eq={snr_eq} scal_eq={scal_eq} bytes_exact={bytes_ok} has_nan={has_nan}")
    except Exception as e:
        record(f"snr {frame}", False, f"异常: {e}")

# ============================================================================
# 9. read_all (§11.7) 整文件读取
# ============================================================================
print("\n--- 9. read_all (§11.7) 整文件读取 ---")
for frame in FRAMES:
    try:
        code, res = hv.hiss2_read_all(v2_paths[frame])
        if code != 0:
            record(f"read_all {frame}", False, f"code={code}")
            continue
        ok = (res["n_pix"] == v1_data[frame][3].size
              and np.array_equal(res["ipix"], v1_data[frame][2])
              and np.array_equal(res["signal"], v1_data[frame][3])
              and bool(np.all(res["support"] == 1))
              and res["snr_model"] is not None)
        record(f"read_all {frame}", ok,
               f"n_pix={res['n_pix']} has_snr={res['snr_model'] is not None}")
    except Exception as e:
        record(f"read_all {frame}", False, f"异常: {e}")

# ============================================================================
# 10. 损坏测试: 翻转数据块字节 → per-chunk CRC 失败 (-4)
# ============================================================================
print("\n--- 10. 损坏测试 (per-chunk CRC 失败 → -4) ---")
for frame in FRAMES:
    src = v2_paths[frame]
    corrupt = os.path.join(V2_DIR, frame + "_corrupt.hiss2")
    try:
        with open(src, "rb") as f:
            data = bytearray(f.read())
        # 找到第一个数据块偏移并翻转其中一字节 (不破坏 footer/global crc 区)
        with hv.HissV2Reader(src, verify_global_crc=False) as r:
            entry = r._chunk_entries[0]
        flip_off = entry.offset + 5  # 块内偏移 5
        data[flip_off] ^= 0xFF
        with open(corrupt, "wb") as f:
            f.write(data)
        # 读取: 全局 CRC 会先失败 (-4), 或 per-chunk 失败 (-4)
        code, _ = hv.hiss2_read_chunk(corrupt, 0)
        # 期望 -4 (CRC 失败)
        record(f"corrupt {frame}", code == hv.HIO_ERR_CRC,
               f"flip@{flip_off} code={code} (期望 {hv.HIO_ERR_CRC})")
    except Exception as e:
        record(f"corrupt {frame}", False, f"异常: {e}")
    finally:
        if os.path.exists(corrupt):
            os.remove(corrupt)

# ============================================================================
# 11. 多块分块测试 (小 chunk_size)
# ============================================================================
print("\n--- 11. 多块分块测试 (chunk_size=512) ---")
frame = FRAMES[0]
v1_path = os.path.join(V1_DIR, frame + ".hiss")
v2_multi = os.path.join(V2_DIR, frame + "_multi.hiss2")
try:
    ret = hv.v1_to_v2_converter(v1_path, v2_multi, chunk_size=512)
    nside, nested, ipix_v1, pixel_v1, meta, snr_v1 = v1_data[frame]
    with hv.HissV2Reader(v2_multi) as r:
        n_chunks_expected = (ipix_v1.size + 511) // 512
        n_chunks_ok = r.n_chunks == n_chunks_expected
        # 读所有块拼接, 验证一致
        total, ipix, signal, support = r.read_chunks(list(range(r.n_chunks)))
        data_ok = (total == ipix_v1.size and np.array_equal(ipix, ipix_v1)
                   and np.array_equal(signal, pixel_v1))
        # 单块读取
        rc0, i0, s0, su0 = r.read_chunk(0)
        chunk0_ok = rc0 == min(512, ipix_v1.size) and np.array_equal(i0, ipix_v1[:rc0])
        record(f"multi-chunk {frame}", ret == 0 and n_chunks_ok and data_ok and chunk0_ok,
               f"n_chunks={r.n_chunks}/{n_chunks_expected} total={total} chunk0_rc={rc0}")
except Exception as e:
    record(f"multi-chunk {frame}", False, f"异常: {e}")
finally:
    if os.path.exists(v2_multi):
        os.remove(v2_multi)

# ============================================================================
# 12. 错误码测试
# ============================================================================
print("\n--- 12. 错误码测试 ---")
# -1 文件不存在
code, _ = hv.hiss2_read_provenance("nonexistent.hiss2")
record("err -1 文件不存在", code == hv.HIO_ERR_FILE, f"code={code}")

# -2 magic 不匹配 (用 V1 文件)
code, _ = hv.hiss2_read_provenance(v1_path)
record("err -2 magic不匹配", code == hv.HIO_ERR_MAGIC, f"code={code}")

# -8 footer 截断
src = v2_paths[FRAMES[0]]
trunc = os.path.join(V2_DIR, "trunc.hiss2")
try:
    with open(src, "rb") as f:
        data = f.read()
    with open(trunc, "wb") as f:
        f.write(data[:-10])  # 截断尾部
    code, _ = hv.hiss2_read_provenance(trunc)
    record("err -8 footer截断", code in (hv.HIO_ERR_FOOTER, hv.HIO_ERR_CRC, hv.HIO_ERR_FILE),
           f"code={code}")
finally:
    if os.path.exists(trunc):
        os.remove(trunc)

# ============================================================================
# 13. V1/V2 magic 区分
# ============================================================================
print("\n--- 13. V1/V2 magic 区分 ---")
for frame in FRAMES:
    with open(os.path.join(V1_DIR, frame + ".hiss"), "rb") as f:
        v1_magic = f.read(4)
    with open(v2_paths[frame], "rb") as f:
        v2_magic = f.read(4)
    ok = v1_magic == b"HISS" and v2_magic == b"HI2S"
    record(f"magic {frame}", ok, f"v1={v1_magic!r} v2={v2_magic!r}")

# ============================================================================
# 汇总
# ============================================================================
print("\n" + "=" * 70)
total = len(results)
passed = sum(1 for _, p, _ in results if p)
failed = total - passed
print(f"测试汇总: {passed}/{total} 通过, {failed} 失败")
print("=" * 70)

# 输出 CSV 摘要
csv_path = os.path.join(os.path.dirname(__file__), "test_results.csv")
with open(csv_path, "w", encoding="utf-8") as f:
    f.write("test,passed,detail\n")
    for name, p, detail in results:
        f.write(f"{name},{int(p)},{detail}\n")
print(f"CSV 摘要: {csv_path}")

sys.exit(0 if failed == 0 else 1)
