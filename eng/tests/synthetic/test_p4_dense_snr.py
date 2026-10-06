#!/usr/bin/env python3
"""P4 重建稠密信噪比 —— 合成全链层的重建算子不变量。

**创新点**：`AGENTS.md` §10「P4 重建稠密信噪比 | 据噪声信号模型把稀疏控制点重建为稠密信噪比场，
现场按像素求值」。P2 产出稀疏控制点，P4 消费它们并把场求值到像素。

**为什么这一层是空白**（只读审计结论）：`grep -rln 'seam|rel_step|天光'` 在
unit/module/integration 三层只命中三个**辅助模块**，**无一条判据**；三层没有任何一条判
「稀疏 → 稠密重建」。`SparseSnrReconstructor` 四算子的数值复现在集成层被明确列入
「需要构建产品才能跑、本层不含」的登记项。⇒ 本文件是该覆盖的第一批。

## 0 三条硬约束在本文件里的落法

### 0.1 预期值不来自被测实现自身（`05` §1）

本文件的**参照量**只用四类，每条判据在 `source` 里写明属于哪一类：

| 类别 | 本文件的用法 |
|---|---|
| **闭式解析** | 控制点自身复现、双线性/样条之差、cell 中心约定的 31.5 px 平移、非负公理 |
| **第三方独立实现** | `scipy.interpolate.CubicSpline(bc_type='natural')` 逐维可分离施加 ⇒ 样条的独立 oracle |
| **定种子蒙特卡洛** | `Var(median)` 与 `κ(N)`（P4-j）——`PHASE2_SAMPLER.md` F3(b) 明写「统计判据必须走 MC」 |
| **非插值型对照臂** | 二次最小二乘拟合面：给 P4-a 判别力（见下） |

`_kit.py` 是**数据构造器**不是 oracle，本文件不把它当唯一预期值的来源。

### 0.2 ⚠ 本文件**不写**的三类判据（三条最硬的禁令）

1. **跨帧平方和恒等式一律不写。** `NOISE_SNR.md:353-359` 逐字
   `Σ_k SNR_k² = F0²/Var(F̂)` 是**定义式恒等式**，「对任何可达输入恒成立，**结构上不可能
   给出判决**」。本文件没有任何一条用例断言它。
2. **`w ≡ 1/σ_F²` 不自证。** `weight_chain.h:435-438` 逐字「`σ_F := F_ref/SNR_layer`
   是把被检验公式**取逆**得来的，用它当参照量只回到同一个式子」。P4-h 只走正本给的
   **两条**路线之一（独立双线性 oracle 路线），并把被禁的那条写成**负例**。
3. **权重效率 `E` 不作判据。** `NOISE_SNR.md:484-492` 逐字「`E` 对整体乘性偏差完全免疫
   …… 报 `E` 必须同时报水平偏差」；且用 `1/σ²` 当最优权重时 `E ≡ 0` 是 Cauchy–Schwarz
   取等。本文件**不用 E**，用的是有绝对标尺的偏差/偏差符号。

### 0.3 ⚠ 裁-27 未裁决矛盾：本文件怎么处理的

`docs/engineering/governance/UNRESOLVED.md:47`「裁-27」：`ACSD_DESIGN.md`
「P4 重建稠密信噪比」写「重建量**随源亮度变化**」，而「信噪比重建与逆方差叠加」写
「三者都产出**同一物理量** `SNR = F_ref/σ_F` 的稠密表示」。若重建量随源亮度变，则
`w = SNR²/F_ref² = 1/σ_F²` 的**相消前提不成立**。

⇒ **本文件的处置**（执行车道不裁决，如实隔离）：

* **P4-a…P4-g、P4-i、P4-j 全部写成纯重建算子性质**，判据只用「层值 ↔ 层值」「场 ↔ 场」
  这类**不含 `w`、不含 `F_ref` 相消**的关系。裁-27 无论裁哪一侧，这九条判据都成立。
* **P4-h 单列**，用例 id 带 `_pending_cai27` 后缀，`intent` / `source` 里显式写明
  **前提未定**，并在断言旁注明「本条不构成对 `w ≡ 1/σ_F²` 的裁决」。
* P4-h 的实测读数照常落盘（它是可复算的），但**不据此改任何判据面**。

## 1 被测算子的口径（逐字取自冻结源）

`lib/algorithms/integration/phase2_integrate/include/acsd/weight_chain.h` 算子词表：

| token | 语义 |
|---|---|
| `natural_bicubic_spline_clip_v1` | 可分离自然边界双三次样条 + 钳到有效控制值值域 |
| `natural_bicubic_spline_clip_mesh_median_v1` | 同上，且样条前插入 3×3 mesh 中值（边界 replicate） |
| `bilinear_regular_grid_v1` | 规则网格双线性（对照/回退；无钳制、无滤波） |
| `nearest_control_point_v1` | 最近控制点（散点模式唯一合法算子） |

几何面逐字：「**节点落在所属 cell 的中心**……其中心 = `grid_origin_x + i*dx + (dx−1)/2`
⇒ **必须 `x0 = grid_origin_x + (dx−1)/2`**（y 同）。把节点当 cell 角点（`x0 = grid_origin_x`）
会使整场平移半个 cell（Δ=64 时 **31.5 px**）。」
定义域逐字：「`[grid_origin_x − 0.5, grid_origin_x + nx·dx − 0.5]`……越出该并集 →
fail-closed（不外推、不回退帧级）。」

## 2 判据恒真清单（如实登记，含本文件**判定恒真因而没写成用例**的）

| 候选判据 | 判定 | 依据 |
|---|---|---|
| 「双线性时控制点自复现显著非零」 | ❌ **恒假，不是判据** | 双线性是**插值型**算子，在规则网格上对节点**逐位精确复现**（实测 0.0）。派单稿的这条说法在数学上不成立，本文件改为「插值型 vs 拟合型」对照（见 P4-a 负例） |
| 「单调剖面上 `spline ≥ bilinear`」 | ❌ **恒假** | 自然三次样条在单调数据上**会下冲**。实测 `min(spline−bilinear) < 0`，且样条本身非单调。本文件改为可判的「样条不越出数据值域」 |
| 常量场上比较两个算子 | ❌ 恒真（各算子同值） | P4-b 负例即此 |
| 单域判 mesh 中值好坏 | ❌ 恒真 | 必须两域都跑；P4-e 负例即此 |
| 跨帧 `Σ SNR_k² = F0²/Var(F̂)` | ❌ 恒真（定义式） | 黑名单，本文件不写 |
| `E = Var_w/Var_opt − 1` | ❌ 对整体乘性偏差免疫 | `NOISE_SNR.md:484-492`；本文件不用 |
| 「两条**不同**算子（spline_clip vs 双线性）的逐像素 `w` 相等」 | ❌ **构造上不成立**（恒红侧） | 两条算子的层值本就不同（实测最大相对差 17.4%、本文件夹具上 0.069%）。拿它们做「一致性」pass/fail 对照只能崩（`np.max` 空集）或恒红 ⇒ P4-h 把它**降为诊断项**，只记录差值、不判红 |
| 「oracle 只要换个 `operator=` 字符串就算独立」 | ❌ **伪独立**（D 类） | 对抗复核判定的缺陷 1：旧版 P4-h 的 oracle 与主实现共用同一个 `Reconstructor` 类、比较面只取 232/841 个**已逐位相等**的像元、断言是 `H.exact(...)` ⇒ 必然恒真（余量 `0×`）。整改见 P4-h 的两条独立代码路径 + 全域比较 |
| 「P4-a 的 scipy oracle 对拍证明了节点间重建正确」 | ⚠ **覆盖面缺口**（非恒真） | 那次对拍只在**控制节点坐标**上采样（`dx = 1` ⇒ 查询点恰是节点），任何插值器在节点上都等于输入值 ⇒ 只能证明「节点自复现」。实测把同一 oracle 移到非节点坐标上，主实现与 scipy 仍一致到 2.2e-16 ⇒ **主实现的样条是对的**，登记的是判据覆盖面 |

## 3 容差

全部取自 `eng/tests/synthetic/_tol_chain_b.py`（本线**新增**的冻结表；
`tolerances.py` 的 `synth.p4.dense_snr_rel = 2.5e-1` 是骨架留的**量级冻结占位**，
本文件的判据没有一条用它——原因见该占位 `note`：它判的是「重建场对逐像素真值的相对偏差」，
需要 `_kit` 的 HST 模板链路；本文件判的是**算子代数性质**，参照量是闭式与 scipy，
量级从 `1e-13`（复现）到 `1e-1`（算子分离度）跨 12 个数量级，占位值覆盖不了。）
"""

from __future__ import annotations

import ast
import inspect
import math
import os
import sys
import textwrap

import numpy as np

_REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))
if _REPO_ROOT not in sys.path:
    sys.path.insert(0, _REPO_ROOT)

from eng.tests.synthetic import _tol_chain_b as TB          # noqa: E402
from eng.tests.synthetic import tolerances as _base_tol     # noqa: E402
from eng.tests.unit import harness as H                     # noqa: E402

# scipy 是**第三方独立实现**，用作样条的独立 oracle（§0.1 表格第 2 行）。
try:
    from scipy.interpolate import CubicSpline as _CubicSpline
except ImportError:  # pragma: no cover - 显式失败面，不静默跳过
    raise ImportError(
        "P4 合成全链需要 scipy.interpolate.CubicSpline 作独立 oracle（§0.1）。"
        "缺它就没有「第三方独立实现」这一类参照量 ⇒ 宁可整条线不执行，"
        "也不许把被测实现自己的输出当期望值（`05` §1「不提供意图与来源的用例不得进本测试集」）。"
    ) from None


# ===========================================================================
# §A 被测实现：冻结算子的参照实现（逐字照 weight_chain.h 的词表与几何面）
# ===========================================================================

OP_SPLINE_CLIP = "natural_bicubic_spline_clip_v1"
OP_SPLINE_CLIP_MESH_MEDIAN = "natural_bicubic_spline_clip_mesh_median_v1"
OP_BILINEAR = "bilinear_regular_grid_v1"

#: 算子 → (是否 mesh 中值前置, 是否值域钳制)。逐字照词表：
#: `sparse_recon_operator_uses_mesh_median` / `sparse_recon_operator_clips_to_ctrl_range`。
OP_TABLE = {
    OP_SPLINE_CLIP: (False, True),
    OP_SPLINE_CLIP_MESH_MEDIAN: (True, True),
    OP_BILINEAR: (False, False),
}


class ReconstructError(RuntimeError):
    """fail-closed 面的显式失败（`weight_chain.h` 的 `err` 出参语义）。"""


def natural_second_derivatives(v: np.ndarray) -> np.ndarray:
    """自然边界样条的二阶导，单位节点间距。

    直接从定义写出三对角系统 `M_{k-1} + 4 M_k + M_{k+1} = 6(v_{k-1} − 2 v_k + v_{k+1})`，
    首末行 `M_0 = M_{n−1} = 0`（自然边界）。**不用** `scipy`——scipy 是本文件的
    oracle，被测实现必须独立。
    """
    n = len(v)
    m = np.zeros(n)
    if n < 3:
        return m
    a = np.zeros((n, n))
    a[0, 0] = a[-1, -1] = 1.0
    for k in range(1, n - 1):
        a[k, k - 1] = 1.0
        a[k, k] = 4.0
        a[k, k + 1] = 1.0
    rhs = np.zeros(n)
    rhs[1:-1] = 6.0 * (v[:-2] - 2.0 * v[1:-1] + v[2:])
    return np.linalg.solve(a, rhs)


def _hermite(vlo, vhi, mlo, mhi, h):
    return float(vlo * (1 - h) + vhi * h
                 + ((1 - h) ** 3 - (1 - h)) * mlo / 6.0
                 + (h ** 3 - h) * mhi / 6.0)


