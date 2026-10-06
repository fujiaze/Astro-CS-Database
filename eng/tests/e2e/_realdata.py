"""端到端层共享工具面：真实数据索引、header 读取、fail-closed 锚守卫、XISF 最小解析与诊断量。

## 用途

`eng/tests/e2e/` 下四个用例模块的**唯一**支撑模块。本模块**不含任何 `test_` 函数**，
只提供四类能力：

1. **fail-closed 锚装载**：把本层硬编码引用的仓内锚（两组真实观测目录、三组标定母版目录、
   `testdata/index.json`、XISF 头）解析出来。任一锚缺失 / 不可读 / 结构缺字段，
   一律抛**具名异常**，**绝不 `pytest.skip`、绝不 `return` 假装通过、绝不 `default=0.0` 充数**。
2. **观测帧清单与独立重算**：读每帧 FITS 头（只读头，`memmap=True`，353 帧实测 0.35 s），
   产出 `(dataset, telescope, panel, exptime, filter, wcs)` 六元组，供 D1–D8 判定。
3. **XISF 1.0 最小解析器**：只解析**文件头**（魔数 + 64 位 XML 长度 + XML 头 + `<Image>` 属性 +
   `<FITSKeyword>`），并按需读回**未压缩附加块**的像素。字节布局不发明，逐条对照仓内
   C++ 读入器 `lib/infrastructure/aio/src/aio_xisf.cpp`。
4. **视觉验收辅助面**：纯 numpy 拉伸（线性 / 对数 / asinh）+ 机器可读诊断量。
   **本模块只产出供人目检的图与诊断读数，不产出任何「合格 / 不合格」判决**（见
   `test_visual_acceptance.py` 与本目录 `README.md` §7）。

## 正本依据（逐条 `文件:行`）

| 本模块用到的量 / 口径 | 来源 |
|---|---|
| e2e 层职责 =「真实数据端到端与视觉验收辅助」 | `run/GOVERN-08/工作包-RECTIFY-09原件/standards/05_INDEPENDENT_TEST_SUITE.md:21` |
| 第三类实验数据 = M42 与银心真实观测，含视觉验收 | 同上 `04_SCIENCE_EVIDENCE_AND_EXPERIMENT.md:32-34`；`docs/ACSD_DESIGN.md:497` |
| L3 小批量端到端判据 | `docs/ACSD_DESIGN.md:532` |
| L4 真实视觉验收判据（负责人目检判定） | `docs/ACSD_DESIGN.md:533`；`docs/ACSD_DESIGN.md:536`；`docs/ACSD_DESIGN.md:558` |
| 容差档（元数据/计数 = 精确一致；f64 非归约 `rtol=1e-12`） | `docs/engineering/testing/TEST.md:46-48` |
| NaN/Inf 位置与语义必须逐项精确一致 | `docs/engineering/testing/TEST.md:75-82` |
| 硬件/构建能力不可用时显式 SKIP，不以伪通过掩盖 | `docs/engineering/testing/TEST.md:60` |
| 每个度量须有非退化判据，恒真比较无证据资格 | `docs/engineering/testing/TEST.md:26` |
| 负例必须可红 | `docs/engineering/testing/TEST.md:62` |
| 锚存活 fail-closed | `docs/engineering/testing/VALIDATION_EVIDENCE.md:412-413` |
| 零对象守卫 | `docs/engineering/testing/VALIDATION_EVIDENCE.md:170-176` |
| 判别力 S2（同时验证绿）/ S6（内容级负例）/ 反例隔离 | `docs/engineering/testing/VALIDATION_EVIDENCE.md:189,193,197` |
| 饱和电平来源优先级（三级皆无 ⇒ 显式 `DISABLED_NO_METADATA`，禁静默） | `eng/contracts/schemas/phase_config_normalize.schema.json#/$defs/noise_config/properties/saturation_level` |
| 三命令唯一命令树 `normalize|mosaic|export --json <config.json>` | `docs/engineering/contracts/CLI_PROTOCOL.md:16,122` |
| 跨阶段唯一载体 = HiPS 产品树（磁盘目录 + manifest + 哈希） | `docs/engineering/contracts/PIPELINE_BLOCK.md:20` |
| 发布决定只属项目负责人 | `AGENTS.md` §11；`docs/ACSD_DESIGN.md:558` |
| XISF 魔数 8 字节 `XISF0100` | `lib/infrastructure/aio/src/aio_xisf.cpp:25,447` |
| XISF 头长为 **64 位小端**、上限 64 MiB | `lib/infrastructure/aio/src/aio_xisf.cpp:29,452-468` |
| XISF `geometry="W:H:C"` / `sampleFormat` / `location="attachment:OFF:SIZE"` 解析 | `lib/infrastructure/aio/src/aio_xisf.cpp:158-181,45-78,91-98` |
| XISF 几何上界 `w,h,c ∈ [1, 65535]` | `lib/infrastructure/aio/src/aio_xisf.cpp:33`（`XISF_MAX_DIM`） |

## 独立 Oracle 声明

- `oracle.truth` = **格式规范 + 仓内一手 C++ 读入器的字节布局**（`aio_xisf.cpp` 逐行对照）+
  **FITS 头自身**；
- `oracle.must_not` = 产品可执行程序 `acsd` / libacsd。本模块**不执行任何产品二进制、
  不起子进程、不链接任何库**。因此
  ⚠ **本模块的 Python XISF 解析结果与产品 `xisf_read_file()` 的像元值未取得逐点一致性证据**；
  该一致性判据登记为 B 类（`test_pipeline_e2e.py::…_XisfReaderMatchesProduct`，需构建）。
- 数据面判据的预期值**不读 `testdata/index.json` 的现值生成**：`index.json` 在本层是
  **被对账对象**（见 REG-06 / REG-07），预期值来自**逐帧 FITS 头的独立实算**。

## ⚠ 与仓内 C++ 读入器的一处**有意分歧**（REG-08）

`aio_xisf.cpp:64` 对未知 `sampleFormat` 的处理是 `aio_log(WARN)` 后 **fallback 到 Float32**
（fail-open）。本模块**不复制该模式**：未知格式一律抛 `XISF_UNSUPPORTED_SAMPLE_FORMAT`
（fail-closed）。仓内 27 个母版全部显式声明 `Float32`，故该分歧在当前数据上**不可观测**；
一旦出现未声明格式的文件，本模块判红而产品会静默按 Float32 解释。登记，不在本层修改产品。

## 不产出阻塞退出码

本模块与同目录用例**不注册进 CMakeLists、不接 CI**，不出现 `sys.exit` /
`raise SystemExit` / `os._exit`。依据 `AGENTS.md` §8「测试集……不与产品代码混放」、
`TEST.md:115`「测试集不产出流水线判决」。判红只由 pytest 的 `AssertionError` /
`Failed` 承载，**由人读对抗性审核消费，本层不裁决代码**。
"""

from __future__ import annotations

import os
import re
import struct
from typing import Any, Dict, List, Optional, Sequence, Tuple

import numpy as np

# ---------------------------------------------------------------------------
# §0 锚点常量（VALIDATION_EVIDENCE.md:413「锚存活」：硬编码路径必须存在）
# ---------------------------------------------------------------------------

#: 仓根。`_realdata.py` 位于 `<repo>/eng/tests/e2e/`，向上四级到仓根。
REPO_ROOT = os.path.dirname(
    os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
)

#: 真实数据根。
TESTDATA_REL = "testdata"
#: 数据集索引（**被对账对象**，不是 Oracle 的输入面）。
INDEX_REL = "testdata/index.json"
#: 三组标定母版目录（`T{n} calibration files`）。
MASTER_DIR_NAMES = ("T2 calibration files", "T3 calibration files", "T4 calibration files")

#: 本层负责的两个数据集（`04_SCIENCE_EVIDENCE_AND_EXPERIMENT.md:32-34`「M42 与银心两组」）。
DATASET_IDS = ("m42", "galaxy_center")

#: 数据集 id → 仓内目录名。**硬编码引用**（锚存活守卫覆盖）。
DATASET_DIR = {
    "m42": "testdata/M42_T2T3_mosaic_Flying_dutchman",
    "galaxy_center": "testdata/Galaxy_Center_T4",
}

#: 数据集 id → 观测所用望远镜代号集合（由逐帧文件名的 `_T{n}_` 段实算，见 `REG-03`）。
DATASET_TELESCOPES = {
    "m42": frozenset({"T2", "T3"}),
    "galaxy_center": frozenset({"T4"}),
}

#: 数据集 id → 标定母版目录名。**硬编码引用**。
DATASET_MASTER_DIR = {
    "m42": ("T2 calibration files", "T3 calibration files"),
    "galaxy_center": ("T4 calibration files",),
}

#: 容差档引用（`TEST.md:46` 第一档）。本层全部比较对象是**元数据、计数、索引、选择结果**，
#: 相等判据即 `==`，故 `EXACT_TOLERANCE = 0.0`，无 `rtol`/`atol`、无适用量级域 `scale`。
EXACT_TOLERANCE = 0.0

#: WCS 往返闭合容差（**事前冻结**，依据见下）。
#:
#: **本判据要抓的不是「精度够不够」，而是「WCS 是不是单射」**——奇异 CD、零行列式、
#: 投影退化这类结构性缺陷会让往返误差发散或出现不可闭合。
#:
#: **取值依据**：取 `1e-4` 像元 = 像元尺度的**万分之一**。该量在任何物理后果上达不到
#: （亚像素定位的典型需求是 0.1 像元，比它宽 3 个数量级）；实测库噪声底为
#: **6.44e-6 像元**（`galaxy_center` 的 TAN+SIP 路径实测最大值），留 **15× 余量**给
#: wcslib 版本漂移。`m42` 的纯 TAN 路径实测中位数 1.21e-10 像元，余量 8 个数量级。
#:
#: ⚠ **不随数据调整**：阈值写在常量里并附本段依据；负例 **N-W1** 用「注入奇异 CD」
#: 证明该判据对目标失效模式**有牙**，故其可信度锚在注入上、不锚在这个数字上。
#: 改阈值属 `TEST.md:36`「容差调整单独提交，附失败分布与推导」的动作。
WCS_ROUNDTRIP_MAX_PX = 1e-4
#: `TEST.md:48`「双精度非归约」相对档。**只作为报告里的构成项记入，不单独设门**
#: （本判据的可满足性由上面的绝对阈值与 N-W1 注入负例共同锚定）。
WCS_RTOL = 1e-12

# ---------------------------------------------------------------------------
# §1 冻结登记（REG-01…REG-08）
#
# 以下每一项都是**对本仓真实数据的实测读数**，写用例前冻结。它们的用途是
# **漂移守卫**：磁盘数据一旦变化（补了母版、加了 WCS、换了一批帧），对应的守卫判红，
# 强制复核方重新裁定登记，而不是让缺陷悄悄通过或悄悄把判据放宽。
# 依据 `VALIDATION_EVIDENCE.md:197`（反例隔离）与模块层「登记项漂移守卫」体例。
# ---------------------------------------------------------------------------

#: REG-D1 帧集合指纹。逐帧 FITS 头独立实算（HEAD `fcc0980b`）。
#: `naxis` 为 `(NAXIS1, NAXIS2)`；`exptime_s` / `filter` 为头值计数。
REGISTERED_FRAME_CENSUS: Dict[str, Dict[str, Any]] = {
    "m42": {
        "n_frames": 196,
        "naxis": (4096, 4096),
        "bitpix": 16,
        "exptime_s_counts": {300.0: 147, 600.0: 49},
        "filter_counts": {"H-alpha": 50, "Blue": 48, "Green": 49, "Red": 49},
        "n_without_wcs": 30,
        "n_with_wcs": 166,
    },
    "galaxy_center": {
        "n_frames": 157,
        "naxis": (4500, 3600),
        "bitpix": 16,
        "exptime_s_counts": {180.0: 98, 300.0: 26, 600.0: 33},
        "filter_counts": {"Blue": 32, "Green": 34, "H-alpha": 26, "Oiii": 33, "Red": 32},
        "n_without_wcs": 1,
        "n_with_wcs": 156,
    },
}

#: REG-C1 标定母版指纹。27 个 `.xisf`（T2/T3/T4 各 9），XISF 1.0、`Float32`、
#: 未压缩附加块、T2/T3 几何 4096×4096×1、T4 几何 4500×3600×1。
REGISTERED_MASTER_CENSUS: Dict[str, Dict[str, Any]] = {
    "T2 calibration files": {"n_masters": 9, "geometry": (4096, 4096, 1)},
    "T3 calibration files": {"n_masters": 9, "geometry": (4096, 4096, 1)},
    "T4 calibration files": {"n_masters": 9, "geometry": (4500, 3600, 1)},
}

