# Module: calibration

> P1-CAL-DOC 事实修订（2026-09-07）：本页由源码核对后修订——生产调用方、
> 线程模型、错误语义、诊断/测试陈述以 lib/calibration 现行源码为准；
> 旧版与源码不符处（"16 线程块并行"、每帧日志/母版 hash、Python 对照
> 测试等）已删除或修正。算法细节见 docs/algorithms/CALIBRATION_ALGORITHMS.md
> （ALG-CAL-001..006）；数据语义见 docs/contracts/DATA_SEMANTICS.md §9
> （DATA-P1-CAL）；API 合同见 docs/contracts/PUBLIC_API.md（API-CAL-001）
> 与 docs/api/PHASE1_API_V1.md（API-P1-001）。
> P1-COS-DOC 增补（2026-09-07）：cosmetic 域合同已独立冻结为
> astrocs.p1.cosmetic（lib/cosmetic/，ALG-COS-001..005 =
> docs/algorithms/COSMETIC_ALGORITHMS.md，DATA-P1-COS = DATA_SEMANTICS
> §10，API-COS-001 = PUBLIC_API.md）——与本页 P1-CAL 合同共享同一编译
> 目标 astrocs_calibration 与头文件；本页仅保留 P1-CAL 视角摘要，
> cosmetic 域以 lib/cosmetic/README.md 为权威。

## 职责

masterBias/Dark/Flat 生成（sigma-clip + median/mean 合并）、单帧图像校准
（bias/dark/flat，dark_opt 双分支）、热/冷像素检测与插值修复（cosmetic）。
模块级状态 CONTRACT_READY（P1-CAL-DOC 冻结）；迁移目标 astrocs.p1.calibration /
astrocs_p1_calibration.dll 由 P1-CAL-IMPL 建立。

## 非职责

不做天体测量/测光定标/噪声方差估计；不做 FITS/XISF 文件读写（astro_image_io）；
不做母版按曝光/滤镜分组匹配（orchestrator）；不做 K=t_light/t_dark 计算
（调用方）；不做天光背景扣除（Phase2）与宇宙线剔除（叠加 rejection）。

## Production callers

当前生产调用点（P1-COS-DOC 增补核对，2026-09-07）：
`lib/phase1_session/p1_session.cpp:243` 调 `ac_calibrate_frame`
（calibrate stage）与 `:294` 调 `ac_correct_frame`（cosmetic stage，
2026-09-01 c5629be6 引入；master_dark/master_bias 传 nullptr → 检测
全禁用、恒等 pass，DISP-COS-009——cosmetic 域现状与整改见
lib/cosmetic/README.md）。`ac_generate_master_*`、`ac_set_num_threads`
当前无生产调用方（master 由外部预生成；ac_set_num_threads 由
session budget 注入通道持有）。

## Public API

astro_calibration.h：12 个科学/工具导出（5 f32 科学 + 5 f64 变体 +
ac_set_num_threads + ac_version）。登记合同 API-CAL-001
（docs/contracts/PUBLIC_API.md；cosmetic 路径 ac_correct_frame(+_f64)/
ac_set_num_threads 另由 API-COS-001 独立登记，模块级合同视角）。
遗留通道（cc_* DLL、optimize_dark_k、apply_photometry）未编译进
CMake 主构建，属计划迁移旧符号。

## Data contract

输入/输出为内存数组（float32/64，ADU，行主序 0-based），非 FITS：
DATA-P1-CAL（docs/contracts/DATA_SEMANTICS.md §9）。落盘由调用方完成
（phase1_session 写 calibrated_<原名>.fits）。母版分组（曝光/滤镜）在
orchestrator 层。

## Ownership

无句柄对象；输出 buffer 调用方分配/提供，模块零 malloc 输出。

## Thread safety

全部函数 reentrant、threadsafe（无共享可变全局）；OpenMP parallel-for
像素/帧域并行（默认 team，线程数非 16 硬编码）。例外：
ac_set_num_threads 进程级改写 OpenMP ICV（DISP-CAL-002，迁移整改点）。

## Errors

AC_OK(0)/AC_ERR_PARAM(-1)（astro_calibration.h:21-24）；AC_ERR_MEMORY(-2)/
AC_ERR_INTERNAL(-3) 定义但从未返回（无 extern "C" 异常屏障，
DISP-CAL-001）。母版缺失/滤镜不匹配 → orchestrator 层 CONFIG/NO_DATA。
flat floor 0.1 下界与 median 归一行为见 ALG-CAL §3（F1–F3）。
遗留 cc_* 通道：window 偶数/<3/>15 → −1（cpp/cosmetic_corrector.cpp，
非 ac_correct_frame）。

## Config

现状为 C 参数直传 + phase1_session JSON（master_* 路径、input_lights、
dark_optimization、dark_scale_factor）；sigma-clip/cosmetic 参数族见
ALG-CAL §8。无全局状态（除 OpenMP ICV）。

## Science IDs

SCI-CAL-001（docs/science/CALIBRATION.md，FROZEN）；ALG-CAL-001..006
（docs/algorithms/CALIBRATION_ALGORITHMS.md，CONTRACT_READY）。
cosmetic 域：SCI-CAL-001 共享 + ALG-COS-001..005
（docs/algorithms/COSMETIC_ALGORITHMS.md，CONTRACT_READY，
MOD-astrocs-phase1-cosmetic）。

## 性能特征

O(pixels) 单 pass（校准/检测/插值）；O(max_iter·n_frames·npix)（master
生成）。OpenMP 并行度=进程默认 team（无 ThreadLease，迁移整改点）。
模块无内置 benchmark 数据；性能结论以 heavy run 资源监控为准。

## 缓存

无模块内缓存；母版在 orchestration/调用方层缓存。

## Diagnostics

stderr 日志：generate_master/generate_master_flat 每次调用 2 行
（ac_log：参数+耗时）；apply_photometry 2 行。无每帧 FITS 头写入、无母版
hash 诊断（旧版陈述已删除）。actual_k/out_hot/out_cold 为可选输出统计。

## Tests

TEST-CAL-DESIGN-001（ALG-CAL 文档 §9：合成 fixture FIX-CAL-A..F、NumPy
独立 oracle、不变量 I1-I6、负面/串并行/ISA/资源设计与冻结容差）——可执行
测试由 P1-CAL-TEST 建立（TEST-P1-CAL-001）。既有共址测试：
lib/calibration/tests/test_photometry_apply.cpp（未挂接 CMake 测试目标）。
Python 对照为历史层（当前树无 python/ 目录）。

## Source files

lib/calibration/{include,src}/（CMake astrocs_calibration：
calibrator/master_generator/cosmetic_corrector/ac_api.cpp）；
未编译：dark_optimizer.cpp、photometry_apply.cpp；遗留双实现：
cpp/cosmetic_corrector.cpp（cc_* 通道）。
