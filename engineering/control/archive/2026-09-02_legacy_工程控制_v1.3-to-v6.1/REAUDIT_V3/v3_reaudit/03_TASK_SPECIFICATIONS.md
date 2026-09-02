# Task 详细规范

本文件定义每个 Task 的固定动作。禁止合并、跳过或改变顺序。任何 Task 的测试失败时：记录 `FAIL`、不得 commit、不得 push、不得继续下一 Task。

## A. 身份与数据

### ID-001 仓库身份

- 输入：当前仓库。
- 命令：`git fetch origin --prune`；记录 `git rev-parse HEAD main origin/main`、`git status --porcelain=v2`、remote 去凭据摘要。
- PASS：当前分支是 `main`；三 SHA 相同；除已在 `EXTERNAL_WORKTREE_CHANGES.md` 精确列出的外部变化外无修改。
- FAIL：任何 SHA 不同、来源不明修改、remote URL 含凭据。
- 输出：`identity.json`、`EXTERNAL_WORKTREE_CHANGES.md`。

### ID-002 主机与工具链

- 记录 OS、kernel、CPU/逻辑核、RAM、磁盘、gcc/clang/cmake/ninja/python/git 版本。
- 仅探测，不安装未知软件。
- PASS：Linux 至少 2 个逻辑 CPU、可用内存满足构建；缺失工具列为明确 prerequisite。

### ID-003 32R 数据冻结

- 从 Fatduck 读取真实数据，不把原始数据加入 Git 或审核包。
- 生成 32 个 Red light + 3 个 master 的相对路径、字节数、SHA-256、FITS/XISF 基本元数据。
- 校验 3 板块帧数必须为 `11 + 11 + 10 = 32`，无重复 hash、无缺帧。
- 与 V2 manifest 不同则 `BLOCKED` 并等待外部确认，禁止自动替换基线。

## B. 并行架构

### CON-001 生产执行盘点

- 扫描所有生产 CLI、库和核心循环，输出 `execution_inventory.csv`。
- 每行必须有：stage、入口 symbol、循环 symbol、复杂度、历史 wall、当前并行机制、线程所有者、I/O 模式、共享状态、是否生产可达、测试 ID。
- 必须覆盖 Calibration、PlateSolve、Photometry、SNR、Drizzle、Coverage、Sampler、UPM build、UPM solve、Rejection、Integration、HiPS write、ACR。
- 禁止只统计 `#pragma omp` 字符串；必须从 CLI 入口给出调用链。

### CON-002 全局 worker 预算合同

- 定义唯一 `ExecutionOptions`/等价对象：`cpu_workers`、`io_workers`、`gpu_route`、`deterministic`、`memory_budget_bytes`。
- 默认 `cpu_workers = max(1, hardware_concurrency)`，允许 CLI/配置覆盖。
- 嵌套模块只能借用预算，不能各自创建等规模线程池。
- 异步队列必须有界，取消/错误传播和关闭顺序必须明确。
- 文档、header、配置 schema 和测试同一 commit。

### CON-003 Phase2 生产路由测试

- 新增从 `astrocs-stage2` CLI 到 sampler/UPM/integration/ACR 的路由测试。
- 测试必须在缺少编译定义、dispatcher 未被调用、worker 参数丢失时失败。
- 禁止用 mock-only 路径证明生产接线。

### CON-004 Sampler 并行化

- 并行单位固定为 coverage cell/control block；每 worker 独立 reader/buffer。
- 结果写入预分配、稳定索引槽；最终排序使用固定 key。
- 禁止用全局 `critical(aio_read)` 把主计算串行化。
- 测试：1T/2T 结果、接受/拒绝计数、frame ID、control 顺序和 hash 在规定容差内一致。

### CON-005 UPM build/solve 并行化

- observation 构建、按 frame/control 聚合、稀疏矩阵 SpMV/残差计算必须并行。
- gauge、连通分量、收敛判据和最终归并保持固定顺序。
- 预分配热循环临时区；禁止 per-pixel/per-observation heap allocation。
- 测试：常量场、已知加性场、Huber 污染、断开分量、空/退化输入、1T/2T 确定性。

