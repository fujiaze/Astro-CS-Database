# 模块 acsd.p2.coverage

> 上游：`docs/ACSD_DESIGN.md`对应章节（模块与 ABI）、对应章节（固定科学流程：coverage 重叠图）、
> 科学正本：`docs/science/PHASE2_UPM.md`对应章节（覆盖并集）、`docs/science/INTEGRATION.md`对应章节
> （support/validity 分离）、docs/science/unified/SCIENCE_SCOPE.md（处理链第 5 环节，`## 2 物理模型`）、
> `docs/science/noise_snr/NOISE_SNR.md`对应章节（适用域与失效域）、
> `docs/science/noise_snr/NOISE_SNR.md`对应章节（信息量定义）
> 算法正本：docs/science/algorithms/PHASE2_COVERAGE.md（ALG-COV-001；状态声明 对应章节、
> 逐公式锚 对应章节、缺陷登记 对应章节、测试设计 对应章节）
> 数据正本：docs/detail/registry/acsd.phase2.coverage.md（DATA-COV-001 端口表，本页输入输出端口表）、
> eng/contracts/schemas/unified/coverage.schema.json（coverage 对象 canonical schema）
> API 正本：docs/engineering/api/PUBLIC_API.md（API-COV-001，Coverage union C API 节）、
> docs/engineering/api/PUBLIC_API.md「分阶段 API 面」（API-P2-001，FROZEN）

模块级事实以 `lib/algorithms/coverage/README.md` + `lib/algorithms/coverage/module.yaml`
（MOD-acsd-phase2-coverage，dll_target=acsd_p2_coverage.dll）为准；权威签名头
`lib/algorithms/coverage/include/astro/phase2/coverage.h`。模块词汇
`acsd.p2.coverage`，owner = SA-P2-S20，实现面 = lib/algorithms/coverage；
depends_on_int = IO-003 / DATA-004 / RT-006。目标交付形态
acsd_p2_coverage.dll 为合同值，尚未落地。

## 职责与明确非职责

职责：建立输入帧 / 区域的重叠图与几何有效域，输出 coverage 产品与连通分量。
本模块是 Phase2 DAG 的首节点。

**不做**：不做权重；**coverage 是几何 / 数据有效域，不是权重，不能作为
inverse-variance 或 SNR 权重**；不做集成。

**科学红线（负向条款）**：coverage / support / validity 三概念分离（support =
SCI-INT-001 样本级 [0,1]、validity = 对应章节 有效性标志，均不在本模块域）；
**no use as implicit scientific weight** —— union cell / n_tiles / 覆盖帧数是
几何登记量，禁入任何权重式（`w_UPM` 的唯一冻结式 = PHASE2_UPM.md 对应章节，support
仅承担 eligibility / coverage 语义）。

**归属边界**：处理链阶段序 = coverage → sampler → …，后续环节不属本模块。

## 输入输出端口、DATA、单位、坐标、invalid

| 端口 | DATA | 必/可 | 单位 | 坐标 |
|---|---|---|---|---|
| `calibrated` | `DATA-COV-001`（入：HiPS 树路径数组 `const char* const* [n_inputs]`） | 必 | —— | —— |
| `coverage` | `DATA-COV-001`（出：union MOC，`P2MocCell [K]` 无量纲整数） | 可 | 无量纲 | `HEALPix NESTED equatorial / ICRS` |

`DATA-P2-COV` 端口名为编排词汇，权威定义 = DATA-COV-001（本页输入输出端口表）。

输入面：一组合同兼容 Phase1 产品（含各自 coverage / validity / WCS / manifest）。
产品的落盘形态不进入科学语义（裸 / 归档同义，形态由落盘名判定）；`hips_paths` 的
元素**保持字符串**，逐帧产品级索引路径由命名规则派生；按天区查帧集合走**块级
覆盖索引**（不压缩；由**加性可选键** `coverage_index` 引用数据集级
`coverage.index.json`，缺失时由产品级索引现场倒排），不逐瓦片探测。输入合同
**不设** `storage_form` 键（Phase2 产物固定裸形态），出现即 REJECT。

**索引边界**：索引给出块 → 候选帧集合与覆盖分数，只用于剪枝与调度；像素级裁决
仍由 support / validity / 排异语义执行；**有效性来源 = support / validity /
排异语义本身**。

输出面：帧 / 区域重叠图、有效面积、信息量、连通分量、coverage 产品。

### 数值落地口径

重叠图的定义、推导与适用域正本 = ALG-COV-001 与 docs/detail/mosaic/pipeline.md；
本页只记落地方式与适用域：

