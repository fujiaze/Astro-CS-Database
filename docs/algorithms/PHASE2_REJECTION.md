# Phase2 Rejection Algorithms（P2-REJ / astrocs.p2.rejection）

> ID: ALG-P2-REJ-001  状态: CONTRACT_READY（P2-REJ-DOC 冻结，2026-09-09）
> 模块: lib/phase2/src/rejection.cpp（2076 行，astrocs_phase2 静态库
> 成员，根 CMakeLists.txt:336-346/:340）+ 唯一权威签名头
> lib/phase2/include/astro/phase2/rejection.h（329 行）
> 权威: 本文档（算法级逐符号锚）。SCI 上游: SCI-REJ-001
> （docs/science/REJECTION.md，FROZEN T107 2026-08-23，集合
> SCI-REJ-001..008，零改动；descriptor 占位 SCI-P2-REJ-001 ⇒
> SCI-REJ-001 映射声明见 §11.5）。共享 L2: ALG-REJ-001
> （docs/algorithms/REJECTION_ALGORITHMS.md，DERIVED，零改动，语义
> 承接见 §12）。DATA: DATA-P2-REJ（DATA_SEMANTICS §22）。API:
> API-P2-REJ-001（PUBLIC_API.md）。TEST: TEST-P2-REJ-001（设计冻结
> 面=本文档 §11.4；可执行落地归 P2-REJ-TEST）。迁移目标
> astrocs_p2_rejection.dll 为矩阵合同值（MISSING），由 P2-REJ-IMPL
> 建立，本文件不声明 IMPLEMENTED；descriptor 占位
> module_id=astrocs.phase2.reject（module_adapters.cpp:639-658）由
> P2-XX-INT 对齐，不作冻结依据。
> 关联: DATA=DATA-P2-REJ（docs/contracts/DATA_SEMANTICS.md §22，P2-REJ-DOC 同批冻结）；API=API-P2-REJ-001（docs/contracts/PUBLIC_API.md 末节，同批）；MOD 页=docs/modules/phase2_rej.md + registry docs/modules/registry/astrocs.phase2.reject.md（同批手写合同页）；TEST 登记面=registry 页 §独立 synthetic 验证节（TEST-P2-REJ-DESIGN-001 设计冻结 VERIFIED）。

## 1 目的与非目标

- **目的**：每像素候选栈排异决策——eligibility 单路径 gather（strided
  frame-major）→ planning 层 AUTO 一次解析（nominal n 路由）→ 显式
  方法核（10 方法，per-sample reason 与 stack-level status 分离）→
  large_scale 结构生长后处理（trail 扩张，compact cosmic 不生长）。
  本文件为阈值/迭代权威锚定文档（rejection.cpp:1-11 冻结头注释
  "本文件为阈值/迭代权威实现，禁止阈值漂移"）。
- **非目标**：不合并/积分样本（SCI-INT-001 / ALG-P2-INT-001）；不
  做权重策略（weights 数组外置，构造在 Stage2 weight_mode；RCR 核
  消费同栈 weights 数组属官方加权语义，非策略）；不做像素外结构
  重建（large_scale 仅对已拒 mask 做 8 邻域扩张，只增不减）；无
  session 依赖（无状态纯函数）；不做瞬变/卫星语义区分（SCI §1
  非目标）；单帧无排异（n=1 进 UNDERDETERMINED 白名单）。

## 2 符号与单位（权威=本表 + DATA_SEMANTICS §22 + SCI §3）

| 符号 | 含义 | 单位/dtype | 锚 |
|---|---|---|---|
| `values[i]` | eligible 候选科学值（kernel 工作域输入） | ADU，f64（gather f32 源→f64 提升） | rejection.h:268 |
| `weights[i]` | 候选科学权重（可空=等权） | 1/ADU²（数值域；策略在调用方），f64 | rejection.h:269 |
| `frame_ids[i]` | 稳定帧标识（ESD tie-break/确定性） | 无量纲 u64 | rejection.h:270 |
| `count` | 候选数（=资格后 n_eff） | 无量纲 u32 | rejection.h:271 |
| `reasons[i]` | per-sample 判定 | u8 0..3（P2RejectReason） | rejection.h:73-78/:277 |
| `status` | stack-level 终态 | int 0..7（P2RejectStatus） | rejection.h:81-90/:282 |
| `n`（nominal） | planning 层几何可贡献数（一次解析） | 无量纲 u32 | rejection.h:174-177 |
| `med/scale` | 工作域中位数/尺度 | ADU（MEDIAN_SCALE 无量纲化后 1） | rejection.cpp:1749-1767 |
| `σ_lo/σ_hi` | 低/高侧阈值（sigma 族） | ADU（z 无量纲阈值 4.0/3.0） | rejection.cpp:1052-1056 |
| `alpha` | ESD 显著性水平 | 无量纲 0.05 | rejection.cpp:1059（默认） |
| `iterations` | 外层迭代计数（kernel 计量） | 无量纲 u32 | rejection.h:281 |

权重语义（调用方构造，本层无知）: weight_mode=2 → ivar（经
`source_indices` 回映射原始 slot，stage2.cpp:1098/:1361/:1379）；
weight_mode=0 → support×snr²（legacy/诊断，ACR 域 acr_kernels.cpp:158）；
null → 等权。reducer 只消费权重数组本身（与 ALG-P2-INT-001 §2
同一政策）。

## 3 逐符号锚（rejection.cpp 2076 行 / rejection.h 329 行，2026-09-09 实测）

