# lib/snr_estimator — astrocs.p1.noise-snr（P1-NOISE）

> 状态: CONTRACT_READY（P1-NOISE-DOC 冻结，2026-09-07）｜doc revision: r1
> 本 README 由源码逐符号核对后新建（P1-NOISE-DOC，wave W1）：函数、单位、
> dtype、shape、invalid、错误、并发、内存均以现行唯一生产实现
> `lib/snr_estimator/cpp/src/noise_model.cpp`（466 行）+ 唯一权威签名源
> `lib/snr_estimator/cpp/include/snr_estimator.h`（431 行）为准；`lib/snr_estimator/`
> 是 P1-NOISE 迁移目标目录（`astrocs_p1_noise.dll` 落码由 P1-NOISE-IMPL 建立）；
> 现状构建走 lib/snr_estimator/cpp/Makefile + build.ps1 → `snr_estimator.dll`
> （未编入根 CMake 主构建，§1 构建行详注；同 lib/cosmetic/ 先例：合同目录
> 与生产实现目录并存）。权威合同：SCI-NOISE-001..015
> （NOISE_MODEL.md，FROZEN T104 2026-08-23，不改）→ ALG-NOISE-001..003 →
> DATA-P1-NOISE / API-NOISE-001（链接见 §4）。

## 1. 身份

| 字段 | 当前值 |
|---|---|
| MOD ID / DLL target | `MOD-astrocs-phase1-noise-snr` / 现状产物 `snr_estimator.dll`（构建来源见本表"构建"行）；迁移目标 `astrocs_p1_noise.dll`（P1-NOISE-IMPL 建立，尚未存在） |
| module / ABI / doc revision | `astrocs.p1.noise-snr` / C ABI（snr_estimator.h `SNR_API` extern "C"，_WIN32 下 __declspec(dllexport) :7-11；无版本化 query 入口，迁移缺口）/ r1 |
| owner / phase scope | SA-P1-N17 / phase1（matrix P1-NOISE，wave W1；depends_on_int=P1-CAL-INT;P1-PHOT-INT） |
| 文档状态 | CONTRACT_READY（实现存在于 lib/snr_estimator/cpp/，模块化迁移未开始；不声明 IMPLEMENTED） |
| 构建 | 现状构建=lib/snr_estimator/cpp/Makefile:5,12（g++ -shared → snr_estimator.dll）+ cpp/build.ps1:29（MinGW 通道）；未编入根 CMake 主构建（无 snr_estimator CMake 目标，与 astrocs_hips/astrocs_drizzle 先例不同）——CMake 集成归 P1-NOISE-IMPL。dll_loader.cpp:41/:55 按该 DLL 名与 lib/snr_estimator/cpp/ 路径装载（生产通道吻合） |

## 2. 负责范围

负责：NoiseWeightModelV1 blank-sky 稳健方差估计 → ivar 权重——8×8
patch 网格（整除划分，循环 :179-208）、星点固定保守掩膜（source_mask
直通 / star 坐标 rmax=max(1,r0)·max(1,scale)=默认 10·6=60 px（rmax 定义
:144-145），掩膜段 :128-163）、patch 内 robust location+scale（median、
σ=1.482602218505602·MAD、5σ cosmic 裁剪 ≤2 轮 :42-107）、合格 patch 作
控制点（:241-267）、可选最小二乘平面空间方差场、全局兜底（合格 patch
variance 稳健中位数，:207-246）；fill 逐像素 variance/ivar 场（平面 LS
var(x,y)=a+b·x+c·y 负预测 clamp floor / 全局常量，fill_impl :371-419）；
scale law（x'=αx → var'=α²var、ivar'=ivar/α²，:447-454）；gain+read-noise
Poisson 诊断函数（var_ADU=max(signal,0)/gain+(rn/gain)²，:456-464，
不入生产权重）。数据语义 DATA-P1-NOISE（DATA_SEMANTICS §13）。

不负责：SNR catalogue 产品与 SNR² 加权落盘（P1-SNR/DRZ 侧，stage6 经
snr_extract_model 稀疏控制点进 snr_model 块——编排层职责）；测光质量
dex/mag（snr_phot_cal_quality，P1-PHOT 合同视角，本 README 不重复冻结）；
PSF q_psf（snr_psf_fit_quality，P1-PSF 合同视角）；编排 stage 序列与
DLL 装载（orchestrator.cpp:4177 stage6 SNR 必需 stage → :4242-4251 函数
指针）；旧乘法 SNR 通道（snr_estimate/snr_estimate_f64/snr_extract_model*
——snr_estimator.h:19-20 头注释已降级 legacy heuristic/diagnostic，
不在本合同）；整 Phase 行为（禁止）。

