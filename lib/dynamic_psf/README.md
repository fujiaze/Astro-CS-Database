# AstroCS P1 PSF 模块（astrocs.p1.psf）— 冻结合同 README

> r1（P1-PSF-DOC，2026-09-07）：由 SRC-PSF-001 源码实测冻结，不信任旧 README。
> r2（PSF-001，2026-09-10）：批 ABI 尺寸边界修复（DISP-PSF-007 候选收口）后
> 行号锚全量复测刷新（§1/§5/§6/§7/§10）；§6 新增 5 批入口统一尺寸守卫
> `dpsf_batch_dims_check`（w/h≤0 与 w*h>INT_MAX → 整体 -1 零输出触碰）与
> 分配失败异常屏障语义；科学公式、默认容差、状态码语义零改动。
> 状态 **CONTRACT_READY**：实现与 C API 已存在于 legacy `lib/dynamic_psf`（SRC-PSF-001
> VERIFIED），独立模块化迁移（`astrocs_p1_psf.dll`、C ABI adapter、ThreadLease）归
> **P1-PSF-IMPL**，可执行测试归 **P1-PSF-TEST**（TEST-PSF-DESIGN-001 → TEST-P1-PSF-001）。
> 本文件为模块唯一合同入口；registry descriptor（lib/core/src/module_adapters.cpp:430-448）
> 现为占位 ID，以本 README + module.yaml 为冻结依据，由 P1-PSF-INT 对齐。

## 0. 标识

| 项 | 值 | 依据 |
|---|---|---|
| MOD ID | `MOD-astrocs-phase1-star-psf` | registry（matrix 行键） |
| module_id | `astrocs.p1.psf` | MODULE_MIGRATION_MATRIX P1-PSF 行 |
| DLL target | `astrocs_p1_psf.dll`（合同值，尚未存在） | 同上；现状 `dynamic_psf.dll`（dll_loader.cpp:39） |
| module/ABI revision | module_version 0.11.0-alpha.2 / abi_version 1 | module.yaml |
| owner | SA-P1-S15 | matrix 行 |
| 状态 | CONTRACT_READY（未 IMPLEMENTED） | 本任务冻结 |
| 遗留路径 | `lib/dynamic_psf`（matrix legacy_paths） | 本目录即生产源 |
| 上游集成依赖 | P1-STAR-INT（matrix depends_on_int） | matrix 行 |

## 1. 负责与不负责

**负责**：Phase1 单帧 light 上按权威星表坐标逐星做 Moffat4（β=4 固定）PSF 拟合，
产出 `psf` 块（DATA-P1-PSF）；uint16/float32/float64 三通道拟合 API；
`star_det_v1:FLOAT64[N,6]` 检测消费契约（dynamic_psf.h:104）。

**不负责**：星检测（star_detector，PSF 阶段消费其检测结果，禁止重检测，
orchestrator.cpp:1754-1756 注释）；饱和剔除决策（star_det v1 列 [4]/[5] 现状不
消费，dpsf_psf.cpp:847-848 仅解包 [0]=x/[1]=y）；下游测光/零点（P1-PHOT）；background
模型与 cosmetics（P1-COSMETIC，本模块仅消费 `cleaned` 块）。

## 2. 输入 / 输出 ports、DATA ID、单位、dtype、shape、invalid

registry descriptor ports（module_adapters.cpp:437-441）：

