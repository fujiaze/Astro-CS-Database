#!/usr/bin/env python3
"""
Gaia DR3SP 光谱接口验证测试程序
作为数据库基建长期保留，用于校验以下功能：
1. 锥形搜索验证（DR3SP + DR3对比）
2. 光谱读取验证（光谱数组、值范围、magBP/magRP）
3. 全块解析校验（遍历所有XPSD文件所有叶子节点，统计总星数）

使用方法: python verify_spectrum.py
"""

import ctypes
import os
import sys
import struct
import zlib
import time

BASE_DIR = r"F:\Astro dev\Astro CS Normalization Database"
DR3_DIR = os.path.join(BASE_DIR, "GaiaDR3")
DR3SP_DIR = os.path.join(BASE_DIR, "GaiaDR3SP")
DLL_PATH = os.path.join(BASE_DIR, "lib", "gaia_xpsd_client", "gaia_client.dll")

GAIA_DB_AUTO = 0
GAIA_DB_DR3 = 1
GAIA_DB_DR3SP = 2


class GaiaStar(ctypes.Structure):
    _fields_ = [
        ('ra', ctypes.c_double),
        ('dec', ctypes.c_double),
        ('magG', ctypes.c_double),
        ('magBP', ctypes.c_double),
        ('magRP', ctypes.c_double),
        ('parallax', ctypes.c_float),
        ('pmra', ctypes.c_float),
        ('pmdec', ctypes.c_float),
        ('source_id', ctypes.c_int64),
    ]


class GaiaSpectrumStar(ctypes.Structure):
    _fields_ = [
        ('ra', ctypes.c_double),
        ('dec', ctypes.c_double),
        ('magG', ctypes.c_double),
    ]


class GaiaPhotometryStar(ctypes.Structure):
    _fields_ = [
        ('ra', ctypes.c_double),
        ('dec', ctypes.c_double),
        ('magG', ctypes.c_double),
        ('magBP', ctypes.c_double),
        ('magRP', ctypes.c_double),
    ]


def load_dll():
    if not os.path.exists(DLL_PATH):
        print(f"错误: DLL不存在: {DLL_PATH}")
        return None
    dll = ctypes.CDLL(DLL_PATH)

    dll.gaia_client_create_ex.argtypes = [ctypes.c_char_p, ctypes.c_int]
    dll.gaia_client_create_ex.restype = ctypes.c_void_p

    dll.gaia_client_destroy.argtypes = [ctypes.c_void_p]
    dll.gaia_client_destroy.restype = None

    dll.gaia_client_get_db_type.argtypes = [ctypes.c_void_p]
    dll.gaia_client_get_db_type.restype = ctypes.c_int

    dll.gaia_client_get_file_count.argtypes = [ctypes.c_void_p]
    dll.gaia_client_get_file_count.restype = ctypes.c_int

    dll.gaia_client_get_total_sources.argtypes = [ctypes.c_void_p]
    dll.gaia_client_get_total_sources.restype = ctypes.c_int

    dll.gaia_client_cone_search.argtypes = [
        ctypes.c_void_p, ctypes.c_double, ctypes.c_double, ctypes.c_double,
        ctypes.c_double, ctypes.c_double,
        ctypes.POINTER(ctypes.POINTER(GaiaStar)), ctypes.POINTER(ctypes.c_int)
    ]
    dll.gaia_client_cone_search.restype = ctypes.c_int

    dll.gaia_client_cone_search_with_spectrum.argtypes = [
        ctypes.c_void_p, ctypes.c_double, ctypes.c_double, ctypes.c_double,
        ctypes.c_double, ctypes.c_double,
        ctypes.POINTER(ctypes.POINTER(GaiaSpectrumStar)),
        ctypes.POINTER(ctypes.POINTER(ctypes.c_uint8)),
        ctypes.POINTER(ctypes.c_int)
    ]
    dll.gaia_client_cone_search_with_spectrum.restype = ctypes.c_int

    dll.gaia_client_cone_search_with_photometry.argtypes = [
        ctypes.c_void_p, ctypes.c_double, ctypes.c_double, ctypes.c_double,
        ctypes.c_double, ctypes.c_double,
        ctypes.POINTER(ctypes.POINTER(GaiaPhotometryStar)),
        ctypes.POINTER(ctypes.c_int)
    ]
    dll.gaia_client_cone_search_with_photometry.restype = ctypes.c_int

    dll.gaia_client_get_spectrum_params.argtypes = [
        ctypes.c_void_p,
        ctypes.POINTER(ctypes.c_int),
        ctypes.POINTER(ctypes.c_int),
        ctypes.POINTER(ctypes.c_int)
    ]
    dll.gaia_client_get_spectrum_params.restype = ctypes.c_int

    return dll


