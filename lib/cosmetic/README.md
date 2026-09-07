# lib/cosmetic — astrocs.p1.cosmetic（P1-COS）

> 状态: CONTRACT_READY（P1-COS-DOC 冻结，2026-09-07）｜doc revision: r1
> 本 README 由源码逐函数核对后新建（P1-COS-DOC，wave W1）：函数、单位、
> 坐标、dtype、shape、invalid、错误、并发、内存、I/O 均以现行唯一生产
> 实现 `lib/calibration/src/cosmetic_corrector.cpp` + `src/ac_api.cpp`
> （CMake `astrocs_calibration`，CMakeLists.txt:321-333）+ 唯一权威签名源
> `include/astro_calibration.h` 为准；`lib/cosmetic/` 是 P1-COS 迁移目标
> 目录（astrocs_p1_cosmetic.dll 落码由 P1-COS-IMPL 建立，当前本目录仅
> 合同文件、无源码）。权威合同：SCI-CAL-001 → ALG-COS-001..005 →
> DATA-P1-COS / API-COS-001（链接见 §4）。与 P1-CAL-DOC 冻结的
> lib/calibration 合同共存不重叠：本模块只冻结 cosmetic 路径
> （ac_correct_frame(+_f64) 域），master 生成/校准归 P1-CAL。

## 1. 身份

| 字段 | 当前值 |
|---|---|
| MOD ID / DLL target | `MOD-astrocs-phase1-cosmetic` / 现状实现编入 CMake 静态库 `astrocs_calibration`（无独立产物）；迁移目标 `astrocs_p1_cosmetic.dll`（P1-COS-IMPL 建立，尚未存在） |
| module / ABI / doc revision | `astrocs.p1.cosmetic` / C ABI（AC_API extern "C"，无版本化 query 入口，迁移缺口）/ r1 |
| owner / phase scope | SA-P1-COS / phase1（wave W1） |
| 文档状态 | CONTRACT_READY（实现存在于 lib/calibration，模块化迁移未开始；不声明 IMPLEMENTED） |
| 构建 | 现状随 CMakeLists.txt:321-333 `astrocs_calibration`（STATIC + OpenMP 可选）；独立目标由 P1-COS-IMPL 建立 |

## 2. 负责范围

负责：热像素检测（master dark 全局 median+hot_sigma·1.4826·MAD 阈值）、
冷像素检测（master bias 全局 median−cold_sigma·σ）、8 连通结构过滤
（≥max_size 连通域不判坏）、坏点插值修复（5×5 镜像边界中值 / 4 方向
1/dist IDW）、out_hot/out_cold 结构过滤后计数；数据语义
DATA-P1-COS（DATA_SEMANTICS §10）。

不负责：master bias/dark/flat 生成与单帧校准算术（P1-CAL /
astrocs.p1.calibration）；FITS/XISF 读写（astro_image_io，调用方侧）；
cosmetic 参数与母版的接线决策（调用方/编排层；现状 p1_session 未接线
母版）；噪声方差/SNR（snr_estimator）；天光背景扣除（Phase2）、宇宙线
剔除（叠加 rejection）；WCS/测光定标；线程池/ThreadLease 授予（现状
OpenMP 默认 team，迁移后由 host 授予）；整 Phase 行为（禁止）。

## 3. 输入与输出（DATA-P1-COS，DATA_SEMANTICS §10）

输入：全部内存数组，行主序 `idx=y·w+x` 0-based，单位 ADU；data 修复帧
`[h][w]` float32（f64 ABI 经 double→float 降级执行）；master_dark /
master_bias `[h][w]` 均**可 NULL**（NULL 或对应 sigma<=0 = 禁用该检测，
两者皆禁 → 模块恒等 pass）；hot/cold_sigma 无量纲（MAD 倍数，
1.4826·mad 换算）；method 0=median / 1=IDW（名义 bilinear，非 0 一律
IDW）；max_structure_size 像素个数（<=0 → 全域清除负面语义）；
out_hot/out_cold 可 NULL。

输出（单位/dtype/shape/invalid 全表见 DATA_SEMANTICS §10.2）：

- `out[h][w]` float32（f64 ABI 回转 double）：非坏点逐像素恒等；坏点=
  插值（空邻域回退原值，不虚构好值）；NaN 输入透传。
- `out_hot/out_cold` int：结构过滤后掩码像素计数（未接线母版时=0）。
- 错误码 int：0=AC_OK；-1=AC_ERR_PARAM（data/out 空指针、w/h 非正）；
  -2/-3 定义但从未返回（DISP-COS-001）。

坐标：无坐标变换、无 WCS（SCI-CAL-001 §3a，frame identity 不变）；
掩码极性 1=坏点（SCI-CAL-001 §9a）。

