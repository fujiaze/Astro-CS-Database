# -*- coding: utf-8 -*-
"""
IPV Plate Solver Python 绑定
============================
功能: 封装 ipv_solver.dll，提供 Python 接口进行天文图像 plate solving
用途: 通过统一求解 (三角形匹配 + 多项式 TRANS + 迭代重投影) 匹配图像星点与 Gaia 星表，求解 WCS+SIP 坐标变换
方法: Valdes 1995 三角形匹配 + iter_trans 多项式拟合 + 固定索引迭代重投影
调用: from ipv_solver import IPVSolver
      solver = IPVSolver(dll_path)
      solver.set_gaia_handle(handle)
      result = solver.solve(image_path, ra, dec, focal_length, pixel_size)
依赖: ipv_solver.dll (由 cpp/ipv/Makefile 编译生成)
作者: IPV Phase I MVP
日期: 2026-07-02
"""

import os
import ctypes
from ctypes import (
    c_int, c_float, c_double, c_char, c_char_p, c_void_p, c_ssize_t,
    Structure, POINTER, byref
)

# c_intptr 在部分 Python 构建中未导出，使用 c_ssize_t 替代
# (两者均为指针大小的有符号整数，等价于 C 的 intptr_t)
c_intptr = c_ssize_t


# ============================================================================
# ctypes 结构体映射 (严格对应 ipv_api.h)
# ============================================================================

class IpvParams(Structure):
    """IPV 求解参数 (对应 C 端 IpvParams)"""
    _fields_ = [
        ("polygon_sides", c_int),                   # 多边形边数 (一般 6)
        ("n_pivot", c_int),                         # 主元星数
        ("sigma_d_arcsec", c_double),               # 描述符距离阈值 (角秒)
        ("vote_threshold", c_int),                  # 投票阈值
        ("ransac_max_iter", c_int),                 # RANSAC 最大迭代次数
        ("ransac_inlier_threshold_arcsec", c_double),  # RANSAC 内点阈值 (角秒)
        ("s_min", c_double),                        # 尺度下限
        ("s_max", c_double),                        # 尺度上限
        ("img_n_target", c_int),                    # 期望图像星数
        ("gaia_density_ratio", c_double),           # Gaia 星密度比
        ("gaia_query_radius_factor", c_double),     # Gaia 查询半径因子
        ("m_lim_step", c_double),                   # 星等搜索步长
        ("m_lim_max_iter", c_int),                  # 星等搜索最大迭代
        ("density_tolerance", c_double),            # 密度容差
        ("log_dir", c_char * 256),                  # 日志目录 (空串=不写日志)
    ]


class IpvWcsResult(Structure):
    """IPV 求解结果 (对应 C 端 IpvWcsResult, V4.20 含 AP/BP + ctype)"""
    _fields_ = [
        ("cd", c_double * 4),                       # CD 矩阵 [cd1_1, cd1_2, cd2_1, cd2_2]
        ("crval", c_double * 2),                    # CRVAL [ra, dec] (度)
        ("crpix", c_double * 2),                    # CRPIX [x, y] (1-based)
        ("sip_order", c_int),                       # 前向 SIP 阶数 (0=无 SIP)
        ("sip_a", c_double * 36),                   # SIP A 系数 (前向)
        ("sip_b", c_double * 36),                   # SIP B 系数 (前向)
        ("sip_ap_order", c_int),                    # V4.20: 逆向 SIP 阶数
        ("sip_ap", c_double * 36),                  # V4.20: SIP AP 系数 (逆向)
        ("sip_bp", c_double * 36),                  # V4.20: SIP BP 系数 (逆向)
        ("rms_px", c_double),                       # RMS (像素)
        ("rms_arcsec", c_double),                   # RMS (角秒)
        ("n_pairs", c_int),                         # 匹配对数
        ("success", c_int),                         # 0=失败, 1=成功
        ("n_detected", c_int),                      # 检测星数
        ("n_catalog", c_int),                       # 星表星数
        ("trans_order", c_int),                     # TRANS 多项式阶数 (1=线性, 2=二次, 3=三次, -1=失败)
        ("best_inliers", c_int),                    # 最优内点数
        ("ctype1", c_char * 16),                    # V4.20: "RA---TAN-SIP" / "RA---TAN"
        ("ctype2", c_char * 16),                    # V4.20: "DEC--TAN-SIP" / "DEC--TAN"
        ("error_msg", c_char * 256),                # 错误信息
    ]


# ============================================================================
# IPVSolver 类: 封装 DLL 调用
# ============================================================================

