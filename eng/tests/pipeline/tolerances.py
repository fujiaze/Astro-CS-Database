"""阶段管线层容差冻结表（**唯一源**）。

冻结发生在写任何用例之前（`docs/engineering/testing/TEST.md` §3/§4）。
本层只冻结阶段管线需要的档：精确一致档、f64/f32 非归约档、归约档、
阶段产物级门限（manifest 计数、接缝/覆盖语义）、性能六维记录档。
科学口径判据的数值一律引用各层正本，不在本层另立数值。

同构说明：载体 `Frozen(key/value/scale_domain/source/note)` 与 `get()` 的
fail-closed 语义同 unit/synthetic 层，但表对象独立——管线层的门限有管线层
自己的来源（阶段合同、manifest 语义、性能探针口径），跨层复制会让冻结时点
不可追。
"""

from __future__ import annotations

import math
import sys

# ---------------------------------------------------------------------------
# §1 通用档（来源：docs/engineering/testing/TEST.md §4，与 unit 层逐字同值）
# ---------------------------------------------------------------------------

#: `TEST.md` §4「双精度非归约」档
F64_RTOL = 1e-12
#: `TEST.md` §4「双精度非归约」档，atol = 1e-13 × scale
F64_ATOL_PER_SCALE = 1e-13

#: `TEST.md` §4「单精度产品非归约」档
F32_RTOL = 5e-6
F32_ATOL_PER_SCALE = 1e-6

#: `TEST.md` §4.1，IEEE 754 binary64 unit roundoff = 2⁻⁵³
U_F64 = 2.0 ** -53
#: `TEST.md` §4.1，IEEE 754 binary32 unit roundoff = 2⁻²⁴
U_F32 = 2.0 ** -24

#: `TEST.md` §4「`C ≤ 4` 事前冻结」；本层取上界（阶段内归约项数 ≤ 1e6，无收紧结构）。
REDUCTION_C = 4

#: `TEST.md` §4「元数据、掩膜、计数、索引、端口、选择结果」= 精确一致
EXACT = 0.0


def ulp(scale: float, u: float = U_F64) -> float:
    """`scale` 处的一个 ulp 上界 = `scale · u`（`TEST.md` §4.3 可满足性下限用）。"""
    return abs(scale) * u


def reduction_tolerance(n_terms: int, sum_abs_terms: float,
                        u: float = U_F64, c: int = REDUCTION_C,
                        atol: float = 0.0) -> float:
    """`TEST.md` §4「归约」档门限：`C·γ_n·Σ|terms| + atol`（Higham 2002 §4.2）。"""
    if n_terms < 0:
        raise ValueError("n_terms 必须 ≥ 0")
    gamma_n = (n_terms * u) / (1.0 - n_terms * u)
    return c * gamma_n * abs(sum_abs_terms) + atol


class Frozen:
    """一条冻结容差：值 + 适用量级域 + 来源。"""

    __slots__ = ("key", "value", "scale_domain", "source", "note")

    def __init__(self, key: str, value, scale_domain: str, source: str,
                 note: str = "") -> None:
        self.key = key
        self.value = value
        self.scale_domain = scale_domain
        self.source = source
        self.note = note

    def __repr__(self) -> str:  # pragma: no cover - 诊断输出
        return f"<Frozen {self.key}={self.value!r}>"


# ---------------------------------------------------------------------------
# §2 阶段管线门限（阶段合同与 manifest 语义）
# ---------------------------------------------------------------------------

#: 阶段产物 manifest 的计数/索引/掩膜语义：精确一致。
PIPE_MANIFEST_EXACT = Frozen(
    "pipe.manifest_exact", EXACT,
    "整数；帧计数、块计数、support/masked 计数、枚举返回值",
    "正本条款：TEST.md §4 第一档「元数据、掩膜、计数、索引、端口、选择结果 = 精确一致」",
    "阶段管线只判「记了什么」，不判科学值本身；科学值由各 science 正本判。",
)

#: 阶段内顺序不变性：同一输入重复跑阶段，产物逐字节一致（确定性门）。
PIPE_STAGE_DETERMINISTIC = Frozen(
    "pipe.stage_deterministic", EXACT,
    "字节；同一输入、固定 seed 的阶段产物哈希",
    "正本条款：TEST.md §3「固定种子；同一输入重复运行逐字节同结果」",
    "Monte Carlo 随机源必须走注入 seed；阶段不得读未播种的全局随机源。",
)

#: normalize 阶段：单帧标量定标残差（合成面）。保守宽门，非恒真。
PIPE_NORM_SCALE_REL = Frozen(
    "pipe.norm.scale_rel", 5.0e-2,
    "无量纲相对量；|ĉ−c|/c，域 1e-4–1",
    "量级冻结（占位）：上游正本 docs/science/photometry/PHOTOMETRY.md；"
    "同 synthetic 层 synth.p1.linear_scale_rel 同量级，管线层独立冻结",
    "把标度写错一个量级（如漏除增益 g）的实现必然超界；正确实现落在几个百分点内。",
)

#: mosaic 阶段：接缝残余相对量（合成面）。D12 吸收：接缝残余未消失是已知局限，
#: 门限判「残余有界且可归因」，不判「接缝为零」。
PIPE_MOSAIC_SEAM_REL = Frozen(
    "pipe.mosaic.seam_rel", 5.0e-2,
    "无量纲相对量；接缝两侧差相对背景，域 1e-4–1",
    "量级冻结（占位）：上游正本 AGENTS.md §10 P5；T01-D12 接缝残余已知局限",
    "D12：残余未消失 ⇒ 本门是「有界」门；「恒零」写法是无效判据，禁止。",
)