| 符号/段 | 锚（rejection.cpp） | 语义 |
|---|---|---|
| 冻结头注释 | :1-11 | 阈值表 + "禁止阈值漂移"（SCI-REJ-*/ALG-REJ-001..008 锚定） |
| ibeta_cf / ibeta | :30-62 / :64-74 | Lentz 连分数 I_x(a,b)（NR betai）；lgamma 域 |
| t_cdf / t_quantile | :77-82 / :85-95 | Student-t CDF（对称）；分位数=二分 80 轮 [0,40] |
| kRcrSSDLUnityCF / kRcrSSUnity / kRcrSSConstants | :117-130 / :133-259 / :261-263 | RCR 官方 frozen 查找表（101/1001/2×8） |
| rcr_is_equal | :265-270 | 相等容差 rel ≤1e-8（官方 isEqual） |
| rcr_distinct_values | :273-293 | distinctValuesCheck：flagged 中 ≥3 不同值 |
| rcr_get_median / _w | :296-308 / :310-327 | 官方 getMedian（偶数平均/奇数中位；加权同步排序） |
| rcr_inverf | :329-337 | erfcCustom 逆（A&S 7.1.26 16 次幂有理近似）；rcr_get_xvec/_w :339-349/:351-365、rcr_count_under_one :367-373 |
| rcr_origin_regression(/_w) | :375-384 / :386-397 | 过原点回归 σ/τ |
| rcr_mfinder(/_w) | :399-457 / :459-520 | 官方 broken-stick 折点搜索（增量细化 /6.36） |
| rcr_fit_sl(/_w) | :522-525 / :527-531 | 单线拟合（under≤1） |
| rcr_fit_dl(/_w) | :590-634 / :636-685 | 双线拟合（under>2；broken stick） |
| rcr_get_68th(/_w) | :686-701 / :703-727 | 官方 get68th（升序 diff 的 68 分位） |
| rcr_get_single_dl_cf(/_w) | :729-745 / :746-778 | single-sigma 修正因子（表插值） |
| rcr_erfc_custom | :780-788 | erfc(z/√2)（Chauvenet 尾概率） |
| rcr_iterative_pass | :796-916 | 单段 iterative Chauvenet（mu/sigma 技术·加权分支） |
| method_minimum_n | :921-935 | 方法最小 N 注册表（NONE 0/σ 族+ESD+RCR+MEDIAN_SIGMA 3/LINEAR_FIT 4/PERCENTILE 2/MINMAX 3） |
| set_err | :937-941 | err 缓冲日志文本（仅日志，不承载语义） |
| ScratchVec | :944-986 | n≤64 固定 scratch（无每像素堆分配）；>64 堆 fallback |
| scratch_median / scratch_mad | :989-997 / :999-1002 | nth_element 中位（偶数均值）；MAD=median(|x−med|)，σ=1.4826·MAD 由调用方乘 |
| p2_reject_plan_resolve | :1028-1084 | planning 层 AUTO 解析 + typed 默认值（冻结表，§5 F1） |
| eligibility_core | :1094-1124 | 连续版 policy core（finite→valid→support→quality 严格大于门；support 严格大于 :1108） |
| p2_eligibility_filter | :1128-1148 | 资格层连续版入口（compat 路径消费） |
| p2_collect_candidate_stack | :1150-1225 | 生产 strided gather（f32/f64；source_indices 显式保留 eligible→原 slot） |
| reject_none_impl | :1235-1237 | 全 ACCEPTED |
| reject_robust_mad_impl | :1240-1282 | sigma=median+1.4826·MAD 迭代 clip |
| reject_winsorized_impl | :1285-1344 | winsorized_sigma（Siril 1.4.3 语义 :1284 注释） |
| reject_averaged_impl | :1347-1382 | averaged_sigma（mean+mean|resid|·√(π/2)） |
| reject_linear_fit_impl | :1385-1479 | linear_fit（Siril 1.4.3 frozen harness） |
| reject_esd_impl | :1482-1552 | generalized ESD（NIST；单 sqrt RJ-005 :1507 注释） |
| reject_rcr_impl | :1555-1576 | RCR 3-pass 链（Median+DoubleLine → Median+68th → Mean+StdDev） |
| reject_percentile_impl | :1579-1600 | percentile（|median| 尺度；iterations=1） |
| reject_median_sigma_impl | :1603-1645 | median+SD 迭代 clip |
| reject_minmax_impl | :1648-1680 | 固定 rank 一次性删除（不迭代） |
| p2_reject_stack_ex | :1684-1858 | 唯一生产 kernel 入口（§4 状态机 + §5 F3/F14） |
| p2_reject_stack（compat） | :1863-1974 | 旧接口 adapter（资格层+MIN_SAMPLES :1891-1900+typed 换算 :1913-1942+non-finite 覆盖 :1971-1972） |
| large_scale_grow_side | :1983-2047 | 8 邻域连通分量（DFS）→ 分量≥min_size 合格 → Chebyshev 半径扩张 → 只增不减 |
| p2_large_scale_apply | :2051-2074 | 逐帧 low/high 独立半径调用；参数非法 rc=1 |

| 头文件段 | 锚（rejection.h） | 语义 |
|---|---|---|
| 三层输入模型注释 | :6-29 | EligibilityPolicy / RejectionPlan / RejectionNormalizationPolicy；Oracle 清单（Astropy mad_std/NIST/Siril 1.4.3 GPL ORACLE ONLY/RCR 2.4.7 ORACLE ONLY :25-28/PIXINSIGHT_EXACT=NOT_CLAIMED :28-29） |
| P2RejectionMethod | :45-57 | 10 方法枚举 + AUTO=10（:56，kernel 永不接收 AUTO） |
| P2_SEMANTIC_* | :59-70 | canonical semantic id 常量（astrocs.*.v1，运行时映射 p2_rejection_semantic_id rejection.cpp:1011-1025） |
| P2RejectReason | :71-77 | ACCEPTED=0/REJECTED_LOW=1/REJECTED_HIGH=2/UNDERDETERMINED=3 |
| P2RejectStatus | :81-90 | OK=0..INTERNAL_ERROR=7（:89）八态 |
| P2RejectionNormalization | :92-97 | NONE=0/MEDIAN_CENTER=1/MEDIAN_SCALE=2（floor 默认 1e-12 在 plan 字段 :157） |
| typed params | :99-130 | P2SigmaParams :100-104/P2LinearFitParams :106-110/P2EsdParams :112-115/P2PercentileParams :117-120（low_fraction 注释 "默认 0.1" 漂移 :118）/P2MinmaxParams :122-126/P2RcrParams :128-130（禁止跨方法共享 low/high/max_iter，:99 注释） |
| P2LargeScaleParams | :132-149 | enabled/min_structure_pixels=8/low·high_grow_radius_pixels=2（默认关闭 :142 注释；语义 :132-143） |
| P2RejectionPlan | :151-169 | 显式计划（method/minimum_n/underdetermined_n=2/normalization/floor/typed 大成员） |
| P2RejectionPlanRequest | :171-180 | request（允许 AUTO）/nominal_contributors :174-177/profile :178/underdetermined_n :179 |
| plan_resolve 注释 | :182-190 | WBPP 2.9.1 路由（:183-184）+ profile 语义（:185-189）；声明 :191-193 |
| P2EligibilityInput/Output | :199-220 | 连续版资格层（support_threshold 严格大于 :206；quality_flags_required=0 不要求 :207）；filter 声明 :222 |
| P2EligibilityGatherInput/Output | :227-261 | 生产 strided gather（Input :227-245/Output :247-261）；PHASE2_IVAR_WIRING 注释 :252-255（source_indices 权威映射 :255，compact 后禁止猜 original slot）；gather 声明 :263 |
| P2CandidateStack | :266-273 | kernel 输入（values/weights/frame_ids/count/data_type 0=fp32,1=fp64 仅诊断 :272） |
| P2RejectionDecision | :275-283 | reasons u8 :277/accepted_count/rejected_low/rejected_high/iterations/status :282 |
| p2_reject_stack_ex 声明 | :285-289 | kernel n≤64 固定 scratch（:286）；AUTO 非法（:285 注释） |
| p2_large_scale_apply 声明 | :291-297 | frame-major 每帧 width×height 字节原地修改（:292）；仅扩张 ≥min_structure_pixels 结构；参数非法 rc=1（:294） |
| compat P2SampleStackView/Result | :299-325 | 旧接口（:299 冻结注释"仅测试/旧调用；生产 Stage2 不再调用"；sigma_low/sigma_high/max_iterations/min_samples 兼容换算 :310-313） |

