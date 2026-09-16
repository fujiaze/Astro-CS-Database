# 任务：SCI-FIX-WEIGHT phase2 权重/UPM 科学订正（R-2 结论落地）

状态：NOT_STARTED　层：L1　依赖：R-2（已完成）　互斥组：S10-A（coverage/upm/rejection 实现与文档）

## 依据（负责人指令：文档必须正确，不要遵循旧制）

证据：reports/PROJECT-GOVERNANCE-01/research/R-2_phase2权重与UPM语义.md + run/PROJECT-GOVERNANCE-01/R-2/**。

## 逐条要做

| # | 订正 | 落点 |
|---|---|---|
| 1 | weight_mode=2 ≡ 逐样本 ivar 逆方差权重、无 fallback（四份权威本来就一致，「三套互斥」是误判）；订正 CONTROL_WEIGHT_SNR 分支号 2→0 与 ALG:125-127 措辞（support×snr² 是 mode 0 legacy；ivar→support 只是降级路径） | SCI/ALG 文档（零代码） |
| 2 | w_UPM 两式并存（ALG F1 的绝对 ADU⁻² 与求解器内份额式）⇒ 分别命名 + 冻结各自范围；同时登记真缺陷：geometric_reliability 在实现里只是配置常数 1.0（SCI §2:21 承诺「单 control 覆盖度」） | 文档 + 登记 |
| 3 | k_corr 强制 ≥1，退役 300–600″ 的 scale 维（生产尺 0.9586″/px 恒饱和、clamp 静默取 300 档；kcorr_lookup_test.domain_clamp 把饱和固化成合同 ⇒ 一并退役）；修 p2_upm_control_variance(0.9,…) 现 rc=0 接受 k<1 | 文档 + 实现 |
| 4 | N≤4 全拒容错：登记进 SCI 并把域写死 n=4（实测可达域恰为 4），补 n=3/4/5/6 边界测试；另立根因：percentile 的 scale=|median| 在近零天光上塌缩（n=6/8 显式 percentile 随机栈全拒 70–74%） | 文档 + 测试 |
| 5 | M4-A-02：SCI INTEGRATION:58 改为 max_{accepted}（实现已正确、零代码）；ALG §5:113/:170-171 与 DISP-P2INT-001/002 的过期登记一并订正 | 文档 |
| 6 | M4-F-01 原判据前提不成立：control_median_mc_test.cpp / kcorr_matrix_test.cpp 存在且受 git 跟踪（lib/algorithms/drizzle/healpix_drizzle/tests/），事实是 tracked-but-unbuilt（CMake/CI 命中 0、ctest -N 414 项无它们）⇒ 门改为「文件存在 ∧ 构建注册 ∧ CI 采集」三合一；SCI:105/:147 与 UPM_SOLVER:98 的现在时「已过」改 MISSING，或把 MC 收进 ctest | 门 + 文档 |
| 7 | M7-H-103：null model 返回 0.0 是 fail-open（0.0 又是 gauge 参考帧的合法值）⇒ 统一为 NaN | 实现 |
| 8 | M7-H-101：iterations/objective 已持久化但不暴露、build 恒 rc=0 ⇒ ALG §4 冻结语义 + 增只读访问器（不动冻结结构体） | 文档 + 实现 |
| 9 | M8-C-002：合同称「worker 数无关（位精确）」而门只用 1e-6 ⇒ 收紧到 1e-12/位精确；phase2_sampler_parallel.OneTvsTwoTDeterminism 在 Linux 是 SKIPPED ⇒ 补真实门 | 门 + 测试 |

## 硬纪律

1. 零 git 写；不得改 ci/**、.github/**（门禁登记面改动转 CI-003）；
2. 每条给「改前 → 改后 → 依据」；实现改动给「改前红 → 改后绿」；
3. ninja -C build -k 0 必须 0 FAILED；相关 ctest 全绿（phase2|coverage|upm|rejection|integration）；改被锚文档后复跑锚点门 rc=0；
4. 在 SCIENCE_CORRECTNESS.md 追加 claim SC-005；
5. 日志落 run/PROJECT-GOVERNANCE-01/SCI-FIX-WEIGHT/logs/。

## 交付（中文，直白）

1. 逐条执行表；2. 红→绿证据；3. 门与 ctest 结果；4. 未做项；5. 自证摘要。