"""集成层：命名块端口合同与代码/IR 的静态一致性判据。

## 定位

**集成层、合同与接口面、多模块协同的静态一致性**。手段是**静态一致性判定**：
读注册表 JSON + 读 C++ 源码文本，自己解析出符号 / 函数体 / 端口，再做双向核对。
**不编译、不链接、不起子进程、不跑构建、不跑端到端**。

测试集**不是门禁**（`AGENTS.md` §8「测试不是约束代码的门，它是暴露代码问题的工具」；
`docs/engineering/testing/TEST.md` §1「接入持续集成时报告红项而不裁决合入」）。
本文件**不产生退出码**、**不裁决代码**：代码里没有 `sys.exit` / `SystemExit` /
`os._exit`，判红一律由 pytest 报项承载。**红项是缺陷信号，不是本文件的缺陷。**

## 判据 → 测试函数映射表

主判据正本：`docs/engineering/contracts/PIPELINE_BLOCK.md`。

| Suite.Feature | pytest 函数 | 判据 | 正本位置 |
|---|---|---|---|
| `INTEGRATION.RegistryParity.C1Structure` | `test_RegistryParity_C1Structure` | C1 结构 | `PIPELINE_BLOCK.md:91` |
| `INTEGRATION.RegistryParity.C2AnchorResolves` | `test_RegistryParity_C2AnchorResolves` | C2 声明⇒实现 | `PIPELINE_BLOCK.md:92` |
| `INTEGRATION.RegistryParity.C3ImplementationToDeclaration` | `test_RegistryParity_C3ImplementationToDeclaration` | C3 实现⇒声明（**基线判红**） | `PIPELINE_BLOCK.md:93`, :75 |
| `INTEGRATION.RegistryParity.C3.UndeclaredFlowMasterRefsProducedByCalibration` | `test_RegistryParity_C3_UndeclaredFlowMasterRefsProducedByCalibration` | C3 缺陷登记 1/3 | `PIPELINE_BLOCK.md:75` |
| `INTEGRATION.RegistryParity.C3.UndeclaredFlowMasterRefsConsumedByCosmetic` | `test_RegistryParity_C3_UndeclaredFlowMasterRefsConsumedByCosmetic` | C3 缺陷登记 2/3 | `PIPELINE_BLOCK.md:75` |
| `INTEGRATION.RegistryParity.C3.UndeclaredFlowBadcolReportWrittenByCosmetic` | `test_RegistryParity_C3_UndeclaredFlowBadcolReportWrittenByCosmetic` | C3 缺陷登记 3/3 | `PIPELINE_BLOCK.md:75` |
| `INTEGRATION.RegistryParity.C3bCarrierConsistency` | `test_RegistryParity_C3bCarrierConsistency` | C3b 载体一致 | `PIPELINE_BLOCK.md:94` |
| `INTEGRATION.RegistryParity.PcC4Direction` | `test_RegistryParity_PcC4Direction` | PC-C4 方向一致（**仅注册表侧**） | `PIPELINE_BLOCK.md:95`, :21 |
| `INTEGRATION.RegistryParity.PcC5CarrierContract` | `test_RegistryParity_PcC5CarrierContract` | PC-C5 载体合同（**基线判红**） | `PIPELINE_BLOCK.md:96`, :21 |
| `INTEGRATION.RegistryParity.PcC6NonDegenerate` | `test_RegistryParity_PcC6NonDegenerate` | PC-C6 非退化 | `PIPELINE_BLOCK.md:97` |
| `INTEGRATION.RegistryParity.IrC4NoPhantomEdge` | `test_RegistryParity_IrC4NoPhantomEdge` | IR-C4 无幻边（双向） | `PIPELINE_BLOCK.md:112`, :22 |
| `INTEGRATION.RegistryParity.IrC5TopologicalOrder` | `test_RegistryParity_IrC5TopologicalOrder` | IR-C5 序为拓扑序（**限同阶段**） | `PIPELINE_BLOCK.md:113`, :22 |
| `INTEGRATION.RegistryParity.IrC5bDeclaredOrder` | `test_RegistryParity_IrC5bDeclaredOrder` | IR-C5b 声明序一致 | `PIPELINE_BLOCK.md:114` + `gen_block_flow_spec.py:50` |
| `INTEGRATION.RegistryParity.IrC6PsfAfterWcs` | `test_RegistryParity_IrC6PsfAfterWcs` | IR-C6 psf 在 wcs 之后 | `PIPELINE_BLOCK.md:115` |
| `INTEGRATION.RegistryParity.IrC7NonDegenerate` | `test_RegistryParity_IrC7NonDegenerate` | IR-C7 非退化（fail-closed） | `PIPELINE_BLOCK.md:116` |
| `INTEGRATION.RegistryParity.IrC8NoExecutionSurface` | `test_RegistryParity_IrC8NoExecutionSurface` | IR-C8 **缺在位执行面** | `PIPELINE_BLOCK.md:117` |
| `INTEGRATION.RegistryParity.IrC4BridgePrecondition` | `test_RegistryParity_IrC4BridgePrecondition` | IR-C4 桥表前提缺失（**基线判红**） | `PIPELINE_BLOCK.md:112` |
| `INTEGRATION.RegistryParity.Neg.PhantomEdge` | `test_RegistryParity_Neg_PhantomEdge` | 负例：声明不存在的边 | `PIPELINE_BLOCK.md:99`, :112 |
| `INTEGRATION.RegistryParity.Neg.MissingRealEdge` | `test_RegistryParity_Neg_MissingRealEdge` | 负例：漏声明真实流 | `PIPELINE_BLOCK.md:22` |
| `INTEGRATION.RegistryParity.Neg.DirectionReversed` | `test_RegistryParity_Neg_DirectionReversed` | 负例：方向写反 | `PIPELINE_BLOCK.md:21`, :95 |
| `INTEGRATION.RegistryParity.Neg.AnchorTokenDeleted` | `test_RegistryParity_Neg_AnchorTokenDeleted` | 负例：锚点 token 删除 | `PIPELINE_BLOCK.md:99` |
| `INTEGRATION.RegistryParity.Neg.AnchorTokenMovedOutOfSymbol` | `test_RegistryParity_Neg_AnchorTokenMovedOutOfSymbol` | 负例：锚点 token 移出符号 | `PIPELINE_BLOCK.md:99` |
| `INTEGRATION.RegistryParity.Neg.SymbolMissing` | `test_RegistryParity_Neg_SymbolMissing` | 负例：符号不存在 | `PIPELINE_BLOCK.md:99` |
| `INTEGRATION.RegistryParity.Neg.AnchorFileMissing` | `test_RegistryParity_Neg_AnchorFileMissing` | 负例：文件不存在 | `PIPELINE_BLOCK.md:99` |
| `INTEGRATION.RegistryParity.Neg.CrossStageEdge` | `test_RegistryParity_Neg_CrossStageEdge` | 负例：跨阶段边 | `PIPELINE_BLOCK.md:96` |
| `INTEGRATION.RegistryParity.Neg.CarrierContractMissing` | `test_RegistryParity_Neg_CarrierContractMissing` | 负例：载体合同缺失 | `PIPELINE_BLOCK.md:96` |
| `INTEGRATION.RegistryParity.Neg.PortWithoutAnchor` | `test_RegistryParity_Neg_PortWithoutAnchor` | 负例：端口无锚点 | `PIPELINE_BLOCK.md:91` |
| `INTEGRATION.RegistryParity.Neg.NodeFunctionRenamed` | `test_RegistryParity_Neg_NodeFunctionRenamed` | 负例：节点函数改名 | `PIPELINE_BLOCK.md:99` |
| `INTEGRATION.RegistryParity.Neg.EmptyRegistry` | `test_RegistryParity_Neg_EmptyRegistry` | 负例：空注册表 | `PIPELINE_BLOCK.md:97`, :78 |
| `INTEGRATION.RegistryParity.Neg.IrC5bDeclaredOrder` | `test_RegistryParity_Neg_IrC5bDeclaredOrder` | 负例：IR 声明序漂移 | `PIPELINE_BLOCK.md:114` + `gen_block_flow_spec.py:50` |
| `INTEGRATION.RegistryParity.Neg.C3bDirectionFlipped` | `test_RegistryParity_Neg_C3bDirectionFlipped` | 负例：HiPS 端口方向写反 | `PIPELINE_BLOCK.md:94` |
| `INTEGRATION.RegistryParity.Neg.MaskingSurvivesBracesInStringsAndComments` | 同名 | 负例：字符串/注释里的花括号不扰动函数体定位 | 派单明写；C2 定位器自证 |
| `INTEGRATION.RegistryParity.Neg.C3PhantomArtifactLiteral` | `test_RegistryParity_Neg_C3PhantomArtifactLiteral` | 负例：再注入一条未声明流（**基线已红时的有牙自证**） | `PIPELINE_BLOCK.md:75` |
| `INTEGRATION.RegistryParity.Neg.IrSwapPsfWcsOrder` | `test_RegistryParity_Neg_IrSwapPsfWcsOrder` | 负例：交换 psf/wcs 节点序 | `PIPELINE_BLOCK.md:119-120` |
| `INTEGRATION.RegistryParity.Neg.IrWcsReadsP1Sources` | `test_RegistryParity_Neg_IrWcsReadsP1Sources` | 负例：注入 `wcs ← p1_sources` 幻边 | `PIPELINE_BLOCK.md:119` |
| `INTEGRATION.RegistryParity.Neg.IrPsfWithoutP1Wcs` | `test_RegistryParity_Neg_IrPsfWithoutP1Wcs` | 负例：移除 psf 的 `p1_wcs` 输入 | `PIPELINE_BLOCK.md:119` |
| `INTEGRATION.RegistryParity.Neg.IrPhotometryReadsP1Psf` | `test_RegistryParity_Neg_IrPhotometryReadsP1Psf` | 负例：注入 `photometry ← p1_psf` 幻边 | `PIPELINE_BLOCK.md:120` |
| `INTEGRATION.RegistryParity.Neg.IrPortNameNotInDescriptor` | `test_RegistryParity_Neg_IrPortNameNotInDescriptor` | 负例：IR 端口名改成 descriptor 里不存在的名字 | `PIPELINE_BLOCK.md:120` |
| `INTEGRATION.RegistryParity.Neg.BridgeTableInjected` | `test_RegistryParity_Neg_BridgeTableInjected` | 负例：注入桥表后 IR-C4 前提转绿（S2 对照） | `VALIDATION_EVIDENCE.md:189` |
| `INTEGRATION.RegistryParity.Neg.BridgeDecoyRejected` | `test_RegistryParity_Neg_BridgeDecoyRejected` | 负例：同名不同形的桥表不被接受 | `VALIDATION_EVIDENCE.md:190` |
| `INTEGRATION.RegistryParity.Neg.IrC8VocabularyInjected` | `test_RegistryParity_Neg_IrC8VocabularyInjected` | 负例：descriptor↔IR 端口名对上后 IR-C8 转可判 | `PIPELINE_BLOCK.md:117` |
| `INTEGRATION.RegistryParity.SelfCheck.JudgesNotVacuouslyTrue` | `test_RegistryParity_SelfCheck_JudgesNotVacuouslyTrue` | 零对象守卫汇总自证（各判定器实算数 > 0） | `VALIDATION_EVIDENCE.md:170-173` |

## 事实源（锚点，硬编码路径必须存活；失效以 `ANCHOR_STALE` 具名判红）

- 端口事实源 `lib/infrastructure/pipeline/module_ports.registry.json`
- 管线 IR `eng/contracts/block_flow/stage_block_flow.json`
- 代码面 `lib/infrastructure/scheduler/src/module_adapters.cpp`（20 个 op 入口 +
  22 个 `ModuleDescriptor` 定义）
- 代码面 `lib/algorithms/drizzle/healpix_drizzle/astro_sphere_sink.cpp`
  （`write_hips_phase1` 模板入口，:309）

## 基线状态（HEAD `bf944fad`，本单交付时的实测）

| 判据 | 读数 | 状态 |
|---|---|---|
| C1 | 20 module / 20 operation / 85 端口 / 101 锚点，`n_violations=0` | 绿 |
| C2 | 101/101 锚点解析，`n_violations=0` | 绿 |
| **C3** | 抽到 **77** 个产物 token（21 个符号），**3 条未声明** | **红（缺陷登记 R1）** |
| C3b | 触发 7 个 module（2 生产 + 5 消费），`n_violations=0` | 绿 |
| PC-C4（注册表侧） | `n_violations=0` | 绿 |
| **PC-C5** | **恰好 1 条 `EXTERNAL_INPUT_CARRIER`**（`output_dir_file` 跨阶段产物 0 个） | **红（缺陷登记 R2）** |
| PC-C6 | `n_violations=0` | 绿 |
| IR-C4 | 51 条注册表边逐条核对，`n_violations=0` | 绿 |
| IR-C5（同阶段） | 43 条同阶段边，0 违例 | 绿 |
| IR-C5b | 三阶段声明序全等 | 绿 |
| IR-C6 | pos(psf)=15 > pos(wcs)=14 且读 `p1_wcs` | 绿 |
| IR-C7 | `n_violations=0` | 绿 |
| **IR-C8** | 端口名口径 85/0；DATA id 桥 55/30 ⇒ **无在位执行面** | **登记（非判红）** |
| **IR-C4 桥表前提** | `PHASE1_ARTIFACT_TO_PORT` 在 `lib/`+`eng/`（排除 `eng/tests`）**0 命中** | **红（缺陷登记 R3）** |

## 如实登记的红项（代码 / 正本缺陷候选，不替负责人裁定）

### R1 C3：`master_refs.json` 与 `badcol_report.json` 是真实存在但未声明的数据流

判据原文：`docs/engineering/contracts/PIPELINE_BLOCK.md:75`
「代码中真实存在的数据流未在注册表声明 ⇒ 判红」。

实测证据（`lib/infrastructure/scheduler/src/module_adapters.cpp`）：

| # | 产物 token | 方向 | 代码位置 | 注册表命中 |
|---|---|---|---|---|
| 1 | `/master_refs.json` | `acsd.phase1.calibration` **写** | `:2761` `const std::string refs_path = out_dir + "/master_refs.json";` | `master_refs` 在 `module_ports.registry.json` 全文 **0 次** |
| 2 | `/master_refs.json` | `acsd.phase1.cosmetic` **读**（回填母版） | `:2910` `const std::string rp = doc.value("output_dir", …) + "/master_refs.json";` | 同上 0 次 |
| 3 | `/badcol_report.json` | `acsd.phase1.cosmetic` **写** | `:3290` `… + "/badcol_report.json"` | `badcol` 在注册表全文 **0 次** |

代码自带的语义说明（`:2717-2725`）：「处置：本节点把**实际消费**的母版落成边车
`master_refs.json`（与产物同目录），供下游回填……这是"把母版回填进 config"的唯一事实源。」
——即作者本人把它当成一条**跨节点的数据流**。

**两种读法并列（需负责人裁定，本单不替裁）**：

- (a) **代码缺陷 / 注册表漏声明**：`master_refs.json` 与 `badcol_report.json` 都是
  `output_dir` 下的具体产物（`PIPELINE_BLOCK.md:19`「注册表里每个端口对应 `output_dir`
  下的一个具体产物」），按 `PIPELINE_BLOCK.md:22`「同阶段内『A 的输出端口名 == B 的输入
  端口名』即一条依赖边」，calibration→cosmetic 这条边**应当**有端口，缺了就判红。
- (b) **正本口径缺失**：若「校准副产物 / 坏列降级报告」这类**旁路产物**按口径本就不进
  端口面（它们既不是管线块、也不参与调度排序），那判红的是**合同**——`PIPELINE_BLOCK.md`
  没有写「哪些磁盘产物不构成端口」的排除面，C3 因此无法区分「数据流」与「旁路日志」。
  处置应是给 C3 补一条排除面条款，或给注册表补一条显式的「非端口产物」声明。

### R2 PC-C5：`acsd.phase1.photometry:p1_photscale` 是无生产者的非 `config_path` 输入

判据原文：`docs/engineering/contracts/PIPELINE_BLOCK.md:96`「节点间端口 `carrier` ∈
{`output_dir_file`, `hips_product_tree`}」+ `:21` 逐字的载体枚举
「`config_path`（阶段外部输入，本阶段无生产者）」。

实测证据：该端口 `carrier=output_dir_file`、`direction=input`、**全注册表无任何生产者**
（`module_ports.registry.json:424-439`；派生 IR 也记成外部输入
`eng/contracts/block_flow/stage_block_flow.json:411-417`：
`"lifecycle": "EXTERNAL_IN"`、`"produced_by": []`）；代码侧是可选外部标定通道
（`lib/infrastructure/scheduler/src/module_adapters.cpp:6071`
`const std::string sp = out_dir + "/p1_photscale.json";`）。

同形态但合规的对照：`lights` / `master_bias` / `master_dark` / `master_flat` /
`run_context` 五条都是 `carrier=config_path` + `direction=input`。

**两种读法并列（需负责人裁定）**：

- (a) **注册表漏声明**：`p1_photscale.json` 应由某个 phase1 节点生产，`carrier` 取值本身没错；
- (b) **`carrier` 取值选错**：它就是阶段外部输入，`carrier` 应当是 `config_path`，
  当前的 `output_dir_file` 与「本阶段无生产者」矛盾。

### R3 IR-C4 的桥表前提缺失：`PHASE1_ARTIFACT_TO_PORT` 全仓不存在

判据原文：`docs/engineering/contracts/PIPELINE_BLOCK.md:112` 末句「IR 产物身份须在
`PHASE1_ARTIFACT_TO_PORT` 桥表中登记（未登记即判红，新增产物必须同步该桥）」。

实测证据：在 `lib/` + `eng/` 下扫 `.cpp/.h/.hpp/.py/.json`
（排除 `eng/tests` 本单元自指面与 `run/` 历史交付件面）**0 命中**。全仓仅 3 处文本命中：
`docs/engineering/contracts/PIPELINE_BLOCK.md:112`（条款自身）、
`run/GOVERN-08/审核包-R2/审稿-R3-T3-代码与文档交叉面.md:284`
（「须修-21 …… 判红依据本身不可执行」）、
`run/GOVERN-08/审核包-R2/审稿-RD07…` 两份审稿的转述。

⇒ IR-C4 的「未登记即判红」这半句**判红依据本身不存在**，故 `test_RegistryParity_IrC4NoPhantomEdge`
只实现「幻边」这半句并在其 docstring 里如实登记该缺口；本条单列成一条会红的
`test_RegistryParity_IrC4BridgePrecondition`。

### R4 IR-C8 在当前代码上缺在位执行面（**不判红**，登记）

判据原文：`docs/engineering/contracts/PIPELINE_BLOCK.md:117`「IR 每个节点的输入/输出
端口名必须出现在 `module_adapters.cpp` 对应 descriptor 的端口表里」。

实测证据：descriptor 端口表**在位**（20/20 module 有对应 descriptor，解析出 22 个
`ModuleDescriptor` 定义），但两侧端口名分属**互不相交的两套词表**：

- IR / 注册表（产物身份词表）：`p1_cleaned` `p1_wcs` `p3_props` `mosaic_hips` …
- descriptor（typed 绑定短名）：`cleaned` `wcs` `props` `mosaic` …
  （出处 `module_adapters.cpp:710-713`、`:905-916`、`:828-832`）

实测：IR 端口引用 **85 条**、逐字命中 **0 条**；退一步用 `data_schema_id` ↔ descriptor
`data_id` 桥接，命中 **55** / 未命中 **30**，亦不闭合。descriptor 端口表本身也不完整
（`p1_writer_descriptor` 只有 2 个端口而注册表同模块声明 5 个；`p2_upm_apply_descriptor`
3 个 vs 6 个；`p2_write_descriptor` 2 个 vs 8 个）。

⇒ **IR-C8 缺在位对象，不是「判红」而是「判据不可执行」**。按 `TEST.md` §2「恒真的比较
没有证据资格」与派单纪律 C5，本文件**不写一条永远红的 IR-C8**，改写成
`test_RegistryParity_IrC8NoExecutionSurface`：把「不存在可判的公共词表」这个事实与它的
三条实测读数固化成断言，并配一条负例（往 descriptor 端口表注入一个 IR 端口名后断言
IR-C8 **转为可判**），证明这条判据既非恒真也非恒假。**需负责人裁定：补桥表，还是撤判据。**

## PC-C4 代码侧角色推断未实现（如实登记）

`PIPELINE_BLOCK.md:95` 要求「变量流分析推断出的读写角色必须包含声明的 `direction`；
推不出角色即判红」。本单**不具备**做这件事的条件：101 条锚点里多数 token 出现在
helper 调用的实参位（例如 `p1_op_calibrate:2394`
`bias = p1_read_image(doc["master_bias"].get<std::string>());`、`:2761` 的落盘、
`:2910` 的回填读），判定「这次调用是读还是写」需要覆盖
`aio_fs::read_all` / `std::fopen(…, "wb")` / 落盘 helper / 跨函数传参的**完整数据流**；
只做局部判定会给出与真实角色相反的结论（例如把 `out_dir + "/master_refs.json"` 判成读）。

按派单纪律「不得硬写一条假装能判红的测试」，`test_RegistryParity_PcC4Direction` **只落注册表
侧可判的三条**（`carrier=config_path ⇒ direction=input`、module 内端口名唯一、同阶段
产物身份唯一生产者），并在断言里显式标注 **PC-C4 的代码侧判红面本单未覆盖**。

## 负例纪律

所有负例在 `tempfile` 造的**临时副本**上跑（`_registry_parity.Sandbox`），仓库一个字节
都不动（`VALIDATION_EVIDENCE.md:197`「反例复跑必须隔离」）。注入点一律落在**该判定自身的
判定逻辑**上（`VALIDATION_EVIDENCE.md:196` S6），并同时给出「注入前 / 注入后」对照读数，
防止「一律判红」的假判定（S2）。

`TEST.md` §4 第一档「元数据、端口、选择结果 = **精确一致**」是本单的比较口径：端口名、
方向、载体、节点序全部逐项精确相等，不使用任何浮点容差。
"""

