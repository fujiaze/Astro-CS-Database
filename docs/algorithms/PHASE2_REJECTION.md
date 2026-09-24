# Phase2 Rejection Algorithms（P2-REJ / astrocs.p2.rejection）

> 上游：ASTROCS_DESIGN.md §5.5（逐像素排异）

> ID: ALG-P2-REJ-001  状态: CONTRACT_READY
> 模块: lib/algorithms/coverage/src/rejection.cpp（2949 行，astrocs_phase2 静态库
> 成员，根 CMakeLists.txt:336-346/:340）+ 唯一权威签名头
> lib/algorithms/coverage/include/astro/phase2/rejection.h（595 行）
> 权威: 本文档（算法级逐符号锚）。SCI 上游: SCI-REJ-001
> （docs/science/REJECTION.md，FROZEN，集合
> SCI-REJ-001..008，零改动；descriptor 占位 SCI-P2-REJ-001 ⇒
> SCI-REJ-001 映射声明见 §11.5）。共享 L2: ALG-REJ-001
> （docs/algorithms/REJECTION_ALGORITHMS.md，DERIVED，零改动，语义
> 承接见 §12）。DATA: DATA-P2-REJ（DATA_SEMANTICS §22）。API:
> API-P2-REJ-001（PUBLIC_API.md）。TEST: TEST-P2-REJ-001（设计冻结
> 面=本文档 §11.4；可执行落地归 P2-REJ-TEST）。迁移目标
> astrocs_p2_rejection.dll 为矩阵合同值（MISSING），由 P2-REJ-IMPL
> 建立，本文件不声明 IMPLEMENTED；descriptor 占位
> module_id=astrocs.phase2.reject（module_adapters.cpp:705-724）由
> P2-XX-INT 对齐，不作冻结依据。
> 关联: DATA=DATA-P2-REJ（docs/contracts/DATA_SEMANTICS.md §22）；API=API-P2-REJ-001（docs/contracts/PUBLIC_API.md 末节）；MOD 页=docs/modules/phase2_rej.md + registry docs/modules/registry/astrocs.phase2.reject.md（手写合同页）；TEST 登记面=registry 页 §独立 synthetic 验证节（TEST-P2-REJ-DESIGN-001 设计冻结 VERIFIED）。

## 1 目的与非目标

- **目的**：每像素候选栈排异决策——eligibility 单路径 gather（strided
  frame-major）→ planning 层 AUTO 一次解析（nominal n 路由）→ 显式
  方法核（10 方法，per-sample reason 与 stack-level status 分离）→
  large_scale 结构生长后处理（trail 扩张，compact cosmic 不生长）。
  本文件为阈值/迭代权威锚定文档（rejection.cpp:1-11 冻结头注释
  "本文件为阈值/迭代权威实现，禁止阈值漂移"）。
- **非目标**：不合并/积分样本（SCI-INT-001 / ALG-P2-INT-001）；不
  做权重策略（weights 数组外置，由 Stage2 按该天球像素对应帧集合现场计算派生量；RCR 核
  消费同栈 weights 数组属官方加权语义，非策略）；不做像素外结构
  重建（large_scale 仅对已拒 mask 做 8 邻域扩张，只增不减）；无
  session 依赖（无状态纯函数）；不做瞬变/卫星语义区分（SCI §1
  非目标）；单帧无排异（n=1 进 UNDERDETERMINED 白名单）。

## 2 符号与单位（权威=本表 + DATA_SEMANTICS §22 + SCI §3）

| 符号 | 含义 | 单位/dtype | 锚 |
|---|---|---|---|
| `values[i]` | eligible 候选科学值（kernel 工作域输入） | 面亮度 ADU·sr⁻¹，f64（gather f32 源→f64 提升） | rejection.h:369 |
| `weights[i]` | 候选科学权重（可空=等权） | (ADU·sr⁻¹)⁻²（数值域；策略在调用方），f64 | rejection.h:370 |
| `frame_ids[i]` | 稳定帧标识（ESD tie-break/确定性） | 无量纲 u64 | rejection.h:371 |
| `count` | 候选数（=资格后 n_eff） | 无量纲 u32 | rejection.h:372 |
| `reasons[i]` | per-sample 判定 | u8 0..3（P2RejectReason） | rejection.h:94-99/:384 |
| `status` | stack-level 终态 | int 0..7（P2RejectStatus） | rejection.h:102-111/:389 |
| `n`（nominal） | planning 层几何可贡献数（一次解析） | 无量纲 u32 | rejection.h:229/:223 |
| `med/scale` | 工作域中位数/尺度 | 与 `values` 同标度（面亮度 ADU·sr⁻¹；MEDIAN_SCALE 无量纲化后 1） | rejection.cpp:2093-2105 |
| `σ_lo/σ_hi` | 低/高侧阈值（sigma 族） | 无量纲 z 阈值（4.0 低 / 3.0 高） | rejection.cpp:1229（默认） |
| `plow/phigh` | percentile 判据带分数 | 无量纲（0.2 低 / 0.1 高），带宽 = 分数 × \|median\| | rejection.cpp:1237（默认）/ :1817-1818 |
| `alpha` | ESD 显著性水平 | 无量纲 0.05 | rejection.cpp:1236（默认） |
| `iterations` | 外层迭代计数（kernel 计量） | 无量纲 u32 | rejection.h:388 |

权重语义（调用方构造，本层无知）: 逐样本 ivar（权重是阶段二按该天球
像素对应帧集合现场算出的派生量，经
`source_indices` 回映射原始 slot，stage2.cpp:1098/:1361/:1379）；
null → 等权。reducer 只消费权重数组本身（与 ALG-P2-INT-001 §2
同一政策）。

## 3 逐符号锚（rejection.cpp 2950 行 / rejection.h 595 行，实测；锚 = `grep -n` + 花括号配对，禁止手抄他版行号）