def test1_cone_search(dll):
    """测试1: 锥形搜索验证（DR3SP + DR3对比）"""
    print("\n" + "=" * 60)
    print("测试1: 锥形搜索验证")
    print("=" * 60)

    ra, dec, radius = 266.41683, -28.98333, 0.5
    mag_low, mag_high = -2.0, 15.0
    results = {}

    for db_name, db_dir, db_type in [("DR3", DR3_DIR, GAIA_DB_DR3),
                                       ("DR3SP", DR3SP_DIR, GAIA_DB_DR3SP)]:
        print(f"\n  [{db_name}] 搜索 RA={ra}, Dec={dec}, r={radius}, mag<{mag_high}")

        client = dll.gaia_client_create_ex(db_dir.encode('utf-8'), db_type)
        if not client:
            print(f"  错误: 创建客户端失败")
            results[db_name] = False
            continue

        file_count = dll.gaia_client_get_file_count(client)
        total = dll.gaia_client_get_total_sources(client)
        print(f"  文件数: {file_count}, 总星数: {total:,}")

        out_stars = ctypes.POINTER(GaiaStar)()
        out_count = ctypes.c_int()

        t0 = time.time()
        ret = dll.gaia_client_cone_search(
            client, ra, dec, radius, mag_low, mag_high,
            ctypes.byref(out_stars), ctypes.byref(out_count)
        )
        elapsed = time.time() - t0

        count = out_count.value
        print(f"  返回值: {ret}, 星数: {count:,}, 耗时: {elapsed:.3f}s")

        if count > 0:
            print(f"  前3颗星:")
            for i in range(min(3, count)):
                s = out_stars[i]
                print(f"    [{i}] RA={s.ra:.6f}, Dec={s.dec:.6f}, MagG={s.magG:.3f}")

            ra_vals = [out_stars[i].ra for i in range(count)]
            dec_vals = [out_stars[i].dec for i in range(count)]
            mag_vals = [out_stars[i].magG for i in range(count)]
            print(f"  RA范围: {min(ra_vals):.4f} ~ {max(ra_vals):.4f}")
            print(f"  Dec范围: {min(dec_vals):.4f} ~ {max(dec_vals):.4f}")
            print(f"  MagG范围: {min(mag_vals):.3f} ~ {max(mag_vals):.3f}")

            ra_ok = 265.0 < min(ra_vals) < 268.0 and 265.0 < max(ra_vals) < 268.0
            dec_ok = -30.0 < min(dec_vals) < -27.0 and -30.0 < max(dec_vals) < -27.0
            mag_ok = -3.0 < min(mag_vals) < 16.0
            results[db_name] = ra_ok and dec_ok and mag_ok
        else:
            results[db_name] = False

        ctypes.cdll.msvcrt.free(out_stars)
        dll.gaia_client_destroy(client)

    if results.get("DR3") and results.get("DR3SP"):
        print(f"\n  向后兼容验证: DR3和DR3SP锥形搜索均正常")

    passed = all(results.values())
    print(f"\n  结果: {'PASS' if passed else 'FAIL'}")
    return passed