from __future__ import annotations

import os
import re

import pytest

import _registry_parity as R

# 仓库实况事实面。锚点缺失 → `ANCHOR_STALE` 具名判红（`VALIDATION_EVIDENCE.md:413`），
# 解析不到 → `RESOLUTION_FAILED` 具名判红（`:412`）。**绝不 skip、绝不当通过**。
@pytest.fixture(scope="module")
def facts() -> R.Facts:
    return R.build_repo_facts()


def _fail(error: R.ParityError) -> None:
    """把 `ParityError` 转成具名 pytest 失败（不 skip、不静默）。"""
    pytest.fail("%s: %s" % (error.code, error.detail))


def _fail_if(errors, criterion: str) -> None:
    """有违例即具名判红，附检查名、对象计数与前 10 条对象（`VALIDATION_EVIDENCE.md:241`）。"""
    if errors:
        pytest.fail("%s\n%s" % (criterion, R.render(errors)))


def _judge(fn, facts_obj: R.Facts, criterion: str, *args):
    """统一入口：判定器抛 `ParityError` → 具名失败；返回违例表。"""
    try:
        return fn(facts_obj, *args)
    except R.ParityError as exc:
        _fail(exc)


def _sandbox() -> R.Sandbox:
    return R.Sandbox()


# ==========================================================================
# A. 正例
# ==========================================================================


def test_RegistryParity_C1Structure(facts: R.Facts) -> None:
    """INTEGRATION.RegistryParity.C1Structure —— C1 结构判据。

    断言内容（逐条对应 `PIPELINE_BLOCK.md:91`）：
    注册表 `registry_schema` / `registry_version` 为 v2 冻结值；`carrier_contract` 存在且
    其 `carriers` 覆盖三个载体枚举；每个 module **恰一个** operation；每个端口的
    `name/direction/carrier/artifacts/code` 五个字段齐全；`direction` 落在封闭枚举
    {`input`,`output`}；`carrier` 落在枚举 {`output_dir_file`,`hips_product_tree`,`config_path`}。

    来源依据：`docs/engineering/contracts/PIPELINE_BLOCK.md:91`
    「C1 结构 | 注册表 v2 + `carrier_contract`；每 module 恰一个 operation；
    端口 `name/direction/carrier/artifacts/code` 齐全」；载体枚举取自 `:21`。
    载体枚举全集取自 `PIPELINE_BLOCK.md:21` 逐字
    「取值 `output_dir_file` / `hips_product_tree` / `config_path`」。

    零对象守卫（`VALIDATION_EVIDENCE.md:170-173`）：本条读完**真实对象**——
    基线实算 20 个 module、20 个 operation、85 个端口、101 条锚点，
    `n_ports == 0` 时本判定会因 `ports` 为空而恒真，故在此显式断言实算数 > 0。

    负例：`test_RegistryParity_Neg_PortWithoutAnchor`（端口无锚点）、
    `test_RegistryParity_Neg_EmptyRegistry`（注册表为空）。
    """
    assert len(facts.modules) > 0, "C1 在零 module 上是恒真（Cf. TEST.md:26）"
    assert len(facts.ports()) > 0, "C1 在零端口上是恒真"
    _fail_if(_judge(R.judge_c1_structure, facts, "C1"), "C1_STRUCTURE_VIOLATION")


def test_RegistryParity_C2AnchorResolves(facts: R.Facts) -> None:
    """INTEGRATION.RegistryParity.C2AnchorResolves —— C2 声明⇒实现。

    逐条对应 `PIPELINE_BLOCK.md:92`「每条端口的 `code` 锚点必须解析：文件存在、
    `symbol` 在本文件内唯一解析到函数体、`token` 落在该函数体内」。

    解析器自研要点（`_registry_parity.CppUnit`）：
    - **等长掩码**：注释内容、字符串/字符字面量内容换成空格，保留引号与全部长度，
      因此掩码下标可原样换算成原文行号；
    - **函数体定位**：从 `symbol` 的形参表右括号起跳空白，要求下一个非空字符是 `{`
      才算定义（前向声明是 `;`、显式实例化也是 `;`、构造初始化列表是 `:`），
      再用花括号配平取函数体区间。配平在**掩码**上做，所以注释与字符串里的花括号
      不会带偏计数。

    实测读数（HEAD `bf944fad`）：**101/101** 锚点全部解析成功，0 违例。
    锚点分布：`module_adapters.cpp` 100 条 + `astro_sphere_sink.cpp` 1 条
    （`write_hips_phase1`，`:309` 的模板函数定义；`:627/:630` 的两个显式实例化不误判）。
    21 个不同 `symbol` 全部唯一解析。

    本条同时是 C3 / C3b 的**共同前置**：这两条的判定器在符号解析不到时
    fail-closed 抛 `RESOLUTION_FAILED`，不会静默按「无产物」处理。

    负例：`test_RegistryParity_Neg_AnchorTokenDeleted`、
    `test_RegistryParity_Neg_AnchorTokenMovedOutOfSymbol`、
    `test_RegistryParity_Neg_SymbolMissing`、
    `test_RegistryParity_Neg_AnchorFileMissing`、
    `test_RegistryParity_Neg_NodeFunctionRenamed`。
    """
    anchors = facts.anchors()
    assert len(anchors) == 101, (
        "锚点数变了（基线 101）——先确认是产品改动还是读数漂移；"
        "PC-C6 的锚点下界依赖它（VALIDATION_EVIDENCE.md:192 S5）")
    _fail_if(_judge(R.judge_c2_anchor_resolves, facts, "C2"), "C2_ANCHOR_VIOLATION")