## 4 状态机与返回码（八态显式、互斥；reason 4 值正交）

### 4.1 stack status（rejection.h:79-90；判据=本文档 §5）

| status | 值 | 触发条件（精确） | 锚（rejection.cpp） | 冻结测试证据 |
|---|---|---|---|---|
| OK | 0 | 判定完成且全栈无 UNDERDETERMINED reason 且 accepted_count>0 | :1853-1855 | G6PermutationInvariance :2863；V15ExPermutationInvarianceTyped :4443 |
| MIN_SAMPLES | 1 | count==0（ex :1706）∨ 资格数 < min_samples（compat :1896-1907） | :1706/:1896-1907 | R2MinSamples :2658-2672 |
| ALL_REJECTED | 2 | accepted_count==0 且 n>4（全拒非小栈） | :1851 | V15SatelliteTrail20Frames 负样本集；p2_reject_stack_ex 直测 |
| INVALID_INPUT | 3 | 任一候选 values 非 finite（ex）；compat: has_nonfinite 且 status≠ALL_REJECTED（:1971-1972） | :1709-1718/:1971-1972 | V15NoneDoesNotReacceptNaN :4138 |
| UNDERDETERMINED | 4 | n ≤ underdetermined_n(2) ∨ n < method minimum_n（:1741）；或部分 UNDERDETERMINED reason（:1854）；或 N≤4 全拒容错 fallback（:1842-1850） | :1739-1747/:1842-1855 | V15SatelliteN2Underdetermined :4241 |
| INVALID_CONFIGURATION | 5 | PERCENTILE×norm≠MEDIAN_CENTER（:1722-1728）∨ RCR×norm≠NONE（:1730-1736） | :1722-1736 | V16InvalidConfigurationCombos :4543 |
| INVALID_METHOD | 6 | plan.method 出界（含 AUTO=10 进 kernel） | :1688-1701（status :1699） | V17InvalidMethodStatus :4763 |
| INTERNAL_ERROR | 7 | kernel 内部不变量破坏（现状不可达；保留态） | h:89 | —（设计保留） |

### 4.2 per-sample reason（rejection.h:71-77；与 status 分离，SCI §7 状态分离不变量）

| reason | 值 | 语义 | 锚 |
|---|---|---|---|
| ACCEPTED | 0 | 未被拒（含 UNDERDETERMINED 白名单外的保留样本） | :1235-1237 等 |
| REJECTED_LOW | 0→1 | 低于 lower threshold（**禁止用原始值正负号判向**，h:20-21 冻结注释） | 各核尾段 |
| REJECTED_HIGH | 0→2 | 高于 upper threshold | 同上 |
| UNDERDETERMINED | 0→3 | 样本数不足，未做拒绝判定（全接受语义） | :1739-1745 |

计数器口径（:1820-1834）: accepted_count 含 UNDERDETERMINED 样本
（全接受语义）；rejected_low/high 仅统计显式拒绝；iterations=
kernel 外层迭代（ESD=k_out；RCR=3 常量；percentile/minmax=1）。

### 4.3 rc（函数返回）与 status 正交

- `p2_reject_stack_ex`: rc=1 仅 stack/plan/out null（:1687）或
  reasons/values null 且 count>0（:1707）；rc=0 时语义全由 status
  承载（含 INVALID_METHOD/INVALID_INPUT/INVALID_CONFIGURATION——
  "科学状态"而非调用错误）。
- `p2_reject_plan_resolve`: rc=0 OK；rc=1 null 请求/plan 或 request
  出界或 profile 非法（:1031-1046；err 仅日志文本）。
- `p2_eligibility_filter` / `p2_collect_candidate_stack`: rc=1 null
  in/out 或必要输出缓冲缺失（:1129-1140/:1152-1163）；n==0 → rc=0
  空输出。
- `p2_large_scale_apply`: rc=0 OK（disabled=noop :2061）；rc=1
  指针 null ∨ width/height/depth ≤0 ∨ min_structure_pixels<1 ∨
  负半径（:2054-2060）。
- 调用方合同: 对 ex 返回 status ∈ {OK, UNDERDETERMINED} 才可继续
  积分，其余 hard fail（stage2.cpp:1189-1195/:1454-1461 冻结门）。

## 5 逐公式定义（算法级，与 SCI §5 同构；单位见 §2）

### F1 planning 解析（p2_reject_plan_resolve :1028-1084）

```text
typed 默认值（冻结阈值表;SCI §5 阈值冻结锚点逐项一致）:
  underdetermined_n = req>0 ? req : 2                        :1048
  normalization = MEDIAN_CENTER;  floor = 1e-12              :1049-1051
  sigma/winsorized/averaged = (4.0, 3.0, 8)                  :1052-1056
  linear_fit = (5.0, 3.5, 8)                                 :1057-1058
  esd = (alpha 0.05, max_outliers 10)                        :1059
  percentile = (low 0.2, high 0.1)                           :1060
  median_sigma = (4.0, 3.0, 8)                               :1061-1062
  minmax = (low 1, high 1, min_kept 4)                       :1063-1064
  rcr.technique = 0（SS_MEDIAN_DL 唯一支持）                  :1065
  large_scale = (enabled 0, min_structure 8, low 2, high 2)  :1066-1069
AUTO 路由（nominal n 一次解析；两 profile 共用阈值表）:       :1071-1079
  request==AUTO: n<6 → PERCENTILE; 6≤n≤15 → WINSORIZED_SIGMA;
                 n>15 → LINEAR_FIT
  minimum_n = method_minimum_n(method)                        :921-935
profile 合法集 = {wbpp_2_9_1, wbpp_current, astrocs_adaptive}  :1039-1046
  （nullptr → wbpp_2_9_1；其余 rc=1；wbpp_current=group active
  count 一次解析，astrocs_adaptive=tile nominal depth——区别仅在
  nominal 来源，h:174-177/:182-190 冻结注释）
```

