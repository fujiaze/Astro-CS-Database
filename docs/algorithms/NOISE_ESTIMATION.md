# Noise Estimation Algorithms (ALG-NOISE)

> ID 覆盖: ALG-NOISE-001 ALG-NOISE-002 ALG-NOISE-003  上游 SCI: SCI-NOISE-001..015
> (docs/science/NOISE_MODEL.md，FROZEN T104 2026-08-23，共享引用不改动)
> 状态: DERIVED (T204 冻结; V5 ALG-003 重验 2026-08-28) + CONTRACT_READY §13 增补
> (P1-NOISE-DOC 冻结，2026-09-07，逐符号源码锚定 §13.1 / DISP-NOISE §13.3 /
> TEST-NOISE-DESIGN-001 §13.4，根公式不变)  模块: snr_estimator/noise_model

## 1 上游 SCI 与输入输出

- 上游: `SCI-NOISE-001..015` (8×8 patch, MAD 1.4826, 平面场 a+b·x+c·y, floor 1e-12)
- 输入: 校准图像 `float32[H×W]` + `star_x/y` (掩膜) + `SnrNoiseModelConfig`
- 输出: `NoiseWeightModelV1` (patch variance空间场 + 全局兜底 + ivar + degenerate标志)

## 2 离散公式

```text
F1: σ_bg = 1.482602218505602 · median(|x−median(x)|)  (MAD→σ Gaussian)
F2: 5σ裁剪 ≤2轮: 剔除 |x−median|>5σ, 剩余求 σ_bg
F3: var(x,y)=a+b·x+c·y (LS平面, enable_spatial_field==1 && n_ctrl≥4 else 全局中位数)
F4: variance = max(var, 1e-12), ivar=1/variance
F5: g_model_floor[model*]=floor 指针隔离
F6: 诊断: var_ADU=max(signal,0)/gain + (rn/gain)² (仅 SNR-005, 不入生产)
```

来源: `noise_model.cpp:32-464` `default_config variance_floor=1e-12`

## 3 伪代码

```text
function snr_noise_model_v1(image, star_x/y, cfg):
  grid 8×8 patches
  for each patch:
    mask rmax=max(1,r0)·max(1,scale) 固定不按振幅
    vals = unmasked pixels; med=median(vals); mad=median(|vals−med|)
    sigma=1.4826·mad; 2轮内剔除 |v−med|>5σ → patch_variance
  if n_valid <4 or !spatial_field → global_median fallback
  else LS平面 a,b,c 最小二乘 patch_centers→variance
  g_model_floor[model*]=1e-12; for fill: var=max(a+b·x+c·y, floor)

function snr_noise_model_v1_free(model): g_model_floor.erase(model*)
```

## 4 边界/NaN/Inf

| 条件 | 行为 |
|---|---|
| 全星场无空 patch | NO_DATA fallback global |
| `MAD==0` | sigma==0 degenerate |
| `variance≤0` | max→1e-12 |
| 输入 NaN | skip |

## 5 确定性与归约

- patch独立 OpenMP 并行, median局部无跨patch归约；平面LS按patch索引固定顺序。

## 6 复杂度

- O(pixels) mask+median; LS O(8×8)

## 7 CPU-only 后端策略（V5）

- 仅 CPU：patch 网格(8×8)间独立，worker pool（按 affinity）patch 级并行，**禁止硬编码线程数**；平面最小二乘(3 参数)为全控制点固定序归约(FP64, 禁重结合)。

## 5c SIMD 安全与取消点

- patch 内 MAD/median 为排序选择(固定输入序)；`fill` 阶段 `ivar=1/max(var,floor)` 逐像素独立(SIMD 安全: 数组连续无别名)；`g_model_floor` 指针 key 注册表为单线程资源(锁自由设计, 跨线程不共享写)。
- 取消点: patch 网格按行带粒度检查; 取消时模型对象半成品作废(免费 `_free` 语义), 已写 fill 输出行带不回滚(调用方按返回码整帧重做)。

## 8 参考实现/Oracle

- MC Gaussian/Poisson/场恢复 SNR-004..006; 1/unc²对比

## 9 容差来源

- sigma 5% (MAD鲁棒性), floor 1e-12 预冻结。

## 10 关联 ARC/API/TST

- API: `snr_estimator.h: snr_noise_model_v1/_f64/_fill/_free`
- TST: `TEST-SCI-NOISE-*` MC矩阵

