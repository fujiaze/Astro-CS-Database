# eng/tests/e2e —— 端到端层测试（真实数据端到端与视觉验收辅助）

本目录是 ACSD 测试集的**端到端层**（e2e layer），与 `eng/tests/unit`（单元层）、
`eng/tests/module`（模块层）、`eng/tests/integration`（集成层）、`eng/tests/synthetic`
（合成全链层）、`eng/tests/conformance`（安装面）、`eng/tests/validation`（验证数据）并列。
分层依据 `standards/05_INDEPENDENT_TEST_SUITE.md:21`（e2e = 「真实数据端到端与视觉验收辅助」）。

## 1 目录内容

| 文件 | 用例数 | 作用 |
|---|---|---|
| `_realdata.py` | 0（无 `test_` 函数） | 共享工具面：真实数据索引、header 读取、fail-closed 锚守卫、XISF 1.0 最小解析器、视觉辅助面、B 类登记 |
| `test_dataset_integrity.py` | 25 | 数据面：可达性、帧集合指纹、WCS 可用性与单射性、命名与路径合同、畸变承载面、索引对账 |
| `test_calibration_inputs.py` | 18 | 标定输入面：母版可达性与可解析、几何匹配、平场覆盖、暗场曝光缺口、电子学量来源显式性 |
| `test_visual_acceptance.py` | 8 | 视觉验收**辅助**：拉伸 PNG 生成器、机器可读诊断量、禁止布尔门 |
| `test_pipeline_e2e.py` | 13（4 条 A 类守卫 + 9 条 B 类需构建） | 三命令端到端、产物符合性、原子性、L4 链路、两条上层移交项 |
| `README.md` | — | 本文件 |

**合计 64 条**：A 类 **55 条**（现在就跑、判绿）、B 类 **9 条**（显式 SKIP，需构建）。

> 本目录**不需要 `conftest.py`**：与 module / integration 层同构，用 pytest 默认的
> `prepend` 导入模式直接 `import _realdata`；共享状态只有一个只读切块
> （`test_visual_acceptance._sample_crop`，`functools.lru_cache(maxsize=1)`），不需要 fixture。

## 2 运行方式与实测读数

```bash
python3 -m pytest eng/tests/e2e/ -v
```

实测（HEAD `d93eb4ed`）：

```
$ python3 -m pytest eng/tests/e2e/ --collect-only -q
64 tests collected in 0.13s

$ python3 -m pytest eng/tests/e2e/ -q
55 passed, 9 skipped, 5 warnings in 24.73s   # 退出码 0
```

## 3 A 类与 B 类（本层硬约定）

| 类别 | 落点 | 能否现在跑 | 标记 |
|---|---|---|---|
| **A 类·数据面** | `test_dataset_integrity.py` / `test_calibration_inputs.py` / `test_visual_acceptance.py` + `test_pipeline_e2e.py` 的 4 条登记守卫 | **能** | 无 skip |
| **B 类·产品端到端** | `test_pipeline_e2e.py` 的 9 条 | **不能** | `@pytest.mark.skip(reason=…)` 显式标记 |

依据 `docs/engineering/testing/TEST.md:60`「硬件能力不可用时用 `GTEST_SKIP`，
不以伪通过掩盖未执行」——本层把同一口径用到「构建能力不可用」上。
**无一条 `pytest.skip`（运行时）、无一条 `xfail`、无 `try/except` 吞异常、无 `return` 假装通过。**

### 3.1 A 类用例逐条清单

**数据面（`test_dataset_integrity.py`，25 条 = 11 条判据 × 2 数据集 + 3 条非参数化）**

| # | 函数 | 判据 | 性质 |
|---|---|---|---|
| 1-2 | `test_DATASET_Integrity_DatasetReachable[ds]` | D1 可达性 | 正例 |
| 3-4 | `test_DATASET_Integrity_FrameCensusMatchesRegistration[ds]` | D8 帧集合指纹 | 漂移守卫 |
| 5-6 | `test_DATASET_Integrity_NamingContract[ds]` | D7 命名合同（4 个交叉合取项 + 2 条作用域） | 正例 |
| 7-8 | `test_DATASET_Integrity_DistortionFaceRegistration[ds]` | D4b 畸变承载面 | 漂移守卫 |
| 9-10 | `test_DATASET_Integrity_WcsUsableAndInvertible[ds]` | D5/D6 WCS 可用性与单射性 | 正例 |
| 11-12 | `test_DATASET_Integrity_WcsConsumesCdMatrixNotCdelt[ds]` | D9a WCS 承重哪张卡 | 正例 |
| 13-14 | `test_DATASET_Integrity_NoWcsFramesEnumeratedNotIgnored[ds]` | D4 无 WCS 帧点名 | 正例 |
| 15-16 | `test_DATASET_Integrity_CdCdeltConflictEnumeratedNotIgnored[ds]` | D9b CD/CDELT 矛盾登记 | 漂移守卫 |
| 17 | `test_DATASET_Integrity_IndexReconciliation` | D10 索引对账 | 漂移守卫 |
| 18 | `test_DATASET_Integrity_NegZeroObjectGuardRaises` | N-D1 零对象守卫 | 负例（fail-closed） |
| 19 | `test_DATASET_Integrity_NegMissingAnchorRaises` | N-D2 锚存活（4 个面） | 负例（fail-closed） |
| 20 | `test_DATASET_Integrity_NegFrameNameMismatchRaises` | N-D3 文件名合同 | 负例（fail-closed） |
| 21 | `test_DATASET_Integrity_NegHeaderMissingFieldRaises` | N-D4 必需头字段 | 负例（fail-closed） |
| 22 | `test_DATASET_Integrity_NegNamingViolationRed` | N-D5 命名合同注入 | 负例（S6） |
| 23 | `test_DATASET_Integrity_NegCensusDriftRed` | N-D6 指纹漂移注入 | 负例（S6） |
| 24 | `test_DATASET_Integrity_NegWcsSingularCdRed` | N-D7 奇异 CD 注入 | 负例（S6） |
| 25 | `test_DATASET_Integrity_NegWcsReaderFaceIsIndependent` | N-D8 两读入面独立性 | 负例（S6） |

