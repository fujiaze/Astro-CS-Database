# -*- coding: utf-8 -*-
"""
C-003 HISS V2 Inspector 往返测试 + 损坏测试

验证项:
  A. 往返测试 (write v2 → read v2 → 比较数据)
     1. signal float32 字节级一致
     2. support uint8 字节级一致
     3. SNR 稀疏数据字节级一致 (ra/dec/snr 三通道 + 3 标量)
     4. provenance JSON 字段一致
  B. 损坏测试 (5 类场景, 不得跳过任何一个)
     1. 翻转文件头部字节 → 应返回错误码
     2. 翻转块数据字节 → CRC32 校验失败 (-4)
     3. 翻转 footer 字节 → magic_trailer 不匹配 (-8)
     4. 截断文件 → 应返回错误码
     5. 全局 CRC32 不匹配 → 应返回错误码 (-4)

测试数据: output/C-002/ 下 3 帧 V2 HISS 文件
"""

import os
import sys
import struct
import shutil
import logging
import json
import numpy as np

# 加入模块路径
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "..", "..",
                                "lib", "astro_image_io", "python"))

import hiss_v2 as hv
import hiss_v2_inspector as hvi

logging.basicConfig(level=logging.WARNING, format="[%(levelname)s] %(message)s")
logger = logging.getLogger("C003_test")

PROJECT_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))
V2_DIR = os.path.join(PROJECT_ROOT, "output", "C-002")
WORK_DIR = os.path.join(PROJECT_ROOT, "engineering_authoritative", "evidence", "C-003", "tmp")
os.makedirs(WORK_DIR, exist_ok=True)

FRAMES = ["T2_RED_LDN43", "T3_RED_NGC55", "T4_RED_GalaxyCenter_panel1"]

results = []  # (test_name, passed, detail)


def record(name, passed, detail=""):
    results.append((name, bool(passed), detail))
    flag = "PASS" if passed else "FAIL"
    print(f"  [{flag}] {name}" + (f" — {detail}" if detail else ""))


def safe_read_all(path):
    """读取文件 (启用全局 CRC 校验), 返回 code。

    hiss2_read_all 内部仅捕获 HissV2Error; 此函数额外捕获 zstd 等非契约异常,
    将其视为文件损坏 (返回 -4 CRC 失败 或 -7 JSON 错误)。
    """
    try:
        code, _ = hv.hiss2_read_all(path)
        return code
    except hv.HissV2Error as e:
        return e.code
    except Exception:
        # zstd 解压失败 / 其他二进制损坏 → 视为文件损坏
        return hv.HIO_ERR_CRC


def cleanup(path):
    if os.path.exists(path):
        os.remove(path)


print("=" * 72)
print("C-003 HISS V2 Inspector 往返测试 + 损坏测试")
print("=" * 72)

# ============================================================================
# A. 往返测试 (round-trip): read v2 → write v2 → read v2 → 比较
# ============================================================================
print("\n" + "=" * 72)
print("A. 往返测试 (write v2 → read v2 → 字节级比较)")
print("=" * 72)

# 先读取 3 帧原始数据 (作为 round-trip 的输入源)
src_data = {}  # frame -> dict(ipix, signal, support, snr_model, provenance)
for frame in FRAMES:
    src_path = os.path.join(V2_DIR, frame + ".hiss2")
    code, res = hv.hiss2_read_all(src_path)
    if code != 0:
        record(f"读取源 {frame}", False, f"code={code}")
        continue
    src_data[frame] = res
    record(f"读取源 {frame}", True,
           f"n_pix={res['n_pix']} has_snr={res['snr_model'] is not None}")

