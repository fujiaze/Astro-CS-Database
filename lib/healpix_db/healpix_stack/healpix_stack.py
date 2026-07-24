"""
healpix_stack.py - 稀疏 HEALpix 堆栈存储 Python 绑定

功能：通过 ctypes 调用 healpix_stack.dll，管理稀疏 HEALpix 堆栈数据库
用途：天文巡天数据的球面堆栈存储、sigma-clip + SNR 加权合并、增量更新

使用示例：
    from healpix_stack import StackDatabase, StackEngine
    
    # 创建数据库
    config = {
        "nsideData": 32768,
        "nsideLod": [512, 2048, 8192, 32768],
        "bands": ["L", "R", "G", "B"],
        "tileNside": 512,
        "sigmaClipLow": 3.0,
        "sigmaClipHigh": 3.0,
        "nested": True
    }
    db = StackDatabase.create("healpix_db/", config)
    
    # 全局更新
    frames = [
        [{"healpixPix": 100, "value": 1.0, "snr": 10.0, "weight": 1.0}, ...],
        ...
    ]
    db.update_global(frames)
    
    # 读取 tile
    result = db.read_tile(0)  # tile ipix=0
    # result = {"pixels": [...], "values": [...], "counts": [...]}
"""

from __future__ import annotations

import os
import json
from ctypes import (
    c_int, c_int64, c_double, c_void_p, c_char_p,
    POINTER, byref, cdll, string_at,
)
from typing import Optional, List, Dict, Any


def _find_dll() -> str:
    """查找 healpix_stack.dll: 同目录 → 上级目录 → lib/healpix_db/healpix_stack/"""
    dll_name = "healpix_stack.dll"
    base = os.path.dirname(os.path.abspath(__file__))
    candidates = [
        os.path.join(base, dll_name),                                              # 同目录
        os.path.normpath(os.path.join(base, "..", dll_name)),                      # 上级目录
        os.path.normpath(os.path.join(base, "..", "..", "..", "lib",               # lib/healpix_db/healpix_stack/
                                      "healpix_db", "healpix_stack", dll_name)),
    ]
    for p in candidates:
        if os.path.isfile(p):
            return p
    return os.path.join(base, dll_name)  # 默认同目录


def _load_dll(dll_path: str):
    """加载 healpix_stack.dll 并配置 C API 函数签名"""
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

    # healpix_io.dll 在 ../healpix_io/ 目录, 加入 DLL 搜索路径
    hio_dir = os.path.normpath(os.path.join(dll_dir, "..", "healpix_io"))
    if os.path.isdir(hio_dir):
        try:
            os.add_dll_directory(hio_dir)
        except OSError:
            pass
        hio_dll = os.path.join(hio_dir, "healpix_io.dll")
        if os.path.isfile(hio_dll):
            try:
                # 预加载 healpix_io.dll, 确保 hp_stack_hiss 能找到符号
                cdll.LoadLibrary(hio_dll)
            except OSError:
                pass

    # astro_image_io.dll 在 ../../astro_image_io/ 目录, 加入 DLL 搜索路径
    aio_dir = os.path.normpath(os.path.join(dll_dir, "..", "..", "astro_image_io"))
    if os.path.isdir(aio_dir):
        try:
            os.add_dll_directory(aio_dir)
        except OSError:
            pass
        aio_dll = os.path.join(aio_dir, "astro_image_io.dll")
        if os.path.isfile(aio_dll):
            try:
                # 预加载 astro_image_io.dll
                cdll.LoadLibrary(aio_dll)
            except OSError:
                pass

    dll = cdll.LoadLibrary(dll_path)

    # ===== 数据库管理 =====
    dll.hp_stack_db_create.argtypes = [c_char_p, c_char_p]
    dll.hp_stack_db_create.restype = c_void_p
    dll.hp_stack_db_open.argtypes = [c_char_p]
    dll.hp_stack_db_open.restype = c_void_p
    dll.hp_stack_db_close.argtypes = [c_void_p]
    dll.hp_stack_db_close.restype = None

    # ===== 堆栈更新 =====
    dll.hp_stack_update_global.argtypes = [c_void_p, c_char_p]
    dll.hp_stack_update_global.restype = c_int
    dll.hp_stack_update_range.argtypes = [c_void_p, c_char_p]
    dll.hp_stack_update_range.restype = c_int

    # ===== 读取堆栈数据 =====
    # read_tile 返回 malloc 分配的 char*, 需用 hp_stack_free_string 释放
    # 使用 c_void_p 避免 ctypes 自动复制后无法释放原内存
    dll.hp_stack_read_tile.argtypes = [c_void_p, c_int64]
    dll.hp_stack_read_tile.restype = c_void_p
    dll.hp_stack_free_string.argtypes = [c_void_p]
    dll.hp_stack_free_string.restype = None

    # ===== HEALpix 工具函数 =====
    dll.hp_radec2pix.argtypes = [c_int, c_int, c_double, c_double]
    dll.hp_radec2pix.restype = c_int64
    dll.hp_pix2radec.argtypes = [c_int, c_int, c_int64,
                                 POINTER(c_double), POINTER(c_double)]
    dll.hp_pix2radec.restype = None
    dll.hp_pixel_resolution_arcsec.argtypes = [c_int]
    dll.hp_pixel_resolution_arcsec.restype = c_double

    # ===== 内存 sigma-clip 堆叠 (.hiss → .hcsd) =====
    # hp_stack_hiss(hiss_paths: char**, n_frames: int,
    #               output_hcsd_path: char*, sigma: double, max_iter: int) -> int
    from ctypes import POINTER as _POINTER
    dll.hp_stack_hiss.argtypes = [_POINTER(c_char_p), c_int, c_char_p,
                                  c_double, c_int]
    dll.hp_stack_hiss.restype = c_int

    # ===== 梯度校正叠加 (.hiss → .hcsd, 含球面 TPS 梯度校正) =====
    # hp_stack_gradient_corrected(hiss_paths: char**, n_frames: int,
    #   gaia_data_dir: char*, output_hcsd_path: char*,
    #   sigma: double, max_iter: int,
    #   gradient_max_iter: int, gradient_lambda: double) -> int
    dll.hp_stack_gradient_corrected.argtypes = [
        _POINTER(c_char_p), c_int, c_char_p, c_char_p,
        c_double, c_int, c_int, c_double]
    dll.hp_stack_gradient_corrected.restype = c_int

    return dll


