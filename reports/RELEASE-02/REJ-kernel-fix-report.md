# REJ-kernel 修复报告 — 逐 stack 几何 n API + n=2 先验 σ 极值排异

- 控制包：RELEASE-02 / 分片 **REJ-kernel**（排异 kernel 修复者）
- 文件面（仅改这两处）：
  - 'lib/algorithms/coverage/include/astro/phase2/rejection.h'
  - 'lib/algorithms/coverage/src/rejection.cpp'
- 未改：'lib/infrastructure/**'（scheduler/CLI 由其他分片负责）、docs/tests/ci。
- 日期：2026-09-18
- 证据/脚本：'run/RELEASE-02/REJ-kernel/'（'verify_rej_kernel.py' + 'out/verify_rej_kernel.json' + 'logs/'）
- 构建约束：本分片**未跑 ninja/cmake/ctest**；验证 = 'g++ -fsyntax-only'（rejection.cpp + 全部 in-repo 消费者，零 warning）+ 纯 Python 语义复刻对 scipy oracle。
- 关联：'reports/RELEASE-02/FIX-REJ-report.md'、'工程控制/RELEASE-02/change-claims/FIX-REJ-001.md'、'ASTROCS_DESIGN.md' §4.5、'docs/science/REJECTION.md'。

---

## 0 一句话结论

kernel 侧已补齐两件事：**（1）逐 stack 传入几何 n 的解析/执行入口**（含按 n 缓存友好的纯函数解析）；
**（2）astrocs.extreme_value_clip_prior_sigma.v1 的 n=2 档排异核**（已知先验 σ 的极值检验，
k=Φ⁻¹(1−α/(2N))，α=0.05，先验 σ/中心由调用方提供，kernel 不自己算邻域）。
AUTO 仍在 planning 层解析、永不进 kernel；MINMAX 保持"仅显式可选 + 恒 WARN"（kernel 侧新增
p2_rejection_applicability 供 CLI/planning 出分级提示，不改执行）。**冻结路由与冻结阈值零改动**。
发现 1 处与冻结科学文档的实质冲突（§7「n≤2 恒 UNDERDETERMINED」），**未擅自改文档/判据**，见 §6 上呈。

---

## 1 API 变更（rejection.h）

### 1.1 新方法 + 语义 ID + profile

| 名称 | 值 | 说明 |
|---|---|---|
| P2_REJECT_EXTREME_VALUE_PRIOR_SIGMA | 11 | 追加在 AUTO=10 之后，**不改变既有枚举数值**（0..10 原样） |
| P2_SEMANTIC_EXTREME_VALUE_PRIOR_SIGMA | "astrocs.extreme_value_clip_prior_sigma.v1" | canonical 语义 ID |
| P2_PROFILE_ASTROCS_ADAPTIVE_PIXEL | "astrocs_adaptive_pixel" | FIX-REJ §3 的 AstroCS 自有「按逐输出像素几何 n」内置映射；**独立命名**，不改 wbpp_2_9_1/astrocs_adaptive 冻结 AUTO 路由 |
| P2_PROFILE_WBPP_2_9_1 / _CURRENT / ASTROCS_ADAPTIVE | 字符串常量 | 便于调用方避免散落字面量 |

### 1.2 方法参数（新增 typed params，不与既有方法共享字段）

P2ExtremeValuePriorSigmaParams（追加进 P2RejectionPlan.extreme_prior）：

| 字段 | 默认（resolve 填） | 语义 |
|---|---|---|
| alpha | 0.05 | 显著性水平（冻结） |
| prior_sigma | 0.0（=未提供） | 先验噪声尺度（标量回退；>0 且 finite 才可用） |
| prior_sky | NaN（=未提供） | 先验中心（标量回退） |
| center_mode | 1 | 0=强制外部 prior_sky（缺则 fail-closed）；1=缺失时允许用候选栈中位数 |

### 1.3 plan / stack 新增字段（均为**尾部追加**，旧调用方 {} 零初始化兼容）