**标定输入面（`test_calibration_inputs.py`，18 条）**

| # | 函数 | 判据 | 性质 |
|---|---|---|---|
| 1-3 | `test_CALIB_Inputs_MastersReachableAndParsable[dir]` | C1/C2 母版可达与可解析 | 正例 |
| 4-5 | `test_CALIB_Inputs_MasterGeometryMatchesLights[ds]` | C3 母版 ↔ 亮场几何 | 正例 |
| 6-7 | `test_CALIB_Inputs_FlatFilterCoverageComplete[ds]` | C5 平场滤镜覆盖（**当前无缺口**） | 正例 |
| 8-9 | `test_CALIB_Inputs_DarkExposureGapMatchesRegistration[ds]` | C4 暗场曝光缺口签名 | 漂移守卫 |
| 10 | `test_CALIB_Inputs_ElectronicsAreExternalInputs` | C6' 电子学量为外部输入 | 漂移守卫 |
| 11 | `test_CALIB_Inputs_SaturationFallsBackToDisabledNoMetadata` | C7 饱和电平落在 `DISABLED_NO_METADATA` | 正例 |
| 12 | `test_CALIB_Inputs_XisfByteLayoutMatchesRepoCpp` | C8 XISF 布局 ↔ `aio_xisf.cpp` 源码文本 | 正例 |
| 13 | `test_CALIB_Inputs_XisfPixelsMatchDeclaredGeometry` | C9 像元 ↔ 声明几何 | 正例 |
| 14 | `test_CALIB_Inputs_NegXisfDefectsRaiseNamedErrors` | N-C1 **12 种**注入逐条具名异常 | 负例（fail-closed） |
| 15-16 | `test_CALIB_Inputs_NegFlatFilterMissingRed[ds]` | N-C2 平场覆盖有牙 | 负例（S6） |
| 17 | `test_CALIB_Inputs_NegDarkGapDetectorHasTeeth` | N-C3 缺口探测器有牙 | 负例（S6） |
| 18 | `test_CALIB_Inputs_NegElectronicsKeywordAppearsRed` | N-C4 电子学量承载面漂移 | 负例（S6） |

**视觉辅助面（`test_visual_acceptance.py`，8 条）**

| # | 函数 | 判据 | 性质 |
|---|---|---|---|
| 1 | `test_VISUAL_Aux_StretchedPngWrittenForRealFrame` | V1 三种拉伸出图 | 正例 |
| 2 | `test_VISUAL_Aux_StretchRoundTripsThroughPng` | V2 图 ↔ 数列往返一致 | 正例 |
| 3 | `test_VISUAL_Aux_DiagnosticsReadWholeArray` | V4 反 fail-open | 正例 |
| 4 | `test_VISUAL_Aux_ReferenceImagesAbsentRegistration` | V3 零参考图登记 | 漂移守卫 |
| 5 | `test_VISUAL_Aux_NoBooleanVerdictIsProduced` | V7 **禁止布尔门** | 正例 |
| 6 | `test_VISUAL_Aux_NegInjectedDefectsAreDiagnosed` | N-V1 四类注入逐条点名 | 负例（S6） |
| 7 | `test_VISUAL_Aux_NegSaturationLevelExternalIsNoneNotZero` | N-V2 反 fail-open | 负例 |
| 8 | `test_VISUAL_Aux_NegStretchRejectsDegenerateInput` | N-V3 拉伸 fail-closed | 负例（fail-closed） |

**B 类登记守卫（`test_pipeline_e2e.py` 的 A 类部分，4 条）**

| # | 函数 | 判据 | 性质 |
|---|---|---|---|
| 1 | `test_E2E_Registry_MarkerAndRegistryAgree` | R1 标记 ↔ 登记双向对齐 | 守卫 |
| 2 | `test_E2E_Registry_SkipReasonsCarryRecord` | R2 reason ↔ 登记**逐字段精确相等** | 守卫 |
| 3 | `test_E2E_Registry_ProbedArtifactsCoverRequires` | R3 登记引用 ⊆ 探针清单 | 守卫 |
| 4 | `test_E2E_Registry_EveryRecordHasReasonAndArtifact` | R4 登记信息完整 | 守卫 |

### 3.2 B 类「需构建」完整清单（9 条，逐条理由）

全部位于 `test_pipeline_e2e.py`，登记在 `_realdata.BUILD_REQUIRED_REGISTRY`：

