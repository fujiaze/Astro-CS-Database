# 逐 Task 施工规范

## A. 每个 Task 的固定动作（不得省略）

1. 从 ledger 核对所有依赖为 PASS；把本 Task 改为 `IN_PROGRESS`。
2. `git fetch origin --prune`；核对 `HEAD=main=origin/main`、工作树外部变化登记完整。
3. 从本文件复制该 Task 的“输入/动作/验证/产物/PASS”到当日日志，不得改写门槛。
4. 只修改 scope 所需文件。新发现不在 scope：写 FINDINGS，不顺手修改。
5. 运行本 Task 指定测试并用内置/过渡监控记录。未运行写 FAIL，不得写“理论通过”。
6. 更新 traceability、相关文档、测试和日志；检查 staged diff 无无关文件。
7. 需要 commit 的 Task：commit message `(<task_id>) <单一目的>`，push main，记录 SHA。
8. 生成 review capsule；状态按真实结果更新。任何 PARTIAL、waiver、忽略错误均不是 PASS。

证据根目录建议：`artifacts/prerelease_v5/<task_id>/`，不提交大产物。仓库内只提交源码、正式文档、测试、schema 和小型机器摘要。

## B. BASE / DOC / SCI / ALG

| Task | 必做动作 | 必验收产物与 PASS 条件 |
|---|---|---|
| BASE-001 | 冻结 HEAD/main/origin/main、remote 脱敏、host、工具链、工作树；把 `12_V3_V4_MIGRATION.md` 六项风险逐项录入 FINDINGS；盘点 V4 当前 main 已实现文件 | `BASELINE.json`、`V3_V4_RISK_REGISTER.csv`；三 SHA 一致、无不明修改，风险 6/6 入账 |
| GOV-001 | 将 11 的短块与现有 AGENTS 去重合并；删除 Linux 默认中的 Git Bash/pwsh 旧表述；写连续执行规则 | AGENTS 精简；checker 找到 main-only/amd64/节点/CPU-only/资源门禁/alpha；无冲突条款 |
| VER-001 | 找出全部版本字面量；建立唯一版本源和生成接口；实现 alpha 格式、clean/dirty build metadata；包名/CLI/schema/manifest 同步 | `version_consistency` test mutation：任意一处伪造版本必须失败；`--version --json` schema PASS；不得出现 stable/RC |
| TRACE-001 | 定义 claim ID 格式、六层字段、唯一性/引用存在/无断链检查；加入 CI/CTest | schema+checker+mutation tests；删除任一层引用时 checker 必须失败 |
| DOC-001 | 从当前文档/代码列出单位、frame、坐标、pixel center、weight/variance/ivar/support/mask/NaN；每词冻结唯一含义，列 legacy alias 与迁移 | 正式 glossary；机器检索模糊单位和冲突定义；核心术语无 TBD/二选一 |

### SCI-001..007 统一写法

每项必须逐段包含：Scope、Symbols、Continuous definition、Units、Frames、Assumptions、Boundary/invalid、Uncertainty、Invariants、Acceptance、Primary literature、Project-defined derivation。引用必须核对原文 section/equation。完成后由 `science_contract_lint` 检查章节和 claim IDs。

