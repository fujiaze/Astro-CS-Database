# Calibration Algorithms (ALG-CAL)

> ID: ALG-CAL-001  范围: ALG-CAL-001..006  上游 SCI: SCI-CAL-001  状态: CONTRACT_READY（P1-CAL-DOC 冻结，2026-09-07）  模块: lib/calibration（astrocs.p1.calibration / 目标 DLL astrocs_p1_calibration.dll）
>
> 本文档由源码逐函数核对后全面重写（P1-CAL-DOC，wave W1）。连续数学定义以
> `docs/science/CALIBRATION.md`（SCI-CAL-001，FROZEN）为唯一权威；本文只做
> 离散化与实现事实登记，**禁止**以本文反向修改 SCI。旧版本文档中的黄金分割
> 搜索、ISA benchmark 注册、取消检查点等陈述与现行源码不符，已删除。

## 1 上游 SCI 与模块边界

- 上游: `SCI-CAL-001`（docs/science/CALIBRATION.md，FROZEN）——§5 连续定义
  （dark_opt 双分支 + flat_norm median=1.0 / floor 0.1）、§9a 专属问题
  （无 pedestal、无 gain、无 read-noise 建模、负值保留、bad_mask 极性 1=坏点、
  variance 不传播）。
- 生产源: `lib/calibration/include/astro_calibration.h`（唯一公共头，14 个
  `AC_API` 符号）+ `lib/calibration/src/master_generator.cpp`、
  `lib/calibration/src/calibrator.cpp`、`lib/calibration/src/cosmetic_corrector.cpp`、
  `lib/calibration/src/ac_api.cpp`（CMake `astrocs_calibration` 静态库唯一构建
  清单，CMakeLists.txt:321-333）。
- 负责: master bias/dark/flat 生成（sigma-clip 合并）、单帧校准算术、
  热像素/冷像素检测与插值修复；Gaia 测光比例标量应用（现状未接线）。
- 不负责: FITS/XISF 文件读写（astro_image_io，调用方侧）、母版按曝光/滤镜
  分组与匹配（orchestrator/调用方）、K=t_light/t_dark 的计算（调用方从
  EXPTIME 得出后传入）、噪声方差/SNR 估计（snr_estimator）、天光背景扣除
  （Phase2）、宇宙线剔除（叠加 rejection）、WCS/测光定标、整 Phase 编排。

## 2 连续定义（SCI-CAL-001 §5 转述，权威以 SCI 为准）

```text
dark_opt=0（默认，Dark 已含 Bias）:
  cal = (raw − dark) / flat_norm
dark_opt=1（显式 Bias/Dark 分离，K=t_light/t_dark）:
  cal = (raw − bias − K·(dark − bias)) / flat_norm
flat_norm = max(flat / median(flat), 0.1)   （median<=0 时不归一，保持原样）
```

离散实现中 `flat/median(flat)` 归一发生在 master flat 生成期
（ALG-CAL-002 步骤 3）；`calibrate`（ALG-CAL-003）对入参 flat 仅施加
floor 0.1，即约定入参 master_flat 已是 median≈1.0 的归一化平场。

## 3 离散公式与伪代码（与源码逐一锚定）

### 3.1 ALG-CAL-001 MasterBias/MasterDark 生成 `ac::generate_master`

源码锚: `master_generator.cpp:67-164`。输入 `stack[n_frames][h][w]` float32
行主序，输出 `out[h][w]`。

```text
F1.1  n_frames==1: out = stack[0]（直接拷贝，不做 sigma-clip，NaN 原样保留）
      [master_generator.cpp:84-88]
F1.2  每像素 idx（OpenMP parallel + for，线程本地 vals[n_frames]/work 缓冲
      复用）[master_generator.cpp:91-97]:
        vals[n] = stack[n*npix + idx]
F1.3  sigma-clip 迭代 iter=0..max_iter-1 [104-133]:
        work = {v∈vals | !isnan(v)};  work 空 → break
        med  = median(work)                      # nth_element，偶数取双中位均值
        mad  = median({|v−med| | !isnan(v)})     # work 复用（clear 后重填）
        σ    = 1.4826·mad                        # k_sigma [76]
        σ<=0 → break                             # 无离散度 [120]
        v=NaN 当 dev<−sigma_low·σ 或 dev>sigma_high·σ   # 非对称阈值 [127]
        本轮 rejected==0 → break                 # 收敛 [132]
F1.4  合并 [136-157]:
        combine==AC_COMBINE_MEDIAN(1): out = median(非 NaN)；全 NaN → NaN
        combine==AC_COMBINE_MEAN(0):   cnt=|非 NaN|;
          cnt>0: out = (Σ_{k=0..n_frames-1} vals[k]·[!isnan]) / cnt
                 # std::accumulate 按帧下标升序 FP32 累加，顺序冻结 [149-152]
          cnt==0: out = NaN
```