class IPVSolver:
    """IPV Plate Solver Python 封装类"""

    def __init__(self, dll_path=None):
        """
        加载 ipv_solver.dll

        参数:
            dll_path: DLL 文件路径 (str)
                      None=自动检测默认位置 lib/plate_solve/cpp/ipv/ipv_solver.dll
        """
        # 自动检测 DLL 默认路径
        if dll_path is None:
            base = os.path.dirname(os.path.abspath(__file__))
            dll_path = os.path.join(base, "..", "cpp", "ipv", "ipv_solver.dll")
            dll_path = os.path.normpath(dll_path)

        if not os.path.isfile(dll_path):
            raise FileNotFoundError(f"找不到 DLL: {dll_path}")

        self._dll_path = dll_path
        # 加载 DLL
        try:
            self._dll = ctypes.CDLL(dll_path)
        except OSError as e:
            raise RuntimeError(f"加载 DLL 失败: {dll_path}\n原因: {e}")

        # 设置函数签名
        self._setup_signatures()

        # 创建求解器实例
        self._handle = self._dll.ipv_solve_create()
        if not self._handle:
            raise RuntimeError("ipv_solve_create 失败 (返回空句柄)")

    def _setup_signatures(self):
        """设置 DLL 函数签名 (restype / argtypes)"""
        d = self._dll

        # void* ipv_solve_create(void)
        d.ipv_solve_create.restype = c_void_p
        d.ipv_solve_create.argtypes = []

        # void ipv_solve_destroy(void* solver)
        d.ipv_solve_destroy.restype = None
        d.ipv_solve_destroy.argtypes = [c_void_p]

        # void ipv_set_gaia_handle(void* solver, intptr_t handle)
        d.ipv_set_gaia_handle.restype = None
        d.ipv_set_gaia_handle.argtypes = [c_void_p, c_intptr]

        # void ipv_set_detector_handle(void* solver, intptr_t handle)
        d.ipv_set_detector_handle.restype = None
        d.ipv_set_detector_handle.argtypes = [c_void_p, c_intptr]

        # int ipv_solve(void*, const char*, double, double, double, double,
        #               const IpvParams*, IpvWcsResult*)
        d.ipv_solve.restype = c_int
        d.ipv_solve.argtypes = [
            c_void_p,               # solver
            c_char_p,               # image_path (UTF-8)
            c_double,               # ra0
            c_double,               # dec0
            c_double,               # focal_length_mm
            c_double,               # pixel_size_um
            POINTER(IpvParams),     # params
            POINTER(IpvWcsResult),  # result
        ]

        # int ipv_solve_from_memory(void*, const float*, int, int,
        #                           double, double, double, double,
        #                           const IpvParams*, IpvWcsResult*)
        d.ipv_solve_from_memory.restype = c_int
        d.ipv_solve_from_memory.argtypes = [
            c_void_p,               # solver
            POINTER(c_float),       # pixels (float32, row-major)
            c_int,                  # width
            c_int,                  # height
            c_double,               # ra0
            c_double,               # dec0
            c_double,               # focal_length_mm
            c_double,               # pixel_size_um
            POINTER(IpvParams),     # params
            POINTER(IpvWcsResult),  # result
        ]

        # void ipv_get_default_params(IpvParams* params)
        d.ipv_get_default_params.restype = None
        d.ipv_get_default_params.argtypes = [POINTER(IpvParams)]

    def set_gaia_handle(self, handle):
        """
        设置 GaiaClient 句柄

        参数:
            handle: GaiaClient 的 intptr_t 句柄 (int)
        """
        self._dll.ipv_set_gaia_handle(self._handle, c_intptr(handle))

    def set_detector_handle(self, handle):
        """
        设置 StarDetector 句柄

        参数:
            handle: StarDetector 的 intptr_t 句柄 (int)
        """
        self._dll.ipv_set_detector_handle(self._handle, c_intptr(handle))

    def get_default_params(self):
        """
        获取默认参数

        返回:
            IpvParams 实例 (已填充默认值)
        """
        params = IpvParams()
        self._dll.ipv_get_default_params(byref(params))
        return params

    def solve(self, image_path, ra0, dec0, focal_length_mm, pixel_size_um,
              params=None):
        """
        执行 plate solving

        参数:
            image_path: 图像文件路径 (str 或 bytes)
            ra0: 初始指向 RA (度)
            dec0: 初始指向 Dec (度)
            focal_length_mm: 焦距 (mm)
            pixel_size_um: 像素尺寸 (um)
            params: IpvParams 参数 (None=用默认值)

        返回:
            IpvWcsResult 结果对象
        """
        if params is None:
            params = self.get_default_params()

        result = IpvWcsResult()

        # 路径编码为 UTF-8 bytes
        if isinstance(image_path, str):
            image_path = image_path.encode('utf-8')

        ret = self._dll.ipv_solve(
            self._handle,
            image_path,
            c_double(ra0),
            c_double(dec0),
            c_double(focal_length_mm),
            c_double(pixel_size_um),
            byref(params),
            byref(result),
        )

        # ret 为 0 时有两种情况:
        #   (a) 正常求解失败 (匹配/拟合未收敛) - error_msg 为空, result.success=0
        #   (b) C++ 异常 (bad_alloc / std::exception) - error_msg 非空
        # 仅 (b) 抛异常; (a) 返回 result 让调用方按 result.success 判断
        if ret == 0:
            err = result.error_msg.decode('utf-8', errors='ignore').strip()
            if err:
                # C++ 异常路径
                raise RuntimeError(f"ipv_solve 调用失败: {err}")
            # 正常求解失败, 返回 result (result.success=0)
        return result

    def solve_from_memory(self, pixels, width, height, ra0, dec0,
                          focal_length_mm, pixel_size_um, params=None):
        """
        从内存像素数据执行 plate solving (不读文件)

        参数:
            pixels: 像素数据 (numpy float32 数组, row-major, shape=[height, width])
                    也接受 ctypes float 数组或 POINTER(c_float)
            width: 图像宽度 (像素)
            height: 图像高度 (像素)
            ra0: 初始指向 RA (度)
            dec0: 初始指向 Dec (度)
            focal_length_mm: 焦距 (mm)
            pixel_size_um: 像素尺寸 (um)
            params: IpvParams 参数 (None=用默认值)

        返回:
            IpvWcsResult 结果对象

        注意:
            - 消除临时 FITS 文件, 直接传内存指针到 C++ DLL
            - pixels 必须为 C-contiguous float32, 内部会做归一化转换
            - 与 solve() 结果一致 (相同算法, 仅输入方式不同)
        """
        if params is None:
            params = self.get_default_params()

        result = IpvWcsResult()

        # 将 numpy 数组转换为 ctypes float 指针
        # 支持 numpy ndarray / ctypes 数组 / 已有指针
        import numpy as np
        if isinstance(pixels, np.ndarray):
            # 确保 C-contiguous + float32
            if pixels.dtype != np.float32:
                pixels = pixels.astype(np.float32)
            if not pixels.flags['C_CONTIGUOUS']:
                pixels = np.ascontiguousarray(pixels)
            pix_ptr = pixels.ctypes.data_as(POINTER(c_float))
        else:
            # 假设已经是 ctypes 兼容的指针类型
            pix_ptr = pixels

        ret = self._dll.ipv_solve_from_memory(
            self._handle,
            pix_ptr,
            c_int(width),
            c_int(height),
            c_double(ra0),
            c_double(dec0),
            c_double(focal_length_mm),
            c_double(pixel_size_um),
            byref(params),
            byref(result),
        )

        # 与 solve() 一致的错误处理
        if ret == 0:
            err = result.error_msg.decode('utf-8', errors='ignore').strip()
            if err:
                raise RuntimeError(f"ipv_solve_from_memory 调用失败: {err}")
        return result

    def close(self):
        """销毁求解器实例，释放资源"""
        if getattr(self, "_handle", None):
            try:
                self._dll.ipv_solve_destroy(self._handle)
            except Exception:
                pass
            self._handle = None

    def __del__(self):
        """析构时自动清理资源"""
        self.close()