| 符号/段 | 锚（rejection.cpp） | 语义 |
|---|---|---|
| 冻结头注释 | :1-12 | 阈值表 + "禁止阈值漂移"（SCI-REJ-*/ALG-REJ-001..008 锚定） |
| ibeta_cf / ibeta | :32-64 / :66-76 | Lentz 连分数 I_x(a,b)（NR betai）；lgamma 域 |
| t_cdf / t_quantile | :79-84 / :87-97 | Student-t CDF（对称）；分位数=二分 80 轮 [0,40] |
| norm_cdf / norm_quantile | :100-102 / :107-156 | 正态 CDF / Φ⁻¹（Acklam 有理逼近 + 二分） |
| kRcrSSDLUnityCF / kRcrSSUnity / kRcrSSConstants | :178 / :194 / :322 | RCR 官方 frozen 查找表（101/1001/2×8） |
| rcr_is_equal | :326-331 | 相等容差 rel ≤1e-8（官方 isEqual） |
| rcr_distinct_values | :334-354 | distinctValuesCheck：flagged 中 ≥3 不同值 |
| rcr_get_median / _w | :357-368 / :371-387 | 官方 getMedian（偶数平均/奇数中位；加权同步排序） |
| rcr_inverf | :390-398 | erfcCustom 逆（A&S 7.1.26 16 次幂有理近似）；rcr_get_xvec/_w :400-410/:412-426、rcr_count_under_one :428-434 |
| rcr_origin_regression(/_w) | :436-445 / :447-458 | 过原点回归 σ/τ |
| rcr_mfinder(/_w) | :460-518 / :520-581 | 官方 broken-stick 折点搜索（增量细化 /6.36） |
| rcr_fit_sl(/_w) | :583-586 / :588-592 | 单线拟合（under≤1） |
| rcr_get_single_fn(/_w) | :594-597 / :613-649 | 单线 σ 修正因子（表插值；rcr_fn_ratio :599-611） |
| rcr_fit_dl(/_w) | :651-695 / :697-744 | 双线拟合（under>2；broken stick） |
| rcr_get_68th(/_w) | :747-761 / :764-788 | 官方 get68th（升序 diff 的 68 分位） |
| rcr_get_single_dl_cf(/_w) | :790-793 / :807-838 | single-sigma 修正因子（表插值；rcr_cf_ratio :795-805） |
| rcr_erfc_custom | :841-851 | erfc(z/√2)（Chauvenet 尾概率） |
| rcr_iterative_pass | :857-977 | 单段 iterative Chauvenet（mu/sigma 技术·加权分支） |
| method_minimum_n | :982-999 | 方法最小 N 注册表（NONE 0/σ 族+ESD+RCR+MEDIAN_SIGMA 3/LINEAR_FIT 4/PERCENTILE 2/MINMAX 3/EXTREME_PRIOR 2） |
| method_is_explicit | :1002-1006 | 合法显式方法判定（0..9 与 11；AUTO=10 与越界一律 false） |
| set_err | :1008-1012 | err 缓冲日志文本（仅日志，不承载语义） |
| ScratchVec | :1016-1058 | n≤64 固定 scratch（无每像素堆分配）；>64 堆 fallback |
| scratch_median / scratch_mad | :1060-1068 / :1070-1073 | nth_element 中位（偶数均值）；MAD=median(|x−med|)，σ=1.482602218505602·MAD 由调用方乘 |
| p2_rejection_semantic_id | :1082-1098 | 方法 → canonical semantic id（11 方法全覆盖） |
| kPixelSmallNPolicy / p2_rejection_percentile_band_min_n | :1138-1146 | 小 N 决策点（kConservativeNone ⇒ 档下界 4）与查询入口 |
| astrocs_n_map_method | :1148-1158 | 生产档逐几何 N 路由（1≤N≤3 none / N<6 percentile / N≤15 winsorized / else linear_fit） |
| auto_method_forbidden | :1167-1179 | AUTO 禁止产出 min/max 与 NoRejection（fail-closed 守卫） |
| p2_reject_plan_resolve | :1182-1288 | planning 层 AUTO 解析 + typed 默认值（冻结表，§5 F1） |
| p2_reject_plan_resolve_n | :1290-1301 | 逐 stack nominal_n 覆盖入口 |
| eligibility_core | :1311-1346 | 连续版 policy core（finite→valid→support→quality 严格大于门；support 严格大于） |
| p2_eligibility_filter | :1350-1370 | 资格层连续版入口（compat 路径消费） |
| p2_collect_candidate_stack | :1372-1455 | 生产 strided gather（f32/f64；source_indices 显式保留 eligible→原 slot） |
| reject_none_impl | :1465-1467 | 全 ACCEPTED |
| reject_robust_mad_impl | :1470-1512 | sigma=median+1.482602218505602·MAD 迭代 clip |
| reject_winsorized_impl | :1515-1574 | winsorized_sigma（Siril 1.4.3 语义 :1514 注释） |
| reject_averaged_impl | :1577-1612 | averaged_sigma（mean+mean|resid|·√(π/2)） |
| reject_linear_fit_impl | :1615-1709 | linear_fit（Siril 1.4.3 frozen harness；残差尺度=平均绝对残差 :1665-1668） |
| reject_esd_impl | :1712-1782 | generalized ESD（NIST；单 sqrt :1737；frame_id tie-break :1747-1752） |
| reject_rcr_impl | :1785-1806 | RCR 3-pass 链（Median+DoubleLine → Median+68th → Mean+StdDev） |
| reject_percentile_impl | :1809-1830 | percentile（\|median\| 尺度；iterations=1；判据带见 §5 F10） |
| reject_median_sigma_impl | :1833-1875 | median+SD 迭代 clip |
| reject_minmax_impl | :1878-1910 | 固定 rank 一次性删除（不迭代；比较器仅按 value） |
| reject_extreme_prior_impl | :1915-1965 | 已知先验 σ 极值检验（Grubbs 变体，原始值域；extreme_prior_valid :1971-1992） |
| p2_reject_stack_ex | :1996-2201 | 唯一生产 kernel 入口（§4 状态机 + §5 F3/F14） |
| p2_reject_stack_resolve_ex | :2203-2217 | 规划+执行合并入口（AUTO 在此消解） |
| p2_reject_stack（compat） | :2222-2333 | 旧接口 adapter（资格层+MIN_SAMPLES :2250-2259+typed 换算+non-finite 覆盖） |
| large_scale_grow_side | :2342-2406 | 8 邻域连通分量（DFS）→ 分量≥min_size 合格 → Chebyshev 半径扩张 → 只增不减 |
| p2_large_scale_apply | :2410-2433 | 逐帧 low/high 独立半径调用；参数非法 rc=1 |
| 继承阈值核对 / class id / applicability | :2449-2484 / :2534-2545 / :2552-2600 | 阈值继承核对、reason 分类、方法×N 适用域 WARN（W_PCT_GT8 / W_PCT_N4_FALLBACK 等） |

| 头文件段 | 锚（rejection.h） | 语义 |
|---|---|---|
| 三层输入模型注释 | :6-29 | EligibilityPolicy / RejectionPlan / RejectionNormalizationPolicy；Oracle 清单（Astropy mad_std/NIST/Siril 1.4.3 GPL ORACLE ONLY/RCR 2.4.7 ORACLE ONLY :25-28/PIXINSIGHT_EXACT=NOT_CLAIMED :28-29） |
| P2RejectionMethod | :45-62 | 11 方法枚举 + AUTO=10（:56，kernel 永不接收 AUTO）+ EXTREME_VALUE_PRIOR_SIGMA=11（:61，显式 opt-in） |
| P2_SEMANTIC_* | :65-77 | canonical semantic id 常量（astrocs.*.v1，运行时映射 p2_rejection_semantic_id :1082-1098） |
| P2RejectReason | :94-99 | ACCEPTED=0/REJECTED_LOW=1/REJECTED_HIGH=2/UNDERDETERMINED=3 |
| P2RejectStatus | :102-111 | OK=0..INTERNAL_ERROR=7（八态） |
| P2RejectionNormalization | :114-118 | NONE=0/MEDIAN_CENTER=1/MEDIAN_SCALE=2（floor 默认 1e-12 在 plan 字段 :208） |
| typed params | :121-151 | P2SigmaParams :121-125/P2LinearFitParams :127-131/P2EsdParams :133-136/P2PercentileParams :138-141（low_fraction 注释 "默认 0.1" 漂移 :139）/P2MinmaxParams :143-147/P2RcrParams :149-151（禁止跨方法共享 low/high/max_iter，:120 注释） |
| P2LargeScaleParams | :165-170 | enabled/min_structure_pixels=8/low·high_grow_radius_pixels=2（默认关闭 :166；语义 :153-164） |
| P2ExtremeValuePriorSigmaParams | :194-200 | 先验 σ 极值检验参数（alpha/prior_sigma/prior_sky/center_mode） |
| P2RejectionPlan | :203-224 | 显式计划（method/minimum_n/underdetermined_n/normalization/floor/typed 大成员/nominal_n） |
| P2RejectionPlanRequest | :227-239 | request（允许 AUTO）/nominal_contributors/profile/underdetermined_n（:234-238 冻结注释：wbpp·adaptive=2、astrocs_adaptive_pixel=3、extreme_prior=1） |
| plan_resolve 注释 | :241-259 | WBPP 2.9.1 路由 + profile 语义 + AUTO 禁止 min/max（fail-closed） |
| p2_rejection_percentile_band_min_n 注释 | :267-274 | 小 N 决策点下界查询（1=档含 N≤3；4=N≤3 走保守 none） |
| p2_rejection_applicability | :291-297 | 方法×nominal_n 适用域 WARN 码（advisory，不阻断执行） |
| P2EligibilityInput/Output | :300-321 | 连续版资格层（support_threshold 严格大于 :307；quality_flags_required=0 不要求 :308） |
| P2EligibilityGatherInput/Output | :328-362 | 生产 strided gather（Input :328-346/Output :348-362）；PHASE2_IVAR_WIRING 注释 :350-355（source_indices 权威映射，compact 后禁止猜 original slot） |
| P2CandidateStack | :368-380 | kernel 输入（values/weights/frame_ids/count/data_type 0=fp32,1=fp64 仅诊断 :373；prior_sigma/prior_sky :378-379） |
| P2RejectionDecision | :383-390 | reasons u8 :384/accepted_count/rejected_low/rejected_high/iterations/status :389 |
| p2_reject_stack_ex 声明 | :394-401 | kernel n≤64 固定 scratch；AUTO 非法 |
| p2_large_scale_apply 声明 | :413-416 | frame-major 每帧 width×height 字节原地修改；仅扩张 ≥min_structure_pixels 结构；参数非法 rc=1 |
| compat P2SampleStackView/Result | :418-443 | 旧接口（生产 Stage2 不再调用；sigma_low/sigma_high/max_iterations/min_samples 兼容换算） |
| reason 分类 / 校准面 | :461-587 | P2RejectClass / p2_reject_classify / p2_reject_calibration（FZ-AP2S-REJ-* 面） |

## 4 状态机与返回码（八态显式、互斥；reason 4 值正交）

### 4.1 stack status（rejection.h:79-90；判据=本文档 §5）