## 3. 输入与输出（DATA-P1-NOISE，DATA_SEMANTICS §13）

输入（build，snr_noise_model_v1/_f64，DATA_SEMANTICS §13.1）：
`data` `[h·w]` 行主序 0-based（v1=float32 / f64=float64，ADU；非 NULL
强制，h>0/w>0 否则 rc=3）；`source_mask` float32 `[h·w]`（≠0=源，可 NULL
改用 star 通道，与 star_x/y 互斥——DISP-NOISE-006）；`star_x/star_y`
double `[n_stars]` 0-based pixel（可 NULL/0；非有限坐标跳过该星
:149）；`cfg` SnrNoiseModelConfig（可 NULL=default_config；部分
下限静默钳位——DISP-NOISE-007）；`variance_floor` double（默认 1e-12）。

输出（DATA_SEMANTICS §13.2）：`NoiseWeightModelV1`（snr_estimator.h
:117-134）——ctrl_x_px/ctrl_y_px/ctrl_sigma/ctrl_variance/ctrl_ivar
double `[n_control_points]`（patch 中心 0-based；σ ADU、variance ADU²、
ivar ADU⁻²）、sigma_bg_global/variance_bg_global/ivar_bg_global 标量
（全局兜底；完全退化 rc=1 时 ivar_bg_global==0.0 显式不可用——§4a）、
n_qualified_patches+n_rejected_patches==64（8×8）、source=0（empirical
blank-sky 唯一生产基线）、has_spatial_field（=1 须 enable_spatial_field
且 n_control_points>=4）、degenerate；fill 输出 `out_variance/out_ivar`
float32 `[h·w]`（任一可 NULL，双 NULL 拒绝 rc=3 :425-426；平面预测
max(a+b·x+c·y, floor)）。所有权：out_model 由调用方分配/持有，ctrl_*
内部数组由实现 malloc/free（须 snr_noise_model_v1_free 释放，未 free
即丢弃指针=注册表泄漏 DISP-NOISE-009）。

## 4. 合同链接

| 层 | ID | 权威文档 |
|---|---|---|
| SCI | SCI-NOISE-001..015 | docs/science/NOISE_MODEL.md（FROZEN T104 2026-08-23，共享引用不改动；SNR-002 scale law/SNR-004 σ5%/SNR-005 gain 诊断/SNR-006 场 10%） |
| ALG | ALG-NOISE-001..003 | docs/algorithms/NOISE_ESTIMATION.md §13（逐符号源码锚定 + DISP-NOISE-001..009 + TEST-NOISE-DESIGN-001） |
| DATA | DATA-P1-NOISE | docs/contracts/DATA_SEMANTICS.md §13（上游 §4a 产品语义 ivar=1/variance、ivar=0 显式不可用） |
| API | API-NOISE-001 / API-P1-006 | docs/contracts/PUBLIC_API.md（snr_estimator.h 7 noise 导出现状 C API）/ docs/api/PHASE1_API_V1.md §2（编排级） |
| ARCH/MOD/SRC | ARCH-001 / MOD-astrocs-phase1-noise-snr / SRC-NOISE-001 | docs/traceability/TRACEABILITY_MATRIX.json；SRC 锚 snr_estimator.h::noise 7 导出 |

## 5. 公共入口与符号（API-NOISE-001）

现状 C ABI 7 导出（lib/snr_estimator/cpp/include/snr_estimator.h；实现
行号 noise_model.cpp）：`snr_noise_model_v1`（头 :143-149，实现 :348-356
门面）/ `snr_noise_model_v1_f64`（:151-157/:357-365，data 为 double）/
`snr_noise_model_v1_default_config`（:115/:333-345）/
`snr_noise_model_v1_fill`（:162-166/:422-429 门面）/ 
`snr_noise_model_v1_free`（:167-168/:431-444）/ `snr_noise_scale_law`
（:173-175/:447-454）/ `snr_noise_gain_variance`（:178-180/:456-464）。
调用时序：build → fill → free；free 幂等（nullptr 安全）。