#: export 阶段：平面投影往返残差（解析 Oracle 面）。WCS 仿射部分的闭式界。
PIPE_EXPORT_WCS_REL = Frozen(
    "pipe.export.wcs_rel", 1.0e-9,
    "无量纲相对量；仿射往返相对残差，域 1e-12–1e-3",
    "科学推导：双精度仿射往返的前向误差 ~ n·u（Higham 2002 §4.2），"
    "n ≤ 16 ⇒ 门限取 1e-9（含三个数量级裕量）；TEST.md §4 f64 非归约档",
    "非仿射畸变项不在本门内，由 projection 正本单独判。",
)

#: T10 复现编号占位：CALIB/CCD/STAR/DRZ/PHOT/SMP/EXP/CFG/BLD/ATOM/GUI 十一族的
#: 阶段归属标签（不是数值门）。数值门由各用例引用本表或 science 正本。
PIPE_T10_FAMILIES = Frozen(
    "pipe.t10.families",
    ["T-CALIB", "T-CCD", "T-STAR", "T-DRZ", "T-PHOT",
     "T-SMP", "T-EXP", "T-CFG", "T-BLD", "T-ATOM", "T-GUI"],
    "无量纲；复现编号族标签集合",
    "任务派单：T10 复现编号十一族；本层为其建可执行锚点，编号待 T10 输出对拍",
    "⚠ 族标签是构造的待对拍锚点，不是 T10 已发布结论；对拍前不得引用为证据。",
)

# ---------------------------------------------------------------------------
# §3 性能六维记录档（std/05 §6：CPU/内存/扩展/IO/编排/缓存）
# ---------------------------------------------------------------------------

#: 性能测试是「记录 + 上界」门：每维记录实测读数，门限为合理性上界（非优化目标）。
PIPE_PERF_CPU_UTIL = Frozen(
    "pipe.perf.cpu_util", (0.0, 1.0),
    "无量纲比率；域 [0, 1]，记录均值与峰值",
    "正本条款：05_INDEPENDENT_TEST_SUITE.md §6「记录 CPU 利用率」",
    "门是「记录了」+「落在 [0,1]」；优化目标由 T09 给，不在本层冻结数值目标。",
)
PIPE_PERF_MEM_WS = Frozen(
    "pipe.perf.mem_working_set", "record_bytes",
    "字节；工作集峰值，按阶段记录",
    "正本条款：05 §6「工作集内存」",
    "门是「记录了字节数」；上界由 concrete 配置的内存预算给，用例只断言记录存在。",
)
PIPE_PERF_SCALE_EFF = Frozen(
    "pipe.perf.scaling_efficiency", 0.5,
    "无量纲；双线程相对单线程的加速比下界（效率 ≥ 50%）",
    "量级冻结：Amdahl 上界的保守下界；并行区占比未知时取 0.5 为可达到宽门",
    "加速比 < 1（越并行越慢）必然红；正确并行实现通过。",
)
PIPE_PERF_IO_WAIT = Frozen(
    "pipe.perf.io_wait_frac", 0.5,
    "无量纲比率；I/O 等待占总耗时上界 50%",
    "正本条款：05 §6「I/O 等待」",
    "合成小批量下 I/O 等待超 50% 说明编排或缓存有结构问题，红。",
)
PIPE_PERF_SCHED_GAP = Frozen(
    "pipe.perf.sched_gap_frac", 0.2,
    "无量纲比率；编排空隙（无任务在跑）占总耗时上界 20%",
    "正本条款：05 §6「编排连续性」",
    "空隙超 20% 说明调度有串行段，红。",
)
PIPE_PERF_CACHE_HIT = Frozen(
    "pipe.perf.cache_hit", 0.5,
    "无量纲比率；缓存命中率下界 50%（K2/块缓存面）",
    "正本条款：05 §6「缓存命中」",
    "命中率低于 50% 说明缓存未复用，红；Y2（K2+缓存交付）关联。",
)


FROZEN_TABLE = {f.key: f for f in (
    PIPE_MANIFEST_EXACT, PIPE_STAGE_DETERMINISTIC,
    PIPE_NORM_SCALE_REL, PIPE_MOSAIC_SEAM_REL, PIPE_EXPORT_WCS_REL,
    PIPE_T10_FAMILIES,
    PIPE_PERF_CPU_UTIL, PIPE_PERF_MEM_WS, PIPE_PERF_SCALE_EFF,
    PIPE_PERF_IO_WAIT, PIPE_PERF_SCHED_GAP, PIPE_PERF_CACHE_HIT,
)}


def get(key: str) -> Frozen:
    """按 key 取冻结容差。未知 key 直接抛错——不许在用例里现编容差。"""
    try:
        return FROZEN_TABLE[key]
    except KeyError:
        raise KeyError(
            f"未冻结的容差 key={key!r}；先在 eng/tests/pipeline/tolerances.py 登记"
        ) from None


if __name__ == "__main__":  # pragma: no cover - 人工查阅入口
    for _k, _f in FROZEN_TABLE.items():
        print(f"{_k:36s} = {_f.value!r}\n    域: {_f.scale_domain}\n    源: {_f.source}")
    print(f"\n通用档: f64 rtol={F64_RTOL} atol={F64_ATOL_PER_SCALE}·scale; "
          f"f32 rtol={F32_RTOL} atol={F32_ATOL_PER_SCALE}·scale; C={REDUCTION_C}")
    sys.stdout.flush()