#: REG-D1b `CD` / `CDELT` 矛盾帧数登记（**当前是真实数据缺陷的读数**，见 REG-01）。
#: `m42` 的 166 个有 WCS 帧**全部**命中；`galaxy_center` 无 `CDELT1`，命中 0 帧。
REGISTERED_CD_CDELT_CONFLICT_FRAMES: Dict[str, int] = {"m42": 166, "galaxy_center": 0}

#: REG-D2 畸变解关键词面（**当前两套互不相同的畸变口径**，见 README REG-01b）。
#: FITS 标准里畸变有两条互不兼容的承载：`PV*_i_jm`（老式不规则畸变）与
#: `A_i_j / B_i_j`（SIP，须由 `CTYPE` 末尾 `-SIP` 声明）。
#: 实测：`m42` 的 166 个有 WCS 帧**只有 `TR1_* / TR2_*`**——既不是 `PV*`、也没有
#: `A_* / B_*`，且 `CTYPE1 = 'RA---TAN'` 未声明 `-SIP`
#: ⇒ **M42 的畸变解无法被任何 FITS 标准读取器套用**，标准读法下它退化为纯 TAN。
#: `galaxy_center` 的 156 个有 WCS 帧是标准 SIP：`A_/B_/AP_/BP_` 齐备且 `CTYPE1` 带 `-SIP`。
#: 这也解释了实测往返误差的两个量级（M42 1.2e-10 px vs 银心 2.4e-7 px）。
REGISTERED_DISTORTION_FACE: Dict[str, Dict[str, Any]] = {
    "m42": {
        "n_with_wcs": 166,
        "ctype1": "RA---TAN",
        "declares_sip": False,
        "has_pv_coeffs": False,
        "has_sip_ab_coeffs": False,
        "nonstandard_prefixes": ["TR1_", "TR2_"],
    },
    "galaxy_center": {
        "n_with_wcs": 156,
        "ctype1": "RA---TAN-SIP",
        "declares_sip": True,
        "has_pv_coeffs": False,
        "has_sip_ab_coeffs": True,
        "nonstandard_prefixes": [],
    },
}

#: V2 视觉往返判据的**余量条**（越界像元比例上限）。1:1 栅格化在图边界存在亚像元效应；
#: 实测 46/1048576 = 4.4e-5，取 1e-3 留 20× 余量。
#: ⚠ 这**不是**承重条：承重条是「往返偏差中位数 == 0」，任何全局变换（gamma / LUT /
#: 再次归一化）都先破中位数条。见 `test_visual_acceptance.py` 的 V2。
PNG_EDGE_FRACTION_MAX = 1e-3

#: 畸变系数关键词前缀（用于实算计数面）。
DISTORTION_KEYWORD_PREFIXES = ("PV1_", "PV2_", "A_", "B_", "AP_", "BP_", "TR1_", "TR2_")

#: D9 `CD` / `CDELT` 矛盾判定的比值门限。FITS WCS Papers II 把两者定为**冗余表示**，
#: 一致时比值应为 1；实测 `m42` 的比值落在 [0.00372, 0.0208]（差 48–269 倍）。
#: 门限 `0.1` 表示「偏离 1 超过一个数量级即判矛盾」——与数据无关的**量级**判据，
#: 不是照着实测分布卡的。
CD_CDELT_CONFLICT_RATIO = 0.1

#: REG-C2 母版暗场曝光覆盖登记（**当前存在真实缺口**，见 REG-02）。
#: 母版目录 → 可用 masterDark 曝光秒数集合。
REGISTERED_MASTER_DARK_EXPOSURES: Dict[str, frozenset] = {
    "T2 calibration files": frozenset({600.0, 1200.0, 1800.0}),
    "T3 calibration files": frozenset({600.0, 1200.0}),
    "T4 calibration files": frozenset({180.0, 300.0, 600.0}),
}

#: REG-C3 母版平场滤镜覆盖登记（**当前无缺口**，可写成绿的正例判据 C5）。
#: 母版目录 → 可用 masterFlat 的 FILTER 值集合。
REGISTERED_MASTER_FLAT_FILTERS: Dict[str, frozenset] = {
    "T2 calibration files": frozenset({"Blue", "Green", "H-alpha", "OIII", "Red"}),
    "T3 calibration files": frozenset({"Blue", "Green", "H-alpha", "Lum", "Oiii", "Red"}),
    "T4 calibration files": frozenset({"Blue", "Green", "H-alpha", "Oiii", "Red"}),
}

#: REG-02 暗场曝光缺口签名。逐帧实算 ⇒ `{数据集: {望远镜: {曝光秒: 未被覆盖帧数}}}`。
#: 当前实测：`{m42: {T2: {300.0: 53}, T3: {300.0: 94}}}`（合计 147 帧），
#: `galaxy_center` 无缺口。M42 的 300 s 档没有任何同望远镜 masterDark 可用。
REGISTERED_DARK_EXPOSURE_GAPS: Dict[str, Dict[str, Dict[float, int]]] = {
    "m42": {"T2": {300.0: 53}, "T3": {300.0: 94}},
    "galaxy_center": {},
}

#: REG-03 电子学量（增益 / 读噪 / 饱和）关键词。逐帧 + 逐母版实算：**全部 353 帧与
#: 27 个母版均不携带其中任何一个**。任一关键词日后出现 ⇒ C6 漂移守卫判红。
ELECTRONICS_ABSENT_KEYWORDS = (
    "GAIN", "EGAIN", "READNOISE", "RDNOISE", "SATURATE", "DATAMAX",
)

#: REG-04 平场滤镜大小写分歧。母版 FILTER 值与亮场 FILTER 值逐字符比对：
#: `T2` 的 OIII 母版写 `'OIII'`，`T3`/`T4` 写 `'Oiii'`；亮场写 `'Oiii'`。
#: 本层**不做大小写归一**（避免把两套写法悄悄合并），只在 C5 里按**精确值**匹配，
#: 该分歧因此是一处**已登记的口径差异**，见 README REG-03。
#: 亮场实际使用的 FILTER 值集合（决定哪些平场真的需要）。
REGISTERED_LIGHT_FILTERS: Dict[str, frozenset] = {
    "m42": frozenset({"H-alpha", "Blue", "Green", "Red"}),
    "galaxy_center": frozenset({"Blue", "Green", "H-alpha", "Oiii", "Red"}),
}

#: REG-05 testdata 内**零**参考图（PNG/JPG/JPEG/PDF/TIF/TIFF 实测计数 = 0）。
#: 故「视觉验收」没有任何现成基准可比 ⇒ 本层只产出**供人目检**的图，不产出比对判据。
#: 有人补进参考图时本守卫判红，强制复核方决定是否改为比对口径。
REFERENCE_IMAGE_SUFFIXES = (".png", ".jpg", ".jpeg", ".pdf", ".tif", ".tiff")

#: REG-06 `testdata/index.json` 对账。`Galaxy_Center_T4.observation_dates` 实测 8 个日期，
#: 索引自陈 7 个（**缺 `2025-08-13`**）。索引在这里是**被对账对象**，故登记其漂移。
REGISTERED_INDEX_DATASET_ID = "Galaxy_Center_T4"
#: 索引自陈的日期集合（逐字转录，用于漂移比对）。
REGISTERED_INDEX_OBSERVATION_DATES = (
    "2025-07-02", "2025-07-03", "2025-07-04",
    "2025-07-16", "2025-07-17", "2025-07-18", "2025-07-19",
)
#: 磁盘实测的日期集合（逐帧 `DATE-OBS` 前 10 字独立实算）。
REGISTERED_DISK_OBSERVATION_DATES = REGISTERED_INDEX_OBSERVATION_DATES + ("2025-08-13",)

#: REG-06b 索引对账缺口的**冻结签名**。索引缺、磁盘有的日期集合。
#: 实测 `{Galaxy_Center_T4: ['2025-08-13']}`。守卫断言缺口恰为该集合：
#: 数据补齐（缺口消失）或换了别的缺口，都会判红并要求重新裁定。
REGISTERED_INDEX_GAPS: Dict[str, tuple] = {REGISTERED_INDEX_DATASET_ID: ("2025-08-13",)}
#: 索引自陈、磁盘没有的日期集合（实测为空集）。
REGISTERED_INDEX_PHANTOMS: Dict[str, tuple] = {REGISTERED_INDEX_DATASET_ID: ()}

# ---------------------------------------------------------------------------
# §2 具名异常（fail-closed；无一条走 `pytest.skip`）
# ---------------------------------------------------------------------------

class AnchorStale(RuntimeError):
    """锚存活失败（`VALIDATION_EVIDENCE.md:413`）。消息前缀恒为 `ANCHOR_STALE`。"""


class FrameUnparsable(RuntimeError):
    """亮场帧不可读 / 头不可解析 / 必需头字段缺失。前缀恒为 `FRAME_UNPARSABLE` 或
    `HEADER_MISSING_FIELD`。"""


class XisfUnparsable(RuntimeError):
    """XISF 母版不可解析。前缀恒为 `XISF_*`。"""


def _raise_anchor_stale(const_name: str, rel_path: str) -> "AnchorStale":
    return AnchorStale(f"ANCHOR_STALE: {const_name} {rel_path}")


# ---------------------------------------------------------------------------
# §3 路径与文件装载（fail-closed）
# ---------------------------------------------------------------------------

def repo_abs(rel_path: str) -> str:
    """仓内相对路径 → 绝对路径（不做存在性判断）。"""
    return os.path.join(REPO_ROOT, rel_path)


def anchor_file(rel_path: str, const_name: str) -> str:
    """返回锚的绝对路径；不存在或不是普通文件即抛 `ANCHOR_STALE`。"""
    abs_path = repo_abs(rel_path)
    if not os.path.isfile(abs_path):
        raise _raise_anchor_stale(const_name, rel_path)
    return abs_path


def anchor_dir(rel_path: str, const_name: str) -> str:
    """返回锚目录的绝对路径；不存在即抛 `ANCHOR_STALE`。"""
    abs_path = repo_abs(rel_path)
    if not os.path.isdir(abs_path):
        raise _raise_anchor_stale(const_name, rel_path)
    return abs_path


def load_index() -> Dict[str, Any]:
    """读 `testdata/index.json`（**被对账对象**）。不可解析 ⇒ `ANCHOR_UNPARSABLE`。"""
    import json

    abs_path = anchor_file(INDEX_REL, "TESTDATA_INDEX")
    try:
        with open(abs_path, "r", encoding="utf-8") as fh:
            doc = json.load(fh)
    except (json.JSONDecodeError, UnicodeDecodeError) as exc:
        raise AnchorStale(f"ANCHOR_UNPARSABLE: TESTDATA_INDEX {INDEX_REL} :: {exc}") from exc
    if not isinstance(doc, dict) or not isinstance(doc.get("datasets"), list) or not doc["datasets"]:
        raise AnchorStale(f"ANCHOR_BAD_SHAPE: TESTDATA_INDEX {INDEX_REL} :: 缺 datasets 数组")
    return doc


def index_dataset_entry(index: Dict[str, Any], dataset_id_in_index: str) -> Dict[str, Any]:
    """按索引自陈的 `id` 取一条 dataset 条目；不存在即 `ANCHOR_MISSING_FIELD`。"""
    for entry in index["datasets"]:
        if isinstance(entry, dict) and entry.get("id") == dataset_id_in_index:
            return entry
    raise AnchorStale(f"ANCHOR_MISSING_FIELD: TESTDATA_INDEX.datasets[{dataset_id_in_index}]")


# ---------------------------------------------------------------------------
# §4 亮场帧清单与独立实算
# ---------------------------------------------------------------------------

#: 亮场文件名合同（逐字）。实测 353/353 匹配。
#:
#: ```text
#: <object>_<panel>_<telescope>_<mode>-<YYYYMMDD>@<HHMMSS>-<EXPTIME>S-<FILTER>.fts
#: ```
#:
#: 抽不出 `<FILTER>` 之外的歧义：`FILTER` 值可含 `-`（`H-alpha`），但它是**最后一个**捕获组；
#: `<mode>` 允许下划线（实测恒为 `flying_dutchman`）；`<object>` 取非贪婪，由后续各段回溯定型。
FRAME_NAME_RE = re.compile(
    r"^(?P<obj>.+?)"
    r"_(?P<panel>[A-Za-z0-9]+)"
    r"_(?P<tel>T\d+)"
    r"_(?P<mode>[A-Za-z_]+)"
    r"-(?P<date>\d{8})"
    r"@(?P<time>\d{6})"
    r"-(?P<exp>\d+)S"
    r"-(?P<filt>[A-Za-z0-9\-]+)\.fts$"
)