返回码（build/fill 一致，noise_model.cpp 实测）：`0`=成功（含
degenerate=1 全局兜底成功，:238/:266）；`1`=完全退化（ivar_bg_global=
0.0，调用方拒绝加权，:225-232）；`3`=nullptr/尺寸非法/内部异常（C ABI
try/catch 屏障 :348-365；malloc 失败 :249-253）。同头三层模型其余符号
（snr_phot_cal_quality/snr_psf_fit_quality/旧乘法 snr_estimate*）不属
本合同，仅登记边界（头注释 :19-20）。

## 6. 配置 schema 与错误码

`SnrNoiseModelConfig`（snr_estimator.h:98-112，default_config
noise_model.cpp:333-345 括号内为默认值）：

| 字段 | 类型 | 默认 | 语义 |
|---|---|---|---|
| patch_grid_x / patch_grid_y | int | 8 / 8 | 每边 patch 数（>=2，更小静默钳位 :166-167——DISP-NOISE-007） |
| source_mask_radius_px | double | 10.0 | 星点固定保守掩膜基础半径 px |
| mask_radius_scale | double | 6.0 | 半径乘数 → 统一 rmax=max(1,r0)·max(1,scale)=60 px（不按亮度/振幅缩放，API 无 amplitude 输入） |
| gain_e_per_adu | double | 0 | 增益 e⁻/ADU（**现状零读取**，use_gain_model 无效——DISP-NOISE-003） |
| read_noise_e | double | 0 | 读出噪声 e⁻（**现状零读取**——DISP-NOISE-003） |
| saturation_level | double | 0 | 饱和电平 ADU（0=禁用；>=该值像素不统计，valid_pixel :64-68） |
| cosmic_clip_sigma | double | 5.0 | patch 内稳健裁剪 σ 倍数（>=1，更小钳位——DISP-NOISE-007） |
| min_patch_samples | int | 64 | patch 合格最小 sky 样本数（>=1；64=8×8 全格） |
| max_clip_rounds | int | 2 | cosmic 裁剪轮数（>=0） |
| use_gain_model | uint32 | 0 | **现状零读取**：置 1 无任何效果（DISP-NOISE-003，字段存在易误用） |
| enable_spatial_field | uint32 | 1 | 1=最小二乘平面空间方差场 |
| variance_floor | double | 1e-12 | ivar 分母下限 ADU²（build ≤0 不 clamp 原值直通、fill ≤0 回退 1e-12——两阶段语义不一致 DISP-NOISE-002） |

错误码：`0`=成功（含 degenerate 兜底）/ `1`=完全退化（无合格 patch 且
全帧兜底退化，ivar_bg_global=0.0）/ `3`=参数非法（nullptr、h/w≤0）或
内部异常（malloc 失败）。无独立版本化 config schema 文件（json/版本号
=MISSING，正式 schema 由 P1-NOISE-IMPL 冻结 acs_module_descriptor_v1.
config_schema_ver）。

## 7. 算法与实现锚

不重复公式（权威见 ALG-NOISE-001..003，NOISE_ESTIMATION §13.1 逐符号
锚表）：ALG-NOISE-001=blank-sky 稳健方差模型（noise_model_impl
:112-267：参数校验 :118、g_model_floor 注册 :126、fixed conservative
掩膜 :128-163、patch 网格循环 :179-208、全局兜底 :207-246、控制点数组
:241-267；robust_median :42-53/robust_sigma :55-62/collect_patch_sky
:73-107）；ALG-NOISE-002=fill/free/scale_law（fill_impl LS 平面+clamp
:371-419、free :431-444、scale_law :447-454）；ALG-NOISE-003=gain 诊断
（gain_variance :456-464）。SCI 公式与默认容差不改动（禁止据代码缺陷
反向修改 SCI，差异全部登记 DISP-NOISE-*）。

## 8. 并发与资源

