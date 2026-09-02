# Task 固定执行要求

## BASE

### BASE-001

- `git fetch origin --prune`，冻结当时 `origin/main`；不得假定任何审核包内 SHA 仍是最新。
- 生成当前代码状态表：V3 已提交成果、未完成实现、已知竞态、低利用率路径、文档歧义、失败测试。
- 不进行历史 commit diff，不构建历史版本。

### GOV-001

- 按 `10_AGENTS_MD_REQUIRED_BLOCK.md` 更新根 `AGENTS.md`，只加入简短长期政策。
- 不加入控制包全文，不出现逐检查点停工。

### BASE-002

- 只运行当前 main 的短构建和短测试；单命令默认 timeout 10分钟。
- 禁止32R、历史版本、超过60秒的性能运行。
- 记录当前失败，不使用豁免改 PASS。

## SCI / ALG / DOC

### SCI-001..006

- 每个科学量必须在机器可读合同中有：SCI ID、符号、定义、单位、域、无效值、假设、不变量、容差来源、authority 文档。
- 同一量只允许一个 authority；其他文档只能引用 ID。
- Drizzle 必须选择唯一输入物理量，禁止继续保留“ADU 或 ADU/pixel 二选一”。
- 所有定义先 commit，再允许相应 ALG、Oracle 或实现修改。

### ALG-001..005

- 从对应 SCI 公式推导离散算法；包含伪代码、边界、误差、复杂度、数据布局、并行粒度、同步点、确定性和内存模型。
- 复杂度必须写全变量，禁止把 frame/control/iteration/tile 因子省略。
- 每个步骤映射到 API/SRC symbol 和未来 SYN test ID。

### DOC-001

- 绘制当前生产调用链，不描述计划中但未接线的并行/异步路径。
- 每阶段声明 `compute / memory / io / mixed`，以及 worker owner、队列、共享状态、错误传播。
- ACR 标为 dormant，不得描述为当前生产 backend。

### DOC-002

- 覆盖全部 public API 和科学核心 private 函数。
- 字段：签名、目的、SCI/ALG ID、输入输出/单位、范围、所有权/lifetime、错误、精度、线程安全、可重入、同步/异步、CPU variant、确定性、复杂度、测试 ID。
- 语义字段不得复制模板；每模块分组检查 distinctness 和空字段。

### REV-001

- 按 `12_REVIEW_CAPSULE_AND_EXTERNAL_AUDIT.md` 打包全部最新 SCI/ALG/ARCH/API、机器合同、引用文献、对应源码与测试。
- `SCIENCE_CLAIMS.csv` 每条核心公式列出主文献 DOI/ADS/arXiv、具体章节/公式及项目自行选择。
- Agent 只能标记 `REVIEW_PENDING`，不得自行填写外部审核结论。
- 等待时继续 CPU/MON/构建等独立任务。

## QA

### QA-001

- 使用 AST/编译数据库提取真实 symbol/signature；正则只能作辅助，不能声称 AST。
- 生成合同而不是手工维护 symbol 数量。

### QA-002

- 检查 SCI/ALG/API/SRC/TEST 全链、单位和公式 ID、配置 schema、生产并行接线、build graph、文档路径。
- checker 必须定位到具体文件/symbol/字段，不能只数关键词。

### QA-003

以下 mutation 必须全部非零退出：

- API 参数换序、类型/const/noexcept 改变；
- ADU 改 second、ADU² 改 ADU、ivar 正负号反转；
- SCI 公式 ID 缺失或 ALG 引用错误；
- 生产 worker 参数丢失、强制1线程、全局固定 AVX；
- 文档 symbol/文件不存在；
- 配置未知 key/错误 enum；
- build graph 引用不存在目标；
- test 文件或 test ID 不存在；
- ACR 被重新接入生产。

任何真实 mutation 漏报均为 FAIL，不得用“轻微局限”判 PASS。

## CPU / MON / PAR

### CPU-001..004

严格执行 `04_CPU_AUTOTUNE_SPEC.md`。硬件检测、variant correctness、benchmark、cache 和 runtime dispatch 缺一不可。

### MON-001..002

严格执行 `05_RESOURCE_MONITOR_SPEC.md`。所有重计算通过 wrapper；CLI 输出结构化 stage begin/end。

### PAR-001

- 解决 cfitsio/AIO 线程安全，生产形态固定为单 I/O owner + 有界预取 + 多计算 worker，或经测试证明安全的独立 reader。
- 单 I/O 线程允许，但必须与计算重叠；禁止主线程全局 mutex 逐次阻塞计算。
- 测试读取失败、EOF、取消、队列满、writer失败、关闭顺序、长时间无死锁。

### PAR-002

- Sampler 的读取、解码、控制点计算分层；I/O与计算重叠。
- 计算阶段必须多核扩展；ID、顺序、计数 exact；浮点过 SCI 容差。

### PAR-003

