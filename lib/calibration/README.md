# lib/calibration — astrocs.p1.calibration（P1-CAL）

> 状态: CONTRACT_READY（P1-CAL-DOC 冻结，2026-09-07）｜doc revision: r2
> 本 README 由源码逐函数核对后全面重写（P1-CAL-DOC，wave W1）：函数、单位、
> 坐标、dtype、shape、invalid、错误、并发、内存、I/O 均以
> `include/astro_calibration.h` + `src/{master_generator,calibrator,
> cosmetic_corrector,ac_api}.cpp`（CMake `astrocs_calibration`，唯一生产构建）
> 为准；旧版 README 中与源码不符的黄金分割搜索、Python 封装、GitHub 仓库
> 等陈述已删除。权威合同：SCI-CAL-001 → ALG-CAL-001..006 → DATA-P1-CAL /
> API-CAL-001（链接见 §6）。

## 1. 身份

| 字段 | 当前值 |
|---|---|
| MOD ID / DLL target | `MOD-astrocs-phase1-calibration` / 现状产物 CMake 静态库 `astrocs_calibration`（+遗留 MinGW DLL 通道）；迁移目标 `astrocs_p1_calibration.dll`（P1-CAL-IMPL 建立，尚未存在） |
| module / ABI / doc revision | `astrocs.p1.calibration` / C ABI（AC_API extern "C"，无版本化 query 入口，迁移缺口）/ r2 |
| owner / phase scope | SA-P1-C14 / phase1（wave W1） |
| 文档状态 | CONTRACT_READY（实现存在，模块化迁移未开始；不声明 IMPLEMENTED） |
| 构建 | CMakeLists.txt:321-333（STATIC + OpenMP 可选，链 astrocs_aio/astrocs_common） |

## 2. 负责范围

负责：master bias/dark/flat 生成（非对称 sigma-clip + median/mean 合并，
NaN 跳过）；单帧校准算术（dark_opt 双分支 + flat floor 0.1，FP32/真 FP64
像素路径）；热/冷像素检测（全局 median+1.4826·MAD 阈值）+ 8 连通结构
过滤 + 5×5 中值 / 4 方向 IDW 修复；Gaia 测光比例标量乘法（现状未接线）。

不负责：FITS/XISF 读写（astro_image_io，调用方侧）；母版按曝光/滤镜分组
与匹配（orchestrator）；K=t_light/t_dark 计算（调用方从 EXPTIME 得出）；
噪声方差/SNR（snr_estimator）；天光背景扣除（Phase2）；宇宙线剔除（叠加
rejection）；WCS/测光定标；线程池/ThreadLease 授予（现状 OpenMP 默认
team，迁移后由 host 授予）；整 Phase 行为（禁止）。

## 3. 输入与输出（DATA-P1-CAL，DATA_SEMANTICS §9）

输入：全部内存数组，行主序 `idx=y·w+x` 0-based，单位 ADU；
stack `[n_frames][h][w]` 连续；master_bias/dark/flat 与 light `[h][w]`，
均可 NULL（语义见 DATA_SEMANTICS §9.1：dark/bias NULL=不减/不检测，
flat NULL=不除）；K 无量纲（=t_light/t_dark）；sigma 参数无量纲（MAD 倍数，
<=0 禁用对应检测）；掩码 1=坏点。

输出（单位/dtype/shape/invalid 全表见 DATA_SEMANTICS §9.2）：

- master 生成 → `out[h][w]` float32/64（ADU；全 NaN 像素列 → NaN 合法）。
- `ac_calibrate_frame(+_f64)` → 校准帧（可负，不 clamp 不加 pedestal，
  SCI §9a）+ `actual_k`（dark_opt=1 生效=入参 K；标准分支=1.0）。
- `ac_correct_frame(+_f64)` → 修复帧 + `out_hot/out_cold`（结构过滤后计数）。
- 错误码 int：0=AC_OK；-1=AC_ERR_PARAM（空指针/非正维度）；-2/-3 定义但
  从未返回（DISP-CAL-001）。

坐标：无坐标变换、无 WCS（SCI §3a，frame identity 不变）；dtype f32 ABI
float32 / f64 ABI double。

## 4. 合同链接