| # | `judge_id` | 依赖产物 | 需构建的理由 | 阻塞来源 |
|---|---|---|---|---|
| 1 | `E2E.NormalizeSerialChain` | `build/acsd`、`normalize.phase_config.json` | 判据对象是 `acsd normalize --json <cfg>` 的运行结果；产品未在本单车道构建，且工作树里的 `build/acsd` 由 commit `143af8a` 产出、早于当前 HEAD，跑它等于验证陈旧构建 | T10 构建车道；本单禁止跑构建与端到端全流程 |
| 2 | `E2E.MosaicSerialChain` | `build/acsd`、`mosa.phase_config.json` | 需消费 normalize 产出的 HiPS 产品树，上游产物不存在则无从进入 | `E2E.NormalizeSerialChain` |
| 3 | `E2E.ExportSerialChain` | `build/acsd`、`export.phase_config.json` | 需消费 mosaic 产出的 HiPS 产品树 | `E2E.MosaicSerialChain` |
| 4 | `E2E.ProductManifestConformance` | `build/acsd` | 判据对象是三命令落盘的 `manifest.json`，无运行即无对象 | 三条链 |
| 5 | `E2E.Atomicity` | `build/acsd` | 「失败后不得留下半成品产品树」是**运行期**性质，纯静态/纯数据层无从观测 | T10 构建车道 |
| 6 | `E2E.VisualAcceptanceChain` | `build/acsd` | L4 链条「马赛克 → 平面 FITS → 拉伸 PNG → 切块目检」前两级都要产品产物；末级判定权属项目负责人 | `E2E.MosaicSerialChain` / `E2E.ExportSerialChain` |
| 7 | `E2E.XisfReaderMatchesProduct` | `build/acsd`、`aio_xisf.cpp` | 本层 Python XISF 解析器与产品 `xisf_read_file()` 的**像元逐点一致性**尚未取得任何证据，只能由产品读同一文件来对拍 | T10 构建车道 |
| 8 | `E2E.WcsTransformDifferential` | `build/acsd`、`phot_verify/wcs_lib.py`、`wcs_transform.cpp` | **集成层报告已登记移交**：以 `wcs_lib.py`（`pc::WcsTransform` 的 numpy 移植）对拍产品，需编译并运行产品 | T10 构建车道（上层已登记移交） |
| 9 | `E2E.PhotometrySigmaRealFrameLeg` | `build/acsd` | **单元层报告已登记移交**：「S6 P1 测光 σ 双边界的真实帧腿」需要产品对真实帧出读数 | T10 构建车道（上层已登记移交） |

## 4 正本依据与容差档

### 4.1 判据 → 正本（逐条 `文件:行`）

| 判据 | 正本依据 |
|---|---|
| e2e = 真实数据端到端与视觉验收辅助 | `standards/05_INDEPENDENT_TEST_SUITE.md:21` |
| 第三类实验数据 = M42 与银心，含视觉验收 | `standards/04_SCIENCE_EVIDENCE_AND_EXPERIMENT.md:32-34`；`docs/ACSD_DESIGN.md:497` |
| L3 小批量端到端（三命令串行 + 原子性抽检） | `docs/ACSD_DESIGN.md:532` |
| L4 真实视觉验收，**由负责人目检判定** | `docs/ACSD_DESIGN.md:533`、`:536`、`:558` |
| 发布决定只属项目负责人 | `AGENTS.md` §11 |
| 三命令唯一命令树 | `docs/engineering/contracts/CLI_PROTOCOL.md:16,122` |
| 跨阶段唯一载体 = HiPS 产品树 | `docs/engineering/contracts/PIPELINE_BLOCK.md:20,66` |
| 饱和电平来源优先级 + 三级皆无 ⇒ `DISABLED_NO_METADATA`，禁静默 | `eng/contracts/schemas/phase_config_normalize.schema.json#/$defs/noise_config/properties/saturation_level` |
| XISF 魔数 / 64 位小端头长 / 64 MiB 上限 / `attachment:` / 几何上界 | `lib/infrastructure/aio/src/aio_xisf.cpp:25,29,33,91-98,158-181,447,452-468` |
| 容差档（元数据/计数/索引 = **精确一致**） | `docs/engineering/testing/TEST.md:46` |
| NaN / Inf / 「缺失」是并列三态；不得折叠成哨兵值 | `docs/engineering/testing/TEST.md:75-82` |
| 硬件能力不可用时显式 SKIP，不以伪通过掩盖 | `docs/engineering/testing/TEST.md:60` |
| 每个度量须有非退化判据；恒真比较无证据资格 | `docs/engineering/testing/TEST.md:26` |
| 判据必须能红；注入缺陷时读数不红即判该判据失效 | `docs/engineering/testing/TEST.md:62` |
| 锚存活 fail-closed / 零对象守卫 | `docs/engineering/testing/VALIDATION_EVIDENCE.md:412-413,170-176` |
| 判别力 S2 / S6 / 反例隔离 | `docs/engineering/testing/VALIDATION_EVIDENCE.md:189,193,197` |

### 4.2 容差档

本层**几乎不比较浮点量**：55 条 A 类里 **53 条**的比较对象是元数据、计数、索引、
选择结果（落 `TEST.md:46` 第一档「精确一致」，`EXACT_TOLERANCE = 0.0`，无 `rtol`/`atol`）。

**唯一一处浮点门限**是 D5/D6 的 WCS 往返闭合：`WCS_ROUNDTRIP_MAX_PX = 1e-4` 像元。
取值依据写在 `_realdata.py` 常量处，摘要：

- 判据要抓的是「WCS 是不是单射」（奇异 CD、零行列式、投影退化），**不是**「精度够不够」；
- `1e-4` 像元 = 像元尺度的万分之一，亚像素定位的典型需求是 0.1 像元，比它宽 3 个数量级；
- 实测库噪声底 `6.44e-6` 像元（银心 TAN+SIP 路径最大值），留 **15×** 余量；
- 判据可信度**锚在负例 N-D7**（注入奇异 CD 必红），不锚在这个数字上；
- 改阈值属 `TEST.md:36`「容差调整单独提交，附失败分布与推导」的动作。

