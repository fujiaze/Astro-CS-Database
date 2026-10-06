"""端到端层 · B 类（产品端到端）——**全"需构建产品并运行（T10 构建/端到端车道） | judge_id=E2E.PhotometrySigmaRealFrameLeg | requires=['build/acsd'] | blocked_by=T10 构建车道（单元层报告已登记移交端到端） | why=单元层报告登记移交的「S6 P1 测光 σ 双边界的真实帧腿」，需要产品对真实帧出读数。"需要编译并运行产品。"Registry_MarkerAndRegistryAgree` | **A 类**守卫 R1 |
| `E2E.Registry.SkipReasonsCarryRecord` | `test_E2E_Registry_SkipReasonsCarryRecord` | **A 类**守卫 R2 |
| `E2E.Registry.ProbedArtifactsCoverRequires` | `test_E2E_Registry_ProbedArtifactsCoverRequires` | **A 类**守卫 R3 |
| `E2E.Registry.EveryRecordHasReasonAndArtifact` | `test_E2E_Registry_EveryRecordHasReasonAndArtifact` | **A 类**守卫 R4 |
| `E2E.Pipeline.NormalizeSerialChain` | `test_E2E_Pipeline_NormalizeSerialChain` | **B 类**（需构建） |
| `E2E.Pipeline.MosaicSerialChain` | `test_E2E_Pipeline_MosaicSerialChain` | **B 类**（需构建） |
| `E2E.Pipeline.ExportSerialChain` | `test_E2E_Pipeline_ExportSerialChain` | **B 类**（需构建） |
| `E2E.Pipeline.ProductManifestConformance` | `test_E2E_Pipeline_ProductManifestConformance` | **B 类**（需构建） |
| `E2E.Pipeline.Atomicity` | `test_E2E_Pipeline_Atomicity` | **B 类**（需构建） |
| `E2E.Pipeline.VisualAcceptanceChain` | `test_E2E_Pipeline_VisualAcceptanceChain` | **B 类**（需构建） |
| `E2E.Pipeline.XisfReaderMatchesProduct` | `test_E2E_Pipeline_XisfReaderMatchesProduct` | **B 类**（需构建） |
| `E2E.Pipeline.WcsTransformDifferential` | `test_E2E_Pipeline_WcsTransformDifferential` | **B 类**（需构建，单元/集成层移交） |
| `E2E.Pipeline.PhotometrySigmaRealFrameLeg` | `test_E2E_Pipeline_PhotometrySigmaRealFrameLeg` | **B 类**（需构建，单元层移交） |

## 与其它层的关系

- `eng/tests/unit/`：S6 P1 测光 σ 双边界的**真实帧腿**由单元层报告登记移交本车道
  ⇒ `E2E.PhotometrySigmaRealFrameLeg`。
- `eng/tests/integration/`：`eng/tests/validation/release02/phot_verify/wcs_lib.py`
  的**差分测试**（对拍产品 `lib/algorithms/photometry/cpp/src/wcs_transform.cpp`）
  由集成层报告登记移交构建/端到端车道 ⇒ `E2E.WcsTransformDifferential`。
- `eng/tests/synthetic/`：合成全链层（哈勃仿真与代数合成端到端）。**不在本目录**。
  `eng/tests/validation/release02/q1_photometry_gradient/src/synth_core.py` 属**合成全链层**，
  本层**不**吸收，在此点明以免读者以为漏了。

## 不产出阻塞退出码

不注册进 CMakeLists、不接 CI；不出现 `sys.exit` / `raise SystemExit` / `os._exit`。
"""

from __future__ import annotations

import ast
import os
from typing import Dict, List

import pytest

import _realdata as rd

#: `judge_id` → 用例函数名的**约定映射**（`Suite.Feature` 的点号形式换成 pytest 的下划线形式，
#: `TEST.md:88`）。R1 用它把登记与代码面对齐。
def _test_name_for(judge_id: str) -> str:
    return "test_E2E_Pipeline_" + judge_id.split(".", 1)[1]