#: 亮场必须携带的头字段（本层判据依赖面）。任一缺失即 `HEADER_MISSING_FIELD`。
REQUIRED_LIGHT_KEYS = ("NAXIS1", "NAXIS2", "BITPIX", "EXPTIME", "FILTER", "DATE-OBS", "OBJECT")

#: XISF 字节布局常量（逐条对照 `aio_xisf.cpp`）。
XISF_MAGIC = b"XISF0100"
#: 头长上限，与 `aio_xisf.cpp:29` `XISF_MAX_XML_HEADER_BYTES` 同值同因。
XISF_MAX_XML_HEADER_BYTES = 64 * 1024 * 1024
#: 几何上界，与 `aio_xisf.cpp:33` `XISF_MAX_DIM` 同值。
XISF_MAX_DIM = 65535
#: `sampleFormat` → `(itemsize, is_float, np.dtype)`。**未列出的格式一律判红**（REG-08）。
XISF_SAMPLE_FORMATS: Dict[str, Tuple[int, bool, Any]] = {
    "UInt8": (1, False, "u1"),
    "Int8": (1, True, "i1"),
    "UInt16": (2, False, "u2"),
    "Int16": (2, True, "i2"),
    "UInt32": (4, False, "u4"),
    "Int32": (4, True, "i4"),
    "UInt64": (8, False, "u8"),
    "Int64": (8, True, "i8"),
    "Float32": (4, True, "f4"),
    "Float64": (8, True, "f8"),
}
#: XISF `byteOrder` 合法取值。缺省按规范取 `little-endian`（`aio_xisf.cpp:172` 空串走
#: 系统序分支；本层显式冻结为小端，因为 27 个母版均为 x86 写出的默认序）。
XISF_BYTE_ORDERS = ("little-endian", "big-endian")


def frame_relpaths(dataset_id: str) -> List[str]:
    """列出某数据集下全部 `.fts` 帧（仓内相对路径，字典序）。

    目录不存在 / 零帧 ⇒ fail-closed：目录抛 `ANCHOR_STALE`，零帧抛 `ZERO_OBJECT_GUARD`。
    """
    if dataset_id not in DATASET_DIR:
        raise AnchorStale(f"ANCHOR_STALE: UNKNOWN_DATASET_ID {dataset_id!r}")
    root_rel = DATASET_DIR[dataset_id]
    anchor_dir(root_rel, f"DATASET_DIR[{dataset_id}]")
    out: List[str] = []
    for dirpath, _dirnames, filenames in os.walk(repo_abs(root_rel)):
        for name in filenames:
            if name.endswith(".fts"):
                out.append(os.path.relpath(os.path.join(dirpath, name), REPO_ROOT))
    out.sort()
    if not out:
        raise AssertionError(
            f"ZERO_OBJECT_GUARD: 数据集 {dataset_id} 在 {root_rel} 下实算帧数为 0"
            "（VALIDATION_EVIDENCE.md:172 第 1 条）；空集必须判红，不得判绿"
        )
    return out


def read_frame_header(rel_path: str):
    """只读一帧 FITS 的主头（`memmap=True`，不读像元）。不可读 ⇒ `FRAME_UNPARSABLE`。"""
    from astropy.io import fits

    abs_path = anchor_file(rel_path, "LIGHT_FRAME")
    try:
        with fits.open(abs_path, memmap=True, lazy_load_hdus=False) as hdul:
            return hdul[0].header.copy()
    except Exception as exc:  # noqa: BLE001 - 一律转具名异常，禁止静默
        raise FrameUnparsable(f"FRAME_UNPARSABLE: {rel_path} :: {exc}") from exc


def require_light_keys(header, rel_path: str, keys: Sequence[str] = REQUIRED_LIGHT_KEYS) -> None:
    """亮场必需头字段守卫；缺任一即 `HEADER_MISSING_FIELD`（不填默认值）。"""
    for key in keys:
        if key not in header:
            raise FrameUnparsable(f"HEADER_MISSING_FIELD: {rel_path} 缺 {key}")


def parse_frame_name(rel_path: str) -> Dict[str, str]:
    """抽帧文件名合同。不匹配 ⇒ `FRAME_NAME_MISMATCH`。**不做任何猜测性回退**。"""
    name = os.path.basename(rel_path)
    m = FRAME_NAME_RE.match(name)
    if m is None:
        raise FrameUnparsable(f"FRAME_NAME_MISMATCH: {rel_path} 不匹配 {FRAME_NAME_RE.pattern}")
    return m.groupdict()


def frame_manifest(dataset_id: str) -> List[Dict[str, Any]]:
    """逐帧实算出本层全部数据面判据的输入面（**不读 `index.json` 任何现值**）。

    每条记录含两条**互相独立**的观测面：
    - 文件名面：`name_object` / `name_panel` / `name_telescope` / `name_mode` /
      `name_date` / `name_time` / `name_exptime_s` / `name_filter`；
    - FITS 头面：`naxis` / `bitpix` / `hdr_exptime_s` / `hdr_filter` / `hdr_date` /
      `hdr_object` / `has_wcs` / `has_cd` / `has_cdelt`。

    两条面的交叉一致性由 D7（命名合同）判定；任何一面缺失都 fail-closed。
    """
    rows: List[Dict[str, Any]] = []
    for rel in frame_relpaths(dataset_id):
        name_face = parse_frame_name(rel)
        header = read_frame_header(rel)
        require_light_keys(header, rel)
        rows.append({
            "dataset": dataset_id,
            "rel_path": rel,
            # 文件名面
            "name_object": name_face["obj"],
            "name_panel": name_face["panel"],
            "name_telescope": name_face["tel"],
            "name_mode": name_face["mode"],
            "name_date": name_face["date"],
            "name_time": name_face["time"],
            "name_exptime_s": float(name_face["exp"]),
            "name_filter": name_face["filt"],
            # FITS 头面
            "naxis": (int(header["NAXIS1"]), int(header["NAXIS2"])),
            "bitpix": int(header["BITPIX"]),
            "hdr_exptime_s": float(header["EXPTIME"]),
            "hdr_filter": str(header["FILTER"]).strip(),
            "hdr_date": str(header["DATE-OBS"])[:10],
            "hdr_object": str(header["OBJECT"]).strip(),
            # WCS 面
            "has_wcs": "CTYPE1" in header,
            "has_cd": "CD1_1" in header,
            "has_cdelt": "CDELT1" in header,
        })
    if not rows:
        raise AssertionError(
            f"ZERO_OBJECT_GUARD: 数据集 {dataset_id} 帧清单实算为 0（VALIDATION_EVIDENCE.md:172）"
        )
    return rows


def frame_telescope_dirs(dataset_id: str) -> Dict[str, List[str]]:
    """按**目录**第一层段分组帧路径（M42 = `T2/…`、`T3/…`；银心 = `lights/…`）。"""
    root_rel = DATASET_DIR[dataset_id]
    grouped: Dict[str, List[str]] = {}
    for rel in frame_relpaths(dataset_id):
        inside = os.path.relpath(rel, repo_abs(root_rel))
        top = inside.split(os.sep)[0] if os.sep in inside else "."
        grouped.setdefault(top, []).append(rel)
    return grouped


# ---------------------------------------------------------------------------
# §5 WCS 读取（唯一读取面 = astropy，遵守 CD 优先于 CDELT 的规范）
# ---------------------------------------------------------------------------

def astropy_wcs(header, rel_path: str):
    """由 FITS 头构造 `astropy.wcs.WCS`；无 `CTYPE1` ⇒ `HEADER_MISSING_FIELD`。"""
    from astropy.wcs import WCS

    if "CTYPE1" not in header:
        raise FrameUnparsable(f"HEADER_MISSING_FIELD: {rel_path} 缺 CTYPE1（无 WCS 面）")
    try:
        return WCS(header, naxis=2)
    except Exception as exc:  # noqa: BLE001
        raise FrameUnparsable(f"FRAME_UNPARSABLE: {rel_path} WCS 构造失败 :: {exc}") from exc


def wcs_pixel_scale_arcsec(header, rel_path: str) -> float:
    """本层**唯一**的像元尺度读数面：经 astropy WCS 解析得到的投影平面尺度。

    刻意**不读 `CDELTi`**：FITS WCS Papers II 把 `CDi_j` 与 `CDELTi` 定为**冗余的两种表示**，
    两者同时在场时 `CD` 承重、`CDELT` 应被忽略。M42 的 166 个有 WCS 帧**两者都在且互相矛盾**
    （实测 `|CD1_1| / |CDELT1|` ∈ [0.00372, 0.0208]，即差 48–269 倍），见 README REG-01。
    """
    import astropy.units as u

    scales = astropy_wcs(header, rel_path).proj_plane_pixel_scales()
    if len(scales) != 2:
        raise FrameUnparsable(f"FRAME_UNPARSABLE: {rel_path} 投影平面尺度维数={len(scales)}")
    return float(max(s.to_value(u.deg) for s in scales)) * 3600.0


def wcs_roundtrip_error_px(header, rel_path: str, n_probe: int = 64) -> float:
    """WCS 往返闭合误差（像元），取 `n_probe²` 个等距角点的最大绝对偏差。

    容差档：绝对门限 `WCS_ROUNDTRIP_MAX_PX`（取值与依据见常量处）。
    相对档 `TEST.md:48` 的 `rtol = 1e-12` 作为构成项记入报告，不单独设门。
    """
    wcs = astropy_wcs(header, rel_path)
    nx, ny = int(header["NAXIS1"]), int(header["NAXIS2"])
    xs = np.linspace(0.0, nx - 1.0, n_probe)
    ys = np.linspace(0.0, ny - 1.0, n_probe)
    gx, gy = np.meshgrid(xs, ys)
    world = wcs.pixel_to_world(gx, gy)
    back = wcs.world_to_pixel(world)
    return float(max(np.max(np.abs(back[0] - gx)), np.max(np.abs(back[1] - gy))))


def wcs_roundtrip_tolerance_px() -> float:
    """`wcs_roundtrip_error_px` 的**冻结门限**（绝对像元）。

    刻意做成**无参**：门限是逐字冻结的项目常量（见常量处依据），**不随帧的几何缩放**，
    也不从被测数据反算。若写成 `rtol·NAXIS + atol·NAXIS`，门限就会随数据变化，
    那正是「改阈值让测试能跑」的一种形态。
    """
    return WCS_ROUNDTRIP_MAX_PX


def wcs_cd_cdelt_ratio(header, rel_path: str) -> Optional[float]:
    """`|CD1_1| / |CDELT1|`。任一缺失返回 `None`（**不是** 0.0，也不是默认值）。

    ⚠ 这**不是**像元尺度比。FITS WCS Papers II 里 `CDELTi` 是「x 步长在 RA 上的投影」的
    无旋转表述，`CDi_j` 是完整线性变换。实测 `m42` 的 166 个有 WCS 帧 **CD 矩阵全部强非对角**
    （对角 ≈0.0165″、非对角 ≈±0.966″，约 90° 旋转），故 `|CD1_1|` 与 `|CDELT1|` 相差约 50 倍
    **不蕴含**像元尺度差 50 倍——两者的**面积尺度** `sqrt(|det CD|)` 与
    `sqrt(|CDELT1·CDELT2|)` 实测只差 1.4e-4 相对量。

    ⇒ 本量的正确语义是「**两种表述对 x 步长的描述是否自洽**」，偏离 1 即判矛盾；
    **不是**「像元尺度差多少」。判红是**表述自洽性**缺陷，不是尺度缺陷（README REG-01）。
    """
    if "CD1_1" not in header or "CDELT1" not in header:
        return None
    den = abs(float(header["CDELT1"]))
    if den == 0.0:
        raise FrameUnparsable(f"FRAME_UNPARSABLE: {rel_path} CDELT1 == 0（比例无定义）")
    return abs(float(header["CD1_1"])) / den