class Reconstructor:
    """稀疏帧内层的重建器（`SparseSnrReconstructor` 的参照实现）。

    口径逐字取自 `weight_chain.h`：
      * 规则网格，节点落在 **cell 中心**：`x0 = grid_origin + (dx − 1)/2`；
      * 查询坐标 = **像素中心坐标**（像素序号 p ↔ 坐标 p）；
      * 定义域 = 层覆盖的 cell 并集 `[grid_origin − 0.5, grid_origin + nx·dx − 0.5]`；
        最外半个 cell 由端点节点常数延拓；越界 ⇒ `out_of_domain`，**不回退帧级标量**。
    """

    def __init__(self, values: np.ndarray, *, dx: int = 1, dy: int | None = None,
                 grid_origin_x: float = 0.0, grid_origin_y: float = 0.0,
                 operator: str = OP_SPLINE_CLIP,
                 corner_convention: bool = False,
                 apply_clip: bool | None = None) -> None:
        self.values = np.asarray(values, dtype=float)
        self.ny, self.nx = self.values.shape
        self.dx = float(dx)
        self.dy = float(dx if dy is None else dy)
        self.gox = float(grid_origin_x)
        self.goy = float(grid_origin_y)
        self.operator = operator
        if operator not in OP_TABLE:
            raise ReconstructError(f"未识别算子 token={operator!r} ⇒ fail-closed，不得回退默认")
        self.mesh_median_applied, value_range_clipped = OP_TABLE[operator]
        # `apply_clip=None` ⇒ 照算子词表。显式给 False 只在**读无钳制对照**时用：
        # 正本 `weight_chain.h:120-123` 逐字「值域钳制**不是可选项**」，所以生产不存在
        # 关掉钳制的算子；这条 override 是量测面，不是可用配置。
        self.value_range_clipped = value_range_clipped if apply_clip is None else apply_clip
        # 角点约定是**注入的缺陷**，不是可选项；默认走冻结的 cell 中心约定。
        self.x0 = self.gox if corner_convention else self.gox + (self.dx - 1.0) / 2.0
        self.y0 = self.goy if corner_convention else self.goy + (self.dy - 1.0) / 2.0
        # 前置滤波（词表把它编进算子标识，不是独立开关）
        base = self.values
        if self.mesh_median_applied:
            base = mesh_median_3x3(base)
        # ⚠ **钳制的作用面**：`weight_chain.h` 词表逐字写的是「钳到**有效控制值值域**」——
        # 那是**区间**的定义（端点 = 有效控制值的 min/max），不是「只钳控制值」的指令。
        # 正本 `NOISE_SNR.md:503` 逐字「**信噪比场值域非负是重建算子的公理的一部分**：
        # 负信噪比不可发布」⇒ 钳制必须作用在**求值输出**上，否则插值器在节点间的
        # 过冲/下冲会让发布的场跑出值域（实测对抗构型上节点间下冲到 −0.025）。
        self.clip_low = float(np.min(base))
        self.clip_high = float(np.max(base))
        self._grid = base
        # 预置（二阶导、插值所需的一切）—— `prepare` 语义
        self._mx = [natural_second_derivatives(row) for row in self._grid]
        self._my = [natural_second_derivatives(self._grid[:, i]) for i in range(self.nx)]

    # -- 几何门 ------------------------------------------------------------
    @property
    def domain(self):
        return (self.gox - 0.5, self.gox + self.nx * self.dx - 0.5,
                self.goy - 0.5, self.goy + self.ny * self.dy - 0.5)

    def in_domain(self, x: float, y: float) -> bool:
        xlo, xhi, ylo, yhi = self.domain
        return (xlo <= x <= xhi) and (ylo <= y <= yhi)

    def node_pixel(self, i: int, j: int):
        """第 (i,j) 个控制节点所在的**像素中心坐标**。"""
        return (self.x0 + i * self.dx, self.y0 + j * self.dy)

    def cell_center_offset_max_abs(self) -> float:
        """节点相对 cell 中心的最大偏移 [px]；正确约定下应 ~0。"""
        worst = 0.0
        for j in range(self.ny):
            for i in range(self.nx):
                cx = self.gox + i * self.dx + (self.dx - 1.0) / 2.0
                cy = self.goy + j * self.dy + (self.dy - 1.0) / 2.0
                px, py = self.node_pixel(i, j)
                worst = max(worst, abs(px - cx), abs(py - cy))
        return worst

    # -- 求值 --------------------------------------------------------------
    def eval(self, x: float, y: float) -> float:
        """在像素中心坐标 (x,y) 求值。越出定义域 ⇒ `ReconstructError`（fail-closed）。"""
        if not self.in_domain(x, y):
            raise ReconstructError(
                f"查询点 ({x}, {y}) 越出层定义域 {self.domain} ⇒ out_of_domain；"
                "不外推、不回退帧级标量")
        u = (x - self.x0) / self.dx
        v = (y - self.y0) / self.dy
        out = self._bilinear(u, v) if self.operator == OP_BILINEAR else self._spline(u, v)
        if self.value_range_clipped:
            out = min(max(out, self.clip_low), self.clip_high)
        return out

    def _bilinear(self, u, v):
        i = min(max(int(math.floor(u)), 0), self.nx - 2)
        j = min(max(int(math.floor(v)), 0), self.ny - 2)
        tu = min(max(u - i, 0.0), 1.0)
        tv = min(max(v - j, 0.0), 1.0)
        g = self._grid
        return float(g[j, i] * (1 - tu) * (1 - tv) + g[j, i + 1] * tu * (1 - tv)
                     + g[j + 1, i] * (1 - tu) * tv + g[j + 1, i + 1] * tu * tv)

    def _spline(self, u, v):
        i = min(max(int(math.floor(u)), 0), self.nx - 2)
        j = min(max(int(math.floor(v)), 0), self.ny - 2)
        tu = min(max(u - i, 0.0), 1.0)
        tv = min(max(v - j, 0.0), 1.0)
        col = [_hermite(self._grid[jj, i], self._grid[jj, i + 1],
                        self._mx[jj][i], self._mx[jj][i + 1], tu)
               for jj in range(self.ny)]
        return _hermite(col[j], col[j + 1], self._my[i][j], self._my[i][j + 1], tv)

    def field(self, xs, ys) -> np.ndarray:
        """把整片区域求值成场（生产「现场按像素求值」的语义）。

        `xs` / `ys` 是**像素中心坐标序列**（由调用方给，避免 `arange` 的浮点累积
        把最后一个采样点顶出定义域——那会触发 fail-closed，把夹具错误误报成实现缺陷）。
        """
        xs = np.asarray(xs, dtype=float)
        ys = np.asarray(ys, dtype=float)
        return np.array([[self.eval(float(x), float(y)) for x in xs] for y in ys])

    def axis(self, lo: float, hi: float, step: float = 1.0) -> np.ndarray:
        """域内均匀采样轴（末点严格不超过 `hi`）。"""
        n = int(math.floor((hi - lo) / step)) + 1
        a = lo + step * np.arange(n, dtype=float)
        return a[a <= hi + 1e-12]

    def node_reproduction_max_abs(self) -> float:
        """控制点自身复现最大绝对残差（`SparseReconstruction.node_reproduction_max_abs`）。"""
        worst = 0.0
        for j in range(self.ny):
            for i in range(self.nx):
                px, py = self.node_pixel(i, j)
                worst = max(worst, abs(self.eval(px, py) - self.values[j, i]))
        return worst


def mesh_median_3x3(grid: np.ndarray) -> np.ndarray:
    """3×3 mesh 中值前置滤波，边界 replicate（`weight_chain.h` 词表逐字）。"""
    ny, nx = grid.shape
    pad = np.pad(grid, 1, mode="edge")
    out = np.empty_like(grid)
    for j in range(ny):
        for i in range(nx):
            out[j, i] = float(np.median(pad[j:j + 3, i:i + 3]))
    return out


# -- 独立 oracle -----------------------------------------------------------

def scipy_spline_2d(grid: np.ndarray, u, v) -> np.ndarray:
    """`scipy.interpolate.CubicSpline(bc_type='natural')` 逐维可分离施加。

    这是 P4 样条的**第三方独立 oracle**：被测实现自己解三对角系统，oracle 走
    scipy 的实现路径，两者不共用代码（§0.1 表格第 2 行）。
    """
    ny, nx = grid.shape
    ub = np.broadcast_arrays(np.asarray(u, float), np.asarray(v, float))
    us, vs = ub
    cs_x = [_CubicSpline(np.arange(nx), grid[j], bc_type="natural") for j in range(ny)]
    usamp = np.array([cs_x[j](us.ravel()) for j in range(ny)])      # [ny, M]
    cs_y = [_CubicSpline(np.arange(ny), usamp[:, m], bc_type="natural") for m in range(us.size)]
    out = np.array([cs_y[m](vs.ravel()[m]) for m in range(us.size)])
    return out.reshape(us.shape)


def lsq_quadratic_fit(grid: np.ndarray) -> np.ndarray:
    """总次数 2 的最小二乘拟合面 —— **非插值型**对照臂（给 P4-a 判别力）。"""
    ny, nx = grid.shape
    j, i = np.meshgrid(np.arange(ny, dtype=float), np.arange(nx, dtype=float), indexing="ij")
    u, v = i / (nx - 1), j / (ny - 1)
    a = np.stack([np.ones(u.size), u.ravel(), v.ravel(), (u * v).ravel(),
                  (u ** 2).ravel(), (v ** 2).ravel()], axis=1)
    coef, *_ = np.linalg.lstsq(a, grid.ravel(), rcond=None)
    return (a @ coef).reshape(ny, nx)


# -- 控制点方差（`PHASE2_UPM.md` §5 / `NOISE_SNR.md` §「控制点估计量的方差」）--

MAD_K = 1.482602218505602          # Rousseeuw & Croux 1993, JASA 88, 1273（正本转引）


def control_variance(sigma_bg_raw: float, n_retained: int, k_corr: float):
    """`control_variance = k_corr·(π/2)·σ_bg² / N_retained`，`control_ivar = 1/variance`。

    ⚠ `σ_bg_raw = 0` 分支逐字照 `PHASE2_UPM.md` §5：「`σ_bg_raw = 0`（patch 内 ≥ 半数
    像素同值）⇒ 无尺度信息：control_ivar 必须为 0，**禁止以数值保护量生成有限方差发布**」。
    ⇒ 该分支返回 `(ivar = 0.0, var = inf)`——**不是** `1/tiny`。
    """
    if not (k_corr > 1.0):
        raise ReconstructError(f"k_corr={k_corr!r} 不在定义域 1 < k_corr（§4 显式拒）")
    if sigma_bg_raw == 0.0:
        return 0.0, math.inf
    var = k_corr * (math.pi / 2.0) * sigma_bg_raw ** 2 / n_retained
    return 1.0 / var, var


def control_variance_protected(sigma_bg_raw: float, n_retained: int, k_corr: float):
    """**注入的缺陷实现**：用数值保护量替零尺度分支生成有限方差。

    逐字违反 `PHASE2_UPM.md` §5「禁止以数值保护量生成有限方差发布」与
    `PHASE2_SAMPLER.md` F5(b)「σ_bg_raw=0 时断言 `control_ivar == 0` 且
    control_variance 非有限」。只在 P4-i 的负例里用。
    """
    eps = 1.0e-300
    s2 = sigma_bg_raw ** 2 if sigma_bg_raw > 0.0 else eps
    var = k_corr * (math.pi / 2.0) * s2 / n_retained
    return 1.0 / var, var


# ===========================================================================
# §B 冻结夹具（字面常量；不是从任何实现输出反推的）
# ===========================================================================

#: 控制点值取 **binary64 不可精确表示**的值（1/3、5/3、101/17）——
#: 否则往返残差恒为 0 ⇒ 门限不触发 ⇒ 容差判据退化成装饰（派单稿 §3.1 型 3）。
CTRL_PROFILE = np.array([1.0 / 3.0, 5.0 / 3.0, 101.0 / 17.0, 7.0,
                         101.0 / 17.0, 2.0, 8.0, 3.0])

#: 单调剖面（P4-b 的真实性质用）。同样含 1/3、101/17。
CTRL_MONOTONE = np.array([1.0 / 3.0, 1.0, 2.0, 101.0 / 17.0, 5.0, 6.0, 7.5, 9.0])

def ctrl_ramp_with_edge(n=8):
    """**P4-a 对照臂的判别构型**：天光斜坡 + 星系边缘阶跃。

    ```text
    C(u, v) = 1/3 + 2u + 1.5v + 6·[u > 1/2] + 4·[v > 1/2]
    ```

    逐字取值理由：`1/3` 是 binary64 不可精确表示的值（纪律二第 2 条）；
    斜坡项落在二次基张成空间内、阶跃项**不在** ⇒ 总次数 2 的最小二乘拟合面
    在这个构型上给出的是**真实的 O(1) 偏差**而不是舍入。
    阶跃代表稀疏信噪比层在星系边缘上的真实结构（HST/地面帧常见），不是人为构造。
    旧夹具 `10 + 6uv + 3u³` 的 `6uv` 项同样在张成空间内、且把跨度从 3 抬到 9，
    把 3u³ 的残差稀释到门限的 1.02 倍（余量 2%）⇒ 本构型替换它。
    """
    j, i = np.meshgrid(np.linspace(0, 1, n), np.linspace(0, 1, n), indexing="ij")
    return (1.0 / 3.0 + 2.0 * i + 1.5 * j
            + 6.0 * (i > 0.5) + 4.0 * (j > 0.5))


#: 非共线控制面（含二次项 ⇒ 样条与双线性**不同构**）。
#: ⚠ 常量场/共线场上两个算子同值 ⇒ 判据恒真，P4-b 负例即此。
def ctrl_curved(n=8):
    j, i = np.meshgrid(np.linspace(0, 1, n), np.linspace(0, 1, n), indexing="ij")
    return 10.0 + 5.0 * j + 3.0 * i + 2.0 * j * i + 1.5 * i ** 2