| 端口 | DATA ID | 方向 | 必/可 | 单位 | 坐标 | dtype/shape（实现层） | invalid |
|---|---|---|---|---|---|---|---|
| `cleaned` | DATA-P1-COSMETIC | 入 | 必 | ADU | PIXEL | uint16/float32/float64 `[height,width]` 行主序（dpsf_fit/dpsf_fit_batch_f32/dpsf_fit_batch_d） | 空指针/`width<=0`/`height<=0` → DPSF_FIT_INVALID_PARAMS 或批 API -1 |
| `sources` | DATA-P1-SOURCES | 入 | 可 | DIMENSIONLESS | ICRS | star_det v1：`FLOAT64[N,6]`，列 `[0]=x_px [1]=y_px [2]=flux [3]=mag [4]=saturated [5]=has_saturated`（dynamic_psf.h:79-82,104） | `n_detections<=0`/空指针 → -1；[4]/[5] 不消费 |
| `psf` | DATA-P1-PSF | 出 | 可 | DIMENSIONLESS（θ 为弧度，FWHM 为像素） | PIXEL | `psf_params:FLOAT64[N,9]`：`[0]=B [1]=A [2]=cx [3]=cy [4]=sx [5]=sy [6]=theta [7]=fwhm_x [8]=fwhm_y`（dynamic_psf.h:105） | 拟合失败星 9 字段全 NaN；批 API `out_n_valid` 仅计 DPSF_FIT_OK |

坐标：输出 cx/cy 为图像像素坐标（局部 patch 坐标已平移回全图，dpsf_psf.cpp:415、
641、766、887-888、1025-1026）；θ 为弧度，起边 x 轴，sx≥sy 约定经 θ 候选消歧保持（见 §5）。

## 3. SCI / ALG / DATA / API / ARCH / TEST 链接

| 层 | ID | 状态 | 文档 |
|---|---|---|---|
| SCI | SCI-P1-PSF-001（文档合同 SCI-PSF-001） | MISSING→本任务冻结（见 §10） | docs/science/PSF.md；docs/algorithms/STAR_PSF_ALGORITHMS.md §11 |
| ALG | ALG-STARPSF-001（descriptor 占位 ALG-002 同文档） | MISSING→本任务冻结 | docs/algorithms/STAR_PSF_ALGORITHMS.md#§11 |
| DATA | DATA-P1-PSF（descriptor 占位 DATA-P1-SOURCES 为编排层误配） | VERIFIED | docs/contracts/DATA_SEMANTICS.md#§15 |
| API | API-PSF-001（descriptor 占位 API-P1-003 仍有效） | VERIFIED | docs/contracts/PUBLIC_API.md#API-PSF-001；docs/api/PHASE1_API_V1.md |
| ARCH | ARCH-001 | VERIFIED | docs/architecture/ARCHITECTURE.md |
| SRC | SRC-PSF-001 | VERIFIED | lib/dynamic_psf/src/dpsf_psf.cpp（本 README 全部行号锚） |
| TEST | TEST-PSF-DESIGN-001 | VERIFIED（设计） | docs/algorithms/STAR_PSF_ALGORITHMS.md#§11.4 |
| EVIDENCE | EVID-MISSING | MISSING | 待 P1-PSF-TEST |

矩阵现值 `TEST-P1-PSF-001`/`DATA-P1-SOURCES`/`ALG-002`（module_adapters.cpp:442-446
占位）由 P1-PSF-INT 对齐本合同，不得反向作为冻结依据。

## 4. module.yaml 与 standards

见本目录 `module.yaml`（schema `astrocs.module-manifest/v1`，字段遵循
11_MODULE_SOURCE_TEST_STANDARD.md §4；必填项无删减，未接项显式 `MISSING`）。

## 5. public entry 与实际主要 source symbols（dpsf_psf.cpp 实测行号）

C API（7 导出，头 lib/dynamic_psf/include/dynamic_psf.h）：

