# Module: star_detector

> 上游：ASTROCS_DESIGN.md §8.4（模块与 ABI）

> 本页为诊断页（ACTIVE_INFORMATIVE，不属 gen_module_readmes.py 生成范围）。模块冻结合同页 =
> docs/modules/registry/astrocs.phase1.star-detection.md；
> 合同入口 = lib/algorithms/star_detection/README.md r1（CONTRACT_READY）+ module.yaml
> （MOD-astrocs-phase1-star，module_id=astrocs.p1.star_detection，
> dll_target=astrocs_p1_star_detection.dll，entrypoint=MISSING）。本页
> 以冻结合同为准，改动一律从合同层发起。

## 职责

Phase1 单帧 light **全图盲检测**（本实现 = 最高设计 §4.2 所述全图盲检测路径的生产源；**权威检测范式 = 星表引导拟合**——检测定义域是
星表位置、用本帧 WCS 反向投影 Gaia 星表，见 `ASTROCS_DESIGN.md` §4.2；全图盲检测连通域**不是**权威路径）：peaker 七步候选（11×11 局部极大/3×3
meanhigh/二阶导零交叉 Sr,Sc/振幅 Ar,Ac/盒半径 R=ceil(3.7172·S)/对称门/候选
去重）+ Moffat4（GSL trust-region LM，7 参数）逐候选拟合 + 饱和星
（edge-walking 中心、A>dynrange 标记）+ mag 排序去重截断（SDetParams.maxStars）；
FP32/FP64 双通道（sdet_detect_ex / sdet_detect_ex_f64）；一帧一次权威检测
（API-P1-003），输出 star_det FLOAT64[N,6] 权威块。

## 非职责

不做 PSF 批拟合/θ 消歧（dynamic_psf）；不做匹配/WCS 求解（ipv/gaia_client）；
不做最终测光（P1-PHOT）；不做背景/cosmetic 校正（消费 cleaned 帧）。

## Public API

API-STAR-001（docs/contracts/PUBLIC_API.md）：10 导出符号
`sdet_create/sdet_destroy/sdet_detect/sdet_free_coords/sdet_detect_debug/
sdet_free_debug_maps/sdet_detect_ex/sdet_detect_ex_f64/sdet_detect_guided_ex_f64/
sdet_free_detect_ex`（签名头正本 lib/algorithms/star_detection/include/star_detector.h，
按符号名定位；全量清单见 docs/architecture/api_inventory.csv）。

`sdet_detect_guided_ex_f64` = **权威路径入口**（星表引导拟合）：调用方给出星表预测
位置数组（本帧近似 WCS 把星表逆投影到像素域的结果），只对该位置做饱和判定、σ 估计、
椭圆高斯拟合与既有质量门；拟合失败**直接丢弃**（不计虚警、不报错）。
统计量 `SDetGuidedStats{n_predicted,n_dropped,n_fit_failed,n_rejected,n_fit_ok,n_output}`
用于把「定义域 / 丢弃 / 拟合失败 / 被拒 / 通过」逐级可审计。`n_pred=0` 返回 rc=0 +
count=0（空定义域非错误）；指针参数非法返回 −1。输出仍由 `sdet_free_detect_ex` 整组释放。

## Data contract

DATA-P1-STAR（docs/contracts/DATA_SEMANTICS.md §17）：输入 image
[uint16|double] [h·w] ADU 行主序；输出 x/y double pixel（0-based，像素中心=
索引+0.5）、flux float ADU（正常星=振幅 A）、mag float（NaN=无效）、
saturated/has_saturated int 0/1；编排序列化 star_det FLOAT64 [N,6]（列
x,y,flux,mag,saturated,has_saturated）+ star_det_psf_compat FLOAT32 [N,4]。

## Ownership

输出十数组模块 malloc，调用方唯一经 `sdet_free_detect_ex` 整组释放
（sdet_api.cpp:2357-2372），释放一律经该接口整组进行。

## Thread safety

handle 级互斥使用（单 handle 单线程，无内部锁）；OpenMP 四处：行差分背景噪声
估计、入口像素类型转换、盲检测候选拟合（dynamic + reduction）、星表引导候选拟合
（同款 dynamic + reduction，每线程私有 LM 工作区）。dedup/sort/maxStars 截断串行，
输出 bitwise 与线程数无关；ThreadBudget 接线与取消检查点缺失已登记
（DISP-STAR，整改归 P1-STAR-IMPL）。

## Errors

入口 rc：0=成功（含 0 星空场：输出全 NULL + count=0，非错误）；−1=参数
无效/句柄 NULL/分配失败（`sdet_detect_guided_ex_f64` 在 `pred_x/pred_y` 为 NULL 且
`n_pred>0` 时同样返回 −1）。节点侧的权威路径 fail-closed 语义（星表缺失/空/部分装载、
取向先验缺失、0 星存活）见 docs/plugins/algorithms_phase1/03_star_detection.md §7.1。拟合级 SDET_FIT_*（非 OK 候选丢弃）；质量门
reject_star SfError 五码；编排级 det_ret≠0 或 count≤0 → 退出码
STAR_DETECT_FAILED（orchestrator.cpp:2200-2212）。

## Science IDs

SCI-P1-STAR-001（docs/science/STAR_DETECTION.md，本任务冻结层，共享 SCI
引用不改动）；ALG-STARDET-001（docs/algorithms/STAR_DETECTION_ALGORITHMS.md
§11 逐符号锚）；DATA-P1-STAR（DATA_SEMANTICS §17）；API-STAR-001
（PUBLIC_API）。「SCI-PSF-001 输入侧」为共享 SCI 引用关系（检测为 PSF 拟合
与 plate solve 提供候选/中心），保留于 ALG 文档 §1。

## Tests

TEST-STAR-DESIGN-001（STAR_DETECTION_ALGORITHMS §11.4，冻结测试设计：
合成场质心/FWHM/召回/虚警、饱和/混合/边缘专项、确定性 bitwise、FP64 独立
oracle、负例、回归锚）；可执行 TEST-P1-STAR-001 由 P1-STAR-TEST 落地
（现状无共址测试套件；lib/algorithms/star_detection/test/sdet_fp64_test.cpp 为
NON_PRODUCTION_TOOL_ONLY 手工验证程序）。

## Source files

lib/algorithms/star_detection/（生产源 src/sdet_api.cpp:1599-2353 sdet_detect_impl，
合同头 lib/include/star_detector.h）；lib/algorithms/star_detection/wrapper_phase1/（P1-003 桥接层独立
sigma-clip 实现，与生产 sdet_api.cpp 非同一算法路径，matrix legacy_paths
第二路径，整合归 P1-STAR-IMPL）。