- P2RejectionPlan.nominal_n（uint32_t）：本次解析使用的**几何 nominal n**（路由依据；
  同时是 n=2 方法的 Bonferroni N）。p2_reject_plan_resolve* 由 req->nominal_contributors 回填。
- P2CandidateStack.prior_sigma / prior_sky（const double*，可空）：**逐样本**先验 σ/中心
  （与 values 同序）。仅 P2_REJECT_EXTREME_VALUE_PRIOR_SIGMA 消费；nullptr → 回退 plan 标量。

### 1.4 新函数

    // 逐 stack 解析（planning 层；nominal_n = 该 stack 几何 n，覆盖 req->nominal_contributors）
    P2_API int p2_reject_plan_resolve_n(std::uint32_t nominal_n,
                                        const P2RejectionPlanRequest* req,
                                        P2RejectionPlan* plan,
                                        char* err, std::size_t err_cap);

    // 逐 stack 解析 + 执行（req->nominal_contributors 必须 = 该 stack 几何 n；
    // req->request 允许 AUTO；resolved_plan 可空）
    P2_API int p2_reject_stack_resolve_ex(const P2CandidateStack* stack,
                                          const P2RejectionPlanRequest* req,
                                          P2RejectionDecision* out,
                                          P2RejectionPlan* resolved_plan,
                                          char* err, std::size_t err_cap);

    // FIX-REJ §4.2 方法适用域（WARN 级 advisory，不改判据/不阻断；仅 planning/CLI 用）
    P2_API int p2_rejection_applicability(int method, std::uint32_t nominal_n,
                                          char* warn_code, std::size_t warn_cap);

**确定性保证**：p2_reject_plan_resolve* 是 (profile, request, nominal_contributors,
underdetermined_n) 的纯函数（无状态、无随机、无缓存），同 n 方法选择恒等 ⇒ 调用方可按 n
缓存 plan，1 worker 与 N worker 结果一致（docs/science/REJECTION.md §7 阈值不变量）。

### 1.5 请求/profile 语义扩展

- P2RejectionPlanRequest.profile 新增接受 "astrocs_adaptive_pixel"；underdetermined_n==0
  时默认值：该 profile = **1**（n=2 由 extreme_prior 承担；n≤1 仍 UNDERDETERMINED），
  其余 profile = 2（冻结）。显式传值优先。
- req->request 合法域扩为 0..11（含新方法）；AUTO=10 仍在 planning 层消解。

---

## 2 n=2 方法实现（astrocs.extreme_value_clip_prior_sigma.v1）

### 2.1 判据（NIST/SEMATECH §1.3.5.17.1 Grubbs / 已知方差单离群变体）

    z_i = (v_i − center_i) / sigma_i
    k   = Φ⁻¹(1 − α/(2N))，α=0.05，N = plan.nominal_n（几何 nominal n；0 时退化为候选数 n）
    z_i > +k → REJECTED_HIGH；z_i < −k → REJECTED_LOW；否则 ACCEPTED
    单趟、无迭代、无 Siril N-r<=4 最小保留闸

- Φ⁻¹ 用 Acklam 有理近似 + 一步 Halley 精修；上尾残差用 ½·erfc(x/√2) 直接算避免
  p→1 灾难性相消。对 scipy ndtri 13 点（含 1e-12 尾）实测 **worst |Δ| = 8.9e-16**（§4 证据）。
- 先验来源优先级：逐样本数组 > plan 标量 > 候选栈中位数（仅 center_mode!=0）。
- **fail-closed**：prior_sigma 缺失/非有限/≤0 ⇒ UNDERDETERMINED 全接受（禁止用栈内尺度冒名顶替）；
  center_mode==0 且无外部 prior_sky ⇒ 同样 fail-closed。
- normalization 必须 NONE（方法在原始 calibrated 值域直接比较绝对 prior_sky）；
  planning 层解析自动设 NONE，手工构造 plan 走 fail-closed 门（INVALID_CONFIGURATION）。