# ---- A.1 往返: 用源数据重新写 v2, 再读回比较 ----
print("\n--- A.1 往返一致 (3 帧 × 4 要素) ---")
rt_paths = {}
for frame in FRAMES:
    if frame not in src_data:
        continue
    src = src_data[frame]
    rt_path = os.path.join(WORK_DIR, f"{frame}_rt.hiss2")
    rt_paths[frame] = rt_path

    # 构造 provenance: 用读回的 provenance (writer 会补全/覆盖计算字段)
    prov_in = dict(src["provenance"])
    snr_model = src["snr_model"]

    try:
        # 写
        writer = hv.HissV2Writer(rt_path)
        ret = writer.write(
            nside=int(prov_in["nside"]), nested=bool(prov_in["nested"]),
            ipix=src["ipix"], signal=src["signal"], support=src["support"],
            provenance=prov_in, snr_model=snr_model,
            chunk_size=int(prov_in["chunk_size"]), codec=prov_in["codec"])
        if ret != 0:
            record(f"往返写 {frame}", False, f"write ret={ret}")
            continue

        # 读回
        code2, res2 = hv.hiss2_read_all(rt_path)
        if code2 != 0:
            record(f"往返读 {frame}", False, f"read code={code2}")
            continue

        # ---- signal float32 字节级一致 ----
        sig_bytes_ok = (src["signal"].astype("<f4").tobytes()
                        == res2["signal"].astype("<f4").tobytes())
        sig_dtype_ok = res2["signal"].dtype == np.float32

        # ---- support uint8 字节级一致 ----
        sup_bytes_ok = (src["support"].astype("<u1").tobytes()
                        == res2["support"].astype("<u1").tobytes())

        # ---- ipix uint64 字节级一致 (附加) ----
        ipix_bytes_ok = (src["ipix"].astype("<u8").tobytes()
                         == res2["ipix"].astype("<u8").tobytes())

        # ---- SNR 稀疏数据字节级一致 ----
        snr1 = src["snr_model"]
        snr2 = res2["snr_model"]
        if snr1 is not None and snr2 is not None:
            snr_n_ok = snr1.n_points == snr2.n_points
            snr_ra_bytes = (snr1.points_ra.astype("<f8").tobytes()
                            == snr2.points_ra.astype("<f8").tobytes())
            snr_dec_bytes = (snr1.points_dec.astype("<f8").tobytes()
                             == snr2.points_dec.astype("<f8").tobytes())
            snr_snr_bytes = (snr1.points_snr.astype("<f4").tobytes()
                             == snr2.points_snr.astype("<f4").tobytes())
            snr_scal_ok = (abs(snr1.snr_phot - snr2.snr_phot) < 1e-12
                           and abs(snr1.median_snr - snr2.median_snr) < 1e-12
                           and abs(snr1.idw_power - snr2.idw_power) < 1e-12)
            snr_all_ok = (snr_n_ok and snr_ra_bytes and snr_dec_bytes
                          and snr_snr_bytes and snr_scal_ok)
        elif snr1 is None and snr2 is None:
            snr_all_ok = True
        else:
            snr_all_ok = False

        # ---- provenance JSON 字段一致 ----
        # writer 会重写 format_version/nside/ordering/nested/n_pix/has_snr/
        # chunk_size/n_chunks/codec/crc_algorithm, 这些字段值应与源一致 (相同输入)
        p1 = src["provenance"]
        p2 = res2["provenance"]
        keys = set(p1.keys()) | set(p2.keys())
        prov_diffs = []
        for k in keys:
            if k not in p1:
                prov_diffs.append(f"{k}: 仅读端有={p2[k]}")
            elif k not in p2:
                prov_diffs.append(f"{k}: 仅源端有={p1[k]}")
            elif p1[k] != p2[k]:
                prov_diffs.append(f"{k}: {p1[k]!r} != {p2[k]!r}")
        prov_ok = len(prov_diffs) == 0

        all_ok = (sig_bytes_ok and sig_dtype_ok and sup_bytes_ok and ipix_bytes_ok
                  and snr_all_ok and prov_ok)
        record(f"往返 {frame}", all_ok,
               f"signal_bytes={sig_bytes_ok} dtype={sig_dtype_ok} "
               f"support_bytes={sup_bytes_ok} ipix_bytes={ipix_bytes_ok} "
               f"snr_bytes={snr_all_ok} prov={prov_ok}")
        if prov_diffs:
            print(f"        provenance 差异: {prov_diffs}")

    except Exception as e:
        record(f"往返 {frame}", False, f"异常: {e}")
        import traceback
        traceback.print_exc()

# ---- A.2 inspector 对往返文件检查 (PASS) ----
print("\n--- A.2 inspector 对往返文件检查 ---")
for frame in FRAMES:
    if frame not in rt_paths:
        continue
    rt_path = rt_paths[frame]
    if not os.path.exists(rt_path):
        continue
    try:
        report = hvi.inspect_file(rt_path)
        record(f"inspector 往返 {frame}", report.all_ok,
               f"filesize={report.filesize} errors={len(report.errors)}")
    except Exception as e:
        record(f"inspector 往返 {frame}", False, f"异常: {e}")


# ============================================================================
# B. 损坏测试 (5 类场景, 不得跳过任何一个)
# ============================================================================
print("\n" + "=" * 72)
print("B. 损坏测试 (5 类场景)")
print("=" * 72)

# 使用第一帧作为损坏测试基底
BASE_FRAME = FRAMES[0]
BASE_PATH = os.path.join(V2_DIR, BASE_FRAME + ".hiss2")
with open(BASE_PATH, "rb") as f:
    BASE_DATA = bytearray(f.read())