def test_RegistryParity_C3ImplementationToDeclaration(facts: R.Facts) -> None:
    """INTEGRATION.RegistryParity.C3ImplementationToDeclaration —— C3 实现⇒声明。

    判据原文：`docs/engineering/contracts/PIPELINE_BLOCK.md:93`
    「以**代码侧闭合文法**（与注册表无关）抽出每个节点函数触碰的产物 token，
    必须全部已在注册表声明」；:75「代码中真实存在的数据流未在注册表声明 ⇒ 判红」。

    文法 G1（闭合、与注册表无关，定义见 `_registry_parity.extract_g1_tokens`）：
    在节点函数体内扫出全部完整字符串字面量（扫描在掩码上做、内容从原文切出），
    取其内容全文匹配 ``/`<名字>.<json|bin|fits>` `` 者为被触及的产物 token。

    已否证的文法（实测后不用）：`*_path/*_dir/*_file` 标识符族抽 171 个、161 个未声明
    ——抽到的是函数内局部变量；`*_hash` 族抽的是 manifest 内部字段。两者都不是「产物 token」。

    **基线状态：判红，3 条**（缺陷登记 R1，见文件头「如实登记的红项」一节）。
    判红是**代码 / 正本缺陷信号**，不是本测试的缺陷。按 `AGENTS.md` §8 与
    `TEST.md` §1「报告红项而不裁决合入」，本条在缺陷修好前会一直红。

    零对象守卫：本条基线抽到 **76 个** G1 token（21 个符号），> 0；若哪天文法抽不出
    任何 token，本条会变成恒真，因此这里同时由
    `test_RegistryParity_C3_UndeclaredFlowMasterRefsProducedByCalibration` 等三条具名断言
    与负例 `test_RegistryParity_Neg_C3PhantomArtifactLiteral` 锁住「抽取逻辑确实在做功」。
    """
    _fail_if(_judge(R.judge_c3_implementation_to_declaration, facts, "C3"),
             "C3_IMPLEMENTATION_TO_DECLARATION_VIOLATION")


def _c3_violation_subjects(facts_obj: R.Facts) -> set:
    return R.subjects(_judge(R.judge_c3_implementation_to_declaration, facts_obj, "C3"))


def test_RegistryParity_C3_UndeclaredFlowMasterRefsProducedByCalibration(facts: R.Facts) -> None:
    """INTEGRATION.RegistryParity.C3.UndeclaredFlowMasterRefsProducedByCalibration
    —— 缺陷登记 R1 第 1 条：`acsd.phase1.calibration` **写** `master_refs.json`，
    注册表未声明。

    这条断言**不是**「期望它红」，而是**把缺陷钉死**：断言判定器在基线上确实
    **点名**了这条具体数据流。修好之后本条会红（那时它会被当作回归护栏）。

    证据：
    - 代码 `lib/infrastructure/scheduler/src/module_adapters.cpp:2761`
      `const std::string refs_path = out_dir + "/master_refs.json";`（在
      `p1_op_calibrate` 的函数体内，函数体 2357..2793 行）；
    - 代码自述 `:2717-2725`：「处置：本节点把**实际消费**的母版落成边车
      `master_refs.json`（与产物同目录），供下游回填……这是"把母版回填进 config"的唯一事实源。」；
    - 注册表 `lib/infrastructure/pipeline/module_ports.registry.json` 全文对
      `master_refs` 命中 **0** 次。

    来源依据：`docs/engineering/contracts/PIPELINE_BLOCK.md:75`。
    两种读法（(a) 注册表漏声明 / (b) 合同缺「非端口产物」排除面）见文件头 R1 节，
    **本单不替负责人裁定**。
    """
    assert "acsd.phase1.calibration@p1_op_calibrate" in _c3_violation_subjects(facts), (
        "未检出 calibration 写 /master_refs.json 的未声明流（module_adapters.cpp:2761）——"
        "判定器失效或缺陷已修，需人工确认")


def test_RegistryParity_C3_UndeclaredFlowMasterRefsConsumedByCosmetic(facts: R.Facts) -> None:
    """INTEGRATION.RegistryParity.C3.UndeclaredFlowMasterRefsConsumedByCosmetic
    —— 缺陷登记 R1 第 2 条：`acsd.phase1.cosmetic` **读** `master_refs.json`
    做母版回填，注册表未声明。

    读侧与写侧构成 `PIPELINE_BLOCK.md:22`「同阶段内『A 的输出端口名 == B 的输入端口名』
    即一条依赖边」意义上的**一条完整跨节点数据流**，两端都没有端口声明。

    证据：
    - 代码 `lib/infrastructure/scheduler/src/module_adapters.cpp:2910`
      `const std::string rp = doc.value("output_dir", std::string(".")) + "/master_refs.json";`
      （在 `p1_op_cosmetic` 函数体 2822..4320 行内）；
    - 代码 `:2903-2906` 自述：「节点 config 未带母版键时，从 cal 节点落的
      `master_refs.json` 回填……回填是**显式可追**的：状态字符串写成
      `"backfilled:master_refs.json"`」；
    - 注册表对 `master_refs` 命中 **0** 次。

    来源依据：`docs/engineering/contracts/PIPELINE_BLOCK.md:75`。
    """
    assert "acsd.phase1.cosmetic@p1_op_cosmetic" in _c3_violation_subjects(facts), (
        "未检出 cosmetic 读 /master_refs.json 的未声明流（module_adapters.cpp:2910）")


def test_RegistryParity_C3_UndeclaredFlowBadcolReportWrittenByCosmetic(facts: R.Facts) -> None:
    """INTEGRATION.RegistryParity.C3.UndeclaredFlowBadcolReportWrittenByCosmetic
    —— 缺陷登记 R1 第 3 条：`acsd.phase1.cosmetic` **写** `badcol_report.json`
    （坏列检测的具名降级报告），注册表未声明。

    与前两条不同，这条**没有消费端**——它在全仓只有写、没有读。按
    `gen_block_flow_spec.py:9-11` 的派生规则，「消费者数 == 0 且非终产物 ⇒ STAGE」，
    无消费者的产物**仍要**有端口声明（对照：`p1_final` / `p1_products` 这两个
    无消费者的阶段终产物就都在注册表里）。

    证据：
    - 代码 `lib/infrastructure/scheduler/src/module_adapters.cpp:3290`
      `… + "/badcol_report.json"`（在 `p1_op_cosmetic` 函数体内）；
    - 代码 `:3246` 自述：「── LINDEF-CLOSE-01：具名降级与三路径计数的**落盘**面
      （`badcol_report.json`）」；
    - 注册表对 `badcol` / `bad_column` 命中均为 **0** 次。

    来源依据：`docs/engineering/contracts/PIPELINE_BLOCK.md:75`。
    """
    subject = "acsd.phase1.cosmetic@p1_op_cosmetic"
    detail = " ".join(v.detail for v in _judge(R.judge_c3_implementation_to_declaration,
                                              facts, "C3") if v.subject == subject)
    assert "/badcol_report.json" in detail, (
        "未检出 cosmetic 写 /badcol_report.json 的未声明流（module_adapters.cpp:3290）")


def test_RegistryParity_C3bCarrierConsistency(facts: R.Facts) -> None:
    """INTEGRATION.RegistryParity.C3bCarrierConsistency —— C3b 载体一致。

    判据原文：`docs/engineering/contracts/PIPELINE_BLOCK.md:94`
    「节点触碰 HiPS 产品树 ⇒ 必须有对应 `carrier=hips_product_tree` 且方向一致的端口」。

    「触碰」的判定是**代码侧命名约定闭合**的（不看注册表）：
    生产者角色 = 调用 `aio_hips_product_begin` / `aio_hips_write_*` / `aio_hips_finalize`；
    消费者角色 = 调用 `aio_hips_read_*`。两者都只按 `aio_hips_` 这个代码侧命名空间的
    前缀判定。

    实测触发规模（HEAD `bf944fad`）：生产者角色 **2** 个 module
    （`acsd.phase1.drizzle`、`acsd.phase2.write`），消费者角色 **5** 个 module
    （`acsd.phase1.writer`、`acsd.phase2.upm-apply`、`acsd.phase2.reject`、
    `acsd.phase2.integrate`），共 7 个触发、`n_violations=0`，方向全部一致。

    本条**非恒真**：删除任一 hips 端口即判红（见负例 `test_RegistryParity_Neg_C3bDirectionFlipped`）。
    本条**非恒假**：`⇒` 方向成立（触碰 ⇒ 有端口），反向不成立且不要求成立
    （5 个只经 helper 间接读 HiPS 树的 module 走 `p2_coverage_from_artifact` 之类，
    函数体内没有 `aio_hips_*` 调用，但它们**确实**声明了 hips 端口）。
    """
    _fail_if(_judge(R.judge_c3b_carrier_consistency, facts, "C3b"),
             "C3B_CARRIER_VIOLATION")


def test_RegistryParity_PcC4Direction(facts: R.Facts) -> None:
    """INTEGRATION.RegistryParity.PcC4Direction —— PC-C4 方向一致（**仅注册表侧**）。

    判据原文：`docs/engineering/contracts/PIPELINE_BLOCK.md:95`。本单只落**注册表侧可判**
    的三条子句：

    - (a) `carrier=config_path ⇒ direction=input`。来源 `PIPELINE_BLOCK.md:21` 逐字：
      「`config_path`（阶段外部输入，本阶段无生产者）」——本阶段无生产者者只能是输入。
      基线实算 5 条 `config_path` 端口，全部 `input`。
    - (b) 同一 module 内端口 `name` 唯一。来源 `PIPELINE_BLOCK.md:30`
      「块名（阶段内唯一，小写蛇形）」+ :40-44 生命周期 DAG 校验第 1 条。
    - (c) 同一产物身份在同一 phase 内不得被两个 module 同时生产。来源
      `PIPELINE_BLOCK.md:42` 第 3 条「同一块被重复生产 ⇒ 非法」。

    **代码侧角色推断本单未实现（如实登记）**：`PIPELINE_BLOCK.md:95` 要求
    「变量流分析推断出的读写角色必须包含声明的 `direction`；推不出角色即判红」。
    101 条锚点里多数 token 出现在 helper 调用的实参位
    （如 `module_adapters.cpp:2394` `bias = p1_read_image(doc["master_bias"]…)`、
    `:2761` 的落盘、`:2910` 的回填读），判「读还是写」需要覆盖
    `aio_fs::read_all` / `std::fopen(…,"wb")` / 落盘 helper / 跨函数传参的**完整数据流**；
    只做局部判定会给出与真实角色相反的结论。故 **PC-C4 的代码侧判红面本单未覆盖**，
    不写一条假装能判红的测试。

    负例：`test_RegistryParity_Neg_DirectionReversed`（对子句 (a) 注入方向写反）。
    """
    _fail_if(_judge(R.judge_pc_c4_direction, facts, "PC-C4"), "PC-C4_DIRECTION_VIOLATION")


def test_RegistryParity_PcC5CarrierContract(facts: R.Facts) -> None:
    """INTEGRATION.RegistryParity.PcC5CarrierContract —— PC-C5 载体合同。

    判据原文：`docs/engineering/contracts/PIPELINE_BLOCK.md:96`
    「节点间端口 `carrier` ∈ {`output_dir_file`, `hips_product_tree`}；
    `output_dir` 产物身份限本阶段；`carrier_contract` 必须显式声明 HiPS 产品树为跨阶段载体」，
    外加 :21 载体枚举的推论子句（外部输入 ⇒ `config_path`）。

    四条子句：
    1. 有生产或有消费的端口（= 节点间端口）载体必须 ∈ 两值集合。
    2. `carrier=output_dir_file` 的产物身份不得跨阶段（生产阶段 ≠ 消费阶段）。
       基线实算：`frame_hips` / `mosaic_hips` / `p1_stack` 三个 HiPS 树产物跨阶段，
       载体都是 `hips_product_tree`；`output_dir_file` 跨阶段产物 **0** 个。
    3. `carrier_contract` 存在、`carriers.hips_product_tree` 在位、`statement` 显式点名 HiPS。
    4. （:21 推论）`direction=input` 且 `carrier ∈ {output_dir_file, hips_product_tree}`
       的端口，其产物身份必须有生产者；否则它是 :21 定义的「阶段外部输入（本阶段无
       生产者）」，那一档的载体枚举值是 `config_path`。

    **基线状态：判红，1 条**（缺陷登记 R2：`acsd.phase1.photometry:p1_photscale`，
    `carrier=output_dir_file` + `direction=input` + 全注册表无生产者）。

    负例：`test_RegistryParity_Neg_CrossStageEdge`（子句 2）、
    `test_RegistryParity_Neg_CarrierContractMissing`（子句 3）。
    """
    _fail_if(_judge(R.judge_pc_c5_carrier_contract, facts, "PC-C5"),
             "PC-C5_CARRIER_CONTRACT_VIOLATION")