#: skip reason 的**固定模板**。字段分隔符用 `" | "`（不是 `；`）——`blocked_by` / `why`
#: 的正文里本身就有 `；`，用 `；` 分隔会把字段切碎。R2 按本模板**逐字段精确比对**。
REASON_TEMPLATE = (
    "需构建产品并运行（T10 构建/端到端车道） | judge_id={jid} | requires={req} "
    "| blocked_by={blk} | why={why}"
)
REASON_FIELD_PREFIX = "需构建产品并运行（T10 构建/端到端车道） | "


def _reason_fields(record: Dict) -> List[str]:
    """一条登记在 skip reason 里必须**逐字段精确相等**的四段。"""
    return [
        f"judge_id={record['judge_id']}",
        f"requires={list(record['requires'])!r}",
        f"blocked_by={record['blocked_by']}",
        f"why={record['why']}",
    ]


def _parse_reason(reason: str) -> List[str]:
    """把 reason 拆成四段字段；模板不符时抛 `ValueError`（不静默通过）。"""
    if not reason.startswith(REASON_FIELD_PREFIX):
        raise ValueError("reason 不以固定模板前缀开头")
    parts = reason[len(REASON_FIELD_PREFIX):].split(" | ")
    if len(parts) != 4:
        raise ValueError(f"reason 字段数 {len(parts)} ≠ 4")
    return parts


def _scan_marked_tests() -> Dict[str, str]:
    """AST 扫**本文件源码**，取全部带 `pytest.mark.skip` 的函数名 → skip reason。

    用源码文本而非运行时 marker，避免依赖 pytest 的收集顺序；这是本层
    「读源码不执行」的既有体例（对照 `eng/tests/module/_blockflow.py` 用 `ast` 取字面量）。
    """
    src_path = os.path.abspath(__file__)
    with open(src_path, "r", encoding="utf-8") as fh:
        tree = ast.parse(fh.read(), filename=src_path)
    marked: Dict[str, str] = {}
    for node in ast.walk(tree):
        if not isinstance(node, ast.FunctionDef) or not node.name.startswith("test_"):
            continue
        for deco in node.decorator_list:
            target = deco.func if isinstance(deco, ast.Call) else deco
            if not (isinstance(target, ast.Attribute) and target.attr == "skip"):
                continue
            reason = ""
            if isinstance(deco, ast.Call):
                for kw in deco.keywords:
                    if kw.arg == "reason" and isinstance(kw.value, ast.Constant):
                        reason = str(kw.value.value)
            marked[node.name] = reason
    return marked


# ---------------------------------------------------------------------------
# A 类守卫：让「需构建」这份事实保持可判读、与代码面一致
# ---------------------------------------------------------------------------

def test_E2E_Registry_MarkerAndRegistryAgree() -> None:
    """R1 标记 ↔ 登记**双向**对齐（A 类，现在就跑）。

    三条判据：(a) 带 `pytest.mark.skip` 的 B 类用例函数名集合 **==** 登记 id 映射出的
    函数名集合（相等，不是包含 —— 两个方向都要卡）；
    (b) 登记非空且 id 互不重复；(c) 每个登记 id 都能映射出一个**唯一**函数名。

    ⇒ 新增一条 B 类判据却忘了登记 ⇒ 红；登记了却没写用例 ⇒ 红；id 写重了 ⇒ 红。

    **来源依据**：`TEST.md:26` 非退化 + 本层 B 类登记的完整性要求。
    """
    marked = _scan_marked_tests()
    assert marked, (
        "R1 本文件里一条 `pytest.mark.skip` 都没有：B 类要么被删了、要么 skip 标记被摘掉了。"
        "**严禁**把需构建的判据改成静默通过（TEST.md:60「不以伪通过掩盖未执行」）"
    )
    registered = list(rd.build_required_ids())
    assert len(registered) == len(set(registered)), (
        f"R1 登记表里有重复的 judge_id：{registered}"
    )
    expected = {_test_name_for(jid) for jid in registered}
    assert set(marked) == expected, (
        f"R1 标记与登记不对齐：\n"
        f"  代码面多出：{sorted(set(marked) - expected)}\n"
        f"  代码面缺少：{sorted(expected - set(marked))}\n"
        f"  登记：{registered}"
    )


