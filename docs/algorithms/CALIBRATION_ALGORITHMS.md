# Calibration Algorithms (ALG-CAL)

> 上游：ASTROCS_DESIGN.md §4.2（Phase1 节点流程）

> ID: ALG-CAL-001  范围: ALG-CAL-001..006  上游 SCI: SCI-CAL-001  状态: CONTRACT_READY  模块: lib/algorithms/calibration（astrocs.p1.calibration / 目标 DLL astrocs_p1_calibration.dll）
>
> 连续数学定义以 `docs/science/CALIBRATION.md`（SCI-CAL-001，FROZEN）为唯一权威；
> 本文只做离散化与实现事实登记，**禁止**以本文反向修改 SCI。现行源码中不存在
> 黄金分割搜索、ISA benchmark 注册与取消检查点（§6 登记）。

## 1 上游 SCI 与模块边界

- 上游: `SCI-CAL-001`（docs/science/CALIBRATION.md，FROZEN）——§5 连续定义
  （dark_opt 双分支 + flat_norm median=1.0 / floor 0.1）、§9a 专属问题
  （无 pedestal、无 gain、无 read-noise 建模、负值保留、bad_mask 极性 1=坏点、
  variance 不传播）。
- 生产源: `lib/algorithms/calibration/include/astro_calibration.h`（唯一公共头，14 个
  `AC_API` 符号）+ `lib/algorithms/calibration/src/master_generator.cpp`、
  `lib/algorithms/calibration/src/calibrator.cpp`、`lib/algorithms/calibration/src/cosmetic_corrector.cpp`、
  `lib/algorithms/calibration/src/ac_api.cpp`（CMake `astrocs_calibration` 静态库唯一构建
  清单，CMakeLists.txt:453-472）。
- 负责: master bias/dark/flat 生成（sigma-clip 合并）、单帧校准算术、
  热像素/冷像素检测与插值修复；Gaia 测光比例标量应用（现状未接线）。
- 不负责: FITS/XISF 文件读写（astro_image_io，调用方侧）、母版按曝光/滤镜
  分组与匹配（orchestrator/调用方）、K=t_light/t_dark 的计算（调用方从
  EXPTIME 得出后传入）、噪声方差/SNR 估计（snr_estimator）、天光背景扣除
  （Phase2）、宇宙线剔除（叠加 rejection）、WCS/测光定标、整 Phase 编排。

## 2 连续定义（SCI-CAL-001 §5 转述，权威以 SCI 为准）

```text
母版约定（SCI-CAL-001 §5 输入合同）:
  master_bias  零曝光本底母版（ADU）
  master_dark  已减 bias 的暗电流母版（ADU）
  master_flat  已归一平场（median=1.0）

dark_opt=0（默认，标准式；master_dark 已减 bias）:
  cal = (raw − bias·[bias≠NULL] − K·dark·[dark≠NULL]) / max(flat, 0.1)
dark_opt=1（兼容式，显式 Bias/Dark 分离；master_dark 含 bias）:
  cal = (raw − bias − K·(dark − bias)) / max(flat, 0.1)
K = t_light / t_dark                        （两分支同一 K）
flat_norm = max(flat / median(flat), 0.1)   （median<=0 时不归一，保持原样）
```

离散实现中 `flat/median(flat)` 归一发生在 master flat 生成期
（ALG-CAL-002 步骤 3）；`calibrate`（ALG-CAL-003）对入参 flat 仅施加
floor 0.1，即约定入参 master_flat 已是 median≈1.0 的归一化平场。

**标度声明（SCI-CAL-001 §3/§6）**：本层 C ABI 单位盲，
入参必须已同标度；把母版文件解释成该标度是**调用方（io_read/编排）义务**，且必须**显式声明**
（禁止按后缀/目录名推断，`eng/contracts/data/phase_product_exchange_matrix.json` R-NO-NAME-BINDING）：

| 文件形态 | 文件自身声明 | 到 ADU 的换算 | 违反时 |
|---|---|---|---|
| FITS 整数（`BITPIX=16`, `BZERO=32768`） | `BSCALE/BZERO` ⇒ 物理值 | 无（已 ADU；[0,65535]） | — |
| XISF `Float32` `bounds="0:1"` | **可表示域**（黑/白点），非物理单位（XISF 1.0 §Image，浮点实型必须带 `bounds`） | 换算因子**不由文件给出**：须声明 `master_units=normalized` + `master_scale`（16 位原生数据取 65535，PCL `NormalizeSamples`/`UInt16 MaxSampleValue`） | 消费边界 DATA 拒绝（rc=2），诊断点名文件 |
| master_flat（任一形态） | 无 | 归一化是**独立维度**：`median(flat)` 须落在 `master_flat_median_range`（默认 [0.5,2.0]，`eng/packaging/config/defaults.json`），否则须显式声明 `master_flat_normalize="median"`（= §2 `flat_norm`，幂等） | 未声明且不落区间 ⇒ DATA 拒绝（rc=2） |
| master_dark | `dark_optimization`（bool）声明是否含 bias | — | 提供 dark 而未声明 ⇒ DATA 拒绝（rc=2） |