def wcs_x_axis_step_arcsec(header, rel_path: str, drop_cd: bool = False) -> float:
    """x 方向单位步长在天球上的实际位移（角秒，`|Δ(RA·cos δ)|`），在 `(0, 0)` 处取值。

    `drop_cd=True` 时先删掉全部 `CD*` 卡片再构造 WCS，得到「**只按 `CDELT` 解释**」的读数。
    两读数的比值就是「WCS 读入器究竟承重哪张卡」的**可观测量**。

    为何用 x 步长而不用 `proj_plane_pixel_scales`：实测 `m42` 的 CD 矩阵强非对角，
    投影平面尺度取的是**面积尺度**，`sqrt(|det CD|)` 与 `sqrt(|CDELT1·CDELT2|)` 只差
    1.4e-4 相对量——用面积尺度做这张判据**不可观测**（会退化成恒真）。
    x 轴步长则相差 **496 倍**，判据有牙。
    """
    import math

    from astropy.wcs import WCS

    hdr = header.copy()
    if drop_cd:
        for key in list(hdr.keys()):
            if key.startswith("CD"):
                del hdr[key]
        if "CTYPE1" not in hdr:
            raise FrameUnparsable(
                f"FRAME_UNPARSABLE: {rel_path} 去掉 CD 后无 CTYPE，无对照面"
            )
    try:
        w = WCS(hdr, naxis=2)
        ra0, dec0 = w.pixel_to_world_values(0.0, 0.0)
        ra1, _dec1 = w.pixel_to_world_values(1.0, 0.0)
    except Exception as exc:  # noqa: BLE001
        raise FrameUnparsable(f"FRAME_UNPARSABLE: {rel_path} WCS 步长求值失败 :: {exc}") from exc
    step = abs(float(ra1) - float(ra0)) * math.cos(math.radians(float(dec0))) * 3600.0
    return step


# ---------------------------------------------------------------------------
# §6 标定母版索引（目录侧）
# ---------------------------------------------------------------------------

def master_relpaths(master_dir_name: str) -> List[str]:
    """列出某母版目录下全部 `.xisf`（仓内相对路径，字典序）。空 ⇒ `ZERO_OBJECT_GUARD`。"""
    rel_dir = f"{TESTDATA_REL}/{master_dir_name}"
    anchor_dir(rel_dir, f"MASTER_DIR[{master_dir_name}]")
    out = [
        f"{rel_dir}/{name}"
        for name in sorted(os.listdir(repo_abs(rel_dir)))
        if name.endswith(".xisf")
    ]
    if not out:
        raise AssertionError(
            f"ZERO_OBJECT_GUARD: 母版目录 {rel_dir} 下实算 .xisf 数为 0"
            "（VALIDATION_EVIDENCE.md:172 第 1 条）"
        )
    return out


def master_type_and_exposure(rel_path: str) -> Dict[str, Any]:
    """由母版**文件名**独立抽 `master_bias` / `master_dark` / `master_flat` 与曝光秒。

    抽不出 ⇒ `MASTER_NAME_MISMATCH`。命名合同（逐字）：
    `master{Bias|Dark|Flat}_BIN-<n>_<W>x<H>[_EXPOSURE-<sec>s][_FILTER-<name>_mono].xisf`。
    """
    name = os.path.basename(rel_path)
    m = re.match(
        r"^master(?P<kind>Bias|Dark|Flat)_BIN-(?P<bin>\d+)_"
        r"(?P<w>\d+)x(?P<h>\d+)"
        r"(?:_EXPOSURE-(?P<exp>[0-9]+(?:\.[0-9]+)?)s)?"
        r"(?:_FILTER-(?P<filt>.+?)(?:_mono)?)?\.xisf$",
        name,
    )
    if m is None:
        raise XisfUnparsable(f"MASTER_NAME_MISMATCH: {rel_path}")
    out: Dict[str, Any] = {
        "kind": f"master_{m.group('kind').lower()}",
        "binning": int(m.group("bin")),
        "w": int(m.group("w")),
        "h": int(m.group("h")),
        "exposure_s": float(m.group("exp")) if m.group("exp") is not None else None,
        "filter": m.group("filt"),
    }
    if out["kind"] == "master_dark" and out["exposure_s"] is None:
        raise XisfUnparsable(f"MASTER_NAME_MISSING_FIELD: {rel_path} masterDark 缺 EXPOSURE 段")
    if out["kind"] == "master_flat" and out["filter"] is None:
        raise XisfUnparsable(f"MASTER_NAME_MISSING_FIELD: {rel_path} masterFlat 缺 FILTER 段")
    return out


# ---------------------------------------------------------------------------
# §7 XISF 1.0 最小解析器（只解析头；按需读回未压缩附加块）
#
# 字节布局**不发明**，逐条对照仓内 C++ 读入器 `lib/infrastructure/aio/src/aio_xisf.cpp`：
#   :447  读 8 字节魔数并比对 `XISF0100`（魔数常量在 :25）
#   :452-460 再读 8 字节、按**小端 uint64** 拼出头长
#   :467  头长 > 64 MiB 硬失败（常量在 :29）
#   :158-181 扫描第一个 `<Image `，取 `geometry` / `sampleFormat` / `byteOrder` / `location`
#   :91-98  `location` 只认 `attachment:<offset>:<size>`
#   :33    几何上界 `w,h,c ∈ [1, 65535]`
# 与产品读入器的**有意分歧**只有一处（未知 sampleFormat：产品 fallback，本层判红），
# 见本模块 docstring「REG-08」。
# ---------------------------------------------------------------------------

class XisfHeader:
    """XISF 头解析结果。只读字段，不含像元。"""

    __slots__ = (
        "rel_path", "schema_location", "geometry", "sample_format", "byte_order",
        "data_offset", "data_size", "file_size", "fits_keywords",
    )

    def __init__(self, **kw: Any) -> None:
        for slot in self.__slots__:
            setattr(self, slot, kw[slot])

    @property
    def width(self) -> int:
        return self.geometry[0]

    @property
    def height(self) -> int:
        return self.geometry[1]

    @property
    def channels(self) -> int:
        return self.geometry[2]

    @property
    def itemsize(self) -> int:
        return XISF_SAMPLE_FORMATS[self.sample_format][0]


def _attr(tag_text: str, name: str) -> str:
    m = re.search(rf'\b{re.escape(name)}="([^"]*)"', tag_text)
    return m.group(1) if m else ""


def read_xisf_header(rel_path: str) -> XisfHeader:
    """读 XISF 1.0 头。任一环节不成立即抛 `XisfUnparsable`（**不回退、不填默认值**）。"""
    abs_path = anchor_file(rel_path, "XISF_MASTER")
    file_size = os.path.getsize(abs_path)
    with open(abs_path, "rb") as fh:
        prefix = fh.read(16)
        if len(prefix) != 16:
            raise XisfUnparsable(f"XISF_TRUNCATED_PREFIX: {rel_path} 连魔数+头长都不足 16 字节")
        if prefix[:8] != XISF_MAGIC:
            raise XisfUnparsable(
                f"XISF_BAD_MAGIC: {rel_path} 实测 {prefix[:8]!r}，期望 {XISF_MAGIC!r}"
            )
        xml_length = struct.unpack("<Q", prefix[8:16])[0]
        if xml_length == 0:
            raise XisfUnparsable(f"XISF_BAD_MAGIC: {rel_path} XML 头长为 0")
        if xml_length > XISF_MAX_XML_HEADER_BYTES:
            raise XisfUnparsable(
                f"XISF_XML_TOO_LARGE: {rel_path} 头长 {xml_length} > 上限 {XISF_MAX_XML_HEADER_BYTES}"
            )
        if 16 + xml_length > file_size:
            raise XisfUnparsable(
                f"XISF_TRUNCATED: {rel_path} 声明头长 {xml_length} + 16 > 文件大小 {file_size}"
            )
        xml_bytes = fh.read(xml_length)
        if len(xml_bytes) != xml_length:
            raise XisfUnparsable(f"XISF_TRUNCATED: {rel_path} 短读 XML 头")

    try:
        xml = xml_bytes.decode("utf-8")
    except UnicodeDecodeError as exc:
        raise XisfUnparsable(f"XISF_UNPARSABLE: {rel_path} XML 头非 UTF-8 :: {exc}") from exc

    schema = re.search(r'xsi:schemaLocation="([^"]+)"', xml)
    if schema is None:
        raise XisfUnparsable(f"XISF_MISSING_FIELD: {rel_path} 缺 xsi:schemaLocation")
    schema_location = schema.group(1)

    tag = re.search(r"<Image\s[^>]*>", xml)
    if tag is None:
        raise XisfUnparsable(f"XISF_NO_IMAGE: {rel_path} XML 头内无 <Image> 元素")

    geom_text = _attr(tag.group(0), "geometry")
    if not geom_text:
        raise XisfUnparsable(f"XISF_MISSING_FIELD: {rel_path} <Image> 缺 geometry")
    parts = geom_text.split(":")
    if len(parts) not in (1, 2, 3):
        raise XisfUnparsable(f"XISF_BAD_GEOMETRY: {rel_path} geometry={geom_text!r}")
    dims = [int(p) for p in parts]
    while len(dims) < 3:
        dims.append(1)
    if any(d <= 0 or d > XISF_MAX_DIM for d in dims):
        raise XisfUnparsable(
            f"XISF_BAD_GEOMETRY: {rel_path} geometry={geom_text!r} 越界 "
            f"[1, {XISF_MAX_DIM}]"
        )
    geometry = (dims[0], dims[1], dims[2])

    sample_format = _attr(tag.group(0), "sampleFormat")
    if sample_format not in XISF_SAMPLE_FORMATS:
        # 有意分歧（REG-08）：产品 `parse_sample_format` 对未知格式 WARN + fallback Float32，
        # 本层判红。不复制 fail-open。
        raise XisfUnparsable(
            f"XISF_UNSUPPORTED_SAMPLE_FORMAT: {rel_path} sampleFormat={sample_format!r}"
            f" 不在受支持集 {sorted(XISF_SAMPLE_FORMATS)}"
            "（本层 fail-closed；产品侧 aio_xisf.cpp:64 对未知格式 fallback Float32，见 REG-08）"
        )

    byte_order = _attr(tag.group(0), "byteOrder") or "little-endian"
    if byte_order not in XISF_BYTE_ORDERS:
        raise XisfUnparsable(f"XISF_BAD_BYTE_ORDER: {rel_path} byteOrder={byte_order!r}")

    location = _attr(tag.group(0), "location")
    if not location.startswith("attachment:"):
        raise XisfUnparsable(
            f"XISF_UNSUPPORTED_LOCATION: {rel_path} location={location!r}；"
            "本层只支持未压缩附加块（attachment:<offset>:<size>）"
        )
    loc_parts = location.split(":")
    if len(loc_parts) != 3:
        raise XisfUnparsable(f"XISF_BAD_LOCATION: {rel_path} location={location!r}")
    data_offset = int(loc_parts[1])
    data_size = int(loc_parts[2])
    if data_offset <= 0 or data_size <= 0:
        raise XisfUnparsable(
            f"XISF_BAD_LOCATION: {rel_path} offset={data_offset} size={data_size} 须均为正"
        )
    expected = geometry[0] * geometry[1] * geometry[2] * XISF_SAMPLE_FORMATS[sample_format][0]
    if data_size != expected:
        raise XisfUnparsable(
            f"XISF_SIZE_MISMATCH: {rel_path} 声明 {data_size} 字节 ≠ "
            f"几何×样宽 {geometry}×{XISF_SAMPLE_FORMATS[sample_format][0]} = {expected}"
        )
    if data_offset + data_size > file_size:
        raise XisfUnparsable(
            f"XISF_ATTACHMENT_TRUNCATED: {rel_path} offset+size={data_offset + data_size}"
            f" > 文件大小 {file_size}"
        )

    keywords: List[Tuple[str, str]] = []
    for m in re.finditer(r'<FITSKeyword\s+name="([^"]*)"\s+value="([^"]*)"', xml):
        keywords.append((m.group(1), m.group(2)))
    if not keywords:
        raise XisfUnparsable(f"XISF_MISSING_FIELD: {rel_path} 无 <FITSKeyword> 条目")

    return XisfHeader(
        rel_path=rel_path,
        schema_location=schema_location,
        geometry=geometry,
        sample_format=sample_format,
        byte_order=byte_order,
        data_offset=data_offset,
        data_size=data_size,
        file_size=file_size,
        fits_keywords=keywords,
    )


def xisf_fits_keyword(header: XisfHeader, name: str) -> Optional[str]:
    """取 XISF 头内嵌的 FITS 关键字值；不存在返回 `None`（**不是** `""`、不是 `0.0`）。"""
    for key, value in header.fits_keywords:
        if key == name:
            return value
    return None