| status | 值 | 触发条件（精确） | 锚（rejection.cpp） | 冻结测试证据 |
|---|---|---|---|---|
| OK | 0 | 判定完成且全栈无 UNDERDETERMINED reason 且 accepted_count>0 | :2196-2198 | G6PermutationInvariance；V15ExPermutationInvarianceTyped |
| MIN_SAMPLES | 1 | count==0（:2018）∨ 资格数 < 调用方显式 min_samples（compat :2250-2259） | :2018/:2250-2259 | R2MinSamples |
| ALL_REJECTED | 2 | accepted_count==0 且 n>4（全拒非小栈） | :2194 | V15SatelliteTrail20Frames 负样本集；p2_reject_stack_ex 直测 |
| INVALID_INPUT | 3 | 任一候选 values 或 weights 非 finite | :2025-2034 | V15NoneDoesNotReacceptNaN |
| UNDERDETERMINED | 4 | n ≤ underdetermined_n ∨ n < method minimum_n（:2067，**underdetermined_n 默认随 profile**：生产档 3 / 对照档 2 / 生产档×extreme_prior 1，:1218-1225）；或部分 UNDERDETERMINED reason（:2197）；或 n≤4 全拒容错 fallback（:2185-2193） | :2064-2073/:2185-2199 | V15SatelliteN2Underdetermined |
| INVALID_CONFIGURATION | 5 | PERCENTILE×norm≠MEDIAN_CENTER（:2038-2045）∨ RCR×norm≠NONE（:2046-2052）∨ EXTREME_PRIOR×norm≠NONE（:2055-2062） | :2038-2062 | V16InvalidConfigurationCombos |
| INVALID_METHOD | 6 | plan.method 出界（含 AUTO=10 进 kernel） | :2000-2013（status :2011） | V17InvalidMethodStatus |
| INTERNAL_ERROR | 7 | kernel 内部不变量破坏（现状不可达；保留态） | rejection.h 枚举定义 P2_STATUS_INTERNAL_ERROR = 7（kernel 不可达，无 rejection.cpp 锚） | —（设计保留） |

### 4.2 per-sample reason（rejection.h:71-77；与 status 分离，SCI §7 状态分离不变量）

| reason | 值 | 语义 | 锚 |
|---|---|---|---|
| ACCEPTED | 0 | 未被拒（含 UNDERDETERMINED 白名单外的保留样本） | :1465-1467 等 |
| REJECTED_LOW | 0→1 | 低于 lower threshold（**禁止用原始值正负号判向**，h:20-21 冻结注释） | 各核尾段 |
| REJECTED_HIGH | 0→2 | 高于 upper threshold | 同上 |
| UNDERDETERMINED | 0→3 | 样本数不足，未做拒绝判定（全接受语义） | :2068-2072/:2028-2031 |

计数器口径（:2163-2177）: accepted_count 含 UNDERDETERMINED 样本
（全接受语义）；rejected_low/high 仅统计显式拒绝；iterations=
kernel 外层迭代（ESD=k_out；RCR=3 常量；percentile/minmax=1）。
**正向约束**：`accepted_count` 是「未判拒」的计数，**不是**「通过排异检验」的计数——
消费方不得用它推断排异确实发生过（`UNDERDETERMINED` 与 `ACCEPTED` 在计数上不可区分）。

### 4.3 rc（函数返回）与 status 正交

- `p2_reject_stack_ex`: rc=1 仅 stack/plan/out null（:1999）或
  reasons/values null 且 count>0（:2019）；rc=0 时语义全由 status
  承载（含 INVALID_METHOD/INVALID_INPUT/INVALID_CONFIGURATION——
  "科学状态"而非调用错误）。
- `p2_reject_plan_resolve`: rc=0 OK；rc=1 null 请求/plan 或 request
  出界或 profile 非法（:1185-1206；err 仅日志文本）。
- `p2_eligibility_filter` / `p2_collect_candidate_stack`: rc=1 null
  in/out 或必要输出缓冲缺失（:1350-1370/:1372-1385）；n==0 → rc=0
  空输出。
- `p2_large_scale_apply`: rc=0 OK（disabled=noop :2421）；rc=1
  指针 null ∨ width/height/depth ≤0 ∨ min_structure_pixels<1 ∨
  负半径（:2413-2420）。
- 调用方合同: 对 ex 返回 status ∈ {OK, UNDERDETERMINED} 才可继续
  积分，其余 hard fail（stage2.cpp 冻结门）。

## 5 逐公式定义（算法级，与 SCI §5 同构；单位见 §2）

### F1 planning 解析（p2_reject_plan_resolve :1182-1288）

```text
typed 默认值（冻结阈值表;SCI §5 阈值冻结锚点逐项一致）:
  underdetermined_n = req>0 ? req : <profile 默认>            :1218-1225
    profile 默认: astrocs_adaptive_pixel ∧ AUTO → 3
                  astrocs_adaptive_pixel ∧ EXTREME_PRIOR → 1
                  其余 profile → 2
  normalization = MEDIAN_CENTER;  floor = 1e-12              :1226-1228
  sigma/winsorized/averaged = (4.0, 3.0, 8)                  :1229-1233
  linear_fit = (5.0, 3.5, 8)                                 :1234-1235
  esd = (alpha 0.05, max_outliers 10)                        :1236
  percentile = (low 0.2, high 0.1)                           :1237
  median_sigma = (4.0, 3.0, 8)                               :1238-1239
  minmax = (low 1, high 1, min_kept 4)                       :1240-1241
  rcr.technique = 0（SS_MEDIAN_DL 唯一支持）                  :1242
  large_scale = (enabled 0, min_structure 8, low 2, high 2)  :1243-1246
  extreme_prior = (alpha 0.05, prior_sigma 0=未提供, prior_sky NaN, center_mode 1) :1247-1251
AUTO 路由（N = 该输出像素的几何覆盖帧数，一次解析；两档共用阈值表）:  :1254-1269
  生产档 astrocs_adaptive_pixel（:1148-1158 唯一决策点）:
    1 ≤ N ≤ 3  → NONE（不排异，直接逆方差加权积分）
    4 ≤ N ≤ 5  → PERCENTILE
    6 ≤ N ≤ 15 → WINSORIZED_SIGMA
    N ≥ 16     → LINEAR_FIT
  对照档 wbpp_2_9_1 / wbpp_current / astrocs_adaptive（:1262-1268）:
    N < 6 → PERCENTILE;  6 ≤ N ≤ 15 → WINSORIZED_SIGMA;  N > 15 → LINEAR_FIT
  min/max 不用于生产（AUTO 禁止产出 min/max 与 NoRejection，生产路径守卫 fail-closed
  :1167-1179/:1272-1278）
  minimum_n = method_minimum_n(method)                        :1280
profile 合法集 = {astrocs_adaptive_pixel(生产默认, Astro Celestial Sphere Database（ACSD） 自研),
                  wbpp_2_9_1(对照档), wbpp_current(别名),
                  astrocs_adaptive(可调档)}                        :1196-1206
  （nullptr → wbpp_2_9_1；其余 rc=1；wbpp_current 解析为 wbpp_2_9_1，
  仅保留 group active count 一次解析语义；astrocs_adaptive=tile nominal
  depth；astrocs_adaptive_pixel=逐输出像素几何 N 映射（上表）；与 WBPP
  档界的差异及其依据见 docs/science/REJECTION.md——canonical/adaptive
  区别仅在 nominal 来源，h:227-239/:241-259 冻结注释。
  生产布局与现行语义详见 DATA_SEMANTICS §22 生产表）
```

### F2 eligibility gather（p2_collect_candidate_stack :1372-1455；core :1311-1346）

```text
逐 slot s=0..count-1（frame-major strided: v=values[s*stride+pixel]）:  :1405-1434
  合格 ⇔ isfinite(v)                                       :1410
        ∧ (valid==null ∨ valid[s*valid_stride+pixel])      :1411-1413
        ∧ (weights==null ∨ isfinite(weights[...]))         :1414-1421
        ∧ (support==null ∨ support[s*support_stride+pixel]
             > support_threshold)      # 严格大于           :1422-1428
        ∧ (quality==null ∨ required==0 ∨
             (q & required)==required)                     :1429-1433
合格样本紧凑写入 values/weights/support/frame_ids            :1435-1445
source_indices[cnt] = s（eligible→original slot 权威映射;
  ivar/quality/variance 一律经此映射，禁止 compact index 猜测,
  h:350-355 PHASE2_IVAR_WIRING）                            :1438-1439
诊断计数 invalid_finite（含非有限 weights）/valid/support/quality :1403-1404/:1410-1433
```

### F3 normalization 工作域（ex :2091-2105）

```text
work[i] = values[i]（原始域副本）                            :2091-2092
norm != NONE: orig_median = median(values)                  :2093-2097
  MEDIAN_CENTER: work -= orig_median      # 判定工作域       :2098-2099
  MEDIAN_SCALE:  work /= max(|orig_median|, floor)           :2100-2104
decision 作用于 work；accepted mask 应用回原始值
  （h:15-17 冻结注释）；PERCENTILE 的 scale=|orig_median| 为原始域
  中位（:1809-1830 :1818），MEDIAN_CENTER 下负值安全。
```

### F4 sigma = robust_mad_clip（:1470-1512）

