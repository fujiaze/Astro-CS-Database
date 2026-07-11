"""
healpix_lod.py - LOD 金字塔 Python 绑定

功能：通过 ctypes 调用 healpix_lod.dll，管理多级 LOD 金字塔
用途：球面数据的降采样预渲染、增量更新、按需计算

使用示例：
    from healpix_lod import LodManager
    
    # 生成完整 LOD 金字塔
    LodManager.generate_full("healpix_db/", band_index=0)
    
    # 增量更新
    LodManager.update_incremental("healpix_db/", band_index=0, 
                                   changed_tiles=[100, 101, 102])
    
    # 按需计算
    tile_data = LodManager.compute_on_demand("healpix_db/", band_index=0,
                                              level=2, tile_ipix=100)
    # tile_data = {"nside": ..., "pixels": [...], "values": [...], ...}
"""

from __future__ import annotations

import os
import json
from ctypes import (
    c_int, c_int64, c_void_p, c_char_p,
    POINTER, byref, cdll, string_at,
)
from typing import Optional, List, Dict, Any


def _find_dll() -> str:
    """查找 healpix_lod.dll: 同目录 → 上级目录 → lib/healpix_db/healpix_lod/"""
    dll_name = "healpix_lod.dll"
    base = os.path.dirname(os.path.abspath(__file__))
    candidates = [
        os.path.join(base, dll_name),                                              # 同目录
        os.path.normpath(os.path.join(base, "..", dll_name)),                      # 上级目录
        os.path.normpath(os.path.join(base, "..", "..", "..", "lib",               # lib/healpix_db/healpix_lod/
                                      "healpix_db", "healpix_lod", dll_name)),
    ]
    for p in candidates:
        if os.path.isfile(p):
            return p
    return os.path.join(base, dll_name)  # 默认同目录


def _load_dll(dll_path: str):
    """加载 healpix_lod.dll 并配置 C API 函数签名"""
    mingw_bin = r"C:\msys64\mingw64\bin"
    if os.path.isdir(mingw_bin):
        # 避免重复追加 PATH 导致环境变量超长 (Windows 32767 字符限制)
        current_path = os.environ.get("PATH", "")
        if mingw_bin not in current_path:
            os.environ["PATH"] = mingw_bin + ";" + current_path
        try:
            os.add_dll_directory(mingw_bin)
        except OSError:
            pass
    dll_dir = os.path.dirname(os.path.abspath(dll_path))
    try:
        os.add_dll_directory(dll_dir)
    except OSError:
        pass
    dll = cdll.LoadLibrary(dll_path)

    # ===== LOD 金字塔管理 =====
    dll.hp_lod_generate_full.argtypes = [c_char_p, c_int]
    dll.hp_lod_generate_full.restype = c_int

    dll.hp_lod_update_incremental.argtypes = [c_char_p, c_int, c_char_p]
    dll.hp_lod_update_incremental.restype = c_int

    # compute_on_demand 返回 malloc 分配的 char*, 需用 hp_lod_free_string 释放
    # 使用 c_void_p 避免 ctypes 自动复制后无法释放原内存
    dll.hp_lod_compute_on_demand.argtypes = [c_char_p, c_int, c_int, c_int64]
    dll.hp_lod_compute_on_demand.restype = c_void_p

    dll.hp_lod_free_string.argtypes = [c_void_p]
    dll.hp_lod_free_string.restype = None

    dll.hp_lod_get_level_count.argtypes = [c_char_p]
    dll.hp_lod_get_level_count.restype = c_int

    return dll


def _get_dll(dll_path: Optional[str] = None):
    """加载 DLL (模块级函数共用)"""
    if dll_path is None:
        dll_path = _find_dll()
    return _load_dll(dll_path)


