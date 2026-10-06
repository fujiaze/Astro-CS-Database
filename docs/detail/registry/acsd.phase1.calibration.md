# 模块 acsd.phase1.calibration

> 上游：`docs/ACSD_DESIGN.md`「模块与 ABI」一节
> 科学正本：docs/science/calibration/CALIBRATION.md（SCI-CAL-001）、docs/science/noise_snr/NOISE_SNR.md（噪声模型「噪声项分类学」与「协方差传播」两节）、
> docs/science/algorithms/CALIBRATION_ALGORITHMS.md（ALG-CAL-001..006）
> 数据正本：docs/detail/registry/acsd.phase1.calibration.md（DATA-P1-CAL 端口表，本页输入输出端口表）
> API 正本：docs/engineering/api/PUBLIC_API.md（API-CAL-001）、docs/engineering/api/PUBLIC_API.md「分阶段 API 面」（API-P1-001）

## 职责与明确非职责

Registry production 模块（唯一源 = module_adapters.cpp 的 phase1_descriptor）。
职责：master 生成（sigma-clip + median/mean 合并）与单帧校准（bias/dark/flat，
dark_opt 双分支）——生产源 lib/algorithms/calibration/src/。

不做：天体测量/测光定标/噪声方差估计；FITS/XISF 文件读写（astro_image_io）；
源检测与背景估计（detect_sources / estimate_snr）；WCS 解算；母版按曝光/滤镜
分组匹配（orchestrator 层）；`K=t_light/t_dark` 的计算（调用方职责）；天光背景
扣除（Phase2）与宇宙线剔除（Phase2 排异）。

cosmetic 域的合同已独立冻结为 acsd.p1.cosmetic（`ALG-COS-001..005` =
docs/science/algorithms/COSMETIC_ALGORITHMS.md，`DATA-P1-COS` =
registry/acsd.phase1.cosmetic.md，`API-COS-001` = PUBLIC_API.md），与本页 P1-CAL 合同共享同一
编译目标与头文件；本页只保留 P1-CAL 视角，cosmetic 域见
registry/acsd.phase1.cosmetic.md。

## 输入输出端口、DATA、单位、坐标、invalid

| 端口 | DATA | 必/可 | 单位 | 坐标 |
|---|---|---|---|---|
| `frames` | `DATA-P1-FRAME` | 必 | `UnitId::ADU` | `CoordinateFrame::PIXEL` |
| `calibrated` | `DATA-P1-CAL` | 可 | `UnitId::ADU` | `CoordinateFrame::PIXEL` |

invalid = NaN/coverage=0（按 DATA 合同）。输入输出为内存数组（float32/64、ADU、
行主序 0-based），非 FITS；落盘由调用方完成（phase1_session 写
`calibrated_<原名>.fits`）。母版分组（曝光/滤镜）在 orchestrator 层。

### 数值落地口径

定标算式、方差传播式、`K=t_light/t_dark` 与 bias/dark/flat 施加顺序的正本 =
SCI-CAL-001 与 ALG-CAL-001..006；本页只记落地约束：

- 负值按原值传递，不静默置零；pedestal 只取显式声明值；夹紧默认保持关闭；
- 共享 master 的相关性以共同 master ID 保留，按相关样本处理，不得因重复
  减同一 master 而把方差低估；
- gain、Poisson、read noise、量化、master 方差分别标识，不合并为单一噪声项；
- flat floor 0.1 下界与 median 归一行为按 ALG-CAL（F1–F3）执行。

归约口径 = ALG-CAL。

## 公共 header、核心 symbol 与生命周期

模块级 API = API-CAL-001（docs/engineering/api/PUBLIC_API.md；头 astro_calibration.h、
实现 lib/algorithms/calibration/src/）；编排级 API = API-P1-001（phase session
extern "C"，签名源 docs/engineering/api/PUBLIC_API.md「分阶段 API 面」）；生命周期
create→validate→run→inspect→destroy。

