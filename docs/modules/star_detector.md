# Module: star_detector

> P1-STAR-DOC 事实修订（2026-09-07）：本页为 legacy 诊断页（ACTIVE_INFORMATIVE，
> 不属 gen_module_readmes.py 生成范围）。模块冻结合同页 =
> docs/modules/registry/astrocs.phase1.star-detection.md（手写，P1-STAR-DOC）；
> 合同入口 = lib/star_detector/README.md r1（CONTRACT_READY）+ module.yaml
> （MOD-astrocs-phase1-star，module_id=astrocs.p1.star_detection，
> dll_target=astrocs_p1_star_detection.dll，entrypoint=MISSING）。本页旧描述
> 以冻结合同为准修订，禁止反向改写合同层。

## 职责

Phase1 单帧 light 权威星点检测：peaker 七步候选（11×11 局部极大/3×3
meanhigh/二阶导零交叉 Sr,Sc/振幅 Ar,Ac/盒半径 R=ceil(3.7172·S)/对称门/候选
去重）+ Moffat4（GSL trust-region LM，7 参数）逐候选拟合 + 饱和星
（edge-walking 中心、A>dynrange 标记）+ mag 排序去重截断（SDetParams.maxStars）；
FP32/FP64 双通道（sdet_detect_ex / sdet_detect_ex_f64）；一帧一次权威检测
（API-P1-003），输出 star_det FLOAT64[N,6] 权威块。

## 非职责

不做 PSF 批拟合/θ 消歧（dynamic_psf）；不做匹配/WCS 求解（ipv/gaia_client）；
不做最终测光（P1-PHOT）；不做背景/cosmetic 校正（消费 cleaned 帧）。

## Public API

API-STAR-001（docs/contracts/PUBLIC_API.md）：9 导出符号
`sdet_create/sdet_destroy/sdet_detect/sdet_free_coords/sdet_detect_debug/
sdet_free_debug_maps/sdet_detect_ex/sdet_detect_ex_f64/sdet_free_detect_ex`
（唯一权威签名头 lib/star_detector/include/star_detector.h:1-73）。

## Data contract

DATA-P1-STAR（docs/contracts/DATA_SEMANTICS.md §17）：输入 image
[uint16|double] [h·w] ADU 行主序；输出 x/y double pixel（0-based，像素中心=
索引+0.5）、flux float ADU（正常星=振幅 A）、mag float（NaN=无效）、
saturated/has_saturated int 0/1；编排序列化 star_det FLOAT64 [N,6]（列
x,y,flux,mag,saturated,has_saturated）+ star_det_psf_compat FLOAT32 [N,4]。

## Ownership

输出十数组模块 malloc，调用方唯一经 `sdet_free_detect_ex` 整组释放
（sdet_api.cpp:2357-2373），禁止逐数组 free。

## Thread safety

handle 级互斥使用（单 handle 单线程，无内部锁）；OpenMP 三处（行差分
:448、入口转换 :2321-2325、候选拟合 :2042-2044 dynamic + reduction），
dedup/sort/maxStars 截断串行，输出 bitwise 与线程数无关；ThreadBudget
接线与取消检查点缺失已登记（DISP-STAR，整改归 P1-STAR-IMPL）。

## Errors

入口 rc：0=成功（含 0 星空场：输出全 NULL + count=0，非错误）；−1=参数
无效/句柄 NULL/分配失败。拟合级 SDET_FIT_*（非 OK 候选丢弃）；质量门
reject_star SfError 五码；编排级 det_ret≠0 或 count≤0 → 退出码
STAR_DETECT_FAILED（orchestrator.cpp:2200-2212）。「无星点 → NO_DATA」旧
描述作废。

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
（现状无共址测试套件；lib/star_detector/test/sdet_fp64_test.cpp 为
NON_PRODUCTION_TOOL_ONLY 手工验证程序）。

## Source files

lib/star_detector/（生产源 src/sdet_api.cpp:1599-2353 sdet_detect_impl，
合同头 include/star_detector.h）；lib/phase1/stars/（P1-003 桥接层独立
sigma-clip 实现，与生产 sdet_api.cpp 非同一算法路径，matrix legacy_paths
第二路径，整合归 P1-STAR-IMPL）。