def xisf_pixels(header: XisfHeader) -> np.ndarray:
    """读回未压缩附加块的像元，形如 `(height, width, channels)` 或 `(height, width)`。

    ⚠ 本函数产出的是**Python 侧解析值**，与产品 `xisf_read_file()` 的逐点一致性**未取得
    证据**（该判据登记为 B 类，需构建）。用途限于本层的元数据与诊断量。
    """
    _itemsize, _is_float, dtype_str = XISF_SAMPLE_FORMATS[header.sample_format]
    abs_path = anchor_file(header.rel_path, "XISF_MASTER")
    w, h, c = header.geometry
    with open(abs_path, "rb") as fh:
        fh.seek(header.data_offset)
        raw = fh.read(header.data_size)
    if len(raw) != header.data_size:
        raise XisfUnparsable(
            f"XISF_ATTACHMENT_TRUNCATED: {header.rel_path} 短读 {len(raw)}/{header.data_size}"
        )
    endian = "<" if header.byte_order == "little-endian" else ">"
    arr = np.frombuffer(raw, dtype=np.dtype(endian + dtype_str))
    if arr.size != w * h * c:
        raise XisfUnparsable(
            f"XISF_SIZE_MISMATCH: {header.rel_path} 读回 {arr.size} 样点 ≠ {w * h * c}"
        )
    if c == 1:
        return arr.reshape(h, w)
    return arr.reshape(h, w, c)


def build_synthetic_xisf(
    xml_body: str,
    payload: bytes,
    magic: bytes = XISF_MAGIC,
    declared_xml_length: Optional[int] = None,
) -> bytes:
    """在内存里拼一个 XISF 1.0 文件（**只给负例用**：注入损坏、验 fail-closed）。

    正常路径由 `test_calibration_inputs.py` 的负例调用者传入合法的 `xml_body` / `payload`，
    逐一注入下列缺陷并断言抛出对应具名异常：
    坏魔数、零头长、超限头长、截断文件、无 `<Image>`、非法 geometry、未知 sampleFormat、
    非附加块 location、声明字节数与几何不符、附加块越界。
    """
    xml_bytes = xml_body.encode("utf-8")
    length_field = len(xml_bytes) if declared_xml_length is None else declared_xml_length
    return magic + struct.pack("<Q", length_field) + xml_bytes + payload


def synthetic_xisf_xml(
    geometry: str = "4:3:1",
    sample_format: str = "Float32",
    location: str = "attachment:16:48",
    keywords: str = '<FITSKeyword name="IMAGETYP" value="\'Master Bias\'"/>',
) -> str:
    """合成 XISF 的 XML 头正文（负例用）。"""
    return (
        '<?xml version="1.0" encoding="UTF-8"?>'
        '<xisf version="1.0" xmlns="http://www.pixinsight.com/xisf" '
        'xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" '
        'xsi:schemaLocation="http://www.pixinsight.com/xisf '
        'http://pixinsight.com/xisf/xisf-1.0.xsd">'
        f'<Image geometry="{geometry}" sampleFormat="{sample_format}" location="{location}">'
        f"{keywords}"
        "</Image></xisf>"
    )


# ---------------------------------------------------------------------------
# §8 标定输入面的实算（增益 / 读噪 / 饱和来源的显式性）
# ---------------------------------------------------------------------------

def scan_electronics_keywords() -> Dict[str, Any]:
    """扫描 353 帧 + 27 母版，统计 `ELECTRONICS_ABSENT_KEYWORDS` 的携带情况。

    返回 `{"present": {kw: [rel_path, ...]}, "scanned_lights": int, "scanned_masters": int}`。
    `present` 里出现任何关键词 ⇒ REG-03 的「该量为外部输入」登记失效 ⇒ C6 判红。
    """
    present: Dict[str, List[str]] = {kw: [] for kw in ELECTRONICS_ABSENT_KEYWORDS}
    n_lights = 0
    for dataset_id in DATASET_IDS:
        for rel in frame_relpaths(dataset_id):
            header = read_frame_header(rel)
            n_lights += 1
            for kw in ELECTRONICS_ABSENT_KEYWORDS:
                if kw in header:
                    present[kw].append(rel)
    n_masters = 0
    for master_dir_name in MASTER_DIR_NAMES:
        for rel in master_relpaths(master_dir_name):
            xh = read_xisf_header(rel)
            n_masters += 1
            names = {k for k, _ in xh.fits_keywords}
            for kw in ELECTRONICS_ABSENT_KEYWORDS:
                if kw in names:
                    present[kw].append(rel)
    return {
        "present": {kw: sorted(v) for kw, v in present.items()},
        "scanned_lights": n_lights,
        "scanned_masters": n_masters,
    }


def declared_external_inputs() -> Dict[str, Any]:
    """本层登记的**外部输入**声明（REG-03）。

    增益 / 读噪 / 饱和这三量在仓内**没有任何承载面**：353 帧 FITS 头与 27 个 XISF 母版
    均不携带（`scan_electronics_keywords()` 实测），三份 `phase_config_*.schema.json` 也**没有**
    `gain` 键。饱和电平另有合同规定的来源优先级
    （`phase_config_normalize.schema.json#/$defs/noise_config/properties/saturation_level`）：
    `noise.saturation_level` > `snr.saturation_level` > FITS 头 `SATURATE`/`DATAMAX`，
    **三级皆无 ⇒ 显式 `DISABLED_NO_METADATA`，禁静默**。本组数据落在该末支。
    """
    return {
        "electron_gain": {
            "in_repo_carrier": None,
            "must_be_supplied_by": "调用方配置或标定台账（本层不发明数值）",
        },
        "read_noise": {
            "in_repo_carrier": None,
            "must_be_supplied_by": "调用方配置或标定台账（本层不发明数值）",
        },
        "saturation_level": {
            "in_repo_carrier": None,
            "frozen_source_priority": [
                "noise.saturation_level",
                "snr.saturation_level",
                "FITS: SATURATE / DATAMAX",
            ],
            "all_absent_outcome": "DISABLED_NO_METADATA",
            "must_be_supplied_by": "同上前两级之一；三级皆无时按合同显式记 DISABLED_NO_METADATA",
        },
    }


# ---------------------------------------------------------------------------
# §9 视觉验收辅助面（**只供人目检，不产出任何合格/不合格判决**）
# ---------------------------------------------------------------------------

#: 诊断量名（冻结词表）。这些是**诊断读数**，不是判据阈值；本层不对它们设「合格线」。
DIAGNOSTIC_NAMES = (
    "n_pixels",
    "n_nan",
    "n_posinf",
    "n_neginf",
    "n_saturated",
    "n_zero_block_pixels",
    "n_zero_blocks",
    "vmin",
    "vmax",
    "dynamic_range",
    "background_median",
    "background_mad",
    "n_below_background_3mad",
    "n_above_background_3mad",
)


def array_diagnostics(
    arr: np.ndarray,
    saturation_level: Optional[float] = None,
    block: int = 64,
) -> Dict[str, Any]:
    """逐像元诊断量。**读全数组，不在任何先筛过的子集上取统计量。**

    ⚠ 这是本仓反面教材 `实验/m42-realdata/code/c3_seam_additive.py:302`
    （`max(..., default=0.0)` 作用在先筛过的子集上）的反面写法：
    这里每个计数都从**完整数组**上直接数出，`n_pixels` 恒等于数组元素数，
    用例 C/V 组逐条断言这一点。

    参数
    ----
    arr : 二维实数数组。
    saturation_level : 饱和电平（ADU）。`None` 表示**该量外部不可得**
        （REG-03）⇒ 饱和像元计数记 `None`（不是 0），并由用例断言它是 `None` 而非 0。
    block : 全零块的分块边长（像元）。
    """
    data = np.asarray(arr)
    if data.ndim != 2:
        raise FrameUnparsable(f"DIAG_BAD_SHAPE: 期望二维数组，实测 ndim={data.ndim}")
    n_pixels = int(data.size)
    nan_mask = np.isnan(data)
    posinf_mask = np.isposinf(data)
    neginf_mask = np.isneginf(data)
    finite = data[np.isfinite(data)]

    saturated: Optional[int] = None
    if saturation_level is not None:
        saturated = int(np.count_nonzero(finite >= float(saturation_level)))

    zero_blocks = 0
    zero_block_pixels = 0
    h, w = data.shape
    ny = h // block
    nx = w // block
    if ny > 0 and nx > 0:
        core = data[: ny * block, : nx * block].reshape(ny, block, nx, block)
        zero_mask = np.all(core == 0, axis=(1, 3))
        zero_blocks = int(np.count_nonzero(zero_mask))
        zero_block_pixels = zero_blocks * block * block

    if finite.size == 0:
        # 全部非有限：背景与动态范围**无定义**，记 None 而不是 0.0（禁 fail-open）。
        return {
            "n_pixels": n_pixels,
            "n_nan": int(np.count_nonzero(nan_mask)),
            "n_posinf": int(np.count_nonzero(posinf_mask)),
            "n_neginf": int(np.count_nonzero(neginf_mask)),
            "n_saturated": saturated,
            "n_zero_block_pixels": zero_block_pixels,
            "n_zero_blocks": zero_blocks,
            "vmin": None,
            "vmax": None,
            "dynamic_range": None,
            "background_median": None,
            "background_mad": None,
            "n_below_background_3mad": None,
            "n_above_background_3mad": None,
            "finite_pixels": 0,
        }

    vmin = float(finite.min())
    vmax = float(finite.max())
    background_median = float(np.median(finite))
    background_mad = float(np.median(np.abs(finite - background_median)))
    lo = background_median - 3.0 * background_mad
    hi = background_median + 3.0 * background_mad
    return {
        "n_pixels": n_pixels,
        "n_nan": int(np.count_nonzero(nan_mask)),
        "n_posinf": int(np.count_nonzero(posinf_mask)),
        "n_neginf": int(np.count_nonzero(neginf_mask)),
        "n_saturated": saturated,
        "n_zero_block_pixels": zero_block_pixels,
        "n_zero_blocks": zero_blocks,
        "vmin": vmin,
        "vmax": vmax,
        "dynamic_range": vmax - vmin,
        "background_median": background_median,
        "background_mad": background_mad,
        "n_below_background_3mad": int(np.count_nonzero(finite < lo)),
        "n_above_background_3mad": int(np.count_nonzero(finite > hi)),
        "finite_pixels": int(finite.size),
    }


def stretch_to_unit(arr: np.ndarray, mode: str = "asinh") -> Tuple[np.ndarray, Dict[str, Any]]:
    """把数组按给定拉伸映到 `[0, 1]` 浮点。返回 `(映射结果, 读数)`。

    非有限值**不折叠成 0、不填哨兵**（`TEST.md:79`「比较器不得把非有限值折叠成 0 或任何
    哨兵值」）：非有限位在结果里保持 `NaN`，由 `matplotlib` 渲成透明，供人眼直接看见黑洞。
    """
    data = np.asarray(arr, dtype=np.float64)
    finite = data[np.isfinite(data)]
    if finite.size == 0:
        raise FrameUnparsable("STRETCH_NO_FINITE_PIXELS: 全数组无非有限值以外的有限像素")
    vmin = float(finite.min())
    vmax = float(finite.max())
    if vmax == vmin:
        raise FrameUnparsable(
            f"STRETCH_DEGENERATE_RANGE: vmin == vmax == {vmin}；常量图无动态范围，不可拉伸"
        )
    out = np.full(data.shape, np.nan, dtype=np.float64)
    f = data[np.isfinite(data)]
    if mode == "linear":
        out[np.isfinite(data)] = (f - vmin) / (vmax - vmin)
        readings = {"vmin": vmin, "vmax": vmax, "mode": mode}
    elif mode == "log":
        # log 要求像元**严格为正**。把 [vmin, vmax] 归一到 [1, vmax/vmin] 再取 log10，
        # 单调且端点精确映到 0 / 1；不做任何偏移兜底（有非正像元就判红，拒绝出图）。
        if vmin <= 0.0:
            raise FrameUnparsable(
                f"STRETCH_LOG_NONPOSITIVE: log 拉伸遇到非正像元（vmin={vmin}），拒绝出图"
            )
        span = np.log10(vmax / vmin)
        out[np.isfinite(data)] = np.log10(f / vmin) / span
        readings = {"vmin": vmin, "vmax": vmax, "mode": mode, "log_reference": vmin}
    elif mode == "asinh":
        # 零点取稳健背景（median），强度取动态范围；天文拉伸的标准做法。
        background = float(np.median(f))
        stretch = max(vmax - background, 1e-12)
        out[np.isfinite(data)] = np.arcsinh((f - background) / stretch) / np.arcsinh(1.0 / 1e-6)
        readings = {"vmin": vmin, "vmax": vmax, "mode": mode, "background": background}
    else:
        raise FrameUnparsable(f"STRETCH_UNKNOWN_MODE: {mode!r}；受支持 'linear' / 'log' / 'asinh'")
    return np.clip(out, 0.0, 1.0), readings