| Task | 必须回答的专属问题 | PASS 条件 |
|---|---|---|
| SCI-001 | bias/dark/flat/pedestal、曝光/gain/read noise、负值、saturation、mask、variance 如何定义和传播 | 每个校准量单位唯一；解析不变量可转成 SYN-001 |
| SCI-002 | WCS frame/pixel convention、PSF 参数、aperture/flux/background、photometric scale 与不确定度 | 坐标和 photometry 语义不依赖代码猜测；可构造解析星场 |
| SCI-003 | signal/noise/SNR/variance/ivar/blank sky、Poisson+read noise、权重归一与适用域 | 所有 weight 字段能指出数学定义；零/负/NaN 条件明确 |
| SCI-004 | drizzle footprint、pixfrac、surface brightness/flux、support、variance/covariance、边界 | 冻结输出单位，给出 flux/support 不变量和归约误差合同 |
| SCI-005 | UPM 的观测方程、控制点、光度面 basis、正则化、gauge/退化、接缝指标 | 项目原创推导完整；目标函数、参数单位、唯一性条件明确 |
| SCI-006 | 每种 rejection 的统计假设、阈值、small-N、frame identity；integration 权重/归一/mask | 自动选择规则可判定，禁止用“效果好”定义；可生成 outlier oracle |
| SCI-007 | 严格执行 13 的 12 个科学项；HiPS/HEALPix/FITS-WCS 原文定位；冻结 Alpha 投影/采样/单位范围 | `UNRESOLVED-SCIENCE=0` 才可 PASS；不能声称未实现的 variance/flux 模式 |

### ALG-001..007 统一写法

从对应 SCI 的每个方程推导离散伪代码；列输入 shape/layout、时间/空间复杂度、数值稳定性、精度、并行分解、归约顺序、取消点和 Oracle。不得用源代码作为推导。

| Task | 专属算法检查点 | PASS 条件 |
|---|---|---|
| ALG-001 | 校准操作顺序、variance/mask 逐步传播、SIMD 安全条件 | 步骤覆盖 SCI-001 全 claim，边界可测试 |
| ALG-002 | WCS 变换、PSF/photometry estimator、批处理与误差 | frame/pixel-center 一致；解析参考明确 |
| ALG-003 | 稳健 noise estimator、SNR/ivar、并行 reduction | small-N 和 reduction tolerance 预冻结 |
| ALG-004 | spherical/planar footprint、overlap 权重、accumulate/normalize | support 与 flux/brightness 不变量可计算；复杂度明确 |
| ALG-005 | 稀疏矩阵构造、solver、preconditioner、regularization/gauge | 收敛/失败条件、determinism 和内存上界明确 |
| ALG-006 | rejection 顺序、statistics、weighted integration、NaN/mask | frame identity 保留；1/N worker 语义一致 |
| ALG-007 | HiPS order 选择、tile lookup、world/pixel、跨 tile sampler、coverage、FITS write | 执行 13；TAN/其他范围精确；所有 FITS-WCS keyword 来源明确 |

REV-001：将七组 SCI/ALG 的完整最新文档、glossary、traceability、引用清单、对应旧/新测试放入一个胶囊；登记 `REVIEW_PENDING` 后继续 ARCH。不得等待审核。

## C. ARCH / API / CLI / Phase3