- minimum_n = 2（n=1 单帧无排异，docs/science/REJECTION.md §1 非目标）。

### 2.2 n=2 的关键设计点（务必接线正确）

n=2 时若只用 prior_sigma 而让 kernel 用**候选栈中位数**作中心：两样本中位数落在两者之间，
单离群会使两侧 |z| 同时超阈 ⇒ 全拒 ⇒ 触发冻结的 n≤4 全拒→UNDERDETERMINED 全接受容错，
**排异被反转**。因此 **n=2 档必须由调用方提供外部 prior_sky**（方案 A 的 31×31 邻域中位数，
与 prior_sigma 同源）。kernel 支持逐样本数组，正是为该帧该 tile 的逐样本先验留接口。
（证据：verify_rej_kernel.py 的 B_n2_stack_median_trap 显式复现该退化。）

### 2.3 实测（Python 语义复刻；见 §4）

| 用例 | 结果 |
|---|---|
| n=2，外部 prior_sky=100/prior_sigma=1，值 [100.02, 150]（+50σ 卫星线） | 样本 0 ACCEPTED、样本 1 **REJECTED_HIGH**，status=OK |
| n=2 无污染 [100.02, 99.98] | 双 ACCEPTED（零误剔） |
| n=2 无 prior_sigma | 双 UNDERDETERMINED（fail-closed） |
| n=2 只有 prior_sigma（中心=栈中位数） | 双 UNDERDETERMINED（对称陷阱，已登记） |
| n=1 | UNDERDETERMINED（单帧无排异） |
| n=3 显式同法，[100,100.1,130] | 130 **REJECTED_HIGH** |

---

## 3 与 FIX-REJ 映射表的对应

### 3.1 内置映射（新 profile astrocs_adaptive_pixel，astrocs_n_map_method）

| n | 方法 | 与 FIX-REJ §3 | 与 WBPP 对照 |
|---|---|---|---|
| 0–1 | none + UNDERDETERMINED | 一致（n=1 无对照量） | — |
| **2** | **extreme_value_clip_prior_sigma** | 一致（新增档） | WBPP 选 percentile；FIX-REJ 实测 percentile 在 Δ>S 全拒→容错全接受，故换先验 σ 极值 |
| 3–7 | percentile（0.2/0.1 继承） | 一致（含 6≤n≤7 用 percentile，消解 WBPP auto∩validator 自相矛盾） | WBPP auto n<6 选 percentile |
| 8–15 | winsorized_sigma（4.0/3.0/8 继承） | 一致 | WBPP auto ∩ validator |
| ≥16 | linear_fit（5.0/3.5/8 继承） | 一致 | WBPP auto n>15 |

**关键取舍**：本映射放在**新 profile**，wbpp_2_9_1/astrocs_adaptive 的冻结 AUTO 路由**逐位未变**
（n<6→percentile、6..15→winsorized、>15→linear_fit），因此冻结回归（synthetic_gate
M4A01/V15AutoPlanResolvesByNominal/V16ProfileGroupVsAdaptive、p2_rejection_test）不受影响。
把 FIX-REJ §3 设为**生产默认**需前台裁决并同步改冻结路由测试（tests 不在本分片文件面）。

### 3.2 MINMAX 处置（与 FIX-REJ §4.2 一致）

- **AUTO 永不选 MINMAX**：astrocs_n_map_method 与 WBPP 冻结表都不产出 P2_REJECT_MINMAX。
- **显式指定照执行、不静默改算法**：kernel 不因 n 不适用而替换方法（仅 underdetermined 语义）。
- **恒 WARN**：p2_rejection_applicability(P2_REJECT_MINMAX, n, ...) 恒返回 W_MINMAX
  （依据 WBPP BPP-FrameGroup.js:1239 "Min/Max rejection should not be used for production work"）。
  kernel 只提供判定码，WARN+确认/-y/-force 的交互由 CLI/scheduler 实现（不在本文件面）。