**容差在写用例前冻结**（`TEST.md:36`）。

### 4.3 独立 Oracle 来源

`oracle.truth` = **FITS/XISF 格式规范 + 仓内一手 C++ 读入器的源码文本 + 逐帧头独立实算**；
`oracle.must_not` = 产品可执行程序与调度器代码面。本层**只读**仓内数据文件与源码文本，
不执行任何产品二进制、不起子进程、不链接库。

⚠ **不读 `testdata/index.json` 的任何现值生成任何预期值** —— 索引在本层是**被对账对象**（D10）。

### 4.4 实算对象数（`VALIDATION_EVIDENCE.md:173` 的 `n_objects`）

| 量 | 值 |
|---|---|
| 亮场帧总数 | **353**（M42 196 + 银心 157） |
| 母版总数 | **27**（T2/T3/T4 各 9，全部 XISF 1.0） |
| 带 WCS 的帧 | 322（M42 166 + 银心 156） |
| 无 WCS 的帧 | 31（M42 30 + 银心 1） |
| 暗场曝光缺口帧 | **147**（M42 的 300 s 档，T2 53 + T3 94） |
| testdata 树内参考图 | **0** |

## 5 XISF 门槛的裁定（本单第一个决策点）

派单给出三选一：(a) 用 `build/acsd` 产品 IO 面读母版；(b) 自写 XISF 1.0 最小解析器；
(c) 母版不在本层职责内，增益/读噪从配置读并标注为外部输入。

**裁定：(b) 收窄到「头 + 未压缩附加块」 + (c) 必然；(a) 否决。**

- **否决 (a)**：它把整个标定输入面变成「需构建」，等于把这一层的证据价值归零；
  派单本身也**禁止本车道构建与运行产品**；且 (a) 下 Python 侧拿不到可复核的中间读数，
  解析器的正确性也无从在纯 Python 面证伪。
- **采纳 (b) 并收窄**：实测 27 个母版全部 `sampleFormat="Float32"`、
  `location="attachment:<off>:<size>"`、**未压缩**，声明字节数恒等于 `W·H·C·4`（27/27 逐个相符）。
  字节布局**不发明**，逐条对照仓内一手 C++ 读入器 `aio_xisf.cpp`；
  判据 **C8** 把本解析器的常量与那份 `.cpp` **源码文本**逐条对撞（魔数 / 64 MiB 上限 /
  几何上界 / 64 位小端拼装 / `attachment:` 前缀 / `memcmp` 魔数比对，6 个锚点）。
  负例 **N-C1** 用 **12 种**注入证明解析器 fail-closed（含坏魔数、零头长、超限头长、
  截断、缺 `schemaLocation`、无 `<Image>`、非法几何、未知样格式、非附加块承载、
  声明字节数不符、附加块越界）。
- **(c) 不是可选项而是必然**：实测**亮场 353 帧与母版 27 个都不携带任何**
  `GAIN/EGAIN/READNOISE/RDNOISE/SATURATE/DATAMAX`；三份 `phase_config_*.schema.json`
  也**没有 `gain` 键**。⇒ 增益与读噪在仓内**没有任何承载面**，只能作外部输入。
  本层的职责是把这件事做成**可判读的漂移守卫**（C6'：这些关键词一旦出现即判红，
  强制重做裁定），而不是替它编一个数。
  饱和电平按合同三级优先级（`noise.saturation_level` > `snr.saturation_level` >
  FITS `SATURATE`/`DATAMAX`）实测**三级皆缺** ⇒ 落在合同规定的末支
  **显式 `DISABLED_NO_METADATA`，禁静默**（C7 逐级实测并把末支写死进登记）。

### 5.1 与仓内 C++ 读入器的一处有意分歧（REG-08）

`aio_xisf.cpp:64` 对未知 `sampleFormat` 的处理是 `aio_log(WARN)` 后 **fallback 到 Float32**
（fail-open）。本层**不复制该模式**：未知格式一律抛 `XISF_UNSUPPORTED_SAMPLE_FORMAT`。
27 个母版全部显式声明 `Float32`，故该分歧在当前数据上**不可观测**；
一旦出现未声明格式的文件，本层判红而产品会静默按 Float32 解释。
**登记，不在本层修改产品**（产品代码不在本单写域内）。

## 6 负例有效性自证表

**隔离**：全部负例在 `tmp_path` 上注入，**不写仓内留证**
（`VALIDATION_EVIDENCE.md:197`「反例复跑必须隔离」）。
**红侧归属**：每条负例都调用**被判定的那个判定函数本体**或**解析器自身**，
不是共用逻辑的旁路（`:193` S6）。
**同时验证绿（S2）**：每条负例先对未注入的仓内数据跑一次并断言判绿。