| Task | 必做动作 | 测试与 PASS 条件 |
|---|---|---|
| ARCH-001 | 用 call graph/符号检索列出所有 exe、Phase 入口、线程池、OpenMP、async/future、锁、队列、ACR 调用、I/O writer；标生产/测试/死代码 | `PRODUCTION_EXECUTION_INVENTORY.csv` 100% 构建目标；历史 AIO/sampler/UPM 风险能定位到 symbol |
| ARCH-002 | 写单一 CLI 组件图、Phase 数据流、配置/manifest/artifact 生命周期、错误/取消/恢复；明确旧 exe 迁移 | 文档与 01/04 完全一致；不存在第二用户入口 |
| ARCH-003 | 写 backend C ABI、loader、per-kernel dispatcher、profile/fallback、信任边界、错误回退 | 覆盖 05 全条目；C++ ABI/任意插件路径为禁止项 |
| ARCH-004 | 写全局 thread budget；列每阶段串行 I/O、CPU task、async pipeline、backpressure、nested parallelism；每 symbol 写线程模型 | 静态 checker 能找未登记线程创建/OpenMP；所有重 kernel 有预算来源 |
| ARCH-005 | 写 HiPS reader/WCS/resampler/FITS writer 模块和数据结构、tile cache、跨 tile 访问、并发与内存上界 | 架构逐 claim 追到 ALG-007，不把科学选择藏在 cache/loader |
| API-001 | 定义公共 POD/opaque handle、allocator、span/buffer、errors、cancel、logger、thread budget；逐字段单位/所有权 | headers 可被 C/C++ 独立编译；ABI layout tests；无 STL/exception 越界 |
| API-002 | 按 04 定义命令树、schema v1、exit codes、JSONL events、cancel/crash | golden help+schema+exit tests 双平台源一致 |
| API-003 | 逐函数定义 Phase1 create/validate/run/inspect；参数、单位、同步/异步、reentrant、错误 | doc-symbol-signature checker PASS；每 API 有直接 test ID |
| API-004 | 同上定义 Phase2；明确 drizzle/UPM/rejection/integration 数据所有权和 thread budget | 同上；不得共享隐藏全局状态 |
| API-005 | 定义 Phase3 request/result、HiPS source、WCS output、sampler、coverage/FITS；拒绝未支持科学模式 | 同上；输入不明确时返回确定错误而非猜测 |
| CLI-001 | 建单一 target 和 Windows/Linux main；只接通 help/version stub | 两平台可编译；发布安装规则只选一个 exe |
| CLI-002 | 实现统一 parser、JSON/JSONL writer、exit mapping、sequence、cancel、crash boundary | 04 全 golden/malformed/Unicode/cancel tests；stdout 无日志污染 |
| CLI-003 | 实现 config init/validate/effective、run manifest、verify、version；科学 config 与 cpu profile 分离 | schema mutation/hash/stale profile tests PASS |
| CLI-004 | 去除 Phase1 shell-out，进程内调用 API；传 cancel/thread budget/monitor | integration test 证明无子进程、事件完整、错误映射正确 |
| CLI-005 | 同上接入 Phase2 | 同上；实际 production route 被 test 覆盖 |
| P3-001 | 严格解析 HiPS properties；验证 order/tile width/format/frame；安全路径和缺 tile | synthetic tiles：合法/缺失/恶意路径/边界 order；无 silent default |
| P3-002 | 实现 FITS-WCS output descriptor、pixel-center world transform和反变换 | 独立 WCS roundtrip；RA wrap/pole/rotation；CRPIX/CD keywords 正确 |
| P3-003 | 实现 order selector、tile lookup、跨 tile nearest/bilinear 或审核批准 sampler、coverage/mask/NaN/单位 | 解析球面场和 tile seam Oracle；未支持 variance/flux 输入明确拒绝 |
| P3-004 | 原子写 FITS、header/provenance/checksum、失败清理；用独立 FITS/WCS reader 重开 | header/data/hash/coverage PASS；取消不留完整假文件 |
| CLI-006 | 接入 `phase3 run`，所有参数走 config/API，不在 CLI 复制算法 | end-to-end Phase3 synthetic 命令 PASS |
| CLI-007 | 实现 `run --phases`，阶段 artifact 传递、resume 校验、run manifest、取消 | 1/2/3 单独与组合；resume hash mismatch 必失败 |
| CLI-008 | 删除发布 install/package 中旧 phase/benchmark exe；保留 test-only target 需明确 | install tree scanner 仅一个用户 exe；CLI 不 shell-out |

## D. CPU Backend / Benchmark