- 其余 §4.2 行的稳定码：W_NONE / W_PCT_GT8 / W_PCT_N4_FALLBACK / W_SIGMA_RANGE /
  W_WINS_LT8 / W_NR_LE4 / W_LF_LT20 / W_AVG_RANGE / W_ESD_LT25 / W_RCR_LT15 /
  W_UNKNOWN_METHOD（advisory，不改执行）。

### 3.3 AUTO 不进 kernel

- p2_reject_stack_ex 用 method_is_explicit() 判合法：AUTO(10) ⇒ INVALID_METHOD（原语义保留）。
- p2_reject_stack_resolve_ex 先解析再执行，kernel 只见显式方法。
- p2_reject_classify 同步改为 method_is_explicit（AUTO 仍 INVALID_METHOD）。

### 3.4 继承阈值守卫

rej_plan_inherited 增：仅当 method==EXTREME_VALUE_PRIOR_SIGMA 时校验 extreme_prior.alpha==0.05；
prior_sigma/prior_sky 是调用方数据、不属继承阈值，不参与校验。既有 4.0/3.0/8、5.0/3.5/8、
0.2/0.1、ESD 0.05/10、minmax 1/1/4、large_scale 8/2/2 **一字未改**。

---

## 4 验证与证据（不构建，遵守硬约束）

| 证据 | 路径 | 结果 |
|---|---|---|
| rejection.cpp 语法+warning 门 | run/RELEASE-02/REJ-kernel/logs/syntax_rejection.log | g++ -fsyntax-only -O3 -DNDEBUG -std=gnu++17 -Wall -Wextra -Wpedantic -Wconversion **rc=0，0 warning** |
| 全 in-repo 消费者语法门 | 同上命令逐一跑 | module_adapters.cpp（scheduler）、stage2_common.cpp、integrate.cpp、acr_kernels.cpp、tools/stage2.cpp、tools/rejection_cli.cpp、synthetic_gate.cpp、p2_rejection_test.cpp、p2_rej_v6_test.cpp、rejection_nonfinite_weights_test.cpp、phase2_integrate.cpp 全部 rc=0 |
| 语义复刻 + scipy oracle | run/RELEASE-02/REJ-kernel/verify_rej_kernel.py / out/verify_rej_kernel.json | **11/11 PASS**：Φ⁻¹ vs scipy.special.ndtri worst|Δ|=8.9e-16；n=2 正/负例；fail-closed；n=1；n=3；映射表；冻结路由不变；确定性 |

> 说明：本分片硬约束"最多 g++ -fsyntax-only"，故**未执行真实 C++ kernel**；Python 复刻逐行对齐
> rejection.cpp 新增逻辑，oracle 用 scipy（BSD-3-Clause，独立 FP64）。前台统一构建后应补跑
> §5 列出的 C++/端到端命令。

---

## 5 前台统一构建后应补跑的命令

    export TMPDIR=/dev/shm/astrocs_rej2
    cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
    ninja -C build astrocs astrocs-stage2 phase2_synthetic_gate \
          phase2_rej_nonfinite_weights p2_rejection_test rejection_cli
    ctest --test-dir build --output-on-failure \
          -R 'p2_rejection|phase2_synthetic_gate|phase2_rej_nonfinite_weights'
    python3 run/RELEASE-02/REJ-kernel/verify_rej_kernel.py   # 应 11/11 PASS
    # 建议新增 C++ 测试（tests 不在本分片文件面，交前台）：
    #   n=2 外部 prior 正例（+50σ 必剔）/ 负例（零误剔）；无 prior → UNDERDETERMINED；
    #   n=2 只有 prior_sigma 的对称陷阱（登记）；astrocs_adaptive_pixel n=1..20 路由；
    #   冻结 wbpp 路由不变；p2_rejection_applicability 码表；1 worker vs N worker 一致。

---

## 6 上呈：与冻结科学文档的实质冲突（未擅自改）