### F2 eligibility gather（p2_collect_candidate_stack :1150-1225；core :1094-1124）

```text
逐 slot s=0..count-1（frame-major strided: v=values[s*stride+pixel]）:
  合格 ⇔ isfinite(v)                                       :1188
        ∧ (valid==null ∨ valid[s*valid_stride+pixel])      :1189-1191
        ∧ (support==null ∨ support[s*support_stride+pixel]
             > support_threshold)      # 严格大于           :1192-1198
        ∧ (quality==null ∨ required==0 ∨
             (q & required)==required)                     :1199-1203
合格样本紧凑写入 values/weights/support/frame_ids            :1205-1221
source_indices[cnt] = s（eligible→original slot 权威映射;
  ivar/quality/variance 一律经此映射，禁止 compact index 猜测,
  h:252-255 PHASE2_IVAR_WIRING）                            :1207-1209
诊断计数 invalid_finite/valid/support/quality               :1188-1203
```

### F3 normalization 工作域（ex :1749-1767）

```text
work[i] = values[i]（原始域副本）                            :1749-1755
norm != NONE: orig_median = median(values)                  :1756-1760
  MEDIAN_CENTER: work -= orig_median      # 判定工作域       :1761-1762
  MEDIAN_SCALE:  work /= max(|orig_median|, floor)           :1763-1766
decision 作用于 work；accepted mask 应用回原始值
  （h:15-17 冻结注释）；PERCENTILE 的 scale=|orig_median| 为原始域
  中位（:1579-1600 :1587），MEDIAN_CENTER 下负值安全。
```

### F4 sigma = robust_mad_clip（:1240-1282）

```text
迭代 it=0..max_iterations-1:                                :1253
  cur = {w[i]: accept[i]}（accept 初始全 1）                 :1254-1256
  nc<2 → break                                              :1258
  m = median(cur);  s = 1.4826 · median(|cur−m|)            :1261-1262
  s ≤ 1e-12 → break                                         :1263
  z_i = (w[i]−m)/s;  z<−|σ_lo| ∨ z>|σ_hi| → 拒             :1269-1271
  无新拒 → break;  计数器 it 外层                             :1273-1276
判定方向: w[i] < m → LOW else HIGH（:1279-1280）
Astropy sigma_clip(median+mad_std) oracle（h:23）。
```

### F5 winsorized_sigma（:1285-1344；Siril 1.4.3 语义锚 :1284）

```text
外层 iters=0..max_iterations-1（r=累计拒绝数，跨外层不清零）:
  cur = accept 集;  nc<2 → break                            :1299-1303
  med = median(cur)                                         :1306
  s0 = sqrt(Σ(cur−med)²/(nc−1))          # accept 集 σ      :1307-1310
  s0 ≤ 1e-12 → break                                        :1311
  内层 winsor 收敛（≤64 轮）:                                :1313-1327
    clamp cur 到 [med−1.5σ, med+1.5σ]                       :1314-1317
    wsd = sqrt(Σ(wcur−med)²/(nc−1));  σ_new = 1.134·wsd     :1318-1322
    |σ_new−σ| ≤ 5e-4·σ → 收敛 break                         :1323-1326
  clip: z=(w[i]−med)/σ;  z<lo ∨ z>hi → 拒, ++r              :1332-1341
    guard (int)nc − r ≤ 4 → break（保底 4 样本，本外层剩余冻结接受）
  无新拒 → break
判定方向: w[i] < med → LOW else HIGH（:1341-1342 尾段）
```

### F6 averaged_sigma（:1347-1382）

```text
mean = Σ_{accept} w[i] / nc                                 :1358-1362
s = (Σ_{accept} |w[i]−mean| / nc) · √(π/2)                  :1363-1366
s ≤ 1e-12 → break;  z=(w[i]−mean)/s 超阈拒                  :1367-1374
判定方向: w[i] < mean → LOW else HIGH（:1377-1380）
```

### F7 linear_fit（:1385-1479；Siril 1.4.3 frozen harness 锚 :1384）

```text
预计算（一次）:  m_x = (n0−1)/2                              :1402
  xf[j] = 1/(j+1);  m_dx2 = 1/Σ dx²·xf  (Welford 风格)      :1403-1409
迭代 it<max_iterations:
  N = |stack|;  N<4 → break                                 :1416
  按 value 升序排序 (value, orig_index) 字典序（tie-break=原索引,
  置换不变）                                                :1417-1423
  m_y = Welford 加权均值（权 xf[i]）                          :1424-1426
  m_dxdy = Welford（dx=j−m_x, dy=stack[i]−m_y）;             :1427-1432
  slope = m_dxdy·m_dx2;  intercept = m_y − m_x·slope        :1433-1434
  σ = mean|stack[i] − fit(i)|            # 平均绝对残差       :1435-1438
  判定: fit(j)−stack[j] > σ·σ_lo → LOW;                     :1445-1449
        stack[j]−fit(j) > σ·σ_hi → HIGH
    guard (int)(N−r) ≤ 4 → keep[j]=1 continue               :1443
  收缩到 kept 集，next iteration                             :1458-1471
尾段: 未显式拒 → ACCEPTED（:1474-1478）
逐位 oracle: LinearFitFindsOutlier（rng42 固定向量与未修改 Siril
1.4.3 官方 harness 逐位核对=拒 {30, 43..49} 共 8，:2699-2702 冻结注释）。
```

### F8 generalized ESD（:1482-1552；NIST oracle）

```text
max_out = max(1, max_outliers);  alpha ≤0 → 0.05            :1489-1490
for rr=0..max_out-1:
  nc = |accept|;  nc<3 → break                              :1502
  mean = Σaccept work/nc                                    :1499-1503
  s = sqrt(Σaccept (work−mean)²/(nc−1))   # 单 sqrt（RJ-005）:1504-1507
  s ≤ 1e-12 → break                                         :1508
  worst = argmax_{accept} |work−mean|/s                     :1509-1523
    tie-break: |rv−max_r| ≤ 1e-15 时取 frame_id 较小者
  n_minus_i = n−(rr+1);  nu = n_minus_i−1                   :1524-1525
  p = 1 − alpha/(2(n_minus_i+1))                            :1526
  tcrit = t_quantile(p, nu)               # 二分 80 轮 [0,40] :1527/:85-95
  Lambda = (tcrit·n_minus_i)/sqrt((nu+tcrit²)(n_minus_i+1))  :1528-1529
  R[rr]=max_r;  accept[worst]=0（先全摘，回看定 k_out）        :1530-1534
k_out = max{ rr+1 : R[rr] > Lambda[rr] }   # Rosner 回看准则  :1536-1538
仅前 k_out 个 removed 置拒；方向=原始值 vs 原始中位:           :1539-1551
  w[i] < med → LOW else HIGH
```