## 11 数据布局

- 输入校准图 `float32[H×W]` 行主序连续（就地, 不复制整图）；`star_x/y`：`double[n_star]`。
- patch 划分为 `8×8` 网格：逐 patch 就地 mask + median/MAD（不物化整图副本）；记录
  `(cx, cy, patch_variance)` 控制点数组（length ≤ n_ctrl ∈ {1..64}）。
- 平面场 `var(x,y)=a+b·x+c·y`：3 个 double 系数；`g_model_floor` = `std::map<void*,double>`
  （以模型指针为 key 的 floor 注册表, 无全局共享）。
- 输出 `NoiseWeightModelV1`：`variance_bg_global`(ADU²)、`has_spatial_field`、
  `degenerate` 标量 + 平面系数/全局兜底。
- 内存：主图 O(H·W)·4B 就地；patch/平面 O(64)/O(1)。无额外整图副本。

## 12 误差预算

- FP64 全链路；MAD 常数 `1.482602218505602`（15 位截断, 与 `1.4826022185` 差 <1e-12）。
- `σ_bg`：Gaussian 假设 + 污染/非高斯空背景 → 科学容差 **5%**（`SNR-004` oracle）。
- `5σ` 裁剪 ≤2 轮：抑制 cosmic/hot, 引入偏差 <~2%。
- 平面场 LS：已知梯度场恢复 `a,b,c` → **10%**（`SNR-006`）；负预测 clamp 到 `variance_floor`。
- `variance_floor=1e-12`：保证 `ivar` 有限; FP64 下数值相对误差 ~1e-15。
- 预算排序：**数值精度(FP64, ≪1e-12) ≪ 科学鲁棒性容差(5%/10%) ≪ 门禁阈值**。误差预算用于
  确定 oracle 容差与门禁, 不宣称覆盖科学不精确性（非 Gaussian/污染天光）。
- 各 F 步骤映射：`F1`→`noise_model.cpp:robust_sigma`（`SNR-004`）；`F3`→`snr_noise_model_v1`
  LS 平面（`SNR-006`）；`F4`→`_fill`（`SNR-002`/`TST-NOISE-INV-*`）。

## 13 P1-NOISE-DOC 模块合同冻结增补（实现锚定修订 / DISP-NOISE / TEST-NOISE-DESIGN-001）

> ID: ALG-NOISE-001..003  状态: CONTRACT_READY（P1-NOISE-DOC 冻结，2026-09-07，
> wave W1）  上游: SCI-NOISE-001..015（docs/science/NOISE_MODEL.md，FROZEN T104
> 2026-08-23，不改 SCI）  下游: DATA-P1-NOISE（DATA_SEMANTICS §13）/
> API-NOISE-001（PUBLIC_API）/ MOD-astrocs-phase1-noise-snr / TEST-NOISE-DESIGN-001。
> 本节由源码逐符号核对后追加（P1-NOISE-DOC）：§1-§12 为 T204/V5 既有登记，
> 根公式（MAD→σ、5σ≤2 轮、平面场、floor、ivar=1/variance）不变；本节登记
> 现行唯一生产实现逐符号锚、与旧节的实现事实差异修订、缺陷清单与测试设计。
> 禁止声明 IMPLEMENTED（迁移落码由 P1-NOISE-IMPL 执行）。禁止根据代码缺陷
> 反向修改 SCI——全部差异登记 DISP-NOISE-*。

### 13.1 逐符号实现锚定（现行唯一生产实现）

生产符号唯一源 = `lib/snr_estimator/cpp/src/noise_model.cpp`（466 行，头
`lib/snr_estimator/cpp/include/snr_estimator.h` 431 行；现状构建 =
`lib/snr_estimator/cpp/Makefile:5,12`（g++ -shared → `snr_estimator.dll`，
MinGW 通道）+ `cpp/build.ps1:29`——未编入根 CMake 主构建（无
snr_estimator CMake 目标，与 astrocs_hips/astrocs_drizzle 先例不同，
CMake 集成归 P1-NOISE-IMPL），dll_loader.cpp:41/55 加载名与路径吻合）；`lib/phase1/noise/noise_model.{h,cpp}` 为
`astrocs::phase1::NoiseModel` 小封装（39+67 行，静态库
`astrocs_phase1_noise`，CMakeLists.txt:490-493，主程序链接 :556；单测
tests/unit/p1_noise_test.cpp 经 tests/unit/CMakeLists.txt:312-316 注册）。