| Task | 必做动作 | 测试与 PASS 条件 |
|---|---|---|
| ABI-001 | 实现 05 的 C ABI v1、struct_size/version handshake、host allocator/log/cancel/budget、kernel table/selftest | 编译器/Debug/Release ABI tests；异常不能跨边界；分配方可验证 |
| ABI-002 | 生成 manifest/hash；CPUID+OSXSAVE+XGETBV；Windows 限制搜索/Linux 私有相对路径；预检与失败策略 | fake manifest/hash/ABI/unsupported ISA/path injection tests 全拒绝且无 illegal instruction |
| ABI-003 | 实现最低 amd64 baseline 的所有生产 kernel；每 kernel 外层按有效 affinity 多线程 | baseline opcode scanner；>=2 CPU 时 compute synthetic 观察 >=2 active threads；Oracle PASS |
| ISA-001..004 | 先用 profile 证明 kernel 热点；只为热点做本地编译变体；共享合同；逐 kernel Oracle 后登记 capability | 无收益允许记录 NOT_SHIPPED 但 Task 仅在有完整测量时 PASS；主/ baseline 无 ISA 污染；错误变体绝不入候选 |
| ISA-005 | 只评估整数/位操作热点；VNNI 等与算法无关则写 NOT_APPLICABLE 证据，不写空 DLL | capability 与热点对应；无机械指令集堆砌 |
| BENCH-001 | 实现硬件/affinity/NUMA/cgroup/Job 检测，输出 schema | fixture/mock 和实机比对；available CPUs 受 affinity 约束 |
| BENCH-002 | harness 对每 backend 先调用独立 scalar Oracle/selftest，再预热/计时；捕获错误 | 故意错误 backend 被禁用且不得测速获胜 |
| BENCH-003 | memory read/write/copy/triad；每 kernel small/medium/large、FP32/64、alignment、自动 worker/block candidates | 候选不含源码硬编码 core count；结果含资源指标和原始样本引用 |
| BENCH-004 | median/MAD/p05/p95、噪声裕量、逐 kernel 选择、profile hash/失效/fallback | profile schema/mutation/stale/AVX512 slower tests；无 profile baseline 多线程 |
| BENCH-005 | 实现 hardware/benchmark/doctor CLI，quick/full，机器可读输出 | full/quick/help/cancel/timeout/profile-output golden tests |

ISA Task 状态说明：某变体因“CPU 不支持”不能在 Linux 验证时不得谎报 PASS；在源码+Oracle+Windows 支持机验证完成后 PASS。最终 manifest 可不含无收益变体，但原因、数据和代码是否保留必须明确。

## E. Monitor / 并行修复

| Task | 必做动作 | PASS 条件 |
|---|---|---|
| MON-001 | Linux `/proc`/系统接口与 Windows API 采集 process/system 指标；单调时间、采样开销测量 | CPU/RSS/I/O/thread 与 OS 工具误差在冻结范围；监控自身开销达标 |
| MON-002 | 所有 Phase/kernel 发 stage/resource/backend 事件；summary/downsample/raw 分层 | >5s 未标注 stage 的 mutation test 必失败；审核摘要小型化 |
| MON-003 | 实现 07 分类和公式、first-10s 快速失败、exit 10、诊断分类 | 人工 sleep/lock/io/memory/compute fixtures 各判对；低 CPU compute 必失败 |
| MON-004 | 20 次循环、预热剔除、稳健斜率/峰值/retained bytes/OOM 预警 | 注入泄漏被抓；稳定 cache 不误判；报告含曲线摘要 |
| ISO-001 | 静态+运行测试证明 CLI/Phase/dispatcher/manifest 不引用 ACR/GPU/Mixed；发行包扫描 | production route 0 触达；配置请求 ACR 明确拒绝；不是默默 fallback |
| PAR-001 | 删除生产全局串行锁；实现有界队列/backpressure/error/cancel；I/O 与 compute 可 overlap | lock contention test、queue saturation、failure drain；CPU compute 不被 writer 饿死 |
| PAR-002 | 修 sampler 生命周期、共享状态、race、异常；任务粒度与 thread budget | TSan/压力/取消/重复运行；无 crash/race，N-worker 正加速 |
| PAR-003 | 分离 UPM thread-local/归约；稀疏求解遵守预算；避免 oversubscription | TSan、合成等价、1/N scaling、内存增长 PASS |
| PAR-004 | Drizzle tile/task 分解、thread-local accumulation/安全归约、cache-friendly layout | Oracle、support/flux、1/N scaling、接缝、内存上界 PASS |
| PAR-005 | rejection/integration 并行；frame identity 不丢；确定性类别匹配 ALG | outlier oracle、1/N tolerance、race/scaling PASS |
| PAR-006 | profile 后只修 Phase1 真热点；校准/PSF/noise 遵守预算 | 每个修改有 before/after 当前实现 microbenchmark；不是历史版本对比 |
| PAR-007 | 统一所有线程池/OpenMP/backend 预算；禁止内部各自吃满全核 | nested stress 时 threads/CPU/RAM 不超合同；端到端利用率 PASS |