### F9 RCR（:1555-1576 + RCR 段 :100-916；官方 rcr 2.4.7 语义）

```text
固定 3-pass 链（:1563-1565）:
  pass1: mu=MEDIAN,  sigma=DOUBLE_LINE
  pass2: mu=MEDIAN,  sigma=68TH
  pass3: mu=MEAN,    sigma=STD_DEV
每 pass（rcr_iterative_pass :796-916）:
  cur = accept 集（加权: sort(w,y) 同步排序后官方 getMedian_w :812-830）
  diff_i = |cur_i − mu|;  worst=argmax diff（首个最大 :845-855）
  sigma 技术: DOUBLE_LINE → broken-stick 双线（under>2 :876-882）
              68TH → 官方 get68th；STD_DEV → 加权样本方差 :890-906
  σ = st_dev · single_dl_cf(n)（官方修正因子表插值 :907-909）
  Chauvenet 终止: distinct_values <3 值（:265-293 rel 1e-8）
    ∨ n·erfc(zmax/√2) ≥ 0.5 → break                        :911-913
  否则 accept[worst]=0，循环
加权分支: weights 非 null 时启用（官方 w·resid² 加权拟合/68th/CF）。
仅 normalization=NONE 合法（ex :1730-1736；raw 域）。
```

### F10 percentile（:1579-1600；小 N 无尺度估计）

```text
plow=|low_fraction|;  phigh=|high_fraction|                  :1585-1586
scale = |orig_median|（原始域中位，MEDIAN_CENTER 工作域下负值安全）:1587
w[i] < −scale·plow  → LOW                                   :1588-1591
w[i] >  scale·phigh → HIGH                                  :1591-1593
else ACCEPTED;  iterations=1（单轮，不迭代）                  :1599
```

### F11 median_sigma（:1603-1645）

```text
med = median(cur);  sd = sqrt(Σ(cur−med)²/(nc−1))           :1621-1628
sd ≤ 1e-12 → break;  z=(w[i]−med)/sd 超阈拒                 :1629-1636
guard (int)nc−r ≤ 4 → break（同 F5 保底）:1633
判定方向: w[i] < med → LOW else HIGH
```

### F12 minmax（:1648-1680；固定 rank 一次性删除）

```text
k_low=|reject_low_count|;  k_high=|reject_high_count|;
min_kept=max(1,min_kept)                                    :1654-1656
(k_low+k_high) ≥ n ∨ n−(k_low+k_high) < min_kept
  → 全栈 UNDERDETERMINED, iterations=0（:1657-1664）
按 value 升序 sort（比较器仅 value——tie-break 未显式冻结，§11.3
DISP-P2REJ-004）                                            :1665-1668
最低 k_low → LOW;  最高 k_high → HIGH                        :1671-1678
iterations=1
```

### F13 large_scale 结构生长（:1983-2074）

```text
p2_large_scale_apply: 参数非法 → rc=1（:2054-2060）；!enabled →
  noop rc=0（:2061）；逐帧 low/high 独立调用 grow_side
grow_side(mask, min_size, radius):                          :1983-2047
  radius ≤0 → mask 不变（仅 pixel 级拒绝）                    :1986
  8 邻域 DFS 连通分量；|comp| ≥ min_size → 分量全部 qualify   :1994-2021
  Chebyshev 扩张 radius 轮（qualify 的 8 邻域逐轮并入）       :2023-2042
  只增不减: 原始 pixel-level rejected（含小分量）保留          :2043-2046
trail 扩张 / compact cosmic 不生长（分量 <8 不 qualify）。
生产调用: stage2 两遍积分（:1544-1554 apply :1549，二次积分 :1555-1560）；
  OMP 路径 buffer 化（分配 :847-861/写回 :1468-1503），large_scale 激活强制串行。
```

### F14 tally 与小栈容错（ex :1820-1858）

```text
tally: accepted_count/rejected_low/rejected_high/iterations  :1820-1834
  UNDERDETERMINED reason 计入 accepted_count（全接受语义）
容错（冻结注释 :1835-1841 "仅调容错路径、阈值冻结不变"）:
  accepted_count==0 ∧ n≤4 → reasons 全 UNDERDETERMINED,
    accepted_count=n, status=UNDERDETERMINED（:1842-1849；
    根因=percentile 0.2/0.1 在 N=4 过严全拒，tile 116446 N_B=4）
  accepted_count==0 ∧ n>4 → status=ALL_REJECTED（:1851）
  否则 status = any_underdetermined ? UNDERDETERMINED : OK
```

## 6 消费链与并行语义

- **Stage2（编排，astrocs.p2.hips_writer 消费者）**:
  group 级 plan resolve（wbpp_2_9_1，nominal=cfg.hips.size()，
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
  权重经 source_indices 回映射原始 slot（:1098/:1361/:1379）；
  weight_mode=0 snr² 加权在 ACR 域（acr_kernels.cpp:147-158）。
- **确定性合同（matrix 专项）**: 同输入同 plan 同 fid → decision
  bitwise 确定且与 worker 数无关（每像素独立决策树；无共享累加
  器；ESD tie-break=frame_id :1515-1518；linear_fit 排序
  (value,orig_index) 字典序 :1417-1423；median 值级置换不变）。
  验证锚: G6PermutationInvariance :2863（method 0..6）、
  V15ExPermutationInvarianceTyped :4443；冻结容差见 §11.4。

## 7 与 SCI 的对应与偏差（如实登记）

- SCI §5 连续定义（7 方法+auto profile+阈值冻结锚点+eligibility
  分层+large_scale 语义）与本文档 §5 逐项同构；阈值表逐值一致
  （§5 F1；rejection.cpp:1-11 冻结头注释同表）。auto 路由=nominal n
  一次解析（SCI §6/§7 阈值不变量；:1071-1079 实测一致）。
- **percentile 默认注释矛盾（DISP-P2REJ-001，文档级）**:
  rejection.h:118 P2PercentileParams 注释 low_fraction "默认 0.1 =
  10%" vs 实现 plan_resolve 默认 low_fraction=0.2（:1060）与 SCI
  权威（REJECTION.md §5 percentile low 0.2/high 0.1）——实现与 SCI
  一致，header 注释漂移；整改=注释对齐（P2-REJ-IMPL）。