| ALG | 符号 | 源锚 |
|---|---|---|
| ALG-NOISE-001 | `noise_model_impl`（模板 f32/f64 内核） | noise_model.cpp:118-267（参数校验 :112、`g_model_floor` 注册 :126、fixed conservative 掩膜 :128-157、patch 网格循环 :165-201、全局兜底 :206-239、控制点数组 :241-278） |
| ALG-NOISE-001 | `snr_noise_model_v1` / `snr_noise_model_v1_f64`（C ABI 门面，extern "C" 异常屏障→rc 3） | noise_model.cpp:348-364 |
| ALG-NOISE-001 | `snr_noise_model_v1_default_config` | noise_model.cpp:333-346（默认 8×8/r0=10/scale=6/clip 5.0/min 64/rounds 2/spatial 1/floor 1e-12） |
| ALG-NOISE-001 | `robust_median` / `robust_sigma`（1.482602218505602·MAD）/ `collect_patch_sky`（掩膜+饱和过滤+5σ≤2 轮裁剪） | noise_model.cpp:42-52,55-62,72-108 |
| ALG-NOISE-002 | `fill_impl`（平面 LS var(x,y)=a+b·x+c·y，负预测 clamp floor；否则全局常量）+ `snr_noise_model_v1_fill` | noise_model.cpp:371-429（LS :340-370，floor 回退 :402-405，fill 门面 :411-422） |
| ALG-NOISE-002 | `snr_noise_model_v1_free`（free ctrl 数组 + 按指针擦除 g_model_floor） | noise_model.cpp:431-445 |
| ALG-NOISE-002 | `snr_noise_scale_law`（x'=αx → var'=α²var, ivar'=ivar/α²） | noise_model.cpp:447-454；snr_estimator.h:173-175 |
| ALG-NOISE-003 | `snr_noise_gain_variance`（var_ADU=max(signal,0)/gain+(rn/gain)²，诊断不入生产） | noise_model.cpp:456-464；snr_estimator.h:178-180 |
| （同头三层其余） | `snr_phot_cal_quality`（dex/mag/rel 换算 :67-79）、`snr_psf_fit_quality`（residual_scale/0.7316727929211932、q_psf=A/residual_scale :296-334） | 属 P1-PHOT/PSF 合同视角引用，本模块不重复冻结 |

返回码合同（三函数一致）：`0`=成功（含 degenerate=1 全局兜底成功）；`1`=
完全退化（ivar_bg_global=0.0，调用方拒绝加权，noise_model.cpp:225-233）；
`3`=nullptr/尺寸非法/内部异常（门面 try/catch 屏障）。

### 13.2 与 §1-§12 既有登记的实现事实差异修订（不改动根公式）

- `g_model_floor` 实际为 `std::unordered_map<const NoiseWeightModelV1*,double>`
  （noise_model.cpp:32），§2 所写 `std::map<void*,double>` 为旧登记——语义
  （model 指针 key、无全局共享）不变，容器与 key 类型以本节为准。
- `min_patch_samples` 默认 **64**（snr_estimator.h:112、default_config :333），
  SCI §4 "min_samples 默认 5" 为旧稿数字——**不改 SCI**，以代码为准登记；
  patch 合格阈即 64。
- 掩膜统一半径 rmax = max(1, source_mask_radius_px)·max(1, mask_radius_scale)
  = 默认 10·6 = **60 px**（noise_model.cpp:144-147），对所有星统一，不按
  振幅/星等缩放（API 无 amplitude 输入）；`amps` 向量残留未用（:139,146
  `(void)amps`）。
- 取消检查点：§5c "patch 行带粒度检查取消"为计划语义，现状
  noise_model_impl/fill_impl **无** cancel 回调（DISP-NOISE-004）。
- §11 "n_ctrl ∈ {1..64}"：n_qualified_patches = 合格 patch 数 ∈ {0..64}
  （8×8 网格上限 64）；<4 时 has_spatial_field=0（:276-277）。
- 线程：§5/§5c "OpenMP 并行"为计划语义——noise_model_impl 现状**单线程
  顺序**（无 omp pragma；patch 循环 :165-201 串行），与
  astrocs.p1.noise-snr descriptor parallel_ok 并行轴为迁移整改点。