**冲突点**：docs/science/REJECTION.md §7「UNDERDETERMINED 单调性：n ≤2 恒 UNDERDETERMINED，
不做剔除」、§8 表「n ≤2 → 全接受」；而 ASTROCS_DESIGN.md §4.5（最高设计）+ FIX-REJ §3 要求
n=2 档具备排异能力（先验 σ 极值）。

**本分片处置（最小改动、不越权）**：

1. kernel **提供** n=2 能力（新方法 + 新 profile 的 n=2 路由）；
2. **不修改**冻结 profile 的 underdetermined_n=2 默认、**不修改** p2_reject_stack_ex 的
   n<=underdetermined_n 门语义、**不修改**文档；
3. n=2 实际生效的唯一开关是**调用方**把 underdetermined_n 设为 1（新 profile 默认 1；显式传值优先）。
   即：是否让 n=2 进入排异 = 产品/路由决策，由前台/负责人裁决，而不是 kernel 静默改判据。

**请前台裁决**：
- (a) 是否批准把 docs/science/REJECTION.md §7/§8 的「n≤2 恒 UNDERDETERMINED」订正为
  「n=2 在有先验 σ/中心时走 extreme_value_clip_prior_sigma；无先验时仍 UNDERDETERMINED」；
- (b) 生产 mosaic 默认是否切到 astrocs_adaptive_pixel（会改 6≤n≤7 路由，需同步改冻结路由测试）；
- (c) 若暂不切默认，则 scheduler 需显式使用新 profile（或 method_map 显式指定 n=2 方法）。

---

## 7 给 scheduler 的接线说明（供前台后续统一接）

**改动面**：lib/infrastructure/scheduler/src/module_adapters.cpp p2_op_reject
（约 4637–4650 group-level 解析处 + 4747–4759 执行处 + 4824–4849 provenance）。

1. **按输出像素几何 n 解析（替换 group-level 一次解析）**
   - n 取该输出像素的**几何可贡献帧数**（coverage 覆盖图；depth=refs.size() 是 tile 级
     几何覆盖数，逐像素覆盖数由 coverage 产物给出——具体口径由前台定）。**不得**用
     frames.size()，**不得**用资格后 eligible_count（n_eff）。
   - 按 n 缓存 plan（最多 n_max 档，建议 std::map<uint32_t, P2RejectionPlan>）：

         P2RejectionPlanRequest rreq{};
         rreq.request = P2_REJECT_AUTO;              // 或显式方法 / 用户 method_map
         rreq.profile = "astrocs_adaptive_pixel";    // 启用 FIX-REJ §3 内置映射
         rreq.underdetermined_n = 1;                 // n=2 档生效（profile 默认已是 1）
         P2RejectionPlan plan;
         char perr[256] = {0};
         p2_reject_plan_resolve_n(geom_n, &rreq, &plan, perr, sizeof(perr));

     （等价单调用：p2_reject_stack_resolve_ex(&stack, &rreq, &dec, &plan, err, cap)。）
2. **n=2 档必须提供先验（方案 A）**
   - 逐样本 prior_sigma[i]（=1.4826×该帧该 tile 31×31 邻域 MAD）与 prior_sky[i]
     （=同邻域中位数）；**按 eligibility 紧凑序填**（用 src_idx 的 eligible→original 映射对齐）。
   - 填 stack.prior_sigma = ...; stack.prior_sky = ...;（P2CandidateStack 新增字段）。
     若只给 prior_sigma 而中心回退到栈中位数，n=2 单离群会全拒→容错全接受（能力失效）。
   - 无法提供先验时：method 仍为 extreme，kernel 返回 UNDERDETERMINED；provenance 记
     fallback="prior_sigma_unavailable"（由 scheduler 写）。
3. **门与执行**：把 4747 行的 eligible_count > plan.underdetermined_n && >= plan.minimum_n
   改为**逐像素 plan** 的同式判定；p2_reject_stack_ex(&stack, &plan, &dec) 不变。
4. **provenance**：plan.method（新方法 semantic id 已由 p2_rejection_semantic_id 返回）、
   plan.nominal_n（几何 n）、underdetermined_n、以及 status==UNDERDETERMINED 且
   method==extreme 时的 fallback。