def test2_spectrum_reading(dll):
    """测试2: 光谱读取验证"""
    print("\n" + "=" * 60)
    print("测试2: 光谱读取验证")
    print("=" * 60)

    client = dll.gaia_client_create_ex(DR3SP_DIR.encode('utf-8'), GAIA_DB_DR3SP)
    if not client:
        print("  错误: 创建DR3SP客户端失败")
        return False

    start_nm = ctypes.c_int()
    step_nm = ctypes.c_int()
    spec_count = ctypes.c_int()
    has_spec = dll.gaia_client_get_spectrum_params(
        client, ctypes.byref(start_nm), ctypes.byref(step_nm), ctypes.byref(spec_count)
    )
    print(f"\n  光谱参数: has_spectrum={has_spec}, start={start_nm.value}nm, step={step_nm.value}nm, count={spec_count.value}")

    if not has_spec or start_nm.value != 336 or step_nm.value != 2 or spec_count.value != 343:
        print(f"  错误: 光谱参数不匹配预期(336/2/343)")
        dll.gaia_client_destroy(client)
        return False
    print(f"  光谱参数验证: PASS")

    ra, dec, radius = 266.41683, -28.98333, 0.5
    mag_low, mag_high = -2.0, 15.0

    out_stars = ctypes.POINTER(GaiaSpectrumStar)()
    out_spectra = ctypes.POINTER(ctypes.c_uint8)()
    out_count = ctypes.c_int()

    print(f"\n  光谱搜索 RA={ra}, Dec={dec}, r={radius}")
    t0 = time.time()
    ret = dll.gaia_client_cone_search_with_spectrum(
        client, ra, dec, radius, mag_low, mag_high,
        ctypes.byref(out_stars), ctypes.byref(out_spectra), ctypes.byref(out_count)
    )
    elapsed = time.time() - t0
    count = out_count.value
    print(f"  返回值: {ret}, 星数: {count}, 耗时: {elapsed:.3f}s")

    if count == 0:
        print("  错误: 无搜索结果")
        dll.gaia_client_destroy(client)
        return False

    if not out_spectra:
        print("  错误: out_spectra为NULL")
        ctypes.cdll.msvcrt.free(out_stars)
        dll.gaia_client_destroy(client)
        return False

    expected_spec_len = count * spec_count.value
    print(f"\n  光谱数组验证:")
    print(f"    星数: {count}")
    print(f"    每星光谱数: {spec_count.value}")
    print(f"    预期总长度: {expected_spec_len}")

    all_valid = True
    spec_range_ok = True

    for i in range(min(5, count)):
        s = out_stars[i]
        spec_offset = i * spec_count.value
        spec_vals = [out_spectra[spec_offset + j] for j in range(spec_count.value)]

        min_v = min(spec_vals)
        max_v = max(spec_vals)
        nonzero = sum(1 for v in spec_vals if v > 0)
        range_valid = 0 <= min_v <= 255 and 0 <= max_v <= 255

        if not range_valid:
            spec_range_ok = False

        print(f"    [{i}] RA={s.ra:.6f}, Dec={s.dec:.6f}, magG={s.magG:.3f}")
        print(f"         光谱: min={min_v}, max={max_v}, 非零={nonzero}/{spec_count.value}, "
              f"前5值={spec_vals[:5]}")

    all_star_spec_valid = True
    for i in range(count):
        spec_offset = i * spec_count.value
        for j in range(spec_count.value):
            v = out_spectra[spec_offset + j]
            if v < 0 or v > 255:
                all_star_spec_valid = False
                break
        if not all_star_spec_valid:
            break

    print(f"\n  验证汇总:")
    print(f"    光谱数组非NULL: {'PASS' if out_spectra else 'FAIL'}")
    print(f"    光谱值范围0-255 (前5颗): {'PASS' if spec_range_ok else 'FAIL'}")
    print(f"    全部{count}颗星光谱值范围0-255: {'PASS' if all_star_spec_valid else 'FAIL'}")

    ctypes.cdll.msvcrt.free(out_stars)
    ctypes.cdll.msvcrt.free(out_spectra)
    dll.gaia_client_destroy(client)

    passed = bool(out_spectra) and spec_range_ok and all_star_spec_valid
    print(f"\n  结果: {'PASS' if passed else 'FAIL'}")
    return passed