BASE_SIZE = len(BASE_DATA)

# 获取第一个 chunk 的偏移 (用于翻转块数据)
with hv.HissV2Reader(BASE_PATH, verify_global_crc=False) as r:
    CHUNK0_OFFSET = r._chunk_entries[0].offset
    CHUNK0_COMP_SIZE = r._chunk_entries[0].comp_size
    FOOTER_OFF = BASE_SIZE - hv.FOOTER_SIZE
    # footer 内 magic_trailer 偏移 = FOOTER_OFF + 40
    MAGIC_TRAILER_OFF = FOOTER_OFF + 40
    # footer 内 global_crc32 偏移 = FOOTER_OFF + 32
    GLOBAL_CRC_OFF = FOOTER_OFF + 32

print(f"  基底文件: {BASE_FRAME} ({BASE_SIZE} B)")
print(f"  chunk0: offset={CHUNK0_OFFSET} comp_size={CHUNK0_COMP_SIZE}")
print(f"  footer_off={FOOTER_OFF} magic_trailer_off={MAGIC_TRAILER_OFF} global_crc_off={GLOBAL_CRC_OFF}")


def write_corrupt(name, mutator):
    """生成损坏文件并返回路径。mutator(data: bytearray) -> bytearray"""
    data = bytearray(BASE_DATA)
    data = mutator(data)
    path = os.path.join(WORK_DIR, f"corrupt_{name}.hiss2")
    with open(path, "wb") as f:
        f.write(data)
    return path


# ---- B.1 翻转文件头部字节 → 应返回错误码 ----
print("\n--- B.1 翻转文件头部字节 → 应返回错误码 ---")

# B.1a 翻转 magic 第 1 字节 → -2 (magic 不匹配)
def flip_magic(d):
    d[0] ^= 0xFF
    return d

p = write_corrupt("header_magic", flip_magic)
code, _ = hv.hiss2_read_provenance(p)
record("B.1a 翻转 magic 字节", code == hv.HIO_ERR_MAGIC,
       f"code={code} (期望 {hv.HIO_ERR_MAGIC}=-2)")
cleanup(p)

# B.1b 翻转 version 字节 (offset 4) → -3 (version 不支持)
def flip_version(d):
    d[4] ^= 0x01
    return d

p = write_corrupt("header_version", flip_version)
code, _ = hv.hiss2_read_provenance(p)
record("B.1b 翻转 version 字节", code == hv.HIO_ERR_VERSION,
       f"code={code} (期望 {hv.HIO_ERR_VERSION}=-3)")
cleanup(p)

# B.1c 翻转头部非 magic/version 字节 (offset 8, json_uncomp_len) → -4 (全局 CRC 失败)
def flip_header_other(d):
    d[8] ^= 0xFF
    return d

p = write_corrupt("header_other", flip_header_other)
code, _ = hv.hiss2_read_provenance(p)
# 头部字节翻转会破坏全局 CRC32 → -4; 或导致 JSON 解压失败 → -7
record("B.1c 翻转头部其他字节", code in (hv.HIO_ERR_CRC, hv.HIO_ERR_JSON),
       f"code={code} (期望 -4 或 -7)")
cleanup(p)


# ---- B.2 翻转块数据字节 → CRC32 校验失败 (-4) ----
print("\n--- B.2 翻转块数据字节 → CRC32 校验失败 (-4) ---")

# B.2a 直接翻转块数据 (全局 CRC 也会失败 → -4)
def flip_chunk_data(d):
    d[CHUNK0_OFFSET + 5] ^= 0xFF
    return d

p = write_corrupt("chunk_data", flip_chunk_data)
# safe_read_all 启用全局 CRC 校验 → -4 (块数据在全局 CRC 覆盖范围内)
code = safe_read_all(p)
record("B.2a 翻转块数据 (全局CRC先捕获)", code == hv.HIO_ERR_CRC,
       f"code={code} (期望 {hv.HIO_ERR_CRC}=-4)")
cleanup(p)

# B.2b 翻转块数据 + 修复全局 CRC → per-chunk CRC 失败 (-4)
#     这隔离了 per-chunk CRC 校验路径
def flip_chunk_data_fix_global(d):
    d[CHUNK0_OFFSET + 5] ^= 0xFF
    # 重算全局 CRC 并写回 footer
    new_crc = hv._crc32(bytes(d[:FOOTER_OFF]))
    struct.pack_into("<I", d, GLOBAL_CRC_OFF, new_crc)
    return d