### CON-006 Stage2 integration 并行化

- 外层 tile 并行；单 tile 内 chunk/pixel 采用受预算约束的并行，禁止嵌套过量线程。
- 每 worker 独立 rejection scratch、source index、统计 buffer；禁止像素热循环创建 vector。
- tile 输出按稳定 tile id 提交；写入阶段可串行但累计不得超过总计算 1%，否则改为有界 writer 队列。
- 测试：signal/support/variance/rejection/valid-depth 图层逐像素差分。

### CON-007 ACR CPU 生产接线

- `acr_route=cpu` 必须进入 ACR Dispatcher 的 CPU backend，而不是 legacy 串行旁路。
- CPU backend 必须使用全局 worker 预算。
- `auto` 在 Linux 无 CUDA 时必须明确记录 fallback 原因并落到同一并行 CPU backend。
- 新增生产 CLI 路由断言和运行日志字段：requested/effective route、workers、fallback_reason。

### CON-008 异步 I/O

- HiPS/FITS/XISF 读取与计算用有界 producer/consumer；队列容量由内存预算计算。
- 所有 buffer 所有权、reader 线程安全、异常传播、取消、flush、shutdown 写入 ARCH 文档。
- 测试模拟读取失败、写入失败、取消和队列满；不得死锁或丢失错误码。

### CON-009 并行正确性

- GCC/Clang 可用时运行 ThreadSanitizer；不可用必须给出编译器证据并在 Fatduck/其他节点补测。
- 同 seed 同输入分别以 1T、2T、重复 2T 运行；结构性计数和 ID/hash 必须 exact；浮点数组按 SCI 容差比较。
- 禁止以最终图“看起来相同”代替逐层比较。

### CON-010 2C Linux 运行门禁

- 使用至少 5 秒、最多 60 秒的合成生产 CLI 工作负载。
- 记录 1T 与 2T wall/user/sys、每 200 ms threads、CPU%、RSS。
- PASS：2T `max_threads>=2`；计算窗口平均 CPU `>=150%`；`wall_1T/wall_2T>=1.50`；数值门禁通过；无串行段 `>=1s`；串行累计 `<1%`。
- 任一不满足即 FAIL；禁止开始 32R。

## C. 科学定义、算法和 oracle

### SCI-001 权威索引

- 建立唯一 SCI 文档索引；每个科学量只有一个权威定义，其他文档仅链接。
- 每条定义有 SCI ID、符号、单位、域、精度、假设、不变量和禁用解释。

### SCI-002 Noise/SNR/variance/ivar

- 冻结 `signal`、`noise_sigma`、`variance`、`ivar`、`support`、`local_snr`、frame quality 的数学定义和单位。
- 明确 gain/read noise/blank sky、无效值、0 variance、缺失估计和传播规则。
- 禁止使用未限定含义的 `weight`、`sigma`、`value`。

### SCI-003 Drizzle

- 固定语义：输入像素值是源像素积分通量还是面亮度，两者只能选一个；从该定义推导面积因子、pixfrac、输出 BUNIT、variance 和 support。
- 常量场 oracle 必须按选定物理量构造，不能把“每像素常量 ADU”和“常量天空面亮度”混为一谈。
- 记录 flux conservation、球面像素面积、边界、SIP/WCS、NaN/Inf、单帧与多帧情形。

### SCI-004 UPM

- 模型固定为 `y_f(p)=M(p)+C_f(p)+epsilon` 或文档正式选择的另一模型；必须给出 gauge。
- 若采用当前实现的参考 gauge，则常量公共场进入 `M`，`C_f=0`；文档不得写 `C_f=C`。
- Huber 残差必须无量纲；给出 delta 的单位与标准化公式。
- 定义 raw weight、control_ivar、support、quality factor、正则化和断开分量行为。