def test_E2E_Registry_SkipReasonsCarryRecord() -> None:
    """R2 每条 skip reason 必须**逐字携带**登记的 id、产物清单与阻塞来源（A 类）。

    ⇒ 理由与登记各说各话（改了理由没改登记表、或反之）⇒ 红。
    `pytest -rs` 打出来的就是这份文本，所以它必须自带完整信息，不能只写「需构建」。

    **来源依据**：`TEST.md:60`。
    """
    marked = _scan_marked_tests()
    missing: List[str] = []
    for record in rd.BUILD_REQUIRED_REGISTRY:
        name = _test_name_for(record["judge_id"])
        reason = marked.get(name)
        if reason is None:
            missing.append(f"{record['judge_id']}: 无用例函数 {name}")
            continue
        try:
            fields = _parse_reason(reason)
        except ValueError as exc:
            missing.append(f"{record['judge_id']}: reason 模板不符（{exc}）")
            continue
        expected = _reason_fields(record)
        for got, want in zip(fields, expected):
            # **精确相等**，不是子串包含：`blocked_by` 从
            # 「T10 构建车道（单元层报告已登记移交端到端）」缩成「T10 构建车道」时，
            # 子串包含仍会通过 —— 这个洞是实测扰动（M4）抓出来后才补上的。
            if got != want:
                missing.append(f"{record['judge_id']}: 字段不符 got={got!r} want={want!r}")
    assert not missing, (
        f"R2 skip reason 未逐字段精确携带登记内容：{missing}"
    )


def test_E2E_Registry_ProbedArtifactsCoverRequires() -> None:
    """R3 登记引用的每个产物路径都必须出现在**独立的**探针清单里（A 类）。

    探针清单 `BUILD_ARTIFACT_PROBE_PATHS` 是**字面量**，与登记表互相独立 ⇒
    登记里写错一个路径（拼写、目录改名）能被抓到。

    本判据**只**断言「覆盖面」，**不**断言「产物存在」——产物存在与否是环境事实，
    给陈旧构建背书不是本层的职责（见本文件顶部说明）。

    **来源依据**：`VALIDATION_EVIDENCE.md:413` 锚存活；本层 B 类登记的可复核性要求。
    """
    probed = set(rd.BUILD_ARTIFACT_PROBE_PATHS)
    uncovered: List[str] = []
    for record in rd.BUILD_REQUIRED_REGISTRY:
        for req in record["requires"]:
            if req not in probed:
                uncovered.append(f"{record['judge_id']}: {req}")
    assert not uncovered, (
        f"R3 登记引用了探针清单之外的产物（路径可能拼错或已漂移）：{uncovered}"
    )
    # 探针清单本身不得为空，且不得有重复项（重复会让覆盖面统计失真）。
    assert probed, "R3 探针清单为空"
    assert len(rd.BUILD_ARTIFACT_PROBE_PATHS) == len(probed), (
        "R3 探针清单有重复项"
    )