## 4. 合同链接

| 层 | ID | 权威文档 |
|---|---|---|
| SCI | SCI-CAL-001 | docs/science/CALIBRATION.md（FROZEN；与 P1-CAL 共享 SCI 层） |
| ALG | ALG-COS-001..005 | docs/algorithms/COSMETIC_ALGORITHMS.md（逐公式源码锚定） |
| DATA | DATA-P1-COS | docs/contracts/DATA_SEMANTICS.md §10（上游 DATA-P1-CAL §9） |
| API | API-COS-001 / API-P1-002 | docs/contracts/PUBLIC_API.md / docs/api/PHASE1_API_V1.md §2 |
| MOD/SRC | MOD-astrocs-phase1-cosmetic / SRC-COS-001 | docs/traceability/TRACEABILITY_MATRIX.json；实现源 cosmetic_corrector.cpp + ac_api.cpp（签名源 astro_calibration.h:97-103,142-148） |
| TEST | TEST-COS-DESIGN-001 | docs/algorithms/COSMETIC_ALGORITHMS.md §9（可执行 TEST-P1-COS-001 由 P1-COS-TEST 落地） |

## 5. 实现事实（源码核对）

- **算法**：检测 = 全局 median + 1.4826·MAD 阈值（不过滤 NaN，
  DISP-COS-002：NaN 源帧 → 检测静默全 false）→ 8 连通过滤（保留小域、
  剔除 ≥max_size 大域；背景 label 0 记 size=max_size 防误清）→ 修复
  （median：5×5 镜像反射邻域、仅收好像素、偶数样本双中位均值
  `(hi+lo)*0.5f` / IDW：4 正交方向步进跳坏点、1/dist 反比加权、
  空邻域回退原值）。合并 `all_bad = hot | cold`；计数 = 过滤后坏点数。
  逐公式源锚见 ALG-COS-001..005。
- **method=1（AC_METHOD_BILINEAR）实为 4 方向 IDW**（DISP-COS-003，
  冻结现状行为；修正需 SCI/控制包变更）。
- **FP64 ABI**：`ac_correct_frame_f64` 非真双精度——double 输入转
  float32 执行（统计/mask/插值全程 f32）、输出回转 double
  （ac_api.cpp:228-263；头文件 :105-115 声明；DISP-COS-004）。
- **生产调用现状（no fabrication of valid coverage）**：唯一生产调用点
  `lib/phase1_session/p1_session.cpp:294-307`（cosmetic stage，
  2026-09-01 c5629be6 引入）传 `master_dark=nullptr、master_bias=nullptr`
  → 两检测全禁用、out_hot=out_cold=0、帧逐像素复制回写——**生产
  cosmetic 阶段现状为恒等 pass，检测/修复从未在生产生效**
  （DISP-COS-009）；母版接线由 P1-COS-IMPL/编排层处理。
- **并发**：函数级 reentrant+threadsafe（无共享可变全局）；OpenMP
  `parallel for schedule(static)` 像素域并行（判定/合并/清零/插值），
  统计（median/MAD）与 BFS 标记串行；计数用 omp reduction(+)（整数加法
  可交换，结果确定）。输出 bitwise 与线程数无关。ac_set_num_threads
  进程级改写 OpenMP ICV（竞态+跨模块副作用，DISP-COS-008；迁移后
  host ThreadLease 取代）。
- **内存**：调用方分配 out；模块内 std::vector RAII；峰值额外内存
  O(n)：检测统计复制 ×2 + labels/sizes 向量（int×n ×2）+ f64 转接层
  全帧复制（DISP-COS-006）；插值每工作线程 O(25)。
- **I/O**：零文件/网络 I/O、零 stderr 日志（cosmetic 路径无 ac_log
  调用）；无日志文件、无 manifest。
- **取消**：无取消检查点（API-P1-002 §2 登记取消点=无；session 层
  帧粒度取消，DISP-COS-007）。

### 5.1 配置 schema（现状：C 参数直传 + phase1_session JSON）

- C 参数：`hot_sigma/cold_sigma`（float，MAD 倍数，<=0 禁用）、
  `method`（0=median，非 0=IDW）、`max_structure_size`（int）。
- phase1_session 键（p1_session.cpp:132-138 校验：cosmetic 必须 object、
  值必须 numeric/bool）：`enabled`（bool，默认 true）、`hot_sigma`
  （默认 5.0）、`cold_sigma`（默认 5.0）、`method`（字符串
  "median"/"bilinear" 映射 AC_METHOD_MEDIAN/BILINEAR，其他→median）、
  `max_structure_size`（默认 4）；无 master_dark/master_bias 键（即
  检测永远禁用——DISP-COS-009 根因）。实验参考
  lib/calibration/batch_config.json（5.0/5.0/median/4）。