现状单线程顺序执行（noise_model_impl 无 omp pragma，patch 循环 :179-208
串行；descriptor parallel_ok=true 为编排轴登记，实际并行轴为迁移整改
点——ALG §13.2）；无 ThreadLease 接线。共享可变状态=进程级
`g_model_floor`（noise_model.cpp:32 `std::unordered_map<const
NoiseWeightModelV1*,double>`）：build :126 注册、fill :402-405 指针 key 查询与
floor 回退、free :433 擦除——原始指针 key（free 后同址复用 ABA 风险）且
并发 build/free 无锁（DISP-NOISE-001）。线程安全口径：reentrant=yes、
threadsafe=yes 以 model 对象隔离为前提（PHASE1_API_V1 §2）。输出
bitwise 与线程数无关。内存：掩膜 O(h·w) uint8（source_mask 直通路径
不复制）、控制点 O(64)、全帧兜底样本 O(h·w)；时间 O(h·w)。无取消
检查点（noise_model_impl/fill_impl 无 cancel 回调——DISP-NOISE-004，
§5c 行带粒度取消为计划语义）。无文件/网络 I/O、无日志输出。

## 9. 验证（TEST-NOISE-DESIGN-001，ALG §13.4）

- 合成 fixture FIX-NOISE-A..G（Gaussian 合成/平面场恢复/常量场退化/
  掩膜解耦/Poisson 诊断交叉/scale law/fill 语义），固定 seed 全离线、
  零真实数据依赖；负面行逐条断言（ALG §4/§13.3 + SCI §7/§8）。
- 冻结容差（不得放宽）：σ 5%（SNR-004）、平面场 10%（SNR-006）、
  Poisson 诊断交叉 5%（SNR-005）、NumPy 参考复算 rtol 1e-9（SCI §11）；
  不变量 I1-I6（ivar=1/variance 精确互倒、variance≥floor、degenerate⇒
  ivar==0.0、确定性 bitwise、n_qualified+n_rejected==64、free 幂等）。
- 既有 tests/unit/p1_noise_test.cpp（6 组：blank_sky/monte_carlo_
  poisson/low_high_signal/negative_values/gain_edges/variance_ivar_not_
  mixed，tests/unit/CMakeLists.txt:312-316 注册，链接 astrocs_phase1_
  noise）为 P1-005 期旧封装测试（astrocs::phase1::NoiseModel 通道），
  非 TEST-NOISE-DESIGN-001 本体；由 P1-NOISE-TEST 对齐重锚（可执行
  TEST-P1-NOISE-001 归 P1-NOISE-TEST 建立）。

## 10. 已知限制与迁移（P1-NOISE-IMPL/TEST/INT 目标，不声明完成）

已知限制（完整清单=ALG §13.3 DISP-NOISE-001..009，登记不改码）：001
g_model_floor 指针 key ABA/并发无锁；002 build/fill floor 语义不一致
（build floor<=0 不 clamp）；003 cfg gain 三字段零读取（use_gain_model
无效）；004 无取消检查点；005 scale_law alpha 无校验（NaN/负值直传，
variance/ivar 可失互倒）；006 source_mask 与 star 通道互斥（掩膜非 NULL
忽略 star 坐标）；007 参数下限静默钳位无返回码区分；008 空 patch 与
质量拒绝混计 n_rejected_patches；009 malloc 失败路径 rc=3 前注册表条目
泄漏（调用方不 free 时）。另：无版本化 config schema；无 SIMD/ISA 变体
（引入时按约束 C.4-C.8 逐内核 benchmark 冻结 ULP 容差）。

迁移落点：本目录（lib/snr_estimator/）为迁移落码目标——
`astrocs_p1_noise.dll`、C ABI adapter、plan/execute/cancel/inspect、
ThreadLease 接线、DISP-NOISE 清单消化、版本化 config schema 由
P1-NOISE-IMPL 建立（module.yaml 已登记 manifest，entrypoint=MISSING）。
遗留通道（计划迁移旧符号，NOISE_ESTIMATION §13.5）：lib/phase1/noise/
noise_model.{h,cpp}（39+67 行，astrocs::phase1::NoiseModel::estimate=
median+MAD 单值退化子集 + gain_variance；静态库 astrocs_phase1_noise
CMakeLists.txt:435-438、主程序链接 :513；单测 tests/unit/
p1_noise_test.cpp :312-316）去留由 P1-NOISE-IMPL 决定并登记其
TASK_RESULT。迁移不得改变 ALG-NOISE-001..003 公式语义与 DATA-P1-NOISE
数据语义（SCI-NOISE-001..015 未变更前）。本 README/合同只描述现状，
禁止声明 IMPLEMENTED（落码验收后由 IMPL 任务更新）。