def test_RegistryParity_PcC6NonDegenerate(facts: R.Facts) -> None:
    """INTEGRATION.RegistryParity.PcC6NonDegenerate —— PC-C6 非退化。

    判据原文：`docs/engineering/contracts/PIPELINE_BLOCK.md:97`
    「模块数/端口数/代码 token 数/生产→消费边数均有下界；空注册表判红」。

    **每个下界都写明来源，零魔数**（`VALIDATION_EVIDENCE.md:192` S5「规模锚用实算值…
    禁止任何无来源的魔数阈值」）：

    | 下界 | 取值 | 来源 |
    |---|---|---|
    | module 数 | ≥ 3 | `gen_block_flow_spec.py:24` 的三相↔三阶段映射 + `PIPELINE_BLOCK.md:6` 三命令各占一阶段 |
    | operation 数 | = module 数 | `PIPELINE_BLOCK.md:91`「每 module 恰一个 operation」 |
    | 端口数 | ≥ 2 × module 数 | `PIPELINE_BLOCK.md:13`「模块从帧读入参块、产出新块写回」⇒ 至少 1 输入 1 输出 |
    | 锚点数 | ≥ 端口数 | `PIPELINE_BLOCK.md:91`「端口 … `code` 齐全」⇒ 每端口至少 1 条锚点 |
    | 生产→消费边数 | 每阶段 ≥ 1 | `VALIDATION_EVIDENCE.md:170-173` 零对象守卫第 1 条，逐阶段施加 |
    | 空注册表 | 判红 | `PIPELINE_BLOCK.md:97` + :78 |

    基线实算：20 module / 20 operation / 85 端口 / 101 锚点 / 三阶段边数均 > 0。

    负例：`test_RegistryParity_Neg_EmptyRegistry`。
    """
    _fail_if(_judge(R.judge_pc_c6_non_degenerate, facts, "PC-C6"),
             "PC-C6_NON_DEGENERATE_VIOLATION")


def test_RegistryParity_IrC4NoPhantomEdge(facts: R.Facts) -> None:
    """INTEGRATION.RegistryParity.IrC4NoPhantomEdge —— IR-C4 无幻边（**双向**）。

    判据原文：`docs/engineering/contracts/PIPELINE_BLOCK.md:112`
    「IR 声明的每条边必须由注册表端口图支持：该产物身份对应的注册表端口必须是生产模块的
    **输出**端口、且是消费模块的**输入**端口」+ `PIPELINE_BLOCK.md:22`
    「IR 的边集与注册表边集逐条相等」。

    两个方向都实现：
    (i)  **幻边**：IR 里每条 `reads` 都必须能在注册表端口图上找到「某模块 output 该身份」
         且「消费模块 input 该身份」；否则判红；
    (ii) **漏声明**：注册表端口图的每条生产→消费边都必须在 IR 里出现。

    基线实算：IR 读边 51 条全部有注册表支持；注册表边 51 条全部出现在 IR；
    `n_violations=0`。

    **本条未实现的子句（如实登记，缺陷登记 R3）**：`PIPELINE_BLOCK.md:112` 末句
    「IR 产物身份须在 `PHASE1_ARTIFACT_TO_PORT` 桥表中登记（未登记即判红，新增产物必须
    同步该桥）」。该桥表**全仓不存在**（`lib/`+`eng/` 下 `.cpp/.h/.hpp/.py/.json`
    排除 `eng/tests` 后 **0 命中**；全仓仅 `PIPELINE_BLOCK.md:112` 条款自身与
    `run/GOVERN-08/审核包-R2/审稿-R3-T3-代码与文档交叉面.md:284` 等审稿文本命中）。
    判红依据本身没有在位对象，故这半句单列为
    `test_RegistryParity_IrC4BridgePrecondition`。

    负例：`test_RegistryParity_Neg_PhantomEdge`、
    `test_RegistryParity_Neg_MissingRealEdge`、
    `test_RegistryParity_Neg_IrWcsReadsP1Sources`、
    `test_RegistryParity_Neg_IrPhotometryReadsP1Psf`、
    `test_RegistryParity_Neg_IrPsfWithoutP1Wcs`。
    """
    _fail_if(_judge(R.judge_ir_c4_no_phantom_edge, facts, "IR-C4"),
             "IR-C4_NO_PHANTOM_EDGE_VIOLATION")


def test_RegistryParity_IrC5TopologicalOrder(facts: R.Facts) -> None:
    """INTEGRATION.RegistryParity.IrC5TopologicalOrder —— IR-C5 序为拓扑序（**限同阶段**）。

    判据原文：`docs/engineering/contracts/PIPELINE_BLOCK.md:113`
    「注册表端口图 DAG 的每条边必须满足 `pos(上游) < pos(下游)`；
    IR 自身声明的边也必须与节点数组序一致」。

    **为什么限同阶段（两个来源，逐字）**：

    - `PIPELINE_BLOCK.md:22` 的依赖边定义带前缀：「**同阶段内**「A 的输出端口名 == B 的
      输入端口名」即一条依赖边；管线 IR 的节点序必须是该 DAG 的拓扑序」——
      跨阶段交换是三个平级命令之间的磁盘交接（`:17-21`「不跨阶段共享内存」），
      不是同一条内存管线的节点边；
    - `eng/tools/quality/gen_block_flow_spec.py:50` 的排序键是
      `nodes.sort(key=lambda n: (n["stage"], reg_index(reg, n["module_id"])))`
      —— `stage` 是**字典序**，所以 `stage_block_flow.json` 的节点数组序必然是
      `export → mosaic → normalize`（实测 `nodes[0].stage == "export"`、
      `nodes[-1].stage == "normalize"`）。

    两个读数并列，读者可自行复算：
    - **同阶段口径**：43 条同阶段边，**0 违例**（本条采用）；
    - **全数组口径**：51 条边里 8 条跨阶段边必然逆序（`frame_hips` 由
      `acsd.phase1.drizzle`[数组位 18] 产、`acsd.phase2.coverage`[位 5] 消；
      `mosaic_hips` 由 `acsd.phase2.write`[位 11] 产、`acsd.phase3.properties`[位 0] 消，
      共 5 + 3 = 8 条）⇒ 全数组口径与派生脚本自相矛盾、恒红，不可采纳。

    负例：`test_RegistryParity_Neg_IrSwapPsfWcsOrder`（交换 `psf`/`wcs` 节点序，
    同时打红 IR-C5 与 IR-C6）。
    """
    _fail_if(_judge(R.judge_ir_c5_topological_order, facts, "IR-C5"),
             "IR-C5_TOPOLOGICAL_ORDER_VIOLATION")


def test_RegistryParity_IrC5bDeclaredOrder(facts: R.Facts) -> None:
    """INTEGRATION.RegistryParity.IrC5bDeclaredOrder —— IR-C5b 声明序一致。

    判据原文：`docs/engineering/contracts/PIPELINE_BLOCK.md:114`
    「IR 的 phase1 节点序必须等于注册表 `modules` 数组里同阶段模块的出现序
    （块流规格 `stage_block_flow.json` 的 declared order 与 R4 同源）」。

    规则来源：`eng/tools/quality/gen_block_flow_spec.py:50` 排序键的第二项
    `reg_index(reg, n["module_id"])`（`:105-109` 定义为 `modules` 数组下标，未命中返回 999），
    外加 `:24` 的 `PHASE_TO_STAGE = {"phase1": "normalize", "phase2": "mosaic",
    "phase3": "export"}`。

    基线实算（三阶段**全等**）：
      normalize = [calibration, cosmetic, wcs-platesolve, star-psf, photometry,
                   noise-snr, drizzle, writer]
      mosaic    = [coverage, sample, upm-fit, upm-apply, reject, integrate, write]
      export    = [properties, wcs, resample2, writer, verify]

    本条**非恒真**：`test_RegistryParity_Neg_IrC5bDeclaredOrder` 交换 IR 里两个同阶段
    节点即判红。
    """
    _fail_if(_judge(R.judge_ir_c5b_declared_order, facts, "IR-C5b"),
             "IR-C5B_DECLARED_ORDER_VIOLATION")


def test_RegistryParity_IrC6PsfAfterWcs(facts: R.Facts) -> None:
    """INTEGRATION.RegistryParity.IrC6PsfAfterWcs —— IR-C6 psf 在 wcs 之后。

    判据原文：`docs/engineering/contracts/PIPELINE_BLOCK.md:115`
    「`pos(psf) > pos(wcs)`，且 `psf` 节点必须声明 `artifact:p1_wcs` 输入边
    （取向先验的真实来源）」。

    「取向先验的真实来源」的代码侧出处：`module_adapters.cpp:907-913`
    （`p1_star_psf_descriptor` 端口表里的注释块）：
    「星表引导检测要把 Gaia 星表逆投影到像素域, 逆投影必须知道像面取向与镜像;
    由配置给出的"中心指向 + 板尺度"**不含取向**……声明为 typed 输入端口 ⇒ 调度器保证
    wcs 节点先落盘, 本节点直接以本帧自解 WCS 作先验, 不再要求调用方预先知道像面取向。」

    基线实算：`pos(acsd.phase1.star-psf) = 15 > pos(acsd.phase1.wcs-platesolve) = 14`，
    且 `star-psf.reads = ["p1_cleaned", "p1_wcs"]` 含 `p1_wcs`。0 违例。

    fail-closed：IR 里解析不到 `psf` / `wcs` 节点 ⇒ `RESOLUTION_FAILED` 具名判红，
    不按「无节点 ⇒ 无违例」处理（`PIPELINE_BLOCK.md:116` 明列同款要求）。

    负例：`test_RegistryParity_Neg_IrSwapPsfWcsOrder`、`test_RegistryParity_Neg_IrPsfWithoutP1Wcs`。
    """
    _fail_if(_judge(R.judge_ir_c6_psf_after_wcs, facts, "IR-C6"), "IR-C6_PSF_AFTER_WCS_VIOLATION")


def test_RegistryParity_IrC7NonDegenerate(facts: R.Facts) -> None:
    """INTEGRATION.RegistryParity.IrC7NonDegenerate —— IR-C7 非退化（fail-closed）。

    判据原文：`docs/engineering/contracts/PIPELINE_BLOCK.md:116`
    「phase1 节点数 / IR 边数 / 注册表端口边数均有下界；解析不到即 fail-closed
    （判据 = 解析成功；解析不到一律判红）」。

    下界与来源（零魔数，`VALIDATION_EVIDENCE.md:192` S5）：

    | 下界 | 取值 | 来源 |
    |---|---|---|
    | normalize 阶段 IR 节点数 | ≥ 3 | `gen_block_flow_spec.py:30` `STAGE_TERMINALS["normalize"] = {frame_hips, p1_final, p1_products}`；`:59` `produced.setdefault((st,b),[]).append(n["module_id"])` 逐节点记录 ⇒ 每个终产物各有其生产节点 |
    | IR 声明的 normalize 阶段内边数 | ≥ 1 | `VALIDATION_EVIDENCE.md:170-173` 零对象守卫第 1 条 |
    | 注册表 normalize 阶段内端口边数 | ≥ 1 | 同上 |
    | 解析不到 | 判红 | `PIPELINE_BLOCK.md:116` 明列 |

    基线实算：normalize 节点 **8**、IR 阶段内边 **>1**、注册表阶段内边 **>1**，0 违例。
    """
    _fail_if(_judge(R.judge_ir_c7_non_degenerate, facts, "IR-C7"),
             "IR-C7_NON_DEGENERATE_VIOLATION")


def test_RegistryParity_IrC8NoExecutionSurface(facts: R.Facts) -> None:
    """INTEGRATION.RegistryParity.IrC8NoExecutionSurface —— IR-C8 **缺在位执行面**（登记，非判红）。

    判据原文：`docs/engineering/contracts/PIPELINE_BLOCK.md:117`
    「IR 每个节点的输入/输出端口名必须出现在 `module_adapters.cpp` 对应 descriptor 的端口表里
    （运行期 `MISSING_PORT` 静态验证的 CI 侧等价判据；不启动产品二进制即可发现 IR ↔ descriptor
    漂移）」。

    **为什么本条不断言「IR 端口名 ∈ descriptor 端口表」**：那会是一条永远红的测试
    （违反 `TEST.md:26`「恒真的比较没有证据资格」与派单纪律 C5）。实测（HEAD `bf944fad`）：

    | 指标 | 实测 | 含义 |
    |---|---|---|
    | 注册表 module 数 | 20 | — |
    | 解析出的 `ModuleDescriptor` 定义 | 22 | 其中 20 个 `module_id` 命中注册表；另 2 个是聚合 descriptor（`module_adapters.cpp:724-731`、`:753-759` 自带「已知不一致（未裁）」注释） |
    | 有 descriptor 的注册表 module | 20 / 20 | **端口表在位** |
    | IR 端口引用条数 `n_ir_refs` | 85 | — |
    | 逐字命中 `n_name_hits` | **0** | 两套词表互不相交 |
    | `data_schema_id` ↔ descriptor `data_id` 桥 | **55 命中 / 30 未命中** | 桥接口径也不闭合 |

    两套端口词表（实测摘录）：
    - IR / 注册表（产物身份词表）：`p1_cleaned` `p1_wcs` `p1_sources` `p1_psf`
      `p1_calibrated` `p3_props` `p3_wcs` `mosaic_hips` `frame_hips` `run_context` …
    - descriptor（typed 绑定短名）：`cleaned` `wcs` `sources` `psf` `calibrated`
      `props` `wcs_plan` `resampled` `fits` `verified` `hips` `mosaic` `stacked` …
      出处：`module_adapters.cpp:710-713`（phase1_descriptor）、
      `:905-916`（p1_star_psf_descriptor）、`:828-832`（p3_resample2_descriptor）。

    descriptor 端口表**本身也不完整**（与注册表逐模块对比）：
    `p1_writer_descriptor`（`:1052`）只有 `stacked`/`fits` 而注册表同模块声明 5 个端口；
    `p2_upm_apply_descriptor`（`:1133`）3 个 vs 6 个；`p2_write_descriptor`（`:1192`）2 个 vs 8 个。

    ⇒ **IR-C8 缺在位对象，不是「判红」而是「判据不可执行」**。本条把「不存在可判的公共词表」
    这个事实与它的三条实测读数固化成断言，并配负例
    `test_RegistryParity_Neg_IrC8VocabularyInjected`（往 descriptor 端口表注入一个 IR 端口名
    ⇒ 断言 IR-C8 **转为可判**），证明这条判据既非恒真也非恒假。
    **需负责人裁定：补桥表，还是撤判据。** 本单不替裁。
    """
    surface = _judge(R.ir_c8_execution_surface, facts, "IR-C8")
    # 前置 1：descriptor 端口表必须在位（否则连「缺在位执行面」都谈不上）
    assert surface["missing_descriptor"] == [], (
        "有注册表 module 没有对应 descriptor: %s —— IR-C8 的缺口不是词表不相交，"
        "而是 descriptor 缺失" % surface["missing_descriptor"])
    # 前置 2：IR 端口引用面非零（零引用 ⇒ 计数恒真）
    assert surface["n_ir_refs"] > 0, "IR 端口引用数为 0，IR-C8 的计数是恒真的"
    # 登记断言：逐字口径与桥接口径都不闭合 ⇒ 判据无在位执行面
    assert surface["n_name_hits"] == 0, (
        "descriptor↔IR 端口名已出现公共词表（%d/%d 命中）⇒ IR-C8 转为可判，"
        "本登记条目需要重写并改写为真正的判据"
        % (surface["n_name_hits"], surface["n_ir_refs"]))
    assert surface["n_dataid_misses"] > 0, (
        "data_schema_id ↔ descriptor data_id 桥已完全闭合（%d/%d 命中），"
        "IR-C8 有第二条可执行口径" % (surface["n_dataid_hits"], surface["n_ir_refs"]))
    assert not surface["has_execution_surface"], (
        "IR-C8 存在在位执行面，与本登记条目矛盾")


