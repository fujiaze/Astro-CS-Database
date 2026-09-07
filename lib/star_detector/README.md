# AstroCS P1 Star Detection 模块（astrocs.p1.star_detection）— 冻结合同 README

> r1（P1-STAR-DOC，2026-09-07）：由 SRC-STAR-001 源码实测冻结，不信任旧 README
> （旧版 V5.0 性能叙事/匹配率表格为过程记录，归本目录 memory.md，
> ARCHIVED_NON_NORMATIVE）。
> 状态 **CONTRACT_READY**：实现与 C API 已存在于 legacy `lib/star_detector`
> （SRC-STAR-001 VERIFIED），独立模块化迁移（`astrocs_p1_star_detection.dll`、
> C ABI adapter、ThreadLease）归 **P1-STAR-IMPL**，可执行测试归 **P1-STAR-TEST**
> （TEST-STAR-DESIGN-001 → TEST-P1-STAR-001），descriptor 对齐归 **P1-STAR-INT**。
> 本文件为模块唯一合同入口。

## 0. 标识

| 项 | 值 | 依据 |
|---|---|---|
| MOD ID | `MOD-astrocs-phase1-star` | registry（matrix 行键） |
| module_id | `astrocs.p1.star_detection` | MODULE_MIGRATION_MATRIX P1-STAR 行 |
| DLL target | `astrocs_p1_star_detection.dll`（合同值，尚未存在） | 同上；现状 `star_detector.dll`（Makefile:39） |
| module/ABI revision | module_version 0.11.0-alpha.1 / abi_version 1 | module.yaml |
| owner | SA-P1-S15 | matrix 行 |
| 状态 | CONTRACT_READY（未 IMPLEMENTED） | 本任务冻结 |
| 遗留路径 | `lib/star_detector;lib/phase1/stars`（matrix legacy_paths） | 本目录即生产源 |
| 上游集成依赖 | IO-003;DATA-004;RT-006（matrix depends_on_int） | matrix 行 |

## 1. 负责与不负责

**负责**：Phase1 单帧 light 上的权威星点检测——peaker 七步候选（11×11 局部
极大/3×3 meanhigh/零交叉 Sr,Sc/振幅 Ar,Ac/盒半径 R/对称门/候选去重）+ Moffat4
（GSL trust-region LM，7 参数 Gaussian 参数化）逐候选拟合 + 饱和星（edge-walking
中心、A>dynrange 标记）+ mag 排序去重截断；输出十数组
`(x,y,flux,saturated,mag,has_saturated[,extras])`（DATA-P1-STAR）；FP32/FP64
双通道（DISP-STAR-001）；一帧一次权威检测原则（API-P1-003）。

**不负责**：PSF 批拟合/θ 消歧（dynamic_psf，DATA-P1-PSF）；plate solve 与
WCS（ipv/gaia_client，消费 star_det 块禁止重检测 orchestrator.cpp:1748-1755）；
最终测光（P1-PHOT）；背景/cosmetic 校正（P1-COSMETIC，本模块消费 cleaned 帧）；
`lib/phase1/stars`（P1-003 桥接层独立 sigma-clip 背景+阈值实现，§10 如实差距）。

## 2. 输入 / 输出 ports、DATA ID、单位、dtype、shape、invalid

权威=DATA-P1-STAR（docs/contracts/DATA_SEMANTICS.md §17）；下表为实现层摘要：

| 端口 | DATA ID | 方向 | 必/可 | 单位 | dtype/shape | invalid |
|---|---|---|---|---|---|---|
| image | DATA-P1-COSMETIC | 入 | 必 | ADU | FP32 通道 uint16（float→uint16 clamp [0,65535]，DISP-STAR-001）；FP64 通道 double `[h·w]` 行主序 | NULL/h≤0/w≤0 → −1（sdet_api.cpp:1612、:2325、:2340） |
| params | SDetParams | 入 | 可 | px/mag 无量纲混合 | 结构体 9 字段（star_detector.h:13-24） | NULL→默认（:963-975）；消费面缺口=DISP-STAR-003 |
| x/y | DATA-P1-STAR | 出 | 必 | pixel（0-based，像素中心=索引+0.5） | double `[n]` | n=0→全 NULL rc=0（:2252-2263） |
| flux | DATA-P1-STAR | 出 | 必 | ADU（正常星=振幅 A） | float `[n]` | 饱和拟合失败=0.0f 哨兵 |
| mag | DATA-P1-STAR | 出 | 必 | mag | float `[n]` | box_sum≤0/拟合失败=NaN；NaN 恒排末尾 |
| saturated/has_saturated | DATA-P1-STAR | 出 | 必 | 0/1 | int `[n]` | saturated=(A>dynrange)（:2159）；has_saturated≡saturated（DISP-STAR-004） |
| extras | DATA-P1-STAR | 出 | 可 | 各列定义 | float `[n]` | 生产传 nullptr/0 |
| star_det 块（编排序列化） | DATA-P1-STAR | 出 | 必 | 同上 | FLOAT64 `[N,6]` 列 x,y,flux,mag,saturated,has_saturated（orchestrator.cpp:2237-2246）+ FLOAT32 `[N,4]` 兼容视图 | 写块失败→阶段失败（:2242-2247） |