| 负例 | 注入点 | 注入内容 | 期望判红 | S2 对照 |
|---|---|---|---|---|
| N-D1 | `DATASET_DIR` 映射 | 指向空目录 | `ZERO_OBJECT_GUARD` | — |
| N-D2 | `DATASET_DIR` / `INDEX_REL` / 未知 id | 目录不存在 / 索引不存在 / 未知数据集 | `ANCHOR_STALE`（4 个面逐条） | — |
| N-D3 | 帧文件名 | 不匹配命名合同 | `FRAME_NAME_MISMATCH` | — |
| N-D4 | 合成帧头 | 删掉 `EXPTIME` | `HEADER_MISSING_FIELD` | — |
| N-D5 | 帧文件名 | 曝光秒 `-999S` vs 头 300 | `NAME_EXPTIME_MISMATCH` | ✓ |
| N-D6 | 帧枚举面 | 抽掉 1 帧 | `FRAME_COUNT_DRIFT` + `FRAME_EXPTIME_CENSUS_DRIFT` | ✓ |
| N-D7 | 帧头 WCS | 注入 `CD` 全零（奇异） | `SingularMatrixError` → `FRAME_UNPARSABLE` | ✓ |
| N-D8 | WCS 两读入面 | 由测试独立构造 CDELT-only 面 | 比值落 `CD` 侧；**同时**证明两面分离 | ✓ |
| N-C1 | XISF 字节入口 | 12 种注入 | 12 条**各自**的具名异常前缀 | ✓（先断言合法合成文件判绿） |
| N-C2 | 亮场头 `FILTER` | 注入 `NoSuchFilterE2E` | `FLAT_FILTER_MISSING` | ✓ |
| N-C3 | 亮场头 `EXPTIME` | 注入 777 s | 缺口图新增 `{'T4': {777.0: 1}}` | ✓ |
| N-C4 | 亮场头 | 注入 `GAIN` + `RDNOISE` | `ELECTRONICS_KEYWORD_APPEARED` 逐个点名 | ✓ |
| N-V1 | 诊断函数输入数组 | 全零块 / NaN / ±Inf / 饱和 | 四个诊断量逐条为预期计数 | ✓（对照面四项全 0） |
| N-V2 | 诊断函数参数 | `saturation_level=None` | `n_saturated is None`（**不是 0**） | — |
| N-V3 | 拉伸函数输入 | 常量图 / log 非正像元 / 未知模式 / 全 NaN | 4 条 `STRETCH_*` 具名异常 | ✓（正数组 log 必须能出图） |
| V2(d) | 期望面 | 人为加一层全局变换 | 中位偏差不再为 0 | — |

### 6.1 R1–R4 登记守卫的有效性（实测扰动，`/tmp` 副本上做，仓内文件零改动）

| 扰动 | 实测结果 |
|---|---|
| baseline | `4 passed` |
| 登记多一条没写用例的 id | `2 failed` |
| 登记里的产物路径拼错（`build/acsd` → `build/acsd_typo`） | `2 failed` |
| 登记理由改成「待定」 | `2 failed` |
| `blocked_by` 从「T10 构建车道（单元层报告已登记移交端到端）」缩成「T10 构建车道」 | `1 failed` |
| 把某个 `skip` 标记换成 `noop` / `skipif` | `2 failed` |
| reason 里的 `blocked_by` 文案漂移 | `1 failed` |
| reason 里的 `judge_id` 漂移 | `1 failed` |
| reason 里的 `requires` 漂移 | `1 failed` |

> ⚠ **`blocked_by` 缩短这一条最初没被抓到**：R2 最初用「子串包含」比对，
> 缩短后的 `blocked_by` 仍是原 reason 的子串 ⇒ 通过。**这是我自己判据里的一个洞**，
> 由上表的扰动实测抓出后改成**逐字段精确相等**（`_parse_reason` + `_reason_fields`），
> 并把 skip reason 改成**字面量**而不是运行时拼接（否则 R2 会退化成「登记表与自己比对」的恒真检查）。

## 7 视觉验收辅助的诚实边界

⚠⚠ **本目录不产出任何「视觉验收通过/不通过」的判决。**

- **规范要求的**（`ACSD_DESIGN.md:533`）是 L4 链条「马赛克 → 平面 FITS → 拉伸 PNG → 切块目检」，
  且明写「**由负责人目检判定**」；`AGENTS.md` §11 与 `ACSD_DESIGN.md:558` 把发布决定划给负责人。
- **实测本仓零参考图**：`testdata/` 全树 PNG/JPG/JPEG/PDF/TIF/TIFF 计数 = **0**（V3 守卫）。
  没有基准可比 ⇒ 本层**不可能**给出比图判据。
- 交付的是**辅助**：
  1. **拉伸 PNG 生成器**（纯 numpy + `matplotlib`，本机实测 3.11.2 可用）。
     逐像元 1:1 渲染（`figsize = w/dpi`、`dpi=100`、坐标轴满幅、标题写进 PNG 元数据
     而不占像素网格）。非有限像元**不折叠成 0、不填哨兵**（`TEST.md:79`），
     在图上渲成洋红 ⇒ 黑洞是**看得见的色块**而不是一片黑。
  2. **机器可读的诊断量**（14 项冻结词表 + `finite_pixels`）：`n_nan`、`n_posinf`、
     `n_neginf`、`n_saturated`、`n_zero_blocks`、`n_zero_block_pixels`、`vmin`、`vmax`、
     `dynamic_range`、`background_median`、`background_mad`、
     `n_below_background_3mad`、`n_above_background_3mad`、`n_pixels`。
     这些是**诊断读数，不是门**；本层**不给它们设合格线**。
  3. **禁止布尔门是可执行约束**（V7）：断言诊断字典的键集恰等于冻结词表 ∪ `{finite_pixels}`，
     且**值里没有任何 `bool`**（`bool` 是 `int` 子类，必须显式排除），
     渲染返回字典同样。⇒ 谁塞一个 `passed: True` 进去就判红。