```text
迭代 it=0..max_iterations-1:                                :1483
  lo=−|σ_lo|; hi=|σ_hi|                                     :1479-1480
  cur = {w[i]: accept[i]}（accept 初始全 1）                 :1484-1487
  nc<2 → break                                              :1488
  m = median(cur);  s = 1.482602218505602 · median(|cur−m|)            :1491-1492
  s ≤ 1e-12 → break                                         :1493
  z_i = (w[i]−m)/s;  z<lo ∨ z>hi → 拒                       :1497-1502
  无新拒 → break;  计数器 it 外层                             :1503-1506
判定方向: w[i] < m → LOW else HIGH（:1507-1510）
**阈值方向（正向约束）**：默认 σ_lo=4.0 > σ_hi=3.0 ⇒ **高侧更敏感**，正离群先被剔。
**适用域**：s ≤ 1e-12 是工作域的绝对退化门；该门只在工作域标度 ≈1（MEDIAN_SCALE 归一后）
时有意义，在原始标度域（ADU·sr⁻¹，天光量级 1e13）等价于恒假。
Astropy sigma_clip(median+mad_std) oracle（h:23）。
```

### F5 winsorized_sigma（:1515-1574；Siril 1.4.3 语义锚 :1514）

```text
外层 iters=0..max_iterations-1（r=累计拒绝数，跨外层不清零）:
  cur = accept 集;  nc<2 → break                            :1528-1533
  med = median(cur)                                         :1536
  s0 = sqrt(Σ(cur−med)²/(nc−1))          # accept 集 σ      :1538-1540
  s0 ≤ 1e-12 → break                                        :1541
  内层 winsor 收敛（≤64 轮）:                                :1543-1558
    clamp cur 到 [med−1.5σ, med+1.5σ]                       :1544-1546
    wsd = sqrt(Σ(wcur−med)²/(nc−1));  σ_new = 1.134·wsd     :1549-1552
    |σ_new−σ| ≤ 5e-4·σ → 收敛 break                         :1553-1556
  clip: z=(w[i]−med)/σ;  z<lo ∨ z>hi → 拒, ++r              :1559-1567
    guard (int)nc − r ≤ 4 → break（保底 4 样本，本外层剩余冻结接受）
  无新拒 → break
判定方向: w[i] < med → LOW else HIGH（:1569-1572）
**适用域**：内层 1.5σ winsor 窗与 1.134 修正因子按对称近高斯 accept 集标定；
分布强偏斜（天光梯度/结构主导）时 σ_new 的偏差未标定。
```

### F6 averaged_sigma（:1577-1612）

```text
mean = Σ_{accept} w[i] / nc                                 :1587-1592
s = (Σ_{accept} |w[i]−mean| / nc) · √(π/2)                  :1594-1596
s ≤ 1e-12 → break;  z=(w[i]−mean)/s 超阈拒                  :1597-1605
判定方向: w[i] < mean → LOW else HIGH（:1607-1610）
**适用域（正向约束）**：√(π/2) 是「平均绝对偏差 → σ」的**零均值高斯**换算因子；
残差分布非对称（天光梯度、结构残差）时该换算给出偏小尺度 ⇒ 实际阈值更严（过拒）。
```

### F7 linear_fit（:1615-1709；Siril 1.4.3 frozen harness 锚 :1614）

```text
预计算（一次）:  m_x = (n0−1)/2                              :1632
  xf[j] = 1/(j+1);  m_dx2 = 1/Σ dx²·xf  (Welford 风格)      :1633-1639
迭代 it<max_iterations:
  N = |stack|;  N<4 → break                                 :1645-1646
  按 value 升序排序 (value, orig_index) 字典序（tie-break=原索引,
  置换不变）                                                :1647-1653
  m_y = Welford 加权均值（权 xf[i]）                          :1654-1656
  m_dxdy = Welford（dx=j−m_x, dy=stack[i]−m_y）;             :1657-1662
  slope = m_dxdy·m_dx2;  intercept = m_y − m_x·slope        :1663-1664
  σ = mean|stack[i] − fit(i)|            # 平均绝对残差       :1665-1668
  判定: fit(j)−stack[j] > σ·σ_lo → LOW;                     :1672-1686
        stack[j]−fit(j) > σ·σ_hi → HIGH
    guard (int)(N−r) ≤ 4 → keep[j]=1 continue               :1673
  收缩到 kept 集，next iteration                             :1688-1701
尾段: 未显式拒 → ACCEPTED（:1704-1708）
**尺度口径（正向约束）**：残差尺度 σ = **平均绝对残差**，**不乘** √(π/2) 正态换算因子；
同一 σ 单位下 linear_fit 的等效阈值比 averaged_sigma 严约 √(π/2) ≈ 1.2533 倍
⇒ 阈值 5.0/3.5 与 σ 族的 4.0/3.0 不可直接比较。
逐位 oracle: LinearFitFindsOutlier（rng42 固定向量与未修改 Siril
1.4.3 官方 harness 逐位核对=拒 {30, 43..49} 共 8）。
```

### F8 generalized ESD（:1712-1782；NIST oracle）

```text
max_out = max(1, max_outliers);  alpha ≤0 → 0.05            :1719-1720
for rr=0..max_out-1:
  nc = |accept|;  nc<3 → break                              :1728-1729
  mean = Σaccept work/nc                                    :1730-1734
  s = sqrt(Σaccept (work−mean)²/(nc−1))   # 单 sqrt          :1735-1737
  s ≤ 1e-12 → break                                         :1738
  worst = argmax_{accept} |work−mean|/s                     :1739-1753
    tie-break: |rv−max_r| ≤ 1e-15 时取 frame_id 较小者       :1747-1748
  n_minus_i = n−(rr+1);  nu = n_minus_i−1                   :1754-1755
  p = 1 − alpha/(2(n_minus_i+1))                            :1756
  tcrit = t_quantile(p, nu)               # 二分 80 轮 [0,40] :1757/:87-97
  Lambda = (tcrit·n_minus_i)/sqrt((nu+tcrit²)(n_minus_i+1))  :1758-1759
  R[rr]=max_r;  accept[worst]=0（先全摘，回看定 k_out）        :1760-1764
k_out = max{ rr+1 : R[rr] > Lambda[rr] }   # Rosner 回看准则  :1766-1768
仅前 k_out 个 removed 置拒；方向=原始值 vs 原始中位:           :1769-1781
  w[i] < med → LOW else HIGH
**适用域（正向约束）**：Rosner 检验的临界值按**近似正态**样本标定；本层
`method_minimum_n` 允许 ESD 自 n=3 起用（:982-999），该域远低于检验的
渐近适用域，**n < ~25 的 ESD 判定不具备标定意义**；本层不声明小 n 的 ESD 能力。
```

### F9 RCR（:1785-1806 + RCR 段 :117-977；官方 rcr 2.4.7 语义）

```text
固定 3-pass 链（:1793-1795）:
  pass1: mu=MEDIAN,  sigma=DOUBLE_LINE
  pass2: mu=MEDIAN,  sigma=68TH
  pass3: mu=MEAN,    sigma=STD_DEV
每 pass（rcr_iterative_pass :857-977）:
  cur = accept 集（加权: sort(w,y) 同步排序后官方 getMedian_w :371-387）
  diff_i = |cur_i − mu|;  worst=argmax diff（首个最大）
  sigma 技术: DOUBLE_LINE → broken-stick 双线（rcr_fit_dl/_w :651-695/:697-744）
              68TH → 官方 get68th（:747-761）；STD_DEV → 加权样本方差
  σ = st_dev · single_dl_cf(n)（官方修正因子表插值 :790-793/:807-838）
  Chauvenet 终止: distinct_values <3 值（rcr_distinct_values :334-354，rel 1e-8）
    ∨ n·erfc(zmax/√2) ≥ 0.5 → break（rcr_erfc_custom :841-851）
  否则 accept[worst]=0，循环
加权分支: weights 非 null 时启用（官方 w·resid² 加权拟合/68th/CF）。
仅 normalization=NONE 合法（ex :2046-2052；raw 域）。
**适用域（正向约束）**：Chauvenet 尾概率近似要求 n·erfc(z/√2) 的尾部估计有效，
小 n 下该近似失效；`method_minimum_n` 允许 RCR 自 n=3 起用（:982-999），
**n < ~10 的 RCR 判定不具备标定意义**。
```

### F10 percentile（:1809-1830；判据带 = 天光电平的固定分数）

```text
plow=|low_fraction|;  phigh=|high_fraction|                  :1817-1818
scale = |orig_median|（原始域中位，MEDIAN_CENTER 工作域下负值安全）:1818
w[i] < −scale·plow  → LOW                                   :1820-1822
w[i] >  scale·phigh → HIGH                                  :1823-1825
else ACCEPTED;  iterations=1（单轮，不迭代）                  :1829
**单位**：scale 与 values 同标度（面亮度 ADU·sr⁻¹）。
**适用域（正向约束）**：判据在 σ 单位下的等效阈值为
  z_low = plow·|median|/s、z_high = phigh·|median|/s（s = 该栈稳健尺度）；
  阈值随 |median|/s 线性漂移：真实天光电平（|median|/s ≈ 50）下退化为只剔 10%/20% 天光以上的样本
  （惰性）；`|median|/s ≲ 10` 时等效阈值落进噪声宽度 ⇒ **对干净数据过拒**（比值 3.33 时
  96.65%（n=6）的干净栈至少被剔一个样本）；`≲ 2` 时退化为全拒。完整三段域、实测值与
  「任何电平下都不是噪声尺度判据」的结论见 docs/science/REJECTION.md §8a。
```