头 astro_calibration.h 导出 12 个符号（5 个 f32 科学入口 + 5 个 f64 变体 +
`ac_set_num_threads` + `ac_version`）：`ac_generate_master_bias`、
`ac_generate_master_dark`、`ac_generate_master_flat`、`ac_calibrate_frame`(+`_f64`)、
`ac_correct_frame`(+`_f64`)、`ac_set_num_threads`、`ac_version`。

### Production callers

生产调用点唯一：`lib/phase1_session/p1_session.cpp` 调 `ac_calibrate_frame`
（calibrate stage）与 `ac_correct_frame`（cosmetic stage）。`ac_generate_master_*`
无生产调用方（master 由外部预生成）；`ac_set_num_threads` 无生产调用方（线程数
由 session budget 注入通道持有）。cosmetic stage 当前 master_dark/master_bias
传 nullptr，检测全禁用、恒等 pass。

### 源文件

`lib/algorithms/calibration/include/`（公共头族）与
`lib/algorithms/calibration/src/`（CMake 目标 `acsd_calibration`：
`lib/algorithms/calibration/src/calibrator.cpp` / `lib/algorithms/calibration/src/master_generator.cpp` /
 `lib/algorithms/calibration/src/cosmetic_corrector.cpp` / `lib/algorithms/calibration/src/ac_api.cpp`）。
未挂接 CMake 的源：`dark_optimizer.cpp`、`photometry_apply.cpp`；双实现通道
`cpp/cosmetic_corrector.cpp`（`cc_*`）。

## Registry descriptor 与配置 schema

module_id=`acsd.phase1.calibration`; execution_class=`cpu_heavy`;
parallel_ok=True; 目标交付形态 acsd_p1_calibration.dll。配置 = phase config JSON
（签名见 docs/engineering/api/PUBLIC_API.md「分阶段 API 面」，默认值见 docs/engineering/contracts/CONFIG.md），字段面：

| 字段 | 默认 | 单位 | 说明 |
|---|---|---|---|
| `bias_path` | —— | —— | master bias 路径 |
| `dark_path` | —— | —— | master dark 路径 |
| `dark_exposure` | —— | s | master dark 曝光 |
| `flat_path` | —— | —— | master flat 路径 |
| `gain` | —— | e⁻/ADU | 由元数据或显式覆盖 |
| `read_noise` | —— | e⁻ | 由元数据或显式覆盖 |
| `clip_negative` | false | —— | true 仅用于显式声明的具名降级路径：必须写 `degraded_reason` 并入 manifest |

sigma-clip 参数族按 ALG-CAL。现状接线为 C 参数直传 + phase1_session JSON
（`master_*` 路径、`input_lights`、`dark_optimization`、`dark_scale_factor`）；
`K=t_light/t_dark` 的计算在调用方完成。

## Execution class、并行轴、ThreadBudget lease、确定性

`cpu_heavy`; parallel=是（资源门拒绝 heavy+serial 组合）; worker 数 =
ThreadBudget.max_workers（唯一取值源，禁 hardware_concurrency）。

现状并行 = OpenMP parallel-for 像素/帧域，schedule 默认 team（线程数非 16
硬编码）；现状无 ThreadLease，取进程默认 team（迁移整改点）。全部函数
reentrant、threadsafe（无共享可变全局）。例外：`ac_set_num_threads` 在进程级
改写 OpenMP ICV（迁移整改点，缺陷登记 = ALG-CAL）。

确定性 = NOT_VERIFIED（未取得验收证据）。

复杂度：校准/检测/插值 O(pixels) 单 pass；master 生成
O(max_iter·n_frames·npix)。模块无内置 benchmark 数据，性能结论以 heavy run
资源监控为准。

## 内存/cache/I-O/所有权

cache/内存按 ALG 合同（bounded）; I-O 单 writer; 无模块内缓存（母版在
orchestration/调用方层缓存）。

所有权 = 调用方分配/提供输出 buffer，模块零 malloc 输出；无句柄对象。