class StackDatabase:
    """稀疏 HEALpix 堆栈数据库, 封装 hp_stack C API"""

    def __init__(self, handle: int, dll):
        self._handle = handle
        self._dll = dll
        self._closed = False

    @classmethod
    def create(cls, db_path: str, config: Optional[Dict[str, Any]] = None,
               dll_path: Optional[str] = None) -> "StackDatabase":
        """创建数据库 (写入 meta.json)

        Args:
            db_path: 数据库目录路径
            config: 配置字典, 支持字段:
                nsideData (int), nsideLod (list[int]), bands (list[str]),
                tileNside (int), sigmaClipLow (float), sigmaClipHigh (float),
                nested (bool)
            dll_path: DLL 路径 (默认自动查找)
        """
        if dll_path is None:
            dll_path = _find_dll()
        dll = _load_dll(dll_path)
        config_json = json.dumps(config) if config else None
        config_bytes = config_json.encode("utf-8") if config_json else None
        handle = dll.hp_stack_db_create(db_path.encode("utf-8"), config_bytes)
        if not handle:
            raise RuntimeError(f"创建数据库失败: {db_path}")
        return cls(handle, dll)

    @classmethod
    def open(cls, db_path: str, dll_path: Optional[str] = None) -> "StackDatabase":
        """打开已有数据库

        Args:
            db_path: 数据库目录路径
            dll_path: DLL 路径 (默认自动查找)
        """
        if dll_path is None:
            dll_path = _find_dll()
        dll = _load_dll(dll_path)
        handle = dll.hp_stack_db_open(db_path.encode("utf-8"))
        if not handle:
            raise RuntimeError(f"打开数据库失败: {db_path}")
        return cls(handle, dll)

    def update_global(self, frames: List[List[Dict[str, Any]]]) -> int:
        """全局更新: 处理一组帧, 更新数据库

        Args:
            frames: 帧列表, 每个帧是 DrizzlePixel 字典列表
                每个 DrizzlePixel: {"healpixPix": int, "value": float,
                                    "snr": float, "weight": float}

        Returns:
            处理的像素数, 失败返回 -1
        """
        if self._closed or not self._handle:
            raise RuntimeError("数据库已关闭")
        frames_json = json.dumps(frames)
        ret = self._dll.hp_stack_update_global(self._handle,
                                               frames_json.encode("utf-8"))
        if ret < 0:
            raise RuntimeError("全局更新失败")
        return ret

    def update_range(self, file_range: Dict[str, List[Dict[str, Any]]]) -> int:
        """局部更新: 只更新指定文件范围

        Args:
            file_range: 文件路径到 DrizzlePixel 列表的映射
                {"frame1.ahpx": [{"healpixPix": 1, "value": 2.5,
                                  "snr": 10, "weight": 1.0}, ...]}

        Returns:
            处理的像素数, 失败返回 -1
        """
        if self._closed or not self._handle:
            raise RuntimeError("数据库已关闭")
        range_json = json.dumps(file_range)
        ret = self._dll.hp_stack_update_range(self._handle,
                                              range_json.encode("utf-8"))
        if ret < 0:
            raise RuntimeError("局部更新失败")
        return ret

    def read_tile(self, tile_ipix: int) -> Dict[str, Any]:
        """读取 tile 数据

        Args:
            tile_ipix: tile 的 HEALpix 像素号

        Returns:
            tile 数据字典, 包含:
                tileIpix, nside, tileNside, pixelCount, bandCount,
                pixels (list[int]),
                bands (list[dict]): 每个 band 含 values/variance/counts
        """
        if self._closed or not self._handle:
            raise RuntimeError("数据库已关闭")
        ptr = self._dll.hp_stack_read_tile(self._handle, tile_ipix)
        if not ptr:
            raise RuntimeError(f"读取 tile 失败: tileIpix={tile_ipix}")
        try:
            raw = string_at(ptr)
            json_str = raw.decode("utf-8", errors="replace")
            return json.loads(json_str)
        finally:
            self._dll.hp_stack_free_string(ptr)

    def close(self) -> None:
        if not self._closed and self._handle:
            self._dll.hp_stack_db_close(self._handle)
            self._handle = None
            self._closed = True

    def __del__(self):
        self.close()

    def __enter__(self):
        return self

    def __exit__(self, *args):
        self.close()