**四条机器规则（`eng/tools/quality/check_master_unit_guard.py --self-test` 可执行正负例）**：
U1 亮场域 ≫ 1 ADU 而 bias/dark 中位数 ≤ 1.0 且未声明 normalized+scale ⇒ 拒；
U2 `median(flat)` 出区间且未声明 median 归一 ⇒ 拒；
U3 提供 dark 而未显式声明 bias 约定 ⇒ 拒；
U4 已声明的换算因子/归一动作与观测统计必须自洽（声明 normalized 必带正 scale；bias/dark 声明域须一致）。

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
        σ    = 1.482602218505602·mad                        # k_sigma [76]
        σ<=0 → break                             # 无离散度 [120]
        v=NaN 当 dev<−sigma_low·σ 或 dev>sigma_high·σ   # 非对称阈值 [127]
        迭代 rejected==0 → break                 # 收敛 [132]
F1.4  合并 [136-157]:
        combine==AC_COMBINE_MEDIAN(1): out = median(非 NaN)；全 NaN → NaN
        combine==AC_COMBINE_MEAN(0):   cnt=|非 NaN|;
          cnt>0: out = (Σ_{k=0..n_frames-1} vals[k]·[!isnan]) / cnt
                 # std::accumulate 按帧下标升序 FP32 累加，顺序冻结 [149-152]
          cnt==0: out = NaN
```

中位数定义（全文档共用）: `median(v)`=nth_element 选择；n 奇取 `v[n/2]`，
n 偶取 `(v[n/2−1]+v[n/2])/2`（`median_inplace`/`median_of`，如
`calibrator.cpp:50-64`、`master_generator.cpp:49-60`）。

### 3.2 ALG-CAL-002 MasterFlat 生成 `ac::generate_master_flat`

源码锚: `master_generator.cpp:178-295`。无 combine 参数，合并固定 mean
（SCI-CAL-001 §12 ALG-CAL-002 语义一致）。

```text
F2.1  步骤1（每帧 n，OpenMP parallel for）[196-246]:
        dst = flat_stack[n] − bias          # bias==NULL 时拷贝
        tmp = {dst[i] | !isnan(dst[i])}     # DISP-CAL-010: 剔 NaN 后取中位数
        tmp 空（全 NaN 帧）→ 返回 AC_ERR_PARAM, out 不写         # [219-221]
        frame_med = median(tmp)                                  # [223]
        非有限 frame_med（含 ±Inf 相消）→ AC_ERR_PARAM, out 不写  # [226-230]
        frame_med == 0.0 → frame_med = 1.0                      # [231]
        frame_med < 0.0 → 返回 AC_ERR_PARAM, out 不写            # [234-238]
        dst = max(dst / frame_med, 0.1)     # 逐帧归一 + floor
F2.2  步骤2: generate_master(norm, …, AC_COMBINE_MEAN)   # [250-251]
F2.3  步骤3（最终归一）[255-288]:
        tmp = {out[i] | !isnan(out[i])}     # 逐像素全 NaN 像素保持 NaN
        tmp 空（全 NaN 输出）→ 返回 AC_ERR_PARAM, out 不写        # [263-265]
        final_med = median(tmp)                                  # [267]
        非有限 final_med → AC_ERR_PARAM, out 不写                 # [268-272]
        final_med == 0.0 → final_med = 1.0                      # [273]
        final_med < 0.0 → 返回 AC_ERR_PARAM, out 不写            # [276-280]
        out = max(out / final_med, 0.1)