- UPM 所有 TSan/竞态证据按真实缺陷处理，除非用最小独立复现严格证明假阳性。
- observation、稀疏构建、SpMV、残差/权重更新并行；gauge和固定归并确定。
- dense cache 若生产不消费则默认不生成；需要生成时使用批量写，不逐元素 insert。

### PAR-004

- 重排 frame-major 跨步热点；为 rejection/integration 选择适合逐像素连续访问的数据布局。
- 不得在像素热循环分配 vector/map。
- signal/support/variance-or-ivar/rejection/valid-depth 全层输出并测试。

### PAR-005

- Drizzle 累积遵守 SCI-004；float64累积/float32输出冻结规则不变。
- tile/source pixel 并行，无竞态、flux/面亮度不变量通过；CPU variant 受 runtime dispatch 控制。

### PAR-006

- 对 Calibration、PSF/star detection、PlateSolve/WCS、Photometry、SNR 做热点测量。
- 只有实测重核进入并行/ISA registry；短解析和元数据不为并行而并行。

### PAR-007

- 当前生产 CLI 不得调用 ACR Dispatcher/GPU/Mixed；纯 CPU executor 为唯一生产路径。
- 不删除 ACR 基座；测试确保 ACR 未被生产链接/路由，文档标记 deferred。

### REV-002

- 打包 CPU topology/ISA/autotune、resource monitor、AIO pipeline、Sampler/UPM/Integration/Drizzle 的最新完整文件、patch、测试和 profile 摘要。
- 外部审核人重点抽查运行时安全、竞态、低利用率掩盖和数值等价。

## SYN

每项按 `06_SYNTHETIC_VALIDATION_MATRIX.md` 创建独立目录和脚本，不准复用生产实现生成期望值。

## LINUX

### LNX-001

- 仓库外 clean build：GCC Release/Debug；可用时 Clang Release。
- 正式文档命令必须直接成功；禁止 ignored error 和 workaround PASS。
- 构建全部 Linux 支持的 CLI/库/测试；不要求 Linux 运行 Windows-only GUI。

### LNX-002

- ASan/UBSan 分片运行，避免固定60秒 watchdog误杀；LSan 在无 ptrace环境执行。
- OpenMP竞态使用可理解 runtime 的工具，或把并行核心抽成 `std::thread` 最小复现；不能只凭 deterministic 宣称无 race。
- 0 sanitizer error、0 leak、0未解释 race。

### LNX-003

- 对所有合成重核运行 1 worker 和 autotuned workers；资源监控和数值门禁同时通过。
- Linux只用短规模，单项5–60秒，不追求32R。

### LNX-004

- Fatduck 在线时拷贝每板块最多1帧及所需小型派生数据；Linux只做辅助验证。
- 如果数据或 Gaia 过大，跳过真实 Stage1，使用已经生成的小型 HiPS 子集；不得因此阻塞 Windows任务。

### REV-003

- 打包全部 `tools/validation/<group>`、固定合同/容差、独立 Oracle 和代表性结果。
- 外部审核人完整审核核心 Oracle，其他测试按最终 SHA 确定性抽样。

## WINDOWS

### WIN-001

- Fatduck `git pull --ff-only`，必须与当时 `origin/main` 完全相同。
- clean build 全部正式模块；任何 `Error ignored` 或测试 FAIL 都必须修复，修复回到 Linux main 原子提交再 pull。

### WIN-002

- 运行当前候选 CPU benchmark；记录可用 CPU、affinity、variant、worker曲线和最终选择。
- 先通过每 variant 科学等价，再允许选性能最快者。

### WIN-003

- 运行全部 SYN、unit、module、contract、sanitizer；FAIL=0，非明确硬件缺失 SKIP=0。

### WIN-004

- 每板块最多2帧真实生产链；所有重计算用 monitor。
- 若 compute 利用率不达门禁，先修复，不得启动32R。

### WIN-005

- 仅当前候选、全部32R、一次成功运行；禁止 A/B/C、禁止历史差分。
- 输入 manifest、config、benchmark profile、commit SHA 固定。

### WIN-006

- 接缝依据 SCI 定义和当前数据的统计不变量判断，不引用历史阈值。
- 输出 candidate signal/support/variance-or-ivar/rejection/valid-depth、seam metrics、固定 STF/视场 HiPS Browser 配置。
- 大产品只进 manifest。

### REV-004

- 提交当前 final SHA 的相关源码/文档快照、Windows build/test、autotune、32R、资源和HiPS摘要。
- 禁止附带历史版本、完整HiPS、原始像素或长日志。

## RELEASE

### REL-001

- 机器重建 SCI->ALG->API->SRC->TEST 矩阵；P0/P1=0；所有当前范围任务 PASS。
- ACR GPU/Mixed 和历史对比记 DEFERRED，不计发布缺陷。

### REL-002

- fresh clone 使用正式命令重做短构建/测试；验证 `HEAD==origin/main`。
- 生成唯一最终审核包，状态 `AWAITING_EXTERNAL_RELEASE_REVIEW`；不得自行打 tag/release。
