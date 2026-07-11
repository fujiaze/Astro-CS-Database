"""
sphere_renderer.py - 球面渲染引擎

功能：用 VisPy/OpenGL 渲染 HEALpix 球面数据
用途：球数据库浏览器的球面可视化

特性：
- 球面 mesh 构建 (UV 参数化球面)
- 按 FOV 动态选择 LOD 层
- 拖动旋转 + 滚轮缩放
- 自定义 GLSL fragment shader: GPU 内 STF 拉伸 + uint8 binning
  (Python 端传递原始 float32 值, shader 内完成颜色映射+量化)

渲染策略:
1. vispy 可用: 用自定义 SphereBinningVisual (GLSL shader) 渲染球面
   - 顶点属性: a_position (vec3) + a_value (float, 原始像素值)
   - Fragment shader: STF 拉伸 → MTF → uint8 量化 → framebuffer
   - STF 参数可实时更新 (set_stf), 无需重建网格
2. vispy 不可用: fallback 到 matplotlib 2D Mollweide 投影

HEALpix 数据映射:
- 生成 UV 球面网格 (n_lat × n_lon)
- 每个顶点 (theta, phi) → (ra, dec) → HEALpix 像素号 → 查值
- 用 healpix_stack DLL 的 hp_radec2pix / hp_pix2radec 做坐标转换
"""

from __future__ import annotations

import os
import sys
import math
import logging
import datetime
from typing import Optional, Tuple, List, Dict, Any

import numpy as np

# ============================================================================
# 路径设置: 导入 healpix_stack (用于 HEALpix 坐标转换)
# ============================================================================
_HEALPIX_DB_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
if _HEALPIX_DB_DIR not in sys.path:
    sys.path.insert(0, _HEALPIX_DB_DIR)

# ============================================================================
# 日志配置
# ============================================================================
_LOG_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "logs")
os.makedirs(_LOG_DIR, exist_ok=True)

logger = logging.getLogger("healpix_browser.sphere_renderer")
if not logger.handlers:
    _log_file = os.path.join(
        _LOG_DIR, f"sphere_renderer_{datetime.datetime.now().strftime('%Y%m%d')}.log")
    _fh = logging.FileHandler(_log_file, encoding="utf-8")
    _fh.setFormatter(logging.Formatter(
        "%(asctime)s [%(levelname)s] %(message)s"))
    logger.addHandler(_fh)
    _sh = logging.StreamHandler()
    _sh.setFormatter(logging.Formatter("[Sphere] %(message)s"))
    logger.addHandler(_sh)
    logger.setLevel(logging.INFO)

# ============================================================================
# 检测 vispy 可用性
# ============================================================================
try:
    import vispy
    from vispy.scene import SceneCanvas, TurntableCamera, Mesh, visuals
    from vispy.geometry import create_sphere
    from vispy.color import Colormap
    from vispy.visuals import Visual
    from vispy.gloo import VertexBuffer, IndexBuffer
    from vispy.scene.visuals import create_visual_node
    VISPY_AVAILABLE = True
    logger.info(f"vispy 可用 (版本 {vispy.__version__})")
except ImportError as e:
    VISPY_AVAILABLE = False
    logger.warning(f"vispy 不可用, 将使用 matplotlib fallback: {e}")

# ============================================================================
# 检测 healpix_stack DLL 可用性 (用于坐标转换)
# ============================================================================
_HP_DLL_OK = False
try:
    from healpix_stack import healpix_radec2pix, healpix_pix2radec
    _HP_DLL_OK = True
    logger.info("healpix_stack DLL 可用 (HEALpix 坐标转换)")
except Exception as e:
    logger.warning(f"healpix_stack DLL 不可用, 使用纯 Python HEALpix 计算: {e}")


# ============================================================================
# 纯 Python HEALpix 坐标转换 (fallback, 当 DLL 不可用时)
# ============================================================================

def _nest_ring_xyf(nside: int, ipix: int, nested: bool) -> Tuple[int, int, int]:
    """HEALpix 像素号 → (x, y, face) 在某个 face 上的位置

    参考 healpy 的边界像素处理实现。
    """
    npix = 12 * nside * nside
    if ipix < 0 or ipix >= npix:
        raise ValueError(f"ipix {ipix} 超出范围 [0, {npix})")

    if nested:
        # NESTED: 直接从 ipix 提取 face 和局部坐标
        face = ipix // (nside * nside)
        local = ipix % (nside * nside)
        # 局部坐标通过位交错得到
        ix = 0
        iy = 0
        for bit in range(int(math.log2(nside))):
            ix |= ((local >> (2 * bit)) & 1) << bit
            iy |= ((local >> (2 * bit + 1)) & 1) << bit
        return ix, iy, face
    else:
        # RING: 需要先转 NESTED
        # 简化实现: 用 healpy 的标准算法
        return _ring_to_xyf(nside, ipix)