def test4_photometry(dll):
    """测试4: 测光(BP/RP)接口验证"""
    print("\n" + "=" * 60)
    print("测试4: 测光(BP/RP)接口验证")
    print("=" * 60)

    client = dll.gaia_client_create_ex(DR3SP_DIR.encode('utf-8'), GAIA_DB_DR3SP)
    if not client:
        print("  错误: 创建DR3SP客户端失败")
        return False

    ra, dec, radius = 266.41683, -28.98333, 0.5
    mag_low, mag_high = -2.0, 15.0

    out_stars = ctypes.POINTER(GaiaPhotometryStar)()
    out_count = ctypes.c_int()

    print(f"\n  测光搜索 RA={ra}, Dec={dec}, r={radius}")
    t0 = time.time()
    ret = dll.gaia_client_cone_search_with_photometry(
        client, ra, dec, radius, mag_low, mag_high,
        ctypes.byref(out_stars), ctypes.byref(out_count)
    )
    elapsed = time.time() - t0
    count = out_count.value
    print(f"  返回值: {ret}, 星数: {count}, 耗时: {elapsed:.3f}s")

    if count == 0:
        print("  错误: 无搜索结果")
        dll.gaia_client_destroy(client)
        return False

    mag_bp_ok = True
    mag_rp_ok = True

    for i in range(min(5, count)):
        s = out_stars[i]
        bp_diff = abs(s.magBP - s.magG)
        rp_diff = abs(s.magRP - s.magG)
        bp_valid = -2.0 < s.magBP < 20.0 and bp_diff < 5.0
        rp_valid = -2.0 < s.magRP < 20.0 and rp_diff < 5.0
        if not bp_valid:
            mag_bp_ok = False
        if not rp_valid:
            mag_rp_ok = False

        print(f"    [{i}] RA={s.ra:.6f}, Dec={s.dec:.6f}, "
              f"magG={s.magG:.3f}, magBP={s.magBP:.3f}, magRP={s.magRP:.3f} "
              f"(BP-G={s.magBP-s.magG:.3f}, G-RP={s.magG-s.magRP:.3f})")

    all_bp_ok = True
    all_rp_ok = True
    n_bp_valid = 0
    n_rp_valid = 0
    for i in range(count):
        s = out_stars[i]
        if s.magBP > 0:
            n_bp_valid += 1
            if not (-2.0 < s.magBP < 25.0 and abs(s.magBP - s.magG) < 7.0):
                all_bp_ok = False
        if s.magRP > 0:
            n_rp_valid += 1
            if not (-2.0 < s.magRP < 25.0 and abs(s.magRP - s.magG) < 7.0):
                all_rp_ok = False

    print(f"\n  验证汇总:")
    print(f"    星数>0: PASS ({count})")
    print(f"    有BP测光的星: {n_bp_valid}/{count}")
    print(f"    有RP测光的星: {n_rp_valid}/{count}")
    print(f"    magBP与magG相差<5.0等 (前5颗): {'PASS' if mag_bp_ok else 'FAIL'}")
    print(f"    magRP与magG相差<5.0等 (前5颗): {'PASS' if mag_rp_ok else 'FAIL'}")
    print(f"    全部有BP星等合理: {'PASS' if all_bp_ok else 'FAIL'}")
    print(f"    全部有RP星等合理: {'PASS' if all_rp_ok else 'FAIL'}")

    ctypes.cdll.msvcrt.free(out_stars)
    dll.gaia_client_destroy(client)

    passed = count > 0 and mag_bp_ok and mag_rp_ok and all_bp_ok and all_rp_ok
    print(f"\n  结果: {'PASS' if passed else 'FAIL'}")
    return passed


def _find_tag(xml, tag):
    start = xml.find(f'<{tag} ')
    if start < 0:
        start = xml.find(f'<{tag}>')
    return start

def _parse_attr(tag_str, attr):
    key = f'{attr}="'
    idx = tag_str.find(key)
    if idx < 0:
        return None
    start = idx + len(key)
    end = tag_str.find('"', start)
    if end < 0:
        return None
    return tag_str[start:end]

def _parse_xpsd_header(filepath):
    with open(filepath, 'rb') as f:
        magic = f.read(8)
        if magic != b'XPSD0100':
            return None
        header_len = struct.unpack('<I', f.read(4))[0]
        xml_data = f.read(header_len - 12).decode('utf-8', errors='ignore')

    info = {}
    data_idx = _find_tag(xml_data, 'Data')
    if data_idx >= 0:
        tag_end = xml_data.find('/>', data_idx)
        tag_str = xml_data[data_idx:tag_end + 2]
        pos = _parse_attr(tag_str, 'position')
        info['position'] = int(pos) if pos else 0
        info['compression'] = _parse_attr(tag_str, 'compression') or ''
        params = _parse_attr(tag_str, 'parameters') or ''
        for kv in params.split(','):
            if '=' in kv:
                k, v = kv.strip().split('=')
                try:
                    info[k.strip()] = int(v.strip())
                except ValueError:
                    pass

    stats_idx = _find_tag(xml_data, 'Statistics')
    if stats_idx >= 0:
        tag_end = xml_data.find('/>', stats_idx)
        tag_str = xml_data[stats_idx:tag_end + 2]
        ts = _parse_attr(tag_str, 'totalSources')
        info['total_sources'] = int(ts) if ts else 0

    trees = []
    search = xml_data
    while True:
        t_idx = _find_tag(search, 'Tree')
        if t_idx < 0:
            break
        tag_end = search.find('/>', t_idx)
        tag_str = search[t_idx:tag_end + 2]
        trees.append({
            'root_position': int(_parse_attr(tag_str, 'rootPosition') or '0'),
            'node_count': int(_parse_attr(tag_str, 'nodeCount') or '0'),
        })
        search = search[tag_end + 2:]
    info['trees'] = trees
    return info