### 13.3 缺陷清单（DISP-NOISE，登记不改码）

均为**现行实现事实**，迁移整改归 P1-NOISE-IMPL/INT；本 DOC 阶段禁止据此
修改生产代码。编号与 SCI §10"不可接受变化"独立（SCI 层禁令不重复）。

| ID | 缺陷（现状事实） | 锚 |
|---|---|---|
| DISP-NOISE-001 | `g_model_floor` 为进程级 unordered_map、以原始指针为 key：`_free` 后同址复用（ABA）可命中旧 floor 值；多线程并发 build/free 对该 map 无锁竞争（PHASE1_API_V1 §2 threadsafe=yes 以"model 对象隔离"为前提，本条即该前提的实现边界） | noise_model.cpp:32,126,433 |
| DISP-NOISE-002 | build 与 fill 两阶段 floor 语义不一致：build 内 `max(vmed, cfg->variance_floor)`/`max(patch_var, floor)` 在 floor<=0 时**不 clamp**（原值直通），fill 阶段 floor<=0 静默回退 1e-12——同一 cfg 两阶段下界不同 | noise_model.cpp:210,256 vs 400-404 |
| DISP-NOISE-003 | `SnrNoiseModelConfig.gain_e_per_adu/read_noise_e/use_gain_model` 三字段在 noise_model_impl 中**零读取**：use_gain_model=1 无任何效果，gain 模型融合路径无实现（与 SCI §10 不融合禁令一致，但字段存在暗示可选路径，易误用） | snr_estimator.h:103-104,109；noise_model.cpp:119-124 |
| DISP-NOISE-004 | 无取消检查点：noise_model_impl/fill_impl 均无 cancel 回调（§5c 为计划语义） | noise_model.cpp:118-267,371-429 |
| DISP-NOISE-005 | `snr_noise_scale_law` 无参数校验：alpha=NaN/Inf/负值未拒绝，variance 无条件乘 α²（NaN 直传）；ivar 仅在 a2>0 且有限时更新——variance 与 ivar 在非法 alpha 下可失去互倒关系 | noise_model.cpp:447-454 |
| DISP-NOISE-006 | `source_mask` 与 `star_x/y` 掩膜通道互斥：source_mask 非 NULL 时完全忽略 star 坐标（含 n_stars>0）；source_mask 长度不单独校验（信任 h·w 布局） | noise_model.cpp:134-163 |
| DISP-NOISE-007 | 参数下限静默钳位无返回码区分：patch_grid<2→2、clip_sigma<1→1、min_patch_samples<1→1、max_clip_rounds<0→0（调用方不可知被钳位） | noise_model.cpp:166-170 |
| DISP-NOISE-008 | 整除划分 patch 网格在小图出现空 patch（x0==x1），几何空 patch 与质量拒绝混入同一 `n_rejected_patches` 计数，语义不区分 | noise_model.cpp:179-204 |
| DISP-NOISE-009 | 控制点 malloc 失败路径 return 3 前 `g_model_floor[out_model]` 已注册：调用方忽略 rc=3 不调 `_free` 则注册表条目泄漏（与 001 同根） | noise_model.cpp:126,244-254 |

### 13.4 测试设计 TEST-NOISE-DESIGN-001（冻结容差）

> 可执行测试由 P1-NOISE-TEST 建立（TEST-P1-NOISE-001 目标 ID）；本节冻结
> fixture/oracle/容差，P1-NOISE-TEST 不得放宽。全离线合成数据（固定 seed
> 生成），零真实数据依赖。负面行必须逐条断言（ALG §4/§13.3 + SCI §7/§8）。
> 既有容差锚：σ 5%（SNR-004）、平面场 10%（SNR-006）、Poisson 诊断交叉 5%
> （SNR-005）、NumPy 参考 rtol 1e-9（SCI §11）——本节不得放宽。

- **FIX-NOISE-A Gaussian 合成**: `N(0,σ²)` 空背景帧（σ=5 ADU，固定 seed
  ≥4 组独立 seed）→ `sigma_bg_global` 相对真值 ≤5%（SNR-004 oracle；
  NumPy median/MAD 复算 rtol 1e-9 对照同帧）。