def test_RegistryParity_IrC4BridgePrecondition() -> None:
    """INTEGRATION.RegistryParity.IrC4BridgePrecondition —— IR-C4 的桥表前提缺失（**基线判红**）。

    判据原文：`docs/engineering/contracts/PIPELINE_BLOCK.md:112` 末句
    「IR 产物身份须在 `PHASE1_ARTIFACT_TO_PORT` 桥表中登记（未登记即判红，
    新增产物必须同步该桥）」。末句的成立条件是那张桥表存在。

    实测：在 `lib/` + `eng/` 下扫 `.cpp/.h/.hpp/.py/.json`，**排除 `eng/tests`**
    （本单元自指面）与 `run/`（历史交付件面，不是产品面），**0 命中**。
    全仓文本命中只有 4 处，全是「说这张表存在」而不是「这张表」：
    `docs/engineering/contracts/PIPELINE_BLOCK.md:112`（条款自身）、
    `run/GOVERN-08/审核包-R2/审稿-R3-T3-代码与文档交叉面.md:284`（「须修-21 ……
    **判红依据本身不可执行**」）、
    `run/GOVERN-08/审核包-R2/审稿-RR07-P5 加性天光去除.md:101` 与
    `run/GOVERN-08/审核包-R2/审稿-P1-DOC-ENG-001.md:139`（两处转述）。

    ⇒ **本条按 `VALIDATION_EVIDENCE.md:412` 的 fail-closed 判红**：
    「输入缺失…时判红；『读不到就按全部合规处理』是未具名形态」。
    本条**按设计长期判红**，直到负责人裁定是补桥表还是撤掉 IR-C4 的这半句。

    排除面自报（`VALIDATION_EVIDENCE.md:236`「已知假阳形态、被排除的采集面、现值」）：
    假阳形态 = 本单元自己把该标识写成常量与检索词；处置 = 排除 `eng/tests` 子树。
    被排除的采集面 = `eng/tests/`（测试面，不是产品面）、`run/`（历史交付件）；
    命中数逐条复算 = 0。

    负例 / S2 对照：`test_RegistryParity_Neg_BridgeTableInjected`（注入真桥表后转绿）、
    `test_RegistryParity_Neg_BridgeDecoyRejected`（同名不同形的桥表不被接受）。
    """
    hit = R.find_bridge_symbol()
    if hit is None:
        pytest.fail(
            "PRECONDITION_MISSING: %s 桥表在 lib/ + eng/（排除 eng/tests、run）的 "
            "源与合同文本面 0 命中 ⇒ PIPELINE_BLOCK.md:112 末句「未登记即判红」"
            "的判红依据不存在（PIPELINE_BLOCK.md:112 / "
            "VALIDATION_EVIDENCE.md:412 fail-closed）" % R.IR_C4_BRIDGE_SYMBOL)
    # 有桥表时，判据退化为「桥表必须在 lib/ 或 eng/ 的源与合同文本面」，而不是恒红
    assert hit.startswith(("lib/", "eng/")), "桥表命中位置异常: %s" % hit


# ==========================================================================
# B. 负例（注入后必须真变红；一律在 tempfile 造的临时副本上跑）
# ==========================================================================


def test_RegistryParity_Neg_PhantomEdge() -> None:
    """INTEGRATION.RegistryParity.Neg.PhantomEdge —— 负例：声明不存在的边。

    注入点：`PIPELINE_BLOCK.md:99` 负例清单第 1 条「声明不存在的边」。
    注入形态：在临时副本的 IR 里给 `acsd.phase1.noise-snr` 的 `reads` 加一个注册表里
    无人生产的产物身份 `p1_nonexistent_probe`。注入落点正是 IR-C4 判定器**自身**的
    「IR 声明的每条边必须由注册表端口图支持」这段逻辑（`VALIDATION_EVIDENCE.md:196` S6）。

    对照读数：注入前 `judge_ir_c4_no_phantom_edge` 违例数 = 0；注入后 ≥ 1，
    且新增违例点名 `acsd.phase1.noise-snr<-p1_nonexistent_probe`。
    """
    with _sandbox() as sb:
        before = _judge(R.judge_ir_c4_no_phantom_edge, sb.facts(), "IR-C4")
        ir = sb.load("ir")
        R.ir_node_by_id(ir, "acsd.phase1.noise-snr")["reads"].append("p1_nonexistent_probe")
        sb.save("ir", ir)
        after = _judge(R.judge_ir_c4_no_phantom_edge, sb.facts(), "IR-C4")
        assert before == [], "注入前 IR-C4 已有违例，S2 对照失效: %s" % R.render(before)
        assert len(after) > len(before), "注入幻边后 IR-C4 未变红（负例无效）"
        assert "acsd.phase1.noise-snr<-p1_nonexistent_probe" in R.subjects(after), (
            "IR-C4 变红了但没点名被注入的幻边: %s" % R.render(after))


def test_RegistryParity_Neg_MissingRealEdge() -> None:
    """INTEGRATION.RegistryParity.Neg.MissingRealEdge —— 负例：漏声明真实流（IR 侧）。

    注入点：`PIPELINE_BLOCK.md:99` 负例清单第 2 条「漏声明真实流」，
    落在 IR 侧的实现形态是 `PIPELINE_BLOCK.md:22`「IR 的边集与注册表边集逐条相等」。
    注入形态：从临时副本 IR 里删掉 `acsd.phase1.photometry` 的 `reads` 中的 `p1_wcs`
    —— 这是注册表端口图里真实存在的边（`acsd.phase1.wcs-platesolve` output `p1_wcs`
    → `acsd.phase1.photometry` input `p1_wcs`），删掉即漏声明。

    对照读数：注入前 0 违例；注入后新增 `IR-C4/MISSING_EDGE` 且点名该边。
    """
    with _sandbox() as sb:
        before = _judge(R.judge_ir_c4_no_phantom_edge, sb.facts(), "IR-C4")
        ir = sb.load("ir")
        node = R.ir_node_by_id(ir, "acsd.phase1.photometry")
        assert "p1_wcs" in node["reads"], "注入前提不成立：photometry 未读 p1_wcs"
        node["reads"].remove("p1_wcs")
        sb.save("ir", ir)
        after = _judge(R.judge_ir_c4_no_phantom_edge, sb.facts(), "IR-C4")
        assert before == [], "注入前 IR-C4 已有违例，S2 对照失效: %s" % R.render(before)
        kinds = {v.criterion for v in after}
        assert "IR-C4/MISSING_EDGE" in kinds, (
            "漏声明真实边后 IR-C4 未报 MISSING_EDGE（负例无效）: %s" % R.render(after))
        assert any("p1_wcs" in v.subject for v in after), (
            "MISSING_EDGE 未点名被删掉的 p1_wcs: %s" % R.render(after))


def test_RegistryParity_Neg_DirectionReversed() -> None:
    """INTEGRATION.RegistryParity.Neg.DirectionReversed —— 负例：方向写反。

    注入点：`PIPELINE_BLOCK.md:99` 负例清单第 3 条「方向写反」。
    注入形态：把临时副本注册表里 `acsd.phase1.calibration:lights` 的 `direction` 从
    `input` 改成 `output`。该端口的 `carrier=config_path`，而
    `PIPELINE_BLOCK.md:21` 逐字把 `config_path` 定义为「阶段外部输入，本阶段无生产者」，
    所以方向只能是 `input`——这正是 PC-C4 判定器**自身**的
    `PC-C4a/CONFIG_PATH_ROLE` 子句（`VALIDATION_EVIDENCE.md:196` S6：只换这条判定自身的逻辑）。

    对照读数：注入前 0 违例；注入后报 `PC-C4a/CONFIG_PATH_ROLE` 且点名
    `acsd.phase1.calibration:lights`。
    """
    with _sandbox() as sb:
        before = _judge(R.judge_pc_c4_direction, sb.facts(), "PC-C4")
        reg = sb.load("registry")
        port = R.first_port(R.Facts(reg, {"nodes": []}, {}), "acsd.phase1.calibration", "input")
        assert port["name"] == "lights" and port["carrier"] == "config_path", \
            "注入前提不成立：calibration 的首个 input 端口不是 lights/config_path"
        port["direction"] = "output"
        sb.save("registry", reg)
        after = _judge(R.judge_pc_c4_direction, sb.facts(), "PC-C4")
        assert before == [], "注入前 PC-C4 已有违例，S2 对照失效: %s" % R.render(before)
        assert any(v.criterion == "PC-C4a/CONFIG_PATH_ROLE"
                   and v.subject == "acsd.phase1.calibration:lights" for v in after), (
            "方向写反后 PC-C4 未点名该端口（负例无效）: %s" % R.render(after))


def test_RegistryParity_Neg_AnchorTokenDeleted() -> None:
    """INTEGRATION.RegistryParity.Neg.AnchorTokenDeleted —— 负例：锚点 token 删除。

    注入点：`PIPELINE_BLOCK.md:99` 负例清单第 4 条「锚点 token 删除」。
    注入形态：在临时副本的 `module_adapters.cpp` 里，把 `acsd.phase1.wcs-platesolve`
    的 `p1_wcs` 输出端口锚点 token `/p1_wcs.json` 从 `p1_op_wcs` 函数体内删掉
    （改成一个不含该 token 的字面量 `/p1_wcs.tmp`）。
    注入落点正是 C2 判定器**自身**的「`token` 落在该函数体内」这段逻辑。

    对照读数：注入前 0 违例；注入后报 `C2/ANCHOR_STALE` 且点名
    `acsd.phase1.wcs-platesolve:p1_wcs:/p1_wcs.json`。
    """
    with _sandbox() as sb:
        before = _judge(R.judge_c2_anchor_resolves, sb.facts(), "C2")
        src = sb.text("adapters")
        unit = R.CppUnit("adapters", src)
        span = unit.resolve_unique("p1_op_wcs")
        body = src[span.body_start:span.body_end + 1]
        assert body.count('"/p1_wcs.json"') >= 1, "注入前提不成立：p1_op_wcs 函数体内无该字面量"
        # 该字面量在函数体内出现多次；**全部**替换，否则 token 仍留在体内、判据不会红
        mutated = body.replace('"/p1_wcs.json"', '"/p1_wcs.tmp"')
        sb.set_text("adapters", src[:span.body_start] + mutated + src[span.body_end + 1:])
        after = _judge(R.judge_c2_anchor_resolves, sb.facts(), "C2")
        assert before == [], "注入前 C2 已有违例，S2 对照失效: %s" % R.render(before)
        assert any(v.criterion == "C2/ANCHOR_STALE"
                   and v.subject.endswith(":/p1_wcs.json") for v in after), (
            "删掉锚点 token 后 C2 未报 ANCHOR_STALE（负例无效）: %s" % R.render(after))


def test_RegistryParity_Neg_AnchorTokenMovedOutOfSymbol() -> None:
    """INTEGRATION.RegistryParity.Neg.AnchorTokenMovedOutOfSymbol —— 负例：锚点 token 移出符号。

    注入点：`PIPELINE_BLOCK.md:99` 负例清单第 5 条「锚点 token 移出符号」。
    注入形态：把 `acsd.phase1.noise-snr` 的 `p1_snr` 输出端口锚点 token `/p1_snr.json`
    的**唯一**一次出现，从 `p1_op_noise` 函数体内挪到紧随其后的 `p1_op_drizzle` 函数体里。
    token 仍在同一文件里、仍能 grep 到，但**落在别的符号的函数体外** ⇒ C2 必须判红。

    这一条专门防「只 grep 全文、不定位函数体」的实现。注入落点是 C2 判定器**自身**的
    函数体区间定位逻辑。

    对照读数：注入前 0 违例；注入后报 `C2/ANCHOR_STALE` 且点名
    `acsd.phase1.noise-snr:p1_snr:/p1_snr.json`。
    """
    with _sandbox() as sb:
        before = _judge(R.judge_c2_anchor_resolves, sb.facts(), "C2")
        src = sb.text("adapters")
        unit = R.CppUnit("adapters", src)
        span = unit.resolve_unique("p1_op_noise")
        off = src.index('"/p1_snr.json"', span.body_start, span.body_end + 1)
        moved = src[:off] + '"/p1_other.json"' + src[off + len('"/p1_snr.json"'):]
        # 把它塞进 p1_op_drizzle 函数体的第一行（另一个符号）
        dz = R.CppUnit("adapters", moved).resolve_unique("p1_op_drizzle")
        insert_at = moved.index("{", dz.body_start) + 1
        moved = moved[:insert_at] + '  const char* probe = "/p1_snr.json";' + moved[insert_at:]
        sb.set_text("adapters", moved)
        after = _judge(R.judge_c2_anchor_resolves, sb.facts(), "C2")
        assert before == [], "注入前 C2 已有违例，S2 对照失效: %s" % R.render(before)
        assert any(v.criterion == "C2/ANCHOR_STALE"
                   and v.subject == "acsd.phase1.noise-snr:p1_snr:/p1_snr.json"
                   for v in after), (
            "token 移出符号后 C2 未点名该锚点（负例无效）: %s" % R.render(after))


