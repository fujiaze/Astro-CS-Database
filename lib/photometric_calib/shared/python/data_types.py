"""
公共数据结构
功能: 定义光谱积分器和梯度估算器共享的数据结构
用途: 为 photometric_calib 双程序提供统一的输入/输出数据载体，减少重复定义
依赖: dataclasses, numpy
"""

from dataclasses import dataclass, field
import numpy as np

@dataclass
class GaiaSpectrumStarPy:
    ra: float = 0.0
    dec: float = 0.0
    mag_g: float = 0.0
    mag_bp: float = 0.0
    mag_rp: float = 0.0
    source_id: int = 0
    spectrum: np.ndarray = None  # uint8[343]

@dataclass
class FSynResult:
    source_id: int = 0
    ra: float = 0.0
    dec: float = 0.0
    mag_g: float = 0.0
    f_syn: float = 0.0