中位数定义（全文档共用）: `median(v)`=nth_element 选择；n 奇取 `v[n/2]`，
n 偶取 `(v[n/2−1]+v[n/2])/2`（`median_inplace`/`median_of`，如
`calibrator.cpp:45-59`、`master_generator.cpp:49-60`）。

### 3.2 ALG-CAL-002 MasterFlat 生成 `ac::generate_master_flat`

源码锚: `master_generator.cpp:176-259`。无 combine 参数，合并固定 mean
（SCI-CAL-001 §12 ALG-CAL-002 语义一致）。

```text
F2.1  步骤1（每帧 n，OpenMP parallel for）[194-225]:
        dst = flat_stack[n] − bias          # bias==NULL 时拷贝
        frame_med = median(dst)
        isnan(frame_med) || frame_med==0.0 → frame_med = 1.0   # [210]
        frame_med < 0.0 → 返回 AC_ERR_PARAM, out 不写            # [213-217]
        dst = max(dst / frame_med, 0.1)     # 逐帧归一 + floor
F2.2  步骤2: generate_master(norm, …, AC_COMBINE_MEAN)   # [229-230]
F2.3  步骤3（最终归一）[234-252]:
        final_med = median(out);  isnan||==0 → 1.0    # [237]
        final_med < 0.0 → 返回 AC_ERR_PARAM, out 不写  # [240-244]
        out = max(out / final_med, 0.1)
```

> **OWNER-04 登记（B2-A6，不反向改 SCI）**：F2.1/F2.3 的"负 median 拒绝"
> （B13-R13-7 实现）与 SCI-CAL-001 §4/§5/§8 对 `median<=0` 的"不归一、
> 保持原样"文本不一致；按 ANCHOR_CONTRACT §2.3 只登记、不改 SCI 原文，由
> 负责人在"改代码"与"按宪章 §1.2 改 SCI 文本"间裁决。
>
> **B2-A6 消费边界（冻结口径）**：P1 校准节点 `p1_op_calibrate`
> （`module_adapters.cpp:1153-1179,1230-1240`）在进入 `ac_calibrate_frame`
> 前对 master flat 做**整帧退化判定**：全零 / median<=0 / 任一非有限像素 →
> DATA 拒绝（CLI rc=2）、不写 `calibrated_*`、不写 complete manifest。
> 校准公式、单位、暗场缩放与逐像素 floor 0.1 均不变。

### 3.3 ALG-CAL-003 单帧校准 `ac::calibrate` / `ac::calibrate_d`

源码锚: `calibrator.cpp:106-138`（float）、`calibrator.cpp:149-181`
（double，逻辑逐行一致）。OpenMP parallel for schedule(static)。

```text
F3.1  dark_opt==1 && bias && dark（K=k_init 直通，非搜索）[117-124]:
        out[i] = (light[i] − bias[i] − k·(dark[i] − bias[i])) / max(flat[i], 0.1)
        （flat==NULL 时跳过除法）；actual_k = k_init
F3.2  否则（标准模式）[125-135]:
        k = 1.0
        out[i] = (light[i] − dark[i]·[dark!=NULL]) / max(flat[i], 0.1)·[flat!=NULL]
        actual_k = 1.0
F3.3  参数无效（!light||!out||w<=0||h<=0）: actual_k=k_init，静默返回，
      out 不写 [109-112]；C API 层先校验并返回 AC_ERR_PARAM（ac_api.cpp:100-101）。
```

- `normalize_flat`（`calibrator.cpp:80-95`，median→1.0 + floor 0.1，
  med<=0 原样返回）：**当前无任何生产调用方**（ac_api 不转发，模块内无
  调用）。median→1.0 归一由 ALG-CAL-002 在 master 生成期承担；登记为
  未接线辅助符号（DISP-CAL-007）。