5. **配置消费**：reject.method / method_map / method_expr 由 scheduler 解析为显式 method
   或有序规则（FIX-REJ §5），逐 n 求值后直接构造 plan（request=显式方法）；auto 用新 profile。
   WARN 分级调用 p2_rejection_applicability(method, n, code, sizeof code)。
6. **兼容性**：不改任何既有函数签名/枚举数值/结构前序字段；旧调用方 {} 零初始化即兼容。
   本分片未改 scheduler，module_adapters.cpp 的现有 group-level 路径在未接线前仍按冻结行为工作
   （n=2 仍 UNDERDETERMINED），不会产生回归。
7. **前台需同步的 out-of-face 接线点（本分片未改，登记备查）**：
   - stage2_common.cpp:227-251 method 名表未含 "extreme_value_clip_prior_sigma"（显式选它会报
     unsupported）；:258-263 profile 白名单未含 "astrocs_adaptive_pixel"；:264-265
     underdetermined_n 默认硬编码 2（**新 profile 想默认 1 需在此按 profile 分支，或用户显式配 1**）。
   - tools/rejection_cli.cpp:137-145 方法名表未含新方法（Oracle 对照脚本要用时需加）。
   - module_adapters.cpp p2_op_reject 仍 group-level（本分片未改，见 1–6 接线说明）。
   kernel 侧 p2_reject_plan_resolve / p2_reject_stack_ex 已完整支持新方法与新 profile。

---

## 8 风险与限制

| 风险 | 等级 | 说明 / 缓解 |
|---|---|---|
| n=2 未提供外部 prior_sky | **高** | 栈中位数中心导致对称全拒→容错全接受，n=2 能力失效。已 fail-closed 文档化 + 证据；scheduler 必须提供邻域先验 |
| 科学文档冲突（§6） | **高** | 已上呈，未擅自改；在裁决前新 profile 非默认，冻结行为不变 |
| FIX-REJ §3 作为生产默认会改 6≤n≤7 路由 | 中 | 本分片不切默认；切换需前台裁决 + 改冻结路由测试 + 一致性回归 |
| 逐样本 prior 数组的紧凑序对齐 | 中 | 必须用 src_idx（eligible→original）对齐，禁止用 compact index 猜 original slot（rejection.h 合同） |
| Φ⁻¹ 精度 | 低 | Acklam+Halley，vs scipy worst 8.9e-16；上尾相消已修 |
| P2RejectionPlan/P2CandidateStack 变长（ABI） | 低 | 仅尾部追加；in-repo 调用方全 {} 零初始化；已对全部消费者跑 -fsyntax-only |
| rej_plan_inherited 对 extreme 的 α 校验 | 低 | 仅 method==extreme 时生效；resolve 已填 0.05 |
| ACR 路径 | 低 | acr_kernels.cpp 仍只走 sigma；新方法未接 ACR（本分片不改），normalization 门会 fail-closed |

---

## 9 交付物索引

| 文件 | 内容 |
|---|---|
| lib/algorithms/coverage/include/astro/phase2/rejection.h | 新方法/参数/字段/3 个新 API 声明 + 文档 |
| lib/algorithms/coverage/src/rejection.cpp | Φ⁻¹、extreme 核、逐 stack 解析/执行、applicability、AUTO/MINMAX/继承门同步 |
| reports/RELEASE-02/REJ-kernel-fix-report.md | 本报告 |
| run/RELEASE-02/REJ-kernel/verify_rej_kernel.py | 语义复刻 + scipy oracle + 路由/确定性断言（11/11 PASS） |
| run/RELEASE-02/REJ-kernel/out/verify_rej_kernel.json | 实验原始输出 |
| run/RELEASE-02/REJ-kernel/logs/syntax_rejection.log | -fsyntax-only 零 warning 日志 |
| run/RELEASE-02/REJ-kernel/logs/verify_rej_kernel.log | 验证脚本运行日志 |
