#!/usr/bin/env python3
"""P06-003 验证按子叶读取 (aio_hcsd_read_leaf) 的正确性
模拟 C++ aio_hcsd_read_leaf 的逻辑, 与全量读取比较。
"""
import struct
import json
import sys
import os
import zstandard as zstd

HCSD_MAGIC = b"HCSD"
N_LEAVES = 49152
LEAF_INDEX_BYTES = N_LEAVES * 24
LEAF_ENTRY_FMT = "<QQQ"
LEAF_ENTRY_SIZE = 24


def read_full(path):
    """全量读取 HCSD: 返回 (meta, sorted_ipix, sorted_pixel)。"""
    with open(path, "rb") as f:
        magic = f.read(4)
        assert magic == HCSD_MAGIC, f"magic mismatch: {magic}"
        uncomp_json_len, comp_json_len = struct.unpack("<II", f.read(8))
        comp_buf = f.read(comp_json_len)
        dctx = zstd.ZstdDecompressor()
        json_bytes = dctx.decompress(comp_buf, max_output_size=uncomp_json_len)
        meta = json.loads(json_bytes.decode("utf-8"))
        n_pix = meta["n_pix"]
        # 跳过 leaf_index
        f.read(LEAF_INDEX_BYTES)
        # 读取 sorted_ipix
        ipix_bytes = f.read(n_pix * 8)
        sorted_ipix = list(struct.unpack(f"<{n_pix}Q", ipix_bytes))
        # 读取 sorted_pixel
        pixel_bytes = f.read(n_pix * 4)
        sorted_pixel = list(struct.unpack(f"<{n_pix}f", pixel_bytes))
    return meta, sorted_ipix, sorted_pixel


def read_leaf(path, leaf_ipix_at_nside64):
    """模拟 aio_hcsd_read_leaf: 按子叶读取, 返回 (n_pix, ipix, pixel)。"""
    with open(path, "rb") as f:
        magic = f.read(4)
        assert magic == HCSD_MAGIC
        uncomp_json_len, comp_json_len = struct.unpack("<II", f.read(8))
        comp_buf = f.read(comp_json_len)
        dctx = zstd.ZstdDecompressor()
        json_bytes = dctx.decompress(comp_buf, max_output_size=uncomp_json_len)
        meta = json.loads(json_bytes.decode("utf-8"))
        total_n_pix = meta["n_pix"]

        # 定位到 leaf_index[leaf_ipix_at_nside64]
        index_entry_pos = 12 + comp_json_len + leaf_ipix_at_nside64 * LEAF_ENTRY_SIZE
        f.seek(index_entry_pos)
        leaf_ipix, data_offset, data_length = struct.unpack(LEAF_ENTRY_FMT, f.read(LEAF_ENTRY_SIZE))

        if data_length == 0:
            return 0, [], [], leaf_ipix, data_offset, data_length

        # ipix 数组起始
        ipix_array_start = 12 + comp_json_len + LEAF_INDEX_BYTES
        pixel_array_start = ipix_array_start + total_n_pix * 8

        # 子叶 ipix 位置
        leaf_ipix_pos = ipix_array_start + data_offset
        # 子叶 pixel 位置: data_offset/8*4
        leaf_pixel_pos = pixel_array_start + (data_offset // 8) * 4

        # 读取 ipix
        f.seek(leaf_ipix_pos)
        ipix_bytes = f.read(data_length * 8)
        ipix = list(struct.unpack(f"<{data_length}Q", ipix_bytes))

        # 读取 pixel
        f.seek(leaf_pixel_pos)
        pixel_bytes = f.read(data_length * 4)
        pixel = list(struct.unpack(f"<{data_length}f", pixel_bytes))

        return data_length, ipix, pixel, leaf_ipix, data_offset, data_length


def verify_read_leaf(path):
    """验证按子叶读取与全量读取一致。"""
    result = {
        "file": path,
        "checks": [],
        "all_pass": True,
    }

    meta, full_ipix, full_pixel = read_full(path)
    n_pix = meta["n_pix"]
    nside = meta["nside"]

    # 计算子叶位移
    shift = 0
    temp = nside
    while temp > 64:
        shift += 2
        temp >>= 1

    result["nside"] = nside
    result["n_pix"] = n_pix
    result["leaf_shift"] = shift

    # 构建全量 ipix -> pixel 映射 (按 leaf_ipix 分组)
    leaf_to_pixels = {}
    for i in range(n_pix):
        leaf = full_ipix[i] >> shift
        if leaf not in leaf_to_pixels:
            leaf_to_pixels[leaf] = []
        leaf_to_pixels[leaf].append((full_ipix[i], full_pixel[i]))

    # 对每个非空子叶, 验证按子叶读取结果
    non_empty_leaves = sorted(leaf_to_pixels.keys())
    result["non_empty_leaves"] = len(non_empty_leaves)

    pass_count = 0
    fail_count = 0
    for leaf_ipix in non_empty_leaves:
        n, ipix, pixel, li, do, dl = read_leaf(path, leaf_ipix)
        expected = leaf_to_pixels[leaf_ipix]
        expected_ipix = [e[0] for e in expected]
        expected_pixel = [e[1] for e in expected]

        check = {
            "leaf_ipix": leaf_ipix,
            "read_n_pix": n,
            "expected_n_pix": len(expected_ipix),
            "n_pix_match": (n == len(expected_ipix)),
            "ipix_match": (ipix == expected_ipix),
            "pixel_match": (pixel == expected_pixel),
            "leaf_index_leaf_ipix": li,
            "leaf_index_data_offset": do,
            "leaf_index_data_length": dl,
        }
        check["pass"] = check["n_pix_match"] and check["ipix_match"] and check["pixel_match"]
        result["checks"].append(check)
        if check["pass"]:
            pass_count += 1
        else:
            fail_count += 1
            result["all_pass"] = False

    # 验证空子叶返回空
    empty_leaf = -1
    for i in range(N_LEAVES):
        if i not in leaf_to_pixels:
            empty_leaf = i
            break
    if empty_leaf >= 0:
        n, ipix, pixel, li, do, dl = read_leaf(path, empty_leaf)
        empty_check = {
            "leaf_ipix": empty_leaf,
            "read_n_pix": n,
            "expected_n_pix": 0,
            "n_pix_match": (n == 0),
            "pass": (n == 0 and len(ipix) == 0 and len(pixel) == 0),
        }
        result["checks"].append(empty_check)
        if empty_check["pass"]:
            pass_count += 1
        else:
            fail_count += 1
            result["all_pass"] = False

    result["pass_count"] = pass_count
    result["fail_count"] = fail_count
    return result


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python verify_read_leaf.py <file.hcsd>")
        sys.exit(1)
    path = sys.argv[1]
    r = verify_read_leaf(path)
    print(json.dumps(r, indent=2, default=str))