- `compute_mad`（`calibrator.cpp:66-76`）：与 cosmetic 的
  `compute_global_mad` 同义，供 C++ 内部使用，公共头无声明。

### 3.4 ALG-CAL-004 坏点检测/修复 `ac::detect_hot_pixels` / `ac::detect_cold_pixels` / `ac::filter_by_structure_size` / `ac::interpolate_pixels` / `ac::correct_frame`

源码锚: `cosmetic_corrector.cpp:61-265`。

```text
F4.1  热像素 [118-134]: med=median(dark 全帧)；mad=median(|dark−med|)；
        σ=1.4826·mad；hot = (dark > med + hot_sigma·σ)
      冷像素 [139-155]: cold = (bias < med − cold_sigma·σ)
      # 阈值统计为单线程 O(n)；**不过滤 NaN**（NaN 参与 nth_element，
      # 行为不可靠——负面测试覆盖，DISP-CAL-004）
F4.2  结构过滤 [61-113]: 8 连通 BFS 标记；size(label)>=max_size 的连通域
      掩码清 0（保留 <max_size 结构以排除星点）；背景 label 0 不参与。
F4.3  修复 [160-225]:
      method==AC_METHOD_MEDIAN(0): 5×5 邻域镜像反射索引（nx<0→−nx;
        nx>=w→2w−nx−2，再 clamp 到 [0,w−1]）[178-189]，仅收非 bad 邻居，
        空 → 原值，否则 median(≤25)
      method==AC_METHOD_BILINEAR(1): **实际为 4 方向（上/下/左/右）最近
        非 bad 像素距离反比加权（IDW）**: 沿方向步进 while bad，dist 步数，
        weight=1/dist，out=Σ v·w/Σ w；方向出界跳过；全方向无 → 原值
        [202-222]。命名 bilinear 与实现不符（DISP-CAL-003）。
F4.4  correct_frame 主入口 [229-265]: !data||!out||n<=0 → 静默返回；
        dark&&hot_sigma>0 才检测热、bias&&cold_sigma>0 才检测冷
        （sigma<=0 = 禁用该类检测）；all_bad = hot||cold；
        out_hot/out_cold = **结构过滤后**的掩码像素计数 [252-258]；
        掩码合并/统计为单线程 O(n) 循环。
```

### 3.5 ALG-CAL-005 最优 Dark 系数估计 `ac::optimize_dark_k`（非 SCI 范围，未接线）

源码锚: `dark_optimizer.cpp:97-364`。模型 `L − B = c + k·(D − B)`；
规范依据 `02_FROZEN_STAGE1_HISS_SPEC §2.3`（engineering 控制，非
docs/science；SCI-CAL-001 明确 K=t_light/t_dark，本算法为该回退值之外的
可选估计器，**未纳入 SCI 合同**）。

```text
F5.1  背景提取: |light−median| <= 3·1.4826·MAD 全帧阈值；MAD=0 → 全部
      视为背景 [125-142]
F5.2  8×8 分区抽样: 每区 ≤1000 背景（等步长跨步），总样本 ≤50000
      [144-207]
F5.3  鲁棒回归: OLS（double 累加）+ 残差 3·1.4826·MAD 离群抑制，≤5 轮
      [219-306]
F5.4  失败→回退 k_init 且 diagnostics.fell_back=1, fallback_from=
      "OPTIMAL", fallback_to="EXPOSURE_RATIO"；诊断码: PARAM、BAD_K_INIT、
      INSUFFICIENT_SAMPLES(<100)、TOO_FEW_INLIERS(<50)、ZERO_VARIANCE
      (denom<=1e-12)、RESIDUAL_NAN、RESIDUAL_ABNORMAL(残差σ≥yσ)、
      K_OUT_OF_RANGE(k<=0 或 >10)、OK [108-118,211-217,240-256,297-302,
      311-355]
```

**现状**: `dark_optimizer.cpp` 不在 CMake `astrocs_calibration` 构建清单
（CMakeLists.txt:321-325），全仓无调用方；`hiss::Stage1Diagnostics` 来自
`lib/astro_image_io/include/hiss_format.h`。登记为计划迁移符号
（P1-CAL-IMPL 决定接线或删除），不声明任何生产语义（DISP-CAL-005）。