- **临时产物纪律**：拉伸 PNG **绝不写进 `testdata/`**，全部落在 pytest 的 `tmp_path`，
  随临时目录销毁。

### 7.1 实际生成了什么

`test_VISUAL_Aux_StretchedPngWrittenForRealFrame` 对**真实帧**的固定中心切块
（`testdata/M42_T2T3_mosaic_Flying_dutchman/T2/M1/M42_M1_T2_flying_dutchman-20251212@012404-300S-Red.fts`
的 `1024×1024` 中心块）出三张图：`sample_linear.png`、`sample_log.png`、`sample_asinh.png`，
每张 **1024×1024** 像素。三张都落在 `tmp_path`
（实测路径形如 `/tmp/pytest-of-dsh/pytest-<n>/test_VISUAL_Aux_Stretche0/sample_asinh.png`），
**不留在仓内**。

## 8 登记项（实现与正本的冲突 / 缺口 / 数据缺陷）

| ID | 事项 | 实测证据 | 处置 |
|---|---|---|---|
| **REG-01** | **M42 的 166 个带 WCS 帧，`CD` 与 `CDELT` 两种表述互相矛盾。** `|CD1_1| / |CDELT1|` ∈ **[0.00372, 0.0208]**（差 48–269 倍）；166 帧的 `CD` 矩阵**全部强非对角**（对角 ≈0.0165″、非对角 ≈±0.966″，约 90° 旋转）。FITS WCS Papers II 把两者定为**冗余表示**，应自洽。 | `judge_cd_cdelt_conflict` 实测 166 条命中；银心无 `CDELT1`、命中 0 条 | **如实登记**，不改数据、不降级阈值。⚠ **口径澄清（实测得出，先前推断已被推翻）**：这**不是**「像元尺度差 50 倍」——两者的**面积尺度** `sqrt(|det CD|)` 与 `sqrt(|CDELT1·CDELT2|)` 只差 **1.4e-4** 相对量。矛盾在「两种表述对 x 步长的描述」上。守卫 D9b 逐帧点名，命中数与冻结登记比对（数据修了就判红）。 |
| **REG-01b** | **M42 的畸变解无法被任何 FITS 标准读取器套用。** 166 帧**只有** `TR1_*` / `TR2_*`，**零** `PV1_*`/`PV2_*`（标准老式不规则畸变前缀）、**零** `A_*`/`B_*`（SIP），且 `CTYPE1 = 'RA---TAN'` **未声明 `-SIP`** ⇒ 标准读法下退化为纯 TAN。银心的 156 帧则是标准 SIP（`A_/B_/AP_/BP_` 齐备、`CTYPE1 = 'RA---TAN-SIP'`）。 | `scan_distortion_face` 实测：`m42` → `TR1_/TR2_` 166 帧、`PV*` 0 帧、`A_/B_` 0 帧、`-SIP` 0 帧；`galaxy_center` → `A_/B_/AP_/BP_` 156 帧、`-SIP` 156 帧 | **如实登记**。这也是实测 WCS 往返误差分两个量级的成因（M42 中位 1.21e-10 px 纯双精度舍入 vs 银心中位 2.36e-7 px 走 wcslib 迭代）。判据 D4b 保证该读数不悄悄漂移。**需负责人裁定**：`TR1_*/TR2_*` 是谁写的、正确的前缀应当是什么。 |
| **REG-02** | **M42 的 300 s 档没有任何同望远镜 masterDark 可用 —— 147/353 帧（41.6%）在曝光匹配上无母版。** T2 有 600/1200/1800 s 暗场、T3 有 600/1200 s 暗场，**都没有 300 s**；而 M42 的 300 s 亮场共 147 帧（T2 53 + T3 94）。银心（T4）的 180/300/600 s 三档暗场齐备，无缺口。 | `judge_dark_exposure_coverage` 实测 `{m42: {T2: {300.0: 53}, T3: {300.0: 94}}, galaxy_center: {}}` | **如实登记**，不改数据、不降级阈值。判据 C4 做**缺口签名漂移守卫**（补了 300 s 母版就判红，强制重做裁定）。**不**写成「期望缺口为空」的绿用例（那等于要求数据没这个缺陷），也**不**写成永远红的用例（那会让本目录长期带红且无裁决主体）。**需负责人裁定**：这 147 帧是否只能走「无暗场」通道，还是数据侧应补母版。 |
| **REG-03** | **平场滤镜大小写两套写法。** T2 的 OIII 母版文件名写 `FILTER-OIII`，T3/T4 写 `FILTER-Oiii`；亮场一律写 `Oiii`。 | 逐母版文件名实算 | 判据 C5 按**精确串**匹配、**不做大小写归一**。银心实际用到 `Oiii`、T4 母版写 `Oiii` ⇒ 判绿。M42 不用 OIII 通道，该分歧当前不可观测。**需负责人裁定**：统一到哪一种写法。 |
| **REG-04** | **353 帧缺 WCS。** M42 **30/196**、银心 **1/157**（`Galaxy_Center_mosaic1_T4_flying_dutchman-20250813@010214-600S-Oiii.fts`）。这 31 帧无 WCS ⇒ 无法参与任何基于天球的跨帧定标。 | 逐帧头实算，判据 D4 逐条点名并两集合互斥 | **如实登记**。判据 D4 只保证「被点名的集合」与「复查结果」一致且两集合互斥；**不在本层**替这 31 帧补 WCS（那是产品/数据侧的事）。 |
| **REG-05** | **testdata 树内零参考图**（PNG/JPG/JPEG/PDF/TIF/TIFF 计数 = 0，全树 906 个 `.fts` + 355 json + 27 xisf + …）。⇒ 「视觉验收」没有任何现成基准可比。 | `count_reference_images()` 实测 `{}` | **如实登记**。判据 V3 做漂移守卫：有人补进参考图就判红，强制先裁定「是否改为与参考图比对」，**不允许默默开始比对**，也不允许本层删参考图。 |
| **REG-06** | **`testdata/index.json` 的 `Galaxy_Center_T4.observation_dates` 过期。** 索引自陈 7 天（`2025-07-02…07-03, 07-04, 07-16…07-19`），磁盘逐帧 `DATE-OBS` 实算 **8 天**，索引缺 **`2025-08-13`**（当天有 12 帧）。 | `index_date_reconciliation` 实测 `only_disk = ('2025-08-13',)` | **如实登记，不私自改 `testdata/`**（不在本单写域）。判据 D10 做**缺口签名漂移守卫**（缺口消失或换样都判红）。⚠ 派单提到的「datasets[7] 的 180 s HDR 帧」**已被索引自己登记**：`datasets[7].notes` 逐字写「素材信息提及『另送T3核心HDR曝光111分钟+925HD核心HDR曝光111分钟（单张180秒）』，磁盘实测未见 180s 帧（对账差异如实登记，以磁盘实测计数为准）」——**不是未登记的缺陷**，此处更正派单表述。 |
| **REG-07** | **电子学量（增益/读噪/饱和）在仓内没有任何承载面。** 353 帧与 27 母版**全部**不携带 `GAIN/EGAIN/READNOISE/RDNOISE/SATURATE/DATAMAX`；三份 `phase_config_*.schema.json` 也**没有 `gain` 键**（连 `READNOISE` 之类都没有）。 | `scan_electronics_keywords()` 实测 `present` 全空、`scanned_lights=353`、`scanned_masters=27`；C6' 对三份 schema 逐份断言无 `"gain"` 键 | **登记为外部输入**（`declared_external_inputs()`），**不发明数值**。饱和电平按合同三级优先级实测**三级皆缺** ⇒ 落在末支 **显式 `DISABLED_NO_METADATA`**（C7）。判据 C6' 做漂移守卫：这些关键词一旦在数据或配置里出现即判红，强制重做裁定。**需负责人裁定**：若后续噪声模型需要物理增益/读噪，承载面必须先立项。 |
| **REG-08** | **仓内 C++ XISF 读入器对未知 `sampleFormat` 是 fail-open**：`aio_xisf.cpp:64` `aio_log(WARN)` 后 fallback 到 Float32。 | 源码逐行；27 个母版全部显式声明 `Float32`，该路径当前**不可观测** | **登记**。本层解析器**有意更严**（未知格式判红）。不在本层修改产品（不在写域）。 |
| **REG-09** | **M42 的 `CTYPE1` 未声明 `-SIP`，但 `TR1_*/TR2_*` 系数在场**（见 REG-01b），⇒ `CTYPE` 与系数两面**互相矛盾**：读 `CTYPE` 的人当成纯 TAN，读系数的人当成有畸变。 | 同 REG-01b | 与 REG-01b 合并裁定。判据 D4b 保证两面各自的读数不漂移；本层**不**选边（选边是产品/数据侧的口径决定）。 |
| **REG-10** | **工作树里的 `build/acsd` 由 commit `143af8a` 产出，早于当前 HEAD。** | `build/astrocs.product.json` 的 `source_commit` 字段逐字为 `143af8a63037935b77080867ccc5e217b671a632` | **登记**。这是 B 类「需构建」的**具体**理由之一：即便允许执行，跑它验证的也不是当前提交。本层**不**把「产物存在」写成绿判据（那是给陈旧构建背书）。 |