| 层 | ID | 权威文档 |
|---|---|---|
| SCI | SCI-CAL-001 | docs/science/CALIBRATION.md（FROZEN） |
| ALG | ALG-CAL-001..006 | docs/algorithms/CALIBRATION_ALGORITHMS.md（§3 逐公式源码锚定） |
| DATA | DATA-P1-CAL | docs/contracts/DATA_SEMANTICS.md §9 |
| API | API-CAL-001 / API-P1-001 | docs/contracts/PUBLIC_API.md / docs/api/PHASE1_API_V1.md |
| MOD/SRC | MOD-astrocs-phase1-calibration / SRC-CAL-001 | docs/traceability/TRACEABILITY_MATRIX.json；include/astro_calibration.h（14 AC_API 符号） |
| TEST | TEST-CAL-DESIGN-001 | docs/algorithms/CALIBRATION_ALGORITHMS.md §9（可执行 TEST-P1-CAL-001 由 P1-CAL-TEST 落地） |

## 5. 实现事实（源码核对）

- **算法**：master 生成 = 逐像素非对称 sigma-clip（σ=1.4826·MAD，
  nth_element 中位数，max_iter 迭代，σ=0/无剔除提前终止）+ median/mean
  合并（mean=帧序升序 FP32 累加，顺序冻结）；单帧直接拷贝。
  master flat = 减 bias → 逐帧 median 归一（0/NaN→1.0）+ floor 0.1 →
  sigma-clip+mean → 最终 median 归一 + floor 0.1（无 combine 参数，固定
  mean）。校准 = `(light−bias−K·(dark−bias))/max(flat,0.1)`（dark_opt=1
  需 bias&dark，否则回退 `(light−dark)/max(flat,0.1)` 且 K=1.0）；
  normalize_flat（median→1.0+floor）当前无调用方（DISP-CAL-007）。
  cosmetic = 全局阈值检测（不过滤 NaN，DISP-CAL-004）→ 8 连通
  >=max_size 剔除 → 修复（median 5×5 镜像反射邻域 / bilinear 名义实为
  4 方向 1/dist IDW，DISP-CAL-003）。
- **FP64 ABI**：仅 `ac_calibrate_frame_f64` 真双精度；4 个
  `ac_generate_master_*_f64`/`ac_correct_frame_f64` 内部转 float32 执行
  （统计/mask 路径降级，头文件 105-115 声明）。
- **并发**：函数级 reentrant+threadsafe（无共享可变全局）；OpenMP 默认
  team；`ac_set_num_threads(n>0)` 进程级改写 OpenMP ICV（竞态 + 跨模块
  副作用，DISP-CAL-002；API-P1-001 §2 列为 V5 整改点，迁移后 host
  ThreadLease 取代）。逐像素独立，输出 bitwise 与线程数无关。
- **内存**：调用方分配输出；模块内 std::vector RAII；峰值额外内存：
  generate_master_flat O(n_frames·npix·4B)（norm 主缓冲）、f64 转接层
  O(n_pix) 全帧复制、其余 O(npix) 以下。
- **I/O**：零文件/网络 I/O；stderr 日志（master 生成 2 行/次 +
  photometry 2 行/次）；无日志文件、无 manifest。
- **生产调用方**：仅 `ac_calibrate_frame`（lib/phase1_session/p1_session.cpp:243，
  calibrate stage；取消在 session 层帧粒度）；其余导出当前无生产调用方。

### 5.1 配置 schema（现状：C 参数直传 + phase1_session JSON）

- master 生成：`sigma_low/sigma_high/max_iterations/combine`
  （combine∈{0=mean,1=median}；master flat 无 combine 参数）。
- 校准：`dark_optimization: 0/1`、`dark_scale_factor: K`（k_init 直通，
  非搜索——旧版"黄金分割搜索"已不存在）、`actual_k` 出参。
- cosmetic：`hot_sigma/cold_sigma/method(0=median,1=IDW)/
  max_structure_size`（batch_config.json 实验默认 5.0/5.0/median/4）。
- phase1_session 键：`master_bias/master_dark/master_flat`（路径或 null）、
  `input_lights[]`、`dark_optimization`（bool）、`dark_scale_factor`
  （float，默认 1.0）。正式版本化 schema 由 P1-CAL-IMPL 冻结
  （acs_module_descriptor_v1.config_schema_ver）。

## 6. 并发与资源