### 3.6 ALG-CAL-006 Gaia 测光比例应用 `calibration::apply_photometry`（非 SCI 范围，未接线）

源码锚: `photometry_apply.cpp:29-65`、契约 `photometry_apply.h`。规范依据
`02_FROZEN_STAGE1_HISS_SPEC §7`（I_photo = k_photo·I_cal，Drizzle 前应用）。

```text
F6.1  out[i] = (float)((double)light[i] · photscal)   # double 乘法防大
        动态范围精度损失 [58-60]
F6.2  错误码: −1 light==NULL、−2 out==NULL、−3 w/h<=0、−4 photscal 非有限
        [32-47]；0=成功
F6.3  NaN/Inf 透传: NaN·k=NaN；±Inf·(k>0)=±Inf、·(k<0)=∓Inf、·(k=0)=NaN；
        下游 Drizzle 跳过非有限像素 [54-57 注释]
F6.4  in-place 安全（逐元素无依赖）；每次调用向 stderr 输出两行日志
        [51,62]
```

**现状**: 不在 CMake 构建清单，无生产调用方；`tests/
test_photometry_apply.cpp` 为其共址测试（同样未挂接 CMake 测试目标）。
k_photo 的来源（Gaia 光谱积分定标）不在本模块（登记 DISP-CAL-006）。

## 4 实现事实（源码核对）

| 事实 | 内容 | 锚 |
|---|---|---|
| 公共符号 | 14 个 `AC_API`：`ac_generate_master_{bias,dark,flat}`、`ac_calibrate_frame`、`ac_correct_frame` 及 5 个 `_f64` 变体、`ac_set_num_threads`、`ac_version` | astro_calibration.h:33-156 |
| C++ 接口 | `ac::optimize_dark_k`（头文件声明但无 AC_API 导出宏，dark_optimizer.cpp 未编译） | astro_calibration.h:168-179 |
| 错误码 | AC_OK=0、AC_ERR_PARAM=−1、AC_ERR_MEMORY=−2、AC_ERR_INTERNAL=−3；**−2/−3 从未返回**（见 DISP-CAL-001） | astro_calibration.h:21-24 |
| FP64 ABI | 仅 `ac_calibrate_frame_f64` 真双精度（calibrate_d）；`ac_generate_master_*_f64`、`ac_correct_frame_f64` 将 double 输入 `static_cast<float>` 走 f32 实现后转回 double（统计/mask 路径降级，头文件 105-115 声明） | ac_api.cpp:147-263 |
| 输出 dtype/shape | f32 ABI: float32 `[h][w]` 行主序（idx=y·w+x，0-based）；f64 ABI: double 同 shape；stack: `[n_frames][h][w]` 连续 | 各 C API 注释 |
| 单位 | 全部 ADU；flat_norm/σ 参数/K 无量纲；曝光秒仅在调用方算 K 时出现；坐标 0-based 像素、无 WCS | SCI-CAL-001 §3/§3a |
| 掩码极性 | bad/hot/cold 掩码 1=坏点（char/uint8） | cosmetic_corrector.cpp:130,151 |
| NaN 语义 | generate_master 统计跳过 NaN、全 NaN→输出 NaN；calibrate/cosmetic 阈值统计**不**过滤 NaN（NaN 算术直传/阈值不可靠） | master_generator.cpp:106-118；cosmetic_corrector.cpp:45-54 |
| 日志 I/O | generate_master/flat 每次调用 2 行 stderr（ac_log）；apply_photometry 2 行 stderr；无文件/网络 I/O | master_generator.cpp:38-45 |
| 内存 | 输出缓冲调用方分配；模块内 std::vector RAII。峰值额外内存: generate_master O(n_frames/线程)；generate_master_flat O(n_frames·npix·4B)（norm 主缓冲）；calibrate O(1)；cosmetic O(npix)（labels+masks+统计副本）；f64 转接层 O(n_pix) 全帧复制 | 各源文件 |
| 构建 | CMake 目标 `astrocs_calibration`（STATIC，4 个 cpp，OpenMP 可选）；遗留 MinGW 通道: build.ps1（astro_calibration.dll）、Makefile（cpp/ 版 cosmetic_corrector.dll，cc_* 4 导出，window 奇数 3..15） | CMakeLists.txt:373-380；lib/calibration/Makefile |
| 生产调用方 | `lib/phase1_session/p1_session.cpp:243` 仅调 `ac_calibrate_frame`（master 由配置传入，帧粒度取消在 session 层）；master 生成与 cosmetic 的 ac_* 入口当前无生产调用方 | p1_session.cpp:200-270 |