## 错误、日志、指标、取消和 checkpoint

错误码与退出码唯一源 = lib/infrastructure/cli/exit_codes.h（本页不复制数值表）。
模块级：`AC_OK(0)` / `AC_ERR_PARAM(-1)`（astro_calibration.h）；`AC_ERR_MEMORY(-2)`
与 `AC_ERR_INTERNAL(-3)` 定义但从未返回（无 extern "C" 异常屏障，缺陷登记 =
ALG-CAL）。母版缺失/滤镜不匹配由 orchestrator 层报 CONFIG/NO_DATA。缺关键
单位、gain 或 read-noise 口径 → fail-closed；master 与 light 尺寸/帧身份不一致
→ 拒绝；NaN/Inf 输入 → 标记 validity，不静默置零。`cc_*` 通道 window 偶数 / <3 /
>15 → −1（cpp/cosmetic_corrector.cpp，非 `ac_correct_frame`）。
- **负例与归零分支 N19（T05–T07 负向轮，本卡死值与静默 scale）**：负例输入构造甲 =
  触发 `AC_ERR_MEMORY/AC_ERR_INTERNAL` 的分配 / 内部异常路径；负例输入构造乙 =
  缺 gain 口径仍要求施加标度的帧。预期行为甲 = 该两码为死值，永不返回，
  异常无屏障，直接上抛，不伪造错误码。落盘标记甲 = 缺该两码的错误登记。
  预期行为乙 = fail-closed，不以 `scale = 1.0` 静默代替标度。落盘标记乙 =
  `CONFIG/NO_DATA` + `validity` 标记；任一静默代替 ⇒ 判红。

Diagnostics：stderr 日志 `ac_log`，`generate_master` / `generate_master_flat`
每次调用 2 行（参数 + 耗时），`apply_photometry` 2 行；无每帧 FITS 头写入；
`actual_k`/`out_hot`/`out_cold` 为可选输出统计。

取消 = 协作取消（契约：宿主 cancel 通道 → 停止调度新单元 → 等运行中单元完成
→ exit 9，最高设计协作取消口径；接线以实测为准）；模块内无 checkpoint（无断点续算）。

## 独立 synthetic 验证命令与容差

测试标识 = `TEST-P1-CAL-001`（registry descriptor 单源）；执行证据 =
NOT_VERIFIED（未取得验收证据）；容差 = NOT_VERIFIED（未取得验收证据）；设计
冻结容差 = `TEST-CAL-DESIGN-001`（CALIBRATION_ALGORITHMS.md［A-1］「判据与误差」一节，合成fixture
FIX-CAL-A..F、NumPy 独立 oracle、不变量 I1–I6、负面/串并行/ISA/资源设计）。

Oracle 面：

- 注入已知 bias/dark/flat，验证定标信号与其方差与解析/Monte Carlo 理论一致；
- 共同 master 相关性不变量（不能因重复减同一 master 而方差被低估）；
- 1 worker vs N worker 输出一致；ISA 等价；
- 注入点源信息权重理论 σ_F=1/√W_psf 与实测一致（与 psf / noise_snr 模块配合）。

已取证的共址验证读数中，定标应用面未挂接 CMake 测试目标。Python 对照实现不在本树。

## 已知限制

- 缺陷与现行语义的登记面 = ALG-CAL；
- `AC_ERR_MEMORY` / `AC_ERR_INTERNAL` 为死值，无异常屏障；
- `ac_set_num_threads` 改写进程级 OpenMP ICV，与「模块不私建线程池」纪律
  不一致，属迁移整改点；
- 现状无 ThreadLease，worker 数不来自 ThreadBudget；
- `dark_optimizer.cpp`、`photometry_apply.cpp`、`cc_*` 通道未挂接 CMake 主构建；
- `ac_generate_master_*` 与 `ac_set_num_threads` 无生产调用方；
- 全局限制登记 = artifacts/evidence/known-limitations-ledger/LIMITATIONS.md。