资源分类：cpu_heavy（master 生成/校准为重计算；descriptor 迁移取值
execution_class=cpu_heavy、parallel_ok=1）。现状并行=OpenMP 默认 team
（线程数=omp_get_max_threads，可被 ac_set_num_threads 进程级改写）；
**不满足**约束 D.3/D.4 的 ThreadLease 模型——迁移整改点（API-P1-001 §2
已登记）。无内建 benchmark/ISA 变体/provider 路由；基线标量 C++（-O2/O3 +
OpenMP pragma），无 fast-math（CMake 主构建；遗留 MinGW 通道除外）。

## 7. provider 能力与 fallback

现状无 provider 概念：纯 CPU 标量+OpenMP，无 ACR/CUDA/GPU 依赖、无
SIMD kernel 注册（ISA 迁移由 P1-CAL-IMPL 按约束 C.4-C.8 逐内核 benchmark
决定，本合同不预设）。f32/f64 双 ABI 即现状的"精度通道"选择。

## 8. 验证（TEST-CAL-DESIGN-001，ALG-CAL §9）

- 合成 fixture FIX-CAL-A..F（常量场/解析梯度/离群+NaN stack/比例场 flat/
  cosmetic 图样/负面），全离线零真实数据依赖。
- 独立 oracle：NumPy 复算 F1–F4（常量场 max_abs==0）；scipy.ndimage.label
  复算连通域；K 恒等映射。
- 不变量 I1-I6：常量场、空平场、幂等归一、确定性（1/2 线程 bitwise）、
  负值保留、掩码极性（SCI §7 四门超集）。
- 负面：DATA_SEMANTICS §9.1/§9.2 invalid 列 + ALG-CAL §7 全表逐行断言。
- 串并行：1/2/4 线程 bitwise 一致；2 核 ≥1.60 加速比（约束 D.7）；无
  嵌套并行。
- ISA：基线断言（无 ISA 变体；引入 SIMD 时按约束冻结 ULP 容差）。
- 资源：heavy run CPU/RAM/线程时序监控；内存上限断言；无 RSS 失控。
- 冻结容差：bitwise / max_abs==0（解析）；NumPy 对照 float32
  rtol=1e-6、atol=1e-7（SCI §11/§15 预冻结）；actual_k 精确相等。

## 9. 构建与已知限制

构建（唯一生产通道）：根 CMake 目标 `astrocs_calibration`
（STATIC：calibrator/master_generator/cosmetic_corrector/ac_api.cpp +
include；OpenMP 可选；链 astrocs_aio/astrocs_common）。遗留通道（非生产、
计划迁移旧符号，P1-CAL-IMPL 决定去留）：build.ps1（MinGW64 g++ 单 DLL，
-march=native -ffast-math）、Makefile（cpp/cosmetic_corrector.cpp →
cosmetic_corrector.dll，cc_* 4 导出：cc_correct_median[data,bad_mask,H,W,
window 奇数 3..15]/cc_detect_hot/cc_detect_cold/cc_last_error）、
batch_config.json（实验配置）、CALIBRATION_PROCESS.md（历史流程参考）。
`lib/calibration/python/`、`astro_calibration.dll`、
`cosmetic_corrector.dll` 在当前树不存在（旧 README 记载失效）。

已知限制（完整清单 = ALG-CAL §10 DISP-CAL-001..011，P1-CAL-IMPL/INT
处理）：无 extern "C" 异常屏障（bad_alloc 可穿越 C ABI；AC_ERR_MEMORY/
INTERNAL 死值）；generate_master_flat 负 median 未防护；
ac_set_num_threads 进程级 ICV 副作用；bilinear 实为 IDW；cosmetic 统计
不过滤 NaN；无取消检查点（session 层帧粒度替代）；w·h int31 溢出无防护；
optimize_dark_k/apply_photometry 未编译未接线；normalize_flat/
compute_mad 死代码风险。

## 10. 迁移（P1-CAL-IMPL 目标，不声明完成）

module.yaml（同目录）登记 manifest：id=MOD-astrocs-phase1-calibration、
module_id=astrocs.p1.calibration、dll_name=astrocs_p1_calibration.dll、
entrypoint=MISSING（registry 入口未接）。C ABI adapter、
plan/execute/cancel/inspect、ThreadLease 接线、DISP-CAL 清单消化见
ALG-CAL §8 与 module.yaml 注释。禁止跨 DLL 传 STL/异常/RTTI（约束 F.3）。