### 4.1 遗留双实现：`cpp/cosmetic_corrector.cpp`（cc_* 通道）

与 `src/cosmetic_corrector.cpp` 是**两套独立实现**：cc_* 由 Makefile 编译为
独立 DLL（Python ctypes 通道），支持参数化窗口 `cc_correct_median(data,
bad_mask,H,W,window)`（window 奇数 3..15，偶数/<3/>15 返回 −1，15×15 栈
缓冲 256 上限）、`cc_detect_hot/cc_detect_cold`（double sigma）返回计数、
`cc_last_error()`。不在 CMake 构建内，属计划迁移旧符号（P1-CAL-IMPL 决定
去留）；`docs/modules/calibration.md` 中 "window 偶数/<3/>15 → −1" 即指此
通道，非 ac_correct_frame。

## 5 复杂度

- `generate_master`: 时间 O(max_iter·n_frames·npix)（median O(n_frames)
  选择 ×2/迭代）；空间 O(n_frames) 每线程 + 输出 O(npix)。
- `generate_master_flat`: 时间 O(n_frames·npix)；空间 O(n_frames·npix)
  （norm 缓冲，主导项）。
- `calibrate/calibrate_d`: 时间 O(npix) 单 pass；空间 O(1)。
- `detect_*`: 阈值统计 O(npix)；`filter_by_structure_size` O(npix)
  （BFS+计数）；`interpolate_pixels` O(25·bad)；`correct_frame` 合计
  O(npix)。
- `optimize_dark_k`: O(npix) 背景 + O(50000·5) 回归。
- `apply_photometry`: O(npix)。

## 6 确定性与归约（修正旧文档）

- **线程模型（现状）**: OpenMP 默认 team（`omp_get_max_threads()`）；
  `ac_set_num_threads` 以 `n>0` 调用时全局改写 OpenMP ICV（进程级副作用，见
  DISP-CAL-002；API-P1-001 §2 已标注 V5 迁移整改点=由 p1 budget 注入取代）。
  无 ThreadLease、无 host executor 接线——迁移目标见 §8。
- **逐像素独立**: 校准/检测/插值均无跨像素归约；输出 bitwise 与线程数
  无关（schedule(static) 行块划分不影响单像素算术）。
- **归约顺序冻结**: mean 合并 = 帧下标升序 FP32 串行累加
  （master_generator.cpp:149-152）；中位数为 nth_element 选择（非常规
  归约，同输入同结果）。回归 OLS 为 double 累加（dark_optimizer.cpp:227-238）。
- **无取消检查点**: 模块内全部函数不检查取消（PHASE1_API_V1 §2 标注
  calibrate "取消点=行带" 为 session 编排目标，当前由 phase1_session 在
  帧间检查实现，模块内无实现——迁移差距见 DISP-CAL-008）。
- **无 ISA 变体**: 无 SIMD 显式向量化、无逐内核 benchmark 注册、无
  provider 选择；编译期仅 `-O2/-O3` 级优化与 OpenMP。旧文档 §5b/§7 的
  ISA 注册陈述与源码不符，已删除。

### 6.1 SIMD 安全（现状：无 SIMD 显式向量化，合同见约束 C.4-C.8）

- **fast-math**: CMake 主构建不加 fast-math；遗留 build.ps1/Makefile 通道
  使用 `-O3 -march=native -ffast-math`（仅遗留通道，不作为确定性合同依据）。

## 7 边界 / invalid / 退化行为（实现如实）