## 9 未覆盖 / 否决的判据

| 项 | 处置 | 理由 |
|---|---|---|
| 用 `sqrt(|det CD|)` 做 D9a 的第二观测面 | **否决** | 实测它与 wcslib 的 `proj_plane_pixel_scales` 相对差达 **2.8e-3**（投影非线性），要判绿就得把容差放宽到 1e-2 量级 —— 那就是「改阈值让判据能跑」。改用 **x 轴步长**（两读入面相差 **496 倍**）作可观测面。 |
| 「本层像元尺度取自 `CD`」 | **推翻重写** | 起初写成「`max(proj_plane_pixel_scales)` 必须不等于 `|CDELT1|×3600`」。实测发现 `m42` 的 `CD` 矩阵强非对角，`proj_plane_pixel_scales` 取的是**面积尺度**，与 `sqrt(|CDELT1·CDELT2|)` 只差 1.4e-4 ⇒ **该判据在 `m42` 上不可观测、恒红**。已改为 D9a 的双读入面版本。 |
| 「WCS 往返闭合用 `rtol·NAXIS + atol·NAXIS`」 | **否决** | 门限会随帧几何缩放，那是「改阈值让测试能跑」的一种形态。改为**无参的冻结常量** `WCS_ROUNDTRIP_MAX_PX`，并在常量处附取值依据与「不随数据调整」的承诺。 |
| 「暗场曝光必须全覆盖」写成绿的正例 | **否决** | 当前数据有真实缺口（REG-02，147 帧），写成绿用例等于要求数据没这个缺陷。改为**缺口签名漂移守卫**（与模块层 REG-02/REG-03 同一处置体例）。 |
| 「CD/CDELT 必须一致」写成「期望 0 命中」的绿用例 | **否决** | 同上。改为命中数与冻结登记比对（REG-01）。 |
| 「索引日期必须与磁盘一致」写成绿用例 | **否决** | 同上（REG-06）。改为缺口签名守卫。 |
| 「视觉验收通过」布尔门 | **否决并反向加固** | 派单明令禁止。本层把它做成**可执行**约束（V7：键集冻结 + 无 `bool` 值）。 |
| L4 全链路（马赛克 → 平面 FITS → 切块目检） | **不在 A 类** | 前两级要产品产物 ⇒ 登记为 B 类 `E2E.VisualAcceptanceChain`。 |
| `synth_core.py` 吸收 | **不在本单范围** | `eng/tests/validation/release02/q1_photometry_gradient/src/synth_core.py` 属**合成全链层**（`eng/tests/synthetic/`），本层不吸收；在此点明以免读者以为漏了。 |