### F11 median_sigma（:1833-1875）

```text
迭代 iters=0..max_iterations-1:                             :1845
  cur = accept 集;  nc<2 → break                            :1847-1850
  med = median(cur);  sd = sqrt(Σ(cur−med)²/(nc−1))         :1852-1858
  sd ≤ 1e-12 → break                                        :1859
  guard (int)nc−r ≤ 4 → break（同 F5 保底）                  :1863
  z=(w[i]−med)/sd;  z<lo ∨ z>hi → 拒                        :1864-1865
判定方向: w[i] < med → LOW else HIGH（:1870-1873）
```

### F12 minmax（:1878-1910；固定 rank 一次性删除）

```text
k_low=|reject_low_count|;  k_high=|reject_high_count|;
min_kept=max(1,min_kept)                                    :1884-1886
(k_low+k_high) ≥ n ∨ n−(k_low+k_high) < min_kept
  → 全栈 UNDERDETERMINED, iterations=0（:1887-1894）
按 value 升序 sort（比较器仅 value——tie-break 未显式冻结，§11.3
DISP-P2REJ-004）                                            :1895-1898
最低 k_low → LOW;  最高 k_high → HIGH                        :1901-1908
iterations=1
```

### F13 large_scale 结构生长（:2342-2433）

```text
p2_large_scale_apply: 参数非法 → rc=1（:2413-2420）；!enabled →
  noop rc=0（:2421）；逐帧 low/high 独立调用 grow_side（:2425-2431）
grow_side(mask, min_size, radius):                          :2342-2406
  radius ≤0 → mask 不变（仅 pixel 级拒绝）
  8 邻域 DFS 连通分量；|comp| ≥ min_size → 分量全部 qualify
  Chebyshev 扩张 radius 轮（qualify 的 8 邻域逐轮并入）
  只增不减: 原始 pixel-level rejected（含小分量）保留
trail 扩张 / compact cosmic 不生长（分量 <8 不 qualify）。
**适用域（正向约束）**：`min_structure_pixels` 与 grow radius 是**像素域常数**
（8 px / 2 px），不随像素角尺度缩放 ⇒ 换仪器/换像素尺度后同一物理结构会跨过或
落空该结构门；本判据只在源像素角尺度与标定域一致时具备「大尺度结构」语义。
```

### F14 tally 与小栈容错（ex :2163-2199）

```text
tally: accepted_count/rejected_low/rejected_high/iterations  :2163-2177
  UNDERDETERMINED reason 计入 accepted_count（全接受语义）
容错（冻结注释 :2178-2184 "仅调容错路径、阈值冻结不变"）:
  accepted_count==0 ∧ n≤4 → reasons 全 UNDERDETERMINED,
    accepted_count=n, status=UNDERDETERMINED（:2185-2192；
    可达根因=percentile 判据带在 |median|/s ≲ 3.3 时全拒，见 §5 F10 与
    docs/science/REJECTION.md §8a）
  accepted_count==0 ∧ n>4 → status=ALL_REJECTED（:2194）
  否则 status = any_underdetermined ? UNDERDETERMINED : OK（:2196-2199）
```

## 6 消费链与并行语义

- **Stage2（编排，astrocs.p2.hips_writer 消费者）**:
  group 级 plan resolve（**工具链现状** wbpp_2_9_1；**生产入口
  p2_op_reject 默认 astrocs_adaptive_pixel（自研）**，nominal=cfg.hips.size()，
  stage2.cpp:643-658）→ tile 级（astrocs_adaptive，nominal=tile
  depth，:677-697）→ typed params 唯一默认源=cfg（:698-729）→
  CPU 像素 lambda: p2_collect_candidate_stack（:1085-1104，
  support_threshold=0.0 :1093）→ p2_reject_stack_ex（:1184）→
  status 门 ∈{OK,UNDERDETERMINED}（:1189-1195）→ reasons
  ACCEPTED|UNDERDETERMINED → 1（:1196-1201）→ large_scale 激活时
  buffer 化不即积分（:1468-1503），二遍 grow 后二次积分
  （:1544-1554）。
- **ACR 加速（acr_kernels.cpp）**: mosaic_reject_legacy
  process_pixel :110-210（gather :119-137 value_dtype=0 fp32；
  wmode=2 已禁用 throw :141-144 "ivar science 模式必须走 CPU
  canonical path"；snr 8×8 cell 权重 :147-158；ex kernel :174-176；
  同 status 门 :177-183）。OMP per-thread scratch（:212-244，
  catch→fail atomic）；CUDA bridge stub（Linux）。
- **并行边界（像素间并行、像素内串行）**: rejection.cpp 无任何线程
  原语——单像素栈决策按候选序固定计算，无跨 worker 浮点重结合。
  像素间并行在调用方: Stage2 OMP workers（:1279-1317，per-thread
  scratch 注释冻结 :1290-1291 + thread id 定序归并 :1310-1317；
  large_scale 激活强制串行 :742-746/:1280）与串行路径
  （:1325-1543）。ivar
  权重经 source_indices 回映射原始 slot（:1098/:1361/:1379）。
- **确定性合同（matrix 专项）**: 同输入同 plan 同 fid → decision
  bitwise 确定且与 worker 数无关（每像素独立决策树；无共享累加
  器；ESD tie-break=较小 frame_id :1747-1748；linear_fit 排序
  (value,orig_index) 字典序 :1647-1653；median 值级置换不变）。
  **例外（正向约束）**：MINMAX 的比较器仅按 value（:1897-1898，
  std::sort 非稳定）⇒ **等值样本的置换不变性在 MINMAX 上不承诺**；
  该域由 §11.3 DISP-P2REJ-004 登记。
  验证锚: G6PermutationInvariance（method 0..6）、
  V15ExPermutationInvarianceTyped（锚 = lib/algorithms/coverage/tests/
  synthetic_gate.cpp，以该文件实测为准）；冻结容差见 §11.4。

## 7 与 SCI 的对应与偏差（如实登记）

- SCI §5 连续定义（7 方法+auto profile+阈值冻结锚点+eligibility
  分层+large_scale 语义）与本文档 §5 逐项同构；阈值表逐值一致
  （§5 F1；rejection.cpp:1-12 冻结头注释同表）。auto 路由=nominal n
  一次解析（SCI §6/§7 阈值不变量；:1254-1269 实测一致）。
- **percentile 默认注释漂移（DISP-P2REJ-001，文档级）**:
  rejection.h:139 P2PercentileParams 注释 low_fraction "默认 0.1 =
  10%" 与实现 plan_resolve 默认 low_fraction=0.2（:1237）、SCI 权威
  （REJECTION.md §5 percentile low 0.2/high 0.1）不一致——**以 SCI 与
  实现为准**（0.2/0.1），header 注释为待对齐项。
- **空栈状态命名（DISP-P2REJ-002，文档级）**: SCI §8 表
  "无候选 → NO_CANDIDATES" 与实现八态枚举（rejection.h:102-111）不一致：
  本层无 NO_CANDIDATES，空栈 → P2_STATUS_MIN_SAMPLES
  （ex :2018 / compat :2228）；NO_CANDIDATES 属积分域
  P2IntegrateStatus（integrate.h，非本模块域）。**语义权威=本文件 §4.1**。
- **行号锚（DISP-P2REJ-003，非语义缺陷）**: 本文档 §3/§5 与
  REJECTION_ALGORITHMS.md §12 的行号锚一律以**本次实测**（rejection.cpp
  2950 行 / rejection.h 595 行）为准；外部文档引用行号与实测不符时，
  以本文档 §3 为准（与 DISP-P2INT-002 同类）。
- **minmax tie-break 未显式冻结（DISP-P2REJ-004，合同级限制）**:
  :1897-1898 比较器仅按 value（std::sort 非稳定）——同输入同编译器
  确定；等值样本 permutation 不变性未承诺（G6 覆盖 method 0..6 不含
  MINMAX）。整改候选=显式 index tie-break（需评估冻结语义）+ 等值门。
- SCI §2 符号表引用行号 → 实测 reason :94-99 / status :102-111
  （并入 DISP-P2REJ-003 锚漂移）。
- SCI §13 公开 API 名 `p2_reject`（compat）与生产入口
  `p2_reject_stack_ex` 并存——API 面=API-P2-REJ-001（PUBLIC_API.md）
  冻结两符号；compat 仅测试/旧调用（h:299 冻结注释"生产 Stage2
  不再调用"）。