def render_stretch_png(
    arr: np.ndarray,
    out_path: str,
    mode: str = "asinh",
    title: Optional[str] = None,
    cmap: str = "gray",
    dpi: int = 100,
) -> Dict[str, Any]:
    """把真实帧写成**供人目检**的拉伸 PNG。返回 `{path, shape, readings, png_bytes}`。

    渲染**逐像元 1:1**：`figsize = (w/dpi, h/dpi)` + `dpi` ⇒ 输出恰好 `w×h` 像素；
    坐标轴满幅、无标题条。标题写进 PNG 元数据（`Description`），不占像素网格
    ——否则输出网格会被重采样，与阵列对不齐，判据 V2 的往返一致性就失去意义。

    ⚠⚠ **本函数只产出供人看的图，不产出任何「合格 / 不合格」判决。**
    目视判定只属项目负责人（`AGENTS.md` §11、`docs/ACSD_DESIGN.md:558`）。
    本层**不得**据此发明布尔门并挂 `pass`（由 `test_visual_acceptance.py` 的 V7 可执行地禁止）。
    """
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    mapped, readings = stretch_to_unit(arr, mode=mode)
    height, width = mapped.shape
    fig = plt.figure(figsize=(width / dpi, height / dpi), dpi=dpi)
    ax = fig.add_axes((0.0, 0.0, 1.0, 1.0))
    # `bad` 决定非有限像元的显示色；不透明洋红 ⇒ 黑洞在图上是真的看得见的色块。
    cmap_obj = plt.get_cmap(cmap).with_extremes(bad=(1.0, 0.0, 1.0, 1.0))
    ax.imshow(mapped, cmap=cmap_obj, origin="lower", vmin=0.0, vmax=1.0,
              interpolation="nearest")
    ax.set_xticks([])
    ax.set_yticks([])
    out_dir = os.path.dirname(os.path.abspath(out_path))
    if out_dir and not os.path.isdir(out_dir):
        os.makedirs(out_dir, exist_ok=True)
    metadata = {"Software": "ACSD e2e visual-acceptance auxiliary"}
    if title:
        metadata["Description"] = title
    fig.savefig(out_path, format="png", metadata=metadata)
    plt.close(fig)
    return {
        "path": out_path,
        "shape": [height, width],
        "readings": readings,
        "png_bytes": os.path.getsize(out_path),
    }


def read_png_back(path: str) -> np.ndarray:
    """把 PNG 读回成 `[0, 1]` 的 RGB 数组（用于「图 ↔ 数列」往返一致性核对）。"""
    import matplotlib.image as mpimg

    abs_path = anchor_file(path, "RENDERED_PNG")
    img = mpimg.imread(abs_path)
    if img.ndim == 2:
        img = np.stack([img, img, img], axis=-1)
    if img.shape[-1] == 4:
        img = img[..., :3]
    return img


def count_reference_images() -> Dict[str, int]:
    """统计 `testdata/` 全树内参考图（REG-05）。**只计数，不删、不改。**"""
    root = anchor_dir(TESTDATA_REL, "TESTDATA_ROOT")
    counts: Dict[str, int] = {}
    for dirpath, _dirnames, filenames in os.walk(root):
        for name in filenames:
            lower = name.lower()
            for suffix in REFERENCE_IMAGE_SUFFIXES:
                if lower.endswith(suffix):
                    counts[suffix] = counts.get(suffix, 0) + 1
                    break
    return counts


def read_light_pixels(rel_path: str) -> np.ndarray:
    """读一帧真实亮场的像元（float64，BZERO/BSCALE 已被 astropy 应用）。"""
    from astropy.io import fits

    abs_path = anchor_file(rel_path, "LIGHT_FRAME")
    try:
        with fits.open(abs_path, memmap=False) as hdul:
            data = hdul[0].data
    except Exception as exc:  # noqa: BLE001
        raise FrameUnparsable(f"FRAME_UNPARSABLE: {rel_path} 像元读取失败 :: {exc}") from exc
    if data is None:
        raise FrameUnparsable(f"HEADER_MISSING_FIELD: {rel_path} 无像元数据（NAXIS=0）")
    return np.asarray(data, dtype=np.float64)


# ---------------------------------------------------------------------------
# §10 B 类「需构建」登记（`test_pipeline_e2e.py` 的唯一事实源）
# ---------------------------------------------------------------------------

#: B 类判据登记。每条记录是**可判读数据**，不是注释：
#:   - `judge_id` 与 `test_pipeline_e2e.py` 的用例函数名、skip reason 三方必须一致；
#:   - `requires` 列出所依赖的构建产物（仓内相对路径）；
#:   - `why` 逐条写明为什么本单跑不了；
#:   - `blocked_by` 列出阻塞它的工单/事实。
#: 一致性由 `test_pipeline_e2e.py` 的 A 类登记守卫用例判定（可红，见该文件 docstring）。
BUILD_REQUIRED_REGISTRY: Tuple[Dict[str, Any], ...] = (
    {
        "judge_id": "E2E.NormalizeSerialChain",
        "requires": ("build/acsd", "eng/packaging/config/templates/normalize.phase_config.json"),
        "why": "需按 CLI_PROTOCOL.md:122 的唯一命令树跑 `acsd normalize --json <cfg>`；"
               "产品未在本单车道构建，且 build/acsd 由 commit 143af8a 产出、早于当前 HEAD。",
        "blocked_by": "T10 构建车道；本单禁止跑构建与端到端全流程",
    },
    {
        "judge_id": "E2E.MosaicSerialChain",
        "requires": ("build/acsd", "eng/packaging/config/templates/mosaic.phase_config.json"),
        "why": "需消费 normalize 产出的 HiPS 产品树（PIPELINE_BLOCK.md:20 跨阶段唯一载体），"
               "上游产物不存在则本判据无从进入。",
        "blocked_by": "E2E.NormalizeSerialChain",
    },
    {
        "judge_id": "E2E.ExportSerialChain",
        "requires": ("build/acsd", "eng/packaging/config/templates/export.phase_config.json"),
        "why": "需消费 mosaic 产出的 HiPS 产品树；上游产物不存在则本判据无从进入。",
        "blocked_by": "E2E.MosaicSerialChain",
    },
    {
        "judge_id": "E2E.ProductManifestConformance",
        "requires": ("build/acsd",),
        "why": "判据对象是三命令落盘的 manifest.json（运行清单），无运行即无对象。",
        "blocked_by": "E2E.NormalizeSerialChain / E2E.MosaicSerialChain / E2E.ExportSerialChain",
    },
    {
        "judge_id": "E2E.Atomicity",
        "requires": ("build/acsd",),
        "why": "判据是「失败后不得留下半成品产品树」这一**运行期**性质，纯静态/纯数据层无从观测。",
        "blocked_by": "T10 构建车道",
    },
    {
        "judge_id": "E2E.VisualAcceptanceChain",
        "requires": ("build/acsd",),
        "why": "L4 判据（ACSD_DESIGN.md:533）的链条是「马赛克 → 平面 FITS → 拉伸 PNG → 切块目检」，"
               "前两级都需要产品产物；且末级判定权属项目负责人（ACSD_DESIGN.md:558），"
               "本层只做辅助，不做判决。",
        "blocked_by": "E2E.MosaicSerialChain / E2E.ExportSerialChain",
    },
    {
        "judge_id": "E2E.XisfReaderMatchesProduct",
        "requires": ("build/acsd", "lib/infrastructure/aio/src/aio_xisf.cpp"),
        "why": "本层的 Python XISF 解析器与产品 `xisf_read_file()` 的像元逐点一致性，"
               "必须由产品读入器实际读同一文件来对拍。",
        "blocked_by": "T10 构建车道",
    },
    {
        "judge_id": "E2E.WcsTransformDifferential",
        "requires": (
            "build/acsd",
            "eng/tests/validation/release02/phot_verify/wcs_lib.py",
            "lib/algorithms/photometry/cpp/src/wcs_transform.cpp",
        ),
        "why": "单元层/集成层移交的差分测试：以 wcs_lib.py（pc::WcsTransform 的 numpy 移植）"
               "对拍产品 wcs_transform.cpp，需要编译并运行产品。",
        "blocked_by": "T10 构建车道（单元层/集成层报告已登记移交构建/端到端车道）",
    },
    {
        "judge_id": "E2E.PhotometrySigmaRealFrameLeg",
        "requires": ("build/acsd",),
        "why": "单元层报告登记移交的「S6 P1 测光 σ 双边界的真实帧腿」，需要产品对真实帧出读数。",
        "blocked_by": "T10 构建车道（单元层报告已登记移交端到端）",
    },
)

#: 构建产物探测面（**只探测存在与否，不执行、不断言其可用**）。
#: 探测清单是**字面量**，与 `BUILD_REQUIRED_REGISTRY[].requires` 互相独立 ⇒
#: 「登记引用了未探测的产物」会被守卫判红（拼写错、路径漂移都能抓到）。
BUILD_ARTIFACT_PROBE_PATHS: Tuple[str, ...] = (
    "build/acsd",
    "eng/packaging/config/templates/normalize.phase_config.json",
    "eng/packaging/config/templates/mosaic.phase_config.json",
    "eng/packaging/config/templates/export.phase_config.json",
    "lib/infrastructure/aio/src/aio_xisf.cpp",
    "lib/algorithms/photometry/cpp/src/wcs_transform.cpp",
    "eng/tests/validation/release02/phot_verify/wcs_lib.py",
)


def probe_build_artifacts() -> Dict[str, bool]:
    """探测 `BUILD_ARTIFACT_PROBE_PATHS` 的存在性。**只读 `os.path` 判定，不执行。**"""
    return {rel: os.path.exists(repo_abs(rel)) for rel in BUILD_ARTIFACT_PROBE_PATHS}


def build_required_ids() -> Tuple[str, ...]:
    """B 类判据 id 序列（登记表的有序视图，供守卫逐条比对）。"""
    return tuple(record["judge_id"] for record in BUILD_REQUIRED_REGISTRY)


# ---------------------------------------------------------------------------
# §11 判定逻辑（`judge_*`：返回违规清单，空清单 = 绿）
#
# 放在本模块而不是散在用例里，是为了让负例能**复用同一条判定**做注入复跑
# （`VALIDATION_EVIDENCE.md:189,193`：S2 同时验证绿、S6 注入点必须是该判定自身的逻辑）。
# ---------------------------------------------------------------------------

def judge_frame_census(dataset_id: str) -> List[str]:
    """D8 帧集合指纹：实测帧数 / 几何 / BITPIX / 曝光计数 / 滤镜计数 / WCS 计数
    与冻结登记逐项精确一致（`TEST.md:46` 第一档）。任一项漂移即红。"""
    reg = REGISTERED_FRAME_CENSUS[dataset_id]
    rows = frame_manifest(dataset_id)
    viol: List[str] = []

    if len(rows) != reg["n_frames"]:
        viol.append(f"FRAME_COUNT_DRIFT: {dataset_id} 实测 {len(rows)} 帧，登记 {reg['n_frames']}")

    shapes = sorted({r["naxis"] for r in rows})
    if shapes != [reg["naxis"]]:
        viol.append(f"FRAME_GEOMETRY_NOT_UNIFORM: {dataset_id} 实测几何集合 {shapes}")

    bits = sorted({r["bitpix"] for r in rows})
    if bits != [reg["bitpix"]]:
        viol.append(f"FRAME_BITPIX_DRIFT: {dataset_id} 实测 BITPIX 集合 {bits}")

    expt: Dict[float, int] = {}
    filt: Dict[str, int] = {}
    n_no_wcs = 0
    for r in rows:
        expt[r["hdr_exptime_s"]] = expt.get(r["hdr_exptime_s"], 0) + 1
        filt[r["hdr_filter"]] = filt.get(r["hdr_filter"], 0) + 1
        n_no_wcs += 0 if r["has_wcs"] else 1
    if expt != reg["exptime_s_counts"]:
        viol.append(f"FRAME_EXPTIME_CENSUS_DRIFT: {dataset_id} 实测 {expt}")
    if filt != reg["filter_counts"]:
        viol.append(f"FRAME_FILTER_CENSUS_DRIFT: {dataset_id} 实测 {filt}")
    if n_no_wcs != reg["n_without_wcs"]:
        viol.append(f"FRAME_NO_WCS_COUNT_DRIFT: {dataset_id} 实测 {n_no_wcs} 帧无 WCS")
    if len(rows) - n_no_wcs != reg["n_with_wcs"]:
        viol.append(f"FRAME_WCS_COUNT_DRIFT: {dataset_id} 实测 {len(rows) - n_no_wcs} 帧有 WCS")
    return viol