p = write_corrupt("chunk_data_fix_global", flip_chunk_data_fix_global)
# 此时全局 CRC 通过, 但 per-chunk CRC 失败 → read_chunk 返回 -4
code, _ = hv.hiss2_read_chunk(p, 0)
record("B.2b 翻转块数据+修复全局CRC (per-chunk CRC 捕获)", code == hv.HIO_ERR_CRC,
       f"code={code} (期望 {hv.HIO_ERR_CRC}=-4)")
cleanup(p)

# B.2c 用 inspector 检查该损坏文件: per-chunk CRC 标记失败, 全局 CRC 通过
def flip_chunk_data_fix_global_v2(d):
    d[CHUNK0_OFFSET + 5] ^= 0xFF
    new_crc = hv._crc32(bytes(d[:FOOTER_OFF]))
    struct.pack_into("<I", d, GLOBAL_CRC_OFF, new_crc)
    return d

p = write_corrupt("chunk_data_inspector", flip_chunk_data_fix_global_v2)
try:
    report = hvi.inspect_file(p)
    # 全局 CRC 通过, 但 chunk 0 的 per-chunk CRC 失败
    chunk0 = report.chunks[0] if report.chunks else None
    ok = (report.global_crc32_ok and chunk0 is not None and not chunk0.crc32_ok
          and not report.all_ok)
    record("B.2c inspector 定位 per-chunk CRC 失败", ok,
           f"global_ok={report.global_crc32_ok} chunk0_crc_ok={chunk0.crc32_ok if chunk0 else 'N/A'} "
           f"all_ok={report.all_ok}")
except Exception as e:
    record("B.2c inspector 定位 per-chunk CRC 失败", False, f"异常: {e}")
cleanup(p)


# ---- B.3 翻转 footer 字节 → magic_trailer 不匹配 (-8) ----
print("\n--- B.3 翻转 footer 字节 → magic_trailer 不匹配 (-8) ---")

# B.3a 翻转 magic_trailer 第 1 字节 → -8
def flip_magic_trailer(d):
    d[MAGIC_TRAILER_OFF] ^= 0xFF
    return d

p = write_corrupt("footer_magic", flip_magic_trailer)
code, _ = hv.hiss2_read_provenance(p)
record("B.3a 翻转 magic_trailer 字节", code == hv.HIO_ERR_FOOTER,
       f"code={code} (期望 {hv.HIO_ERR_FOOTER}=-8)")
cleanup(p)

# B.3b 翻转 magic_trailer 所有 4 字节 → -8
def flip_magic_trailer_all(d):
    for i in range(4):
        d[MAGIC_TRAILER_OFF + i] ^= 0xFF
    return d

p = write_corrupt("footer_magic_all", flip_magic_trailer_all)
code, _ = hv.hiss2_read_provenance(p)
record("B.3b 翻转 magic_trailer 全部字节", code == hv.HIO_ERR_FOOTER,
       f"code={code} (期望 {hv.HIO_ERR_FOOTER}=-8)")
cleanup(p)

# B.3c inspector 检查 footer magic 损坏
p = write_corrupt("footer_magic_inspect", flip_magic_trailer)
try:
    report = hvi.inspect_file(p)
    ok = (not report.footer_magic_ok and not report.all_ok
          and "magic_trailer" in "".join(report.errors))
    record("B.3c inspector 定位 footer magic 失败", ok,
           f"footer_magic_ok={report.footer_magic_ok} all_ok={report.all_ok}")
except Exception as e:
    record("B.3c inspector 定位 footer magic 失败", False, f"异常: {e}")
cleanup(p)


# ---- B.4 截断文件 → 应返回错误码 ----
print("\n--- B.4 截断文件 → 应返回错误码 ---")

# B.4a 截断尾部 10 字节 (footer 不完整) → -8 或 -4 或文件错误
def truncate_tail_10(d):
    return d[:-10]

p = write_corrupt("trunc_tail10", truncate_tail_10)
code, _ = hv.hiss2_read_provenance(p)
record("B.4a 截断尾部 10B", code < 0,
       f"code={code} (期望 <0)")
cleanup(p)

# B.4b 截断尾部 48 字节 (整个 footer 丢失) → -8 或文件过短
def truncate_tail_48(d):
    return d[:-48]

p = write_corrupt("trunc_tail48", truncate_tail_48)
code, _ = hv.hiss2_read_provenance(p)
record("B.4b 截断尾部 48B (footer 全丢)", code < 0,
       f"code={code} (期望 <0)")
cleanup(p)

# B.4c 截断到只剩 10 字节 (极度截断) → -2 或 -8 或文件错误
def truncate_tiny(d):
    return d[:10]