## 8 单位与 dtype 登记（唯一权威=DATA_SEMANTICS §22）

- kernel 工作域全浮点 IEEE f64（gather 按 `value_dtype` 分派 f32/f64 源→f64 提升，
  :1386-1401）；无 long double/复数。**单位逐项**（权威 = SCI REJECTION §3
  + DATA_SEMANTICS §22）：`values` = 面亮度 **ADU·sr⁻¹**；
  `weights` = **(ADU·sr⁻¹)⁻²**；`support` 无量纲 [0,1]（仅作资格门，不进统计）；
  `frame_ids` 无量纲 u64；`reasons` u8；计数/iterations u32；`status` int。
- MINMAX 判定直接用原始域值（:2150 分派 stack->values）；PERCENTILE
  scale=原始域 |median|（同 `values` 标度）；σ 族/ESD/RCR 在工作域
  （normalization 后）——单位在 MEDIAN_SCALE 下无量纲化
  （work/max(|median|,floor)，floor 默认 1e-12），DATA §22.4 登记。
- **跨方法单位不可比（正向约束）**：同一候选栈在不同方法下处于不同判定域
  （MINMAX/PERCENTILE = 原始域；σ 族/ESD/RCR = 工作域）⇒ 各方法的
  阈值数值**不得跨方法比较**；RCR 的加权分支只在 normalization=NONE
  （原始域）下与 `weights` 的 (ADU·sr⁻¹)⁻² 标度自洽（:2046-2052 强制）。
- 整数登记量（status/reasons/计数）bitwise 确定；浮点仅域内四则
  与 sqrt/t 二分（80 轮定轮数，无自适应收敛 → 跨平台同输入 bitwise
  同 tcrit 路径；libm 差异域=lgamma/exp/log/sin，oracle 门以 rtol
  承接，§11.4 F7）。

## 9 边界与退化（SCI §8 逐条实现现状）

| 条件 | 行为 | 实现锚 |
|---|---|---|
| count==0 | MIN_SAMPLES（rc=0；**非** NO_CANDIDATES，DISP-P2REJ-002） | :2018 |
| n≤underdetermined_n ∨ n<minimum_n | UNDERDETERMINED 全接受（recall=0 显式）；underdetermined_n 默认随 profile（3/2/1） | :2064-2073 |
| 全拒且 n≤4 | 容错 fallback → UNDERDETERMINED 全接受（阈值冻结不变） | :2185-2192 |
| 全拒且 n>4 | ALL_REJECTED | :2194 |
| values 或 weights 非 finite | INVALID_INPUT（reasons 全 UNDERDETERMINED、accepted=count） | :2025-2034 |
| method=AUTO/出界进 kernel | INVALID_METHOD（reasons=UNDERDETERMINED、accepted=count） | :2000-2013 |
| PERCENTILE×norm≠MEDIAN_CENTER / RCR×norm≠NONE / EXTREME_PRIOR×norm≠NONE | INVALID_CONFIGURATION | :2036-2062 |
| accept 集 nc<2（σ 族）/nc<3（ESD） | break（不再拒） | :1488/:1729 |
| s≤1e-12（工作域尺度退化） | break（不除零） | :1493/:1541/:1597/:1738/:1859 |
| linear_fit N<4 | break | :1646 |
| minmax 删后 <min_kept | 全栈 UNDERDETERMINED, iterations=0 | :1887-1894 |
| winsor/median_sigma 保底 (nc−r)≤4 | 剩余样本冻结接受 | :1562/:1863 |
| large_scale 参数非法/disabled | rc=1 / noop rc=0 | :2413-2421 |
| radius=0 | mask 不变（仅 pixel 级拒绝） | :2342-2406 |
| 质量指针 | 现状 control 级数据模型，stage2 传 nullptr 并记录（h:337-338） | gather :1429-1433 |
| AUTO 在 planning 解析 | kernel 永不接收 AUTO（h:56 注释） | :2000-2013 |

## 10 已冻结禁改清单（本层不可接受变化）

1. 十一方法枚举 name/value/顺序 + AUTO=10（rejection.h:45-62）。
2. reason 4 值/status 8 值（rejection.h:94-111）与"reason/status
   分离"（SCI §7 状态分离不变量）。
3. 阈值表（4.0/3.0/8；5.0/3.5/8；0.05/10；0.2/0.1；1/1/4；
   minimum_n 注册表）——rejection.cpp:1-12/:1226-1251/:982-999；
   禁止"效果好"重定义（SCI §9a）。
4. AUTO 路由（生产档 1≤N≤3 none / 4≤N≤5 percentile / 6≤N≤15 winsorized /
   N≥16 linear_fit；对照档 N<6 percentile / 6..15 winsorized / N>15 linear_fit）
   与 nominal n 一次解析（禁止 per-pixel n_eff 重选，SCI §7/§10）。
5. normalization 默认 MEDIAN_CENTER + floor 1e-12；PERCENTILE/RCR/
   EXTREME_PRIOR 的 normalization 绑定（:2036-2062）。
6. RCR technique=0（SS_MEDIAN_DL）唯一支持 + 3-pass 链序
   （Median+DoubleLine → Median+68th → Mean+StdDev）。
7. ESD 单 sqrt（:1737）+ 较小 frame_id tie-break（:1747-1748）+ Rosner 回看准则。
8. UNDERDETERMINED=全接受（recall=0 显式，不做伪剔除）；reason
   判向=阈值侧（禁原始值正负号）。
9. n≤4 全拒容错 fallback（:2178-2184 注释/:2185-2192 逻辑；仅容错
   路径，阈值冻结不变）。
10. eligibility 单路径（strided gather 与连续版同一 policy core；
    source_indices 权威映射，PHASE2_IVAR_WIRING）。
11. large_scale 只增不减 + compact（<8 px）不生长 + 默认关闭。
12. policy/reducer 分离镜像: 本层不引入权重策略（RCR 官方加权
    语义除外）；weights 数组外置。

## 11 冻结附录（SRC-P2-REJ-001 源码实测）

### 11.1 逐符号锚

见 §3 表（锚=`grep -n`/read 实测；禁止手抄他版行号）。

### 11.2 返回码/并发合同

- rc 语义: §4.3（ex rc=1 仅 null 三态；科学语义全在 status；
  plan_resolve/eligibility/gather/large_scale rc=1=参数非法）。
- 并发合同: reentrant=yes（无全局/静态可变状态；kRcrSS* 为只读
  const 表）；threadsafe=no（无内部锁，并发由调用方像素划分）；
  internal_parallel=none（kernel 单栈纯函数；OMP 在调用方 Stage2
  像素 lambda / ACR，per-thread scratch，thread id 定序归并）；
  取消点=无（ThreadLease 接线归编排面，与 DISP-COV-005/DISP-P2INT 同构）。
- 内存合同: kernel n≤64 固定 scratch（ScratchVec :1016-1058，无每像素堆分配），
  >64 堆 fallback；RCR 用 std::vector（段内局部）；无整帧副本
  （决策 per-pixel）；生产 buffer 化仅在调用方（stage2）。

### 11.3 现状缺陷/限制清单（DISP-P2REJ-001..004，登记不改码）

- **DISP-P2REJ-001**（文档级）: rejection.h:139 percentile
  low_fraction 注释 "默认 0.1" 与实现/SCI 权威 0.2 不一致（§7）；
  以 SCI 与实现为准，header 注释为待对齐项。
- **DISP-P2REJ-002**（文档级）: SCI §8 "无候选→NO_CANDIDATES" 与
  实现 MIN_SAMPLES 不一致（八态无 NO_CANDIDATES；NO_CANDIDATES 属积分域
  P2IntegrateStatus）（§7）；语义权威=本文件 §4.1。
- **DISP-P2REJ-003**（锚漂移，非语义）: 外部文档行号引用与实测
  （rejection.cpp 2950 行 / rejection.h 595 行）不符时（§7）；
  行号权威=本文件 §3。
- **DISP-P2REJ-004**（合同级限制）: minmax 比较器仅 value，等值
  tie-break 未显式冻结（§7）；整改候选=显式 index tie-break + 等值门。
- 附注（冻结事实，非缺陷）: n≤4 全拒 fallback（:2178-2192 冻结
  注释）、winsor/median_sigma 保底 (nc−r)≤4（:1562/:1863）、ESD tie
  epsilon 1e-15 + 较小 frame_id（:1747-1748）、winsor 内层 64 轮/
  收敛 5e-4·σ（:1543/:1553）——均为冻结名义行为，禁止漂移。

### 11.4 TEST-P2-REJ-DESIGN-001 冻结测试设计（可执行 TEST-P2-REJ-001 由 P2-REJ-TEST 落地）