# ===== 模块级 HEALpix 工具函数 =====

def _get_dll(dll_path: Optional[str] = None):
    """加载 DLL (模块级函数共用)"""
    if dll_path is None:
        dll_path = _find_dll()
    return _load_dll(dll_path)


def healpix_radec2pix(nside: int, nested: bool, ra: float, dec: float,
                      dll_path: Optional[str] = None) -> int:
    """RA/Dec(度) → HEALpix 像素号

    Args:
        nside: HEALpix nside 参数
        nested: True=嵌套排序, False=环排序
        ra: 赤经 (度)
        dec: 赤纬 (度)
        dll_path: DLL 路径 (默认自动查找)

    Returns:
        HEALpix 像素号 (int64)
    """
    dll = _get_dll(dll_path)
    return dll.hp_radec2pix(nside, 1 if nested else 0, ra, dec)


def healpix_pix2radec(nside: int, nested: bool, ipix: int,
                      dll_path: Optional[str] = None) -> tuple:
    """HEALpix 像素号 → RA/Dec(度)

    Args:
        nside: HEALpix nside 参数
        nested: True=嵌套排序, False=环排序
        ipix: HEALpix 像素号
        dll_path: DLL 路径 (默认自动查找)

    Returns:
        (ra_deg, dec_deg) 元组
    """
    dll = _get_dll(dll_path)
    ra = c_double(0.0)
    dec = c_double(0.0)
    dll.hp_pix2radec(nside, 1 if nested else 0, ipix, byref(ra), byref(dec))
    return (ra.value, dec.value)


def healpix_pixel_resolution(nside: int,
                             dll_path: Optional[str] = None) -> float:
    """获取 HEALpix 像素分辨率 (角秒)

    Args:
        nside: HEALpix nside 参数
        dll_path: DLL 路径 (默认自动查找)

    Returns:
        像素分辨率 (角秒)
    """
    dll = _get_dll(dll_path)
    return dll.hp_pixel_resolution_arcsec(nside)


# ===== 内存 sigma-clip 堆叠 (.hiss → .hcsd) =====