- 正式版本化 schema 由 P1-COS-IMPL 冻结（acs_module_descriptor_v1.
  config_schema_ver）。

## 6. 并发与资源

资源分类：cpu_heavy（descriptor 迁移取值 execution_class=cpu_heavy、
parallel_ok=1；O(n) 逐像素，检测统计串行段占比随 n 增大而稀释）。
现状并行=OpenMP 默认 team（线程数=omp_get_max_threads，可被
ac_set_num_threads 进程级改写）；**不满足**约束 D.3/D.4 的 ThreadLease
模型——迁移整改点（与 DISP-CAL-002 同根，API-P1-001 §2 已登记）。
无内建 benchmark/ISA 变体/provider 路由；基线标量 C++（-O2/O3 +
OpenMP pragma，无 fast-math；CMake 主构建）。

## 7. provider 能力与 fallback

现状无 provider 概念：纯 CPU 标量+OpenMP，无 ACR/CUDA/GPU 依赖、无
SIMD kernel 注册（ISA 迁移由 P1-COS-IMPL 按约束 C.4-C.8 逐内核
benchmark 决定，本合同不预设）。f32/f64 双 ABI 即现状的"精度通道"
选择（f64 降级语义见 §5）。

## 8. 验证（TEST-COS-DESIGN-001，ALG-COS §9）

- 合成 fixture FIX-COS-A..F（常量场/解析注入坏点/连通域结构/NaN-Inf
  注入/IDW 方向性/双中位均值），全离线零真实数据依赖。
- 独立 oracle：NumPy 复算判定与插值公式（rtol=1e-6/atol=1e-7，
  SCI-CAL-001 §11 标度）；scipy.ndimage.label 复算 8 连通域；
  常量场与解析解 bitwise。
- 不变量 I1-I6：非坏点恒等、无检测条件恒等（dark/bias NULL 或
  sigma<=0 → out==data bitwise）、掩码极性 1=坏点、确定性（1/2/4
  线程 bitwise）、计数≤候选数、空邻域回退。
- 负面：DATA_SEMANTICS §10.1/§10.2 invalid 列 + ALG-COS §9 参数矩阵
  逐行断言（含 method 非法值、max_size<=0、NaN 源帧现状行为）。
- 串并行：1/2/4 线程 bitwise 一致；heavy run CPU/RSS 监控 + 内存上限
  断言（O(n) 常数界）；无嵌套并行。
- ISA：基线断言（无 ISA 变体；引入 SIMD 时按约束冻结 ULP 容差）。

## 9. 构建与已知限制

现状构建：根 CMake 目标 `astrocs_calibration` 内
cosmetic_corrector.cpp + ac_api.cpp（无独立 cosmetic 产物）；迁移目标
astrocs_p1_cosmetic.dll + C ABI adapter + plan/execute/cancel/inspect +
ThreadLease 接线由 P1-COS-IMPL 建立（module.yaml 已登记 manifest；
禁止跨 DLL 传 STL/异常/RTTI，约束 F.3）。遗留通道（计划迁移旧符号，
P1-COS-IMPL 决定去留）：Makefile 编译 lib/calibration/cpp/
cosmetic_corrector.cpp → cosmetic_corrector.dll（cc_* 4 导出：
cc_correct_median[data,bad_mask,H,W,window 奇数 3..15]/
cc_detect_hot/cc_detect_cold/cc_last_error；局部窗口修复，公式与
ac_* 通道不同，未编译进 CMake 主构建）。

已知限制（完整清单 = ALG-COS §10 DISP-COS-001..011，P1-COS-IMPL/INT
处理）：无 extern "C" 异常屏障（bad_alloc 可穿越 C ABI；AC_ERR_MEMORY/
INTERNAL 死值）；检测统计不过滤 NaN（NaN 源帧检测静默失效）；
bilinear 实为 IDW 且非 0 method 一律 IDW；f64 ABI 降级 f32；
w·h int31 溢出无防护；in-place 别名未定义；OpenMP ICV 违反
ThreadLease 约束；无取消检查点；**生产调用未接线母版（检测从未在
生产生效）**。

## 10. 迁移（P1-COS-IMPL 目标，不声明完成）

本目录（lib/cosmetic/）为迁移落点：astrocs_p1_cosmetic.dll、
module.yaml（同目录，已冻结 manifest：entrypoint=MISSING——registry
入口未接）、C ABI adapter、plan/execute/cancel/inspect、ThreadLease
接线、DISP-COS 清单消化、母版接线修复（DISP-COS-009）见 ALG-COS
§0/§8 与 module.yaml 注释。迁移不得改变 ALG-COS-001..005 公式语义与
DATA-P1-COS 数据语义（SCI-CAL-001 未变更前）。