- **空栈状态命名矛盾（DISP-P2REJ-002，文档级）**: SCI §8 表
  "无候选 → NO_CANDIDATES（rejection.h:83）" vs 实现八态枚举
  （rejection.h:81-90）无 NO_CANDIDATES，空栈→P2_STATUS_MIN_SAMPLES
  （ex :1706 / compat :1869）；NO_CANDIDATES 为积分域
  P2IntegrateStatus 值（integrate.h:46，非本模块域）。SCI FROZEN
  禁改；语义权威=本文件 §4.1；澄清归 SCI 修订流程（不在本任务）。
- **行号锚漂移（DISP-P2REJ-003，非语义缺陷）**: SCI §5:36/:104
  引用 rejection.h:76-154、rejection.cpp:1051-1592/1859；
  REJECTION_ALGORITHMS.md:47 引用 rejection.cpp:1,1051-1592——实测
  2076/329 行，kernel 段 :1235-1680、入口 :1684-1858。FROZEN/DERIVED
  零改动；行号权威=本文件 §3 实测（与 DISP-P2INT-002 同类）。
- **minmax tie-break 未显式冻结（DISP-P2REJ-004，合同级限制）**:
  :1665-1668 比较器仅按 value（std::sort 非稳定）——同输入同编译器
  确定；等值样本 permutation 不变性未承诺（G6 覆盖 method 0..6 不含
  MINMAX）。整改候选=P2-REJ-IMPL 显式 index tie-break（需评估冻结
  语义）+ P2-REJ-TEST 等值门。
- SCI §2 符号表引用行号 rejection.h:76/:83-86 → 实测 reason :71-77/
  status :79-89（并入 DISP-P2REJ-003 锚漂移）。
- SCI §13 公开 API 名 `p2_reject`（compat）与生产入口
  `p2_reject_stack_ex` 并存——API 面=API-P2-REJ-001（PUBLIC_API.md）
  冻结两符号；compat 仅测试/旧调用（h:299 冻结注释"生产 Stage2
  不再调用"）。

## 8 单位与 dtype 登记（唯一权威=DATA_SEMANTICS §22）

- kernel 工作域全浮点 IEEE f64（gather f32 源→f64 提升，
  :1164-1179）；无 long double/复数。values: ADU；weights: 1/ADU²；
  support: 无量纲 [0,1]（仅作资格门，不进统计）；frame_ids: u64；
  reasons: u8；计数/iterations: u32；status: int。
- MINMAX 判定直接用原始域值（:1812-1814 分派 stack->values）；PERCENTILE
  scale=原始域 |median|；σ 族/ESD/RCR 在工作域（normalization 后）
  ——单位在 MEDIAN_SCALE 下无量纲化（work/max(|median|,1e-12)），
  DATA §22.4 登记。
- 整数登记量（status/reasons/计数）bitwise 确定；浮点仅域内四则
  与 sqrt/t 二分（80 轮定轮数，无自适应收敛 → 跨平台同输入 bitwise
  同 tcrit 路径；libm 差异域=lgamma/exp/log/sin，oracle 门以 rtol
  承接，§11.4 F7）。

## 9 边界与退化（SCI §8 逐条实现现状）

| 条件 | 行为 | 实现锚 |
|---|---|---|
| count==0 | MIN_SAMPLES（rc=0；**非** NO_CANDIDATES，DISP-P2REJ-002） | :1706 |
| n≤underdetermined_n(2) ∨ n<minimum_n | UNDERDETERMINED 全接受（recall=0 显式） | :1739-1747 |
| 全拒且 n≤4 | 容错 fallback → UNDERDETERMINED 全接受（阈值冻结不变） | :1842-1849 |
| 全拒且 n>4 | ALL_REJECTED | :1851 |
| values 非 finite | INVALID_INPUT（compat: 覆盖于 :1971-1972） | :1709-1718 |
| method=AUTO/出界进 kernel | INVALID_METHOD（reasons=UNDERDETERMINED、accepted=count） | :1688-1701 |
| PERCENTILE×norm≠MEDIAN_CENTER / RCR×norm≠NONE | INVALID_CONFIGURATION | :1722-1736 |
| accept 集 nc<2（σ 族）/nc<3（ESD） | break（不再拒） | :1258/:1502 |
| s≤1e-12（尺度退化） | break（不除零） | :1263/:1311/:1367/:1508/:1629 |
| linear_fit N<4 | break | :1416 |
| minmax 删后 <min_kept | 全栈 UNDERDETERMINED, iterations=0 | :1657-1664 |
| winsor/median_sigma 保底 (nc−r)≤4 | 本轮剩余冻结接受 | :1332/:1633 |
| large_scale 参数非法/disabled | rc=1 / noop rc=0 | :2054-2061 |
| radius=0 | mask 不变（仅 pixel 级拒绝） | :1986 |
| 质量指针 | 现状 control 级数据模型，stage2 传 nullptr 并记录（h:236-237） | gather :1199-1203 |
| AUTO 在 planning 解析 | kernel 永不接收 AUTO（h:285 注释） | :1688-1701 |

## 10 已冻结禁改清单（本层不可接受变化）

1. 十方法枚举 name/value/顺序 + AUTO=10（rejection.h:45-57）。
2. reason 4 值/status 8 值（rejection.h:73-90）与"reason/status
   分离"（SCI §7 状态分离不变量）。
3. 阈值表（4.0/3.0/8；5.0/3.5/8；0.05/10；0.2/0.1；1/1/4；
   minimum_n 注册表）——rejection.cpp:1-11/:1048-1069/:921-935；
   禁止"效果好"重定义（SCI §9a）。
4. AUTO 路由 <6/≤15/>15 与 nominal n 一次解析（禁止 per-pixel
   n_eff 重选，SCI §7/§10）。
5. normalization 默认 MEDIAN_CENTER + floor 1e-12；PERCENTILE/RCR
   的 normalization 绑定（:1722-1736）。
6. RCR technique=0（SS_MEDIAN_DL）唯一支持 + 3-pass 链序
   （Median+DoubleLine → Median+68th → Mean+StdDev）。
7. ESD 单 sqrt（RJ-005）+ frame_id tie-break + Rosner 回看准则。
8. UNDERDETERMINED=全接受（recall=0 显式，不做伪剔除）；reason
   判向=阈值侧（禁原始值正负号）。
9. N≤4 全拒容错 fallback（:1835-1841 注释/:1842-1849 逻辑；仅容错
   路径，阈值冻结不变）。
10. eligibility 单路径（strided gather 与连续版同一 policy core；
    source_indices 权威映射，PHASE2_IVAR_WIRING）。
11. large_scale 只增不减 + compact（<8 px）不生长 + 默认关闭。
12. policy/reducer 分离镜像: 本层不引入权重策略（RCR 官方加权
    语义除外）；weights 数组外置。

## 11 P2-REJ-DOC 冻结附录（2026-09-09，SRC-P2-REJ-001 源码实测）