```

> **OWNER-04 登记（不反向改 SCI）**：F2.1/F2.3 的"负 median 拒绝"与
> SCI-CAL-001 §4/§5/§8 对 `median<=0` 的"不归一、保持原样"文本不一致；
> 按 ANCHOR_CONTRACT §2.3 只登记、不改 SCI 原文，处置选项为改代码，或按
> `ENGINEERING_SPEC.md` §3 + `SCIENCE_CORRECTNESS.md` 流程订正 SCI 文本。
>
> **DISP-CAL-010 关联（不改 SCI 文本）**：F2.1/F2.3 帧级
> median 与 `generate_master` 逐像素路径同一 NaN 策略（先剔 NaN 再取
> 中位数）；"全 NaN 帧 / 全 NaN 输出 → fail-closed" 的退化语义在 SCI-CAL-001
> §4/§8 无显式条文，作为 **OWNER-04 关联**登记，处置选项同上。
>
> **消费边界（冻结口径）**：P1 校准节点 `p1_op_calibrate`
> （`module_adapters.cpp:1153-1179,1230-1240`）在进入 `ac_calibrate_frame`
> 前对 master flat 做**整帧退化判定**：全零 / median<=0 / 任一非有限像素 →
> DATA 拒绝（CLI rc=2）、不写 `calibrated_*`、不写 complete manifest。
> 校准公式、单位、暗场缩放与逐像素 floor 0.1 均不变。

### 3.3 ALG-CAL-003 单帧校准 `ac::calibrate` / `ac::calibrate_d`

源码锚: `calibrator.cpp:113-145`（float）、`calibrator.cpp:157-189`
（double，逻辑逐行一致）。OpenMP parallel for schedule(static)。

```text
F3.1  dark_opt==1 && bias && dark（兼容式；K=k_init 直通，非搜索）[124-132]:
        out[i] = (light[i] − bias[i] − k·(dark[i] − bias[i])) / max(flat[i], 0.1)
        （flat==NULL 时跳过除法）；actual_k = k_init
F3.2  否则（标准式, dark_opt==0；dark 视为已减 bias）[133-142]:
        k = k_init                       # K 必须由调用方给出（t_light/t_dark），不再强制 1.0
        out[i] = (light[i] − bias[i]·[bias!=NULL] − k·dark[i]·[dark!=NULL])
                 / max(flat[i], 0.1)·[flat!=NULL]
        actual_k = k
F3.3  参数无效（!light||!out||w<=0||h<=0）: actual_k=k_init，静默返回，
      out 不写 [116-119]；C API 层先校验并返回 AC_ERR_PARAM（ac_api.cpp:100-101）。
F3.4  契约边界:
      · bias 项在**两个分支都出现**：标准式 −bias、兼容式 −bias−K(dark−bias)；
        缺 bias（NULL）时该项为 0，且调用方必须在预检/manifest 显式登记"本底未去除"。
      · K 在**两个分支都施加**；缺 EXPTIME 时调用方 fail-closed，不得静默 K=1。
        K=1 时两分支代数恒等（SCI-CAL-001 §5/§7），逐位相等性不作判据。
```

**K / bias 退化对照表（逐格给期望诊断与 rc）**：

| 情形 | dark | bias | EXPTIME | K | 期望行为 | rc |
|---|---|---|---|---|---|---|
| 标准式 K=1 | 在位 | 在位 | t_light = t_dark | 1.0 | `(raw−bias−dark)/flat`；`bias_participated=true` | 0 |
| 标准式 K≠1 | 在位 | 在位 | t_light ≠ t_dark | t_l/t_d | `(raw−bias−K·dark)/flat` | 0 |
| 兼容式 K≠1 | 在位 | 在位 | t_light ≠ t_dark + `dark_optimization=true` | t_l/t_d | `(raw−bias−K·(dark−bias))/flat` | 0 |
| dark 缺 EXPTIME | 在位 | 在位 | master_dark 无/非正 EXPTIME | — | DATA fail-closed（不得静默 K=1） | 2 |
| light 缺 EXPTIME | 在位 | 在位 | light 无/非正 EXPTIME | — | DATA fail-closed | 2 |
| 显式 `dark_scale_factor` 与 EXPTIME 比不一致 | 在位 | 在位 | 在位 | — | DATA fail-closed（>1e-6 相对） | 2 |
| dark 缺失 | NULL | 在位 | 任意 | 不进入算术 | `(raw−bias)/flat` | 0 |
| bias 缺失 | 在位 | NULL | 在位 | t_l/t_d | `(raw−K·dark)/flat`；manifest `optimize`「本底未去除」+ stderr warning | 0（预检 error 需 `-force` 越过） |
| `dark_optimization=true` 但 bias/dark 缺一 | 缺一 | 缺一 | 任意 | k_init | 回退标准式且沿用 k_init（不强制 1.0） | 0 |

> 上表 rc 口径 = CLI 退出码（DATA 拒绝 → 2；`-force` 只越过「缺标定帧」类预检
> error，不越过 DATA 校验）。单位一致性（母版与 light 同标度）由**消费
> 边界门**（`p1_op_calibrate` 前置校验 + `eng/tools/quality/check_master_unit_guard.py`，
> 见 §2 标度声明表与 DISP-CAL-013）fail-closed 校验；**本层（calibrator）保持单位盲**，
> 只做 SCI-CAL-001 §5 的逐像素算术。

- `normalize_flat`（`calibrator.cpp:85-100`，median→1.0 + floor 0.1，
  med<=0 原样返回）：**当前无任何生产调用方**（ac_api 不转发，模块内无
  调用）。median→1.0 归一由 ALG-CAL-002 在 master 生成期承担；登记为
  未接线辅助符号（DISP-CAL-007）。
- `compute_mad`（`calibrator.cpp:71-82`）：与 cosmetic 的
  `compute_global_mad` 同义，供 C++ 内部使用，公共头无声明。

### 3.4 ALG-CAL-004 坏点检测/修复 `ac::detect_hot_pixels` / `ac::detect_cold_pixels` / `ac::filter_by_structure_size` / `ac::interpolate_pixels` / `ac::correct_frame`

源码锚: `cosmetic_corrector.cpp:61-265`。

```text
F4.1  热像素 [118-134]: med=median(dark 全帧)；mad=median(|dark−med|)；
        σ=1.482602218505602·mad；hot = (dark > med + hot_sigma·σ)
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
F5.1  背景提取: |light−median| <= 3·1.482602218505602·MAD 全帧阈值；MAD=0 → 全部
      视为背景 [125-142]