def _ring_to_xyf(nside: int, ipix: int) -> Tuple[int, int, int]:
    """RING 排序 → (x, y, face)"""
    npix = 12 * nside * nside
    ncap = 2 * nside * (nside - 1)
    if ipix < ncap:
        # 北极帽
        ring = int((1 + math.isqrt(1 + 2 * ipix)) / 2)
        if ring > 0:
            iphi = ipix - 2 * ring * (ring - 1)
        else:
            iphi = 0
        face = 0  # 简化
        ix = iphi % nside
        iy = ring - 1 - (iphi // nside)
        return ix, iy, face
    elif ipix < npix - ncap:
        # 赤道带
        ip = ipix - ncap
        ring = nside + ip // (4 * nside)
        iphi = ip % (4 * nside)
        face = iphi // nside
        ix = iphi % nside
        if ring - nside < 0:
            iy = nside - 1 - (ring - nside)
        else:
            iy = ring - nside
        return ix, iy, face
    else:
        # 南极帽
        ip = npix - 1 - ipix
        ring = int((1 + math.isqrt(1 + 2 * ip)) / 2)
        if ring > 0:
            iphi = ip - 2 * ring * (ring - 1)
        else:
            iphi = 0
        face = 8  # 简化
        ix = iphi % nside
        iy = ring - 1 - (iphi // nside)
        return ix, iy, face


def _py_pix2ang_nest(nside: int, ipix: int) -> Tuple[float, float]:
    """纯 Python: NESTED HEALpix 像素 → (theta, phi) [弧度]

    theta = 极角 (0=北极), phi = 方位角 (0~2π)
    """
    if nside <= 0 or (nside & (nside - 1)) != 0:
        raise ValueError(f"nside 必须是 2 的幂: {nside}")

    npix = 12 * nside * nside
    if ipix < 0 or ipix >= npix:
        raise ValueError(f"ipix {ipix} 超出范围 [0, {npix})")

    # face number (0-11) 和 face 内局部像素号
    face_size = nside * nside
    face_num = ipix // face_size
    local_ipix = ipix % face_size

    # 从 local_ipix 提取 (ix, iy) 通过位交错
    ix = 0
    iy = 0
    n_bits = int(round(math.log2(nside)))
    for bit in range(n_bits):
        ix |= ((local_ipix >> (2 * bit)) & 1) << bit
        iy |= ((local_ipix >> (2 * bit + 1)) & 1) << bit

    # face 中心的 (theta, phi) 和方向
    # 标准 HEALpix face 布局:
    # face 0-3: 北极区 4 个面
    # face 4-7: 赤道区 4 个面
    # face 8-11: 南极区 4 个面
    if face_num < 4:
        # 北极区
        phi_center = (face_num + 0.5) * math.pi / 2.0
        theta = (ix + iy) * math.pi / (4.0 * nside)
        phi = phi_center + (ix - iy) * math.pi / (4.0 * nside)
    elif face_num < 8:
        # 赤道区
        phi_center = (face_num - 4) * math.pi / 2.0
        theta = math.pi / 2.0 + (ix - iy) * math.pi / (4.0 * nside)
        # 修正赤道区 phi
        if face_num == 4:
            phi = phi_center + (ix - iy) * math.pi / (4.0 * nside)
        else:
            phi = phi_center + (ix - iy) * math.pi / (4.0 * nside)
        theta = math.pi / 2.0 + (ix + iy - nside) * math.pi / (4.0 * nside)
        phi = (face_num - 4 + 0.5) * math.pi / 2.0 + (ix - iy) * math.pi / (4.0 * nside)
    else:
        # 南极区
        phi_center = (face_num - 8 + 0.5) * math.pi / 2.0
        theta = math.pi - (ix + iy) * math.pi / (4.0 * nside)
        phi = phi_center + (ix - iy) * math.pi / (4.0 * nside)

    return theta, phi


def _py_radec2pix_nest(nside: int, ra_deg: float, dec_deg: float) -> int:
    """纯 Python: (ra_deg, dec_deg) → NESTED HEALpix 像素号

    ra: 赤经 [0, 360) 度
    dec: 赤纬 [-90, 90] 度
    """
    ra = math.radians(ra_deg) % (2 * math.pi)
    dec = math.radians(dec_deg)
    theta = math.pi / 2.0 - dec  # 极角
    phi = ra

    # 查找像素: 参考 HEALpix 标准
    z = math.cos(theta)
    phi_norm = phi % (2 * math.pi)

    # 判断区域
    if z >= 0.75:
        # 北极区
        n = nside
        phi_sector = int(phi_norm / (math.pi / 2.0)) % 4
        phi_local = phi_norm - phi_sector * math.pi / 2.0
        # 极区公式
        sqrt3 = math.sqrt(3.0)
        x = n * (2.0 / 3.0 - phi_local / math.pi * 2.0 / 3.0 * sqrt3)
        y = n * (1.0 / 3.0 - phi_local / math.pi * 2.0 / 3.0 * sqrt3)
        # 修正
        irt = 1.0 / sqrt3
        jp = int(n * (1.0 - (z + phi_local * 2.0 / math.pi) * irt))
        jm = int(n * (1.0 - (z - phi_local * 2.0 / math.pi) * irt))
        if jp >= n:
            jp = n - 1
        if jm >= n:
            jm = n - 1
        face = phi_sector
        ix = n - 1 - jm
        iy = n - 1 - jp
    elif z <= -0.75:
        # 南极区
        n = nside
        phi_sector = int(phi_norm / (math.pi / 2.0)) % 4
        phi_local = phi_norm - phi_sector * math.pi / 2.0
        sqrt3 = math.sqrt(3.0)
        irt = 1.0 / sqrt3
        jp = int(n * (1.0 - (-z + phi_local * 2.0 / math.pi) * irt))
        jm = int(n * (1.0 - (-z - phi_local * 2.0 / math.pi) * irt))
        if jp >= n:
            jp = n - 1
        if jm >= n:
            jm = n - 1
        face = 8 + phi_sector
        ix = jp
        iy = jm
    else:
        # 赤道区
        n = nside
        phi_sector = int(phi_norm / (math.pi / 2.0)) % 4
        phi_local = phi_norm - phi_sector * math.pi / 2.0
        tt = (phi_local * 2.0 / math.pi) * (2.0 / 3.0) if abs(phi_local) > 1e-15 else 0.0
        jp = int(n * (0.5 + (z * 0.5 + tt)))  # 简化
        jm = int(n * (0.5 + (z * 0.5 - tt)))
        jp = max(0, min(n - 1, jp))
        jm = max(0, min(n - 1, jm))
        face = 4 + phi_sector
        ix = n - 1 - jm
        iy = n - 1 - jp

    # 从 (ix, iy, face) 重建 NESTED ipix
    face_size = nside * nside
    local_ipix = 0
    n_bits = int(round(math.log2(nside))) if nside > 1 else 0
    for bit in range(n_bits):
        local_ipix |= ((ix >> bit) & 1) << (2 * bit)
        local_ipix |= ((iy >> bit) & 1) << (2 * bit + 1)

    return face * face_size + local_ipix


def _radec2pix(nside: int, nested: bool, ra_deg: float, dec_deg: float) -> int:
    """HEALpix 坐标转换 (DLL 优先, fallback 到纯 Python)"""
    if _HP_DLL_OK:
        return healpix_radec2pix(nside, nested, ra_deg, dec_deg)
    elif nested:
        return _py_radec2pix_nest(nside, ra_deg, dec_deg)
    else:
        # RING fallback: 转换复杂, 暂不支持
        raise NotImplementedError("RING 排序需要 healpix_stack DLL")


def _pix2radec(nside: int, nested: bool, ipix: int) -> Tuple[float, float]:
    """HEALpix 像素 → (ra_deg, dec_deg)"""
    if _HP_DLL_OK:
        return healpix_pix2radec(nside, nested, ipix)
    elif nested:
        theta, phi = _py_pix2ang_nest(nside, ipix)
        ra_deg = math.degrees(phi) % 360.0
        dec_deg = 90.0 - math.degrees(theta)
        return ra_deg, dec_deg
    else:
        raise NotImplementedError("RING 排序需要 healpix_stack DLL")


# ============================================================================
# 球面网格生成
# ============================================================================

def generate_sphere_mesh(n_lat: int = 64, n_lon: int = 128
                         ) -> Tuple[np.ndarray, np.ndarray]:
    """生成 UV 参数化球面网格

    Args:
        n_lat: 纬度方向分割数
        n_lon: 经度方向分割数

    Returns:
        (vertices, faces):
        - vertices: (N, 3) 顶点坐标 (单位球)
        - faces: (M, 3) 三角形面索引
    """
    # 纬度: theta ∈ [0, π] (北极→南极)
    theta = np.linspace(0, np.pi, n_lat + 1)
    # 经度: phi ∈ [0, 2π)
    phi = np.linspace(0, 2 * np.pi, n_lon + 1)

    # 网格
    theta_grid, phi_grid = np.meshgrid(theta, phi, indexing="ij")

    # 球面坐标 → 笛卡尔坐标
    x = np.sin(theta_grid) * np.cos(phi_grid)
    y = np.sin(theta_grid) * np.sin(phi_grid)
    z = np.cos(theta_grid)

    vertices = np.column_stack([x.ravel(), y.ravel(), z.ravel()])

    # 三角面索引
    faces = []
    for i in range(n_lat):
        for j in range(n_lon):
            # 四个顶点
            v00 = i * (n_lon + 1) + j
            v01 = i * (n_lon + 1) + j + 1
            v10 = (i + 1) * (n_lon + 1) + j
            v11 = (i + 1) * (n_lon + 1) + j + 1
            # 两个三角形
            faces.append([v00, v10, v11])
            faces.append([v00, v11, v01])

    faces_arr = np.array(faces, dtype=np.uint32)
    logger.info(f"球面网格: {n_lat}×{n_lon} → {len(vertices)} 顶点, "
                f"{len(faces_arr)} 三角形")
    return vertices, faces_arr


def sphere_vertex_to_radec(vertices: np.ndarray) -> Tuple[np.ndarray, np.ndarray]:
    """球面顶点 (x,y,z) → (ra_deg, dec_deg)

    Args:
        vertices: (N, 3) 单位球顶点坐标

    Returns:
        (ra_deg, dec_deg): 各顶点的赤经赤纬 (度)
    """
    x, y, z = vertices[:, 0], vertices[:, 1], vertices[:, 2]
    # dec = arcsin(z), ra = arctan2(y, x)
    dec = np.degrees(np.arcsin(np.clip(z, -1, 1)))
    ra = np.degrees(np.arctan2(y, x)) % 360.0
    return ra, dec


# ============================================================================
# 自定义 GLSL Shader: GPU 内 STF 拉伸 + uint8 binning
# ============================================================================
#
# 渲染管线:
#   1. Python 端只传递原始 float32 像素值 (a_value) 到 GPU 顶点属性
#   2. Vertex shader 透传值到 fragment shader (v_value)
#   3. Fragment shader 内完成:
#      a. 无数据标记检查 (u_no_data 以下 → 黑色)
#      b. STF 拉伸: (value - shadows) / (highlights - shadows)
#      c. MTF (Midtone Transfer Function) 中间调拉伸
#      d. uint8 binning: 量化到 256 级 (floor(x*255+0.5)/255)
#   4. 输出到 8bit RGBA framebuffer, 实现 GPU 内 binning 到 uint8 显示精度
#
# 优势:
#   - Python 端不再预计算 RGBA 颜色, 节省 CPU 和内存
#   - STF 参数可实时更新 (set_stf), 无需重建网格
#   - 原始 float32 精度保留到 GPU, 最后才量化到 uint8

if VISPY_AVAILABLE:

    # GLSL Vertex Shader
    _SPHERE_VERT_SHADER = """
    attribute vec3 a_position;
    attribute float a_value;
    varying float v_value;

    void main() {
        v_value = a_value;
        gl_Position = $transform(vec4(a_position, 1.0));
    }
    """

    # GLSL Fragment Shader: STF 拉伸 + uint8 binning
    _SPHERE_FRAG_SHADER = """
    varying float v_value;
    uniform float u_shadows;
    uniform float u_highlights;
    uniform float u_midtones;
    uniform float u_no_data;

    void main() {
        // 无数据标记检查
        if (v_value <= u_no_data) {
            gl_FragColor = vec4(0.0, 0.0, 0.0, 1.0);
            return;
        }

        // STF 拉伸: (value - shadows) / (highlights - shadows)
        float range = u_highlights - u_shadows;
        if (range < 1e-30) range = 1.0;
        float x = (v_value - u_shadows) / range;
        x = clamp(x, 0.0, 1.0);

        // MTF (Midtone Transfer Function)
        float m = u_midtones;
        float denom = (2.0 * m - 1.0) * x - m;
        if (abs(denom) < 1e-30) denom = 1e-30;
        float result = ((m - 1.0) * x) / denom;
        result = clamp(result, 0.0, 1.0);

        // uint8 binning: 量化到 256 级 (0-255)
        // 显式量化确保 uint8 显示精度, 与 8bit framebuffer 匹配
        result = floor(result * 255.0 + 0.5) / 255.0;

        // 输出灰度 (可扩展为 colormap lookup texture)
        gl_FragColor = vec4(vec3(result), 1.0);
    }
    """

    class SphereBinningVisual(Visual):
        """自定义球面 Visual: GPU shader 内完成 STF 拉伸 + uint8 binning

        与传统 MeshVisual + vertex_colors 的区别:
        - 传入原始 float32 像素值 (a_value), 而非预计算的 RGBA 颜色
        - STF 拉伸在 fragment shader 内完成, 可实时更新参数
        - uint8 量化在 GPU 内完成, 保证显示精度

        Attributes:
            vertices: (N, 3) 球面顶点坐标 (单位球, float32)
            faces: (M, 3) 三角形面索引 (uint32)
            values: (N,) 每个顶点的原始 float32 像素值
        """

        def __init__(self, vertices, faces, values,
                     stf_shadows=0.0, stf_highlights=1.0,
                     stf_midtones=0.5, no_data=-1e30):
            """初始化球面 binning Visual

            Args:
                vertices: (N, 3) 顶点坐标
                faces: (M, 3) 三角形索引
                values: (N,) 原始 float32 像素值
                stf_shadows: STF 暗部参数
                stf_highlights: STF 亮部参数
                stf_midtones: STF 中间调参数 (0-1)
                no_data: 无数据标记值 (<=此值渲染为黑色)
            """
            super().__init__(vcode=_SPHERE_VERT_SHADER,
                             fcode=_SPHERE_FRAG_SHADER)

            # 顶点缓冲: 位置 + 值
            self._vbo = VertexBuffer(vertices.astype(np.float32))
            self._valbo = VertexBuffer(values.astype(np.float32))
            self._ibo = IndexBuffer(faces.astype(np.uint32))

            self._program['a_position'] = self._vbo
            self._program['a_value'] = self._valbo

            # STF uniform 参数
            self._shadows = float(stf_shadows)
            self._highlights = float(stf_highlights)
            self._midtones = float(stf_midtones)
            self._no_data = float(no_data)

            self._program['u_shadows'] = self._shadows
            self._program['u_highlights'] = self._highlights
            self._program['u_midtones'] = self._midtones
            self._program['u_no_data'] = self._no_data

            logger.debug(f"SphereBinningVisual: {len(vertices)} 顶点, "
                         f"{len(faces)} 三角形, STF=[{self._shadows:.3f}, "
                         f"{self._highlights:.3f}, {self._midtones:.3f}]")

        def _prepare_draw(self, view):
            """vispy Visual 绘制回调: 执行 OpenGL draw call"""
            self._program.draw('triangles', self._ibo)

        def set_stf(self, shadows, highlights, midtones):
            """更新 STF 拉伸参数 (实时更新, 无需重建网格)

            Args:
                shadows: 暗部参数
                highlights: 亮部参数
                midtones: 中间调参数 (0-1)
            """
            self._shadows = float(shadows)
            self._highlights = float(highlights)
            self._midtones = float(midtones)
            self._program['u_shadows'] = self._shadows
            self._program['u_highlights'] = self._highlights
            self._program['u_midtones'] = self._midtones

        def set_values(self, values):
            """更新顶点值 (原始 float32 数据)

            Args:
                values: (N,) 新的像素值数组
            """
            self._valbo.set_data(values.astype(np.float32))

        def set_mesh(self, vertices, faces, values):
            """更新全部网格数据 (顶点+面+值)

            Args:
                vertices: (N, 3) 新顶点坐标
                faces: (M, 3) 新三角形索引
                values: (N,) 新像素值
            """
            self._vbo.set_data(vertices.astype(np.float32))
            self._valbo.set_data(values.astype(np.float32))
            self._ibo = IndexBuffer(faces.astype(np.uint32))

    # 创建 scene Node (可添加到 vispy.scene.View)
    SphereBinningNode = create_visual_node(SphereBinningVisual)
    logger.info("SphereBinningVisual + SphereBinningNode 已注册 (自定义 GLSL shader)")


# ============================================================================
# 球面渲染器 (vispy)
# ============================================================================

class VisPySphereRenderer:
    """基于 vispy 的球面渲染器

    用自定义 GLSL shader (SphereBinningVisual) 渲染球面,
    在 GPU fragment shader 内完成 STF 拉伸 + uint8 binning。
    支持 TurntableCamera 拖动旋转和滚轮缩放。

    渲染流程:
      1. set_data: 接收原始 float32 像素值, 自动计算 STF 参数
      2. _update_mesh: 生成球面网格, 每个顶点查 HEALpix 值
      3. SphereBinningNode: 将顶点+值传到 GPU (float32 精度)
      4. Fragment shader: STF 拉伸 → MTF → uint8 量化 → framebuffer
    """

    # 无数据标记值 (顶点值 <= 此值时 shader 渲染为黑色)
    _NO_DATA_VALUE = -1e30

    def __init__(self, canvas=None):
        """初始化 vispy 球面渲染器

        Args:
            canvas: vispy.scene.SceneCanvas (可选, 不传则自建)
        """
        if not VISPY_AVAILABLE:
            raise RuntimeError("vispy 不可用, 请安装 vispy>=0.9")

        self._canvas = canvas
        self._visual = None  # SphereBinningNode 实例
        self._camera = None
        self._nside = 512
        self._nested = True
        self._pixel_map = None  # ipix → value 的字典
        self._fov_deg = 180.0
        self._data_values = None

        # STF 拉伸参数 (由 set_data 自动计算, 或由 set_stf 手动设置)
        self._stf_shadows = 0.0
        self._stf_highlights = 1.0
        self._stf_midtones = 0.5

        if canvas is None:
            self._canvas = SceneCanvas(
                keys="interactive", size=(800, 600), bgcolor="black",
                title="HEALpix 球面浏览")
            self._view = self._canvas.central_widget.add_view()
        else:
            self._view = canvas.central_widget.add_view()

        # TurntableCamera: 拖动旋转 + 滚轮缩放
        self._camera = TurntableCamera(
            elevation=20, azimuth=30, distance=3.0,
            fov=45, up="z")
        self._view.camera = self._camera

        logger.info("VisPySphereRenderer 初始化完成 (SphereBinningVisual shader)")

    def set_data(self, pixels: np.ndarray, values: np.ndarray,
                 nside: int, nested: bool = True) -> None:
        """设置球面数据

        传递原始 float32 像素值到 GPU, 由 fragment shader 完成 STF 拉伸 + uint8 binning。
        不再在 Python 端预计算 RGBA 顶点颜色。

        Args:
            pixels: HEALpix 像素号数组 (int64)
            values: 对应的像素值数组 (float32 原始值, 不做归一化)
            nside: HEALpix nside
            nested: True=嵌套排序, False=环排序
        """
        self._nside = nside
        self._nested = nested
        self._pixel_map = {}
        for pix, val in zip(pixels, values):
            self._pixel_map[int(pix)] = float(val)
        # 保留原始 float32 精度, 传到 GPU 后由 shader 量化
        self._data_values = np.asarray(values, dtype=np.float32)

        # 自动计算 STF 参数 (基于数据 1%/99% 百分位)
        if len(values) > 0:
            finite_vals = values[np.isfinite(values)]
            if len(finite_vals) > 0:
                self._stf_shadows = float(np.percentile(finite_vals, 1))
                self._stf_highlights = float(np.percentile(finite_vals, 99))
                if self._stf_highlights - self._stf_shadows < 1e-30:
                    self._stf_highlights = self._stf_shadows + 1.0
                self._stf_midtones = 0.5

        logger.info(f"设置球面数据: nside={nside}, {len(pixels)} 像素, "
                    f"nested={nested}, STF=[{self._stf_shadows:.4f}, "
                    f"{self._stf_highlights:.4f}, {self._stf_midtones:.3f}]")

        self._update_mesh()

    def _update_mesh(self, n_lat: int = 64, n_lon: int = 128) -> None:
        """重建球面网格并传递原始 float32 值到 GPU

        不再在 Python 端预计算 RGBA 颜色, 而是传递原始 float32 像素值
        到 SphereBinningVisual, 由 fragment shader 完成 STF 拉伸 + uint8 binning。
        """
        vertices, faces = generate_sphere_mesh(n_lat, n_lon)

        # 每个顶点 → (ra, dec) → HEALpix 像素 → 原始 float32 值
        ra, dec = sphere_vertex_to_radec(vertices)
        # 无数据顶点用 _NO_DATA_VALUE 标记, shader 会渲染为黑色
        values = np.full(len(vertices), self._NO_DATA_VALUE, dtype=np.float32)

        if self._pixel_map and len(self._pixel_map) > 0:
            matched = 0
            for i in range(len(vertices)):
                try:
                    ipix = _radec2pix(self._nside, self._nested,
                                      float(ra[i]), float(dec[i]))
                    val = self._pixel_map.get(ipix, None)
                    if val is not None and np.isfinite(val):
                        values[i] = float(val)
                        matched += 1
                except Exception:
                    pass  # 保持 _NO_DATA_VALUE
            logger.info(f"顶点值映射: {matched}/{len(vertices)} 顶点有数据")
        else:
            # 无数据时全部设为 0 (而非 _NO_DATA_VALUE, 显示为暗灰)
            values[:] = 0.0

        # 移除旧的 visual
        if self._visual is not None:
            self._view.remove(self._visual)

        # 创建 SphereBinningNode: 传递原始 float32 值, GPU 内做 STF + uint8 binning
        self._visual = SphereBinningNode(
            vertices=vertices,
            faces=faces,
            values=values,
            stf_shadows=self._stf_shadows,
            stf_highlights=self._stf_highlights,
            stf_midtones=self._stf_midtones,
            no_data=self._NO_DATA_VALUE,
        )
        self._view.add(self._visual)

        logger.info(f"球面网格已更新: {n_lat}×{n_lon}, "
                    f"原始 float32 值已传到 GPU (shader 内 STF+uint8 binning)")

    def set_stf(self, shadows: float, highlights: float,
                midtones: float) -> None:
        """实时更新 STF 拉伸参数 (无需重建网格)

        STF 拉伸在 GPU fragment shader 内执行, 更新 uniform 后
        下一帧自动应用新参数, 无需重新计算顶点颜色或重建网格。

        Args:
            shadows: 暗部裁剪点 (原始像素值)
            highlights: 亮部裁剪点 (原始像素值)
            midtones: 中间调位置 (0-1, 0.5=线性)
        """
        self._stf_shadows = float(shadows)
        self._stf_highlights = float(highlights)
        self._stf_midtones = float(midtones)

        if self._visual is not None:
            self._visual.set_stf(shadows, highlights, midtones)
            logger.info(f"STF 参数已更新 (GPU uniform): "
                        f"shadows={shadows:.4f}, highlights={highlights:.4f}, "
                        f"midtones={midtones:.3f}")
        else:
            logger.warning("STF 参数已缓存, 但 visual 尚未创建")

    def update_lod_level(self, fov_deg: float) -> None:
        """根据视场角选择 LOD 层

        FOV 越小 (放大越多), 使用更高分辨率网格

        Args:
            fov_deg: 当前视场角 (度)
        """
        self._fov_deg = fov_deg
        # FOV > 90° → 粗网格; FOV < 10° → 细网格
        if fov_deg > 90:
            n_lat, n_lon = 48, 96
        elif fov_deg > 30:
            n_lat, n_lon = 96, 192
        elif fov_deg > 10:
            n_lat, n_lon = 128, 256
        else:
            n_lat, n_lon = 192, 384

        logger.info(f"LOD 选择: FOV={fov_deg:.1f}° → 网格 {n_lat}×{n_lon}")
        self._update_mesh(n_lat, n_lon)

    def on_rotate(self, dx: float, dy: float) -> None:
        """拖动旋转

        Args:
            dx: 水平拖动量 (像素)
            dy: 垂直拖动量 (像素)
        """
        if self._camera:
            self._camera.azimuth -= dx * 0.3
            self._camera.elevation -= dy * 0.3
            self._camera.elevation = max(
                -90, min(90, self._camera.elevation))

    def on_zoom(self, delta: float) -> None:
        """滚轮缩放

        Args:
            delta: 滚轮增量 (正=放大, 负=缩小)
        """
        if self._camera:
            factor = 0.9 if delta > 0 else 1.1
            self._camera.distance *= factor
            self._camera.distance = max(
                1.5, min(10.0, self._camera.distance))

    def render(self) -> None:
        """渲染一帧"""
        if self._canvas:
            self._canvas.update()

    @property
    def canvas(self):
        """返回 vispy canvas 对象"""
        return self._canvas


# ============================================================================
# 球面渲染器 (matplotlib fallback)
# ============================================================================

class MatplotlibSphereRenderer:
    """基于 matplotlib 的球面渲染 fallback

    用 Mollweide 投影显示球面数据 (2D)。
    不支持 3D 旋转, 但支持基本的缩放和平移。
    """

    def __init__(self, figsize: Tuple[float, float] = (10, 6)):
        """初始化 matplotlib 球面渲染器

        Args:
            figsize: 图像大小 (宽, 高)
        """
        import matplotlib
        matplotlib.use("Qt5Agg")  # 确保 Qt 后端
        import matplotlib.pyplot as plt

        self._fig, self._ax = plt.subplots(figsize=figsize)
        self._fig.patch.set_facecolor("black")
        self._ax.set_facecolor("black")
        self._nside = 512
        self._nested = True
        self._pixel_map = None
        self._data_values = None
        self._image = None
        self._fov_deg = 180.0

        logger.info("MatplotlibSphereRenderer 初始化完成 (Mollweide 投影)")

    def set_data(self, pixels: np.ndarray, values: np.ndarray,
                 nside: int, nested: bool = True) -> None:
        """设置球面数据"""
        self._nside = nside
        self._nested = nested
        self._pixel_map = {}
        for pix, val in zip(pixels, values):
            self._pixel_map[int(pix)] = float(val)
        self._data_values = np.array(values, dtype=np.float64)
        logger.info(f"设置球面数据: nside={nside}, {len(pixels)} 像素")
        self._render_mollweide()

    def _render_mollweide(self, resolution: int = 400) -> None:
        """用 Mollweide 投影渲染球面数据

        Args:
            resolution: 投影图像分辨率 (像素)
        """
        self._ax.clear()
        self._ax.set_facecolor("black")

        if not self._pixel_map or len(self._pixel_map) == 0:
            self._ax.text(0, 0, "无数据", color="white",
                          ha="center", va="center", fontsize=14)
            self._ax.set_xticks([])
            self._ax.set_yticks([])
            self._fig.canvas.draw_idle()
            return

        # 生成 Mollweide 投影网格
        # l: 经度 [-π, π], b: 纬度 [-π/2, π/2]
        l = np.linspace(-np.pi, np.pi, resolution)
        b = np.linspace(-np.pi / 2, np.pi / 2, resolution // 2)
        L, B = np.meshgrid(l, b)

        # (l, b) → (ra_deg, dec_deg) → HEALpix 像素 → 值
        ra_deg = (np.degrees(L) + 360) % 360
        dec_deg = np.degrees(B)

        # 向量化查值
        vals = np.full_like(ra_deg, np.nan, dtype=np.float64)
        for i in range(resolution // 2):
            for j in range(resolution):
                try:
                    ipix = _radec2pix(self._nside, self._nested,
                                      float(ra_deg[i, j]), float(dec_deg[i, j]))
                    vals[i, j] = self._pixel_map.get(ipix, np.nan)
                except Exception:
                    pass

        # 归一化
        valid = vals[np.isfinite(vals)]
        if valid.size > 0:
            vmin = float(np.percentile(valid, 1))
            vmax = float(np.percentile(valid, 99))
            if vmax - vmin < 1e-30:
                vmax = vmin + 1.0
            norm_vals = np.clip((vals - vmin) / (vmax - vmin), 0, 1)
        else:
            norm_vals = np.zeros_like(vals)

        # 渲染
        self._image = self._ax.imshow(
            norm_vals, extent=[-np.pi, np.pi, -np.pi / 2, np.pi / 2],
            aspect="auto", origin="lower",
            cmap="inferno", vmin=0, vmax=1)

        # 坐标轴标签
        self._ax.set_xlabel("RA (经度)", color="white")
        self._ax.set_ylabel("Dec (纬度)", color="white")
        self._ax.tick_params(colors="white")

        self._fig.canvas.draw_idle()
        logger.info(f"Mollweide 投影渲染完成: {resolution}×{resolution//2}")

    def update_lod_level(self, fov_deg: float) -> None:
        """根据 FOV 调整投影分辨率"""
        self._fov_deg = fov_deg
        if fov_deg > 60:
            resolution = 300
        elif fov_deg > 20:
            resolution = 500
        else:
            resolution = 800
        logger.info(f"LOD: FOV={fov_deg:.1f}° → 分辨率={resolution}")
        self._render_mollweide(resolution)

    def on_rotate(self, dx: float, dy: float) -> None:
        """matplotlib fallback 不支持 3D 旋转"""
        pass

    def on_zoom(self, delta: float) -> None:
        """matplotlib fallback 缩放 (通过 set_xlim/ylim)"""
        xlim = self._ax.get_xlim()
        ylim = self._ax.get_ylim()
        factor = 0.9 if delta > 0 else 1.1
        cx = (xlim[0] + xlim[1]) / 2
        cy = (ylim[0] + ylim[1]) / 2
        half_w = (xlim[1] - xlim[0]) / 2 * factor
        half_h = (ylim[1] - ylim[0]) / 2 * factor
        self._ax.set_xlim(cx - half_w, cx + half_w)
        self._ax.set_ylim(cy - half_h, cy + half_h)
        self._fig.canvas.draw_idle()

    def render(self) -> None:
        """渲染一帧"""
        self._fig.canvas.draw_idle()

    @property
    def figure(self):
        """返回 matplotlib figure"""
        return self._fig

    @property
    def canvas(self):
        """返回 matplotlib canvas"""
        return self._fig.canvas


# ============================================================================
# 统一接口: 自动选择渲染器
# ============================================================================

class SphereRenderer:
    """球面渲染器统一接口

    自动选择 vispy (优先) 或 matplotlib (fallback)。
    """

    def __init__(self, canvas=None):
        """初始化球面渲染器

        Args:
            canvas: 可选的 canvas 对象 (vispy SceneCanvas)
        """
        self._renderer = None
        self._backend = "none"

        if VISPY_AVAILABLE:
            try:
                self._renderer = VisPySphereRenderer(canvas)
                self._backend = "vispy"
                logger.info("使用 vispy 后端渲染球面")
            except Exception as e:
                logger.warning(f"vispy 渲染器初始化失败: {e}")
                self._renderer = None

        if self._renderer is None:
            self._renderer = MatplotlibSphereRenderer()
            self._backend = "matplotlib"
            logger.info("使用 matplotlib 后端渲染球面")

    @property
    def backend(self) -> str:
        """当前渲染后端名称 ("vispy" / "matplotlib")"""
        return self._backend

    def set_data(self, pixels: np.ndarray, values: np.ndarray,
                 nside: int, nested: bool = True) -> None:
        """设置球面数据"""
        self._renderer.set_data(pixels, values, nside, nested)

    def set_stf(self, shadows: float, highlights: float,
                midtones: float) -> None:
        """实时更新 STF 拉伸参数

        vispy 后端: 更新 GPU uniform, 无需重建网格
        matplotlib 后端: 若支持则更新, 否则忽略

        Args:
            shadows: 暗部裁剪点 (原始像素值)
            highlights: 亮部裁剪点 (原始像素值)
            midtones: 中间调位置 (0-1, 0.5=线性)
        """
        if hasattr(self._renderer, 'set_stf'):
            self._renderer.set_stf(shadows, highlights, midtones)
        else:
            logger.debug(f"当前后端 {self._backend} 不支持 set_stf, 忽略")

    def update_lod_level(self, fov_deg: float) -> None:
        """根据 FOV 选择 LOD"""
        self._renderer.update_lod_level(fov_deg)

    def on_rotate(self, dx: float, dy: float) -> None:
        """拖动旋转"""
        self._renderer.on_rotate(dx, dy)

    def on_zoom(self, delta: float) -> None:
        """滚轮缩放"""
        self._renderer.on_zoom(delta)

    def render(self) -> None:
        """渲染一帧"""
        self._renderer.render()

    @property
    def canvas(self):
        """返回底层 canvas"""
        return self._renderer.canvas

    @property
    def native_widget(self):
        """返回可嵌入 Qt 的原生 widget

        vispy: 返回 canvas.native (QWidget)
        matplotlib: 返回 FigureCanvas (QWidget)
        """
        if self._backend == "vispy":
            return self._renderer.canvas.native
        elif self._backend == "matplotlib":
            return self._renderer.canvas
        return None