| 条件 | 实现行为 | 锚 |
|---|---|---|
| 空指针 / n_frames<=0 / w<=0 / h<=0（C API 入口） | 返回 AC_ERR_PARAM，不写 out | ac_api.cpp:60-61,72-73,86-87,100-101,115-116 及 f64 对应 |
| ac:: 层参数无效（void 函数） | 静默返回，out 不写，actual_k=k_init | calibrator.cpp:109-112；cosmetic_corrector.cpp:236 |
| median(flat)<=0（normalize_flat） | 不归一保持原样（SCI §4/§5/§8 文本；与 master_generator 的"拒绝"分歧登记 OWNER-04） | calibrator.cpp:86 |
| frame_med/final_med 为 0 或 NaN（master flat） | 置 1.0（不缩放） | master_generator.cpp:210,237 |
| frame_med/final_med < 0（master flat） | 返回 AC_ERR_PARAM，不写 out（B13-R13-7） | master_generator.cpp:213-217,240-244 |
| flat 帧含 NaN（master flat 步骤1 帧 median） | `median_of` 不剔除 NaN（nth_element 含 NaN 属未定义序；SCI §4"median 跳过 NaN"仅在 generate_master 逐像素路径实现）→ 夹具不得依赖该路径 | master_generator.cpp:207-209,49-60 |
| master flat 全零 / median<=0 / 非有限（p1_op_calibrate 消费边界） | DATA 拒绝（CLI rc=2），不进入 calibrate、不写 calibrated_*（B2-A6 fail-closed） | module_adapters.cpp:1153-1179,1230-1240 |
| flat==NULL（calibrate） | 跳过除法，退化减法 | calibrator.cpp:122,132 |
| dark==NULL（calibrate 标准分支） | out=light（flat 处理后） | calibrator.cpp:131 |
| dark_opt=1 但 bias/dark 缺一 | 回退标准分支且 k=1.0 | calibrator.cpp:117,127 |
| σ=0（generate_master） | 提前终止不剔除 | master_generator.cpp:120 |
| 单帧 master | 直接拷贝不做 clip | master_generator.cpp:84-88 |
| 全 NaN 像素列 | median 路径 NaN；mean 路径 cnt=0 → NaN | master_generator.cpp:141,155 |
| hot_sigma/cold_sigma<=0 | 对应类检测禁用 | cosmetic_corrector.cpp:242,247 |
| 坏点邻域全坏（median 5×5） | 保持原值 | cosmetic_corrector.cpp:196-197 |
| 4 方向均无好像素（IDW） | 保持原值 | cosmetic_corrector.cpp:218-221 |
| 输入含 NaN（calibrate/cosmetic 统计） | 算术直传/统计不可靠（负面测试覆盖） | DISP-CAL-004 |
| w·h 溢出 int31 | 无防护（npix=w·h int 乘法），≥2^31 像素 UB | DISP-CAL-009 |
| extern "C" 异常屏障 | 无 try/catch；std::bad_alloc 可穿越 C 边界（UB）；AC_ERR_MEMORY/INTERNAL 为死值 | DISP-CAL-001 |

## 8 迁移合同（P1-CAL-IMPL 目标，不声明已完成）

- 目标模块边界: `astrocs.p1.calibration` / `astrocs_p1_calibration.dll`
  （MODULE_MIGRATION_MATRIX.csv P1-CAL 行；DLL 当前不存在，现状产物为
  CMake 静态库 `astrocs_calibration` 与遗留 MinGW DLL）。
- 三方一致: `acs_module_descriptor_v1`（include/astrocs/abi/module_api_v1.h:40-53，
  字段 module_id/sci_id/alg_id/api_id/execution_class/parallel_ok）与
  module.yaml、运行 manifest 一致校验（12 号标准 §5）。descriptor 取值:
  sci_id=SCI-CAL-001、alg_id=ALG-CAL-001、api_id=API-P1-001、
  execution_class=cpu_heavy、parallel_ok=1。
- plan/execute/cancel/inspect 语义: plan=装配 master 路径/校准参数与
  资源分类（heavy）；execute=逐帧 ac_calibrate_frame 等价算术（内部并行
  须经 host ThreadLease，废除 ac_set_num_threads 全局 ICV）；cancel=帧
  粒度（现 phase1_session 模式收编为模块合同）；inspect=manifest 输出
  actual_k/帧计数/资源时序。C ABI adapter 禁止 STL/异常/RTTI 跨界
  （约束 F.3）。
- 配置 schema（现状为 C 参数直传 + phase1_session JSON）:
  `master_bias/master_dark/master_flat`（路径或 NULL）、`input_lights[]`、
  `dark_optimization: bool`（默认 false）、`dark_scale_factor: float`
  （默认 1.0，即 k_init=t_light/t_dark 由调用方算出）；sigma-clip 族参数
  `sigma_low/sigma_high/max_iterations/combine`、cosmetic 族
  `hot_sigma/cold_sigma/method/max_structure_size`（默认见
  lib/calibration/batch_config.json 实验配置：5.0/5.0/median/4）。
  正式 schema 版本化由 P1-CAL-IMPL 冻结（config_schema_ver）。