### SCI-005 Rejection/Integration

- 固定每个 rejection method 的样本数边界、中心/尺度估计、阈值、winsorization、原因码。
- 固定 rejected sample 是否进入 support/variance、fallback 的含义和输出图层。

### ALG-001..004

- 每份 ALG 文档从对应 SCI 公式逐步离散化。
- 必须有伪代码、数据布局、边界条件、数值稳定性、并行划分、同步点、确定性、复杂度（含 F/K/E/iterations/pixels/tiles）和误差预算。
- 禁止只复述源码；每个算法步骤链接具体 SRC symbol 和 TEST ID。

### ORA-001

- 机器化 oracle 至少覆盖：常量面亮度、点源 flux、平坦场、已知噪声、已知 additive field、Huber 单离群、断开 UPM、rejection 边界、NaN/Inf、空输入。
- Oracle 不能调用被测实现来生成期望值；期望值由独立公式或小规模高精度参考实现产生。
- 所有容差在运行前固化进 JSON，禁止看到结果后修改。

## D. 架构、接口和机器检查

### ARCH-001

- 记录模块职责、依赖方向、数据结构、文件格式、单位、所有权、错误边界和 CLI 调用链。
- 架构文档中的每个模块和边必须由脚本从 build graph/include/call evidence 验证。

### ARCH-002

- 记录 CPU pool、OpenMP、异步 I/O、GPU Dispatcher 的唯一所有者和交互。
- 每个 stage 写明串行/并行区域、并行粒度、同步点、worker 上限、内存上限、取消和 fallback。

### API-001..005

- 从 header/AST 生成签名，人工语义字段不得由模板复制。
- 每个 public API 和科学核心函数必须包含：symbol、完整 signature、purpose、SCI/ALG ID、输入/输出定义与单位、范围、所有权/lifetime、错误模型、精度、线程安全、可重入性、同步/异步、backend、确定性、复杂度、测试 ID。
- 422 只是 V2 数量；V3 以实际 AST 为准。任何 header symbol 缺行、文档多余 symbol、签名不一致均 FAIL。
- 分模块完成，每项 Task 只修改该模块合同及测试。

### DOC-001

- public API 使用 Doxygen 等机器可提取注释；科学核心 private 函数同样必须说明“为什么”和约束。
- 删除版本流水账、审核轮次、错误承诺和纯代码复述；保留单位、算法原因、线程/所有权和边界。

### CHK-001..006

- 修复 V2 已证明的全部假阴性。
- 每个 checker 必须有正例、负例和 mutation；mutation 至少包括：参数换序、ADU 改 second、移除并行生产接线、清空 ALG ID、添加不存在 symbol、未知配置键、不存在 build 文件、不存在 test 文件、删除错误处理。
- 所有 mutation 必须导致 checker 非零；原仓库正确样例必须为零；禁止 hardcode 特定文件行数/关键字计数冒充语义检查。

## E. Linux 构建与测试

### BLD-001..004

- 提供根级可重复 Linux configure/build/test 入口。
- 修复所有 `-fPIC`、Windows API/链接参数泄漏、`-static` 和 `cmd.exe` 语法问题。
- 禁止“workaround PASS”；文档命令必须直接成功。
- 每个构建缺陷独立 Task/commit，不混入科学修改。

### TST-001

- 从仓库外全新 build dir 构建 Release 与 Debug；禁止复用旧对象。
- 保存命令、退出码、编译器、warning count、产物清单和 SHA。
- PASS：全部正式 CLI/库构建成功；0 error；新 warning=0。

### TST-002

- 机器枚举全部测试；每行唯一 test ID、binary、command、timeout、结果、耗时、日志 hash。
- PASS：FAIL=0，非外部依赖 SKIP=0；不得用说明文字把 FAIL 变 PASS。

### TST-003

- ASan+UBSan 运行核心单元、合成端到端和错误路径。
- 并行代码运行 TSan 或等价竞态验证。
- PASS：0 sanitizer error、0 data race、0 leak（明确第三方豁免需外部批准）。