def test_E2E_Registry_EveryRecordHasReasonAndArtifact() -> None:
    """R4 每条登记都必须给出**非空**理由与**至少一个**产物（A 类）。

    ⇒ 出现「占位登记」（理由写「待定」、产物写空）⇒ 红。
    理由字数下界取 20 个字符，只为挡住「需构建」三个字这种无信息量的占位；
    真正的理由长度由 `README.md` §8 的完整清单承载。

    **来源依据**：`TEST.md:26`（恒真比较无证据资格 ⇒ 空理由等于没有判据）。
    """
    weak: List[str] = []
    for record in rd.BUILD_REQUIRED_REGISTRY:
        if len(record["why"].strip()) < 20:
            weak.append(f"{record['judge_id']}: why 过短（{len(record['why'].strip())} 字）")
        if not record["requires"]:
            weak.append(f"{record['judge_id']}: requires 为空")
        if not record["blocked_by"].strip():
            weak.append(f"{record['judge_id']}: blocked_by 为空")
    assert not weak, f"R4 登记信息不足：{weak}"
    assert len(rd.BUILD_REQUIRED_REGISTRY) >= 9, (
        f"R4 登记条目数 {len(rd.BUILD_REQUIRED_REGISTRY)} < 9，与 README §8 的清单不一致"
    )


# ---------------------------------------------------------------------------
# B 类：全部显式 SKIP（需构建产品 / 需运行端到端）
#
# 每条 reason 由「登记 id + 产物清单 + 阻塞来源」拼成，与 R2 逐字校验。
# ---------------------------------------------------------------------------

# ⚠ 下面的 skip reason 一律写成**字面量**，不是由登记表在运行时拼出来的。
# 否则 R2 就成了「登记表与自己比对」的恒真检查；写成字面量后，登记（`_realdata`）与
# 代码面（本文件的装饰器）是**两个互不派生的面**，R2 才能真的抓到二者不一致。
# 改登记忘记改 reason（或反之）⇒ R2 判红。


@pytest.mark.skip(reason="需构建产品并运行（T10 构建/端到端车道） | judge_id=E2E.NormalizeSerialChain | requires=['build/acsd', 'eng/packaging/config/templates/normalize.phase_config.json'] | blocked_by=T10 构建车道；本单禁止跑构建与端到端全流程 | why=需按 CLI_PROTOCOL.md:122 的唯一命令树跑 `acsd normalize --json <cfg>`；产品未在本单车道构建，且 build/acsd 由 commit 143af8a 产出、早于当前 HEAD。")
def test_E2E_Pipeline_NormalizeSerialChain() -> None:
    """L3 小批量端到端 · 第一棒：按唯一命令树 `acsd normalize --json <cfg>` 跑通一批真实帧。

    判据内容（需构建后才能执行）：
    (a) 退出码为 0；(b) 逐帧 HiPS 产品树按 `HIPS_STORAGE_FORM.md` 落盘且 manifest 齐备；
    (c) 每帧的 `provenance` 能回溯到输入帧的 `file` + `sha256`；
    (d) 运行清单登记的科学量（测光坐标系统一化）非恒等。

    需构建的理由见 `_realdata.BUILD_REQUIRED_REGISTRY` 的同名条目。

    **来源依据**：`docs/engineering/contracts/CLI_PROTOCOL.md:16,122`；
    `docs/engineering/contracts/PIPELINE_BLOCK.md:20,66`；`docs/ACSD_DESIGN.md:532`。
    """


@pytest.mark.skip(reason="需构建产品并运行（T10 构建/端到端车道） | judge_id=E2E.MosaicSerialChain | requires=['build/acsd', 'eng/packaging/config/templates/mosaic.phase_config.json'] | blocked_by=E2E.NormalizeSerialChain | why=需消费 normalize 产出的 HiPS 产品树（PIPELINE_BLOCK.md:20 跨阶段唯一载体），上游产物不存在则本判据无从进入。")
def test_E2E_Pipeline_MosaicSerialChain() -> None:
    """L3 · 第二棒：`acsd mosaic --json <cfg>` 消费逐帧 HiPS 树，产出马赛克 HiPS 树。

    判据内容：(a) 退出码 0；(b) 马赛克 HiPS 树 + 马赛克清单落盘；
    (c) 跨帧定标只经磁盘产品树交换（不靠内存管线耦合）；
    (d) 排异与集成记录可复核。

    **来源依据**：`PIPELINE_BLOCK.md:20`（跨阶段唯一载体）；`docs/ACSD_DESIGN.md:532`。
    """