## 9 TEST-CAL-DESIGN-001 测试设计与冻结容差（P1-CAL-TEST 落地）

**合成 fixture**（全离线，无真实数据依赖）:
- FIX-CAL-A 常量场: raw=C、dark=D、flat=1.0（C,D∈{0,100,1000}）。
- FIX-CAL-B 解析梯度: raw=x+2y、dark=0.5x、flat=1+0.01x（可解析期望）。
- FIX-CAL-C 离群 stack: n_frames=5，其中 1 帧注入 +1000·δ 的尖刺
  （δ 逐像素 0/1 图样）；NaN 注入变体（1 帧 2 像素 NaN）。
- FIX-CAL-D master flat 三帧: 逐帧 median=100/200/400 的比例场。
- FIX-CAL-E cosmetic: 20×20 合成，热像素孤立点 + 3 像素 L 形连通域 +
  12 像素方块（星点模拟）+ 边缘坏点。
- FIX-CAL-F 负面: NULL 指针、w=0/h=0/n_frames=0、NaN 注入 dark/bias、
  photscal=NaN/Inf（apply_photometry）、window=4（cc 通道）。

### 9.1 独立 Oracle（不复用实现代码）

- Python/NumPy 复算 F1–F4 全部分支（float32 位级模拟 mean 帧序累加用
  numpy 顺序求和验证一致性），常量场期望 max_abs==0。
- 坏点 oracle: scipy.ndimage.label 独立复算连通域过滤（8 连通），IDW
  修复按 F4.3 定义逐步复算。
- K oracle: 恒等映射（dark_opt=1 时 actual_k==k_init；标准分支
  actual_k==1.0）。

**不变量**（冻结）:
- I1 常量场: 常数输入输出逐像素恒定，无空间调制（SCI §7）。
- I2 空平场: flat=NULL 输出与手算减法逐位相等。
- I3 幂等归一: 对已归一 master flat 重复 ALG-CAL-002 步骤 3 语义不变
  （median 已 1.0）。
- I4 确定性: 同输入双跑（1 与 2 线程）输出 bitwise 相等。
- I5 负值保留: raw<dark 输出负值，无 clamp（SCI §9a）。
- I6 掩码极性: 掩码 1=坏点，修复只改 bad 像素（好像素 bitwise 不变）。

**负面**: §7 全表逐行断言（AC_ERR_PARAM、actual_k 回写、out 不写、
sigma<=0 禁用、NaN 行为按 §7 声明）。

**串并行**: 线程数 1/2/4 三档 bitwise 一致（I4）；2 核合成负载（≥8 帧
4500×3600 等效像素量）相对 1 worker 加速比 ≥1.60（约束 D.7）；无嵌套
并行断言（模块内不自建线程池，约束 D.3）。

**ISA**: 现状无 ISA 变体——基线断言即可（任意 -O2 构建 bitwise 一致）；
若 P1-CAL-IMPL 引入 SIMD kernel，须先按约束 C.4-C.8 注册 benchmark 与
正确性自测，并冻结 ULP 容差（本设计不预设）。

**资源**: heavy run 资源监控（CPU/RAM/线程时序）随测；内存上限断言
（generate_master_flat ≤ n_frames·npix·4B×1.5+余量）；无 RSS 持续增长
（约束 D.9）。

**冻结容差**: 常量场/不变量/负面 = bitwise 或 max_abs==0；NumPy 对照
（含除法路径）float32 rtol=1e-6、atol=1e-7（SCI §11/§15 预冻结，来源:
float32 相对精度 ~1e-7 × floor 0.1 放大 ≤10×）；IDW/median 修复对照
oracle 同容差；actual_k 精确相等。

## 10 现状缺陷清单（如实登记，P1-CAL-IMPL/INT 处理；本任务不改代码）