| symbol | 头行 | 定义行 | 语义摘要 |
|---|---|---|---|
| `dpsf_fit` | dynamic_psf.h:44 | dpsf_psf.cpp:459 | uint16 单星拟合；空 rect→INVALID_PARAMS（:477-482） |
| `dpsf_fit_batch` | :49 | dpsf_psf.cpp:534 | uint16 批量，逐星裁 float patch |
| `dpsf_fit_batch_f` | :59 | dpsf_psf.cpp:661 | float32 图 + (cx[],cy[]) → DPSFFitResult*[] |
| `dpsf_free_results` | :64 | dpsf_psf.cpp:681 | 释放批量结果 |
| `dpsf_fit_batch_f32` | :107 | dpsf_psf.cpp:792 | float32 图 + star_det v1 [N,6] → [N,9]（成功行 compact）+ 可选 out_status[N] |
| `dpsf_fit_batch_f64` | :142 | dpsf_psf.cpp:937 | float64 图 + star_det v1 → [N,9]（成功行 compact；moffat4_fit_d，不降级）+ 可选 out_status[N] |
| `dpsf_fit_batch_d` | :181 | dpsf_psf.cpp:694 | float64 图 + (cx[],cy[]) → DPSFFitResult*[] |

内部核心（static，同文件）：`dpsf_batch_dims_check`（:514，PSF-001 批 ABI 尺寸
守卫：w/h≤0 确定性拒绝 + w*h>INT_MAX 寻址上界，双维超界→调用方 -1）、
`fit_batch_float_image`（:575，float32 批量核心，dpsf_fit_batch 转换后与
dpsf_fit_batch_f 共用）、`moffat4_fit_tmpl`（:238，ImageT=float→`moffat4_fit`
:441，double→`moffat4_fit_d` :451 薄封装）、`lm_solve`（:105，LM 数值雅可比 +
`gauss_solve` 高斯消元 :34，λ 初值 1e-3 :113，成功/失败 ×0.1/×10 :181/:183）、
`compute_trimmed_mad`（:203，10%–90% 截尾 MAD :226-227）、moffat4 残差（:73）。

关键常量与语义（冻结）：
- `MOFFAT4_FWHM_FACTOR=1.230310`（:25，β=4 解析 FWHM 系数）；`NPARAMS=7`（:26）。
- 参数向量序 `params[0..6] = B,A,x0,y0,sx,sy,theta`（:74-75）。
- 初值链：`bkg0`=截尾样本中位背景（:332）、`A0=max−bkg0`（:340）、
  `params={bkg0,A0,0,0,sx0,sx0,0}`（:347）；LM 调用容差 1e-8 / max_iter=200
  硬编码（:352-353，`DPSFFitParams.maxIter/tolerance` 字段死参数，DISP-PSF-003）。
- θ 消歧：`thetas[4]={θ, π/2−θ, π/2+θ, π−θ}`（:389）逐候选取 trimmed MAD 最小
  （:391-401），非重拟；保证 (sx,sy,θ) 参数化对称简并的确定性回选。
- `fwhm_x/y = MOFFAT4_FWHM_FACTOR·sx/sy`（:371-372）；
  `flux = 2π·A·sx·sy/3`（β=4 Moffat 解析积分 ∫=2πAα²/(β−1)，:406-407）；
  `eccentricity=√(1−(sx_min/sx_max)²)`（:410）；`img_cx=cx+x0`（:415）。
- 步后硬钳位 `sx,sy≥0.3`、`A≥0`（:176-178，DISP-PSF-002）。

## 6. config schema / default / 错误码

`DPSFFitParams{fitRadius, maxIter, tolerance}`（dynamic_psf.h:38-42）：批 API
`params=NULL` 用默认 fitRadius=8/maxIter=200/tolerance=1e-8（:819-821、:963-965）；
仅 `fitRadius` 实际参与裁窗，`maxIter/tolerance` 不被消费（LM 硬编码 1e-8/200，
:352-353——DISP-PSF-003 登记）。

`DPSFFitResult` 12 字段 `{B,A,cx,cy,sx,sy,theta,fwhm_x,fwhm_y,mad,flux,eccentricity}`
（dynamic_psf.h:17-31）。

错误码（dynamic_psf.h:33-36，冻结语义，P1-PSF-TEST 逐码负例）：