- **FIX-NOISE-B 平面场恢复**: 注入 var(x,y)=a+b·x+c·y（b,c 非零、覆盖
  正/负梯度方向）→ 拟合系数在 10% 内复现（SNR-006）；负预测像素
  variance==floor（1e-12）且 ivar==1e12（逐位断言 clamp 行为）。
- **FIX-NOISE-C 常量场退化**: 常量输入 C → MAD=0 → 全部 patch 拒绝 →
  全帧兜底路径 sigma=0 → rc=1、degenerate=1、ivar_bg_global==0.0
  （bitwise 断言零值，不产生伪权重）。
- **FIX-NOISE-D 掩膜解耦**: 亮星（振幅 10⁴）与暗星（振幅 10¹）同坐标
  掩膜 → source_mask 逐位相同（rmax 与振幅无关；star 通道与手工
  source_mask 通道结果一致）。
- **FIX-NOISE-E Poisson 诊断交叉**: 已知 gain/rn 合成帧 →
  `snr_noise_gain_variance` 与 var_th=μ/gain+(rn/gain)² 解析式逐位一致；
  与经验 variance_bg_global 相对差 ≤5%（SNR-005）；**并断言**
  use_gain_model=1 不改变生产输出（DISP-NOISE-003 现状：字段无效）。
- **FIX-NOISE-F scale law**: α∈{0.5,2,10} → var'=α²var、ivar'=ivar/α²
  逐位；回乘 α·(1/α) 恒等。负面：alpha=NaN → 按 DISP-NOISE-005 断言
  **现状行为**（variance=NaN 直传），不得断言"已校验"。
- **FIX-NOISE-G fill 语义**: has_spatial_field=0（n_ctrl<4）→ 输出全场
  常量 variance_bg_global/ivar_bg_global；n_ctrl≥4 → 平面像素值与 NumPy
  独立 LS 复算 rtol 1e-9；out_variance/out_ivar 任一可 NULL（可空输出）。
- **不变量 I1-I6**: ①ivar=1/variance 精确互倒（fill 后逐像素）；
  ②variance≥floor 处处成立；③degenerate=1 ⇒ ivar_bg_global==0.0；
  ④确定性：同输入同 cfg 两次运行 bitwise 一致（1/2/4 线程一致——现状
  单线程，迁移后加并行复验）；⑤n_qualified+n_rejected==64（8×8）；
  ⑥free 后再 free 不崩（幂等）。
- **负面/参数矩阵**: data/out NULL、h/w≤0 → rc=3；cfg=NULL → 默认配置
  rc=0；source_mask 全 1（无 sky）→ rc=1；NaN/Inf 像素 → 过滤不计入
  （valid_pixel）；饱和电平以上像素排除；star 坐标 NaN → 跳过该星。
- **串并行/资源**: O(h·w) 时间、O(h·w) 掩膜 + O(64) 控制点内存界断言；
  现状单线程（§13.2），P1-NOISE-IMPL 引入并行后按 ④ 复验。
- **ISA**: 基线标量断言（无 SIMD 变体；引入时按约束 C.4-C.8 逐内核
  benchmark 冻结 ULP 容差）。

### 13.5 遗留通道与迁移旧符号（非生产）

- `lib/phase1/noise/NoiseModel::estimate`（median+MAD 全像素集单值，无
  掩膜/patch/平面场）与 `NoiseModel::gain_variance`（signal/gain+rn²
  诊断）——P1-005 期封装，语义为 ALG-NOISE-001/003 的退化子集；随
  `astrocs_phase1_noise` 静态库编译（CMakeLists.txt:490-493）并进主程序
  （:556），属计划迁移旧符号：P1-NOISE-IMPL 决定改写为
  snr_noise_model_v1 薄封装或退出，去留登记其 TASK_RESULT。
- 旧乘法 SNR 通道 `snr_estimate/snr_estimate_f64/snr_extract_model{,_v2,_v3}`
  （snr_estimator.h:200-218,394-428）——legacy heuristic/diagnostic，已由
  三层模型降级（头注释 :19-20）；不属 P1-NOISE 合同（SNR catalogue 语义
  归 P1-SNR/DRZ 侧），仅登记边界。
- 迁移落点: `lib/snr_estimator;lib/phase1/noise`（matrix legacy_paths）→
  `astrocs_p1_noise.dll`（P1-NOISE-IMPL 建 C ABI adapter +
  plan/execute/cancel/inspect + ThreadLease 接线）；本 DOC 不改任何
  生产代码。