# ============================================================================
# 辅助函数
# ============================================================================

def result_to_dict(result):
    """
    将 IpvWcsResult 转换为字典 (便于打印/序列化)

    参数:
        result: IpvWcsResult 结构体

    返回:
        dict
    """
    return {
        'success': bool(result.success),
        'cd': list(result.cd),
        'crval': list(result.crval),
        'crpix': list(result.crpix),
        'sip_order': result.sip_order,
        'sip_a': list(result.sip_a),
        'sip_b': list(result.sip_b),
        'sip_ap_order': result.sip_ap_order,          # V4.20
        'sip_ap': list(result.sip_ap),                # V4.20
        'sip_bp': list(result.sip_bp),                # V4.20
        'rms_px': result.rms_px,
        'rms_arcsec': result.rms_arcsec,
        'n_pairs': result.n_pairs,
        'n_detected': result.n_detected,
        'n_catalog': result.n_catalog,
        'trans_order': result.trans_order,
        'best_inliers': result.best_inliers,
        'ctype1': result.ctype1.decode('utf-8', errors='ignore').rstrip('\x00'),  # V4.20
        'ctype2': result.ctype2.decode('utf-8', errors='ignore').rstrip('\x00'),  # V4.20
        'error_msg': result.error_msg.decode('utf-8', errors='ignore'),
    }