#: 对抗构型（`weight_chain.h:120` 实测去掉钳制后 E 达 2.48e4、给出**负的 σ**）：
#: 角上四组高值、内区低值 ⇒ 曲率剧烈 ⇒ 无钳制样条在胞内显著为负。
CTRL_ADVERSARIAL = np.array([
    [3.0, 9.0, 1.0, 1.0, 1.0, 1.0, 9.0, 3.0],
    [3.0, 9.0, 1.0, 1.0, 1.0, 1.0, 9.0, 3.0],
    [1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0],
    [1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0],
    [1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0],
    [1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0],
    [3.0, 9.0, 1.0, 1.0, 1.0, 1.0, 9.0, 3.0],
    [3.0, 9.0, 1.0, 1.0, 1.0, 1.0, 9.0, 3.0],
])

#: P4-d：胞内含源。像素域 64×64、cell 宽 8 px ⇒ 8×8 个控制点，
#: cell (3,3) 覆盖像素 24..31，源心落在像素 28.5（**严格在胞内、不在任何控制点上**）。
PX, CELL = 64, 8
SRC_LEVEL = 2.0
SRC_FLUX = 400.0
SRC_SIGMA_PX = 2.55


def source_scene():
    """真值场 = 均匀底 + 胞内高斯点源（PSF 核按构造归一，闭式）。"""
    yy, xx = np.mgrid[0:PX, 0:PX]
    k = np.exp(-0.5 * (((xx - 28.5) ** 2 + (yy - 28.5) ** 2) / SRC_SIGMA_PX ** 2))
    k = k / k.sum()
    return SRC_LEVEL + SRC_FLUX * k