| 码 | 宏 | 触发 | 输出副作用 |
|---|---|---|---|
| 0 | DPSF_FIT_OK | ‖Δx‖<tol·(‖x‖+1e-30)（:164）且通过 §5 验证链 | 全参数有效 |
| 1 | DPSF_FIT_NO_CONVERGENCE | 非有限/A≤0/sx≤0.3/sy≤0.3（:363-366）；FWHM>rect（:374-380）；背景约束 \|B−bkg0\|/max(bkg0,0.01)>0.5（:381-385） | result 已 memset 0（:243），status 置码；批接口该星不计 valid |
| 2 | DPSF_FIT_INVALID_PARAMS | 空指针/w≤0/h≤0（:463）；rect 面积<9（:249-252）；rect 越界（:253-258）；空 rect（:477-482） | 单星返回码；批 API 整体 -1（:514-527 尺寸守卫 + :805-813） |
| 3 | DPSF_FIT_ITERATION_LIMIT | max_iter=200 耗尽（:188） | 单星接口仍回填当前最优参数（:423-436） |

批接口：5 批入口（batch/batch_f/batch_d/batch_f32/batch_f64）共享
`dpsf_batch_dims_check`（:514-527，PSF-001）：w/h≤0（0/-1/INT_MIN）与
w*h>INT_MAX（int 索引寻址上界）→ 确定整体 -1，零整图分配、零输出触碰；
uint16 入口另套分配失败 try/catch 屏障（:555-576，bad_alloc→-1 不外抛跨
C ABI），逐星 patch 分配失败星级隔离为 INVALID_PARAMS/NaN 占位（不外抛）。
`dpsf_fit_batch_f32/f64` **B2-A2（RESCUE-P0-05）**：OK 星写 9 字段
（:893-901、:1030-1038），随后按检测下标升序**顺序 compact** 到
`out_psf_params` 第 0..n_valid−1 行；失败星不占参数行（不再留下 NaN 洞），
其真值经可选 `out_status`（`psf_status:INT32[N]`，dynamic_psf.h:114-120）
按检测下标报告（0=OK/1=拟合失败/2=空 rect/3=分配失败）；`out_n_valid` 只数
`DPSF_FIT_OK`（:919、:1053）。调用方必须用 out_status 做星 ID↔行映射，
**禁止 `i < n_valid` 前缀截断**。`gauss_solve` 奇异→λ×10 重试（:151-154）。
N*9 以 int64 计（入口 n ≤ INT_MAX/9 拒绝，:810/:955）。

## 7. threading / parallel axis / lease / memory / I/O / cancel / checkpoint

现状（登记不改码，DISP-PSF-001..006 见 ALG §11.3）：
- OpenMP `parallel for schedule(dynamic) reduction(+:success_count)` 4 处
  （dpsf_psf.cpp:593,718,842,986）；逐星独立、输出按索引写，无跨星共享可变状态，
  计数 reduction 与星序无关 → 结果确定（determinism=fixed_reduction_order）。
- 无取消检查点（DISP-PSF-004）；无 checkpoint；I/O 仅日志（dpsf_log，默认
  threshold LOG_WARN，2026-07-12 性能修复）。
- 内存：批接口 malloc `results`（调用者 `dpsf_free_results` 释放）/ 调用者预分配
  `out_psf_params`；每星 patch `std::vector` 局部分配（fitRadius² 量级）。
- 目标合同：`threading_model=host_executor_lease`（11 号标准 §4），ThreadLease
  接线与取消检查点归 P1-PSF-IMPL。

## 8. provider 能力与 fallback

`cpu_providers: [baseline]`（无 ISA 特化路径；`-march=native` 编译期向量化属
构建配置，非 provider 选择）。fallback：无（baseline 单通道）。

## 9. oracle / property / boundary / performance / 容差来源（测试设计）

TEST-PSF-DESIGN-001（STAR_PSF_ALGORITHMS.md §11.4，P1-PSF-TEST 执行）：
- **oracle**：解析 Moffat4（β=4）合成图上回收 7 参数（参数真值与容差由 fixtures
  生成器给出，禁止与生产共用同一公式实现）。