## 3. SCI / ALG / DATA / API / ARCH / TEST 链接

| 层 | ID | 状态 | 文档 |
|---|---|---|---|
| SCI | SCI-P1-STAR-001（共享 SCI 引用不改动） | FROZEN（本任务） | docs/science/STAR_DETECTION.md |
| ALG | ALG-STARDET-001 | FROZEN（本任务） | docs/algorithms/STAR_DETECTION_ALGORITHMS.md#§11 |
| DATA | DATA-P1-STAR | VERIFIED | docs/contracts/DATA_SEMANTICS.md#§17 |
| API | API-STAR-001（编排占位 API-P1-003 仍有效） | VERIFIED | docs/contracts/PUBLIC_API.md#API-STAR-001；docs/api/PHASE1_API_V1.md |
| ARCH | ARCH-001 | VERIFIED | docs/architecture/ARCHITECTURE.md |
| SRC | SRC-STAR-001 | VERIFIED | lib/star_detector/src/sdet_api.cpp（本 README 全部行号锚） |
| TEST | TEST-STAR-DESIGN-001 | FROZEN（设计） | docs/algorithms/STAR_DETECTION_ALGORITHMS.md#§11.4 |
| EVIDENCE | EVID-MISSING | MISSING | 待 P1-STAR-TEST |

矩阵现值 descriptor 占位词汇（module_adapters.cpp:430-448，module_id=
astrocs.phase1.star-psf）由 P1-PSF-INT/P1-STAR-INT 对齐本合同，不得反向作为
冻结依据。

## 4. module.yaml 与 standards

见本目录 `module.yaml`（schema `astrocs.module-manifest/v1`，字段遵循
11_MODULE_SOURCE_TEST_STANDARD.md §4；必填项无删减，未接项显式 `MISSING`；
entrypoint=MISSING）。

## 5. public entry 与实际主要 source symbols（sdet_api.cpp 实测行号）

C API（9 导出，头 lib/star_detector/include/star_detector.h:1-73）：

| symbol | 头行 | 定义行 | 语义摘要 |
|---|---|---|---|
| `sdet_create` | star_detector.h:33 | sdet_api.cpp:954 | handle 创建；NULL→默认参数（:963-975）；生产实参 orchestrator.cpp:1593-1612 |
| `sdet_destroy` | :34 | :984 | 唯一释放对 |
| `sdet_detect` | :36-40 | :992 | 旧 uint16 入口（仅 x/y；内部旧 CC 路径，非生产，DISP-STAR-005） |
| `sdet_free_coords` | :42 | :1276 | sdet_detect 专用释放 |
| `sdet_detect_debug` | :44-50 | :1281 | 诊断入口（CC 路径+平滑图导出+extras） |
| `sdet_free_debug_maps` | :52 | :1595 | debug 图专用释放 |
| `sdet_detect_ex` | :54-62 | :2318 | 生产 FP32 入口（uint16→float，impl<float>） |
| `sdet_detect_ex_f64` | :67-75 | :2343 | 生产 FP64 入口（impl<double>，全程不降级，PREC-105） |
| `sdet_free_detect_ex` | :77-79 | :2357 | 十数组唯一释放（extras 同组） |

内部核心（static/template，同文件）：`sdet_detect_impl<T>`（:1599-2353，
float/double 双实例生产核心）、`sdet_compute_bgnoise`（:440-476，FnNoise1
行差分+3×5σ clip）、peaker 七步主扫描（:1709-1974，star_finder.c 族对齐
注释 :1653-1660）、`sdet_moffat4_fit`（:483-620，采样/饱和 mask/bkg0 截尾
MAD/halfA 初始化）、`sdet_lm_fit`（:262-437，GSL TR-LM 7 参数
`{B,A,x0,y0,SX,fr,alpha}`，:348-352 trs=LM）、`reject_star`（:189-239，
SfError 五码 :177-186）、`sdet_dedup_stars`（:822-939）、`sdet_sort_stars`
（:941-956）、`edge_walking_center`（:627-674）/`sdet_detect_saturated_stars`
（:675-775，debug 路径）、`get_extra_field`/`parse_extra_name`（:778-819）。

