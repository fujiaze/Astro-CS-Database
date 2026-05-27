#!/usr/bin/env python3
"""
测试gaia_client多数据库支持
验证DR3和DR3SP数据库能否正常调用
"""

import ctypes
import os
import sys

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

GAIA_DB_AUTO = 0
GAIA_DB_DR3 = 1
GAIA_DB_DR3SP = 2

def test_database(db_path, db_type, db_name):
    """测试指定数据库"""
    print(f"\n{'='*60}")
    print(f"测试数据库: {db_name}")
    print(f"路径: {db_path}")
    print(f"{'='*60}")
    
    dll_path = os.path.join(os.path.dirname(__file__), '..', 'gaia_client.dll')
    dll_path = os.path.abspath(dll_path)
    
    if not os.path.exists(dll_path):
        print(f"错误: DLL不存在: {dll_path}")
        return False
    
    try:
        gaia = ctypes.CDLL(dll_path)
    except Exception as e:
        print(f"错误: 加载DLL失败: {e}")
        return False
    
    gaia.gaia_client_create_ex.argtypes = [ctypes.c_char_p, ctypes.c_int]
    gaia.gaia_client_create_ex.restype = ctypes.c_void_p
    
    gaia.gaia_client_destroy.argtypes = [ctypes.c_void_p]
    gaia.gaia_client_destroy.restype = None
    
    gaia.gaia_client_get_db_type.argtypes = [ctypes.c_void_p]
    gaia.gaia_client_get_db_type.restype = ctypes.c_int
    
    gaia.gaia_client_get_file_count.argtypes = [ctypes.c_void_p]
    gaia.gaia_client_get_file_count.restype = ctypes.c_int
    
    gaia.gaia_client_get_total_sources.argtypes = [ctypes.c_void_p]
    gaia.gaia_client_get_total_sources.restype = ctypes.c_int
    
    gaia.gaia_client_cone_search.argtypes = [
        ctypes.c_void_p, ctypes.c_double, ctypes.c_double, ctypes.c_double,
        ctypes.c_double, ctypes.c_double,
        ctypes.POINTER(ctypes.POINTER(GaiaStar)), ctypes.POINTER(ctypes.c_int)
    ]
    gaia.gaia_client_cone_search.restype = ctypes.c_int
    
    client = gaia.gaia_client_create_ex(db_path.encode('utf-8'), db_type)
    if not client:
        print("错误: 创建客户端失败")
        return False
    
    detected_type = gaia.gaia_client_get_db_type(client)
    file_count = gaia.gaia_client_get_file_count(client)
    total_sources = gaia.gaia_client_get_total_sources(client)
    
    type_names = {0: 'AUTO', 1: 'DR3', 2: 'DR3SP'}
    print(f"检测到的数据库类型: {type_names.get(detected_type, '未知')}")
    print(f"加载文件数: {file_count}")
    print(f"总星数: {total_sources:,}")
    
    if file_count == 0:
        print("错误: 没有加载任何文件")
        gaia.gaia_client_destroy(client)
        return False
    
    print("\n测试锥形搜索 (银心附近)...")
    ra, dec, radius = 266.41683, -28.98333, 0.5
    mag_low, mag_high = -2.0, 15.0
    
    out_stars = ctypes.POINTER(GaiaStar)()
    out_count = ctypes.c_int()
    
    ret = gaia.gaia_client_cone_search(
        client, ra, dec, radius, mag_low, mag_high,
        ctypes.byref(out_stars), ctypes.byref(out_count)
    )
    
    if ret != 0:
        print(f"错误: 搜索失败, 返回值: {ret}")
        gaia.gaia_client_destroy(client)
        return False
    
    count = out_count.value
    print(f"搜索结果: {count:,} 颗星")
    
    if count > 0:
        print("\n前5颗星:")
        for i in range(min(5, count)):
            star = out_stars[i]
            print(f"  [{i+1}] RA={star.ra:.6f}, Dec={star.dec:.6f}, Mag={star.magG:.2f}")
    
    gaia.gaia_client_destroy(client)
    print(f"\n{db_name} 测试成功!")
    return True

def main():
    print("="*60)
    print("Gaia客户端多数据库测试")
    print("="*60)
    
    base_dir = r"F:\Astro dev\Astro CS Normalization Database"
    dr3_dir = os.path.join(base_dir, "GaiaDR3")
    dr3sp_dir = os.path.join(base_dir, "GaiaDR3SP")
    
    results = {}
    
    results['DR3'] = test_database(dr3_dir, GAIA_DB_DR3, "GaiaDR3 (完整版)")
    results['DR3SP'] = test_database(dr3sp_dir, GAIA_DB_DR3SP, "GaiaDR3SP (光谱版)")
    
    print("\n" + "="*60)
    print("测试结果汇总:")
    print("="*60)
    for name, success in results.items():
        status = "✓ 成功" if success else "✗ 失败"
        print(f"  {name}: {status}")
    print("="*60)
    
    if all(results.values()):
        print("\n所有测试通过!")
        return 0
    else:
        print("\n部分测试失败!")
        return 1

if __name__ == "__main__":
    sys.exit(main())