F5.2  8×8 分区抽样: 每区 ≤1000 背景（等步长跨步），总样本 ≤50000
      [144-207]
F5.3  鲁棒回归: OLS（double 累加）+ 残差 3·1.482602218505602·MAD 离群抑制，≤5 轮
      [219-306]
F5.4  失败→回退 k_init 且 diagnostics.fell_back=1, fallback_from=
      "OPTIMAL", fallback_to="EXPOSURE_RATIO"；诊断码: PARAM、BAD_K_INIT、
      INSUFFICIENT_SAMPLES(<100)、TOO_FEW_INLIERS(<50)、ZERO_VARIANCE
      (denom<=1e-12)、RESIDUAL_NAN、RESIDUAL_ABNORMAL(残差σ≥yσ)、
      K_OUT_OF_RANGE(k<=0 或 >10)、OK [108-118,211-217,240-256,297-302,
      311-355]
```

**现状**: `dark_optimizer.cpp` 不在 CMake `astrocs_calibration` 构建清单
（CMakeLists.txt:446-456），全仓无调用方；`hiss::Stage1Diagnostics` 来自
`lib/infrastructure/aio/include/hiss_format.h`。登记为待迁移符号
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

**现状**: 不在 CMake 构建清单，无生产调用方；`eng/tests/
test_photometry_apply.cpp` 为其共址测试（同样未挂接 CMake 测试目标）。
k_photo 的来源（Gaia 光谱积分定标）不在本模块（登记 DISP-CAL-006）。

**k_photo 语义**：`k_photo` 的**绝对值无物理意义**
（吸收增益/口径/曝光等未知量；设计前提 = FITS 头拿不到这些量）；**禁止**用物理闭合式反推仪器参数，
**禁止**设绝对窗口；验收只用**一个尺度无关判据**——**测光一致性**（星等与 Gaia 残差散度/MAD 小）。
**「帧间一致性」（各帧落同一测光体系）是语义目标与报告字段，不是门禁判据**。
见 `docs/science/PHOTOMETRY.md` §1。

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
| NaN 语义 | generate_master 统计跳过 NaN、全 NaN→输出 NaN；**generate_master_flat 帧级 median 同样先剔 NaN（DISP-CAL-010），全 NaN 帧/全 NaN 输出 fail-closed**；calibrate/cosmetic 阈值统计**不**过滤 NaN（NaN 算术直传/阈值不可靠） | master_generator.cpp:106-118,214-223,258-267；cosmetic_corrector.cpp:45-54 |
| 日志 I/O | generate_master/flat 每次调用 2 行 stderr（ac_log）；apply_photometry 2 行 stderr；无文件/网络 I/O | master_generator.cpp:38-45 |
| 内存 | 输出缓冲调用方分配；模块内 std::vector RAII。峰值额外内存: generate_master O(n_frames/线程)；generate_master_flat O(n_frames·npix·4B)（norm 主缓冲）；calibrate O(1)；cosmetic O(npix)（labels+masks+统计副本）；f64 转接层 O(n_pix) 全帧复制 | 各源文件 |
| 构建 | CMake 目标 `astrocs_calibration`（STATIC，4 个 cpp，OpenMP 可选）；非生产 MinGW 通道: build.ps1（astro_calibration.dll）、Makefile（cpp/ 版 cosmetic_corrector.dll，cc_* 4 导出，window 奇数 3..15） | CMakeLists.txt:446-465；lib/algorithms/calibration/Makefile |
| 生产调用方 | `lib/phase1_session/p1_session.cpp:243` 仅调 `ac_calibrate_frame`（master 由配置传入，帧粒度取消在 session 层）；master 生成与 cosmetic 的 ac_* 入口当前无生产调用方 | p1_session.cpp:200-270 |

### 4.1 非生产双实现：`cpp/cosmetic_corrector.cpp`（cc_* 通道）

与 `src/cosmetic_corrector.cpp` 是**两套独立实现**：cc_* 由 Makefile 编译为
独立 DLL（Python ctypes 通道），支持参数化窗口 `cc_correct_median(data,
bad_mask,H,W,window)`（window 奇数 3..15，偶数/<3/>15 返回 −1，15×15 栈
缓冲 256 上限）、`cc_detect_hot/cc_detect_cold`（double sigma）返回计数、
`cc_last_error()`。不在 CMake 构建内，属待迁移符号（P1-CAL-IMPL 决定
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

## 6 确定性与归约

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
  provider 选择；编译期仅 `-O2/-O3` 级优化与 OpenMP。

### 6.1 SIMD 安全（现状：无 SIMD 显式向量化，合同见约束 C.4-C.8）

- **fast-math**: CMake 主构建不加 fast-math；非生产 build.ps1/Makefile 通道
  使用 `-O3 -march=native -ffast-math`（不作为确定性合同依据）。

## 7 边界 / invalid / 退化行为（实现如实）

| 条件 | 实现行为 | 锚 |
|---|---|---|
| 空指针 / n_frames<=0 / w<=0 / h<=0（C API 入口） | 返回 AC_ERR_PARAM，不写 out | ac_api.cpp:60-61,72-73,86-87,100-101,115-116 及 f64 对应 |
| ac:: 层参数无效（void 函数） | 静默返回，out 不写，actual_k=k_init | calibrator.cpp:116-119；cosmetic_corrector.cpp:236 |
| median(flat)<=0（normalize_flat） | 不归一保持原样（SCI §4/§5/§8 文本；与 master_generator 的"拒绝"分歧登记 OWNER-04） | calibrator.cpp:86 |
| frame_med/final_med == 0（master flat，全零帧） | 置 1.0（不缩放） | master_generator.cpp:231,273 |
| flat 帧全 NaN / 全 NaN 输出（master flat） | 剔 NaN 后无有效中位数 → 返回 AC_ERR_PARAM，out 不写（DISP-CAL-010 fail-closed；OWNER-04 关联：全 NaN 帧退化语义 SCI-CAL-001 §4/§8 无显式条文） | master_generator.cpp:219-221,263-265 |
| frame_med/final_med < 0（master flat） | 返回 AC_ERR_PARAM，不写 out | master_generator.cpp:234-238,276-280 |
| flat 帧含部分 NaN（master flat 步骤1/3 median） | 与 generate_master 逐像素路径同一策略：先剔 NaN 再取中位数（DISP-CAL-010） | master_generator.cpp:214-223,258-267,49-60 |
| master flat 全零 / median<=0 / 非有限（p1_op_calibrate 消费边界） | DATA 拒绝（CLI rc=2），不进入 calibrate、不写 calibrated_*（fail-closed） | module_adapters.cpp:1153-1179,1230-1240 |
| flat==NULL（calibrate） | 跳过除法，退化减法 | calibrator.cpp:129,139 |
| dark==NULL（calibrate 标准分支） | out=(light−bias)/flat（bias 在位时） | calibrator.cpp:137-138 |
| dark_opt=1 但 bias/dark 缺一 | 回退标准式且**沿用调用方给的 k**（不再强制 k=1.0） | calibrator.cpp:124,133-142 |
| bias==NULL 而 dark 在位（标准式） | 本底不去除：out=(light−K·dark)/flat；调用方预检/manifest 必须显式登记（DISP-CAL-012） | calibrator.cpp:136-140；module_adapters.cpp（p1_op_calibrate） |
| dark_opt=1 且 bias+dark 在位（p1_op_calibrate K 分支） | K 由 light/dark FITS EXPTIME 推导 = t_light/t_dark；EXPTIME 缺失/非正或显式 dark_scale_factor 与 EXPTIME 比不一致 → DATA 拒绝（CLI rc=2），不进入 calibrate、不写 calibrated_*（fail-closed） | module_adapters.cpp:1243-1305 |
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
  CMake 静态库 `astrocs_calibration` 与非生产 MinGW DLL）。
- 三方一致: `acs_module_descriptor_v1`（lib/include/astrocs/abi/module_api_v1.h:40-53，
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
  lib/algorithms/calibration/batch_config.json 实验配置：5.0/5.0/median/4）。
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
- K oracle: 恒等映射（**两分支** actual_k==k_init；标准式不再把 k 强制为 1.0）。
- **bias 参与 oracle**：同一 light/dark/flat/K 下，bias 在位与
  bias=NULL 两次调用的输出必须逐像素不同（dark=NULL 时差恒为 bias/max(flat,0.1)）；
  把实现里的 bias 项删掉（ignore-bias 变异）该判据必须判红。

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

## 10 现状缺陷清单（如实登记，P1-CAL-IMPL/INT 处理；不改代码）

- DISP-CAL-001（**部分关闭**）`generate_master_flat` 逐帧/最终归一对
  **负 median** 取**拒绝**语义（返回 AC_ERR_PARAM，不写 out；
  `master_generator.cpp:234-238,276-280`），不直除翻转符号。**残留**：
  与 `normalize_flat`（median<=0 完全不归一，`calibrator.cpp:91`）语义
  不一致，且与 SCI-CAL-001 §4/§5/§8 的"保持原样"文本分歧仍为开放项 →
  **OWNER-04**（不反向改 SCI）。`ac_generate_master_*` 系列
  无 extern "C" 异常屏障仍未处理：std::bad_alloc 可穿越 C ABI
  （AC_ERR_MEMORY/AC_ERR_INTERNAL 为死值，从未返回）。
- DISP-CAL-010（**已按现行语义落地**）`generate_master_flat` 步骤1 的
  **帧 median** 与 `generate_master` 逐像素路径同一 NaN 策略：先剔 NaN 再取
  中位数（`master_generator.cpp:49-60,214-223`）；全 NaN 帧无有效中位数 →
  返回 AC_ERR_PARAM（fail-closed，SCI §4）。帧 median 不依赖 nth_element
  含 NaN 的未定义序。
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
- **DISP-CAL-012（BIAS-001）bias/K 语义**：标准式
  `(raw − bias − K·dark)/flat`（master_dark 已减 bias，默认）+ 兼容式
  `dark_opt=1`（master_dark 含 bias，显式分离）；K 在两分支都施加，bias 在
  标准式显式减除（§2/§3.3）。外部标准同向：astropy ccdproc 要求 master dark
  "has been bias-subtracted so that it can be scaled by exposure time"，顺序为
  bias → dark(×曝光比) → flat；LSST `ip_isr` 为 `biasCorrection` →
  `darkCorrection`（`maskedImage -= dark * expScaling / darkScaling`）→
  `flatCorrection`（证据与 URL 见 `docs/science/CALIBRATION.md` §14 第 4/5 条）。
  **判据（真实 T2 NGC1727 Red 600s，固化二进制 63da68618aa73203）**：默认分支下
  "提供 vs 缺失 master_bias" 的 `calibrated_*.fts` 必须不同——**逐位相同
  （16,777,216 px 全等，max|Δ|=0）即偏离**；只提供 bias（无 dark/flat）时产物与
  完全不标定必须不同（**逐位相同即 bias 零影响，属偏离**）；缺 bias 的 dark+flat
  产物 100% 像素不同（max 300192、mean 5863.8）⇒ dark/flat 确实参与。
  `dark_opt=1` 分支有/无 bias 并非逐位相同（2,141,721 px 差，max|Δ|=0.03125），
  差异来自 FP32 舍入而非代数相消。**影响面**：默认分支（`dark_optimization` 缺省）
  消费"已减 bias 暗电流母版"的标定产物；含 bias 的暗场母版必须用
  `dark_optimization=true` 显式声明（否则多减一次 bias）。相邻项（UNIT-001，
  见 DISP-CAL-013 与 SCI-CAL-001 §3/§6/§8）：XISF 母版 [0,1] 归一化 vs 亮场
  ADU 的量级不一致、master_flat 未归一（median 0.2064）均无机器门——二者使
  真实链路产物标度错 ≈×4.85 且本底几乎未减；UNIT-001 落地后该两类输入
  fail-closed，bias/K 语义独立成立。
- **DISP-CAL-013（UNIT-001）XISF 母版 [0,1] 归一化被当 ADU 消费**：
  `aio_xisf.cpp` 对 `sampleFormat="Float32"` 只做字节布局转换（`convert_xisf_pixels`），
  **不解释 `Image` 元素的 `bounds`（可表示域）**，也不做任何单位换算；`p1_op_calibrate`
  把读到的 float 直接当 ADU 传给 `ac_calibrate_frame`——该路径由 §2 标度声明表与
  U1–U4 四条机器规则 fail-closed 拦截。
  **独立实测（自写 XISF/FITS 读取器，不导入 AstroCS）**：T2 母版
  `sampleFormat=Float32 bounds="0:1"`——`masterBias` median
  0.015288（×65535 = **1001.87 ADU**）、`masterDark600` median 0.015391（**1008.63 ADU**）、
  `masterFlatRed` median 0.206381（**13525.15 ADU**，**未归一**到 1.0）；真实亮场
  `BITPIX=16 BZERO=32768` ⇒ [0,65535] ADU（T2 NGC1727 600s Red median 1481 ADU）。
  两条暗场证据：T2 600s/1200s 线性外推零曝光截距 999.96 ADU
  ≈ `masterBias` 1001.87 ADU；T4 180/300/600s 斜率 0.5700 ADU/s、截距 955.05 ADU ≈ bias
  916.16 ADU ⇒ **两套真实母版暗场均含 bias**（须 `dark_optimization=true`）。
  **标度后果（实测定稿）**：未声明标度时 flat 以 0.206381 作除数 ⇒ 整帧放大 **1/0.206381 = 4.845×**；
  bias/dark 只减 0.0153 而应减 1001.87 ADU ⇒ 本底几乎未减。二者叠加使真实产物中位数量级错：
  **T2 NGC1727 600s Red：未声明 7048.618 ADU → 声明后 436.155 ADU（16.161×，实测 vs 逐像素 oracle）**；
  **T4 Galaxy_Center panel1 180s Red：3650.390 → 361.236 ADU（10.105×）**。
  **端到端验证（真实链路 rc=0）**：T2 同帧声明后产物 `median=436.1555`、
  `mean=500.7639`、`min=−23215.701`、`max=74318.594`，与门内独立 NumPy oracle 相对差 **1.26e−08**；
  未声明标度时同一帧 `median=7048.6179`、`mean=7372.6422` ⇒ 比值 **16.161×**。
  **现行规定**：①§2「标度声明」表与 U1–U4 四条机器规则；②消费边界（`p1_op_calibrate`）
  新增声明解析 + 观测统计校验 + 声明换算 + 节点 manifest 溯源（`master_unit_guard`），
  违反即 DATA 拒绝（rc=2）并点名文件 + 观测值 + 缺失声明项；
  ③`eng/tools/quality/check_master_unit_guard.py`：**四条负例（U1–U4）+ 两条正例**（显式声明组合 /
  本就合规组合），合成与真实 T2 双模式，门内逐像素 NumPy oracle，`--self-test` 为期望 token 变异注入；
  ④`eng/packaging/config/defaults.json` 登记 `calibration.master_flat_median_range`（[0.5,2.0]）；
  ⑤与 U3 冲突的既有节点级夹具（`eng/tests/unit/p1001_real_nodes_test.cpp` 9 处 doc）补显式
  `dark_optimization=false`（该夹具 `vd=5 < vb=10` = 已减 bias 的暗电流，声明后数值不变）。
  **残留（登记待裁定）**：⑥**未新增 ctest 目标**（新目标必须在 `eng/ci/checks.json` 的
  `ctest_targets` 登记）⇒ U1–U4 的机器覆盖由上述门脚本承担；
  ⑦**节点 manifest 未落盘**：`master_unit_guard` 写入节点 manifest 与 `stages.calibrate`，
  但当前 CLI 面只持久化 run manifest（`summary`/`provenance.units=["ADU"]`）与失败时的
  `error.message` ⇒ **拒绝路径可审计（token + 点名文件 + 观测值已入 run manifest）**，
  **接受路径的"实际施加换算"尚未落盘**；声明内容本身可由 run manifest 的 `config_path`/
  `config_sha256` 复核。落盘位置（节点 manifest dump 或扩展 `provenance.units`）属 CLI 域，待裁定。
  **影响面**：一切消费 XISF 母版的真实链路产物标度（`calibrated_*`、`cleaned_*`、Phase1 HiPS
  signal 及其下游 mosaic/export）与由之派生的测光/SNR 台账数字；合成测试若使用同域母版不受影响。
- **DISP-CAL-014（门侧已覆盖，通用语义待裁定）拒绝/失败路径的产物残留**：
  单位/归一化门在 calibrate 节点**前置**判红（DATA/rc=2），混标度输入不产出任何
  `calibrated_*`；门脚本 `eng/tools/quality/check_master_unit_guard.py` 对三条负例显式断言
  「拒绝路径不留 calibrated_* 半成品」。**残留（通用语义，不在本文件域，待裁定）**：
  run 级 incomplete 时上游节点已原子发布的产品如何标记/清理（`.incomplete` 后缀、独立
  staging、或 run 结束统一回滚）——ENGINEERING_SPEC §9「失败不得留下可被误认成正式产品的
  半成品」的落地口径；该场景下 output_dir 会留下形状完整、可被误认成正式产品的
  `calibrated_*`/`cleaned_*`（下游节点如 plate_solve 失败时 rc≠0、run manifest
  `status=incomplete`）。
- DISP-CAL-011 非生产通道（build.ps1 的 `-march=native -ffast-math`、
  Makefile cc_* DLL、`cpp/cosmetic_corrector.cpp` 双实现）与 CMake 主
  构建并存，语义漂移风险；`lib/algorithms/calibration/python/`、
  `cosmetic_corrector.dll`、`astro_calibration.dll` 等产物在当前树中不存在。

## 11 关联

- SCI: SCI-CAL-001（docs/science/CALIBRATION.md，FROZEN）
- DATA: DATA-P1-CAL（docs/contracts/DATA_SEMANTICS.md §9）；输入帧端口 DATA-P1-FRAME
- API: API-P1-001（docs/api/PHASE1_API_V1.md，编排合同 §2 已登记 ac_*）；API-CAL-001（docs/contracts/PUBLIC_API.md，现状 C API 合同）
- MOD/SRC: MOD-astrocs-phase1-calibration；SRC-CAL-001（astro_calibration.h 14 符号）
- 测试: TEST-CAL-DESIGN-001（本文 §9，P1-CAL-TEST 落地可执行 TEST-P1-CAL-001）；既有共址测试 lib/algorithms/calibration/tests/test_photometry_apply.cpp
- ARCH: ARCH-001（docs/contracts/ARCH-001.md）

## 参考文献与参考代码库（含许可证）— SCI-001-S2 补齐

> 本节只补出处与参考实现，不改动本文件任何公式、锚点、阈值与容差；原有条款全部保留。

- 母版约定与 ISR 顺序：ccdproc（BSD-3-Clause）reduction_toolbox/subtract_dark；LSST ip_isr（GPL-3.0）isrFunctions.py。
- 探测器噪声/gain：Janesick 2001, SPIE PM83, Ch.2；Newberry 1991, PASP 103, 122；Howell 2006, Handbook of CCD Astronomy 2nd ed., CUP, Ch.4。
- MAD→σ 常数 1.482602218505602：标准正态分位恒等式；Rousseeuw & Croux 1993, JASA 88, 1273（DOI 10.1080/01621459.1993.10476408）。
- FITS BSCALE/BZERO：FITS Standard 3.0 §4.2.1/§4.3；CFITSIO 作独立读取器 Oracle。
- XISF bounds 与 65535：XISF 1.0 Spec（PixInsight；PCL 自定义 source-available 许可）。
- IRAF ccdproc/zerocombine（IRAF/NOAO 许可，非 OSI）：经典归约顺序对照。

参考代码库（含许可证；GPL 代码仅作行为/数值对照，不复制进本仓）：
- Astropy（BSD-3-Clause，https://github.com/astropy/astropy）；photutils（BSD-3-Clause，https://github.com/astropy/photutils）；astropy-healpix（BSD-3-Clause，https://github.com/astropy/astropy-healpix）；ccdproc（BSD-3-Clause，https://github.com/astropy/ccdproc）；reproject（BSD-3-Clause，https://github.com/astropy/reproject）。
- DrizzlePac（BSD-3-Clause，https://github.com/spacetelescope/drizzlepac）。
- SExtractor / PSFEx / SWarp / SCAMP（GPL-3.0，https://github.com/astromatic/）。
- healpy（GPL-2.0，https://github.com/healpy/healpy）；Siril（GPL-3.0，https://gitlab.com/free-astro/siril）；LSST ip_isr（GPL-3.0，https://github.com/lsst/ip_isr）；GSL（GPL-3.0，https://www.gnu.org/software/gsl/）。
- WCSLIB（LGPL-3.0）；CFITSIO（宽松许可，NASA/HEASARC，https://heasarc.gsfc.nasa.gov/fitsio/）。
- NumPy / SciPy（BSD-3-Clause）：独立 FP64 Python Oracle。