def judge_dataset_reachable(dataset_id: str) -> List[str]:
    """D1 可达性：数据集目录存在、至少一帧、每帧是**非空普通文件**且头可解析。"""
    rel_dir = DATASET_DIR[dataset_id]
    viol: List[str] = []
    # 目录缺失 ⇒ 具名异常（fail-closed），**不**返回一条违规串后继续：
    # 「目录没了」是锚存活失败，必须响，不能与「帧内容有问题」混在同一层面。
    rels = frame_relpaths(dataset_id)  # 目录缺失⇒ANCHOR_STALE；零帧⇒ZERO_OBJECT_GUARD
    for rel in rels:
        abs_path = repo_abs(rel)
        if os.path.getsize(abs_path) == 0:
            viol.append(f"FRAME_EMPTY_FILE: {rel} 大小为 0")
        try:
            header = read_frame_header(rel)
        except FrameUnparsable as exc:
            viol.append(str(exc))
            continue
        try:
            require_light_keys(header, rel)
        except FrameUnparsable as exc:
            viol.append(str(exc))
    return viol


def judge_naming_contract(dataset_id: str) -> List[str]:
    """D7 命名合同：**文件名面**与 **FITS 头面**逐项交叉一致（四个独立合取项）。

    合取项：(a) 曝光秒；(b) 滤镜（精确串，逐字比对，不做大小写归一）；
    (c) 观测日期（文件名 `YYYYMMDD` ⇒ 头 `DATE-OBS` 前 10 字）；
    (d) OBJECT 头值出现在文件名对象段内。

    四个合取项取自**两条互不派生的观测面**（文件名串 vs FITS 头），任一漂移都能被抓到，
    不是恒真比较。
    """
    viol: List[str] = []
    for rel in frame_relpaths(dataset_id):
        name_face = parse_frame_name(rel)
        header = read_frame_header(rel)
        require_light_keys(header, rel)
        if float(name_face["exp"]) != float(header["EXPTIME"]):
            viol.append(
                f"NAME_EXPTIME_MISMATCH: {rel} 文件名 {name_face['exp']}s ≠ 头 EXPTIME "
                f"{header['EXPTIME']}"
            )
        if name_face["filt"] != str(header["FILTER"]).strip():
            viol.append(
                f"NAME_FILTER_MISMATCH: {rel} 文件名 {name_face['filt']!r} ≠ 头 FILTER "
                f"{str(header['FILTER']).strip()!r}"
            )
        name_date = f"{name_face['date'][:4]}-{name_face['date'][4:6]}-{name_face['date'][6:]}"
        if name_date != str(header["DATE-OBS"])[:10]:
            viol.append(
                f"NAME_DATE_MISMATCH: {rel} 文件名 {name_date} ≠ 头 DATE-OBS "
                f"{str(header['DATE-OBS'])[:10]}"
            )
        if f"{name_face['obj']}_{name_face['panel']}_{name_face['tel']}_{name_face['mode']}" \
                != str(header["OBJECT"]).strip():
            viol.append(
                f"NAME_OBJECT_MISMATCH: {rel} 头 OBJECT {str(header['OBJECT']).strip()!r} "
                f"≠ 文件名前缀 "
                f"{name_face['obj']}_{name_face['panel']}_{name_face['tel']}_{name_face['mode']!r}"
            )
        if name_face["mode"] != "flying_dutchman":
            viol.append(
                f"NAME_MODE_UNEXPECTED: {rel} 模式段 {name_face['mode']!r} ≠ 'flying_dutchman'"
            )
        if name_face["tel"] not in DATASET_TELESCOPES[dataset_id]:
            viol.append(
                f"NAME_TELESCOPE_OUT_OF_SCOPE: {rel} 望远镜 {name_face['tel']} 不在 "
                f"{dataset_id} 的登记集合 {sorted(DATASET_TELESCOPES[dataset_id])}"
            )
    return viol


def judge_wcs_usable(dataset_id: str) -> List[str]:
    """D5/D6 WCS 可用性：**只对带 `CTYPE1` 的帧**判定（无 WCS 的帧由 D4 登记守卫管）。

    判据：(a) `astropy.wcs.WCS` 可构造；(b) 投影平面像元尺度为正有限；
    (c) 像元↔天球往返闭合误差 ≤ `wcs_roundtrip_tolerance_px`（`TEST.md:48` 档）。
    """
    viol: List[str] = []
    for rel in frame_relpaths(dataset_id):
        header = read_frame_header(rel)
        require_light_keys(header, rel)
        if "CTYPE1" not in header:
            continue
        try:
            scale = wcs_pixel_scale_arcsec(header, rel)
            err = wcs_roundtrip_error_px(header, rel)
            tol = wcs_roundtrip_tolerance_px()
        except FrameUnparsable as exc:
            viol.append(str(exc))
            continue
        if not np.isfinite(scale) or scale <= 0.0:
            viol.append(f"WCS_SCALE_INVALID: {rel} 像元尺度={scale}″")
        # 非有限误差必须判红（禁 fail-open：NaN 与任何门限比较都返回 False）。
        if not np.isfinite(err):
            viol.append(f"WCS_ROUNDTRIP_NOT_FINITE: {rel} 往返误差非有限（={err}）")
        elif err > tol:
            viol.append(f"WCS_ROUNDTRIP_EXCEEDED: {rel} 往返误差={err:.6e} px > 门限 {tol:.6e} px")
    return viol


def scan_distortion_face(dataset_id: str) -> Dict[str, Any]:
    """实算某数据集的畸变解关键词面（**只用 FITS 头，不用任何库的解释**）。

    逐帧只回答四个**可数**的问题：`CTYPE1` 取什么值、是否声明 `-SIP`、有无 `PV*` 系数、
    有无 `A_* / B_*` 系数；外加非标准前缀 `TR1_* / TR2_*` 的出现帧数。
    """
    reg = REGISTERED_DISTORTION_FACE[dataset_id]
    n_with_wcs = 0
    ctype_values: set = set()
    n_declares_sip = 0
    n_pv = 0
    n_ab = 0
    n_nonstandard = 0
    for rel in frame_relpaths(dataset_id):
        header = read_frame_header(rel)
        if "CTYPE1" not in header:
            continue
        n_with_wcs += 1
        ctype_values.add(str(header["CTYPE1"]))
        keys = list(header.keys())
        declares_sip = str(header["CTYPE1"]).endswith("-SIP")
        n_declares_sip += 1 if declares_sip else 0
        n_pv += 1 if any(k.startswith(("PV1_", "PV2_")) for k in keys) else 0
        n_ab += 1 if any(k.startswith(("A_", "B_")) for k in keys) else 0
        n_nonstandard += 1 if any(k.startswith(("TR1_", "TR2_")) for k in keys) else 0
    return {
        "n_with_wcs": n_with_wcs,
        "ctype1_values": sorted(ctype_values),
        "declares_sip": n_declares_sip == n_with_wcs and n_with_wcs > 0,
        "has_pv_coeffs": n_pv == n_with_wcs and n_with_wcs > 0,
        "has_sip_ab_coeffs": n_ab == n_with_wcs and n_with_wcs > 0,
        "nonstandard_prefix_frames": n_nonstandard,
        "registered": reg,
    }


def judge_distortion_face(dataset_id: str) -> List[str]:
    """D4b 畸变解关键词面漂移守卫（REG-D2）。

    判红条件（任一）：有 WCS 的帧数变化；`CTYPE1` 取值集合与登记不符；`-SIP` 声明面变化；
    `PV*` / `A_*B_*` 系数面变化；非标准前缀 `TR1_*/TR2_*` 的出现帧数变化。

    这条判据**不**判「M42 的畸变解不可读」为红——那是**已登记的数据事实**
    （README REG-01b），不是判据失效；本判据只保证该事实的读数不会悄悄漂移。
    """
    face = scan_distortion_face(dataset_id)
    reg = face["registered"]
    viol: List[str] = []
    if face["n_with_wcs"] != reg["n_with_wcs"]:
        viol.append(
            f"DISTORTION_WCS_COUNT_DRIFT: {dataset_id} 实测 {face['n_with_wcs']} 帧带 WCS，"
            f"登记 {reg['n_with_wcs']}"
        )
    if face["ctype1_values"] != [reg["ctype1"]]:
        viol.append(
            f"DISTORTION_CTYPE_DRIFT: {dataset_id} 实测 CTYPE1 取值 {face['ctype1_values']}，"
            f"登记 ['{reg['ctype1']}']"
        )
    for key in ("declares_sip", "has_pv_coeffs", "has_sip_ab_coeffs"):
        if bool(face[key]) != bool(reg[key]):
            viol.append(
                f"DISTORTION_FACE_DRIFT: {dataset_id} 实测 {key}={face[key]}，登记 {reg[key]}"
            )
    if face["nonstandard_prefix_frames"] != reg["n_with_wcs"] - (
        0 if reg["nonstandard_prefixes"] else reg["n_with_wcs"]
    ):
        viol.append(
            f"DISTORTION_NONSTANDARD_PREFIX_DRIFT: {dataset_id} 实测非标准前缀帧数 "
            f"{face['nonstandard_prefix_frames']}，登记前缀 {reg['nonstandard_prefixes']}"
        )
    return viol


def judge_cd_cdelt_conflict(dataset_id: str) -> List[str]:
    """D9 `CD` 与 `CDELT` 同时在场且互相矛盾（FITS WCS Papers II 冗余表示应一致）。

    判红条件：某帧**两者都在**且 `|CD1_1| / |CDELT1|` 偏离 1 超过 `CD_CDELT_CONFLICT_RATIO`。
    实测 `m42` 的 166 个有 WCS 帧全部命中（比值 ∈ [0.00372, 0.0208]）；
    `galaxy_center` 无 `CDELT1`，不命中。

    ⚠ **本判据在当前数据上判红，是数据缺陷的真实读数，不是判据失效**（`TEST.md:62`
    「判据必须能红」）。处置见 README REG-01：登记、不私自改数据、不降级阈值。
    本层因此**不**把它写成一条绿的用例，而由 `test_dataset_integrity.py`
    的漂移守卫逐条点名命中帧数。
    """
    viol: List[str] = []
    for rel in frame_relpaths(dataset_id):
        header = read_frame_header(rel)
        if "CTYPE1" not in header:
            continue
        ratio = wcs_cd_cdelt_ratio(header, rel)
        if ratio is None:
            continue
        if abs(ratio - 1.0) > CD_CDELT_CONFLICT_RATIO:
            viol.append(f"CD_CDELT_CONFLICT: {rel} |CD1_1|/|CDELT1| = {ratio:.6g}")
    return viol


def judge_wcs_consumes_cd_matrix(dataset_id: str) -> Dict[str, Any]:
    """D9a WCS 读入器**承重哪张卡**的可观测判据。

    对每个「`CD` 与 `CDELT` 同时在场」的帧，构造两个读数面：
    (a) `wcs_x_axis_step_arcsec(header)` —— 本层**唯一**的读入路径（astropy WCS）；
    (b) `wcs_x_axis_step_arcsec(header, drop_cd=True)` —— 删掉 `CD*` 后只按 `CDELT` 解释，
        由**本函数独立实现**的第二条解释路径。

    判据：(a)/(b) 的比值必须落在 `CD` 侧（`<= CD_CDELT_CONFLICT_RATIO`），
    即两个面必须**相差一个量级以上** ⇒ 判据不是恒真比较：若读入器改为承重 `CDELT`
    （比值趋 1）立即判红。

    返回 `{viol, n_observable, n_both, ratios}`。`n_observable` 供冻结登记比对。
    """
    viol: List[str] = []
    ratios: List[float] = []
    n_both = 0
    for rel in frame_relpaths(dataset_id):
        header = read_frame_header(rel)
        if "CTYPE1" not in header or "CDELT1" not in header:
            continue
        n_both += 1
        try:
            step_cd = wcs_x_axis_step_arcsec(header, rel)
            step_cdelt = wcs_x_axis_step_arcsec(header, rel, drop_cd=True)
        except FrameUnparsable as exc:
            viol.append(str(exc))
            continue
        if step_cdelt <= 0.0 or not np.isfinite(step_cdelt):
            viol.append(f"CDELT_FACE_DEGENERATE: {rel} 只按 CDELT 解释的步长={step_cdelt}″")
            continue
        if not np.isfinite(step_cd):
            viol.append(f"CD_FACE_NOT_FINITE: {rel} 本层读数的 x 步长非有限")
            continue
        ratio = step_cd / step_cdelt
        ratios.append(ratio)
        if ratio > CD_CDELT_CONFLICT_RATIO:
            viol.append(
                f"WCS_READER_FELL_ONTO_CDELT: {rel} 本层 x 步长 {step_cd:.6g}″ / "
                f"仅 CDELT 解释 {step_cdelt:.6g}″ = {ratio:.6g}，落在 CDELT 面上"
            )
    return {"viol": viol, "n_both": n_both, "n_observable": len(ratios), "ratios": ratios}