关键常量与语义（冻结，ALG-STARDET-001 §2 逐条公式锚）：
- 平滑 σ=2.0（YvV IIR，:1623-1633）；阈值=median+5·bgnoise（:1645）。
- `SQRT_EXP1=√e`（:1674）、`s_factor=√(−2·ln 0.001)=3.7172`（:1678）、
  `MAX_BOX_RADIUS=200`（:1679）、norm=65535（:1689）、locthreshold=5·bgnoise
  （:1693）。
- R=max(ceil(3.7172·Sr),ceil(3.7172·Sc),r)（:1920-1931）；对称门 dA/dSr/dSc≤2
  （:1933-1945）；候选曼哈顿去重 0.2·R（:1947-1959）。
- mag 正常星=−2.5·log10(Σ_box(pixel−B_fit))（:2177-2198）；饱和星=−2.5·log10(A)
  （:2175）；is_saturated=(A>dynrange)（:2159）。
- dedup：饱和保、正常星 d²≤1.0（:900-926）；sort mag 升序 NaN 末尾（:941-956）；
  maxStars 截断（:2240-2242）。

## 6. config schema / default / 错误码

`SDetParams` 9 字段（star_detector.h:13-24）默认值（sdet_api.cpp:963-975）：
structureLayers=5 / hotPixelFilterRadius=1 / iterativeClipSigma=9.0f /
iterativeMaxRounds=5 / medianFilterDetail=1 / maxStars=2000 / fitRadius=6 /
fwhmClipSigma=3.0f / maxAxisRatio=2.0f。生产消费面：maxStars/maxAxisRatio
完整消费；fitRadius 仅驱动 auto 半径日志推导（:2024-2026，实际用 per-candidate
R）；fwhmClipSigma 仅 debug 入口（:1450-1452）；其余 5 字段仅旧 CC 路径
（sdet_detector.cpp:14-56）——DISP-STAR-003 登记不改码。

错误/返回码（冻结）：

| 码 | 层 | 触发 | 输出副作用 |
|---|---|---|---|
| 0 | 入口 rc | 成功（含 0 星空场，:2252-2263） | 数组 malloc；空场全 NULL |
| −1 | 入口 rc | handle/image/输出指针 NULL、h/w≤0（:1612、:2325、:2340）、malloc 失败（:2264-2270） | 部分数组可能为 NULL |
| SDET_FIT_OK/INVALID_PARAMS/NO_CONVERGENCE | 拟合 | sdet_api.cpp:260-262/:425-429/:597-609 | 非 OK 候选丢弃（:2139），不出 NaN 行 |
| SF_OK/SF_FWHM_NEG/SF_FWHM_TOO_SMALL/SF_ROUNDNESS_BELOW_CRIT/SF_RMSE_TOO_LARGE/SF_FWHM_TOO_LARGE | 质量门 | :177-186 | 拒绝候选（:2148-2150）；饱和星豁免 RMSE（:203-206） |
| STAR_DETECT_FAILED | 编排退出码 | det_ret≠0 或 count≤0（orchestrator.cpp:2200-2212） | 阶段失败 |

## 7. threading / parallel axis / lease / memory / I/O / cancel / checkpoint

现状（登记不改码，DISP-STAR 见 ALG-STARDET-001 §11.3）：
- OpenMP 三处：行差分 `parallel for schedule(static)`（:448）、入口 uint16→float
  转换（:2321-2325）、候选拟合 `omp parallel + omp for schedule(dynamic)
  reduction(+:fit_ok_count)`（:2042-2044）；逐候选独立、结果按索引写回；
  dedup/sort/maxStars 截断串行（:2224-2242）→ 输出 bitwise 与线程数无关
  （determinism=fixed_reduction_order）。
- 无取消检查点；无 checkpoint；日志 sdet_log 默认阈值（lib/star_detector/logs/）。
- 内存：输出十数组模块 malloc（唯一释放 `sdet_free_detect_ex` :2357-2373）；
  smooth/candidates/fit_results 为函数内 `std::vector` 局部分配（O(N)+O(C)）。