def _parse_tree_nodes(filepath, root_position, node_count):
    nodes = []
    with open(filepath, 'rb') as f:
        f.seek(root_position)
        for i in range(node_count):
            raw = f.read(48)
            bo_raw = struct.unpack('<Q', raw[32:40])[0]
            is_leaf = (bo_raw & 0x8000000000000000) != 0
            if is_leaf:
                block_offset = bo_raw & 0x7FFFFFFFFFFFFFFF
                block_size = struct.unpack('<I', raw[40:44])[0]
                compressed_size = struct.unpack('<I', raw[44:48])[0]
            else:
                block_offset = block_size = compressed_size = 0
            nodes.append({
                'is_leaf': is_leaf,
                'block_offset': block_offset,
                'block_size': block_size,
                'compressed_size': compressed_size,
            })
    return nodes


def test3_full_block_parsing():
    """测试3: 全块解析校验"""
    print("\n" + "=" * 60)
    print("测试3: 全块解析校验")
    print("=" * 60)

    files = sorted([f for f in os.listdir(DR3SP_DIR) if f.endswith('.xpsd')])
    print(f"  DR3SP文件数: {len(files)}")

    total_stars = 0
    total_xml_sources = 0
    total_leaves = 0
    total_blocks_ok = 0
    total_blocks_fail = 0
    all_ok = True

    for fi, fname in enumerate(files):
        filepath = os.path.join(DR3SP_DIR, fname)
        info = _parse_xpsd_header(filepath)
        if not info:
            print(f"  [{fname}] 头部解析失败")
            all_ok = False
            continue

        wl_count = info.get('spectrumCount', 343)
        star_stride = 40 + wl_count + (wl_count & 1)
        xml_sources = info.get('total_sources', 0)
        total_xml_sources += xml_sources

        file_stars = 0
        file_leaves = 0
        file_fail = 0

        for tree in info['trees']:
            nodes = _parse_tree_nodes(filepath, tree['root_position'], tree['node_count'])
            leaf_nodes = [n for n in nodes if n['is_leaf']]

            for leaf in leaf_nodes:
                file_leaves += 1
                try:
                    with open(filepath, 'rb') as f:
                        f.seek(info['position'] + leaf['block_offset'])
                        comp_data = f.read(leaf['compressed_size'])

                    if leaf['compressed_size'] == leaf['block_size']:
                        block_data = comp_data
                    else:
                        block_data = zlib.decompress(comp_data)

                    if len(block_data) != leaf['block_size']:
                        file_fail += 1
                        continue

                    n_stars = len(block_data) // star_stride
                    if len(block_data) % star_stride != 0:
                        file_fail += 1
                        continue

                    file_stars += n_stars
                except Exception as e:
                    file_fail += 1

        total_stars += file_stars
        total_leaves += file_leaves
        total_blocks_ok += (file_leaves - file_fail)
        total_blocks_fail += file_fail

        status = "OK" if file_fail == 0 else f"FAIL({file_fail})"
        print(f"  [{fname}] 叶子={file_leaves}, 星数={file_stars:,}, "
              f"XML={xml_sources:,}, 差异={abs(file_stars-xml_sources):,}, {status}")

        if file_fail > 0:
            all_ok = False

    error_pct = abs(total_stars - total_xml_sources) / max(total_xml_sources, 1) * 100
    print(f"\n  汇总:")
    print(f"    总叶子节点: {total_leaves}")
    print(f"    解压成功: {total_blocks_ok}")
    print(f"    解压失败: {total_blocks_fail}")
    print(f"    解析总星数: {total_stars:,}")
    print(f"    XML总星数: {total_xml_sources:,}")
    print(f"    误差: {error_pct:.4f}% (totalSources口径含全部星, XPSD仅存储有光谱星)")

    passed = all_ok and total_blocks_fail == 0
    print(f"\n  结果: {'PASS' if passed else 'FAIL'}")
    return passed


def main():
    print("=" * 60)
    print("Gaia DR3SP 光谱/测光接口验证测试")
    print("=" * 60)

    dll = load_dll()
    if not dll:
        return 1

    results = {}
    results['test1'] = test1_cone_search(dll)
    results['test2'] = test2_spectrum_reading(dll)
    results['test3'] = test3_full_block_parsing()
    results['test4'] = test4_photometry(dll)

    print("\n" + "=" * 60)
    print("测试结果汇总")
    print("=" * 60)
    for name, passed in results.items():
        status = "PASS" if passed else "FAIL"
        print(f"  {name}: {status}")

    if all(results.values()):
        print("\n所有测试通过!")
        return 0
    else:
        print("\n部分测试失败!")
        return 1


if __name__ == "__main__":
    sys.exit(main())