def stack_hiss_files(hiss_paths: List[str],
                     output_hcsd_path: str,
                     sigma: float = 3.0,
                     max_iter: int = 5,
                     dll_path: Optional[str] = None) -> int:
    """堆叠多个 .hiss 文件到 .hcsd (内存 sigma-clip)

    流程:
        1. 逐帧读入 .hiss, 按 ipix 对齐累加 count/sum/sum_sq (float64)
        2. sigma-clip 迭代剔除离群值 (最多 max_iter 次)
        3. 输出最终 mean 值到 .hcsd (含子叶块索引, 由 hcsd_write 自动构建)

    内存占用: 3 × n_unique_pix × 8B (与帧数无关, sigma-clip 时重新读帧)

    Args:
        hiss_paths: .hiss 文件路径列表
        output_hcsd_path: 输出 .hcsd 文件路径
        sigma: sigma-clip 阈值 (默认 3.0)
        max_iter: 最大迭代次数 (默认 5)
        dll_path: healpix_stack.dll 路径 (默认自动查找)

    Returns:
        int: 0=成功, <0=失败

    Raises:
        RuntimeError: DLL 调用失败
        ValueError: hiss_paths 为空
    """
    if not hiss_paths:
        raise ValueError("hiss_paths 不能为空")
    if not output_hcsd_path:
        raise ValueError("output_hcsd_path 不能为空")

    dll = _get_dll(dll_path)

    # 准备 C 字符串数组 (const char**)
    path_bytes = [p.encode("utf-8") for p in hiss_paths]
    paths_arr = (c_char_p * len(path_bytes))(*path_bytes)

    out_bytes = output_hcsd_path.encode("utf-8")

    ret = dll.hp_stack_hiss(
        paths_arr,
        len(hiss_paths),
        out_bytes,
        float(sigma),
        int(max_iter),
    )

    if ret != 0:
        raise RuntimeError(
            f"hp_stack_hiss 失败, 返回码={ret} ("
            f"{len(hiss_paths)} 帧 → {output_hcsd_path})")
    return ret


def stack_gradient_corrected(hiss_paths: List[str],
                             output_hcsd_path: str,
                             gaia_data_dir: Optional[str] = None,
                             sigma: float = 3.0,
                             max_iter: int = 5,
                             gradient_max_iter: int = 10,
                             gradient_lambda: float = 1e-4,
                             dll_path: Optional[str] = None) -> int:
    """梯度校正叠加: .hiss → .hcsd (球面 TPS 梯度校正 + SNR² 加权 sigma-clip)

    完整流程:
        1. 阶段1: gradient_sampler 逐帧采样 (控制点 bg_median + SNR-B)
        2. 阶段2: gradient_fitter Gauss-Seidel 迭代拟合球面 TPS 梯度场 g_i(p)
        3. 阶段3: 逐帧读取 .hiss, 稀疏 SNR 模型 IDW 重建逐像素 SNR
        4. 阶段4: corrected_stacker 梯度校正 (pixel - g_i) + SNR² 加权 sigma-clip
        5. 阶段5: 写入 .hcsd (含子叶块索引)

    若采样/拟合失败, 自动回退到 hp_stack_hiss (无梯度校正)。

    Args:
        hiss_paths: .hiss 文件路径列表
        output_hcsd_path: 输出 .hcsd 文件路径
        gaia_data_dir: Gaia 数据目录 (用于星拒绝, None=跳过星拒绝)
        sigma: sigma-clip 阈值 (默认 3.0)
        max_iter: sigma-clip 最大迭代 (默认 5)
        gradient_max_iter: Gauss-Seidel 最大迭代 (默认 10)
        gradient_lambda: TPS 正则化参数 (默认 1e-4)
        dll_path: healpix_stack.dll 路径 (默认自动查找)

    Returns:
        int: 0=成功, <0=失败

    Raises:
        RuntimeError: DLL 调用失败
        ValueError: hiss_paths 为空
    """
    if not hiss_paths:
        raise ValueError("hiss_paths 不能为空")
    if not output_hcsd_path:
        raise ValueError("output_hcsd_path 不能为空")

    dll = _get_dll(dll_path)

    # 准备 C 字符串数组 (const char**)
    path_bytes = [p.encode("utf-8") for p in hiss_paths]
    paths_arr = (c_char_p * len(path_bytes))(*path_bytes)

    out_bytes = output_hcsd_path.encode("utf-8")
    gaia_bytes = gaia_data_dir.encode("utf-8") if gaia_data_dir else None

    ret = dll.hp_stack_gradient_corrected(
        paths_arr,
        len(hiss_paths),
        gaia_bytes,
        out_bytes,
        float(sigma),
        int(max_iter),
        int(gradient_max_iter),
        float(gradient_lambda),
    )

    if ret != 0:
        raise RuntimeError(
            f"hp_stack_gradient_corrected 失败, 返回码={ret} ("
            f"{len(hiss_paths)} 帧 → {output_hcsd_path})")
    return ret