## 10 与其它层的关系

| 层 | 本层与它的边界 |
|---|---|
| `eng/tests/unit/` | 单元层报告已登记把「S6 P1 测光 σ 双边界的**真实帧腿**」移交本车道 ⇒ B 类 `E2E.PhotometrySigmaRealFrameLeg`（需构建）。 |
| `eng/tests/module/` | 块生命周期判据域（静态/纯数据）。本层只读 FITS/XISF 数据面，两层无共享判定。 |
| `eng/tests/integration/` | 集成层报告已登记把 `eng/tests/validation/release02/phot_verify/wcs_lib.py` 的**差分测试**（对拍产品 `wcs_transform.cpp`）移交构建/端到端车道 ⇒ B 类 `E2E.WcsTransformDifferential`（需构建）。 |
| `eng/tests/synthetic/` | 合成全链层（哈勃仿真与代数合成端到端）。本层**只做真实数据**面，两者互补不重叠。 |
| `eng/tests/conformance/` | 安装面（`noop` 单元进生产安装树）。本层**不触碰** —— 误删会静默失去生产件。 |
| `eng/tests/validation/` | 历史验证脚本的**来源**，按 `05_INDEPENDENT_TEST_SUITE.md:31-33` 逐条评估后**吸收有效检验、重写为有意图的用例**；本层吸收了 `wcs_lib.py` 对应的差分判据（B 类）与 XISF/暗场/平场面的元数据判据。 |

## 11 命名

函数名 `test_<Suite>_<Feature>`（pytest 不接受点号），`Suite.Feature` 标识与函数名的
映射表在每个 `test_*.py` 的**模块 docstring** 里逐条列出（`TEST.md:88`）。
B 类判据另用 `judge_id`（点号形式）登记在 `_realdata.BUILD_REQUIRED_REGISTRY`，
由 R1 守卫把 `judge_id` → 函数名按 `test_E2E_Pipeline_<Feature>` 约定双向对齐。

## 12 纪律自检

- **不编造**：每条断言的预期值来自 (a) 逐帧 FITS 头 / XISF 头的独立实算、
  (b) 写用例前冻结的登记常量（附取值依据）、(c) FITS/XISF 格式规范、
  (d) `AGENTS.md` / `ACSD_DESIGN.md` / `TEST.md` / `VALIDATION_EVIDENCE.md` 的条款、
  (e) 仓内 C++ 读入器的**源码文本**。每条用例 docstring 末尾有 `**来源依据**：…`。
- **不迁就实现**：REG-01…REG-10 如实登记，未改数据、未改 `index.json`、未改 schema、
  未降级阈值、未修改产品代码。
- **不堆叠豁免**：全目录**无**运行时 `pytest.skip`、**无** `xfail`、无空断言。
  9 条 B 类用 `@pytest.mark.skip` 且 reason 写成**字面量**（含 id / 产物 / 阻塞来源 / 理由），
  `pytest -rs` 逐条打出来。
- **不写恒真断言**：每条断言写前自问「字面读法下是否恒为真/恒为假」。
  §6.1 记录了一条**我自己判据里的恒真洞**（R2 的子串包含）及其修法；
  §9 记录了两条被**实测推翻**的判据设计。
- **fail-closed**：数据缺失 / 文件不可解析 / 缺必需头字段 / XISF 任一环节不成立 ⇒
  抛**具名异常**（`ANCHOR_STALE` / `ANCHOR_UNPARSABLE` / `ANCHOR_MISSING_FIELD` /
  `ZERO_OBJECT_GUARD` / `FRAME_UNPARSABLE` / `HEADER_MISSING_FIELD` /
  `FRAME_NAME_MISMATCH` / `XISF_*` / `STRETCH_*`），N-C1 用 **12 种**注入逐条证过。
  **无一条 skip、无一条静默降级、无一条 `default=0.0` 式 fail-open**
  （反面教材 `实验/m42-realdata/code/c3_seam_additive.py:302` 已由 V4 做成可执行对照）。
- **不裁决代码**：不产出阻塞退出码，不接 CI，不注册进构建；
  测试代码里**无** `sys.exit` / `raise SystemExit` / `os._exit`。
  判红是**缺陷信号**，处置由人读对抗性审核给出。
- **不越界**：写域仅 `eng/tests/e2e/`；未跑构建、未跑 `build/acsd`、未跑三命令全流程；
  扰动自证全部在 `/tmp` 副本上做，仓内文件零改动；未做任何 git 写操作。