@pytest.mark.skip(reason="需构建产品并运行（T10 构建/端到端车道） | judge_id=E2E.ExportSerialChain | requires=['build/acsd', 'eng/packaging/config/templates/export.phase_config.json'] | blocked_by=E2E.MosaicSerialChain | why=需消费 mosaic 产出的 HiPS 产品树；上游产物不存在则本判据无从进入。")
def test_E2E_Pipeline_ExportSerialChain() -> None:
    """L3 · 第三棒：`acsd export --json <cfg>` 消费马赛克 HiPS 树，产出平面 WCS FITS。

    判据内容：(a) 退出码 0；(b) 投影 FITS + 校验报告落盘；
    (c) FITS 头里的 WCS 与源天球一致（往返闭合在冻结容差内）。

    **来源依据**：`CLI_PROTOCOL.md:16`；`docs/ACSD_DESIGN.md:532`。
    """


@pytest.mark.skip(reason="需构建产品并运行（T10 构建/端到端车道） | judge_id=E2E.ProductManifestConformance | requires=['build/acsd'] | blocked_by=E2E.NormalizeSerialChain / E2E.MosaicSerialChain / E2E.ExportSerialChain | why=判据对象是三命令落盘的 manifest.json（运行清单），无运行即无对象。")
def test_E2E_Pipeline_ProductManifestConformance() -> None:
    """L3 · 产物 schema 符合性：三命令落盘的 `manifest.json` 逐项对照仓内 schema。

    判据内容：(a) 三个阶段的 manifest 各自符合对应 schema；
    (b) manifest 里登记的 `sha256` 与实际产物一致；
    (c) 未验收的阶段**不得**标 `available`（`docs/ACSD_DESIGN.md:556`）。

    **来源依据**：`docs/ACSD_DESIGN.md:556`；`PIPELINE_BLOCK.md:66`。
    """


@pytest.mark.skip(reason="需构建产品并运行（T10 构建/端到端车道） | judge_id=E2E.Atomicity | requires=['build/acsd'] | blocked_by=T10 构建车道 | why=判据是「失败后不得留下半成品产品树」这一**运行期**性质，纯静态/纯数据层无从观测。")
def test_E2E_Pipeline_Atomicity() -> None:
    """L3 · 原子性：注入失败后**不得**留下半成品产品树。

    判据内容：(a) 人为让某阶段中途失败；(b) 输出目录要么是完整的旧产品、要么不存在，
    **不得**是「有目录、有 manifest、缺块」；(c) 运行清单不把失败运行记成成功。

    ⚠ 这是**运行期**性质，纯静态/纯数据层无从观测 ⇒ 必须构建并执行。

    **来源依据**：`docs/engineering/contracts/PIPELINE_BLOCK.md:66`（阶段终产物以磁盘
    产品形态发布）；`docs/ACSD_DESIGN.md:532`（L3 含原子性抽检）。
    """


@pytest.mark.skip(reason="需构建产品并运行（T10 构建/端到端车道） | judge_id=E2E.VisualAcceptanceChain | requires=['build/acsd'] | blocked_by=E2E.MosaicSerialChain / E2E.ExportSerialChain | why=L4 判据（ACSD_DESIGN.md:533）的链条是「马赛克 → 平面 FITS → 拉伸 PNG → 切块目检」，前两级都需要产品产物；且末级判定权属项目负责人（ACSD_DESIGN.md:558），本层只做辅助，不做判决。")
def test_E2E_Pipeline_VisualAcceptanceChain() -> None:
    """L4 · 真实视觉验收链路：马赛克 → 平面 FITS → 拉伸 PNG → 切块目检。

    ⚠⚠ **本用例只负责把图造出来供人看，不产出任何「合格 / 不合格」判决。**
    判定权属项目负责人（`AGENTS.md` §11；`docs/ACSD_DESIGN.md:533,558`）。
    可自动化的部分（诊断量）已在 `test_visual_acceptance.py` 里做完。

    需构建的理由：链条前两级（马赛克、平面 FITS）都要产品产物；
    本层无任何现成的产品产物，也不允许在本车道构建。

    **来源依据**：`docs/ACSD_DESIGN.md:533`；`AGENTS.md` §11。
    """