- 目标合同：`threading_model=host_executor_lease`（11 号标准 §4）；ThreadBudget
  接线与取消检查点归 P1-STAR-IMPL。

## 8. provider 能力与 fallback

`cpu_providers: [baseline]`（无 ISA 特化路径；`-march=native -O3` 为构建配置
Makefile:20-26，非 provider 选择）；GSL gsl_multifit_nlinear 链接（Makefile
LDLIBS）。fallback：无（baseline 单通道）。

## 9. oracle / property / boundary / performance / 容差来源（测试设计）

TEST-STAR-DESIGN-001（STAR_DETECTION_ALGORITHMS.md §11.4，P1-STAR-TEST 执行，
容差冻结不得放宽）：
- **oracle**：合成高斯星场回收中心/流量/FWHM（F1 |Δc|≤0.3px@SNR≥20、FWHM
  相对误差≤10%）；FP64 通道独立 Moffat4 复算（F4 |Δc|≤0.05px、A/B≤1e−3）；
  FP32 通道 uint16 量化容差独立冻结（F4 |Δc|≤0.5px）。
- **property**：mag 升序全序+NaN 末尾；饱和优先 dedup；输出 bitwise 与线程数
  无关（F3 线程 1/2/4）；maxStars 截断保最亮。
- **boundary**：候选为空 rc=0；帧边界 2px 丢弃；R 收缩不出帧；box 钳 [5,200]。
- **negative**：NULL/空图/0 尺寸 → −1（F5）；非有限输入。
- **degenerate/saturated**：饱和平台≥3px 检出且 saturated=1；饱和+正常 d<2px
  保饱和星（F2）。
- **performance**：批 1/N worker 缩放（OpenMP 3 处）、provider=baseline 固定；
  现状参考值（旧 README V5.0 叙事，非合同）：16 线程 4500×3600 银心 ~9s。
- **容差来源**：fixtures generator 注记 + 11 号标准 §5（seed/commit/hash/
  tolerance source），不来自本 README 手抄数值。

## 10. 构建 / 测试命令、已知限制、未实现项

- 构建（现状）：`make -C lib/star_detector` → `star_detector.dll`
  （Makefile:39，`g++ -shared -fopenmp -O3 -march=native -std=c++17`，lto/pch
  目标可选）。未编入根 CMake 主构建；根 CMakeLists.txt:441 另有
  `astrocs_phase1_stars` STATIC 库（lib/phase1/stars P1-003 桥接层，
  独立 sigma-clip 背景+3σ 阈值实现，与 sdet_api.cpp 非同一算法路径——
  matrix legacy_paths 第二路径，如实差距，整合归 P1-STAR-IMPL）。
- 生产加载：orchestrator.cpp:1539-1549（`lib/star_detector/star_detector.dll`
  显式加载，失败即错）；生产消费 `sdet_create`（:1593-1612）、
  `sdet_detect_ex`/`sdet_detect_ex_f64`（:2149-2198）、`sdet_free_detect_ex`
  （:2204-2213、:2466）。
- 测试：lib/star_detector/test/sdet_fp64_test.cpp（NON_PRODUCTION_TOOL_ONLY
  手工合成星图对比程序，非共址测试套件）；共址测试建立归 P1-STAR-TEST。
- **已知限制（如实登记，DISP-STAR-001..005 + 线程/取消，ALG-STARDET-001
  §11.3，整改归 P1-STAR-IMPL/INT）**：① FP32 通道 uint16 量化；
  ② 全局单阈值无局部背景自适应；③ SDetParams 9 字段消费面缺口；
  ④ 饱和星 mag 量纲不一致 + has_saturated 列未分化；⑤ 双实现并存（生产
  peaker 路径 vs 旧 CC 路径 :992-1274/:1281-1593）；⑥ ThreadBudget 未接线、
  无取消检查点。
- **未实现（MISSING，禁止宣称 IMPLEMENTED）**：`astrocs_p1_star_detection.dll`
  模块壳、C ABI adapter、plan-execute-cancel-inspect、ThreadLease、共址测试、
  EVIDENCE 证据链、registry 入口（entrypoint=MISSING）。

## 11. 历史与记忆

旧版 README（V5.0 性能叙事、匹配率表格、目录结构图）为过程记录，其中「有序
输出：饱和星在前按 r 降序，正常星按 flux 降序」等排序描述与现状实现不符
（实际 mag 升序+NaN 末尾+饱和星优先去重保留，:822-956），以本 README r1 为准；
历史细节归本目录 `memory.md`（ARCHIVED_NON_NORMATIVE）。