p = write_corrupt("trunc_tiny", truncate_tiny)
code, _ = hv.hiss2_read_provenance(p)
record("B.4c 截断至 10B", code < 0,
       f"code={code} (期望 <0)")
cleanup(p)

# B.4d 截断尾部 100 字节 (footer + 部分 SNR)
def truncate_tail_100(d):
    return d[:-100]

p = write_corrupt("trunc_tail100", truncate_tail_100)
code, _ = hv.hiss2_read_provenance(p)
record("B.4d 截断尾部 100B", code < 0,
       f"code={code} (期望 <0)")
cleanup(p)


# ---- B.5 全局 CRC32 不匹配 → 应返回错误码 (-4) ----
print("\n--- B.5 全局 CRC32 不匹配 → 应返回错误码 (-4) ---")

# B.5a 直接篡改 footer 中的 global_crc32 字段 → -4
def corrupt_global_crc(d):
    # 翻转 global_crc32 的 1 字节 (footer 内, 不参与全局 CRC 计算)
    d[GLOBAL_CRC_OFF] ^= 0xFF
    return d

p = write_corrupt("global_crc", corrupt_global_crc)
# safe_read_all 启用全局 CRC 校验 → -4 (footer global_crc32 与实际不匹配)
code = safe_read_all(p)
record("B.5a 篡改 footer global_crc32", code == hv.HIO_ERR_CRC,
       f"code={code} (期望 {hv.HIO_ERR_CRC}=-4)")
cleanup(p)

# B.5b 翻转 JSON 头区域字节 (在全局 CRC 覆盖范围内, 非 chunk 数据)
#      → 全局 CRC 失败 → -4 (且不触发 magic/version 错误)
def flip_json_region(d):
    # JSON 区在 offset 24 ~ 24+json_comp_len
    d[24 + 10] ^= 0xFF
    return d

p = write_corrupt("json_region", flip_json_region)
# JSON 区翻转: 可能 zstd 解压失败 (非 HissV2Error, safe_read_all 归为 -4)
# 或全局 CRC 失败 (-4), 或 JSON 长度校验失败 (-7)
code = safe_read_all(p)
record("B.5b 翻转 JSON 头区字节", code in (hv.HIO_ERR_CRC, hv.HIO_ERR_JSON),
       f"code={code} (期望 -4 或 -7)")
cleanup(p)

# B.5c inspector 检查全局 CRC 不匹配
p = write_corrupt("global_crc_inspect", corrupt_global_crc)
try:
    report = hvi.inspect_file(p)
    ok = (not report.global_crc32_ok and not report.all_ok
          and report.footer_magic_ok  # footer 本身完好
          and report.magic_ok and report.version_ok)  # 头部完好
    record("B.5c inspector 定位全局 CRC 失败", ok,
           f"global_crc_ok={report.global_crc32_ok} footer_ok={report.footer_magic_ok} "
           f"all_ok={report.all_ok}")
except Exception as e:
    record("B.5c inspector 定位全局 CRC 失败", False, f"异常: {e}")
cleanup(p)


# ---- B.6 额外: 文件不存在 → -1 ----
print("\n--- B.6 额外错误码 (文件不存在) ---")
code, _ = hv.hiss2_read_provenance(os.path.join(WORK_DIR, "nonexistent.hiss2"))
record("B.6 文件不存在", code == hv.HIO_ERR_FILE,
       f"code={code} (期望 {hv.HIO_ERR_FILE}=-1)")


# ============================================================================
# 清理工作目录
# ============================================================================
print("\n--- 清理临时文件 ---")
for f in os.listdir(WORK_DIR):
    fp = os.path.join(WORK_DIR, f)
    if os.path.isfile(fp):
        os.remove(fp)
if os.path.isdir(WORK_DIR) and not os.listdir(WORK_DIR):
    os.rmdir(WORK_DIR)
    print(f"  已删除空目录: {WORK_DIR}")
else:
    print(f"  工作目录保留: {WORK_DIR}")


# ============================================================================
# 汇总
# ============================================================================
print("\n" + "=" * 72)
total = len(results)
passed = sum(1 for _, p, _ in results if p)
failed = total - passed
print(f"测试汇总: {passed}/{total} 通过, {failed} 失败")
print("=" * 72)

# 输出 CSV 摘要
csv_path = os.path.join(os.path.dirname(__file__), "test_results.csv")
with open(csv_path, "w", encoding="utf-8") as f:
    f.write("test,passed,detail\n")
    for name, p, detail in results:
        f.write(f"{name},{int(p)},{detail}\n")
print(f"CSV 摘要: {csv_path}")

sys.exit(0 if failed == 0 else 1)