def test_RegistryParity_Neg_SymbolMissing() -> None:
    """INTEGRATION.RegistryParity.Neg.SymbolMissing —— 负例：符号不存在。

    注入点：`PIPELINE_BLOCK.md:99` 负例清单第 6 条「符号不存在」。
    注入形态：把临时副本注册表里某条锚点的 `symbol` 改成 `p1_op_does_not_exist`。
    注入落点是 C2 判定器**自身**的「`symbol` 在本文件内唯一解析到函数体」这段逻辑。

    对照读数：注入前 0 违例；注入后报 `C2/SYMBOL_UNRESOLVED`（解析出 0 个函数体）。
    """
    with _sandbox() as sb:
        before = _judge(R.judge_c2_anchor_resolves, sb.facts(), "C2")
        reg = sb.load("registry")
        port = R.module_by_id(R.Facts(reg, {"nodes": []}, {}),
                              "acsd.phase1.photometry")["operations"][0]["ports"][0]
        port["code"][0]["symbol"] = "p1_op_does_not_exist"
        sb.save("registry", reg)
        after = _judge(R.judge_c2_anchor_resolves, sb.facts(), "C2")
        assert before == [], "注入前 C2 已有违例，S2 对照失效: %s" % R.render(before)
        assert any(v.criterion == "C2/SYMBOL_UNRESOLVED" for v in after), (
            "符号不存在后 C2 未报 SYMBOL_UNRESOLVED（负例无效）: %s" % R.render(after))


def test_RegistryParity_Neg_AnchorFileMissing() -> None:
    """INTEGRATION.RegistryParity.Neg.AnchorFileMissing —— 负例：文件不存在。

    注入点：`PIPELINE_BLOCK.md:99` 负例清单第 7 条「文件不存在」。
    注入形态：把临时副本注册表里某条锚点的 `file` 改成
    `lib/infrastructure/scheduler/src/module_adapters_absent.cpp`（沙箱内确实不存在该文件）。

    fail-closed 取向（`VALIDATION_EVIDENCE.md:412`「『文件不存在』按『有违规』处理
    （实扫对象数为 0 即判红）」）：C2 判定器报 `C2/FILE_MISSING`，**不**报通过、**不** skip。

    对照读数：注入前 0 违例；注入后报 `C2/FILE_MISSING`。
    """
    with _sandbox() as sb:
        before = _judge(R.judge_c2_anchor_resolves, sb.facts(), "C2")
        reg = sb.load("registry")
        port = R.module_by_id(R.Facts(reg, {"nodes": []}, {}),
                              "acsd.phase1.photometry")["operations"][0]["ports"][0]
        port["code"][0]["file"] = "lib/infrastructure/scheduler/src/module_adapters_absent.cpp"
        sb.save("registry", reg)
        after = _judge(R.judge_c2_anchor_resolves, sb.facts(), "C2")
        assert before == [], "注入前 C2 已有违例，S2 对照失效: %s" % R.render(before)
        assert any(v.criterion == "C2/FILE_MISSING" for v in after), (
            "锚文件不存在后 C2 未报 FILE_MISSING（负例无效）: %s" % R.render(after))


def test_RegistryParity_Neg_CrossStageEdge() -> None:
    """INTEGRATION.RegistryParity.Neg.CrossStageEdge —— 负例：跨阶段边。

    注入点：`PIPELINE_BLOCK.md:99` 负例清单第 8 条「跨阶段边」。
    注入形态：在临时副本注册表里给 `acsd.phase2.coverage` 加一个 `direction=input`、
    `carrier=output_dir_file` 的端口 `p1_wcs`。`p1_wcs` 由 phase1 的
    `acsd.phase1.wcs-platesolve` 生产，被 phase2 的 `acsd.phase2.photometry`…
    实测注册表里 `acsd.phase2.coverage` 的**输出** `p2_coverage` 无消费者，
    所以这里改成直接给 `p1_wcs` 加一个 phase2 的输入端口——它天然构成一条
    phase1 → phase2 的 `output_dir_file` 跨阶段边。

    注入落点是 PC-C5 判定器**自身**的「`output_dir` 产物身份限本阶段」这段逻辑。

    对照读数：注入前该子句 0 违例；注入后报 `PC-C5/CROSS_STAGE_OUTPUT_DIR`。
    """
    with _sandbox() as sb:
        before = [v for v in _judge(R.judge_pc_c5_carrier_contract, sb.facts(), "PC-C5")
                  if v.criterion == "PC-C5/CROSS_STAGE_OUTPUT_DIR"]
        reg = sb.load("registry")
        mod = R.module_by_id(R.Facts(reg, {"nodes": []}, {}), "acsd.phase2.coverage")
        mod["operations"][0]["ports"].append({
            "name": "p1_wcs", "direction": "input", "carrier": "output_dir_file",
            "artifacts": ["<frame_key>/p1_wcs.json"],
            "data_schema_id": "DATA-P1-WCS", "unit": "DIMENSIONLESS",
            "coordinate": "ICRS", "scalar": "f64", "shape_hint": "[1]",
            "code": [{"file": "lib/infrastructure/scheduler/src/module_adapters.cpp",
                      "symbol": "p2_op_coverage", "token": "/p1_wcs.json"}],
        })
        sb.save("registry", reg)
        after = [v for v in _judge(R.judge_pc_c5_carrier_contract, sb.facts(), "PC-C5")
                 if v.criterion == "PC-C5/CROSS_STAGE_OUTPUT_DIR"]
        assert before == [], "注入前 PC-C5 跨阶段子句已有违例，S2 对照失效: %s" % R.render(before)
        assert after, "注入跨阶段 output_dir 边后 PC-C5 未变红（负例无效）"
        assert any(v.subject == "p1_wcs" for v in after), \
            "PC-C5 跨阶段违例未点名 p1_wcs: %s" % R.render(after)


def test_RegistryParity_Neg_CarrierContractMissing() -> None:
    """INTEGRATION.RegistryParity.Neg.CarrierContractMissing —— 负例：载体合同缺失。

    注入点：`PIPELINE_BLOCK.md:99` 负例清单第 9 条「载体合同缺失」。
    注入形态：从临时副本注册表里删掉顶层 `carrier_contract` 键。
    注入落点是 PC-C5 判定器**自身**的第三条子句
    「`carrier_contract` 必须显式声明 HiPS 产品树为跨阶段载体」。

    对照读数：注入前该子句 0 违例；注入后报 `PC-C5/NO_HIPS_CARRIER_DECL`。
    """
    with _sandbox() as sb:
        before = [v for v in _judge(R.judge_pc_c5_carrier_contract, sb.facts(), "PC-C5")
                  if v.criterion == "PC-C5/NO_HIPS_CARRIER_DECL"]
        reg = sb.load("registry")
        assert "carrier_contract" in reg, "注入前提不成立：注册表本就没有 carrier_contract"
        del reg["carrier_contract"]
        sb.save("registry", reg)
        after = _judge(R.judge_pc_c5_carrier_contract, sb.facts(), "PC-C5")
        assert before == [], "注入前 PC-C5 载体合同子句已有违例，S2 对照失效: %s" % R.render(before)
        assert any(v.criterion == "PC-C5/NO_HIPS_CARRIER_DECL" for v in after), (
            "删掉 carrier_contract 后 PC-C5 未报 NO_HIPS_CARRIER_DECL（负例无效）: %s"
            % R.render(after))


def test_RegistryParity_Neg_PortWithoutAnchor() -> None:
    """INTEGRATION.RegistryParity.Neg.PortWithoutAnchor —— 负例：端口无锚点。

    注入点：`PIPELINE_BLOCK.md:99` 负例清单第 10 条「端口无锚点」。
    注入形态：把临时副本注册表里某个端口的 `code` 置空数组 `[]`。
    注入落点是 C1 判定器**自身**的「端口 … `code` 齐全」这条字段齐全性检查
    （`PIPELINE_BLOCK.md:91`）。

    对照读数：注入前 0 违例；注入后 C1 报该端口缺字段 `code`。
    """
    with _sandbox() as sb:
        before = _judge(R.judge_c1_structure, sb.facts(), "C1")
        reg = sb.load("registry")
        port = R.module_by_id(R.Facts(reg, {"nodes": []}, {}),
                              "acsd.phase1.writer")["operations"][0]["ports"][0]
        del port["code"]
        sb.save("registry", reg)
        after = _judge(R.judge_c1_structure, sb.facts(), "C1")
        assert before == [], "注入前 C1 已有违例，S2 对照失效: %s" % R.render(before)
        assert any(v.criterion == "C1" and "code" in v.detail for v in after), (
            "端口无锚点后 C1 未报缺字段 code（负例无效）: %s" % R.render(after))


def test_RegistryParity_Neg_NodeFunctionRenamed() -> None:
    """INTEGRATION.RegistryParity.Neg.NodeFunctionRenamed —— 负例：节点函数改名。

    注入点：`PIPELINE_BLOCK.md:99` 负例清单第 11 条「节点函数改名」。
    注入形态：在临时副本 `module_adapters.cpp` 里把 `p2_op_sample` 的**定义**改名成
    `p2_op_sample_renamed`（注册表不动，于是锚点指向的符号不再存在）。

    这一条同时验证解析器对「前向声明 + 定义」两处的处理：前向声明仍叫
    `p2_op_sample`，定义改名后，解析器必须报「解析出 0 个函数体」而不是把
    前向声明误当定义。

    对照读数：注入前 0 违例；注入后报 `C2/SYMBOL_UNRESOLVED`。
    """
    with _sandbox() as sb:
        before = _judge(R.judge_c2_anchor_resolves, sb.facts(), "C2")
        src = sb.text("adapters")
        unit = R.CppUnit("adapters", src)
        span = unit.resolve_unique("p2_op_sample")
        # 只改**定义处**的符号名（函数名前向声明保持原名），从而让「前向声明 + 定义」
        # 这一形态被真正考察到。
        rel = src.rindex("p2_op_sample", 0, span.body_start)
        renamed = src[:rel] + "p2_op_sample_renamed" + src[rel + len("p2_op_sample"):]
        assert R.CppUnit("adapters", renamed).find_definitions("p2_op_sample") == [], \
            "注入前提不成立：改名后 p2_op_sample 仍能解析出定义"
        sb.set_text("adapters", renamed)
        after = _judge(R.judge_c2_anchor_resolves, sb.facts(), "C2")
        assert before == [], "注入前 C2 已有违例，S2 对照失效: %s" % R.render(before)
        assert any(v.criterion == "C2/SYMBOL_UNRESOLVED"
                   and "p2_op_sample" in v.detail for v in after), (
            "节点函数改名后 C2 未报 SYMBOL_UNRESOLVED（负例无效）: %s" % R.render(after))
        # 该 module 的两条端口锚点都必须被判红，不能只红一条
        assert len([v for v in after if v.criterion == "C2/SYMBOL_UNRESOLVED"]) == 2, (
            "节点函数改名后 C2 只报了个别锚点，未覆盖该 module 全部锚点（负例无效）: %s"
            % R.render(after))


def test_RegistryParity_Neg_MaskingSurvivesBracesInStringsAndComments() -> None:
    """INTEGRATION.RegistryParity.Neg.MaskingSurvivesBracesInStringsAndComments
    —— 负例：往函数体里塞「字符串字面量里的花括号」与「注释里的花括号」，
    证明掩码 + 括号配平的函数体定位器没有被带偏（派单明写「要正确处理字符串字面量里的
    花括号、注释里的花括号」）。

    注入形态：在临时副本 `module_adapters.cpp` 的 `p2_op_sample` 函数体开头插入

        const char* kBraceInString = "}}} {{";
        /* }}} {{ 不成对的块注释花括号 */
        // }} }} 行注释里的花括号
        const char* kCharLit = '}';

    若定位器不抹掉这些花括号，`_match_pair` 会把函数体边界数错，`p2_op_sample` 就不再唯一解析
    （或解析到错误的结束位置），C2 与 C3 都会跟着误报。

    对照读数：注入前 C2 = 0 违例、`p2_op_sample` 唯一解析、函数体行区间已知；
    注入后三者**全部不变** ⇒ 定位器对上述四类干扰免疫。
    这条同时是 S2「同时验证绿」的另一半：证明 C2 的绿不是因为「恰好没有干扰」。
    """
    with _sandbox() as sb:
        b2 = _judge(R.judge_c2_anchor_resolves, sb.facts(), "C2")
        src = sb.text("adapters")
        unit = R.CppUnit("adapters", src)
        span = unit.resolve_unique("p2_op_sample")
        before_range = (span.name_line, unit.line_of(span.body_end))
        insert_at = src.index("{", span.body_start) + 1
        poison = ('  const char* kBraceInString = "}}} {{";\n'
                  '  /* }}} {{ 不成对的块注释花括号 */\n'
                  '  // }} }} 行注释里的花括号\n'
                  "  const char kCharLit = '}';\n")
        sb.set_text("adapters", src[:insert_at] + poison + src[insert_at:])
        a2 = _judge(R.judge_c2_anchor_resolves, sb.facts(), "C2")
        unit2 = R.CppUnit("adapters", sb.text("adapters"))
        span2 = unit2.resolve_unique("p2_op_sample")
        after_range = (span2.name_line, unit2.line_of(span2.body_end))
        assert b2 == [], "注入前 C2 已有违例，S2 对照失效: %s" % R.render(b2)
        assert unit2.find_definitions("p2_op_sample").__len__() == 1, \
            "注入干扰花括号后 p2_op_sample 不再唯一解析 —— 定位器被带偏了"
        assert after_range[0] == before_range[0] and after_range[1] > before_range[1], \
            "注入干扰花括号后函数体区间异常：前 %s，后 %s" % (before_range, after_range)
        assert a2 == [], "注入干扰花括号后 C2 变红（负例无效，证明定位器确实被干扰了）: %s" % R.render(a2)