- **property**：`flux=2π·A·sx·sy/3` 解析积分恒等；`fwhm=1.230310·s`；θ 候选消歧
  确定性；eccentricity∈[0,1)；批输出 NaN 占位与 out_n_valid 一致性。
- **boundary**：fitRadius 裁到图边（:438-441 clamp）；rect 面积<9；FWHM>rect；
  背景约束 0.5 阈值边界；max_iter 耗尽路径（ITERATION_LIMIT 仍回填）。
- **negative**：空指针/空图/空 rect/rect 越界 → 错误码 §6 逐码断言；非有限输入。
- **degenerate/saturated**：θ 对称简并消歧（4 候选）；饱和列 [4]/[5] 不消费
  （现状冻结，饱和源直接参与拟合，剔除决策归上游）——P1-PSF-TEST 专项覆盖。
- **performance**：批 1/N worker 缩放（OpenMP 4 处）、provider=baseline 固定。
- **容差来源**：fixtures generator 注记 + 11 号标准 §5（测试元数据 seed/commit/
  hash/tolerance source），不来自本 README 手抄数值。

## 10. 构建 / 测试命令、已知限制、未实现项

- 构建（现状，Linux/MinGW 通道）：`make -C lib/dynamic_psf` → `dynamic_psf.dll`
  （Makefile:3-5，`g++ -shared -fopenmp -O2 -march=native -std=c++17`）。
  未编入根 CMake（根 CMakeLists.txt 无 dynamic_psf 目标）——迁移后 CMake 集成归
  P1-PSF-IMPL。生产加载：dll_loader.cpp:39,53（`lib/dynamic_psf/dynamic_psf.dll`），
  PSF 为必需 stage（orchestrator.cpp:2071-2075，DLL 未加载→退出码 2）；
  orchestrator 现消费 `dpsf_free_results`（:2290）、`dpsf_fit_batch_d`（:2304）、
  `dpsf_fit_batch_f`（:2327）。
- 测试：共址测试面 `tests/p1psf/`（P1-PSF-TEST 建立，root CMake 经
  tests/unit/CMakeLists.txt:984 接入；`ctest -R p1psf_`：units/properties/
  oracle/negative/boundary/performance/selfcheck，故障注入
  `ASTROCS_P1PSF_FAULT=<name>` 必败自检）。批 ABI 尺寸边界负例 = negative 组
  N4b/N4c（PSF-001：w/h∈{0,-1,INT_MIN} × 5 入口 + w*h>INT_MAX，注入名
  `batch_boundary`）。
- **已知限制（如实登记）**：① `DPSFFitParams.maxIter/tolerance` 死参数（DISP-PSF-003）；
  ② 前向差分雅可比（DISP-PSF-002 同族，精度与收敛半径受限）；③ 步后硬钳位
  （DISP-PSF-002）；④ 饱和列不消费；⑤ θ 消歧仅 4 候选对称集；⑥ 无参数协方差/
  不确定性输出（科学专项 "covariance" 为迁移整改项，P1-PSF-IMPL 落地，现状不宣称）；
  ⑦ 批 f32/f64 路径逐星退败静默（仅 n_valid 汇总，DISP-PSF-006）。
- **未实现（MISSING，禁止宣称 IMPLEMENTED）**：`astrocs_p1_psf.dll` 模块壳、C ABI
  adapter、plan-execute-cancel-inspect、ThreadLease、共址测试、EVIDENCE 证据链。

## 11. 历史与记忆

旧版 README（GitHub 仓库 a3ae0d6/v1.1 性能修复叙事）为过程记录，其中"7 参数 =
(amplitude,x0,y0,sigma_x,sigma_y,beta,background)"参数序描述与本实现不符
（实际序 B,A,x0,y0,sx,sy,theta，β 固定为 4 不可拟合），以本 README r1 为准；
历史细节归本目录 `memory.md`（ARCHIVED_NON_NORMATIVE）。