### TST-004

- 合成 CLI 覆盖 Stage1->HiPS->Stage2->mosaic->verify，至少包含 3 frame、重叠、背景偏置、噪声、离群点。
- 必须生成 signal/support/variance-or-ivar/rejection/valid-depth 所有正式图层。

### TST-005

- 每个银心板块选择 2 个真实 R 帧，在 Linux 完成 mini pipeline。
- 每次运行 60 分钟 timeout；并行门禁持续采样。
- 不满足并行或科学门禁立即 FAIL，不允许继续全量。

## F. Fatduck、ACR、32R 与接缝

### WIN-001

- Fatduck 上只拉取 `main`；开始前验证 `HEAD==origin/main==CP5 accepted SHA`。
- Windows 工作区有未知修改时停止；不得覆盖或清理。

### WIN-002

- Windows 正式构建命令和测试全部执行；修复需要回到 Linux 主仓 `main` 完成原子 commit/push，再由 Fatduck pull；禁止直接在远端留下未提交修补。

### ACR-001..003

- 同一数据、同一精度依次强制 CPU、GPU、Mixed，不允许 `auto` 冒充三路验证。
- 日志必须证明 requested/effective route、GPU kernel、CPU workers、fallback=none。
- CPU 与 GPU/Mixed 按预冻结容差逐层比较；GPU 不可用记 BLOCKED，不能 PASS。
- 记录 GPU utilization/memory、CPU utilization、wall；Mixed 必须同时观测到 CPU 与 GPU 工作。

### RUN-001

- 用 `git archive` 导出 A/B 到仓库外，只读构建；禁止 branch/worktree commit。
- 记录 archive SHA、compiler、binary SHA。历史代码构建失败不得修历史锚。

### RUN-002

- 每个板块 2 帧，运行 A/B/C/D 小矩阵。先验证配置语义映射，特别是 `weight_mode`。
- 小矩阵任何科学差异或接缝回归则停止，禁止全量。

### RUN-003..006

- A、B、C、D 各只允许一次成功的 32R 全量运行；失败只允许修复明确原因后重跑失败项。
- 输入 manifest、配置、seed、线程数、precision、compiler flags 固定。
- 每次记录 wall/user/sys、CPU/GPU、RSS/VRAM、I/O、线程时间序列、所有产物 hash。
- A/B/C 是历史证据，不修改；D 是预发布候选。

### SEAM-001

- 对两个板块边界使用同一坐标/宽度/星 mask/depth 分层。
- 固定指标：overlap robust affine offset/scale、低频去除后阶跃、梯度跳变、边界/内部残差比、support/depth 分层、UPM field 连续性、black-hole fraction、星点区域残差。
- bootstrap seed 与重复次数预冻结；95% CI 必须相对 B 基线判断。
- PASS：D 不劣于 B 的冻结阈值；任何指标缺失均 BLOCKED/FAIL，不能以目视替代。

### HIPS-001

- HiPS Browser 同时提供 B baseline、C start、D candidate、D-B difference、support、variance/ivar、rejection、valid-depth。
- 固定中心、FOV、projection、tile order、同一 locked STF；禁止每幅自动拉伸。
- 输出 browser 配置 JSON、产品 manifest、每个固定视场截图；大 HiPS 只给路径/大小/hash，不入包。

## G. 发布收尾

### REL-001

- 重新生成 SCI->ALG->API->SRC->TEST 矩阵；禁止手填统计。
- P0/P1 必须为 0；P2 每项必须有外部接受或修复证据。

### REL-002

- 在新 clone 上只用正式文档命令重做构建、测试和小型 CLI；验证 `origin/main` SHA 与审核 SHA 一致。

### PKG-001

- 运行 `scripts/validate_audit_package.py`；所有错误清零。
- 最终只输出白名单证据；Agent 报告 `AWAITING_EXTERNAL_REVIEW`。