### 11.1 逐符号锚

见 §3 表（锚=2026-09-09 grep/read 实测；禁止手抄他版行号）。

### 11.2 返回码/并发合同

- rc 语义: §4.3（ex rc=1 仅 null 三态；科学语义全在 status；
  plan_resolve/eligibility/gather/large_scale rc=1=参数非法）。
- 并发合同: reentrant=yes（无全局/静态可变状态；kRcrSS* 为只读
  const 表）；threadsafe=no（无内部锁，并发由调用方像素划分）；
  internal_parallel=none（kernel 单栈纯函数；OMP 在调用方 Stage2
  :1279-1317 / ACR :212-244，per-thread scratch，thread id 定序
  归并）；取消点=无（ThreadLease 接线归 P2-REJ-IMPL，与
  DISP-COV-005/DISP-P2INT 同构）。
- 内存合同: kernel n≤64 固定 scratch（:944-986，无每像素堆分配），
  >64 堆 fallback；RCR 用 std::vector（段内局部）；无整帧副本
  （决策 per-pixel）；生产 buffer 化仅在调用方（stage2 :847-861/
  :1468-1503）。

### 11.3 现状缺陷/限制清单（DISP-P2REJ-001..004，登记不改码）

- **DISP-P2REJ-001**（文档级）: rejection.h:118 percentile
  low_fraction 注释 "默认 0.1" vs 实现/SCI 权威 0.2（§7）；整改=
  注释对齐（P2-REJ-IMPL）。
- **DISP-P2REJ-002**（文档级）: SCI §8 "无候选→NO_CANDIDATES" vs
  实现 MIN_SAMPLES（八态无 NO_CANDIDATES；NO_CANDIDATES 属积分域
  integrate.h:46）（§7）；SCI FROZEN 禁改；语义权威=本文件 §4.1。
- **DISP-P2REJ-003**（锚漂移，非语义）: SCI §5/§2、
  REJECTION_ALGORITHMS.md:47 行号引用超出/偏离实测（2076/329 行）
  （§7）；行号权威=本文件 §3。
- **DISP-P2REJ-004**（合同级限制）: minmax 比较器仅 value，等值
  tie-break 未显式冻结（§7）；整改候选=P2-REJ-IMPL index
  tie-break + P2-REJ-TEST 等值门。
- 附注（冻结事实，非缺陷）: N≤4 全拒 fallback（:1835-1850 冻结
  注释）、winsor/median_sigma 保底 (nc−r)≤4、ESD tie epsilon
  1e-15+frame_id、winsor 内层 64 轮/收敛 5e-4·σ——均为冻结名义
  行为，禁止漂移。

### 11.4 TEST-P2-REJ-DESIGN-001 冻结测试设计（可执行 TEST-P2-REJ-001 由 P2-REJ-TEST 落地）

锚归属声明: 本节及 §6 全部测试 `:N` 行号锚 =
`lib/phase2/tests/synthetic_gate.cpp`（2026-09-09 实测；与其余
小节 rejection.cpp/h 锚不同文件）。

- **F1 ESD NIST 门**（SCI §11）: 54 值 NIST Rosner 集恰拒
  {5.34, 5.42, 6.01}（V15EsdSingleSqrtExactRosnerSet :4191 拒集
  逐位=索引 {51,52,53}；G6EsdNistRosner54 :2779 n_reject==3；
  "双 sqrt bug 会误拒"负向对照）；masking 对检出
  （G6EsdMaskingCase :2806 ≥2 拒）。容差=拒集精确（bitwise）。
- **F2 AUTO 路由/profile 门**（SCI §11 阈值不变量）:
  V15AutoPlanResolvesByNominal :4213（n=2/5→PERCENTILE、6/15→
  WINSORIZED、16/20→LINEAR_FIT；非法 profile rc≠0）；
  V16ProfileGroupVsAdaptive :4646（wbpp_current group 一次 vs
  astrocs_adaptive tile depth）。容差=方法枚举精确。
- **F3 small-N/状态穷尽门**（SCI §7/§8）:
  V15SatelliteN2Underdetermined :4241（n=2 全 UNDERDETERMINED、
  accepted_count=2）；R2MinSamples :2658（status==1）；
  V17InvalidMethodStatus :4763；V16InvalidConfigurationCombos
  :4543；V16RejectionNormalizationValidation :4559；
  V15NoneDoesNotReacceptNaN :4138；V15ValidFalseStaysRejected
  :4158。八态互斥显式断言；容差=枚举/计数精确。
- **F4 注入门**（SCI §11 卫星线注入）:
  V15SatelliteTrail20Frames :4259（AUTO 20 帧→LINEAR_FIT、
  reasons[7]=REJECTED_HIGH、accepted≥15、clean false-reject ≤4）；
  large_scale 结构门: V17LargeScaleGrowsTrailNotCosmic :4798
  （40px trail 扩 ±2 非 ±3；2×2 cosmic 不生长；低/高独立）、
  V17LargeScaleSparseNotGrown :4846、V17LargeScaleDisabledNoop
  :4829、V17LargeScaleInvalidParams :4864（min_structure_pixels=0
  →rc=1）。容差=mask/计数精确。
- **F5 置换不变性门**（SCI §11 确定性门）:
  G6PermutationInvariance :2863（method 0..6，fid=1000+i 稳定帧
  identity，σ_low=−4/σ_high=3/min_samples=3，shuffle 决策一致）；
  V15ExPermutationInvarianceTyped :4443（ex typed 面）。容差=
  decision bitwise（同 fid 语义）。
- **F6 typed params/harness 逐位门**: R1SigmaClippingFindsOutliers
  :2639；LinearFitFindsOutlier :2674（rng42 固定向量与未修改 Siril
  1.4.3 harness 逐位核对=拒 {30,43..49} 共 8 :2699-2702）；RcrFindsOutlier
  :2704；G4SequentialRcrMask :2725；G6WinsorizedDiffersFromSigma
  :2831（MAD=0 vs std 尺度分离）；V15LowHighThresholdSemantics
  :4177；V15TypedPercentileParams :4284；V15TypedMinmaxParams
  :4298；V16MinMaxFixedCountExact :4316；
  V15RejectionTypedParseAndDefaultAuto :4369（typed 参数透传+
  默认 AUTO）。容差=逐位/计数精确。
- **F7 Python 参考 Oracle**（SCI §11）: Astropy
  sigma_clip(median+mad_std) 对 robust_mad_clip 复算 reject set；
  SciPy stats 对 ESD/RCR 决策复算（新增设计面，落地归 P2-REJ-TEST）。
  容差=rtol 1e-12（Python 参考域）；decision 集合精确一致。