def test_RegistryParity_Neg_EmptyRegistry() -> None:
    """INTEGRATION.RegistryParity.Neg.EmptyRegistry —— 负例：空注册表。

    注入点：`PIPELINE_BLOCK.md:99` 负例清单第 12 条「空注册表」，判据原文
    `PIPELINE_BLOCK.md:78`「注册表为空…⇒ 判红」+ `:97`「模块数/端口数/代码 token 数/
    生产→消费边数均有下界；空注册表判红」。

    注入形态：把临时副本注册表的 `modules` 置空数组。

    对照读数：注入前 0 违例；注入后 PC-C6 报 `PC-C6/EMPTY_REGISTRY`。
    """
    with _sandbox() as sb:
        before = _judge(R.judge_pc_c6_non_degenerate, sb.facts(), "PC-C6")
        reg = sb.load("registry")
        reg["modules"] = []
        sb.save("registry", reg)
        after = _judge(R.judge_pc_c6_non_degenerate, sb.facts(), "PC-C6")
        assert before == [], "注入前 PC-C6 已有违例，S2 对照失效: %s" % R.render(before)
        assert any(v.criterion == "PC-C6/EMPTY_REGISTRY" for v in after), (
            "空注册表后 PC-C6 未报 EMPTY_REGISTRY（负例无效）: %s" % R.render(after))


def test_RegistryParity_Neg_C3PhantomArtifactLiteral() -> None:
    """INTEGRATION.RegistryParity.Neg.C3PhantomArtifactLiteral
    —— 负例：在**基线已红**的前提下证明 C3 判定器抓的是「被注入的那个 token」。

    为什么需要这条：C3 正例在 HEAD 上本来就红 3 条（缺陷登记 R1）。若不做这条隔离，
    无法区分「判定器抓到了新注入的 `/injected_probe_artifact.json`」与「判定器无论注什么都红」
    （`VALIDATION_EVIDENCE.md:189` S2「同时验证绿」的反面：同时验证「红的内容是本次注入的」）。

    注入形态：在临时副本 `module_adapters.cpp` 的 `p3_op_verify` 函数体里插入
    `const char* probe = "/injected_probe_artifact.json";`——一个注册表未声明的
    第 4 条产物字面量。

    断言：注入后的违例集合 **恰好** 是基线那 3 条 **加** 这一条
    （`acsd.phase3.verify@p3_op_verify`），基线那 3 条仍在。
    这同时证明判定器既不是恒真（会多报），也不是恒假（会漏报）。
    """
    with _sandbox() as sb:
        base = _judge(R.judge_c3_implementation_to_declaration, sb.facts(), "C3")
        base_pairs = sorted((v.subject, v.detail) for v in base)
        assert len(base_pairs) == 3, (
            "基线 C3 违例数不再是 3 条（现 %d）——本负例的「恰好新增一条」前提需重算: %s"
            % (len(base_pairs), R.render(base)))
        src = sb.text("adapters")
        unit = R.CppUnit("adapters", src)
        span = unit.resolve_unique("p3_op_verify")
        insert_at = src.index("{", span.body_start) + 1
        mutated = (src[:insert_at]
                   + '  const char* probe = "/injected_probe_artifact.json";'
                   + src[insert_at:])
        sb.set_text("adapters", mutated)
        after = _judge(R.judge_c3_implementation_to_declaration, sb.facts(), "C3")
        after_pairs = sorted((v.subject, v.detail) for v in after)
        added = [p for p in after_pairs if p not in base_pairs]
        assert len(after_pairs) == len(base_pairs) + 1, (
            "注入后 C3 违例数不是基线+1（%d → %d），判定器要么多报要么漏报（负例无效）: %s"
            % (len(base_pairs), len(after_pairs), R.render(after)))
        assert len(added) == 1 and "acsd.phase3.verify@p3_op_verify" in added[0][0] \
            and "/injected_probe_artifact.json" in added[0][1], (
            "新增的那一条不是被注入的 token: %s" % added)
        # 基线三条仍在
        for pair in base_pairs:
            assert pair in after_pairs, "注入后基线违例消失了（判定器状态泄漏）: %s" % (pair,)


def test_RegistryParity_Neg_IrSwapPsfWcsOrder() -> None:
    """INTEGRATION.RegistryParity.Neg.IrSwapPsfWcsOrder —— 负例：交换 psf/wcs 节点序。

    注入点：`PIPELINE_BLOCK.md:119-120` IR `--self-test` 负例第 1 条「交换 `psf`/`wcs` 节点序」。
    注入形态：在临时副本 IR 的 `nodes` 数组里把 `acsd.phase1.wcs-platesolve` 与
    `acsd.phase1.star-psf` 的位置对调。

    注入落点同时打两条判定：
    - IR-C5（`:113`，同阶段边逆序）：`acsd.phase1.star-psf` 会排到
      `acsd.phase1.wcs-platesolve` 上游，而它读 `p1_wcs`；
    - IR-C6（`:115`）：`pos(psf) > pos(wcs)` 不再成立。

    对照读数：注入前两条判定各 0 违例；注入后两条都红。
    """
    with _sandbox() as sb:
        b5 = _judge(R.judge_ir_c5_topological_order, sb.facts(), "IR-C5")
        b6 = _judge(R.judge_ir_c6_psf_after_wcs, sb.facts(), "IR-C6")
        ir = sb.load("ir")
        nodes = ir["nodes"]
        i_wcs = next(i for i, n in enumerate(nodes)
                     if n["module_id"] == "acsd.phase1.wcs-platesolve")
        i_psf = next(i for i, n in enumerate(nodes)
                     if n["module_id"] == "acsd.phase1.star-psf")
        assert i_psf > i_wcs, "注入前提不成立：基线已是 psf 在前"
        nodes[i_wcs], nodes[i_psf] = nodes[i_psf], nodes[i_wcs]
        sb.save("ir", ir)
        a5 = _judge(R.judge_ir_c5_topological_order, sb.facts(), "IR-C5")
        a6 = _judge(R.judge_ir_c6_psf_after_wcs, sb.facts(), "IR-C6")
        assert b5 == [] and b6 == [], "注入前已有违例，S2 对照失效"
        assert any(v.criterion == "IR-C6/ORDER" for v in a6), \
            "交换 psf/wcs 后 IR-C6 未报 ORDER（负例无效）: %s" % R.render(a6)
        assert any(v.criterion == "IR-C5/NOT_TOPOLOGICAL" for v in a5), \
            "交换 psf/wcs 后 IR-C5 未报 NOT_TOPOLOGICAL（负例无效）: %s" % R.render(a5)


def test_RegistryParity_Neg_IrWcsReadsP1Sources() -> None:
    """INTEGRATION.RegistryParity.Neg.IrWcsReadsP1Sources —— 负例：注入 `wcs ← p1_sources` 幻边。

    注入点：`PIPELINE_BLOCK.md:119` IR `--self-test` 负例第 2 条
    「注入 `wcs ← p1_sources` 幻边」。`p1_sources` 由 `acsd.phase1.star-psf` 生产，
    而 `acsd.phase1.wcs-platesolve` 的注册表端口里**没有** `p1_sources`
    （它只声明 `p1_calibrated` 输入 / `p1_wcs` 输出）⇒ 注入后是一条幻边。

    注入落点是 IR-C4 判定器**自身**的「IR 声明的每条边必须由注册表端口图支持」这段逻辑。

    对照读数：注入前 0 违例；注入后报 `IR-C4/NOT_INPUT` 且点名
    `acsd.phase1.wcs-platesolve<-p1_sources`。
    """
    with _sandbox() as sb:
        before = _judge(R.judge_ir_c4_no_phantom_edge, sb.facts(), "IR-C4")
        ir = sb.load("ir")
        R.ir_node_by_id(ir, "acsd.phase1.wcs-platesolve")["reads"].append("p1_sources")
        sb.save("ir", ir)
        after = _judge(R.judge_ir_c4_no_phantom_edge, sb.facts(), "IR-C4")
        assert before == [], "注入前 IR-C4 已有违例，S2 对照失效: %s" % R.render(before)
        assert "acsd.phase1.wcs-platesolve<-p1_sources" in R.subjects(after), (
            "注入 wcs←p1_sources 幻边后 IR-C4 未点名该边（负例无效）: %s" % R.render(after))


def test_RegistryParity_Neg_IrPsfWithoutP1Wcs() -> None:
    """INTEGRATION.RegistryParity.Neg.IrPsfWithoutP1Wcs —— 负例：移除 psf 的 `p1_wcs` 输入。

    注入点：`PIPELINE_BLOCK.md:119` IR `--self-test` 负例第 3 条
    「移除 `psf` 的 `artifact:p1_wcs` 输入」。`PIPELINE_BLOCK.md:115` 要求
    「`psf` 节点必须声明 `artifact:p1_wcs` 输入边（取向先验的真实来源）」。

    注入落点是 IR-C6 判定器**自身**的「psf 读 p1_wcs」这段逻辑。

    对照读数：注入前 0 违例；注入后报 `IR-C6/MISSING_WCS_INPUT` 且同时报
    `IR-C4/MISSING_EDGE`（注册表里 `wcs-platesolve → psf` 那条边在 IR 里消失了）。
    """
    with _sandbox() as sb:
        b6 = _judge(R.judge_ir_c6_psf_after_wcs, sb.facts(), "IR-C6")
        b4 = _judge(R.judge_ir_c4_no_phantom_edge, sb.facts(), "IR-C4")
        ir = sb.load("ir")
        node = R.ir_node_by_id(ir, "acsd.phase1.star-psf")
        assert "p1_wcs" in node["reads"], "注入前提不成立：psf 本就没读 p1_wcs"
        node["reads"].remove("p1_wcs")
        sb.save("ir", ir)
        a6 = _judge(R.judge_ir_c6_psf_after_wcs, sb.facts(), "IR-C6")
        a4 = _judge(R.judge_ir_c4_no_phantom_edge, sb.facts(), "IR-C4")
        assert b6 == [] and b4 == [], "注入前已有违例，S2 对照失效"
        assert any(v.criterion == "IR-C6/MISSING_WCS_INPUT" for v in a6), \
            "移除 psf 的 p1_wcs 后 IR-C6 未报 MISSING_WCS_INPUT（负例无效）: %s" % R.render(a6)
        assert any(v.criterion == "IR-C4/MISSING_EDGE" for v in a4), \
            "移除 psf 的 p1_wcs 后 IR-C4 未报 MISSING_EDGE: %s" % R.render(a4)


def test_RegistryParity_Neg_IrPhotometryReadsP1Psf() -> None:
    """INTEGRATION.RegistryParity.Neg.IrPhotometryReadsP1Psf —— 负例：注入 `photometry ← p1_psf` 幻边。

    注入点：`PIPELINE_BLOCK.md:120` IR `--self-test` 负例第 4 条
    「注入 `photometry ← p1_psf` 幻边」。`p1_psf` 由 `acsd.phase1.star-psf` 生产，
    `acsd.phase1.photometry` 的注册表端口里**没有** `p1_psf`。

    注意这正是 `module_adapters.cpp:957-959` 的注释块已登记的真实形态：
    「PSF 参数随 `p1_sources.json` 的 `psf_params` 行到达（同一产物、同一帧序），
    故**不**声明 `artifact:p1_psf` 输入端口: 该边在注册表里不存在」——所以这条幻边
    在代码侧是被明确否掉的，IR 若声明它就是漂移。

    对照读数：注入前 0 违例；注入后报 `IR-C4/NOT_INPUT` 且点名
    `acsd.phase1.photometry<-p1_psf`。
    """
    with _sandbox() as sb:
        before = _judge(R.judge_ir_c4_no_phantom_edge, sb.facts(), "IR-C4")
        ir = sb.load("ir")
        R.ir_node_by_id(ir, "acsd.phase1.photometry")["reads"].append("p1_psf")
        sb.save("ir", ir)
        after = _judge(R.judge_ir_c4_no_phantom_edge, sb.facts(), "IR-C4")
        assert before == [], "注入前 IR-C4 已有违例，S2 对照失效: %s" % R.render(before)
        assert "acsd.phase1.photometry<-p1_psf" in R.subjects(after), (
            "注入 photometry←p1_psf 幻边后 IR-C4 未点名该边（负例无效）: %s" % R.render(after))