def control_from_source(median: bool = True):
    """从真值场在 8×8 控制格上取 cell 内中位数（`control estimator = patch median`，
    `PHASE2_UPM.md` §5 逐字）。"""
    truth = source_scene()
    out = np.full((PX // CELL, PX // CELL), SRC_LEVEL)
    for jj in range(PX // CELL):
        for ii in range(PX // CELL):
            blk = truth[jj * CELL:(jj + 1) * CELL, ii * CELL:(ii + 1) * CELL]
            out[jj, ii] = float(np.median(blk)) if median else float(blk.mean())
    return out


# ===========================================================================
# §C P4-a 控制点自复现
# ===========================================================================


@H.test(
    "p4a_node_reproduction_interpolants",
    intent="两个冻结插值型算子在控制点位置复现输入值（node_reproduction_max_abs ≈ 0）；"
           "并与 scipy 独立 oracle 对拍",
    inputs="8×8 规则网格，控制值 [1/3, 5/3, 101/17, 7, 101/17, 2, 8, 3]（含 binary64 不可精确"
           "表示的值）；node_reproduction 在**冻结的 cell 中心坐标**上逐点复现",
    expected="两个算子的 node_reproduction_max_abs ≤ 1.0e-13（冻结容差 "
             "synth.p4.node_repro_abs）；且与 scipy CubicSpline(natural) 的独立 oracle "
             "逐点一致到 f64 非归约档",
    source="闭式解析（插值型算子的定义）+ 第三方独立实现（scipy.interpolate.CubicSpline, "
           "bc_type='natural'）；`weight_chain.h` 几何面逐字「节点落在所属 cell 的中心」；"
           "正本 `SparseReconstruction.node_reproduction_max_abs` 注释逐字「应 ~0」",
)
def p4a_node_reproduction_interpolants():
    grid = np.tile(CTRL_PROFILE, (8, 1))
    with H.evidence() as ev:
        for op in (OP_SPLINE_CLIP, OP_BILINEAR):
            rc = Reconstructor(grid, operator=op)
            repro = rc.node_reproduction_max_abs()
            tol = TB.get("synth.p4.node_repro_abs").value
            ev.record(f"{op}.node_reproduction_max_abs", repro, tol, "",
                      "cell 中心约定，dx=1")
            H.less_equal(repro, tol, f"{op}: 控制点自复现残差")
            # cell 中心门：节点相对 cell 中心的偏移应逐位 0
            off = rc.cell_center_offset_max_abs()
            H.less_equal(off, 0.0, f"{op}: cell 中心偏移")
            # 独立 oracle 对拍（只在样条臂；双线性是闭式，不需要 oracle）
            if op == OP_SPLINE_CLIP:
                js, is_ = np.meshgrid(np.arange(8.0), np.arange(8.0), indexing="ij")
                mine = rc.field(rc.axis(0.0, 7.0, 1.0), rc.axis(0.0, 7.0, 1.0))
                orc = scipy_spline_2d(grid, is_, js)
                d = float(np.max(np.abs(mine - orc)))
                ev.record("spline_vs_scipy_oracle.max_abs", d,
                          _base_tol.F64_RTOL * float(np.max(np.abs(grid))), "",
                          "第三方独立实现对拍")
                H.less_equal(d, _base_tol.F64_RTOL * float(np.max(np.abs(grid))),
                             "样条 vs scipy CubicSpline(natural) 可分离 oracle")


@H.test(
    "p4a_negative_fit_arm_is_not_reproduced",
    intent="证明「node_reproduction ≈ 0」不是恒真：把插值型求值面换成**拟合型**"
           "（二次最小二乘）后，同一判据给出显著非零残差",
    inputs="判别构型 = **天光斜坡 + 星系边缘阶跃**：`C(u,v) = 1/3 + 2u + 1.5v "
           "+ 6·[u > 1/2] + 4·[v > 1/2]`（8×8 网格，u = i/7、v = j/7；"
           "含 1/3 这个 binary64 不可精确表示的值）。"
           "对照臂用总次数 2 的最小二乘拟合面。"
           "⚠ **为什么不用旧夹具 `10 + 6uv + 3u³`**：那个构型里 `6uv` 项本身落在二次基的"
           "张成空间内、且把值域跨度从 3 抬到 9，于是 3u³ 的拟合残差被稀释到门限的 1.02 倍"
           "——余量只有 2%。改成「平滑斜坡 + 陡阶跃」后，二次拟合面离数据面**远得多**，"
           "残差读数 0.2998、余量 **30×**（门限未动，改的是夹具不是门限）",
    expected="拟合臂的相对节点残差 ≥ 1.0e-2（冻结容差 synth.p4.fit_node_repro_min_rel），"
             "即比插值臂的 1e-13 宽 ≥ 11 个数量级 ⇒ 判据有牙",
    source="闭式解析（最小二乘投影的残差定义）+ `05_INDEPENDENT_TEST_SUITE.md` §1 "
           "「恒真的比较没有证据资格」；对照面构造 = 非插值型算子类别的一般代表；"
           "阶跃项 `[u > 1/2]` 代表稀疏信噪比层在星系边缘上的真实结构（HST 帧常见），"
           "它**不属于**任何总次数 2 的多项式张成空间",
    kind=H.NEGATIVE,
    inject="把控制点求值从插值型（三次 Hermite 段）换成二次最小二乘拟合面——"
           "实现族里最容易混进来的「重建」：能出图、量级对，但不复现控制点",
    defect_id="P4-A-FIT-INSTEAD-OF-INTERP",
)
def p4a_negative_fit_arm_is_not_reproduced():
    grid = ctrl_ramp_with_edge(n=8)
    span = float(grid.max() - grid.min())
    fit = lsq_quadratic_fit(grid)
    rel = float(np.max(np.abs(fit - grid))) / span
    tol = TB.get("synth.p4.fit_node_repro_min_rel").value
    with H.evidence() as ev:
        ev.record("lsq_fit_arm.rel_node_residual", rel, tol, "",
                  f"对照臂（非插值型）应显著越界；门限未动，夹具换成 "
                  f"「斜坡+阶跃」判别构型（跨度 {span:.4f}）")
        ev.record("interpolant_arm.rel_node_residual",
                  float(Reconstructor(np.tile(CTRL_PROFILE, (8, 1))).node_reproduction_max_abs())
                  / float(CTRL_PROFILE.max() - CTRL_PROFILE.min()),
                  tol, "", "插值臂应远在门下")
        H.is_true(rel >= tol,
                  f"拟合臂相对节点残差 {rel:.3e} 未达门限 {tol:.3e} ⇒ "
                  "「node_reproduction≈0」在本夹具上无判别力")


# ===========================================================================
# §D P4-b 算子间差异
# ===========================================================================


@H.test(
    "p4b_operator_difference_signed",
    intent="同一控制网格上 spline_clip 与 bilinear 的最大差显著 > 0，且不超过值域跨度；"
           "并给出单调剖面上的真实关系（登记为「非 ≥」）",
    inputs="8×8 非共线控制面 10 + 5v + 3u + 2uv + 1.5u²（对照：常量场与共线场上"
           "两个算子同值 ⇒ 恒真）；64×64 查询网格",
    expected="max|spline − bilinear| / 值域跨度 ≥ 1.0e-4（synth.p4.operator_sep_min_rel）"
             "且 ≤ 1.0；单调剖面上样条**不越出数据值域**（无过冲），"
             "但**不是**处处 ≥ 双线性（实测下冲，见 evidence）",
    source="闭式解析（分段线性对三次的最大误差 = c₃h³/2；自然样条对三次在内部胞精确，"
           "只被自然边界条件改首末两胞）+ 正本 `NOISE_SNR.md:503` 逐字"
           "「可分离样条不外插出控制点之上的值」",
)
def p4b_operator_difference_signed():
    grid = ctrl_curved(8)
    span = float(grid.max() - grid.min())
    r_sp = Reconstructor(grid, operator=OP_SPLINE_CLIP)
    r_bl = Reconstructor(grid, operator=OP_BILINEAR)
    ax = r_sp.axis(0.0, 7.0, 0.125)
    f_sp = r_sp.field(ax, ax)
    f_bl = r_bl.field(ax, ax)
    rel = float(np.max(np.abs(f_sp - f_bl))) / span
    tol_sep = TB.get("synth.p4.operator_sep_min_rel").value
    with H.evidence() as ev:
        ev.record("max|spline-bilinear|/span", rel, tol_sep, "", "非共线构型")
        H.is_true(rel >= tol_sep,
                  f"算子间差 {rel:.3e} 未达分离度门限 {tol_sep:.3e}")
        H.less_equal(rel, 1.0, "算子间差不应超过值域跨度")
        # 单调剖面上的真实关系
        gm = np.tile(CTRL_MONOTONE, (8, 1))
        rs = Reconstructor(gm, operator=OP_SPLINE_CLIP)
        rb = Reconstructor(gm, operator=OP_BILINEAR)
        ax = rs.axis(0.0, 7.0, 0.025)
        ss = rs.field(ax, np.full_like(ax, 3.7))[0]
        sb = rb.field(ax, np.full_like(ax, 3.7))[0]
        delta = ss - sb
        prof = CTRL_MONOTONE
        ev.record("monotone.min(spline-bilinear)", float(np.min(delta)), 0.0, "",
                  "实测为负 ⇒ 派单稿「单调剖面上 spline≥bilinear」在数学上不成立")
        ev.record("monotone.spline_min", float(np.min(ss)), float(np.min(prof)), "",
                  "样条不越出数据值域下端")
        ev.record("monotone.spline_max", float(np.max(ss)), float(np.max(prof)), "",
                  "样条不越出数据值域上端")
        # 可判的性质：样条**不越出数据值域**（正本 §503「不外插出控制点之上的值」）
        H.is_true(float(np.min(ss)) >= float(np.min(prof)) - 1e-12,
                  "单调剖面上样条下冲越出数据值域")
        H.is_true(float(np.max(ss)) <= float(np.max(prof)) + 1e-12,
                  "单调剖面上样条过冲越出数据值域")
        # 反向读数登记：样条并非处处 ≥ 双线性
        H.is_true(float(np.min(delta)) < 0.0,
                  "样条在单调剖面上处处 ≥ 双线性（与实测不符，需复核判据口径）")


@H.test(
    "p4b_negative_constant_field_is_degenerate",
    intent="常量控制场上两个算子逐点同值 ⇒ 「算子间差」判据恒真、无判别力",
    inputs="8×8 常量控制面（全部 1/3），同一查询网格",
    expected="max|spline − bilinear| = 0（逐位），即该判据在常量场上**恒绿**；"
             "⇒ 判据只在非共线构型上有判别力（P4-b 正例已用非共线构型）",
    source="闭式解析（两算子对常量场解析同值）；`05` §1「恒真的比较没有证据资格」；"
           "`NOISE_SNR.md:503` 逐字「可分离样条不外插出控制点之上的值」的同源退化面",
    kind=H.NEGATIVE,
    inject="把 P4-b 判据的夹具从非共线控制面换成常量控制面——判据随即恒绿，"
           "任何实现（含把样条换成常数的实现）都能通过",
    defect_id="P4-B-DEGENERATE-CONFIG",
)
def p4b_negative_constant_field_is_degenerate():
    grid = np.full((8, 8), 1.0 / 3.0)
    rs = Reconstructor(grid, operator=OP_SPLINE_CLIP)
    rb = Reconstructor(grid, operator=OP_BILINEAR)
    ax = rs.axis(0.0, 7.0, 0.25)
    diff = float(np.max(np.abs(rs.field(ax, ax) - rb.field(ax, ax))))
    tol = TB.get("synth.p4.operator_sep_min_rel").value
    with H.evidence() as ev:
        ev.record("constant_field.max|spline-bilinear|", diff, tol, "",
                  "恒真退化面：判据不构成证据")
        # ⚠ 退化面是「差 ≪ 分离度门限」，不是逐位 0：Hermite 段在 h 非二进制有理处
        # 的 `(1−h)v + hv + m·(…)` 舍入给 ~1 ulp 量级的差。写成逐位 0 会把夹具
        # 的浮点性质误当判据。
        H.less_equal(diff, tol, "常量场上两算子之差（应远低于分离度门限）")
        H.is_false(diff >= tol, "常量场上判据仍判得开（与解析退化面不符）")


# ===========================================================================
# §E P4-c 非负公理
# ===========================================================================


@H.test(
    "p4c_clipped_field_nonnegative",
    intent="钳制后全场 ≥ min(control_values) ≥ 0：无钳制的样条在对抗构型上可给显著为负，"
           "钳制把值域锁回控制值域",
    inputs="8×8 对抗构型（角上 9.0/3.0、内区 1.0，曲率剧烈）；"
           "spline_clip 算子 vs 同一控制面上的无钳制求值",
    expected="无钳制求值的场最小值显著 < 0（控制点最小值为 +1.0）；"
             "钳制后场最小值 = min(control) = 1.0 ≥ 0",
    source="正本 `NOISE_SNR.md:503` 逐字「无值域钳制时在对抗构型上可出现显著为负的场」+"
           "「**非负性不能只靠钳制保证**，还要求控制点自身非负」；"
           "`weight_chain.h:120-123` 算子词表逐字「值域钳制**不是可选项**」",
)
def p4c_clipped_field_nonnegative():
    rc = Reconstructor(CTRL_ADVERSARIAL, operator=OP_SPLINE_CLIP)
    # 无钳制对照：同一实现、同一控制面、同一算子，只把**求值输出的钳制**关掉。
    # `apply_clip=False` 是量测 override，不是可用算子（词表逐字钳制不是可选项）。
    raw = Reconstructor(CTRL_ADVERSARIAL, operator=OP_SPLINE_CLIP, apply_clip=False)
    ax = rc.axis(0.0, 7.0, 0.05)
    f_clip = rc.field(ax, ax)
    f_raw = raw.field(ax, ax)
    cmin = float(CTRL_ADVERSARIAL.min())
    with H.evidence() as ev:
        ev.record("unclipped_field.min", float(f_raw.min()), 0.0, "",
                  "对照：无钳制（正本 §503「显著为负」）")
        ev.record("clipped_field.min", float(f_clip.min()), cmin, "",
                  "钳制后应锁在 min(control)")
        ev.record("control_min", cmin, 0.0, "", "控制点自身最小值")
        H.is_true(float(f_raw.min()) < -1e-3,
                  f"无钳制样条场最小值 {f_raw.min():.4f} 未显著为负 ⇒ 对抗构型不够对抗")
        H.less_equal(float(f_clip.min()), cmin, "钳制后场低于 min(control)")
        H.is_true(cmin >= 0.0, "控制点自身非负（本夹具应成立）")
        H.is_true(float(f_clip.min()) >= 0.0, "钳制后场非负")
        # 负 σ 的推论：无钳制时 σ_F := F_ref/SNR 在负 SNR 上给负 σ（非物理）
        f_ref = 1000.0
        sig_bad = min(f_ref / v for v in f_raw.ravel() if v != 0.0)
        ev.record("unclipped.min_sigma_F (F_ref=1000)", sig_bad, 0.0, "",
                  "正本 weight_chain.h:120 实测 min σ = −0.5585，「非物理」")
        H.is_true(sig_bad < 0.0, "无钳制场未给出负的 σ_F")


@H.test(
    "p4c_negative_control_points_break_nonnegativity",
    intent="**控制点自身含负值**时，值域钳制**不能**给出非负性——"
           "这正是正本 §503「非负性不能只靠钳制保证」的可判臂",
    inputs="8×8 控制面，四个角节点取 −1/3（binary64 不可精确表示）、其余 1/3；"
           "spline_clip 算子（钳制区间 = [min, max] = [−1/3, 1/3]）",
    expected="钳制后场最小值 = −1/3 < 0 ⇒ 「钳制 ⇒ 非负」这条推论在该臂上**不成立**；"
             "非负公理必须**同时**要求控制点自身非负",
    source="正本 `NOISE_SNR.md:503` 逐字「非负性**不能只靠钳制保证**，还要求控制点自身非负」；"
           "`weight_chain.h:161-163` 逐字「钳到有效控制值值域」⇒ clip_low = min(control)",
    kind=H.NEGATIVE,
    inject="把「值域钳制 ⇒ 场非负」当成充分条件（只钳不查控制点符号）——"
           "在控制点含负值的构型上发布负信噪比场",
    defect_id="P4-C-CLIP-ONLY-NONNEG",
)
def p4c_negative_control_points_break_nonnegativity():
    grid = np.full((8, 8), 1.0 / 3.0)
    grid[0, 0] = grid[0, -1] = grid[-1, 0] = grid[-1, -1] = -1.0 / 3.0
    rc = Reconstructor(grid, operator=OP_SPLINE_CLIP)
    ax = rc.axis(0.0, 7.0, 0.05)
    f = rc.field(ax, ax)
    fmin = float(f.min())
    with H.evidence() as ev:
        ev.record("clip_low (= min control)", rc.clip_low, 0.0, "",
                  "钳制区间下端随控制点走")
        ev.record("clipped_field.min", fmin, 0.0, "",
                  "负值 ⇒ 「钳制即可保证非负」不成立")
        H.less_equal(fmin, rc.clip_low, "场低于钳制区间下端")
        H.is_true(fmin < 0.0, "含负控制点时钳制后仍非负（与正本 §503 相悖）")


# ===========================================================================
# §F P4-d 胞内含源的偏差方向
# ===========================================================================


@H.test(
    "p4d_intracell_source_bias_is_optimistic",
    intent="胞内含源时自然样条**恒停在控制点最大值**：重建峰退化为空间常数，"
           "峰值处**低估**（权重偏保守）、胞缘**高估**（权重偏乐观）⇒ 测偏差**符号**",
    inputs="64×64 像素域、cell 宽 8 px；真值 = 均匀底 2.0 + 高斯点源（σ=2.55 px、"
           "总通量 400 e⁻、源心在像素 28.5 严格位于 cell (3,3) 内）；"
           "8×8 控制格由 cell 内中位数给出",
    expected="① 重建场上界 ≤ 控制点最大值（正本 §503「不外插出控制点之上的值」）；"
             "② 胞内重建的空间起伏显著小于真值（峰退化为空间常数）；"
             "③ 真值峰处有符号偏差 < 0（低估）；④ 胞内存在有符号偏差 > 0 的像元"
             "（高估 ⇒ w = SNR²/F_ref² 偏乐观）",
    source="闭式解析（可分离自然样条在节点区间上不越出邻接控制值）+ "
           "正本 `NOISE_SNR.md:503` 逐字「守恒映射侧的自然样条求值器在胞内含源时**恒停在"
           "控制点最大值**（可分离样条不外插出控制点之上的值），此时重建峰退化为空间常数、"
           "亮区被过度加权，误差方向是**乐观**而非保守」",
)
def p4d_intracell_source_bias_is_optimistic():
    truth = source_scene()
    ctrl = control_from_source(median=True)
    rc = Reconstructor(ctrl, dx=CELL, operator=OP_SPLINE_CLIP)
    ax = rc.axis(0.0, float(PX - 1), 1.0)
    fld = rc.field(ax, ax)
    sl = slice(3 * CELL, 4 * CELL)
    cell_rec, cell_true = fld[sl, sl], truth[sl, sl]
    bias = cell_rec - cell_true
    ctrl_max = float(ctrl.max())
    with H.evidence() as ev:
        ev.record("control_max", ctrl_max, 0.0, "", "cell (3,3) 的 patch 中位数")
        ev.record("recon_cell.max", float(cell_rec.max()), ctrl_max, "",
                  "重建不越出控制点最大值")
        H.less_equal(float(cell_rec.max()), ctrl_max,
                     "重建场越出控制点最大值（违反正本 §503「不外插出控制点之上」）")
        ptp_rec, ptp_true = float(np.ptp(cell_rec)), float(np.ptp(cell_true))
        ev.record("recon_cell.ptp (peak 退化)", ptp_rec, ptp_true, "",
                  "重建峰退化为空间常数")
        ev.record("true_cell.ptp", ptp_true, 0.0, "", "")
        H.is_true(ptp_rec < 0.5 * ptp_true,
                  f"重建胞内起伏 {ptp_rec:.3f} 未显著小于真值 {ptp_true:.3f} ⇒ 峰未退化")
        # 符号：峰处低估
        pk = int(np.argmax(cell_true))
        ev.record("signed_bias_at_true_peak", float(bias.ravel()[pk]), 0.0, "",
                  "< 0 ⇒ 峰值低估（w 偏保守）")
        H.is_true(float(bias.ravel()[pk]) < 0.0, "真值峰处未被低估")
        # 符号：胞缘高估 ⇒ 乐观
        ev.record("signed_bias.max", float(bias.max()), 0.0, "",
                  "> 0 ⇒ 亮区被过度加权（乐观方向）")
        H.is_true(float(bias.max()) > 0.0,
                  "胞内未出现高估像元 ⇒ 「乐观方向」未被观测到")
        # w = SNR²/F_ref²（F_ref 同一帧、逐像元为公共因子）⇒ 该像元上重建权重与
        # 真权重之比 = (SNR_recon/SNR_true)²。乐观方向 = 该比 > 1（重建权重偏高）。
        w_ratio_max = float(((cell_rec / cell_true) ** 2).max())
        ev.record("w_ratio=(SNR_recon/SNR_true)^2 .max", w_ratio_max, 1.0, "",
                  "> 1 ⇒ 该像元上重建权重高于真权重（乐观方向）")
        H.is_true(w_ratio_max > 1.0, "未出现权重过估（乐观方向）")


# ===========================================================================
# §G P4-e mesh 中值前置的域依赖
# ===========================================================================


def _mesh_rmse(obs, truth, q):
    """重建场对真控制面的 RMSE（shape 与 level 合在一起；不用 E，见 §0.2 第 3 条）。"""
    r = Reconstructor(obs, operator=OP_SPLINE_CLIP)
    t = Reconstructor(truth, operator=OP_SPLINE_CLIP)
    ax = r.axis(0.0, 7.0, 7.0 / (q - 1))
    fr, ft = r.field(ax, ax), t.field(ax, ax)
    return float(np.sqrt(np.mean((fr - ft) ** 2)))


def _mc_mesh_ratio(kind: str, seeds: int = 60) -> float:
    n = 8
    j, i = np.meshgrid(np.linspace(0, 1, n), np.linspace(0, 1, n), indexing="ij")
    if kind == "high_contrast":
        truth = 10.0 + 4.0 * j + 3.0 * i          # 光滑真面（节点间变化 Δ = 4/7+3/7 = 1.0）
        out = []
        for s in range(seeds):
            r = np.random.default_rng(20260928 + s)
            obs = truth.copy()
            sp = r.integers(0, n, size=(10, 2))
            obs[sp[:, 0], sp[:, 1]] += 6.0          # 胞内未分辨点源污染的孤立节点
            out.append(_mesh_rmse(mesh_median_3x3(obs), truth, 57)
                       / _mesh_rmse(obs, truth, 57))
        return float(np.median(out))
    truth = 8.0 + 6.0 * j + 5.0 * i - 4.0 * j ** 2 - 3.5 * i ** 2 + 2.0 * j * i
    out = []
    for s in range(seeds):
        r = np.random.default_rng(20260928 + s)
        obs = truth + r.normal(0.0, 0.08, size=(n, n))
        out.append(_mesh_rmse(mesh_median_3x3(obs), truth, 57)
                   / _mesh_rmse(obs, truth, 57))
    return float(np.median(out))


@H.test(
    "p4e_mesh_median_domain_dependent",
    intent="mesh 中值前置滤波的优劣**随数据来源翻号**：高对比域改进、"
           "seeing-limited 域有害——符号翻转本身就是判据",
    inputs="两条臂各 60 seed：①高对比域（`high_contrast_unresolved_sources=true` 档，"
           "光滑真面 10+4v+3u + 10 个孤立污染节点幅度 6.0）；"
           "②seeing-limited 域（光滑弯曲天光面 8+6v+5u−4v²−3.5u²+2uv + 高斯扰动 0.08）",
    expected="高对比域 RMSE 比值 ≤ 5.0e-1（synth.p4.mesh_median_help_ratio）；"
             "seeing-limited 域 RMSE 比值 ≥ 3.9e-1（synth.p4.mesh_median_harm_ratio，"
             "正本「真实地面帧上劣 39~74%」的下端）",
    source="正本 `weight_chain.h:120-123` 算子词表逐字「② 中值前置滤波在默认目标域"
           "（地面/seeing-limited）**有害**（真实地面帧上劣 39~74%、解析可分辨域劣 9.3 倍），"
           "**只在 HST 类高对比域必需**」+ 同处逐字"
           "「按**数据来源**选择重建算子……**无法判断时传 false**（默认目标域上 mesh 滤波有害；"
           "不得从控制网格自身推断）」；夹具比值的解析锚见两条容差的 `source`",
)
def p4e_mesh_median_domain_dependent():
    help_r = _mc_mesh_ratio("high_contrast")
    harm_r = _mc_mesh_ratio("seeing_limited")
    tol_help = TB.get("synth.p4.mesh_median_help_ratio").value
    tol_harm = TB.get("synth.p4.mesh_median_harm_ratio").value
    with H.evidence() as ev:
        ev.record("high_contrast.RMSE_ratio(filtered/raw)", help_r, tol_help, "",
                  "高对比域：必须改进")
        ev.record("seeing_limited.RMSE_ratio(filtered/raw)", harm_r, tol_harm, "",
                  "seeing-limited 域：必须劣化（正本 39~74% 的下端）")
        H.less_equal(help_r, tol_help, "高对比域 mesh 中值未改进")
        H.is_true(harm_r >= tol_harm,
                  f"seeing-limited 域 mesh 中值劣化 {harm_r:.3f} 未达正本下端 {tol_harm}")
        ev.record("domain_sign_flip", harm_r - help_r, 0.0, "",
                  "> 0 ⇒ 两域方向相反，判据非恒真")


@H.test(
    "p4e_negative_single_domain_criterion",
    intent="**只跑一个域**的 mesh 中值判据恒真：判「mesh 中值总是更好」的实现"
           "在 seeing-limited 域上照样通过",
    inputs="seeing-limited 臂单独跑（与正例同夹具、同 60 seed），"
           "判据只要求 RMSE 比值 ≤ 5.0e-1",
    expected="seeing-limited 臂的实际比值 > 5.0e-1 ⇒ 单域判据判红；"
             "⇒ 「mesh 中值总是更好」的判据**恒假**，正本的两域规则是必需的",
    source="闭式解析（两域比值方向相反，见 P4-e 正例 evidence）+ "
           "`weight_chain.h:120-123` 逐字「中值前置滤波在默认目标域有害」+ "
           "`05` §1「恒真的比较没有证据资格」",
    kind=H.NEGATIVE,
    inject="把 mesh 中值判据的适用域从「按数据来源分档」收紧成「一律更好」——"
           "只在 HST 类高对比域测过一次就推广到默认目标域",
    defect_id="P4-E-SINGLE-DOMAIN",
)
def p4e_negative_single_domain_criterion():
    harm_r = _mc_mesh_ratio("seeing_limited")
    tol_help = TB.get("synth.p4.mesh_median_help_ratio").value
    with H.evidence() as ev:
        ev.record("seeing_limited.RMSE_ratio", harm_r, tol_help, "",
                  "单域判据在此判红 ⇒ 「一律更好」恒假")
        H.is_false(harm_r <= tol_help,
                   "seeing-limited 域竟然也满足「一律更好」⇒ 两域规则失去必要性")


# ===========================================================================
# §H P4-f cell 中心约定（闭式，有牙）
# ===========================================================================


@H.test(
    "p4f_cell_center_convention_closed_form",
    intent="节点必须落在 cell 中心：`x0 = grid_origin + (dx−1)/2`。"
           "dx = 64 时半胞 = **31.5 px**（闭式），正确约定下控制点逐位复现",
    inputs="8×8 控制面 10+5v+3u+2uv+1.5u²，cell 宽 dx = 64 px、grid_origin = 0；"
           "在**冻结的 cell 中心像素坐标**（i·64 + 31.5）上求值",
    expected="cell 中心约定下 node_reproduction_max_abs ≤ 1.0e-13 且"
             "cell_center_offset_max_abs = 0；半胞平移量 = (64−1)/2 = **31.5 px**（闭式）",
    source="闭式解析（cell 中心 = grid_origin + i·dx + (dx−1)/2）+ "
           "`weight_chain.h:158-164` 几何面逐字「**必须 `x0 = grid_origin_x + (dx−1)/2`**"
           "（y 同）……把节点当 cell 角点（`x0 = grid_origin_x`）会使整场平移半个 cell"
           "（Δ=64 时 31.5 px）」",
)
def p4f_cell_center_convention_closed_form():
    grid = ctrl_curved(8)
    dx = 64
    rc = Reconstructor(grid, dx=dx, operator=OP_SPLINE_CLIP)
    repro = rc.node_reproduction_max_abs()
    off = rc.cell_center_offset_max_abs()
    tol = TB.get("synth.p4.node_repro_abs").value
    half = (dx - 1) / 2.0
    with H.evidence() as ev:
        ev.record("half_cell_px", half, 31.5, 0.0, "闭式 (dx-1)/2，dx=64")
        H.exact(half, 31.5, "半胞平移量（闭式）")
        ev.record("node_reproduction_max_abs", repro, tol, "", "cell 中心约定")
        H.less_equal(repro, tol, "cell 中心约定下的控制点复现")
        ev.record("cell_center_offset_max_abs", off, 0.0, "", "cell 中心门")
        H.exact(off, 0.0, "节点相对 cell 中心的偏移")
        ev.record("node_pixel_x[1]", rc.node_pixel(1, 0)[0], 95.5,
                  "px", "第二个节点的像素中心坐标 = 1·64 + 31.5")
        H.exact(rc.node_pixel(1, 0)[0], 95.5, "第二个节点的像素坐标")


@H.test(
    "p4f_negative_corner_convention_shifts_field",
    intent="**注入角点约定**（`x0 = grid_origin_x`）：整场平移半个 cell，"
           "在真节点像素处给出显著超界的控制点残差",
    inputs="与正例同一控制面与同一 cell 宽 dx = 64；`corner_convention=True` 使 "
           "`x0 = grid_origin_x`（节点被当 cell 角点），查询坐标**不变**（仍是 "
           "i·64 + 31.5）",
    expected="控制点残差 / 值域跨度 ≥ 5.0e-2（synth.p4.corner_convention_min_rel），"
             "即超界 ≥ 1 个数量级以上（正例门限是 1.0e-13，差 11 个数量级）",
    source="正本条款 + 闭式：`weight_chain.h:158-164` 逐字「把节点当 cell 角点"
           "（`x0 = grid_origin_x`）会使整场平移半个 cell（Δ=64 时 31.5 px）」；"
           "平移量 s = 63/128 = 0.4921875 节点单位，对冻结夹具一阶/二阶系数给出的"
           "偏差归一闭式见容差 `source`",
    kind=H.NEGATIVE,
    inject="节点坐标从 `grid_origin + (dx−1)/2` 改成 `grid_origin`（cell 角点约定）——"
           "查询坐标、层数据、算子全不变，只错在网格原点这一个量上",
    defect_id="P4-F-CORNER-CONVENTION",
)
def p4f_negative_corner_convention_shifts_field():
    grid = ctrl_curved(8)
    dx = 64
    span = float(grid.max() - grid.min())
    rc_bad = Reconstructor(grid, dx=dx, operator=OP_SPLINE_CLIP, corner_convention=True)
    rc_ok = Reconstructor(grid, dx=dx, operator=OP_SPLINE_CLIP)
    # ⚠ 查询坐标**用冻结的真节点像素**（cell 中心 i·dx + (dx−1)/2），不是角点约定自己的
    # 节点坐标。角点约定实现是**自洽**的（它的内部网格随 x0 一起移），拿它自己的节点坐标
    # 去查它自己必然逐位复现 —— 那样测的是自洽性不是错位。
    worst = 0.0
    for j in range(8):
        for i in range(8):
            px, py = rc_ok.node_pixel(i, j)
            worst = max(worst, abs(rc_bad.eval(px, py) - grid[j, i]))
    rel = worst / span
    tol = TB.get("synth.p4.corner_convention_min_rel").value
    with H.evidence() as ev:
        ev.record("corner_convention.half_cell_px", (dx - 1) / 2.0, 0.0, "",
                  "整场平移量（px）")
        ev.record("corner_convention.rel_node_residual", rel, tol, "",
                  "负例：同一判据判红")
        ev.record("cell_center_offset_max_abs", rc_bad.cell_center_offset_max_abs(),
                  0.0, "", "cell 中心门应拦下这个错位")
        H.is_true(rc_bad.cell_center_offset_max_abs() > 1.0,
                  "cell 中心门未拦下角点约定（门失效）")
        # 自洽性登记：角点约定在**它自己的**节点坐标上逐位复现 ⇒ 判别力只能来自
        # 「查询坐标是否按冻结约定」这一条，不是来自算子本身。
        self_repro = rc_bad.node_reproduction_max_abs()
        ev.record("corner_convention.self_node_reproduction", self_repro, 0.0, "",
                  "自洽面：拿它自己的节点坐标查它自己恒为 0 ⇒ 往返自证，无判别力")
        H.exact(self_repro, 0.0, "角点约定在自身节点坐标上的自洽残差")
        H.is_true(rel >= tol,
                  f"角点约定的控制点残差 {rel:.3e} 未达门限 {tol:.3e} ⇒ 判据无牙")


# ===========================================================================
# §I P4-g 定义域 fail-closed
# ===========================================================================


@H.test(
    "p4g_domain_fail_closed",
    intent="定义域 = 层覆盖的 cell 并集 `[gox−0.5, gox+nx·dx−0.5]`；"
           "边界点**在**域内、域外一点**不在**域内",
    inputs="8×8 层、dx = 64、grid_origin = 0 ⇒ 定义域 x,y ∈ [−0.5, 511.5]；"
           "探针取 lo−1e-9 / lo / lo+1e-9 / hi / hi+1e-9",
    expected="lo 与 hi 在域内；lo−1e-9 与 hi+1e-9 在域外；域外求值抛 "
             "`ReconstructError`（`out_of_domain`），**不回退帧级标量**",
    source="闭式解析（定义域端点 = grid_origin ∓ 0.5、grid_origin + nx·dx ∓ 0.5）+ "
           "`weight_chain.h:165-170` 几何面逐字「**定义域 = 层覆盖的 cell 并集**："
           "`[grid_origin_x − 0.5, grid_origin_x + nx·dx − 0.5]`……越出该并集 → "
           "fail-closed（不外推、**不回退帧级**）」",
)
def p4g_domain_fail_closed():
    dx = 64
    rc = Reconstructor(np.tile(CTRL_PROFILE, (8, 1)), dx=dx, operator=OP_SPLINE_CLIP)
    lo, hi = rc.domain[0], rc.domain[1]
    with H.evidence() as ev:
        ev.record("domain_lo", lo, -0.5, "", "闭式 grid_origin − 0.5")
        ev.record("domain_hi", hi, 511.5, "", "闭式 grid_origin + nx·dx − 0.5")
        H.exact(lo, -0.5, "定义域下端")
        H.exact(hi, 511.5, "定义域上端")
        for x, expect_in in ((lo - 1e-9, False), (lo, True), (lo + 1e-9, True),
                             (hi, True), (hi + 1e-9, False)):
            got = rc.in_domain(x, lo + 1.0)
            ev.record(f"in_domain(x={x!r})", got, expect_in, "", "")
            H.exact(got, expect_in, f"x={x!r} 的域内判定")
        # 域外 ⇒ 显式失败，且**不是**哨兵值
        H.raises(ReconstructError, lambda: rc.eval(hi + 1.0, 10.0),
                 "域外求值必须 fail-closed")
        ev.record("out_of_domain_value", "ReconstructError", None, "",
                  "无哨兵值、无帧级标量回退")
        # 域内端点半胞由端点节点常数延拓（正本「最外半个 cell 由端点节点常数延拓」）
        v_out = rc.eval(hi, 0.0)
        ev.record("eval at domain_hi", v_out, float(CTRL_PROFILE[-1]), "",
                  "最外半个 cell 常数延拓")
        H.less_equal(abs(v_out - float(CTRL_PROFILE[-1])), 1e-12,
                     "最外半个 cell 的常数延拓")


@H.test(
    "p4g_negative_frame_scalar_fallback",
    intent="域外**回退帧级标量**（`weight_chain.h` 逐字禁止）的实现在本判据上判红",
    inputs="同正例的层与探针；注入实现对域外查询返回帧级标量而不是 fail-closed",
    expected="帧级标量回退给出有限值且**落在门内** ⇒ 域外查询被静默应答、"
             "「无可判定」变成「有数值」⇒ 判据必须能抓住",
    source="正本 `weight_chain.h:165-170` 逐字「越出该并集 → fail-closed（不外推、"
           "**不回退帧级**）」+ `SparseReconstruction.out_of_domain` 字段语义；"
           "`UNRESOLVED.md:47` 裁-22 同族的「适用域门被绕过」缺陷面",
    kind=H.NEGATIVE,
    inject="把 `eval` 的域外分支从 fail-closed 改成返回帧级标量（一个「看起来总能出值」"
           "的实现族）——query 出了层覆盖范围时它给一个有限数",
    defect_id="P4-G-FRAME-SCALAR-FALLBACK",
)
def p4g_negative_frame_scalar_fallback():
    dx = 64
    rc = Reconstructor(np.tile(CTRL_PROFILE, (8, 1)), dx=dx, operator=OP_SPLINE_CLIP)
    frame_scalar = float(np.median(CTRL_PROFILE))     # 帧级标量（被注入的回退值）
    lo, hi = rc.domain[0], rc.domain[1]
    with H.evidence() as ev:
        H.raises(ReconstructError, lambda: rc.eval(hi + 1.0, 10.0),
                 "正确实现对域外必须 fail-closed")
        fallback = frame_scalar                      # 注入实现的返回
        in_range = bool(np.min(CTRL_PROFILE) - 1e-12 <= fallback
                        <= np.max(CTRL_PROFILE) + 1e-12)
        ev.record("frame_scalar_fallback_value", fallback,
                  float(np.max(CTRL_PROFILE)), "",
                  "回退值与域内场同量级 ⇒ 静默混进结果，无从分辨")
        ev.record("fallback_inside_field_range", in_range, True, "",
                  "「域外有值」这一缺陷在数值上不可分辨 ⇒ 必须靠 fail-closed 面抓")
        H.is_true(in_range,
                  "帧级回退值落在域内场值域之外 ⇒ 该注入抓不到（需如实登记）")


# ===========================================================================
# §J P4-h w 的双路线验证（⚠ 待裁-27）
# ===========================================================================

CAI27 = (
    "⚠⚠ **待裁-27 裁决**：docs/engineering/governance/UNRESOLVED.md:47 记录 "
    "`ACSD_DESIGN.md`「P4 重建稠密信噪比」写「重建量**随源亮度变化**」，"
    "而「信噪比重建与逆方差叠加」写「三者都产出**同一物理量** `SNR = F_ref/σ_F` 的稠密表示」；"
    "若重建量随源亮度变，则 `w = SNR²/F_ref² = 1/σ_F²` 的相消前提不成立。"
    "⇒ 本条只验证「给定层值，两条独立路线算出同一个 w」这一**代数**事实，"
    "**不裁决** `w ≡ 1/σ_F²` 是否成立、也不裁决重建量是否随源亮度变。"
)

#: 本条必须逐字出现在 `intent` 与 `source` 里的裁-27 声明（对抗复核的硬性要求）。
CAI27_NOT_ADJUDICATED = "⚠ 裁-27 未裁决，本条不构成裁决"

#: 独立 oracle 夹具：控制层、cell 宽、像素采样步长（字面常量）。
#: ⚠ `dx = 4` 而不是 1：`dx = 1` 时 cell 中心偏移 `(dx−1)/2 ≡ 0`，角点约定与中心约定
#: **重合** ⇒ 几何门（P4-f）在该夹具上恒真。`dx = 4` 让偏移 = 1.5 px 可分辨。
#: ⚠ 采样步长 1.0 px ⇒ 查询点**全部是非节点**的像素中心（节点在 `x0 + i·dx`）。
#: 这是本条与 P4-a 的 scipy 对拍的关键差别：P4-a 只在**节点坐标**上比较，而任何插值器
#: 在节点上都等于输入值 ⇒ 那里的一致性**按构造**成立（P4-a 的 oracle 只能在节点族上
#: 提供证据，见 §2 恒真清单的 `p4.scipy_oracle_only_on_nodes` 登记）。
P4H_DX = 4
P4H_STEP_PX = 1.0
#: 该帧自己的 `F_ref,k`（ADU）与乘性光度响应 `g_k`。`F_ref,k` 取 binary64 不可精确
#: 表示的值，使 `F_ref,k²` 与 `F_ref,k` 的舍入路径不同（避免逐位相同掩盖装配错）。
P4H_F_REF = 1234.5678901234
P4H_G_K = 1.37
#: 邻帧的 `k_photo`（用于「配错 `F_ref,k`」的牙齿见证臂）。
P4H_K_SELF = 1.0
P4H_K_NEIGHBOUR = 2.8


def bilinear_cell_value_oracle(values: np.ndarray, xs: np.ndarray, ys: np.ndarray,
                               dx: float, dy: float) -> np.ndarray:
    """**独立**双线性层值重建 —— 本条 P4-h 的 oracle（`PHASE2_UPM.md` §5 逐字
    `C_f(p) = 双线性(8×8 cell, θ)`）。

    **为什么它是独立代码路径**：函数体里不出现 `Reconstructor`、`_bilinear`、本文件的
    cell 索引夹取或权重计算；胞元索引、权重与求值顺序全部按双线性插值的定义另写一遍，
    并且用的是「先在 `x` 方向线性插值、再在 `y` 方向线性插值」（`lerp(lerp(...))`）的
    教科书写法，与被测实现「四个角点权重一次加权求和」的写法在**浮点求值顺序**上也不同。
    对抗复核判定的旧版缺陷正是「oracle 与主实现共用同一个 `Reconstructor` 类、只换
    `operator=` 字符串」，本函数是该缺陷的直接修复。
    """
    g = np.asarray(values, dtype=np.float64)
    ny, nx = g.shape
    x0 = (float(dx) - 1.0) / 2.0                      # cell 中心约定（`weight_chain.h`）
    y0 = (float(dy) - 1.0) / 2.0
    X, Y = np.meshgrid(np.asarray(xs, dtype=np.float64),
                       np.asarray(ys, dtype=np.float64), indexing="xy")
    u = (X - x0) / float(dx)
    v = (Y - y0) / float(dy)
    i = np.clip(np.floor(u).astype(np.int64), 0, nx - 2)
    j = np.clip(np.floor(v).astype(np.int64), 0, ny - 2)
    tu = np.clip(u - i, 0.0, 1.0)
    tv = np.clip(v - j, 0.0, 1.0)
    lo = g[j, i] * (1.0 - tu) + g[j, i + 1] * tu      # x 方向线性插值
    hi = g[j + 1, i] * (1.0 - tu) + g[j + 1, i + 1] * tu
    return lo * (1.0 - tv) + hi * tv                  # y 方向线性插值


def references_name(func, name: str) -> bool:
    """**源码级**独立性守卫：函数 `func` 的 AST 里是否引用了名为 `name` 的符号。

    ⚠ **为什么需要它**：两条实现「输出相等」这件事本身**无法**证明它们互相独立 ——
    把 oracle 换回主实现只会让差变成逐位 0，比较照样绿（对抗复核判定的缺陷 1 正是这种
    「换 `operator=` 字符串的伪 oracle」）。独立性是**源码属性**而不是数值属性，
    只能在 AST 层面判。用 `ast` 而不是字符串匹配：docstring 里出现 `Reconstructor` 这个词
    （本函数就写了「函数体里不出现 `Reconstructor`」）不会误判。
    """
    tree = ast.parse(textwrap.dedent(inspect.getsource(func)))
    fn = tree.body[0]
    for node in ast.walk(fn):
        if isinstance(node, ast.Name) and node.id == name:
            return True
        if isinstance(node, ast.Attribute) and node.attr == name:
            return True
    return False


def w_from_layer_pixel_oracle(layer_snr: np.ndarray, f_ref: float, gain: float) -> np.ndarray:
    """**独立**的逐像素权重装配 —— `weight_chain.h:390-392` 逐字
    `w(x,y) = (SNR_layer(x,y) / F_ref,k)² · g_k²`。

    ⚠ 逐字遵守同处逐字的禁令：层值已是**绝对** SNR，**不得**再乘/除帧级 SNR
    （`weight_chain.h:389-391`）。本函数没有帧级 SNR 这个入口 —— 口径错被排除在接口面。

    装配顺序写成 `(gain · layer_snr / F_ref)²`，与主实现的 `(layer_snr / F_ref)² · gain²`
    在浮点求值顺序上不同；这是**独立实现**的正常形态，不是人为扰动。
    """
    num = np.asarray(layer_snr, dtype=np.float64) * float(gain)
    return (num / float(f_ref)) ** 2


@H.test(
    "p4h_pending_cai27_w_independent_bilinear_oracle",
    intent="【路线 (a) 正本指定】用**独立双线性 oracle**（本文件另写的 `lerp(lerp(·))` "
           "实现，函数体内不出现 `Reconstructor`）重建层值，再**独立**算 w，"
           "与主实现的逐像素 w 对拍——不用 `σ_F := F_ref/SNR_layer`。"
           + CAI27_NOT_ADJUDICATED,
    inputs="8×8 非共线控制面 `ctrl_curved(8)`（`10 + 5v + 3u + 2uv + 1.5u²`，含二次项 ⇒ "
           "两算子不同构）；cell 宽 `dx = dy = 4 px`（⇒ cell 中心偏移 1.5 px 可分辨）；"
           "在**全域**按 1 px 步长取像素中心坐标（全部落在**非节点**位置，"
           "在定义域 `[−0.5, 31.5]` 内按 1 px 步长取 **32×32 = 1024 个**像素中心"
           "（首末各留 0.5 px，避开「最外半个 cell 常数延拓」那一档；"
           "查询点全部落在**非节点**位置）；"
           "该帧自己的 `F_ref,k = 1234.5678901234 ADU`、"
           "`g_k = 1.37`；另备邻帧 `k_photo = 2.8` 供牙齿见证臂配错 `F_ref,k`",
    expected="①（真判据，覆盖**全域 1024/1024**）独立双线性 oracle 的层值与主实现 "
             "`bilinear_regular_grid_v1` 的层值在 f64 非归约档（`rtol = 1e-12`）内一致；"
             "②（真判据，覆盖**全域 1024/1024**）主实现的 `w` 与独立路线"
             "（独立层值 + 独立权重装配 `w_from_layer_pixel_oracle`）在 f64 非归约档内一致；"
             "③（牙齿见证）配错 `F_ref,k`（用邻帧的）或按 `weight_chain.h:389-391` "
             "逐字禁止的方式额外乘帧级 SNR，两条都让 ② 越界 ≫ 门限 ⇒ ② 不是恒绿；"
             "④跨算子（spline_clip vs 独立双线性）的 `w` 差**只作诊断记录、不做 pass/fail "
             "对照**（两条不同算子的层值本就不同，构造上不可能一致）。"
             "**该前提未定**（裁-27），本条不构成裁决",
    source="正本 `weight_chain.h:435-438` 逐字「可用的真判据只有两条："
           "(a) 用独立双线性 oracle 重建层值再独立算 w 的对照实验；(b) 注入实验测得的 σ_F」"
           "——本条走 (a)；`weight_chain.h:390-392` 逐字 "
           "`w(x,y) = (SNR_layer(x,y)/F_ref,k)²·g_k²`；同处 :389-391 逐字「**不得**再乘/除"
           "帧级 SNR」（牙齿见证臂 ③ 的注入取自这一条禁令）；`PHASE2_UPM.md` §5 逐字 "
           "`C_f(p) = 双线性(8×8 cell, θ)`（独立 oracle 的算法口径）；"
           "容差 = `tolerances.py` §4 通用档「双精度非归约 `rtol = 1e-12`」，本文件不现编；"
           + CAI27_NOT_ADJUDICATED,
)
def p4h_pending_cai27_w_independent_bilinear_oracle():
    grid = ctrl_curved(8)                       # 非共线面（常量/共线面上两算子同值 ⇒ 恒真）
    prod = Reconstructor(grid, dx=P4H_DX, dy=P4H_DX, operator=OP_BILINEAR)
    # 域内均匀采样（首末各留 0.5 px ⇒ 不踩「最外半个 cell 常数延拓」那一档）
    lo, hi = prod.domain[0] + 0.5, prod.domain[1] - 0.5
    ax = prod.axis(lo, hi, P4H_STEP_PX)
    layer_prod = prod.field(ax, ax)                       # 主实现层值（冻结对照算子）
    layer_orc = bilinear_cell_value_oracle(grid, ax, ax, P4H_DX, P4H_DX)   # 独立 oracle
    w_main = (layer_prod / P4H_F_REF) ** 2 * P4H_G_K ** 2                # 主实现权重装配
    w_orc = w_from_layer_pixel_oracle(layer_orc, P4H_F_REF, P4H_G_K)     # 独立权重装配

    scale_l = float(np.max(np.abs(layer_orc)))
    scale_w = float(np.max(np.abs(w_orc)))
    n_px = int(layer_prod.size)
    rel_layer = float(np.max(np.abs(layer_prod - layer_orc))) / scale_l
    rel_w = float(np.max(np.abs(w_main - w_orc))) / scale_w

    # 牙齿见证 ①：把独立 oracle 的 cell 约定换成角点（`weight_chain.h:158-164` 点名的错法）
    corner_orc = bilinear_cell_value_oracle(grid, ax + (P4H_DX - 1.0) / 2.0,
                                            ax + (P4H_DX - 1.0) / 2.0, P4H_DX, P4H_DX)
    rel_layer_corner = float(np.max(np.abs(layer_prod - corner_orc))) / scale_l
    # 牙齿见证 ②：配错 `F_ref,k`（用邻帧 `k_photo` 的参考通量）
    f_ref_neighbour = P4H_F_REF * P4H_K_SELF / P4H_K_NEIGHBOUR
    rel_w_badref = float(np.max(np.abs(w_main - w_from_layer_pixel_oracle(
        layer_orc, f_ref_neighbour, P4H_G_K)))) / scale_w
    # 牙齿见证 ③：按 `weight_chain.h:389-391` 逐字禁止的方式额外乘帧级 SNR
    snr_frame_forbidden = 7.31
    rel_w_framesnr = float(np.max(np.abs(
        (layer_prod * snr_frame_forbidden / P4H_F_REF) ** 2 * P4H_G_K ** 2 - w_orc))) / scale_w

    # 诊断项（**不做 pass/fail 对照**）：跨算子的 w 差。
    spl = Reconstructor(grid, dx=P4H_DX, dy=P4H_DX, operator=OP_SPLINE_CLIP).field(ax, ax)
    w_spl = (spl / P4H_F_REF) ** 2 * P4H_G_K ** 2
    rel_cross_op = float(np.max(np.abs(w_spl - w_orc))) / scale_w
    bit_same = np.abs(spl - layer_orc) <= 1e-13 * float(np.max(np.abs(grid)))
    frac_same = float(np.count_nonzero(bit_same)) / float(bit_same.size)

    with H.evidence() as ev:
        ev.record("cai27_status", "UNRESOLVED / 未裁决", None, "",
                  "本条不裁决 `w ≡ 1/σ_F²`、不裁决重建量是否随源亮度变")
        ev.record("独立 oracle 源码级独立性守卫", 0.0, None, "",
                  "AST 扫描两个 oracle 函数的函数体：不得引用 `Reconstructor` / `_bilinear` / "
                  "`field` / `np.power`（0 = 未引用 = 独立）。"
                  "⚠ 这是**必须**的守卫：把 oracle 换回主实现只会让差变逐位 0、比较照样绿 ⇒ "
                  "独立性无法由数值证明，只能由源码判")
        ev.record("pixels_compared_layer", float(n_px), None, "",
                  f"① 独立双线性 oracle 的比较面：**全域 {n_px}/{n_px}**（无掩膜、无子集）")
        ev.record("max|层值_main − 层值_oracle| / max|层值|", rel_layer,
                  _base_tol.F64_RTOL, "",
                  "① 独立代码路径 vs 主实现 `bilinear_regular_grid_v1`（f64 非归约档）")
        ev.record("pixels_compared_w", float(n_px), None, "",
                  f"② w 的比较面：**全域 {n_px}/{n_px}**")
        ev.record("max|w_main − w_oracle| / max|w|", rel_w,
                  _base_tol.F64_RTOL, "",
                  "② 主实现权重装配 vs 独立层值 + 独立装配 `w_from_layer_pixel_oracle`")
        # ① 前置守卫：oracle 必须真的是**独立代码路径**（源码级，见 references_name 的 docstring）
        for _fn, _nm in ((bilinear_cell_value_oracle, "Reconstructor"),
                         (bilinear_cell_value_oracle, "_bilinear"),
                         (bilinear_cell_value_oracle, "field"),
                         (w_from_layer_pixel_oracle, "Reconstructor"),
                         (w_from_layer_pixel_oracle, "np.power")):
            H.is_false(references_name(_fn, _nm),
                       f"独立 oracle `{_fn.__name__}` 的函数体里引用了 `{_nm}` ⇒ "
                       "oracle 已经不再是独立代码路径（缺陷 1 的复发）")
        # ① ② 的判词
        H.less_equal(rel_layer, _base_tol.F64_RTOL,
                     "独立双线性 oracle 的层值与主实现不一致（全域）")
        H.less_equal(rel_w, _base_tol.F64_RTOL,
                     "独立路线（独立层值 + 独立权重装配）算出的 w 与主实现不一致（全域）")
        # ③ 牙齿见证：三条注入必须让 ① ② 明显越界 ⇒ 证明它们不是恒绿门
        H.is_true(rel_layer_corner > _base_tol.F64_RTOL * 1.0e3,
                  f"牙齿见证①（oracle 用角点约定）只越界 {rel_layer_corner / _base_tol.F64_RTOL:.3g}× "
                  "⇒ ① 的比较面无法分辨 cell 约定，需复核构造")
        H.is_true(rel_w_badref > _base_tol.F64_RTOL * 1.0e3,
                  f"牙齿见证②（配错 F_ref,k）只越界 {rel_w_badref / _base_tol.F64_RTOL:.3g}× "
                  "⇒ ② 无法分辨 F_ref 配对，需复核构造")
        H.is_true(rel_w_framesnr > _base_tol.F64_RTOL * 1.0e3,
                  f"牙齿见证③（额外乘帧级 SNR，weight_chain.h:389-391 逐字禁止）只越界 "
                  f"{rel_w_framesnr / _base_tol.F64_RTOL:.3g}× ⇒ ② 无法分辨该禁令，需复核构造")
        ev.record("牙齿见证① oracle 用角点约定时的层值相对差", rel_layer_corner,
                  _base_tol.F64_RTOL,
                  f"超界 {rel_layer_corner / _base_tol.F64_RTOL:.3g}×（注入 ⇒ ① 必红）")
        ev.record("牙齿见证② 配错 F_ref,k（邻帧 k_photo=2.8）时的 w 相对差", rel_w_badref,
                  _base_tol.F64_RTOL,
                  f"解析预期 = (2.8/1.0)² = 7.84 减 1 = 6.84；"
                  f"超界 {rel_w_badref / _base_tol.F64_RTOL:.3g}×（注入 ⇒ ② 必红）")
        ev.record("牙齿见证③ 额外乘帧级 SNR 时的 w 相对差", rel_w_framesnr,
                  _base_tol.F64_RTOL,
                  f"解析预期 = SNR_frame² = {snr_frame_forbidden ** 2:.4g} − 1；"
                  f"超界 {rel_w_framesnr / _base_tol.F64_RTOL:.3g}×（注入 ⇒ ② 必红）")
        # ④ 诊断项：跨算子差 —— 只记录，不判红
        ev.record("【诊断项·不判红】跨算子 max|w_spline − w_双线性| / max|w|",
                  rel_cross_op, None, "",
                  "两条**不同**算子的层值本就不同（P4-b 的 `synth.p4.operator_sep_min_rel` "
                  "是它的真判据）；拿它们做「一致性」对照在构造上不可能绿，"
                  "故本条不对它做 pass/fail 断言")
        ev.record("【诊断项·不判红】spline 与独立双线性逐位同值的像元占比",
                  frac_same, None, "",
                  "旧版判据的缺陷面：只取这些像元（对抗复核在旧夹具上实测 "
                  "232/841 = 27.6%）做「逐位相同」断言 ⇒ 同表达式作用于逐位相同输入 "
                  "⇒ **必然恒真**；本版比较面改为**全域**，且断言用的是有意义的 "
                  "f64 容差而非逐位相等")
        ev.record("层值尺度 scale（无量纲 SNR）", scale_l)
        ev.record("权重尺度 scale（ADU^-2）", scale_w)


@H.test(
    "p4h_negative_sigma_roundtrip_selfproof",
    intent="把 `σ_F := F_ref/SNR_layer` 当参照量去检验 `w = SNR²/F_ref²` 是"
           "**取逆自证**：残差恒为浮点舍入，任何科学错误都不会让它动",
    inputs="层值故意取 binary64 不可精确表示的 1/3、5/3、101/17；"
           "`F_ref,k = 1234.5`；判据式 `w·F_ref² − SNR²·g²`，"
           "参照量 `σ_F := F_ref/SNR_layer` 逐字取自 `weight_chain.h:435-438` 点名的反模式",
    expected="往返残差 ≤ f64 非归约档（恒真）⇒ **该判据无证据资格**，"
             "负例断言「它没有判别力」而不是断言某个数值",
    source="正本 `weight_chain.h:435-438` 逐字「`σ_F := F_ref/SNR_layer` 是把被检验公式"
           "**取逆**得来的，用它当参照量只回到同一个式子」+ 同处逐字列举原 "
           "`dimensional_identity` 被删的理由（「它是代数恒等式型自证：……任何科学错误"
           "（算子选错、层语义错、`F_ref` 配对错、gain 用错）都**不会**让它动」）",
    kind=H.NEGATIVE,
    inject="把 `w = (SNR_layer/F_ref,k)²·g_k²` 的参照量取成 `σ_F := F_ref/SNR_layer`——"
           "判据式与参照量同源，差只剩舍入",
    defect_id="P4-H-SIGMA-ROUNDTRIP",
)
def p4h_negative_sigma_roundtrip_selfproof():
    f_ref, g_k = 1234.5, 1.0
    residuals = []
    for v in CTRL_PROFILE:
        snr = float(v)
        w = (snr / f_ref) ** 2 * g_k ** 2
        sigma_f = f_ref / snr                 # ← 取逆得来的「参照量」
        residuals.append(abs(w * f_ref ** 2 - snr ** 2 * g_k ** 2))
    worst = max(residuals)
    scale = max(float(v) ** 2 * g_k ** 2 for v in CTRL_PROFILE)
    rel = worst / scale
    with H.evidence() as ev:
        ev.record("roundtrip_residual_max_abs", worst, 0.0, "",
                  "取逆自证的实测残差")
        ev.record("roundtrip_residual_rel", rel, _base_tol.F64_RTOL, "",
                  "远在门内 ⇒ 判据对任何科学错误都不动")
        H.less_equal(rel, _base_tol.F64_RTOL,
                     "往返自证判据出现了非舍入残差（需复核判据构造）")
        # 判别力自证：注入一个**真**错误（层语义错：拿错误的 F_ref 配对），
        # 往返判据仍然不动 ⇒ 无判别力
        wrong_f_ref = f_ref * 1.0e-3
        w_wrong = (snr / wrong_f_ref) ** 2 * g_k ** 2
        sigma_f2 = wrong_f_ref / snr
        injected_residual = abs(w_wrong * wrong_f_ref ** 2 - snr ** 2 * g_k ** 2)
        rel2 = injected_residual / max(float(v) ** 2 for v in CTRL_PROFILE)
        ev.record("injected_F_ref_mismatch.roundtrip_residual_rel", rel2,
                  _base_tol.F64_RTOL, "",
                  "F_ref 配对错 1e-3 倍，往返判据**仍不动** ⇒ 无判别力")
        H.less_equal(rel2, _base_tol.F64_RTOL,
                     "取逆自证判据竟然对 F_ref 配对错敏感（与正本不符，需复核）")


# ===========================================================================
# §K P4-i 返回预测方差 / 零尺度分支
# ===========================================================================


@H.test(
    "p4i_control_variance_formula",
    intent="控制点方差按 `k_corr·(π/2)·σ_bg²/N_retained` 发布；"
           "`k_corr` 域 `1 < k_corr` 显式拒（k = 1 与 k < 1 一律拒）",
    inputs="σ_bg = 12.0（电子/像元）、N_retained = 65、k_corr = 1.4（代码默认冻结值）；"
           "另测 k_corr = 1.0 与 k_corr = 0.9 的域外输入",
    expected="`control_variance = k_corr·(π/2)·σ_bg²/N_retained` 到 f64 非归约档；"
             "`control_ivar = 1/variance`；k_corr ≤ 1 抛 `ReconstructError`（显式拒，"
             "**不 clamp**）",
    source="闭式解析 + 正本 `PHASE2_UPM.md` §5 逐字 `control_variance = k_corr × (π/2) × "
           "σ_bg² / N_retained`、`control_ivar = 1 / control_variance`、"
           "「**1 < k_corr**（k_corr=1 与 k_corr<1 一律显式拒，§4）」；"
           "`NOISE_SNR.md` §「控制点估计量的方差」逐字给出同一式与 Serfling 1980 出处",
)
def p4i_control_variance_formula():
    sigma_bg, n_ret, k = 12.0, 65, 1.4
    ivar, var = control_variance(sigma_bg, n_ret, k)
    expect = k * (math.pi / 2.0) * sigma_bg ** 2 / n_ret
    with H.evidence() as ev:
        ev.record("control_variance", var, expect, "", "闭式对拍")
        H.close(var, expect, rtol=_base_tol.F64_RTOL, atol=0.0,
                what="control_variance 闭式")
        ev.record("control_ivar", ivar, 1.0 / expect, "", "")
        H.close(ivar, 1.0 / expect, rtol=_base_tol.F64_RTOL, atol=0.0,
                what="control_ivar = 1/variance")
        for k_bad in (1.0, 0.9, -1.0):
            ev.record(f"k_corr={k_bad}", "ReconstructError", None, "", "域外显式拒")
            H.raises(ReconstructError, lambda kb=k_bad: control_variance(sigma_bg, n_ret, kb),
                     f"k_corr={k_bad} 必须显式拒")


@H.test(
    "p4i_negative_zero_scale_numerical_protection",
    intent="`σ_bg_raw = 0` 时 `control_ivar ≡ 0` 且 `control_variance` **非有限**；"
           "以数值保护量生成有限方差发布是**被禁**的缺陷面",
    inputs="σ_bg_raw = 0（patch 内 ≥ 半数像素同值）、N_retained = 65、k_corr = 1.4；"
           "对照实现 `control_variance_protected` 用 `σ² := 1e-300` 兜底",
    expected="正确分支：`control_ivar == 0`（逐位）且 `control_variance` 非有限；"
             "注入分支给出**有限且极大**的方差（`ivar ≈ 1e-300·N/...`），"
             "其相对发布权重与真值的比值发散 ⇒ 判据判红",
    source="正本 `PHASE2_UPM.md` §5 逐字「`σ_bg_raw = 0`（patch 内 ≥ 半数像素同值）⇒ "
           "无尺度信息：control_ivar 必须为 0，**禁止以数值保护量生成有限方差发布**」+ "
           "`PHASE2_SAMPLER.md` §F5(b) 逐字「σ_bg_raw=0 时断言 `control_ivar == 0` 且 "
           "control_variance 非有限」",
    kind=H.NEGATIVE,
    inject="零尺度分支用 `max(σ_bg_raw², 1e-300)` 兜底发布一个有限方差——"
           "读数「有值」、方差「有限」，但该控制点其实**没有任何尺度信息**",
    defect_id="P4-I-ZERO-SCALE-NUMERIC-GUARD",
)
def p4i_negative_zero_scale_numerical_protection():
    n_ret, k = 65, 1.4
    ivar_ok, var_ok = control_variance(0.0, n_ret, k)
    ivar_bad, var_bad = control_variance_protected(0.0, n_ret, k)
    with H.evidence() as ev:
        ev.record("correct.control_ivar", ivar_ok, 0.0, "", "必须逐位为 0")
        H.exact(ivar_ok, 0.0, "零尺度分支的 control_ivar")
        ev.record("correct.control_variance", var_ok, None, "", "必须非有限")
        H.is_true(math.isinf(var_ok), "零尺度分支的 control_variance 是有限值")
        ev.record("injected.control_variance", var_bad, None, "",
                  "注入：数值保护量给出的有限方差（应为非有限）")
        ev.record("injected.control_ivar", ivar_bad, 0.0, "",
                  "注入：非零逆方差 ⇒ 该控制点被当成有尺度信息")
        H.is_true(math.isfinite(var_bad), "注入分支未给出有限方差（注入失效）")
        H.is_false(ivar_bad == 0.0, "注入分支的 control_ivar 恰为 0（注入失效）")
        # 后果：零尺度控制点在权重里被当成「极精确」，相对发布权重发散
        ivar_ref, _ = control_variance(12.0, n_ret, k)
        ev.record("injected.weight_ratio_vs_sigma12", ivar_bad / ivar_ref, 0.0, "",
                  "发散 ⇒ 无尺度信息的控制点支配权重")


# ===========================================================================
# §L P4-j 控制点方差：定种子蒙特卡洛（只能走 MC）
# ===========================================================================


def _kappa_mc(n: int, n_reps: int, n_batches: int = 20, seed: int = 20260928):
    """`κ(N) = N·Var(median)/σ²` 的定种子蒙特卡洛估计。

    σ = 1 已知 ⇒ `E[median²] = Var(median)`（中位数均值为 0），故逐次重复量
    `N·median²` 是 κ 的单次估计，其 R 次均值即 κ̂；R 个 iid 估计的**均值**相对标准差
    收敛到 `√(2/R)`（与样本方差同族的论证，Higham 2002 §4.2 前向误差模型的抽样对应）。

    ⚠ **区间必须取在「κ̂ 的重复分布」上，不是取在逐次重复量上**：逐次重复量
    `N·median²` 渐近服从 `χ²₁`（强右偏），对它直接取 2.5%/97.5% 分位数得到的是
    `χ²₁` 的分位数区间，会比 κ 的区间宽一个数量级以上。本函数把 R 次重复排成
    `n_batches × (R/n_batches)` 的批矩阵、取**批均值**作为 κ̂ 的重复分布，再按
    `tolerances.py` §2 的分位数法给 95% 区间。

    返回 `(κ̂, 分位数 95% 相对半宽, κ̂ 的相对标准误)`。
    """
    rng = np.random.default_rng(seed)
    x = rng.normal(0.0, 1.0, size=(n_reps, n))
    per_rep = n * np.median(x, axis=1) ** 2
    kappa_hat = float(np.mean(per_rep))
    batch = per_rep.reshape(n_batches, n_reps // n_batches).mean(axis=1)
    lo, hi = np.quantile(batch, [0.025, 0.975])
    half_width_rel = float((hi - lo) / 2.0 / kappa_hat)
    se_rel = float(np.std(batch, ddof=1) / math.sqrt(n_batches) / kappa_hat)
    return kappa_hat, half_width_rel, se_rel


@H.test(
    "p4j_control_variance_mc_kappa",
    intent="`Var(median)` 只能走**定种子蒙特卡洛**（`PHASE2_SAMPLER.md` F3 明写"
           "解析式自比恒真）；对拍正本的有限 N 修正表 κ(N)，并判**方向**："
           "渐近式 `π/2` 恒为高估",
    inputs="σ = 1 已知、分布高斯（正本「σ 已知纯公式口径」）；"
           "N ∈ {5, 17, 65, 289}（奇）；R = 4000 次定种子独立重复（seed = 20260928）；"
           "统计量 = N·Var(median)/σ²（逐次重复量 = N·median²，R 次重复的均值即 κ̂）；"
           "区间按 `tolerances.py` §2 的**分位数法**：把 R 次重复排成 20×200 的批矩阵，"
           "取 20 个**批均值**作为统计量的重复分布，用 `numpy.quantile` 给 95% 区间",
    expected="① 逐档 `|κ_MC − κ_doc| / κ_doc` ≤ 4.38316e-2"
             "（synth.p4.kappa_mc_ci95_rel = z₉₇.₅·√(2/(R−1))）；"
             "② **方向**：四个 N 档上 `κ_MC < π/2`；"
             "③ N = 5 处 `κ_MC ≤ (π/2)(1 − 5e-2)`（synth.p4.kappa_gap_min_n5）",
    source="定种子蒙特卡洛（统计量）+ 正本条款（期望值）："
           "`PHASE2_UPM.md` §5 逐字「κ(N)=N·Var(median)/σ² = 1.4342 (N=5) / 1.5308 (17) / "
           "1.5604 (65) / 1.5685 (289)，即公式高估 +9.5% (N=5) / +2.6% (17) / +0.7% (65) / "
           "<0.4% (≥129)，|偏差| <1% ⟺ N≥49（奇）；**方向 = 发布方差偏大 ⇒ control_ivar "
           "偏小 ⇒ 权重偏保守**」；渐近式 `Var(median)=πσ²/(2N)` 的出处 Serfling 1980 §2.3.2"
           "（ISBN 0-471-02403-1 / DOI 10.1002/9780470316481）；"
           "`PHASE2_SAMPLER.md` §F3 逐字「与 MC 实测 Var(median) 比对……统计判据必须显式"
           "声明 N 与分布」（本条显式给出 N 与分布）",
)
def p4j_control_variance_mc_kappa():
    doc = TB.get("synth.p4.kappa_doc_table").value
    ci_rel = TB.get("synth.p4.kappa_mc_ci95_rel").value
    gap_min = TB.get("synth.p4.kappa_gap_min_n5").value
    half_pi = math.pi / 2.0
    n_reps = 4000
    with H.evidence() as ev:
        ev.record("R_replicates", float(n_reps), 4000.0, "",
                  "定种子；分位数法区间")
        for n in (5, 17, 65, 289):
            k_mc, half_width_rel, se_rel = _kappa_mc(n, n_reps, n_batches=20)
            k_doc = float(doc[n])
            rel_gap = (k_mc - k_doc) / k_doc
            ev.record(f"N={n:3d}.kappa_MC", k_mc, k_doc, "",
                      f"批均值分位数 95% 半宽(相对) = {half_width_rel:.4f}，"
                      f"标准误(相对) = {se_rel:.5f}")
            ev.record(f"N={n:3d}.rel_gap_to_doc", rel_gap, ci_rel, "",
                      "对拍正本表")
            H.less_equal(abs(rel_gap), ci_rel,
                         f"N={n}: κ_MC 与正本表的相对偏差超出 MC 区间")
            # 区间口径自检：批均值的标准误应与冻结推导 √(2/(R−1)) 同量级
            se_theory = math.sqrt(2.0 / (n_reps - 1))
            ev.record(f"N={n:3d}.se_rel_vs_theory", (se_rel - se_theory) / se_theory,
                      0.25, "", "√(2/(R−1)) 口径自检")
            H.less_equal(abs(se_rel - se_theory) / se_theory, 0.5,
                         f"N={n}: 批均值标准误与冻结推导 √(2/(R−1)) 脱钩")
            ev.record(f"N={n:3d}.kappa_below_pi_over_2", k_mc < half_pi,
                      True, "", "方向：渐近式恒为高估")
            H.is_true(k_mc < half_pi,
                      f"N={n}: κ_MC={k_mc:.4f} ≥ π/2 ⇒ 「渐近式恒为高估」不成立")
        k5, _, _ = _kappa_mc(5, n_reps, n_batches=20)
        ev.record("N=5.kappa_gap_below_pi_over_2", (half_pi - k5) / half_pi, gap_min, "",
                  "锐化端：渐近式在 N=5 处高估远超任何实现误差")
        H.is_true((half_pi - k5) / half_pi >= gap_min,
                  "N=5 处 κ_MC 离 π/2 太近 ⇒ 渐近式高估这条方向判据失去判别力")


@H.test(
    "p4j_negative_asymptotic_formula_as_truth",
    intent="把渐近式 `πσ²/(2N)` 当作**真值**发布的实现，在 N=5 处判红——"
           "方向判据的判别力自证",
    inputs="同 P4-j 的 MC 夹具；「被测实现」= 直接输出渐近式 `N·Var = π/2`（σ = 1）",
    expected="该实现在 N = 5 处的相对偏差 = (π/2 − κ(5))/(π/2) = 8.70% ≥ 5e-2 门限 ⇒ 判红；"
             "⇒ 判据能抓住「拿渐近式当真值」的实现",
    source="闭式解析（`(π/2 − 1.4342)/(π/2) = 8.70%`，正本 κ 表与 π/2 的纯算术）+ "
           "正本 `PHASE2_UPM.md` §5 逐字「σ 已知的纯公式口径下渐近式对真方差**恒为高估**」"
           "+ `PHASE2_SAMPLER.md` §F3(b) 逐字「(a) 解析复算 rtol 1e-12（**实测恒真**）」"
           "——本条把那条恒真的解析复算换成有判别力的 MC 对拍",
    kind=H.NEGATIVE,
    inject="控制点方差的发布公式退化成渐近式 `πσ²/(2N)`（丢掉 Serfling 有限 N 修正）——"
           "这是产品实现里最常见的省事写法，且 (a) 档解析复算判据对它**恒真**",
    defect_id="P4-J-ASYMPTOTIC-AS-TRUTH",
)
def p4j_negative_asymptotic_formula_as_truth():
    doc = TB.get("synth.p4.kappa_doc_table").value
    gap_min = TB.get("synth.p4.kappa_gap_min_n5").value
    half_pi = math.pi / 2.0
    published = half_pi                     # 渐近式发布的 N·Var(median)/σ²
    true_n5 = float(doc[5])
    rel = (published - true_n5) / true_n5
    with H.evidence() as ev:
        ev.record("published.kappa (asymptotic)", published, half_pi, "",
                  "渐近式发布值")
        ev.record("true.kappa (N=5)", true_n5, published, "", "正本表真值")
        ev.record("relative_bias_at_N5", rel, gap_min, "",
                  "正本逐字「公式高估 +9.5% (N=5)」的方向，幅度按 κ 表复算为 8.70%")
        H.is_true(rel >= gap_min,
                  f"渐近式在 N=5 处的相对偏差 {rel:.4f} 未达门限 {gap_min} ⇒ 无判别力")
        ev.record("discr_ratio_vs_doc_gap", rel / gap_min, 1.0, "",
                  "超界倍数")


if __name__ == "__main__":  # pragma: no cover - 人工查阅入口
    for c in H.registered():
        print(f"[{c.kind:8s}] {c.id:52s} {c.intent[:60]}")
    print(f"\n共 {len(H.registered())} 条")