- **F8 gather/eligibility 门**: V16GatherStridedFp32Fp64 :4592
  （f32/f64 strided、source_indices 恒等映射）；Phase2Eligibility
  V15FilterAllPolicies :4337（四诊断计数）。容差=逐元素精确。
- 冻结容差汇总: F1-F6/F8 = bitwise/枚举/计数精确（无 epsilon 门）；
  F7 = rtol 1e-12（Python 参考域）；large_scale=mask 精确。本层
  禁引入其他 epsilon（ESD tie 1e-15、RCR isEqual rel 1e-8、winsor
  收敛 5e-4·σ 为实现内部冻结常数，非门容差）。
- 登记面: 本节容差同步登记于 docs/modules/registry/astrocs.phase2.reject.md §独立 synthetic 验证节（TEST-P2-REJ-DESIGN-001 设计冻结 VERIFIED，承载 TEST-P2-REJ-001 登记锚；P2-INT-DOC registry 承载先例）。

### 11.5 SCI 层状态声明（本任务零 SCI 改动）

- 排异语义权威已有 FROZEN SCI: SCI-REJ-001（docs/science/
  REJECTION.md，T107 2026-08-23 冻结，集合 SCI-REJ-001..008，
  legacy RJ-001..008）。**不因本任务改动**（共享 SCI 引用不改动；
  P1-WCS/P2-COV/P2-INT/P2-HIPS 先例）。
- matrix P2-REJ 行 science_id=SCI-P2-REJ-001（descriptor 占位词汇，
  module_adapters.cpp:649）的语义映射由本节声明——
  **SCI-P2-REJ-001 ⇒ SCI-REJ-001**（docs/science/REJECTION.md，
  矩阵 science_doc=docs/science/REJECTION.md，MOD-astrocs-phase2-
  reject 行，2026-09-09 P2-REJ-DOC 冻结）。descriptor 占位
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
  ⇒ 本文档 §5: ALG-REJ-001 None（F4 前置 none :1235-1237）；
  ALG-REJ-002 Sigma/robust_mad（F4）；ALG-REJ-003 Winsorized
  （F5）；ALG-REJ-004 AveragedSigma（F6）；ALG-REJ-005 LinearFit
  （F7）；ALG-REJ-006 ESD（F8）；ALG-REJ-007 RCR（F9）；
  ALG-REJ-008 Percentile/Minmax + large_scale + wbpp 路由/状态
  分层（F1/F10/F12/F13/F14）。共享 ID 不抢注、不重复登记
  （INDEX.yaml ALG-REJ-001 path 维持 REJECTION_ALGORITHMS.md，
  downstream 追加 ALG-P2-REJ-001 指向语义承接）。
- legacy `RJ-001..008` = SCI-REJ-001..008 别名（SCI 头注）；冻结
  整改记录 RJ-002（NONE NaN）、RJ-004/RJ-005（ESD 双→单 sqrt，
  :1507 注释）由本文档 §5 冻结承接。

## 13 追溯

- 实现: lib/phase2/src/rejection.cpp（2076 行）+
  lib/phase2/include/astro/phase2/rejection.h（329 行）。
- 合同: DATA-P2-REJ（DATA_SEMANTICS §22）/ API-P2-REJ-001
  （PUBLIC_API.md）/ TEST-P2-REJ-001（设计冻结 VERIFIED=registry
  承载页 §独立验证节；可执行落地归 P2-REJ-TEST + EVIDENCE）。
- 交叉: docs/modules/phase2_rej.md + lib/phase2_rej/ 三件套
  （README/module.yaml/memory.md，按 lib/phase2_int/ 先例新建；
  lib/phase2/ 三件套已被 P2-COV 占用）；registry
  astrocs.phase2.reject.md；REJECTION_ALGORITHMS.md（旧 L2，ID
  让位/承接关系见 §12）。
- 消费者: stage2.cpp（§6；DATA_SEMANTICS §20 编排域）/
  acr_kernels.cpp（ACR 域）/ tests/unit/p2_rejection_test.cpp
  （P2-005 语义 id/解析面）/ tests/backend/test_p2004_reject_
  integrate.py（P2-004 生产 Oracle）/ module_adapters.cpp:638-655
  descriptor 占位。
- SCI: docs/science/REJECTION.md（SCI-REJ-001..008，FROZEN T107，
  零改动）。

## 14 合同落位（P2-REJ-DOC 同批产物）

- DATA-P2-REJ = docs/contracts/DATA_SEMANTICS.md §22：输入
  eligibility/gather/kernel 三层 + P2RejectionDecision 输出 +
  八态状态机 + 单位/确定性唯一权威；与 §21 DATA-P2-INT 的消费
  边界 = accepted mask → P2PixelStack.accepted。
- API-P2-REJ-001 = docs/contracts/PUBLIC_API.md 末节：
  planning/eligibility/gather/kernel/large_scale 导出符号冻结；
  compat p2_reject_stack 冻结两符号；与 API-P2-001 编排面并存。
- MOD = docs/modules/phase2_rej.md（模块页）+ lib/phase2_rej/
  三件套（README/module.yaml CONTRACT_READY entrypoint=MISSING/
  memory.md，按 lib/phase2_int/ 先例）+ registry
  docs/modules/registry/astrocs.phase2.reject.md（手写合同页
  重写；TEST 登记面承载）。
- 一致性声明: 本文件（ALG）与上述同批产物冲突时以本文件为
  算法/锚权威，DATA/API 以各自文件为单位/dtype/消费面权威；
  SCI 权威永远在 docs/science/（禁止反向）。
- 缺陷联动: DISP-P2REJ-001..004 同时登记于 registry 页与
  module.yaml known_defects；本文件 §7 为权威表述。
- TRACEABILITY_MATRIX.json MOD-astrocs-phase2-reject 行由
  P2-REJ-DOC 更新: science_id=SCI-REJ-001（映射 §11.5）/
  algorithm_id=ALG-P2-REJ-001（本文件）/data_id=DATA-P2-REJ/
  api_id=API-P2-REJ-001/src_id=SRC-P2-REJ-001（src_path=lib/
  phase2/include/astro/phase2/rejection.h::p2_reject_plan_resolve,
  p2_reject_stack_ex,p2_collect_candidate_stack,
  p2_eligibility_filter,p2_large_scale_apply,
  p2_rejection_semantic_id）/test_id=TEST-P2-REJ-001
  （test_path=docs/modules/registry/astrocs.phase2.reject.md::
  TEST-P2-REJ-001，设计冻结 VERIFIED + 可执行 MISSING 双
  statement）——由主控写入，本节仅声明预期终态。
