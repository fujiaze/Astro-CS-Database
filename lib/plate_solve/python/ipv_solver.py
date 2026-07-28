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
    Structure, POINTER, byref, CFUNCTYPE
)

# P11-004 v1.3: get_last_inliers 使用 numpy 构造 (N,9) 视图
import numpy as np

# c_intptr 在部分 Python 构建中未导出，使用 c_ssize_t 替代
# (两者均为指针大小的有符号整数，等价于 C 的 intptr_t)
c_intptr = c_ssize_t

# ============================================================================
# P02-002: 路径 B 回调函数类型 (对应 C 端 IpvDetectionCallback)
#   void (*IpvDetectionCallback)(const double* detections, int n_detections, void* user_data)
# 注意: Python 端回调函数必须保持引用, 避免 GC 回收导致 segfault
# ============================================================================
IpvDetectionCallback = CFUNCTYPE(
    None,                      # void return
    POINTER(c_double),         # detections [N,6] FLOAT64
    c_int,                     # n_detections
    c_void_p,                  # user_data
)


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

        # ====================================================================
        # P02-002: 路径 A / 路径 B 函数签名
        # ====================================================================

        # 路径 A: int ipv_solve_from_detections_v1(void*, const double*, int,
        #           int, int, double, double, double, double,
        #           const IpvParams*, IpvWcsResult*)
        d.ipv_solve_from_detections_v1.restype = c_int
        d.ipv_solve_from_detections_v1.argtypes = [
            c_void_p,               # solver
            POINTER(c_double),      # detections [N,6] FLOAT64
            c_int,                  # n_detections
            c_int,                  # image_width
            c_int,                  # image_height
            c_double,               # ra0
            c_double,               # dec0
            c_double,               # focal_length_mm
            c_double,               # pixel_size_um
            POINTER(IpvParams),     # params
            POINTER(IpvWcsResult),  # result
        ]

        # 路径 B: int ipv_solve_from_memory_with_callback(void*, const float*,
        #           int, int, double, double, double, double,
        #           const IpvParams*, IpvDetectionCallback, void*, IpvWcsResult*)
        d.ipv_solve_from_memory_with_callback.restype = c_int
        d.ipv_solve_from_memory_with_callback.argtypes = [
            c_void_p,               # solver
            POINTER(c_float),       # pixels (float32, row-major)
            c_int,                  # width
            c_int,                  # height
            c_double,               # ra0
            c_double,               # dec0
            c_double,               # focal_length_mm
            c_double,               # pixel_size_um
            POINTER(IpvParams),     # params
            IpvDetectionCallback,   # callback (C FUNCTYPE)
            c_void_p,               # user_data
            POINTER(IpvWcsResult),  # result
        ]

        # void ipv_get_default_params(IpvParams* params)
        d.ipv_get_default_params.restype = None
        d.ipv_get_default_params.argtypes = [POINTER(IpvParams)]

        # P11-004 v1.3: 权威 inlier 导出 API
        # int ipv_get_last_inlier_count(void* solver)
        d.ipv_get_last_inlier_count.restype = c_int
        d.ipv_get_last_inlier_count.argtypes = [c_void_p]

        # int ipv_get_last_inliers(void* solver, double* out_buffer, int max_count)
        d.ipv_get_last_inliers.restype = c_int
        d.ipv_get_last_inliers.argtypes = [c_void_p, POINTER(c_double), c_int]

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

    def get_last_inlier_count(self):
        """
        P11-004 v1.3: 获取最后一次成功求解的权威 inlier 数量

        返回:
            int: inlier 数 (0 表示无缓存或求解失败)
        """
        return int(self._dll.ipv_get_last_inlier_count(self._handle))

    def get_last_inliers(self, max_count=None):
        """
        P11-004 v1.3: 获取最后一次成功求解的权威 inlier 详细数据

        用于 WCS Gate v2 双层闭环, 避免外部诊断工具用 kd-tree 重新匹配导致误配。
        详见 docs/24_WCS_VALIDATION_V2_SPEC.md 与 docs/25_AUTHORITATIVE_MATCH_PAIR_CONTRACT.md

        参数:
            max_count: int or None, 缓冲区最大行数; None 时自动用 get_last_inlier_count()

        返回:
            numpy.ndarray, shape=(N, 9), dtype=float64
            若无缓存或求解失败, 返回空数组 shape=(0, 9)
            每行 9 个字段:
                [0] det_x_px       - 检测器 x (像素, 图像中心原点, Y 轴向上)
                [1] det_y_px       - 检测器 y
                [2] gaia_ra_deg    - Gaia RA (度)
                [3] gaia_dec_deg   - Gaia Dec (度)
                [4] pred_x_px      - 内部 TRANS 预测 x (像素, 经 s0 缩放)
                [5] pred_y_px      - 内部 TRANS 预测 y
                [6] residual_x_px  - 残差 x = det_x - pred_x (像素)
                [7] residual_y_px  - 残差 y = det_y - pred_y
                [8] residual_dist_px - 残差距离 sqrt(res_x² + res_y²)
        """
        n = int(self._dll.ipv_get_last_inlier_count(self._handle))
        if n <= 0:
            return np.empty((0, 9), dtype=np.float64)

        if max_count is None:
            max_count = n
        else:
            max_count = min(int(max_count), n)

        if max_count <= 0:
            return np.empty((0, 9), dtype=np.float64)

        buf = (c_double * (max_count * 9))()
        ret = self._dll.ipv_get_last_inliers(self._handle, buf, max_count)
        if ret < 0:
            raise RuntimeError(f"ipv_get_last_inliers 调用失败 (ret={ret})")
        arr = np.frombuffer(buf, dtype=np.float64).reshape(ret, 9).copy()
        return arr

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

    # ========================================================================
    # P02-002: 路径 A / 路径 B 接口 (实验性)
    # ========================================================================

    def solve_from_detections_v1(self, detections, image_width, image_height,
                                  ra0, dec0, focal_length_mm, pixel_size_um,
                                  params=None):
        """
        P02-002 路径 A: 从外部 detections 求解 (跳过 sdet_detect_ex)

        参数:
            detections: 检测结果, FLOAT64 [N,6] star_det v1 格式
                        列: x_px, y_px, flux, mag, saturated(0/1), has_saturated(0/1)
                        支持 numpy.ndarray (shape=[N,6], dtype=float64) 或 ctypes 指针
            image_width:  图像宽度 (像素)
            image_height: 图像高度 (像素)
            ra0: 初始指向 RA (度)
            dec0: 初始指向 Dec (度)
            focal_length_mm: 焦距 (mm)
            pixel_size_um: 像素尺寸 (um)
            params: IpvParams 参数 (None=用默认值)

        返回:
            IpvWcsResult 结果对象

        注意:
            - 跳过 star_detector, 直接使用调用方提供的检测结果
            - 算法其余部分与 solve_from_memory 完全一致
            - 调用方负责确保 detections 格式正确
        """
        if params is None:
            params = self.get_default_params()

        result = IpvWcsResult()

        import numpy as np
        if isinstance(detections, np.ndarray):
            # 确保 C-contiguous + float64 + shape=[N,6]
            if detections.dtype != np.float64:
                detections = detections.astype(np.float64)
            if not detections.flags['C_CONTIGUOUS']:
                detections = np.ascontiguousarray(detections)
            if detections.ndim != 2 or detections.shape[1] != 6:
                raise ValueError(
                    f"detections 必须是 [N,6] 数组, 实际 shape={detections.shape}"
                )
            det_ptr = detections.ctypes.data_as(POINTER(c_double))
            n_det = int(detections.shape[0])
        else:
            # 假设已是 ctypes 兼容指针, n_detections 需调用方另行提供
            # 这里不支持该模式, 因为 n_detections 必须明确
            raise TypeError(
                "detections 必须是 numpy.ndarray (shape=[N,6], dtype=float64)"
            )

        ret = self._dll.ipv_solve_from_detections_v1(
            self._handle,
            det_ptr,
            c_int(n_det),
            c_int(image_width),
            c_int(image_height),
            c_double(ra0),
            c_double(dec0),
            c_double(focal_length_mm),
            c_double(pixel_size_um),
            byref(params),
            byref(result),
        )

        if ret == 0:
            err = result.error_msg.decode('utf-8', errors='ignore').strip()
            if err:
                raise RuntimeError(f"ipv_solve_from_detections_v1 调用失败: {err}")
        return result

    def solve_from_memory_with_callback(self, pixels, width, height,
                                         ra0, dec0, focal_length_mm,
                                         pixel_size_um, callback, user_data=None,
                                         params=None):
        """
        P02-002 路径 B: 带 callback 的内存求解 (保持原有检测 + 导出检测结果)

        参数:
            pixels: 像素数据 (numpy float32 数组, row-major, shape=[height, width])
            width: 图像宽度 (像素)
            height: 图像高度 (像素)
            ra0: 初始指向 RA (度)
            dec0: 初始指向 Dec (度)
            focal_length_mm: 焦距 (mm)
            pixel_size_um: 像素尺寸 (um)
            callback: 检测结果导出回调, 签名 (detections_arr, n_detections, user_data)
                      detections_arr: numpy.ndarray float64 [N,6] (在回调内有效, 回调返回后失效)
                      n_detections: 检测星数
                      user_data: 调用方传入的 user_data
                      传 None 时行为与 solve_from_memory 完全一致
            user_data: 传给 callback 的用户数据 (任意 Python 对象)
                       注意: ctypes 会将其转为 c_void_p (整数), 回调内需要自行还原
            params: IpvParams 参数 (None=用默认值)

        返回:
            IpvWcsResult 结果对象

        注意:
            - 算法与 solve_from_memory 完全一致, 区别仅在 sdet_detect_ex 后调用 callback
            - callback 内必须立即复制 detections 数据, 返回后缓冲区失效
            - 本方法负责将 C 指针包装为 numpy 数组传给 Python callback
        """
        if params is None:
            params = self.get_default_params()

        result = IpvWcsResult()

        # 将 numpy 数组转换为 ctypes float 指针
        import numpy as np
        if isinstance(pixels, np.ndarray):
            if pixels.dtype != np.float32:
                pixels = pixels.astype(np.float32)
            if not pixels.flags['C_CONTIGUOUS']:
                pixels = np.ascontiguousarray(pixels)
            pix_ptr = pixels.ctypes.data_as(POINTER(c_float))
        else:
            pix_ptr = pixels

        # 包装 Python callback 为 C FUNCTYPE
        # 必须保持引用避免 GC, 存到局部变量 (函数作用域内有效)
        cb_ref = None
        if callback is not None:
            def _trampoline(det_ptr, n_det, ud_ptr):
                """C→Python 回调桥: 将 [N,6] FLOAT64 指针包装为 numpy 数组"""
                try:
                    if det_ptr and n_det > 0:
                        # 从 C 指针构造 numpy 数组 (零拷贝视图)
                        arr = np.ctypeslib.as_array(det_ptr, shape=(n_det, 6))
                        # 复制一份, 防止 C 端释放后 Python 端访问失效
                        arr_copy = arr.copy()
                        callback(arr_copy, int(n_det), user_data)
                except Exception as e:
                    # 回调内异常不能泄漏到 C 边界, 打印到 stderr
                    import sys
                    print(f"[IPVSolver callback] 异常: {e}", file=sys.stderr)

            cb_ref = IpvDetectionCallback(_trampoline)

        ret = self._dll.ipv_solve_from_memory_with_callback(
            self._handle,
            pix_ptr,
            c_int(width),
            c_int(height),
            c_double(ra0),
            c_double(dec0),
            c_double(focal_length_mm),
            c_double(pixel_size_um),
            byref(params),
            cb_ref if cb_ref is not None else IpvDetectionCallback(0),
            None,  # user_data 已通过闭包捕获, 这里传 None
            byref(result),
        )

        # 显式释放 cb_ref 引用 (避免循环引用)
        del cb_ref

        if ret == 0:
            err = result.error_msg.decode('utf-8', errors='ignore').strip()
            if err:
                raise RuntimeError(f"ipv_solve_from_memory_with_callback 调用失败: {err}")
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