## F. Synthetic / 文档检查

每个 SYN Task 必须创建 `tools/validation/<group>` 五件套；先提交 contract/seed/tolerance，再运行。所有 shipped backend、1 worker、自动 N worker均比较独立 Oracle；结果按 precision contract，不做历史对比。

| Task | 数据与不变量 | PASS 条件 |
|---|---|---|
| SYN-001 | constant/ramp/dark exposure/flat/gain/read noise/saturation/mask/NaN | value+variance+mask 单位/解析值全过 |
| SYN-002 | 已知 WCS 星场、解析 PSF、已知 flux/background、frame roundtrip | 坐标/flux/uncertainty 在预冻结容差 |
| SYN-003 | Gaussian/Poisson/constant/blank sky/outlier/small-N | estimator bias/variance/SNR/ivar 和边界符合 SCI |
| SYN-004 | 常数/点源/梯度/旋转/亚像素 shift/pixfrac/tile boundary | flux或brightness/support/variance/coverage 不变量全过 |
| SYN-005 | 已知低阶光度面、重叠图、gauge/退化/正则强度 | 参数恢复、残差、接缝降低且不破坏星 flux |
| SYN-006 | 已知 inlier/outlier/cosmic ray、small-N、多权重 | reject set、identity、weighted result 可解析 |
| SYN-007 | 执行 13 第5节全部 case | 独立 WCS/FITS reader 通过；tile seam/RA wrap/mask/单位正确 |
| SYN-008 | 三块重叠合成场，已知背景面+星点+coverage | seam 指标门槛预冻结并过；无“视觉上可以”替代 |
| SYN-009 | 合成帧经过 Phase1→2→3；中断/resume/hash mismatch | 单 CLI、artifact chain、events、资源与科学不变量全过 |
| DOCCHK-001 | 解析 headers/source/schema/help；核对文档函数名/签名/字段/退出码 | 删除/改名/签名 mutation 均使 checker fail |
| DOCCHK-002 | 对 traceability 六层、单位 glossary、test/oracle IDs 做闭环；注入 Drizzle 单位二义性 | 100% 核心 claims 闭环；单位 mutation 被抓，修复 V3 漏检 |

## G. Linux / Windows / Release