- 重叠图：帧间球面交叠（几何有效域交集），以 MOC 表达（NESTED）；
- 记录有效面积（球面交叠面积积分，单位 deg² / sr）与信息量（可推导到
  `point_information` 的域面；该量的定义与推导正本 =
  `docs/science/noise_snr/NOISE_SNR.md`对应章节，模块侧口径见
  registry/acsd.phase1.noise-snr.md）；
- 连通分量：在几何有效域上按球面邻接求连通分量，互不相连的分量一律分组件输出，
  **不**按同一零点 / 背景基准合并；分量划分由 `connected_components` 键控制；
- **假设与适用域**：输入帧的同一零点 / 背景基准只在**连通分量内**成立；几何
  有效域不重叠的帧不构成同一分量（域外不做外推）。

## 公共 header、核心 symbol 与生命周期

生产符号（SRC-COV-001，2 导出 + 2 内部链接）：`p2_coverage_build`（唯一生产
入口）/ `p2_coverage_free`（POD memset 清零，不释放堆）/ `parse_props` /
`inspect_frame`；类型 `P2MocCell` / `P2HipsInputInfo` / `P2CoverageResult`
（coverage.h）。模块注册 = `lib/infrastructure/pipeline/module_ports.registry.json`
的 `acsd.phase2.coverage`。

entrypoint = Phase1 产品组 → 重叠图 + coverage；生产接线 =
lib/infrastructure/scheduler/src/module_adapters.cpp 的 `p2_op_coverage`。输出可被
sampling / upm / integration 消费。

### Production callers

生产调用 = lib/phase2_session/p2_session.cpp 的 coverage 阶段（两阶段调用，
manifest 登记）。

### 源文件

`lib/algorithms/coverage/src/coverage.cpp`。层级 = Phase2 生产模块（DAG 首节点）；
现状构建 = 根 CMakeLists 目标 `acsd_phase2` STATIC（无独立 DLL target）；
`lib/algorithms/coverage/CMakeLists.txt` 的 phase2 STATIC 为模块自测
compatibility target（非产品事实源）。

## Registry descriptor 与配置 schema

配置字段：

| 字段 | 默认 | 单位 | 说明 |
|---|---|---|---|
| `min_overlap` | —— | deg² | 最小有效重叠 |
| `connected_components` | true | —— | 是否分解连通分量 |

## Execution class、并行轴、ThreadBudget lease、确定性

reentrant=yes / threadsafe=no（独立对象）/ internal_parallel = none（单线程整数
集合运算）。determinism = `fixed_reduction_order`（bitwise 确定）。ThreadLease /
取消检查点未接线 —— 阶段级取消由 session 阶段边界提供（p2_session.cpp 的检查在
其内）；该接线尚未落地。

## 内存/cache/I-O/所有权

`p2_coverage_free` 以 POD memset 清零，不释放堆（调用方持有输出 POD）。

## 错误、日志、指标、取消和 checkpoint

rc：0 = 成功（含 K = 0）/ 1 = 失败 + `error[512]` 载因；status 与 rc 同步
（"no inputs" 分支例外，缺陷登记 = ALG-COV-001）。编排映射
`ACS_ERR_PARAM` / `ACS_ERR_STATE`（API-P2-001）。

- 输入互不兼容（不同 frame / 滤镜 / 单位）→ 拒绝；
- 断图 → 输出分组件，不假装同一基准；
- coverage 缺失的输入 → fail-closed；
- 错误码与退出码唯一源 = lib/infrastructure/cli/exit_codes.h（本页不复制数值表）。

## 独立 synthetic 验证命令与容差

测试设计 = `TEST-COV-DESIGN-001`（PHASE2_COVERAGE.md 对应章节，冻结容差 = 整数 /
bitwise 断言，零数值容差）；可执行 `TEST-P2-COV-001` 待建。gate
`Phase2Coverage.RealHipsUnion` / `FilterMismatchRejected`（均 synthetic_gate.cpp）
依赖本地大数据路径 GTEST_SKIP，合成 fixture 待建。

Oracle 面：

- 构造已知重叠几何 → 覆盖面积 / 连通分量符合解析；
- 断图检测；
- **coverage 不作为权重**的负例测试。

## 已知限制

缺陷登记 = ALG-COV-001 的缺陷登记节："no inputs" status 不一致 /
frame_id 基名截断 / 空 filter 静默放行 / intersection / depth / missing-tiles
产品缺失（四语义仅 union 落地；覆盖度几何非 UPM geometric_reliability 权重因子，
该乘数恒 1.0，修正归 P2-UPM 域）/ extern "C" include + 两阶段全量重扫 / ThreadLease。

目标交付形态 acsd_p2_coverage.dll 尚未落地。全局限制登记 =
artifacts/evidence/known-limitations-ledger/LIMITATIONS.md。