def to_astropy_wcs(result):
    """
    将 IpvWcsResult 转换为 astropy.wcs.WCS 对象 (含 SIP)

    参数:
        result: IpvWcsResult 结构体

    返回:
        astropy.wcs.WCS 对象
        - sip_order=0 时返回普通 TAN WCS
        - sip_order>0 时返回 RA---TAN-SIP / DEC--TAN-SIP WCS

    依赖: astropy
    """
    from astropy.wcs import WCS, Sip
    import numpy as np

    w = WCS(naxis=2)
    cd11, cd12, cd21, cd22 = result.cd
    # V4.29: C++ extract_wcs_sip() 已在输出边界转换为标准 FITS WCS (Y-down),
    # Python 层直接透传, 无需额外 Y 翻转。
    w.wcs.cd = [[cd11, cd12], [cd21, cd22]]
    w.wcs.crval = [result.crval[0], result.crval[1]]
    w.wcs.crpix = [result.crpix[0], result.crpix[1]]  # 1-based FITS 约定

    if result.sip_order > 0:
        # V4.20: 优先使用 ctype1/ctype2, 回退到根据 sip_order 推断
        ctype1 = result.ctype1.decode('utf-8', errors='ignore').rstrip('\x00')
        ctype2 = result.ctype2.decode('utf-8', errors='ignore').rstrip('\x00')
        if ctype1 and ctype2:
            w.wcs.ctype = [ctype1, ctype2]
        else:
            w.wcs.ctype = ["RA---TAN-SIP", "DEC--TAN-SIP"]
        order = result.sip_order
        # 将扁平 36 系数打包到 (order+1)x(order+1) 矩阵
        # C 端索引: A[i*6+j] 对应 dx^i * dy^j
        a_mat = np.zeros((order + 1, order + 1))
        b_mat = np.zeros((order + 1, order + 1))
        for i in range(order + 1):
            for j in range(order + 1 - i):
                if i * 6 + j < 36:
                    a_mat[i, j] = result.sip_a[i * 6 + j]
                    b_mat[i, j] = result.sip_b[i * 6 + j]
        # V4.20: 逆向 SIP AP/BP (如果可用)
        if result.sip_ap_order > 0:
            ap_order = result.sip_ap_order
            ap_mat = np.zeros((ap_order + 1, ap_order + 1))
            bp_mat = np.zeros((ap_order + 1, ap_order + 1))
            for i in range(ap_order + 1):
                for j in range(ap_order + 1 - i):
                    if i * 6 + j < 36:
                        ap_mat[i, j] = result.sip_ap[i * 6 + j]
                        bp_mat[i, j] = result.sip_bp[i * 6 + j]
            w.sip = Sip(a_mat, b_mat, ap_mat, bp_mat, w.wcs.crpix)
        else:
            # ap/bp 逆多项式设为 None, astropy 内部迭代求解
            w.sip = Sip(a_mat, b_mat, None, None, w.wcs.crpix)
    else:
        w.wcs.ctype = ["RA---TAN", "DEC--TAN"]

    return w


def params_to_dict(params):
    """
    将 IpvParams 转换为字典 (便于查看)

    参数:
        params: IpvParams 结构体

    返回:
        dict
    """
    return {
        'polygon_sides': params.polygon_sides,
        'n_pivot': params.n_pivot,
        'sigma_d_arcsec': params.sigma_d_arcsec,
        'vote_threshold': params.vote_threshold,
        'ransac_max_iter': params.ransac_max_iter,
        'ransac_inlier_threshold_arcsec': params.ransac_inlier_threshold_arcsec,
        's_min': params.s_min,
        's_max': params.s_max,
        'img_n_target': params.img_n_target,
        'gaia_density_ratio': params.gaia_density_ratio,
        'gaia_query_radius_factor': params.gaia_query_radius_factor,
        'm_lim_step': params.m_lim_step,
        'm_lim_max_iter': params.m_lim_max_iter,
        'density_tolerance': params.density_tolerance,
        'log_dir': params.log_dir.decode('utf-8', errors='ignore'),
    }


# ============================================================================
# 模块自测
# ============================================================================

if __name__ == "__main__":
    print("=" * 60)
    print("IPV Plate Solver Python 绑定 - 模块自测")
    print("=" * 60)

    try:
        solver = IPVSolver()
        print(f"[OK] DLL 加载成功: {solver._dll_path}")

        params = solver.get_default_params()
        print(f"[OK] 默认参数:")
        for k, v in params_to_dict(params).items():
            print(f"     {k} = {v}")

        solver.close()
        print("[OK] 资源已释放")
        print("=" * 60)
        print("测试完成")
    except Exception as e:
        print(f"[FAIL] {e}")
        import sys
        sys.exit(1)