def test_RegistryParity_Neg_IrPortNameNotInDescriptor() -> None:
    """INTEGRATION.RegistryParity.Neg.IrPortNameNotInDescriptor
    —— 负例：把 IR 端口名改成 descriptor 里不存在的名字。

    注入点：`PIPELINE_BLOCK.md:120` IR `--self-test` 负例第 5 条
    「把 IR 端口名改成 descriptor 里不存在的名字」。

    **如实登记（见 IR-C8 登记条目 R4）**：在当前代码上，两侧端口词表互不相交，
    所以「改成不存在的名字」与「保持原样」对 IR-C8 判据而言**效果相同**——IR-C8 本来
    就没有在位执行面。因此本负例**不注入 IR-C8 判定器**（那会是一次自指的假验证），
    改为注入 **IR-C4** 判定器：把 `acsd.phase3.wcs` 的 `reads` 里的 `p3_props`
    改成一个注册表里不存在、也绝不可能出现在任何 descriptor 端口表里的名字
    `p3_props_typo`。IR-C4 的「消费侧必须是注册表 input 端口」+「IR 边集与注册表边集
    逐条相等」两条逻辑都会把它判红。

    这条负例的真实用途是把「IR 端口名漂移」这个缺陷形态**接上一个会红的判定**，
    并在报告里点名：IR-C8 想覆盖的漂移面目前只能由 IR-C4 部分兜住。

    对照读数：注入前 0 违例；注入后报 `IR-C4/MISSING_EDGE` 且点名该名。
    """
    with _sandbox() as sb:
        before = _judge(R.judge_ir_c4_no_phantom_edge, sb.facts(), "IR-C4")
        ir = sb.load("ir")
        node = R.ir_node_by_id(ir, "acsd.phase3.wcs")
        assert "p3_props" in node["reads"], "注入前提不成立：phase3.wcs 未读 p3_props"
        node["reads"][node["reads"].index("p3_props")] = "p3_props_typo"
        sb.save("ir", ir)
        after = _judge(R.judge_ir_c4_no_phantom_edge, sb.facts(), "IR-C4")
        assert before == [], "注入前 IR-C4 已有违例，S2 对照失效: %s" % R.render(before)
        assert any(v.criterion == "IR-C4/MISSING_EDGE" and "p3_props" in v.subject
                   for v in after), (
            "IR 端口名漂移后 IR-C4 未报 MISSING_EDGE（负例无效）: %s" % R.render(after))


def test_RegistryParity_Neg_IrC5bDeclaredOrder() -> None:
    """INTEGRATION.RegistryParity.Neg.IrC5bDeclaredOrder —— 负例：IR 声明序与注册表不一致。

    注入点：`PIPELINE_BLOCK.md:114` IR-C5b 自身的判定逻辑
    （规则文本来自 `gen_block_flow_spec.py:50` 的 `reg_index` 排序键）。
    注入形态：在临时副本 IR 里对调 `acsd.phase2.coverage` 与 `acsd.phase2.sample`
    两个同阶段节点的数组位置（注册表不动）。

    对照读数：注入前 0 违例；注入后报 `IR-C5b/DECLARED_ORDER` 且点名 `mosaic`。
    """
    with _sandbox() as sb:
        before = _judge(R.judge_ir_c5b_declared_order, sb.facts(), "IR-C5b")
        ir = sb.load("ir")
        nodes = ir["nodes"]
        i_a = next(i for i, n in enumerate(nodes)
                   if n["module_id"] == "acsd.phase2.coverage")
        i_b = next(i for i, n in enumerate(nodes)
                   if n["module_id"] == "acsd.phase2.sample")
        assert i_a < i_b, "注入前提不成立：基线 coverage 已在 sample 前"
        nodes[i_a], nodes[i_b] = nodes[i_b], nodes[i_a]
        sb.save("ir", ir)
        after = _judge(R.judge_ir_c5b_declared_order, sb.facts(), "IR-C5b")
        assert before == [], "注入前 IR-C5b 已有违例，S2 对照失效: %s" % R.render(before)
        assert any(v.criterion == "IR-C5b/DECLARED_ORDER" and v.subject == "mosaic"
                   for v in after), \
            "对调同阶段节点序后 IR-C5b 未报 DECLARED_ORDER（负例无效）: %s" % R.render(after)


def test_RegistryParity_Neg_C3bDirectionFlipped() -> None:
    """INTEGRATION.RegistryParity.Neg.C3bDirectionFlipped —— 负例：HiPS 端口方向写反。

    注入点：C3b 判定器**自身**的「代码侧生产者角色 ⇒ 必须有 `direction=output` 的
    hips 端口」这条逻辑（`PIPELINE_BLOCK.md:94`）。
    注入形态：在临时副本注册表里把 `acsd.phase2.write` 的 `mosaic_hips` 端口
    `direction` 从 `output` 改成 `input`。该 module 的 `p2_op_write` 函数体里
    调用了 `aio_hips_product_begin` / `aio_hips_write_*` / `aio_hips_finalize`
    ⇒ 代码侧是生产者角色，但注册表里不再有 output 的 hips 端口。

    对照读数：注入前 0 违例；注入后报 `C3B/DIRECTION` 且点名 `acsd.phase2.write`。
    """
    with _sandbox() as sb:
        before = _judge(R.judge_c3b_carrier_consistency, sb.facts(), "C3b")
        reg = sb.load("registry")
        mod = R.module_by_id(R.Facts(reg, {"nodes": []}, {}), "acsd.phase2.write")
        target = [p for p in mod["operations"][0]["ports"]
                  if p["name"] == "mosaic_hips" and p["direction"] == "output"]
        assert len(target) == 1, "注入前提不成立：mosaic_hips 不是唯一 output 端口"
        target[0]["direction"] = "input"
        sb.save("registry", reg)
        after = _judge(R.judge_c3b_carrier_consistency, sb.facts(), "C3b")
        assert before == [], "注入前 C3b 已有违例，S2 对照失效: %s" % R.render(before)
        assert any(v.criterion == "C3B/DIRECTION" and v.subject == "acsd.phase2.write"
                   for v in after), (
            "HiPS 端口方向写反后 C3b 未报 DIRECTION（负例无效）: %s" % R.render(after))


def test_RegistryParity_Neg_IrC8VocabularyInjected() -> None:
    """INTEGRATION.RegistryParity.Neg.IrC8VocabularyInjected
    —— 负例：descriptor↔IR 端口名对上之后，IR-C8 **转为可判**（S2「同时验证绿」的反面）。

    为什么需要这条：IR-C8 在当前代码上**无在位执行面**（登记条目 R4），所以
    `test_RegistryParity_IrC8NoExecutionSurface` 断言的是「公共词表不存在」。
    如果这条断言写错方向（比如其实存在一条被漏掉的公共词表），测试仍然绿。
    本负例补上另一半：往临时副本的 descriptor 端口表里注入一个 IR 端口名后，
    公共词表**必须**出现、`has_execution_surface` **必须**转 True ⇒ 证明登记条目
    断言的是真实缺口，不是恒真。

    注入形态：把 `p1_cosmetic_descriptor` 的 `d.ports` 里 `"cleaned"` 改成
    `"p1_cleaned"`（后者是 IR/注册表侧的产物身份名，`acsd.phase1.cosmetic` 的
    输出端口名）。
    """
    with _sandbox() as sb:
        before = _judge(R.ir_c8_execution_surface, sb.facts(), "IR-C8")
        assert before["n_name_hits"] == 0 and not before["has_execution_surface"], \
            "注入前提不成立：基线已存在公共词表"
        src = sb.text("adapters")
        # 在 p1_cosmetic_descriptor 里改端口名：{"cleaned", ...} → {"p1_cleaned", ...}
        fn = R.DESCRIPTOR_FN.search(src)
        assert fn is not None, "注入前提不成立：找不到 ModuleDescriptor 定义"
        fn = None
        for cand in R.DESCRIPTOR_FN.finditer(src):
            if cand.group(1) == "p1_cosmetic_descriptor":
                fn = cand
                break
        assert fn is not None, "注入前提不成立：找不到 p1_cosmetic_descriptor"
        open_brace = src.index("{", fn.start())
        close_brace = R._match_pair(R.mask_cpp(src), open_brace, "{", "}")
        body = src[open_brace:close_brace + 1]
        mutated_body = body.replace('{"cleaned"', '{"p1_cleaned"', 1)
        assert mutated_body != body, "注入前提不成立：没找到 {\"cleaned\" 端口项"
        sb.set_text("adapters", src[:open_brace] + mutated_body + src[close_brace + 1:])
        after = _judge(R.ir_c8_execution_surface, sb.facts(), "IR-C8")
        assert after["n_name_hits"] > 0, (
            "注入 descriptor 端口名后公共词表仍为 0 —— 登记条目的断言是恒真的（负例无效）")
        assert after["has_execution_surface"], (
            "注入后 IR-C8 仍未转为可判（has_execution_surface=False）")
        assert "acsd.phase1.cosmetic:p1_cleaned" not in after["misses"], (
            "注入后 p1_cleaned 仍被判为 descriptor 缺失（公共词表未真正建立）: %s"
            % after["misses"][:5])


def test_RegistryParity_Neg_BridgeTableInjected() -> None:
    """INTEGRATION.RegistryParity.Neg.BridgeTableInjected —— 负例：注入真桥表后 IR-C4 前提转绿。

    `test_RegistryParity_IrC4BridgePrecondition` 按设计长期判红（判据 = 桥表存在）。
    若那条断言写成了恒假（无论桥表在不在都判红），本条会暴露它：注入一张真桥表到
    **临时目录**（不改仓库，`VALIDATION_EVIDENCE.md:197`「反例复跑必须隔离」），
    `find_bridge_symbol(base=...)` 必须找到它，且 `test_RegistryParity_IrC4BridgePrecondition`
    判据的通过条件必须被满足。

    对照读数：基线（仓库实况）0 命中 ⇒ 判红；注入临时副本 ⇒ 命中 `lib/…` ⇒ 转绿。
    """
    with _sandbox() as sb:
        repo_hit = R.find_bridge_symbol()
        assert repo_hit is None, (
            "仓库实况已存在 %s 桥表（%s）——IR-C4 前提已满足，"
            "test_RegistryParity_IrC4BridgePrecondition 需要改成真正的判据"
            % (R.IR_C4_BRIDGE_SYMBOL, repo_hit))
        # 在沙箱临时目录里注入一张真桥表（base 指向它，仓库不动）
        probe = os.path.join(sb.tmp_root, "lib", "infrastructure", "pipeline",
                             "artifact_to_port.h")
        os.makedirs(os.path.dirname(probe), exist_ok=True)
        with open(probe, "w", encoding="utf-8") as fh:
            fh.write("// 负例夹具：仅用于验证桥表存在性判据，不是产品面\n"
                     "static const char* %s = \"p1_wcs\";\n" % R.IR_C4_BRIDGE_SYMBOL)
        hit = R.find_bridge_symbol(base=sb.tmp_root)
        assert hit is not None, (
            "注入桥表后 find_bridge_symbol 仍未命中 —— 桥表存在性判据是恒假的（负例无效）")
        assert hit.startswith("lib/"), "注入的桥表命中位置异常: %s" % hit


def test_RegistryParity_Neg_BridgeDecoyRejected() -> None:
    """INTEGRATION.RegistryParity.Neg.BridgeDecoyRejected —— 负例：同名不同形的桥表不被接受。

    `VALIDATION_EVIDENCE.md:190` S3「负例落在该判定自己的豁免或兜底分支」的同族要求：
    桥表存在性判据不能被一个**形似但不是桥表**的东西放行。
    注入形态：在临时目录里放一个**只在注释里**提到该标识的文件（真桥表必须是数据声明，
    不是散文提及），断言判据的**收紧面**——本判据只认「源与合同文本面里出现该标识」，
    因此注释提及**会**被接受，故本条改为锁定真实语义：把标识放进 `.md` 文件（不在搜索面内），
    断言判据**不**接受它。

    对照读数：只有 `.md` 提及 ⇒ 0 命中；换成 `.h` 数据声明 ⇒ 命中（由
    `test_RegistryParity_Neg_BridgeTableInjected` 覆盖）。
    """
    with _sandbox() as sb:
        md = os.path.join(sb.tmp_root, "lib", "bridge_doc.md")
        os.makedirs(os.path.dirname(md), exist_ok=True)
        with open(md, "w", encoding="utf-8") as fh:
            fh.write("# 说明\n本应有一张 %s 桥表，但它是散文不是桥表。\n"
                     % R.IR_C4_BRIDGE_SYMBOL)
        assert R.find_bridge_symbol(base=sb.tmp_root) is None, (
            "只有 .md 散文提及就被当成桥表 —— 判据把文档面混进了数据面（负例无效）")


# ==========================================================================
# C. 自检：判定器非恒真（S6）
# ==========================================================================


def test_RegistryParity_SelfCheck_JudgesNotVacuouslyTrue(facts: R.Facts) -> None:
    """INTEGRATION.RegistryParity.SelfCheck.JudgesNotVacuouslyTrue
    —— S6「内容级负例」的汇总自证：把判定结果换成恒真前，先证明**基线判定本身不是恒真**。

    做法（`VALIDATION_EVIDENCE.md:193` S6「把某个真判定的判定逻辑换成恒真，**只换它**，
    不换共用逻辑，核查必须点名**该判定**」的镜像检查）：逐条打印各判定器在基线上的
    实算对象数，并断言这些计数**大于零**——若某判定器在自己的输入面上读到 0 个对象，
    它的「0 违例」读数就没有证据资格（`TEST.md:26`）。

    本条不注入缺陷（逐判定的注入由各自的 Neg.* 负例承担），它守的是**零对象守卫**
    （`VALIDATION_EVIDENCE.md:170-173` 第 1 条）。
    """
    assert len(facts.anchors()) == 101
    assert len(facts.ports()) == 85
    assert len(facts.operations()) == 20
    assert len(facts.ir_nodes()) == 20
    assert len(facts.registry_edges()) == 51
    # C3 的抽取面必须非零，否则 C3 恒真
    total_g1 = 0
    for m in facts.modules:
        declared = facts.declared_tokens(m["module_id"])
        sym_files = {}
        for op in m.get("operations") or []:
            for p in op.get("ports") or []:
                for c in p.get("code") or []:
                    sym_files.setdefault(c["symbol"], c["file"])
        for symbol, rel in sym_files.items():
            unit = facts.units[rel]
            total_g1 += len(R.extract_g1_tokens(unit, unit.resolve_unique(symbol)))
    assert total_g1 > 0, "C3 文法在基线上一个 token 都抽不到 —— C3 是恒真的"
    assert total_g1 == 77, (
        "C3 抽取数不再是 77（现 %d）——词表或文法漂移，请人工确认" % total_g1)
    # C3b 的触发面必须非零
    assert _judge(R.ir_c8_execution_surface, facts, "IR-C8")["n_ir_refs"] == 85