锚归属声明: 本节及 §6 的测试锚 = **测试名**（`TEST(Suite, Name)` 的
`Name`），落盘文件 = `lib/algorithms/coverage/tests/synthetic_gate.cpp`；
**行号不作为权威**（测试文件随测试面演进，行号以文件内 `TEST` 宏实测为准）。

- **F1 ESD NIST 门**（SCI §11）: 54 值 NIST Rosner 集恰拒
  {5.34, 5.42, 6.01}（V15EsdSingleSqrtExactRosnerSet 拒集逐位=索引
  {51,52,53}；G6EsdNistRosner54 n_reject==3；"双 sqrt bug 会误拒"
  负向对照）；masking 对检出（G6EsdMaskingCase ≥2 拒）。容差=拒集精确（bitwise）。
- **F2 AUTO 路由/profile 门**（SCI §11 阈值不变量）:
  V15AutoPlanResolvesByNominal（n=2/5→PERCENTILE、6/15→
  WINSORIZED、16/20→LINEAR_FIT；非法 profile rc≠0）；
  V16ProfileGroupVsAdaptive（wbpp_current（wbpp_2_9_1 的别名）
  group 一次 vs astrocs_adaptive tile depth）。容差=方法枚举精确。
  **门面缺口（正向约束）**：现有门只覆盖**对照档**的 \`N<6 → percentile\`
  下界；生产档 \`astrocs_adaptive_pixel\` 的 \`1≤N≤3 → none\` 与 \`4≤N≤5 →
  percentile\` 边界由 \`p2_rejection_percentile_band_min_n\` 查询值锁定，
  该值必须与路由表同源（改一处必须同时改另一处）。
- **F3 small-N/状态穷尽门**（SCI §7/§8）:
  V15SatelliteN2Underdetermined（n=2 全 UNDERDETERMINED、
  accepted_count=2）；R2MinSamples（status==1）；V17InvalidMethodStatus；
  V16InvalidConfigurationCombos；V16RejectionNormalizationValidation；
  V15NoneDoesNotReacceptNaN；V15ValidFalseStaysRejected；
  M4A01N3PercentileMedianAlwaysInBand / M4A01N4AllRejectedFallsBackUnderdetermined /
  M4A01N5PercentileMedianAlwaysInBand / M4A01N6AllRejectedIsHardFailure
  （**容错域边界：n=3/5 时中位样本必在带内、n=4 全拒降级、n=6 全拒 hard fail**）。
  八态互斥显式断言；容差=枚举/计数精确。
- **F4 注入门**（SCI §11 卫星线注入）:
  V15SatelliteTrail20Frames（AUTO 20 帧→LINEAR_FIT、
  reasons[7]=REJECTED_HIGH、accepted≥15、clean false-reject ≤4）；
  large_scale 结构门: V17LargeScaleGrowsTrailNotCosmic
  （40px trail 扩 ±2 非 ±3；2×2 cosmic 不生长；低/高独立）、
  V17LargeScaleSparseNotGrown、V17LargeScaleDisabledNoop、
  V17LargeScaleInvalidParams（min_structure_pixels=0 →rc=1）。
  容差=mask/计数精确。
- **F5 置换不变性门**（SCI §11 确定性门）:
  G6PermutationInvariance（method 0..6，fid=1000+i 稳定帧
  identity，σ_low=−4/σ_high=3/min_samples=3，shuffle 决策一致）；
  V15ExPermutationInvarianceTyped（ex typed 面）。容差=
  decision bitwise（同 fid 语义）。**覆盖边界**：method 0..6 不含
  MINMAX（§11.3 DISP-P2REJ-004）。
- **F6 typed params/harness 逐位门**: R1SigmaClippingFindsOutliers；
  LinearFitFindsOutlier（rng42 固定向量与未修改 Siril
  1.4.3 harness 逐位核对=拒 {30,43..49} 共 8）；RcrFindsOutlier；
  G4SequentialRcrMask；G6WinsorizedDiffersFromSigma（MAD=0 vs std
  尺度分离）；V15LowHighThresholdSemantics；V15TypedPercentileParams；
  V15TypedMinmaxParams；V16MinMaxFixedCountExact；
  V15RejectionTypedParseAndDefaultAuto（typed 参数透传+默认 AUTO）。
  容差=逐位/计数精确。
- **F7 Python 参考 Oracle**（SCI §11）: Astropy
  sigma_clip(median+mad_std) 对 robust_mad_clip 复算 reject set；
  SciPy stats 对 ESD/RCR 决策复算（新增设计面，落地归 P2-REJ-TEST）。
  容差=rtol 1e-12（Python 参考域）；decision 集合精确一致。
- **F8 gather/eligibility 门**: V16GatherStridedFp32Fp64
  （f32/f64 strided、source_indices 恒等映射）；V15FilterAllPolicies
  （finite/valid/support/quality 四诊断计数）。容差=逐元素精确。
  **覆盖边界**：非有限 **weights** 在资格层判不合格（rejection.cpp:1414-1421），
  该分支的负例须与 values 非有限分支分开断言。
- 冻结容差汇总: F1-F6/F8 = bitwise/枚举/计数精确（无 epsilon 门）；
  F7 = rtol 1e-12（Python 参考域）；large_scale=mask 精确。本层
  禁引入其他 epsilon（ESD tie 1e-15、RCR isEqual rel 1e-8、winsor
  收敛 5e-4·σ 为实现内部冻结常数，非门容差）。
- 登记面: 本节容差同步登记于 docs/modules/registry/astrocs.phase2.reject.md §独立 synthetic 验证节（TEST-P2-REJ-DESIGN-001 设计冻结 VERIFIED，承载 TEST-P2-REJ-001 登记锚）。

### 11.5 SCI 层状态声明（本域零 SCI 改动）

- 排异语义权威已有 FROZEN SCI: SCI-REJ-001（docs/science/
  REJECTION.md，集合 SCI-REJ-001..008）。**共享 SCI 引用不改动**
  （P1-WCS/P2-COV/P2-INT/P2-HIPS 同构）。
- matrix P2-REJ 行 science_id=SCI-P2-REJ-001（descriptor 占位词汇，
  module_adapters.cpp:715）的语义映射由本节声明——
  **SCI-P2-REJ-001 ⇒ SCI-REJ-001**（docs/science/REJECTION.md，
  矩阵 science_doc=docs/science/REJECTION.md，MOD-astrocs-phase2-
  reject 行）。descriptor 占位
  SCI-P2-REJ-001 不入矩阵（无 docs/science 权威页）；SCI 公式语义
  不在此重复定义，两处冲突时以 docs/science/ 为准并回改本文档
  （禁止反向）。
- descriptor 占位 alg_id=ALG-P2-REJ-001 恰与本文件 ID 同名——以
  本文件（docs/algorithms/PHASE2_REJECTION.md）为该 ID 的唯一
  权威页；descriptor 占位 data_id=DATA-P2-REJ 与 DATA_SEMANTICS
  §22 同名对齐；api_id=API-P2-001（编排层词汇）的 kernel 消费面
  细化=API-P2-REJ-001（PUBLIC_API.md），两 ID 并存（API-P2-001
  编排层仍 VERIFIED）。本节禁止被编排层词汇反向改写（descriptor
  astrocs.phase2.reject 由 P2-XX-INT 对齐，不作冻结依据）。

## 12 关联 ID 映射（本文件承接）

- `ALG-P2-REJ-001` = 本文档整体（逐符号锚 §3/§5；矩阵 P2-REJ 行
  algorithm_id）。
- `ALG-REJ-001..008`（SCI §12；共享层 ALG 词汇，
  docs/algorithms/REJECTION_ALGORITHMS.md 承载，DERIVED 零改动）
  ⇒ 本文档 §5: ALG-REJ-001 None（F4 前置 none :1465-1467）；
  ALG-REJ-002 Sigma/robust_mad（F4）；ALG-REJ-003 Winsorized
  （F5）；ALG-REJ-004 AveragedSigma（F6）；ALG-REJ-005 LinearFit
  （F7）；ALG-REJ-006 ESD（F8）；ALG-REJ-007 RCR（F9）；
  ALG-REJ-008 Percentile/Minmax + large_scale + wbpp 路由/状态
  分层（F1/F10/F12/F13/F14）。共享 ID 不抢注、不重复登记
  （INDEX.yaml ALG-REJ-001 path 维持 REJECTION_ALGORITHMS.md，
  downstream 追加 ALG-P2-REJ-001 指向语义承接）。
- `RJ-001..008` = SCI-REJ-001..008 别名（SCI 头注）；NONE 对非有限候选
  不得回读为接受、ESD 单 sqrt（:1737）由本文档 §5 冻结承接。

## 13 追溯

- 实现: lib/algorithms/coverage/src/rejection.cpp（2950 行）+
  lib/algorithms/coverage/include/astro/phase2/rejection.h（595 行）（实测）。
- 合同: DATA-P2-REJ（DATA_SEMANTICS §22）/ API-P2-REJ-001
  （PUBLIC_API.md）/ TEST-P2-REJ-001（设计冻结 VERIFIED=registry
  承载页 §独立验证节；可执行落地归 P2-REJ-TEST + EVIDENCE）。
- 交叉: docs/modules/phase2_rej.md + lib/algorithms/rejection/ 三件套
  （README/module.yaml/memory.md，按 lib/algorithms/integration/ 先例新建；
  lib/algorithms/coverage/ 三件套已被 P2-COV 占用）；registry
  astrocs.phase2.reject.md；REJECTION_ALGORITHMS.md（旧 L2，ID
  让位/承接关系见 §12）。
- 消费者: stage2.cpp（§6；DATA_SEMANTICS §20 编排域）/
  acr_kernels.cpp（ACR 域）/ eng/tests/unit/p2_rejection_test.cpp
  （P2-005 语义 id/解析面）/ eng/tests/backend/test_p2004_reject_
  integrate.py（P2-004 生产 Oracle）/ module_adapters.cpp:704-720
  descriptor 占位。
- SCI: docs/science/REJECTION.md（SCI-REJ-001..008，FROZEN）。**阈值表、
  判据带定义、§10 禁改清单零改动**；本文档只同步行号锚、profile 依赖的
  `underdetermined_n` 取值与 percentile 适用域（§8a）的量化表述。

## 14 合同落位

- DATA-P2-REJ = docs/contracts/DATA_SEMANTICS.md §22：输入
  eligibility/gather/kernel 三层 + P2RejectionDecision 输出 +
  八态状态机 + 单位/确定性唯一权威；与 §21 DATA-P2-INT 的消费
  边界 = accepted mask → P2PixelStack.accepted。
- API-P2-REJ-001 = docs/contracts/PUBLIC_API.md 末节：
  planning/eligibility/gather/kernel/large_scale 导出符号冻结；
  compat p2_reject_stack 冻结两符号；与 API-P2-001 编排面并存。
- MOD = docs/modules/phase2_rej.md（模块页）+ lib/algorithms/rejection/
  三件套（README/module.yaml CONTRACT_READY entrypoint=MISSING/
  memory.md，按 lib/algorithms/integration/ 先例）+ registry
  docs/modules/registry/astrocs.phase2.reject.md（手写合同页
  重写；TEST 登记面承载）。
- 一致性声明: 本文件（ALG）与上述同批产物冲突时以本文件为
  算法/锚权威，DATA/API 以各自文件为单位/dtype/消费面权威；
  SCI 权威永远在 docs/science/（禁止反向）。
- 缺陷联动: DISP-P2REJ-001..004 同时登记于 registry 页与
  module.yaml known_defects；本文件 §7 为权威表述。
- TRACEABILITY_MATRIX.json MOD-astrocs-phase2-reject 行:
  science_id=SCI-REJ-001（映射 §11.5）/
  algorithm_id=ALG-P2-REJ-001（本文件）/data_id=DATA-P2-REJ/
  api_id=API-P2-REJ-001/src_id=SRC-P2-REJ-001（src_path=lib/
  phase2/include/astro/phase2/rejection.h::p2_reject_plan_resolve,
  p2_reject_stack_ex,p2_collect_candidate_stack,
  p2_eligibility_filter,p2_large_scale_apply,
  p2_rejection_semantic_id）/test_id=TEST-P2-REJ-001
  （test_path=docs/modules/registry/astrocs.phase2.reject.md::
  TEST-P2-REJ-001，设计冻结 VERIFIED + 可执行 MISSING 双
  statement）——由主控写入，本节仅声明预期终态。

## 参考文献与参考代码库（含许可证）— SCI-001-S2 补齐

> 本节只给可核验出处与参考实现，不改动本文件任何公式、锚点、阈值与容差。

- **Generalized ESD**：Rosner, B. 1983, *Percentage Points for a Generalized ESD Many-Outlier Procedure*, Technometrics **25**, 165-172（DOI [10.1080/00401706.1983.10487848](https://doi.org/10.1080/00401706.1983.10487848)，Crossref 元数据核验：题名/卷/页/作者一致）——**支持**本层 ESD 的「多离群、回看定 k_out」判据。独立可执行实现与临界值表：NIST/SEMATECH e-Handbook §1.3.5.17/§7.1.6。
- **RCR**：Maples, M. P., Reichart, D. E., Konz, N. C., et al. 2018, *Robust Chauvenet Outlier Rejection*, ApJS **238**, 2（DOI [10.3847/1538-4365/aad23d](https://doi.org/10.3847/1538-4365/aad23d)；[arXiv:1807.05276](https://arxiv.org/abs/1807.05276)，题名/作者核验一致）——**支持**本层 3-pass 链（序贯更换集中趋势测度）与经验修正因子表；后续方法学 Konz, N. & Reichart, D. E. 2023, [arXiv:2301.07838](https://arxiv.org/abs/2301.07838)（摘要逐字：sequentially applying different measures of central tendency and empirically determining the rejective sigma value）。
- **percentile 判据带**：Siril 1.4.3 `src/stacking/rejection_float.c:31-44`（`percentile_clipping`：`median - pixel > median*plow` ⇒ 低拒、`pixel - median > median*phigh` ⇒ 高拒）与 `:163-171`（`if (median == 0.0) return 0;` 零中位提前返回）——**支持**本层 §5 F10 的判据带形式与 `scale=|median|` 语义；本层不采用 `:163-171` 的提前返回分支（理由见 docs/science/REJECTION.md §8a）。
- **winsorization/稳健尺度**：Hoaglin, Mosteller & Tukey (eds.) 1983, *Understanding Robust and Exploratory Data Analysis*, Wiley（ISBN 0-471-09777-2）——书籍级背景。
- **Tukey biweight**：Beaton, A. E. & Tukey, J. W. 1974, *The Fitting of Power Series, Meaning Polynomials, Illustrated on Band-Spectroscopic Data*, Technometrics **16**, 147-185（DOI [10.1080/00401706.1974.10489171](https://doi.org/10.1080/00401706.1974.10489171)）——**本层 11 个方法均不使用 biweight 核**，该引用只作稳健估计背景，不作为任何方法的语义依据。
- **clipped-mean/伪影**：Gruen, D., Seitz, S. & Bernstein, G. M. 2014, PASP **126**, 158（页面级未核验）。
- **开源对照**：PixInsight WBPP 2.9.1 `bestRejectionMethod`（非学术软件来源，档界来源）；IRAF `imcombine`（方法族命名来源）。**核语义依据 = Siril 1.4.3 源码**（上列 `:31-44`，逐式核验）；IRAF/PixInsight 页面级未核验。

参考代码库（含许可证；GPL 代码仅作行为/数值对照，不复制进本仓）：
- Astropy（BSD-3-Clause，https://github.com/astropy/astropy）；photutils（BSD-3-Clause，https://github.com/astropy/photutils）；astropy-healpix（BSD-3-Clause，https://github.com/astropy/astropy-healpix）；ccdproc（BSD-3-Clause，https://github.com/astropy/ccdproc）；reproject（BSD-3-Clause，https://github.com/astropy/reproject）。
- DrizzlePac（BSD-3-Clause，https://github.com/spacetelescope/drizzlepac）。
- SExtractor / PSFEx / SWarp / SCAMP（GPL-3.0，https://github.com/astromatic/）。
- healpy（GPL-2.0，https://github.com/healpy/healpy）；Siril（GPL-3.0，https://gitlab.com/free-astro/siril）；LSST ip_isr（GPL-3.0，https://github.com/lsst/ip_isr）；GSL（GPL-3.0，https://www.gnu.org/software/gsl/）。
- WCSLIB（LGPL-3.0）；CFITSIO（宽松许可，NASA/HEASARC，https://heasarc.gsfc.nasa.gov/fitsio/）。
- NumPy / SciPy（BSD-3-Clause）：独立 FP64 Python Oracle。


---

## 对照档 `wbpp_2_9_1` 的过拒率证据（受控真值）

- **true sample FPR = 1.88%（565 / 30000）**；与冻结的 Siril 1.4.3 harness 同源 case 上 **8000 decisions 逐样本 100% 一致**（Siril 自身 1.8375%）⇒ 该过拒率是 **frozen Siril reference 的行为**，不是 ACSD 过拒。
- **pixel any-rejection FPR = 26.3%**（任一帧被拒即计）。
- 科学量偏差：星点通量 **−0.07%**、FWHM **+0.012%**、faint structure **−0.29%**、背景噪声效率 **1.045**、三类 outlier recall = **1.0**。
- 该组数字只作**对照档行为证据**登记，不改变 §5 四档路由与阈值表；复跑入口 = `lib/algorithms/coverage/tools/controlled_rejection_truth.py`（受控真值）与 `lib/algorithms/coverage/tools/rejection_oracle_compare.py`（Siril 1.4.3 harness 逐位对照）。

