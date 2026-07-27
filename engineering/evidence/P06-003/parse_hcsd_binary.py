#!/usr/bin/env python3
"""P06-003 HCSD 字节级结构验证脚本
解析 HCSD 二进制头、leaf_index、sorted_ipix，验证格式契约。
"""
import struct
import json
import sys
import os
import zstandard as zstd

HCSD_MAGIC = b"HCSD"
N_LEAVES = 49152
LEAF_INDEX_BYTES = N_LEAVES * 24  # 1179648
LEAF_ENTRY_FMT = "<QQQ"  # leaf_ipix(u64) + data_offset(u64) + data_length(u64)
LEAF_ENTRY_SIZE = 24

def parse_hcsd(path, max_ipix_sample=200000):
    """解析 HCSD 文件，返回结构化结果。"""
    result = {
        "file": path,
        "file_exists": False,
        "file_size": 0,
        "magic": None,
        "magic_ok": False,
        "uncomp_json_len": 0,
        "comp_json_len": 0,
        "json_header": None,
        "json_parse_ok": False,
        "nside": None,
        "nested": None,
        "n_pix": None,
        "has_snr": None,
        "leaf_index": {
            "n_leaves_expected": N_LEAVES,
            "n_leaves_read": 0,
            "leaf_index_bytes_expected": LEAF_INDEX_BYTES,
            "non_empty_leaves": 0,
            "empty_leaves": 0,
            "leaf_ipix_consistent": True,
            "data_offset_zero_for_empty": True,
            "data_length_non_negative": True,
            "first_non_empty_leaf": None,
            "last_non_empty_leaf": None,
            "min_data_length": 0,
            "max_data_length": 0,
            "sum_data_length": 0,
            "non_empty_list": [],
        },
        "sorted_ipix": {
            "checked": False,
            "is_ascending": True,
            "is_leaf_sorted": True,
            "sample_size": 0,
            "first_ipix": None,
            "last_ipix": None,
        },
        "summary": {},
        "errors": [],
    }

    if not os.path.exists(path):
        result["errors"].append(f"file not found: {path}")
        return result

    result["file_exists"] = True
    result["file_size"] = os.path.getsize(path)

    with open(path, "rb") as f:
        # 1. Magic
        magic = f.read(4)
        result["magic"] = magic.decode("ascii", errors="replace")
        result["magic_ok"] = (magic == HCSD_MAGIC)
        if not result["magic_ok"]:
            result["errors"].append(f"magic mismatch: {magic} != {HCSD_MAGIC}")
            return result

        # 2. uncomp_json_len, comp_json_len
        hdr = f.read(8)
        uncomp_json_len, comp_json_len = struct.unpack("<II", hdr)
        result["uncomp_json_len"] = uncomp_json_len
        result["comp_json_len"] = comp_json_len

        # 3. 读取并解压 JSON 头
        comp_buf = f.read(comp_json_len)
        try:
            dctx = zstd.ZstdDecompressor()
            json_bytes = dctx.decompress(comp_buf, max_output_size=uncomp_json_len)
            if len(json_bytes) != uncomp_json_len:
                result["errors"].append(
                    f"json decompress size mismatch: {len(json_bytes)} != {uncomp_json_len}"
                )
            json_str = json_bytes.decode("utf-8")
            result["json_header"] = json.loads(json_str)
            result["json_parse_ok"] = True
        except Exception as e:
            result["errors"].append(f"json decompress/parse failed: {e}")
            return result

        # 提取必填字段
        jh = result["json_header"]
        result["nside"] = jh.get("nside")
        result["nested"] = jh.get("nested")
        result["n_pix"] = jh.get("n_pix")
        result["has_snr"] = jh.get("has_snr")

        nside = result["nside"]
        n_pix = result["n_pix"]

        # 计算子叶位移
        shift = 0
        temp = nside
        while temp > 64:
            shift += 2
            temp >>= 1
        result["leaf_shift"] = shift

        # 4. 读取 leaf_index[49152]
        leaf_index_start = 12 + comp_json_len
        leaf_index_data = f.read(LEAF_INDEX_BYTES)
        if len(leaf_index_data) != LEAF_INDEX_BYTES:
            result["errors"].append(
                f"leaf_index read short: {len(leaf_index_data)} != {LEAF_INDEX_BYTES}"
            )
            return result

        result["leaf_index"]["n_leaves_read"] = len(leaf_index_data) // LEAF_ENTRY_SIZE
        non_empty_list = []
        sum_data_length = 0
        for i in range(N_LEAVES):
            off = i * LEAF_ENTRY_SIZE
            leaf_ipix, data_offset, data_length = struct.unpack_from(
                LEAF_ENTRY_FMT, leaf_index_data, off
            )
            # 校验 leaf_ipix 与下标一致
            if leaf_ipix != i:
                result["leaf_index"]["leaf_ipix_consistent"] = False
            # 空子叶 data_offset 应为 0
            if data_length == 0 and data_offset != 0:
                result["leaf_index"]["data_offset_zero_for_empty"] = False
            if data_length < 0:
                result["leaf_index"]["data_length_non_negative"] = False
            if data_length > 0:
                result["leaf_index"]["non_empty_leaves"] += 1
                sum_data_length += data_length
                if result["leaf_index"]["first_non_empty_leaf"] is None:
                    result["leaf_index"]["first_non_empty_leaf"] = {
                        "index": i,
                        "leaf_ipix": leaf_ipix,
                        "data_offset": data_offset,
                        "data_length": data_length,
                    }
                result["leaf_index"]["last_non_empty_leaf"] = {
                    "index": i,
                    "leaf_ipix": leaf_ipix,
                    "data_offset": data_offset,
                    "data_length": data_length,
                }
                if data_length < result["leaf_index"]["min_data_length"] or result["leaf_index"]["min_data_length"] == 0:
                    result["leaf_index"]["min_data_length"] = data_length
                if data_length > result["leaf_index"]["max_data_length"]:
                    result["leaf_index"]["max_data_length"] = data_length
                non_empty_list.append({
                    "index": i,
                    "leaf_ipix": leaf_ipix,
                    "data_offset": data_offset,
                    "data_length": data_length,
                })
            else:
                result["leaf_index"]["empty_leaves"] += 1

        result["leaf_index"]["sum_data_length"] = sum_data_length
        result["leaf_index"]["non_empty_list"] = non_empty_list

        # 5. 验证 sum(data_length) == n_pix
        result["sum_data_length_equals_n_pix"] = (sum_data_length == n_pix)

        # 6. 读取 sorted_ipix 验证升序
        # ipix 数组起始 = 12 + comp_json_len + LEAF_INDEX_BYTES
        ipix_array_start = leaf_index_start + LEAF_INDEX_BYTES
        if n_pix > 0:
            # 对小文件全读，对大文件抽样
            sample_size = min(n_pix, max_ipix_sample)
            # 读取前 sample_size 项
            f.seek(ipix_array_start)
            sample_ipix = []
            for i in range(sample_size):
                buf = f.read(8)
                if len(buf) == 8:
                    (v,) = struct.unpack("<Q", buf)
                    sample_ipix.append(v)
            result["sorted_ipix"]["sample_size"] = sample_size
            result["sorted_ipix"]["first_ipix"] = sample_ipix[0] if sample_ipix else None
            result["sorted_ipix"]["last_ipix"] = sample_ipix[-1] if sample_ipix else None

            # 检查升序
            is_asc = True
            is_leaf_sorted = True
            prev_leaf = sample_ipix[0] >> shift if sample_ipix else 0
            for i in range(1, len(sample_ipix)):
                if sample_ipix[i] < sample_ipix[i - 1]:
                    is_asc = False
                    break
                cur_leaf = sample_ipix[i] >> shift
                if cur_leaf < prev_leaf:
                    is_leaf_sorted = False
                prev_leaf = cur_leaf
            result["sorted_ipix"]["checked"] = True
            result["sorted_ipix"]["is_ascending"] = is_asc
            result["sorted_ipix"]["is_leaf_sorted"] = is_leaf_sorted

            # 验证第一个非空子叶的 data_offset 对应第一个 ipix
            if non_empty_list:
                first_leaf = non_empty_list[0]
                expected_first_ipix_offset = first_leaf["data_offset"]
                # data_offset 是字节偏移, 第一个子叶的 data_offset 应为 0 (相对 ipix 数组起始)
                # 实际上第一个非空子叶的 data_offset 应该是 0
                result["first_leaf_data_offset_is_zero"] = (first_leaf["data_offset"] == 0)

        # 7. 验证文件大小
        expected_file_size = (
            12 + comp_json_len + LEAF_INDEX_BYTES + n_pix * 8 + n_pix * 4
        )
        result["expected_file_size"] = expected_file_size
        result["file_size_matches"] = (result["file_size"] == expected_file_size)

    # 总结
    result["summary"] = {
        "magic_ok": result["magic_ok"],
        "json_parse_ok": result["json_parse_ok"],
        "nside": result["nside"],
        "nested": result["nested"],
        "n_pix": result["n_pix"],
        "has_snr": result["has_snr"],
        "leaf_ipix_consistent": result["leaf_index"]["leaf_ipix_consistent"],
        "non_empty_leaves": result["leaf_index"]["non_empty_leaves"],
        "sum_data_length_equals_n_pix": result["sum_data_length_equals_n_pix"],
        "sorted_ipix_ascending": result["sorted_ipix"]["is_ascending"],
        "sorted_ipix_leaf_sorted": result["sorted_ipix"]["is_leaf_sorted"],
        "file_size_matches": result["file_size_matches"],
        "first_leaf_data_offset_is_zero": result.get("first_leaf_data_offset_is_zero", None),
    }
    return result


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python parse_hcsd_binary.py <file.hcsd> [max_ipix_sample]")
        sys.exit(1)
    path = sys.argv[1]
    max_sample = int(sys.argv[2]) if len(sys.argv) > 2 else 200000
    r = parse_hcsd(path, max_ipix_sample=max_sample)
    print(json.dumps(r, indent=2, default=str))