- DISP-CAL-001（**部分关闭**）`generate_master_flat` 逐帧/最终归一对
  **负 median** 已在 B13-R13-7 改为**拒绝**（返回 AC_ERR_PARAM，不写 out；
  `master_generator.cpp:213-217,240-244`），不再直除翻转符号。**残留**：
  与 `normalize_flat`（median<=0 完全不归一，`calibrator.cpp:86`）语义
  不一致，且与 SCI-CAL-001 §4/§5/§8 的"保持原样"文本分歧未裁决 →
  **OWNER-04**（B2-A6 登记，不反向改 SCI）。`ac_generate_master_*` 系列
  无 extern "C" 异常屏障仍未处理：std::bad_alloc 可穿越 C ABI
  （AC_ERR_MEMORY/AC_ERR_INTERNAL 为死值，从未返回）。
- DISP-CAL-010（B2-A6 登记）`generate_master_flat` 步骤1 的**帧 median**
  经 `median_of` 计算，而 `median_of` 不剔除 NaN（`master_generator.cpp:
  49-60,207-209`）；SCI §4 声明的"median 计算跳过 NaN"只在 `generate_master`
  的逐像素路径实现。含 NaN 的 flat 帧其帧 median 依赖 nth_element 含 NaN
  的未定义序 ⇒ 测试夹具不得把 NaN 放进 flat stack（NaN 覆盖由 bias/dark
  stack 的 generate_master 路径承担）。
- DISP-CAL-002 `ac_set_num_threads` 全局改写 OpenMP ICV（进程级副作用，
  并发调用竞态；违反约束 D.3/D.4 ThreadLease 模型；API-P1-001 §2 已列
  V5 整改点）。
- DISP-CAL-003 `AC_METHOD_BILINEAR` 实际为 4 方向 IDW（1/dist 反比），
  非标准双线性插值；命名误导。
- DISP-CAL-004 `detect_hot/cold_pixels` 与 `calibrate` 不过滤 NaN：
  NaN 参与 cosmetic 全局 median/MAD（阈值不可靠）、NaN 算术直传校准输出
  （下游 Drizzle 跳过非有限像素，行为可用但未在 ABI 文档化）。
- DISP-CAL-005 `optimize_dark_k`（ALG-CAL-005）未编译未接线（不在
  CMake 清单、无调用方）；02_FROZEN §2.3 语义未进入生产路径。
- DISP-CAL-006 `apply_photometry`（ALG-CAL-006）未编译未接线；k_photo
  来源（Gaia 定标）不在本模块。
- DISP-CAL-007 `normalize_flat`/`compute_mad` 为公共命名空间符号但无
  头文件声明、无调用方（死代码风险）。
- DISP-CAL-008 模块内无取消检查点（PHASE1_API_V1 §2 承诺的"行带取消"
  未实现；现状取消在 phase1_session 帧粒度实现）。
- DISP-CAL-009 `w·h`（int）与 `w·h·n_frames`（f64 转接层已 int64，f32 主
  路径 npix 为 int）超大图溢出无防护；C API 不校验 w·h 上限。
- DISP-CAL-010 `ac::` 层 `n_frames<=0` 仅日志不返回（C API 层已挡），
  双层校验语义不一致；`generate_master` 对 `out==NULL` 无防护（依赖
  C API 层校验）。
- DISP-CAL-011 遗留通道（build.ps1 的 `-march=native -ffast-math`、
  Makefile cc_* DLL、`cpp/cosmetic_corrector.cpp` 双实现）与 CMake 主
  构建并存，语义漂移风险；`lib/calibration/python/`、
  `cosmetic_corrector.dll`、`astro_calibration.dll` 等旧 README 记载
  产物在当前树中不存在。

## 11 关联

- SCI: SCI-CAL-001（docs/science/CALIBRATION.md，FROZEN）
- DATA: DATA-P1-CAL（docs/contracts/DATA_SEMANTICS.md §9）；输入帧端口 DATA-P1-FRAME
- API: API-P1-001（docs/api/PHASE1_API_V1.md，编排合同 §2 已登记 ac_*）；API-CAL-001（docs/contracts/PUBLIC_API.md，现状 C API 合同）
- MOD/SRC: MOD-astrocs-phase1-calibration；SRC-CAL-001（astro_calibration.h 14 符号）
- 测试: TEST-CAL-DESIGN-001（本文 §9，P1-CAL-TEST 落地可执行 TEST-P1-CAL-001）；既有共址测试 lib/calibration/tests/test_photometry_apply.cpp
- ARCH: ARCH-001（docs/contracts/ARCH-001.md）