def judge_master_reachable_and_parsable(master_dir_name: str) -> List[str]:
    """C1/C2 母版可达与可解析：目录存在、≥1 个母版、每个母版 XISF 头可解析、
    `<FITSKeyword>` 非空、几何与 `REGISTERED_MASTER_CENSUS` 一致。"""
    viol: List[str] = []
    reg = REGISTERED_MASTER_CENSUS[master_dir_name]
    rels = master_relpaths(master_dir_name)  # 零个 ⇒ ZERO_OBJECT_GUARD
    if len(rels) != reg["n_masters"]:
        viol.append(
            f"MASTER_COUNT_DRIFT: {master_dir_name} 实测 {len(rels)} 个，登记 {reg['n_masters']} 个"
        )
    for rel in rels:
        try:
            xh = read_xisf_header(rel)
        except XisfUnparsable as exc:
            viol.append(str(exc))
            continue
        if xh.geometry != reg["geometry"]:
            viol.append(
                f"MASTER_GEOMETRY_DRIFT: {rel} 实测几何 {xh.geometry}，登记 {reg['geometry']}"
            )
        if not xh.fits_keywords:
            viol.append(f"XISF_MISSING_FIELD: {rel} 无 <FITSKeyword> 条目")
    return viol


def judge_master_geometry_matches_lights(dataset_id: str) -> List[str]:
    """C3 母版几何 ↔ 同望远镜亮场几何，逐帧逐母版对照（两条独立读面的交叉）。"""
    viol: List[str] = []
    light_shapes: Dict[str, set] = {}
    for rel in frame_relpaths(dataset_id):
        face = parse_frame_name(rel)
        header = read_frame_header(rel)
        require_light_keys(header, rel)
        light_shapes.setdefault(face["tel"], set()).add(
            (int(header["NAXIS1"]), int(header["NAXIS2"]))
        )
    for master_dir_name in DATASET_MASTER_DIR[dataset_id]:
        for rel in master_relpaths(master_dir_name):
            xh = read_xisf_header(rel)
            tel = master_telescope(master_dir_name)
            expect = light_shapes.get(tel)
            if not expect:
                viol.append(
                    f"MASTER_NO_MATCHING_LIGHTS: {master_dir_name} 对应望远镜 {tel} 在 "
                    f"{dataset_id} 无亮场"
                )
                continue
            got = (xh.width, xh.height)
            if got not in expect:
                viol.append(
                    f"MASTER_GEOMETRY_MISMATCH: {rel} 实测 {got} ∉ 同望远镜亮场几何 {sorted(expect)}"
                )
    return viol


def master_telescope(master_dir_name: str) -> str:
    """母版目录名 → 望远镜代号（`T4 calibration files` → `T4`）。"""
    m = re.match(r"^(T\d+)\s+calibration files$", master_dir_name)
    if m is None:
        raise XisfUnparsable(f"MASTER_DIR_NAME_MISMATCH: {master_dir_name!r}")
    return m.group(1)


def judge_dark_exposure_coverage(dataset_id: str) -> Dict[str, Dict[str, Dict[float, int]]]:
    """C4 暗场曝光覆盖（**当前存在真实缺口，见 REG-02**）。

    返回 `{望远镜: {曝光秒: 未被同望远镜 masterDark 覆盖的帧数}}`，**只给有缺口的键**。
    本函数是**观测器**不是判定器：缺口是真实数据事实，本层如实登记并做漂移守卫
    （`test_calibration_inputs.py` 的 C4 守卫断言缺口签名恰为登记值），
    **不**把它写成一条永远红的用例，也**不**把阈值放宽到「有缺口也算过」。
    """
    darks: Dict[str, set] = {}
    for master_dir_name in DATASET_MASTER_DIR[dataset_id]:
        tel = master_telescope(master_dir_name)
        for rel in master_relpaths(master_dir_name):
            name_face = master_type_and_exposure(rel)
            if name_face["kind"] == "master_dark":
                darks.setdefault(tel, set()).add(name_face["exposure_s"])

    need: Dict[str, Dict[float, int]] = {}
    for rel in frame_relpaths(dataset_id):
        face = parse_frame_name(rel)
        header = read_frame_header(rel)
        require_light_keys(header, rel)
        tel, exp = face["tel"], float(header["EXPTIME"])
        if exp not in darks.get(tel, set()):
            need.setdefault(tel, {}).setdefault(exp, 0)
            need[tel][exp] += 1
    return need


def judge_flat_filter_coverage(dataset_id: str) -> List[str]:
    """C5 平场滤镜覆盖（**当前无缺口**，可写成绿的正例判据）。

    对每个 `(数据集, 望远镜, 亮场 FILTER)`，必须存在**同望远镜**且 FILTER 精确匹配的
    masterFlat。匹配按**精确串**（不做大小写归一）：T2 的 OIII 与 T3/T4 的 Oiii 是两套写法，
    该分歧见 REG-03。银心实际用到 `Oiii`，T4 母版写 `Oiii` ⇒ 判绿。
    """
    viol: List[str] = []
    flats: Dict[str, set] = {}
    for master_dir_name in DATASET_MASTER_DIR[dataset_id]:
        tel = master_telescope(master_dir_name)
        for rel in master_relpaths(master_dir_name):
            name_face = master_type_and_exposure(rel)
            if name_face["kind"] == "master_flat":
                flats.setdefault(tel, set()).add(name_face["filter"])

    need = set()
    for rel in frame_relpaths(dataset_id):
        face = parse_frame_name(rel)
        header = read_frame_header(rel)
        require_light_keys(header, rel)
        need.add((face["tel"], str(header["FILTER"]).strip()))
    for tel, filt in sorted(need):
        if filt not in flats.get(tel, set()):
            viol.append(
                f"FLAT_FILTER_MISSING: {dataset_id} 望远镜 {tel} 的 FILTER={filt!r} "
                f"无同望远镜 masterFlat（现有 {sorted(flats.get(tel, set()))}）"
            )
    return viol


def judge_electronics_absent() -> List[str]:
    """C6 电子学量为**外部输入**这一登记的漂移守卫。

    任一 `ELECTRONICS_ABSENT_KEYWORDS` 出现在任一帧或任一母版上即红——
    那意味着仓内**长出了**承载面，`REG-03` 的「外部输入」登记必须重做，
    不能让增益/读噪在两个口径下同时活着。
    """
    scan = scan_electronics_keywords()
    viol: List[str] = []
    for kw, hits in scan["present"].items():
        if hits:
            viol.append(
                f"ELECTRONICS_KEYWORD_APPEARED: {kw} 出现在 {len(hits)} 个文件（首个 {hits[0]}）；"
                "REG-03『该量为外部输入』的登记失效，须重做裁定"
            )
    return viol


def judge_reference_images_absent() -> List[str]:
    """V3 视觉验收**无基准**这一登记的漂移守卫（REG-05）。

    testdata 树内一旦出现参考图，本层就必须决定是否改为「与参考图比对」的口径；
    在裁定之前不允许默默开始比对。
    """
    counts = count_reference_images()
    viol: List[str] = []
    for suffix, n in sorted(counts.items()):
        viol.append(
            f"REFERENCE_IMAGE_APPEARED: testdata 内出现 {n} 个 {suffix} 参考图；"
            "REG-05『零参考图』登记失效，须裁定是否改为比对口径"
        )
    return viol


def index_date_reconciliation(dataset_id_in_index: str) -> Dict[str, Any]:
    """索引自陈日期集合 vs 逐帧 `DATE-OBS` 实算日期集合，**双向**求差。

    返回 `{self_dates, disk_dates, only_index, only_disk}`。索引是**被对账对象**
    （`REG-06`），预期值不取自索引自身。
    """
    index = load_index()
    entry = index_dataset_entry(index, dataset_id_in_index)
    self_dates = tuple(entry.get("observation_dates") or ())
    if not self_dates:
        raise AnchorStale(
            f"ANCHOR_MISSING_FIELD: TESTDATA_INDEX.datasets[{dataset_id_in_index}].observation_dates"
        )
    disk_rel = next(
        rel for rel in DATASET_DIR.values() if rel.endswith(dataset_id_in_index)
    )
    dataset_id = next(k for k, v in DATASET_DIR.items() if v == disk_rel)
    disk_dates = tuple(sorted({r["hdr_date"] for r in frame_manifest(dataset_id)}))
    return {
        "index_dataset_id": dataset_id_in_index,
        "repo_dataset_id": dataset_id,
        "self_dates": self_dates,
        "disk_dates": disk_dates,
        "only_index": tuple(sorted(set(self_dates) - set(disk_dates))),
        "only_disk": tuple(sorted(set(disk_dates) - set(self_dates))),
    }


def judge_index_gap_matches_registration(dataset_id_in_index: str) -> List[str]:
    """REG-06b 索引对账缺口的**漂移守卫**。

    断言「索引缺、磁盘有」的日期集合恰为 `REGISTERED_INDEX_GAPS`，
    且「索引自陈、磁盘没有」的集合恰为 `REGISTERED_INDEX_PHANTOMS`。

    ⚠ 缺口**非零**是数据缺陷的真实读数（当前缺 `2025-08-13`），不是判据失效。
    本守卫保证的是「这个缺口的签名不悄悄漂移」，并把缺陷持续挂在报告上；
    **不**把它写成「期望缺口为空」的绿用例——那等于要求数据没这个缺陷，等于降级判据。
    与模块层 REG-03 同一处置体例（登记 + 漂移守卫，不私自改数据）。
    """
    rec = index_date_reconciliation(dataset_id_in_index)
    viol: List[str] = []
    if rec["only_disk"] != REGISTERED_INDEX_GAPS.get(dataset_id_in_index, ()):
        viol.append(
            f"INDEX_GAP_SIGNATURE_DRIFT: {dataset_id_in_index} 索引缺 {list(rec['only_disk'])}，"
            f"登记 {list(REGISTERED_INDEX_GAPS.get(dataset_id_in_index, ()))}"
        )
    if rec["only_index"] != REGISTERED_INDEX_PHANTOMS.get(dataset_id_in_index, ()):
        viol.append(
            f"INDEX_PHANTOM_SIGNATURE_DRIFT: {dataset_id_in_index} 索引多出 {list(rec['only_index'])}，"
            f"登记 {list(REGISTERED_INDEX_PHANTOMS.get(dataset_id_in_index, ()))}"
        )
    if rec["self_dates"] != REGISTERED_INDEX_OBSERVATION_DATES:
        viol.append(
            f"INDEX_DATES_DRIFT: {dataset_id_in_index} 索引自陈日期集合变化：{list(rec['self_dates'])}"
        )
    if rec["disk_dates"] != REGISTERED_DISK_OBSERVATION_DATES:
        viol.append(
            f"DISK_DATES_DRIFT: {dataset_id_in_index} 磁盘实算日期集合变化：{list(rec['disk_dates'])}"
        )
    return viol


def judge_diagnostics_read_whole_array(arr: np.ndarray) -> List[str]:
    """V4 反 fail-open：诊断量必须读**全数组**。

    这是对 `实验/m42-realdata/code/c3_seam_additive.py:302`
    （`max(..., default=0.0)` 作用在先筛过的子集上）的直接对照：
    判据断言 `n_pixels` 恰等于数组元素数、`n_nan + n_posinf + n_neginf + finite_pixels`
    恰等于 `n_pixels`（两个**互不派生**的计数面闭合）。任一子集统计量误用都会打破该闭合。
    """
    diag = array_diagnostics(arr)
    viol: List[str] = []
    if diag["n_pixels"] != int(np.asarray(arr).size):
        viol.append(
            f"DIAG_ARRAY_NOT_WHOLE: n_pixels={diag['n_pixels']} ≠ 数组元素数 {int(np.asarray(arr).size)}"
        )
    accounted = diag["n_nan"] + diag["n_posinf"] + diag["n_neginf"] + diag["finite_pixels"]
    if accounted != diag["n_pixels"]:
        viol.append(
            f"DIAG_COUNT_NOT_CLOSED: 非有限+有限={accounted} ≠ n_pixels={diag['n_pixels']}"
        )
    return viol