| Task | 执行清单 | PASS 条件 |
|---|---|---|
| LNX-001 | clean configure/build Debug+Release amd64；记录 compiler/flags；warnings 分类并清零新增债 | 所有命令 exit 0；无 ignored error；architecture check 拒绝非 amd64 |
| LNX-002 | inspect、doctor、quick benchmark；测试缺失/stale/corrupt profile、unsupported backend | 安全 fallback baseline 且多线程；profile 决策可解释 |
| LNX-003 | `test synthetic all`、CLI golden、resource fixtures、20-loop memory | 全 PASS；低利用率 fixture 必 FAIL，正常 compute 门禁 PASS |
| LNX-004 | ASan/UBSan；TSan 可编译/可运行范围；clang-tidy/cppcheck 等现有工具；结果不能截断 | 0 P0/P1；工具缺失记 prerequisite 并用现有替代，不能假 PASS |
| LNX-005 | staging install、package whitelist、manifest/SBOM/licenses/hash；在空临时目录 smoke | 只有一个 user exe；私有 SO/manifest 完整；包名 alpha；解包运行 PASS |
| REV-002 | 完整 ARCH/API/headers/core source/oracles/checkers/Linux reports 胶囊 | 索引/hash/全文件完整，REVIEW_PENDING 后继续 Windows探测 |
| WIN-001 | SSH/远程只读探测；在线后 fetch/reset 禁止；仅 fast-forward/pull 到相同 main；重算数据 manifest | SHA 相同、clean、32R/masters hash 完整；离线则记录并继续可做事项 |
| WIN-002 | MSVC amd64 clean Debug/Release；CTest/CLI basic；不得忽略 Error 1 | exit code 全 0、0 failed tests、0 ignored build errors |
| WIN-003 | doctor；逐 backend安全检查；full benchmark；检查 AVX512 downclock/无收益；冻结 profile | 每 shipped backend Oracle PASS；逐 kernel选择；资源门禁 PASS |
| WIN-004 | 全 SYN-001..009、CLI/ABI/loader/resource/memory tests | 全 PASS；Windows/Linux差异在预冻结容差 |
| WIN-005 | MSVC static analysis、Application Verifier/ASan 可用集、重复/取消/路径测试 | 0 P0/P1；无竞态/泄漏/invalid access |
| WIN-006 | 从本机数据选三板块少量 R 帧+masters；hash manifest；跑代表链路并自动监控 | contribution、science/接缝/资源/内存全 PASS 才可 32R |
| WIN-007 | 冻结当前候选 SHA/profile/config/32R manifest；只启动一次；first-10s gate；全 Phase | 32/32 contribution；run manifest/hash完整；任何失败不得再偷偷重跑，先新 Task 根因 |
| WIN-008 | 固定坐标/FOV/STF 生成 HiPS candidate/support/residual；计算 seam/flux/coverage；保存降采样资源图 | 数值门槛 PASS；无低利用率区间；用户可在 HiPS Browser 审核 |
| WIN-009 | Windows staging/package/SBOM/licenses/hash/smoke；模拟无 profile | 单 `astrocs.exe`+私有 DLL；alpha 名；baseline 多线程 fallback；解包 PASS |
| REV-003 | 32R manifest/摘要/固定截图/资源摘要/Windows package manifest 胶囊，不放数据与二进制 | <5MiB/文件且总白名单；hash/索引 PASS |
| REL-001 | 合并外部 review；P0/P1 每项用独立 Task commit 修复并重验；P2 写 accept/fix 理由 | P0=P1=0；无 open unresolved science；所有 review conclusion 可追溯 |
| REL-002 | 核心清单 100% 核查；对其他源码按最终 SHA+path hash 排序取前20%并记录 | 抽样算法/列表可复现；发现问题回 REL-001，不降低样本 |
| REL-003 | 锁最终 main SHA、alpha version、两平台 artifact hash、traceability、ledger、findings、测试/资源表 | ledger 只有 PASS；各表计数机器一致；发布物来自同 SHA |
| REL-004 | 用 package_final 白名单生成审核包、SHA256SUMS、运行 validate_final_package | `FINAL_PACKAGE_PASS`；verdict 只能 `AWAITING_EXTERNAL_RELEASE_REVIEW`；然后停止 |

## H. 禁止 Agent 的替代解释

- “算法本来就是单线程”不能解释重计算。
- “AVX512 更先进”不能替代 benchmark。
- “Linux 只有 2 核”不能解释只用 1 核。
- “历史报告通过”不能替代当前 SHA 证据。
- “视觉没有明显问题”不能替代 seam/flux/coverage 指标。
- “Windows 工具报错但主目标生成”不能 PASS。
- “文档大体一致”不能替代 symbol/signature/traceability checker。
- “Phase3 以后再做”不能通过本 alpha 的 Phase1/2/3 单入口目标。

