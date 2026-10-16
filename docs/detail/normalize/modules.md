# Normalize 模块与接口

本页按生产依赖组织节点接口，依据[阶段管线](pipeline.md)和所引科学、工程合同。每节说明输入输出、公开头、所有权、错误及实现边界；库能力、局部会话与 CLI 生产节点分别标识。

| 模块 | 调用职责 |
|---|---|
| [calibration](#calibration) | Normalize 校准 |
| [cosmetic](#cosmetic) | Normalize 缺陷修复 |
| [wcs-platesolve](#wcs-platesolve) | Normalize 天体测量 |
| [star-detection](#star-detection) | Normalize 科学源检测 |
| [star-psf](#star-psf) | Normalize PSF 拟合与模型 |
| [photometry](#photometry) | Normalize 测光与标度应用 |
| [noise-snr](#noise-snr) | Normalize 噪声与参考精度 |
| [drizzle](#drizzle) | Normalize 球面 Drizzle |
| [writer](#writer) | Normalize 帧产品发布 |
| [session](#session) | Normalize 局部会话 |

## calibration

**Normalize 校准。**

校准把 lights 和已绑定的 bias/dark/flat 转为同标度的积分线性像元。母版约定、暗流曝光比例、平场归一与误差传播取自[校准科学分册](../../science/calibration/CALIBRATION.md)及 [Normalize 科学正本](../../science/normalize/NORMALIZE_SCIENCE.md)。

### 生产入口与数据交接

CLI 八节点的入口是 [module_adapters.cpp](../../../lib/infrastructure/scheduler/src/module_adapters.cpp) 的 `p1_op_calibrate`，descriptor 为 `acsd.phase1.calibration`。它消费 `frames`，调用校准库，产生 `calibrated` 供 cosmetic 和 IPV；不会由本节点生成 WCS。局部 `p1_session` 也调用同一库，但其会话边界不同。

当前 adapter 读取 lights/masters，检查尺寸与声明，逐帧调用 `ac_calibrate_frame`，经 AIO 写校准工作面及母版来源记录。像素载体仍包含 FITS 工作文件；目标 `CalibratedPlane`、identity 和对象版本见[数据合同](../../engineering/contracts/NORMALIZE_DATA.md)。

| 字段/数组 | 单位、坐标与有效性 |
|---|---|
| light、bias、dark | 同一已解释的积分线性尺度；FITS BSCALE/BZERO 仅应用一次 |
| flat | 归一后的相对响应；下界和受影响范围由科学分册说明 |
| width、height、pixels | 行主序 `y*width+x`；内部零基像元中心 |
| K、actual_k | 无量纲；曝光来源/暗场含偏置约定由 adapter 登记 |
| 输出 | 负值与有限零合法；坏值及母版能力须携带来源，不能静默补零 |

### 公开 API 与所有权

签名头为 [astro_calibration.h](../../../lib/algorithms/calibration/include/astro_calibration.h)，实现位于 [calibrator.cpp](../../../lib/algorithms/calibration/src/calibrator.cpp) 和 [ac_api.cpp](../../../lib/algorithms/calibration/src/ac_api.cpp)。

`ac_calibrate_frame(light,width,height,master_dark,master_flat,master_bias,out,dark_optimization,dark_scale_factor,actual_k)` 读取调用方数组并写调用方 `out`；不分配输出、不读文件。对应 `_f64` 是独立入口，不能只因名称含 f64 就假定所有母版/修复 API 均无内部降精度。

`ac_generate_master_bias`、`ac_generate_master_dark`、`ac_generate_master_flat` 及其 f64 变体为库能力；CLI 本节点消费已提供母版，不能写成每次 normalize 自动生成它们。`ac_set_num_threads` 改 OpenMP 设置，由宿主预算调用；模块不另定资源预算。

### 配置、错误与实现边界

配置来自本次 block 的 `input_lights`、`master_*`、暗场约定/曝光与标度声明。不得从文件后缀猜归一因子；K 应与真实曝光配对。返回码定义于公开头（`AC_OK`、`AC_ERR_*`），adapter 非成功时转 `Result` 错误；不能把声明了错误枚举等同所有异常已转码。

像元算术与母版随机/共同不确定度是不同能力；当前覆盖以[差异地图](../../engineering/architecture/SCIENCE_IMPLEMENTATION_DELTA.md)为准，不宣称 master Jacobian 已全传递。大面寿命和 typed 迁移依[接口规格](contracts.md)。

### 验证入口

[阶段一管线测试](../../../eng/tests/pipeline/test_stage_normalize.py)检查生产入口；独立校准检验应覆盖两种暗场约定、非单位 K、平场边界、标度声明、负信号及共享母版误差。并发等价需以实际 budget 配置与输出读数核对，本页不提供未经运行的通过结论。

## cosmetic

**Normalize 缺陷修复。**

本节点消费校准面并产生 cleaned 面及修复来源，供科学源、测光和噪声模型使用。判据由 [CCD 缺陷分册](../../science/calibration/CCD_DEFECT.md)和 [Normalize 科学正本](../../science/normalize/NORMALIZE_SCIENCE.md)规定；修复产生的相关性不能凭一张对角方差面宣称完整。

### 入口与生产链

[scheduler adapter](../../../lib/infrastructure/scheduler/src/module_adapters.cpp) 的 `p1_op_cosmetic` 对应 `acsd.phase1.cosmetic`，端口为 `calibrated → cleaned`。节点仅在 `cosmetic.enabled=false` 时整体关闭；缺该配置段不表示关闭。

实际路径读取校准工作面，解析显式 dark/bias 或校准节点的 `master_refs.json`，调用 `ac_correct_frame`，并按列配置调用 `ac_correct_columns_ex2`。母版不可得、不可读或尺寸不符的来源被记录。局部 `p1_session.cpp` 的有限修复接线不能代表此生产节点。

### 数组与公开 API

[astro_calibration.h](../../../lib/algorithms/calibration/include/astro_calibration.h) 声明接口，[cosmetic_corrector.cpp](../../../lib/algorithms/calibration/src/cosmetic_corrector.cpp) 实现检测与修复。

| 接口/字段 | 含义 |
|---|---|
| `ac_correct_frame` / `_f64` | 输入图像、dark/bias、hot/cold sigma、插值方法与结构大小；输出修复面和热/冷计数 |
| `ac_correct_columns`、`ac_detect_bad_columns_from_master` | 科学面或单母版的列检测/修复基础能力 |
| `ac_correct_columns_ex`、`ac_correct_columns_ex2` | 科学面/dark/bias 并集及来源/置信度；ex2 分别承载母版阈值与宽段登记范围 |
| col_mask | 每列 0=未检出、1=已修、2=仅标记；不能按布尔值解释已修数量 |
| source/conf mask | 科学面、dark、bias 来源位及其置信度，和修复像素分账 |
| `ac_column_variance_inflate` | 对外部给定方差按修复权重平方和及调用方 kappa 更新；不是独立噪声估计器 |

图像为行主序，内部中心是零基索引；信号保留校准单位，方差用其平方。非坏像元保持原值，修复取输入锚点而不级联读取已修值。`neighbor_k` 在当前列接口中没有行为读点；段长上限由 `max_seg_len` 承担。`_f64` 修复入口的内部转换须依实现解释，不能按名字声明全程 FP64。

### 所有权、并行与错误

调用方提供输入/输出与列掩膜缓冲；库不写盘。adapter 保存 cleaned 工作面、列掩膜与逐帧报告，按帧分片后定序汇总；线程来自 Runtime 授权预算。底层像素循环与帧级并行需要共同服从预算，取消由上层协作传播。

`AC_COLSTAT_*` 区分零尺度、单侧锚、无锚、宽段和边界排除；无锚保留输入值并置状态，不能写成成功补全。`AC_ERR_PARAM` 等返回交给 adapter 上行。单帧 cosmic-ray、trail 的完整分类不属于这些坏点/坏列函数的既有能力。

### 配置与验证入口

有效配置包含 hot/cold sigma、method、max_structure_size、`bad_column_*`，实际默认值由配置与 adapter 解析给出。[Normalize 数据合同](../../engineering/contracts/NORMALIZE_DATA.md)要求修复状态/算子与噪声模型配对；具体生产差异只见[差异地图](../../engineering/architecture/SCIENCE_IMPLEMENTATION_DELTA.md)。

验证从本页公开函数及[阶段一管线测试](../../../eng/tests/pipeline/test_stage_normalize.py)进入：注入点/列缺陷，检查三路来源、0/1/2 掩膜、宽段不修、边缘单侧、无锚、负信号和修复相关性。本页未运行这些检验。

## wcs-platesolve

**Normalize 天体测量。**

本节点在校准像元上调用 IPV，自行盲检、匹配本地 Gaia 并求 TAN/SIP。原始 FITS 没有 CRVAL/CD 只是输入属性；实际 solved/unsolved 由求解尝试决定。[Normalize 科学正本](../../science/normalize/NORMALIZE_SCIENCE.md)规定此因果顺序，[天测分册](../../science/detection/ASTROMETRY.md)说明坐标与残差口径。

### 入口、上下游与载体

[module_adapters.cpp](../../../lib/infrastructure/scheduler/src/module_adapters.cpp) 的 `p1_op_wcs` 对应 `acsd.phase1.wcs-platesolve`，输入 `calibrated`，输出 `wcs`；它调用 `ipv_solve_from_memory_with_callback_d`，不依赖下游 `science_sources`。该解供 guided sources、photometry、Drizzle 和 writer。

近似指向/尺度可来自 header_pointing、config、neighbor_crval；它们只是初值。明确配置的完整 WCS 标为 `explicit_config`，不能标为 IPV 求解成功。当前结果写 `p1_wcs.json`；Runtime 对象、attempts 和版本合同见 [Normalize 数据合同](../../engineering/contracts/NORMALIZE_DATA.md)。

### 公开 API 与字段

[ipv_api.h](../../../lib/algorithms/platesolve/cpp/ipv/include/ipv_api.h)声明 `ipv_solve_create/destroy`、`ipv_solve_from_memory*`、`ipv_solve_from_detections_v1`、参数初始化和 inlier 读取接口。后一个入口可接外部候选，但不改变 CLI 从像元求解的实际调用。

| 输出/输入 | 单位与合同 |
|---|---|
| pixels | 行主序校准图像；回调候选属于 solver 私有域 |
| CRVAL、CD | ICRS degree、degree/pixel；几何与匹配元数据 FP64 |
| CRPIX | FITS 一基连续参考像元；外部合法 WCS 不强制位于画幅中心 |
| SIP | A/B 与 AP/BP 按声明阶数和 i*6+j 布局保留；不能丢畸变 |
| rms_px、rms_arcsec、n_pairs | 内部拟合诊断，不能当留出绝对残差 |
| success、error_msg | 成功状态和真实失败原因；不可用解不发布伪几何 |

内部 `(x0,y0)` 是零基数组中心，进入 FITS/SIP 恰加一。IPV adapter 同时处理其自身坐标输出，不能再重复加半像元或一像元。`p1_wcs.json.samples[]` 的 sky 已包含 SIP；消费方不再加一次畸变。完整 SIP 反演、参考历元与 epoch propagation 依正式合同解释。

### 所有权、并行与错误

solver 用 create/destroy 配对，result POD 和 inlier 缓冲由调用方提供；callback 视图只在调用期有效。帧级并行中 solver/detector 按 worker 隔离，Gaia 查询缓存由星表模块管理；线程预算由 Runtime 提供。

返回值与 `success` 一起检查，参数/匹配不足、CD 退化或不收敛逐帧保留原因。取消与失败由 adapter 上行，不能由 API 未声明取消参数推断长调用已经可中断。后续无合法 WCS 的帧不进入 Drizzle；合法其它帧状态单独记录。

### 验证入口与实现边界

[WCS 单元入口](../../../eng/tests/unit/test_wcs_plausibility.py)及[阶段一管线入口](../../../eng/tests/pipeline/test_stage_normalize.py)分别用于几何和实际调用检查。需要独立 WCS reader、留出 Gaia、非零 SIP、RA 回绕、手性、边角和真实无 WCS 头输入；内部往返自洽不证明绝对坐标正确。本页未运行这些验证，具体生产差异与已有证据边界以[实施地图](../../engineering/architecture/SCIENCE_IMPLEMENTATION_DELTA.md)为准。

## star-detection

**Normalize 科学源检测。**

本页登记检测库与八节点 `star-psf` 中的科学源生产。处理顺序与两类星点的区别由 [Normalize 科学正本](../../science/normalize/NORMALIZE_SCIENCE.md)规定；检测本身不是 WCS 已解或条件参考精度正确的证明。

### 入口、调用者与消费端

[公开头](../../../lib/algorithms/star_detection/include/star_detector.h)及 [sdet_api.cpp](../../../lib/algorithms/star_detection/src/sdet_api.cpp)提供两类入口：

| 路径 | 调用及职责 |
|---|---|
| `sdet_detect_ex` / `sdet_detect_ex_f64` | 图像盲检；IPV 用于生成私有 `solver_detections`，无已解 WCS 前提 |
| `sdet_detect_guided_ex_f64` | 接收调用方预测位置，逐位置质心/轮廓拟合；完整 WCS 后生产 `science_sources` |
| `sdet_create` / `sdet_destroy` | 管理检测参数/句柄；同一 handle 不并发使用 |
| `sdet_free_detect_ex` | 成组释放输出位置、通量、标志及 extras |

CLI 调用链为 [module_adapters.cpp](../../../lib/infrastructure/scheduler/src/module_adapters.cpp) 的 `p1_op_star_psf_impl → p1_guided_predict → pc_project_sky_to_pixel → sdet_detect_guided_ex_f64`。投影消费本帧已解 WCS 与 SIP，近似指向仅用于搜索/初值，不能代替最终预测。该步骤属于 `acsd.phase1.star-psf`，没有另一个独立 `star-detection` 生产调度节点。

输出经 source ID 与 PSF 参数关联，供 photometry、noise、Drizzle 使用；解算候选与科学源不互相冒充。实际跨节点载体含 `p1_sources.json`，完整 typed 对象目标见[数据合同](../../engineering/contracts/NORMALIZE_DATA.md)。

### 输入输出合同

输入图像为行主序 uint16 或 double；引导位置是 detector 像元坐标，外部 sky 为 ICRS degrees。内部数组中心遵循零基索引、FITS WCS 桥接加一；不得从头部注释中的半像元写法另造一次偏移。投影/adapter 必须按同一合同核对。

输出包含 x/y、flux、mag、saturated、has_saturated 和 extras；位置为 double，但既有 C API 的 flux/mag 输出数组是 float。检测 flux 的估计器身份与后续 PSF 积分通量分开，不能按相同字段名互换。

`SDetGuidedStats` 分列 predicted、dropped、fit_failed、rejected、fit_ok、output；去重/截断前后数量各有意义。单候选拟合失败可丢弃并计数；底层零预测域返回成功且 count=0，节点层再按所需下游能力判断。

### 参数、所有权与错误

配置 `star_detection` 提供 mode、Gaia 目录、查询/星数范围与诊断阈值；PSF 拟合样本上限和 `snr.max_sources` 交付上限不是同一个数。当前 mode 分支及可用 WCS 检查由 adapter 执行，不在库内查询星表。

库返回 0 表示调用完成，-1 表示参数/分配失败；候选丢弃用统计表达。调用方必须使用成组 free，视图有效期覆盖同步调用。候选拟合可按独立位置并行，宿主提供预算；不能仅按实现循环推断不同 worker 数逐位等价。

### 验证入口与边界

[检测科学分册](../../science/detection/STAR_DETECTION.md)说明召回、质心和选择偏差的定义。验证应以已知位置/亮度的注入场、错位预测、空域、边缘、饱和、拥挤与去重计数覆盖公开入口；[阶段一管线测试](../../../eng/tests/pipeline/test_stage_normalize.py)用于检查实际节点序。独立绝对 WCS 残差和 source SNR/参考精度仍需分别验证；当前差异统一见[实施地图](../../engineering/architecture/SCIENCE_IMPLEMENTATION_DELTA.md)。

## star-psf

**Normalize PSF 拟合与模型。**

PSF 节点把已解 WCS 引导的源测量转为逐星轮廓参数。空间、像元积分归一、有效域与通量估计器的目标由 [Normalize 科学正本](../../science/normalize/NORMALIZE_SCIENCE.md)和[数据合同](../../engineering/contracts/NORMALIZE_DATA.md)定义。

### 生产调用与接口

[adapter](../../../lib/infrastructure/scheduler/src/module_adapters.cpp) 的 `p1_op_star_psf_impl` 对应 `acsd.phase1.star-psf`，消费 `cleaned` 与 `wcs`，输出 `sources`、`psf`。科学源检测后，它将检测行送入 `dpsf_fit_batch_f64`。函数存在且由生产调用，不应写成未接节点。

公开头为 [dynamic_psf.h](../../../lib/algorithms/psf/include/dynamic_psf.h)。uint16 接口 `dpsf_fit`、`dpsf_fit_batch`，float 接口 `dpsf_fit_batch_f`，及 double 的 `dpsf_fit_batch_d` 返回 `DPSFFitResult`；分配型批结果统一用 `dpsf_free_results` 释放。

生产 compact 接口为：
`dpsf_fit_batch_f32/f64(image,width,height,detections,n_detections,params,out_psf_params,out_n_valid,out_status)`。调用方提供输出缓冲，返回 0 表示批调用完成，逐星结果仍需查状态；非法参数返回 -1。

### 字段、坐标与消费者

| 对象 | 字段与语义 |
|---|---|
| detections | double [N,6]，位置、检测 flux/mag、饱和状态；flux 估计器与 PSF flux 分名 |
| compact 参数 | double [n_valid,9]，B、A、cx、cy、sx、sy、theta、fwhm_x、fwhm_y |
| out_status | 长度 N，按输入检测索引；成功参数行按检测下标升序 compact，不能直接将参数第 i 行当输入第 i 星 |
| DPSFFitResult | 另有 status、mad、flux、eccentricity；B/A 为图像尺度，位置/宽度/FWHM 为 px，theta 为 rad |
| 参数配置 | fitRadius 为 px、maxIter 为次数、tolerance 为数值容差；实际消费以实现为准 |

像元中心沿 detector 零基合同，sky 关联沿 ICRS degrees。帧/source ID 与 compact 映射同时保留。photometry 和 noise 当前通过 sources 工作文件中的参数取数；独立 `psf` 端口尚无完整显式消费者，不能因此写成 PSF 数值完全无人消费。

逐星 Gaussian 检测宽度、Moffat4 拟合参数、PSF 残差与背景噪声是不同量。FWHM 或残差可作诊断，不能独自替代后续模板 h 或面亮度统计权重。总通量、窗内通量与截断策略须配对记录。

### 所有权、并行与错误

算法借用图像/检测数组；调用方持有 compact 参数和逐星状态。不同星拟合可并行，节点由 Runtime 注入预算；不建立独立资源来源。批级返回与星级 `DPSF_FIT_*` / `DPSF_PSF_STATUS_*` 分开处理，空窗/未收敛/分配失败不能当有效参数。

当前接口是逐星拟合；空间有效 PSF 求值、像元积分归一与噪声 completion 的真实覆盖由[差异地图](../../engineering/architecture/SCIENCE_IMPLEMENTATION_DELTA.md)维护。目标 `EffectivePsfModel` 不能仅靠帧中位宽度或 `psf_spatial_order` 名称宣称完成。

[pixel_integrated_psf.h](../../../lib/algorithms/psf/include/acsd/pixel_integrated_psf.h) 已提供 `pixel_integrated_moffat4_profile(request,out,diagnostics)`：request 指定 sx/sy/theta、零基参考中心、像元中心数组和求积阶数，借用输入数组，写调用方 vector；diagnostics 给实际集合包络及相邻阶自检差。completion 已调用此库，但当前调用传帧级圆化宽度。函数的像元积分能力不能代替空间 PSF 的真实参数与留出验证，相邻阶差也不构成严格真误差界。

### 验证入口

从本头文件批接口和[阶段一管线测试](../../../eng/tests/pipeline/test_stage_normalize.py)进入，检查已知轮廓恢复、非首行失败后的 compact/source ID 映射、边缘截断、饱和、空间变化及按位置积分归一。串并行、跨类型与真实模型误差需实际读数，本页不列未经复核的通过标签。

### 逐星拟合实现补充

原算法页的布局与调用内容集中在本节，数学轮廓、残差换算及误差解释见[Moffat4 数值方法](../../science/algorithms/normalize/STAR_PSF_ALGORITHMS.md)。

| 表示 | 列序与用途 |
|---|---|
| 科学/oracle 参数 | B,A,x0,y0,sx,sy,theta,residual_scale,q_psf；x0/y0 为局部偏移，没有生产块写出保证 |
| legacy orchestrator 块 | status,B,flux,cx,cy,fwhm,A,mad,eccentricity；全图坐标，mad 为截尾平均绝对残差 |
| 当前批 ABI | B,A,cx,cy,sx,sy,theta,fwhm_x,fwhm_y；成功行 compact |
| 当前 CLI JSON | 按 star_id 关联对象字段，不按某份九列布局直接解析 |

`DPSFFitResult` 当前为13个字段，包含完整双轴参数、mad、flux、eccentricity，不能沿旧文“12字段”分配。批返回的 `DPSF_PSF_STATUS_*` 分别表示OK、拟合失败、空窗、分配失败；它们与 `DPSF_FIT_*` 的OK、未收敛、非法参数、迭代上限是不同状态域。特别是数值2和3不能跨域解释；out_status 按原检测下标，参数行按成功星下标压缩。

当前拟合在 `dpsf_psf.cpp` 的 `moffat4_fit_tmpl_core` 装配有限样本、背景/振幅/宽度初值，调用 `lm_solve` 后检查参数和窗口，再派生结果。源码定位条目中的初始宽度0.15倍窗口宽、硬编码200轮/1e−8、宽度0.3下界及四候选角度回选属于该实现配置，不能把 `DPSFFitParams.maxIter/tolerance` 的声明当作它们已经由调用者控制。

逐星可并行、星内有固定累加，不由此保证任意ISA/优化/worker下逐位一致。旧文按批检查取消与“没有取消点”的描述并存，应沿当前循环核对；调用者取消不表示已在飞拟合能立即中止。完整原文和历史诊断保存在[来源档案](../../../实验/documentation-refactor/docs/SPATIAL_NOTES_SOURCE.txt)，不把旧实测数字或测试名字当本轮执行结论。

## photometry

**Normalize 测光与标度应用。**

测光消费同帧 cleaned 像素、已解 WCS/SIP、科学源及 PSF，使用 Gaia XP 与响应拟合线性标度，并恰应用一次。物理响应、参考约定和跨通带限制由[测光分册](../../science/photometry/PHOTOMETRY.md)及 [Normalize 科学正本](../../science/normalize/NORMALIZE_SCIENCE.md)拥有。

### 实际入口与交接

[module_adapters.cpp](../../../lib/infrastructure/scheduler/src/module_adapters.cpp) 的 `p1_op_photometry` 对应 `acsd.phase1.photometry`，调用 `acsd::photometry::fit_frame_photometry`。其 [FramePhotFitRequest/Result](../../../lib/algorithms/photometry/cpp/src/frame_photometry_fit.h)装配本地 Gaia、滤镜/QE、PSF 通量和完整 WCS，内部进入 `pc_calibrate_simple_with_gaia_f64_v2_qf`。

descriptor 声明 calibrated、sources、wcs 输入和 fluxes、photprov 输出；真实像元取 sources 指向的 cleaned 面，PSF 参数也随 sources 工作文件到达。当前载体含 `p1_phot.json` 和 photoapplied FITS。目标 `PhotometricSolution`、`PhotoAppliedPlane` 与独立 PSF 输入依[数据合同](../../engineering/contracts/NORMALIZE_DATA.md)，端口排序不能冒称完整 typed 取数。

### API、字段与单位

[photometric_calib.h](../../../lib/algorithms/photometry/cpp/include/photometric_calib.h)提供标量/双精度 Gaia 测光及 `pc_project_sky_to_pixel`；[frame_photometry_fit.h](../../../lib/algorithms/photometry/cpp/src/frame_photometry_fit.h)提供文件无关的单帧装配接口。

| 字段 | 消费语义 |
|---|---|
| psf_cx/cy、psf_flux/status/quality | 同一 source 行绑定；像元为 detector 零基，flux 带具体 PSF 估计器与积分单位 |
| crval/crpix/cd/sip | ICRS degree、FITS 一基参考像元、degree/pixel 和完整 SIP |
| filter_name、qe_name、曲线身份 | 实际响应积分依据；滤镜头标签不能代替曲线/QE 内容 |
| k_photo、fit_ok、failure_scope | 标度、是否真实拟合得到、失败属于帧或环境；占位 scale=1 不表示已施加 |
| sigma_residual_dex | 匹配对数残差散度；不是像元噪声或参考 SNR |
| spatial | 空间乘性场与支持；与全局标度共同决定实际像元变换 |
| synthetic_colour_* | 星表与响应的天区合成颜色摘要，mag/星数/布尔；不是独立帧零点或探测器增益测量 |

`photscale_fit` 记录拟合结果，`photscale_detail` 与实际已施加帧集合配对；不能仅凭拟合成功判断某个下游面已缩放。线性像元、参考通量及协方差须按同一次应用配对；BUNIT 随真实数值标度表达。Gaia 恒星拟合不能自动校正未知星云 SED。

### 所有权、配置与错误

request 借用像素、PSF 与 SIP 数组，同步调用期保持有效；result 由调用方持有。adapter 负责 AIO、逐帧应用与 provenance，算法不以 output_dir 猜上游。拟合按帧组织，线程预算来自 Runtime；返回码、`fit_ok` 和具名 reason 分开处理。

配置主要为 `photometry.fit`、block `filter_passband`、Gaia 目录、响应/QE 和空间拟合参数；`photometry.frame_overrides` 已有逐输入路径/basename/key的首项命中绑定，仍不等于严格输入 occurrence 序号。当前响应、参考 SED、标度 application ID 与单位的具体覆盖见[实施差异地图](../../engineering/architecture/SCIENCE_IMPLEMENTATION_DELTA.md)。本页不另立参考通带、公共零点表或数据组合禁令。

### 验证入口

[光度合成测试入口](../../../eng/tests/synthetic/test_p1_photometry.py)与[生产阶段一入口](../../../eng/tests/pipeline/test_stage_normalize.py)用于不同层次核对。验证应检查 PSF/source 行绑定、非零 SIP、已知 SED/响应、scale 一次与协方差二次幂、缺响应/拟合不足状态和颜色诊断独立性。源码接线不等于已完成上述实测。

## noise-snr

**Normalize 噪声与参考精度。**

noise 库拥有平面条件噪声和 Drizzle 后球面参考 GLS 的数学实现。三种源 SNR/帧摘要/参考控制点的区别，以及点源精度到面亮度精度的传递，由[噪声正本](../../science/noise_snr/NOISE_SNR.md)统一定义；接口表达依 [Normalize 数据合同](../../engineering/contracts/NORMALIZE_DATA.md)。

### 生产路径与消费者

[adapter](../../../lib/infrastructure/scheduler/src/module_adapters.cpp) 的 `p1_op_noise` 是 `acsd.phase1.noise-snr` 节点，实际读取 cleaned、sources、photprov，产出平面噪声诊断和兼容帧级 SNR。`fluxes` descriptor 边用于合成 IR，在生产没有对应读取点。

Drizzle 节点获取同次球面几何后，经 [normalize_precision.cpp](../../../lib/infrastructure/scheduler/src/normalize_precision.cpp) 的 `stage_frame_precision` 调用 `complete_spherical_reference`；没有 noise←Drizzle 调度环。球面模型/controls 随 staging 交 writer 发布，不能把 `p1_snr.json` 改名就视作 canonical 精度。

根 CMake 已包含 `acsd_phase1_noise`；[sparse_reference.cpp](../../../lib/algorithms/noise_snr/cpp/src/sparse_reference.cpp)已有球面计算。构建接入、实际调用、模型完整和独立科学支持分别判断。

### 接口与数据

| 入口/类型 | 作用及所有权 |
|---|---|
| [snr_estimator.h](../../../lib/algorithms/noise_snr/cpp/include/snr_estimator.h) 的 `snr_noise_model_v1` / `_f64` / `_fill` / `_free` | patch 背景模型与平面方差求值；model 经配对 free 释放，填充数组由调用方提供 |
| [snr_frame_science.h](../../../lib/algorithms/noise_snr/wrapper_phase1/snr_frame_science.h) 的 `compute_snr_frame_science` | 兼容逐源行到帧级诊断/参考记录，不自动成为球面 controls |
| [sparse_reference.h](../../../lib/algorithms/noise_snr/include/acsd/sparse_reference.h) 的 `complete_spherical_reference(windows,cfg)` | 同步读取局部平面窗口与实际 R，返回 precision、局部模型载荷和逐点原因 |
| `summarize_frame_reference(controls)` | 对可用 controls 生成独立帧摘要；不重缩放 controls |
| [normalize_data.h](../../../lib/include/acsd/normalize_data.h) | reference、模型身份、availability/accuracy、control 和 summary 类型 |

`PlanarPatchWindow` 保存 detector 零基中心、源像元 row-major indices、P、条件方差、源 Ω/drop A、NESTED 叶索引及稀疏 R；几何为 sr、R 为 1/sr。球面 h/C/Z 必须来自同一观测与索引顺序。控制点 sky 是 ICRS degree；F_ref 为声明积分单位，V_F 为其平方，SNR 无量纲，稀疏计算 FP64。leaf 0 合法，不是未知哨兵。

`SphericalCompletionConfig` 分别承载 reference、背景 nuisance、原生 gain、实际 scale、母版状态、秩容差及矩阵预算。gain 单位 e-/ADU；不能将光度缩放后数组直接除原生 gain 而漏标度幂次。源 Poisson、天空/读噪、母版和共同参数须分项，已有经验总 RMS 不再重复加读噪。

### 状态、配置与实现边界

`snr.reference` 的 canonical 参考路径与兼容 `reference_flux_adu`、参考星等诊断分名。现有 Catalogue 的 `snr_psf` proxy、测光残差派生 SNR、规整 `sparse_snr_layer` 不能仅凭数值可读就升级 canonical。正式产品载体为 writer 发布的 `frame_science.json`。

合法信号可以伴随 precision unavailable；损坏、非法 shape、非有限计算和不可辨识分别表达。available 的 accuracy/omitted_terms 如实保留，不能把缺项当零。I_F=SNR²/F_ref² 属点源通量精度，不能直接命名面亮度权重。Runtime、SED、空间 PSF、局部模型及 reader 的具体限制以[差异地图](../../engineering/architecture/SCIENCE_IMPLEMENTATION_DELTA.md)为单一登记。

大面由调用方/Runtime 预算管理，局部窗口和模型不长期复制全帧。线程和取消由 adapter/宿主提供；本同步 completion 无独立资源预算或文件发布职责。

当前常规 completion 使用列主元 Householder QR 行空间投影，背景有效秩与通量可辨识性分别求；大工作集仍有 Jacobi 分支。scheduler 已调用 PSF 库的像元积分，factor writer 已使用 v2。这些实现进展不等于空间 PSF、秩阈值、近共线误差与独立 reader 已完整，避免引用迭代之前的实现描述。

### 验证入口

[生产 GLS 单元入口](../../../eng/tests/unit/sparse_reference_gls_test.cpp)、[噪声合成入口](../../../eng/tests/synthetic/test_p2_absolute_snr.py)、[sidecar 合同入口](../../../eng/tests/conformance/normalize_frame_science_schema_test.py)和[已保存局部验证记录](../../../实验/normalize-precision-p5/evidence/ROUND1_VERIFY.log)分别覆盖不同问题。需区分背景已知/共同拟合、异方差、秩亏、非单位 scale、缺 gain、leaf 0、PSF 截断及 Monte Carlo 的实际条件；已有日志不支持全 CLI、真实 SED/空间 PSF或完整误差链已验证。

## drizzle

**Normalize 球面 Drizzle。**

Drizzle 将本帧已解 WCS/SIP 下的已施加光度线性面映射到 HEALPix NESTED。面积、归一和协方差定义属于 [Drizzle 正本](../../science/drizzle/DRIZZLE.md)；球面参考精度完成边界属于 [Normalize 正本](../../science/normalize/NORMALIZE_SCIENCE.md)。

### 实际入口与上下游

[module_adapters.cpp](../../../lib/infrastructure/scheduler/src/module_adapters.cpp) 的 `p1_op_drizzle` 对应 `acsd.phase1.drizzle`。descriptor 消费 calibrated、wcs、photprov、sources、snr，输出 stacked。实际逐帧选面须核对 photprov 与 photoapplied；端口名 calibrated 不表示允许重复缩放或忽略 cleaned 来源。

生产调用 `hp_drizzle_run_phase1_hips`，引擎和 sphere sink 通过统一 AIO 写 HiPS。相同几何贡献捕获后，由 `stage_frame_precision → complete_spherical_reference` 完成球面参考模型，局部载荷及 sidecar staging 交 writer。此调用存在，不能继续写成 descriptor 未接或 canonical producer 不存在；具体限制集中见[实施地图](../../engineering/architecture/SCIENCE_IMPLEMENTATION_DELTA.md)。

### 公开接口与几何载荷

[hp_drizzle_api.h](../../../lib/algorithms/drizzle/healpix_drizzle/hp_drizzle_api.h)公开帧接口 `hp_drizzle_run_phase1_hips` 及 `_begin` 异步 job 形态，并保留 `hp_drizzle_run`、`hp_drizzle_run_hips`、文件/反向接口。不同入口的能力和返回不能互相推断。

| 输入/输出 | 数值合同 |
|---|---|
| PipelineFrame data/header | 行主序图像及本帧完整 WCS/SIP；FITS CRPIX 一基，内部像元中心零基 |
| nside、ordering | 2 的幂和 NESTED；叶阶 log2(nside)，512 tile 阶比叶阶小 9 |
| pixfrac、Ω、A、a | 无量纲收缩及未收缩/收缩/交叠 sr；A 不能无条件用 pixfrac²Ω 代替 |
| 信号算子 R | 作用于积分像元的系数，1/sr；归一分母须与同次正式叶完整样本一致 |
| signal | 实际积分线性单位/sr；负值和零合法，无数据 NaN |
| variance/ivar 辅助输入输出 | 只在真实模型可用时参与；平方传播与 support 分名 |
| precision staging | 控制点、h/C/Z/R、支持和具名状态，算法归 noise 库而非 scheduler 重写 |

[drizzle_engine.cpp](../../../lib/algorithms/drizzle/healpix_drizzle/drizzle_engine.cpp)承担交叠/累加，[astro_sphere_sink.cpp](../../../lib/algorithms/drizzle/healpix_drizzle/astro_sphere_sink.cpp)适配写出。分配通量守恒、常值保持、支持面积、目标孔径通量和真实曲边误差是不同检验，不能以单个闭合残差替代全部。

### 配置、寿命与错误

nside/目标尺度、pixfrac、ordering、精度和存储请求来自配置与实际解析；不在卡片另立默认值。生产可按帧、源行和 tile 组织并行，预算由 Runtime 提供；捕获窗口、tile 累加器及异步写 job 在所有消费者/wait 完成前保持有效。不同 worker 数归约序可变，不宣称无条件逐位一致。

返回 0 表示调用成功，非零读取 result 的 error_msg 并上行；不能直接把各入口局部整数当 CLI 退出码。无解 WCS 不生成伪球面，非法参数、AIO 失败与 precision unavailable 分别处理。开发快照只观察，不供 completion 或 writer 补正式输入。

### 验证入口

[守恒映射合成入口](../../../eng/tests/synthetic/test_p3_conservative_map.py)与[实际阶段一入口](../../../eng/tests/pipeline/test_stage_normalize.py)分别检查算子和 production。核对常值、drop 分割、真实 HEALPix 曲边、源/叶尺度比、12 面/极区、NaN、孔径量与相邻协方差；precision 另核对完整 R 与模型条件。本页不据接口或已有局部日志宣称全链已完成。

## writer

**Normalize 帧产品发布。**

本节点检查 Drizzle 已写的逐帧 HiPS，发布科学模型与唯一 `frame_science.json`，登记帧产品。它属于 [Normalize 八节点链](../../engineering/architecture/NORMALIZE_PIPELINE.md)，并非把 stacked 另导出一张平面 FITS。

### 实际入口与数据交接

[adapter](../../../lib/infrastructure/scheduler/src/module_adapters.cpp) 的 `p1_op_writer` 对应 `acsd.phase1.writer`，在 Drizzle 后执行，消费逐帧 `p1_stack.json`、已完成 HiPS 和 precision staging，产生帧摘要/产品引用。descriptor 目前仍以 `stacked → fits` 命名，不能据此改写真实球面产品身份。

目标 writer 输入为同帧同版本的 WCS、photprov、noise、stacked、Drizzle 模型和参考 controls；Runtime 类型、occurrence、状态和发布范围见 [Normalize 数据合同](../../engineering/contracts/NORMALIZE_DATA.md)及[接口规格](contracts.md)。

### 统一 serializer/reader 与发布 API

[normalize_science.h](../../../lib/algorithms/integration/normalize_science/include/acsd/normalize_science.h)和[实现](../../../lib/algorithms/integration/normalize_science/src/normalize_science.cpp)提供：

| 入口 | 输入、输出与边界 |
|---|---|
| `write_local_model_file(root,payload,out)` | 序列化局部模型及 hash；bool 和 ModelWriteResult |
| `write_frame_science_document(root,doc,out)` | 唯一 sidecar serializer；未知值 null，稀疏 FP64 round-trip |
| `read_frame_science_document(path)` | FrameScienceReadResult：ok/error、present、supported、doc、findings |
| `publish_staged_frame_science(staging,root,out)` | 核对/复制模型 hash，原子写 sidecar 并回读；由 writer 决定调用时机 |
| `load_local_model_payload` / `reconstruct_factor_covariance` | 读取模型，核对因子/CSR结构并在预算内重建 C；不同于 sidecar 解码 |
| `verify_frame_science_factor_models` | 对模型引用组织因子核对；当前通用 reader 尚未调用它，不能据函数存在宣称发布回读已执行全部模型校验 |

生产 `p1_op_writer` 有实际发布调用。sidecar 指向 signal、WCS、reference、模型、controls、summary 和状态；model refs 使用产品内相对路径/hash。sky 为 ICRS degree、CRPIX 为 FITS 一基、叶号按 NESTED 实际叶阶；F_ref/V_F/SNR 与真实单位、模板/协方差配对。精度缺项不允许用零值填满。

### 所有权、错误与实现边界

writer 是 I/O 节点，单帧产品只有一个最终发布者。输入图像/模型由 Runtime/AIO 或 staging 持有；writer 不重新估计 PSF、噪声或光度，不创建另一份 canonical 真源。JSON/FITS 工作文件仍是实际依赖，开发诊断不能供 writer 补科学数据。

合法 signal 且 precision unavailable 是完整但能力受限的产品；损坏、reader findings 与发布失败是不同状态。运行清单应枚举全部输入 occurrence 和失败/取消原因，不能仅枚举成功目录。当前逐文件 hash/原子写和回读不等于 signal+全部模型+sidecar+完成 manifest 已形成整帧事务；唯一现行差异正文见[实施地图](../../engineering/architecture/SCIENCE_IMPLEMENTATION_DELTA.md)。

取消与 AIO 错误按 Runtime/统一 I/O 上行；写出后存在文件不足以说明可作为完成产品消费。模型能力状态与磁盘完整性也不能互相代替。

### 验证入口

[sidecar schema 检验](../../../eng/tests/conformance/normalize_frame_science_schema_test.py)与[生产阶段一测试](../../../eng/tests/pipeline/test_stage_normalize.py)分别检查形状和实际节点。独立验证还需模型 hash/shape/引用、读写语义、applied application ID、未知 schema、缺 sidecar 兼容，以及发布中途失败/取消后不出现伪完成产品。本页未运行测试，不以 serializer/reader 存在宣称这些合同已全部满足。

## session

**Normalize 局部会话。**

本页登记进程内 `p1_session_*` facade。完整用户命令由 [Normalize 生产链](../../engineering/architecture/NORMALIZE_PIPELINE.md) 的八个节点执行；本 facade 仅覆盖读取、校准、cosmetic 和校验写出。

### 入口与调用边界

公开头为 [p1_session.h](../../../lib/phase1_session/p1_session.h)，实现为 [p1_session.cpp](../../../lib/phase1_session/p1_session.cpp)。接口为 `p1_session_create(host,out)`、`p1_session_validate(handle,config_json)`、`p1_session_run(handle,config_json,async_io_depth)`、`p1_session_inspect(handle,out_manifest_json)`、`p1_session_destroy(handle)`；返回 `acsd_status`。配置与 inspect 文本均用 `acsd_span_u8` 传递。C++ `acsd::phase1::last_error` 提供诊断摘要。

[scheduler adapter](../../../lib/infrastructure/scheduler/src/module_adapters.cpp) 保留 `P1Api` 会话接入能力；CLI 的 `run_phase1_block` 经 `P1NodeModule` 分别调用八个 `p1_op_*`，不会因持有本组五函数就获得完整 WCS、测光、参考精度或 HiPS 能力。本页不登记独立生产 DAG 节点。

### 数据、所有权与执行

| 输入或输出 | 合同 |
|---|---|
| host | allocator、logger、cancel、budget，创建时注入；有效期覆盖会话 |
| config | lights、masters、output_dir 及校准/cosmetic 参数；允许字段和类型由 validate 实现检查 |
| 校准结果 | 行主序图像，内部像元中心用零基索引；单位随校准输入声明 |
| manifest | 本局部会话的阶段状态、文件和错误；其它科学域不可用，不冒称完整 Normalize 产品 |

创建者唯一拥有 handle，同一 handle 串行使用，不同 handle 可分别运行。`inspect` 输出经 host allocator 分配，调用方经同一 host free 释放。AIO 图像使用 `aio_free_image_data`；算法输出缓冲由调用方管理。

`async_io_depth` 接受 0、1、2；有预算时预读下一帧。校准线程数由 host budget 注入，取消在帧边界检查。此局部会话的线程/取消行为不能代替八节点链的实际接线说明。

### 错误与验证入口

返回码使用 `ACS_ERR_*`，错误由会话 manifest 和 `last_error` 说明；失败不写成 complete。校准与 cosmetic 的状态须按实际调用分别记录，未启用修复与已修复不能混写。

验证入口为本头文件五函数和 [Normalize 测试入口](../../../eng/tests/pipeline/test_stage_normalize.py)；前者检查生命周期、host 分配配对、非法配置和取消，后者检查完整命令。测试入口存在不表示已执行或得到科学支持。完整数据/能力要求见 [Normalize 数据合同](../../engineering/contracts/NORMALIZE_DATA.md)；具体实施边界见[差异地图](../../engineering/architecture/SCIENCE_IMPLEMENTATION_DELTA.md)。