@pytest.mark.skip(reason="需构建产品并运行（T10 构建/端到端车道） | judge_id=E2E.XisfReaderMatchesProduct | requires=['build/acsd', 'lib/infrastructure/aio/src/aio_xisf.cpp'] | blocked_by=T10 构建车道 | why=本层的 Python XISF 解析器与产品 `xisf_read_file()` 的像元逐点一致性，必须由产品读入器实际读同一文件来对拍。")
def test_E2E_Pipeline_XisfReaderMatchesProduct() -> None:
    """本层 Python XISF 解析器 ↔ 产品 `xisf_read_file()` 的**逐点像元一致性**。

    判据内容：(a) 对同一母版，两个读取器给出的形状、样点数、全部像元值逐项一致；
    (b) 头字段（几何、样格式、字节序、内嵌 FITS 关键字）逐项一致。

    ⚠ 本层的解析器**不是**产品读入器：它的字节布局抄自
    `lib/infrastructure/aio/src/aio_xisf.cpp`（判据 C8 逐条对撞源码文本），
    但**像元值层面的一致性尚未取得任何证据**。唯一能取得它的办法是让产品读同一文件。

    **来源依据**：`aio_xisf.cpp:437-650`；本层 `_realdata.py` §7 的布局对照表。
    """


@pytest.mark.skip(reason="需构建产品并运行（T10 构建/端到端车道） | judge_id=E2E.WcsTransformDifferential | requires=['build/acsd', 'eng/tests/validation/release02/phot_verify/wcs_lib.py', 'lib/algorithms/photometry/cpp/src/wcs_transform.cpp'] | blocked_by=T10 构建车道（单元层/集成层报告已登记移交构建/端到端车道） | why=单元层/集成层移交的差分测试：以 wcs_lib.py（pc::WcsTransform 的 numpy 移植）对拍产品 wcs_transform.cpp，需要编译并运行产品。")
def test_E2E_Pipeline_WcsTransformDifferential() -> None:
    """单元/集成层**移交**的差分测试：`wcs_lib.py` 对拍产品 `wcs_transform.cpp`。

    判据内容：取两组真实帧的头（含 SIP 与不含 SIP 两面），在角点网格上比对
    `pixel_to_sky` / `sky_to_pixel` 的双向输出，差值 ≤ 冻结容差
    （`TEST.md:46-48` 档）。

    需构建的理由：必须编译并运行产品的 `pc::WcsTransform`。

    **来源依据**：`eng/tests/validation/release02/phot_verify/wcs_lib.py:1-5`
    （自陈是 `wcs_transform.cpp` 的 numpy 移植）；`lib/algorithms/photometry/cpp/src/wcs_transform.cpp`；
    集成层报告已登记移交构建/端到端车道。
    """


@pytest.mark.skip(reason="需构建产品并运行（T10 构建/端到端车道） | judge_id=E2E.PhotometrySigmaRealFrameLeg | requires=['build/acsd'] | blocked_by=T10 构建车道（单元层报告已登记移交端到端） | why=单元层报告登记移交的「S6 P1 测光 σ 双边界的真实帧腿」，需要产品对真实帧出读数。")
def test_E2E_Pipeline_PhotometrySigmaRealFrameLeg() -> None:
    """单元层**移交**的「S6 P1 测光 σ 双边界的真实帧腿」。

    判据内容：取 M42 的一批真实帧，逐星测光给出 σ，读数落在单元层冻结的 σ 双边界内；
    且该边界对真实帧（非合成帧）仍然成立。

    需构建的理由：需要产品对真实帧出读数。

    **来源依据**：单元层报告已登记移交端到端；`docs/ACSD_DESIGN.md:501`（P1）。
    """