class LodManager:
    """LOD 金字塔管理器, 封装 hp_lod C API"""

    @staticmethod
    def generate_full(db_path: str, band_index: int,
                      dll_path: Optional[str] = None) -> bool:
        """生成完整 LOD 金字塔

        从堆栈数据库的数据层逐级降采样, 生成所有 LOD 层 tile 文件 (.ahpl)。

        Args:
            db_path: 堆栈数据库路径 (包含 meta.json 和 tiles/ 目录)
            band_index: 波段索引 (0..N-1)
            dll_path: DLL 路径 (默认自动查找)

        Returns:
            True=成功

        Raises:
            RuntimeError: 生成失败
        """
        dll = _get_dll(dll_path)
        ret = dll.hp_lod_generate_full(db_path.encode("utf-8"), band_index)
        if ret != 0:
            raise RuntimeError(
                f"generate_full 失败: dbPath={db_path} band={band_index}")
        return True

    @staticmethod
    def update_incremental(db_path: str, band_index: int,
                           changed_tiles: List[int],
                           dll_path: Optional[str] = None) -> bool:
        """增量更新: 数据层某区域变化后重算受影响 tile 的 LOD

        Args:
            db_path: 数据库路径
            band_index: 波段索引
            changed_tiles: 变化的 tile ipix 列表 (tileNside 级别)
            dll_path: DLL 路径 (默认自动查找)

        Returns:
            True=成功

        Raises:
            RuntimeError: 更新失败
        """
        dll = _get_dll(dll_path)
        tiles_json = json.dumps([int(t) for t in changed_tiles])
        ret = dll.hp_lod_update_incremental(
            db_path.encode("utf-8"), band_index, tiles_json.encode("utf-8"))
        if ret != 0:
            raise RuntimeError(
                f"update_incremental 失败: dbPath={db_path} band={band_index}")
        return True

    @staticmethod
    def compute_on_demand(db_path: str, band_index: int,
                          level: int, tile_ipix: int,
                          dll_path: Optional[str] = None) -> Dict[str, Any]:
        """按需计算: 请求某层某 tile, 若 LOD tile 已存在则直接读取, 否则实时降采样

        Args:
            db_path: 数据库路径
            band_index: 波段索引
            level: LOD 层级 (0=最粗 .. N-1=数据层)
            tile_ipix: tile 的 HEALpix 像素号 (tileNside 级别)

        Returns:
            tile 数据字典, 包含:
                nside (int): 该层 nside
                tileIpix (int): tile 像素号
                pixelCount (int): 像素数
                pixels (list[int]): 像素号数组
                values (list[float]): 值数组 (加权均值)
                weights (list[float]): 权重数组 (权重和)
                counts (list[int]): 计数数组 (贡献的子像素数)

        Raises:
            RuntimeError: 计算失败
        """
        dll = _get_dll(dll_path)
        ptr = dll.hp_lod_compute_on_demand(
            db_path.encode("utf-8"), band_index, level, int(tile_ipix))
        if not ptr:
            raise RuntimeError(
                f"compute_on_demand 失败: dbPath={db_path} band={band_index} "
                f"level={level} tile={tile_ipix}")
        try:
            raw = string_at(ptr)
            json_str = raw.decode("utf-8", errors="replace")
            return json.loads(json_str)
        finally:
            dll.hp_lod_free_string(ptr)

    @staticmethod
    def get_level_count(db_path: str,
                        dll_path: Optional[str] = None) -> int:
        """获取 LOD 层级数

        读取 meta.json 中的 nsideData, 返回默认 4 级 LOD 的层级数。

        Args:
            db_path: 数据库路径
            dll_path: DLL 路径 (默认自动查找)

        Returns:
            层级数 (通常为 4)

        Raises:
            RuntimeError: 读取失败
        """
        dll = _get_dll(dll_path)
        ret = dll.hp_lod_get_level_count(db_path.encode("utf-8"))
        if ret < 0:
            raise RuntimeError(f"get_level_count 失败: dbPath={db_path}")
        return ret
