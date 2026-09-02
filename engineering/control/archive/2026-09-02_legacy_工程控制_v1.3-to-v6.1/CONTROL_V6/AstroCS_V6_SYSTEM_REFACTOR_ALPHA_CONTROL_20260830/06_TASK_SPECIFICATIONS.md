# 逐任务执行规格

本文件把 `05_TASK_LEDGER.csv` 展开为不可自行发挥的动作。所有任务均使用 `04` 的固定执行模板；每个任务证据目录固定为 `evidence/refactor/tasks/<TASK_ID>/`，至少包含 `TASK_RESULT.json`、执行命令、stdout/stderr 日志、测试报告和变更文件清单。

`TASK_RESULT.json` 必须写：task_id、start_commit、end_commit、source_commit、control_hash、status、change_class、science_changed、files_changed、commands（含 timeout）、tests、metrics、findings、evidence_sha256、UTC。不得只写自然语言 PASS。

## G0：只调查，不先“修到看起来正常”

### BAS-001

- 运行 `git remote -v` 的脱敏采集、`git status --porcelain=v2`、`git rev-parse HEAD/main/origin/main`、submodule/LFS 检查。
- 登记所有已有修改的路径、来源和是否允许保留；绝不覆盖用户修改。
- 验证本包 `SHA256SUMS` 与 `validate_control.py`。
- 输出 `REPOSITORY_IDENTITY.json`、`EXTERNAL_CHANGES.md`。
- 若三 SHA 不同或 remote 含凭据，状态 `BLOCKED`；不得自行 reset/rebase。

### BAS-002

- clean configure 当前源码，但不做耗时科学运行。
- 从 CMake file-api、link command、symbol table 生成 target/dependency/entry 清单。
- 必须覆盖 `astrocs` CLI、orchestrator、stage2、phase sessions、AIO PipelineEngine、ACR registry。
- 输出 `BUILD_TARGET_GRAPH.json/.dot`、`PRODUCTION_ENTRY_INVENTORY.csv`。
- 任何“正式入口”必须有可执行路径证据；文档声称不算。

### BAS-003

- 用 clang tooling/call graph + `rg` 盘点 `std::thread/jthread/async/OpenMP/TBB/omp_set_num_threads/cpu_workers/worker_count/mutex`。
- 对 `main.cpp`、`p1_session.cpp`、`p2_session.cpp`、`p3_session.cpp`、`orchestrator.cpp`、`aio_pipeline_engine.cpp`、`stage2.cpp` 生成调用图。
- 输出 `SCHEDULER_INVENTORY.csv`：owner、创建线程位置、预算来源、嵌套关系、是否生产可达。
- 输出 `SERIAL_HEAVY_FINDINGS.csv` 和 `GLOBAL_LOCKS.csv`。

### BAS-004

- 为 P0-001..007 做最小静态/动态复现；不修代码。
- 对 `ImageDeleter` 用 10–100 次小图循环证明泄漏路径；若现有构建阻止动态证据，至少用所有权调用图并标 NOT_RUN。
- 对 Phase1/2 输出实际 runtime stage trace，与文档 stage 列表做差集。
- Phase3 记录输出 header 的版本、run id、BUNIT 来源。
- 每项绑定后续 Ledger Task，不允许登记后无人负责。

## G1：先建立“什么才是正确”

### VER-001

- 根 `VERSION` 写 `0.10.0-alpha.1`；CMake 从它生成 version header/resource；CLI、manifest、artifact provenance、文档 status 均读取生成值。
- 删除源码中的产品版本字面量；第三方版本不动。
- 测试 `astrocs version --json`、二进制元数据、发布 manifest、L0 文档四者一致。

### DOC-001

- 创建 `docs/contracts/INDEX.yaml`，字段严格使用 schemas 中的 contract index 约束。
- 把现有 SCI/ALG/DATA/ARCH/API/MOD/TEST 文档逐项登记为 ACTIVE/DRAFT/OBSOLETE/CONFLICT，不得批量假设 ACTIVE。
- 检查每个 ID 唯一、路径存在、owner 非空、上游/下游引用存在。
- 旧审计编号不作为永久科学合同 ID。

### SCI-001

- 按 Phase1 每一步写对象、公式、输入单位、输出单位、假设、无效值、边界和科学不变量。
- Calibration 明确 raw/bias/dark/flat、dark scale、flat normalization、gain/read noise、负值保留；禁止 pedestal/clamp 偷渡。
- PlateSolve/Photometry/SNR/Drizzle 写明失败时是否允许继续及数据质量标记。
- Drizzle 说明 drop overlap、support、weight、flux/constant-field 不变量、FP64 累积与 FP32 产品。
- 只写科学定义，不在本 commit 改实现。

### SCI-002

- 覆盖 overlap/control point、additive UPM、正则化/gauge、断连覆盖图、rejection、integration、输出 support/variance。
- 接缝只允许加性校正；背景模型与叠加权重分开；源/亮星通过 mask/tolerance 排除。
- 明确 UPM 不能制造未覆盖像素、黑洞或跨真实天体结构的过拟合。
- 每一种 rejection 的适用条件、样本数下限、统计量和 deterministic tie rule 明确；自动选择不是黑箱 heuristic。

### SCI-003

- 把 Phase3 定义为 HiPS/HEALPix 球面采样到用户指定平面 WCS FITS。
- 明确 output pixel center→WCS sky→HEALPix sampling 的方向；插值核、support、coverage、RA 0/360、极区、无效值、单位传播和误差。
- 不允许默认 `Jy/beam`；若单位未知则保留 UNKNOWN 并拒绝需要物理单位的转换。
- 现有 prototype 只能作为反例/参考，不是设计权威。

### DATA-001

- 创建 DataArtifact schemas：raw/calibrated image、variance、inverse variance、quality weight、support、mask、WCS、PSF catalog、photometry catalog、HiPS dataset、UPM model、rejection map、planar FITS。
- 每个 schema 写 scalar、shape、axis order、unit、coordinate、invalid、ownership、serialization 和 version compatibility。
- 对现有 `weight/scale/sigma/value/snr` 字段逐个映射或登记 ambiguity；未消除 ambiguity 不得进 G2 consumer API。

### ARCH-001 / API-001

- 按 `03_TARGET_ARCHITECTURE.md` 写唯一职责图和依赖方向。
- 公共 C++ API 只在同一 toolchain 边界；CPU DLL/so 使用 C ABI。
- 明确 error/result、allocator ownership、thread-safety、lifetime、cancellation、reentrancy。
- API 文档列真实 header/symbol；此阶段可先写 expected signature，后续 AST 必须锁定。

### TEST-001

- 为每个 SCI/ALG ID 创建至少一个 test ID；区分 analytic oracle、high-precision reference、property、metamorphic、boundary、parallel/backend equivalence。
- 容差包含 abs/rel/ULP 或统计置信界、数据范围和理由；不得运行失败后再改阈值。
- 定义影响触发：普通内部重构只跑模块+downstream；科学/单位/拓扑/backend/platform 变化扩大验证。

## G2：Runtime 基座

### BLD-001

- 建 root CMake；显式 `astrocs_contracts`、`astrocs_runtime`、`astrocs_io_*`、每个 module、每个 CPU provider、`astrocs_cli` target。
- options：`ASTROCS_ENABLE_ACR=OFF`、`ASTROCS_BUILD_TESTS=ON`、`ASTROCS_ENABLE_SANITIZERS`。
- 禁止 production `file(GLOB)`、目录级 `-w`/`/w`、全程序 `/arch:AVX2`/`-mavx2`。
- 输出 CMake graphviz 与 link scan 测试。

### CORE-001

- 实现稳定 error domains：CONFIG、DATA、SCIENCE_PRECONDITION、IO、RESOURCE、BACKEND、CANCELLED、INTERNAL。
- module 不调用 exit/abort；异常只在内部转 Result，不能穿过 C ABI。
- 测试 nested cause、serialization、cancel、错误退出码映射。

### CORE-002

- 实现 descriptor 与 RAII handle；Artifact ID 不由文件名代替。
- provenance hash 必须稳定且排除易变显示字段；包含真实 commit/build/config/input。
- 测试所有权移动/共享、schema mismatch、单位 mismatch、序列化 roundtrip。

### CORE-003

- registry 拒绝重复 ID、ABI 不兼容、缺合同、缺端口、CPU-heavy+serial 描述。
- 从 registry 生成 `MODULE_INDEX.json`，不手写第二份清单。
- 模块 factory 只注册，不执行 I/O/benchmark/线程创建。

### CORE-004

- parser 只接受 `astrocs.pipeline/v1`；未知字段默认报错，除非 schema 明示 extension。
- DAG validator 完成类型、单位、坐标、producer、cycle、必需端口校验。
- negative fixtures 至少覆盖 10 类错误；验证信息必须指向 node/port/contract。

### CORE-005

- RunContext 按 `03` 实现接口与 mock；所有服务有 lifetime 和 thread-safe 注释。
- 模块测试可注入 in-memory ArtifactStore、fake clock、deterministic executor。
- 禁止 singleton service locator。

### CORE-006

- 实现唯一固定 worker pool + 有界 I/O executor；线程数基于配额与 profile，不直接用裸 hardware_concurrency。
- `ThreadLease` 防止嵌套过订阅；不同 frame 与节点依赖可调度。
- 用合成 sleep/CPU/memory nodes 验证并行、backpressure、公平、失败传播和取消；所有等待有 timeout。

### CORE-007

- checkpoint 只引用已提交 Artifact；记录 node config/module/input hashes。
- hash 变更自动失效 downstream；故障注入模拟写中断/进程中断。
- 恢复不得重用半成品或跨 commit 证据。

### CORE-008

- JSONL event schema固定：run/node/artifact/time/sequence/severity/type/metrics。
- 每 node planned/queued/started/progress/completed/failed/cancelled 完整；sequence 单调。
- stderr human log 可读，stdout `--json` 纯机器数据；敏感路径/凭据脱敏。

## G3：I/O、CPU 后端、监测

### IO-001 / IO-002

- I/O adapter 不 include Runtime scheduler 或模块实现。
- Artifact transaction 使用同目录临时文件、close/verify/rename；Windows rename 失败给确定错误。
- 全部 `aio_image*` 搜索并归类 owner；替换为 canonical deleter；不允许 `free(aio_image*)`。
- LSan 用不同 bit depth、keywords、data_f64 反复读写；报告 allocation/free 差额为 0。

### IO-003 / IO-004

- CFITSIO 构建显式 reentrant；`doctor` 现场调用 `fits_is_reentrant()`，不能只看编译 flag。
- 每 worker 建独立 reader/dataset，并独立打开同一 tile；不得共享 `fitsfile*`。
- 运行 2/4/8 worker（按机器可用配额）重复随机 tile 读取压力；hash 必须一致，ASan/TSan 无错。
- 只有通过后删除 `g_aio_mu`；若仍崩溃，定位 handle/缓存所有权，禁止保留全局锁后宣称并行完成。

### CPU-001..005

- C ABI 结构带 size/version；DLL 由 CLI/Runtime 验证，不把 STL、异常或 caller allocator 所有权跨边界。
- baseline 只要求 AMD64 基本能力，但 heavy kernel 必须可用 Runtime lease 多线程。
- AVX2/AVX-512 只实现 profile 证明为热点且向量化有意义的 kernel；每个 provider 导出相同 kernel ID/semantics。
- CPUID 后必须核验 OSXSAVE/XGETBV；AVX-512 核验所需具体子集，不以“CPU 名称”推断。
- 测试伪造 feature、损坏 provider、ABI mismatch、缺 kernel 和优雅 fallback。

### CPU-006 / CPU-007

- benchmark 顺序固定：能力探测→Oracle correctness→线程扩展→各 ISA→block size→内存带宽→profile。
- quick 使用小型代表 kernel；3 次 warmup + 7 次短测，报告 median/MAD；full 也不得调用 32R。
- 每 kernel 单独选择 provider/thread/block；AVX-512 若实际不快于 AVX2 的 3% 则不选。
- profile 含 CPU signature、OS、quota、compiler、binary/provider build IDs、benchmark version、UTC、结果 hash；任一关键字段变化失效。
- 未运行/无效 profile 使用 baseline provider + Runtime 自动线程，发一次明确 warning，不退化为单线程。

### CPU-008

- Linux 采 `/proc`/cgroup/process counters；Windows 使用可用系统 API/计数器；不得要求用户手看任务管理器。
- 每 1s 采样并记录 process/system CPU、active workers、RSS/commit、page faults、read/write bytes、queue depth、lock/wait time、模块 progress。
- heavy window 去除初始化/收尾后计算 mean/p50/p95；CPU p50<90% 或 mean<85% 自动 FAIL，除非形成严格 memory-bound evidence。
- 监测器开销单独 benchmark，目标 <2% wall time；无法满足则调低采样频率并记录。

## G4：Phase1 迁移

### P1-001

- 建 old-symbol→module-ID→input/output→SCI/ALG/DATA/TEST 表；任何未映射旧能力标 blocker。
- adapter 只转换数据和错误，不复制算法。

### P1-002

- 输入、校准、美容拆成模块；统一 AIO Artifact ownership。
- synthetic：constant bias/dark/flat、gradient flat、negative result、NaN/mask、u16/f32/f64；用解析公式逐像素比较。
- 并行只用于大图；small fixture可走串行阈值，但生产重计算必须通过 lease。

### P1-003

- Star/PSF 使用孤立 Gaussian/Moffat、重叠星、饱和星、边缘星、纯噪声；检测 completeness/false positive、centroid、FWHM/ellipticity。
- 去重规则稳定且有 tie breaker；catalog 带坐标/单位/质量字段。

### P1-004

- PlateSolve 用已知 WCS 合成星场和扰动初值；验证 pixel↔sky roundtrip、残差和失败条件。
- Photometry 用已知 flux/背景/PSF；原先“解析成功但积分失败”必须有回归 fixture。
- 失败不能留下貌似有效的空 catalog。

### P1-005

- Noise/SNR 必须从 SCI 公式生成 reference；Poisson+read noise Monte Carlo 固定 seed，另有解析均值/方差。
- variance 与 inverse variance 不混；blank sky、低/高信号、负值、零 gain、无效 read noise 覆盖。

### P1-006

- NSIDE 由输入尺度/过采样合同计算，不硬编码 2048。
- Drizzle fixtures：常数场、单 impulse、subpixel shifts、rotation、不同 pixfrac、边界/RA wrap、mask/zero support。
- 验证 flux/constant invariance、centroid、support、权重传播；accumulator FP64；provider结果按 TEST 容差。

### P1-007..009

- writer 写合法 HiPS properties/tile tree；verify 重新打开随机/边界 tile 和 hash。
- canonical Phase1 IR 必须含文档声明全部 node；功能关闭通过 IR 显式 preset。
- 运行 trace 与静态图逐节点一致；`p1_session` 只作 Runtime facade。
- 2 核 heavy synthetic 记录资源；ASan/LSan 循环多 frame。

## G5：Phase2 迁移与接缝修复

### P2-001 / P2-002

- 完整 old Stage2 每步骤映射；把 ACR call 标为禁止迁移依赖。
- coverage/sampler 使用 deterministic chunk/worker-local reader；删除 `cpu_workers=1`。
- 同一 seed 和 input 在 1 worker 测试参考与 N worker 结果在容差内；生产 heavy 配置禁止选择 1。

### P2-003

- 合成三块重叠面：已知常量、线性、平滑低阶加性背景 + 恒星/扩展结构；控制点 mask 排除亮源。
- 建 pairwise difference 方程、权重、regularization 和 gauge（例如固定一块/零和约束）；断连 graph 明确分量行为。
- 评估 overlap median/RMS/gradient before-after、源 flux ratio、非重叠结构变化；接缝下降且源不被拟合。
- 禁止乘性校正，禁止在 zero-support 生成值，禁止通过过度平滑隐藏接缝。

### P2-004

- block plan 由预算生成；不得构造 35GB 稠密全局 cache。
- 小数据 block 与 full reference 等价；边界无重复/遗漏；峰值 RAM 符合 plan 误差界。

### P2-005

- 每种 rejection 独立 fixture；自动选择输出明确 reason code。
- cosmic ray、hot pixel、satellite streak、真实星核、低样本数覆盖；拒绝图与计数输出 Artifact。
- integration 验证 mean/weighted mean/variance/support；frame identity 不丢失。

### P2-006..008

- writer 输出 mosaic、support、实际权重类型、UPM surface、rejection diagnostics；名字不含模糊 `weight`。
- canonical IR 全链；`p2_session` 只作 Runtime facade。
- synthetic seam 的数值门和资源门同时通过；不得仅凭预览“看起来没缝”。

## G6：Phase3 正式开发

### P3-001

- 从 production registry/preset 移除现有 prototype；文档状态统一为 NOT_IMPLEMENTED/PROTOTYPE。
- 保留源码供定向参考，不做破坏性删除。

### P3-002

- 输入 HiPS properties/NSIDE/order/frame/unit，输出 WCS/dimensions/pixel scale/kernel计划。
- 所有尺寸乘法做溢出检查；最大尺寸来自资源/配置合同，不硬编码 20000。
- WCS fixture 覆盖 TAN 等正式支持投影、旋转、RA wrap、南北极附近。

### P3-003 / P3-004

- 按 output tile 分工；每 tile 独立写 buffer，最后单 writer/有序提交；无共享像素 data race。
- 1-thread reference 仅测试可用；production large workload 从 Runtime 取≥2 workers。
- constant sphere、解析纬经函数、impulse/point source、边界缺失、coverage mask 验证插值与几何。

### P3-005 / P3-006

- FITS header 从 plan/DataArtifact/provenance 生成；版本/run_id/output path 不硬编码。
- 重新打开验证 dimensions/WCS/BUNIT/checksum/mask；pixel→sky→sample 对照 Oracle。
- 全部合同通过后才把 registry status 改 IMPLEMENTED。

## G7：唯一生产路径

### CLI-001..003

- 把 1500 行 `main.cpp` 拆为 parser、command handlers、output formatter；handler 只调用 public Runtime/Benchmark/Test API。
- `run --phases` 改为 preset/IR，不手工顺序调用 sessions。
- `test synthetic` 必须真正运行 test registry；未知 group 返回 2，不落 stub。
- 所有网络/远程/外部进程参数必须 timeout；长任务持续输出事件。

### LEG-001..004

- CLI drizzle 变为标准 Pipeline preset 或测试 wrapper，不手工重读 FITS header。
- 旧 Orchestrator/AIO PipelineEngine/old Stage2 逐个确认没有 canonical caller、链接符号、文档入口、安装产物后退出。
- 删除调度职责可保留底层有效函数；不要为了删除旧文件重写算法。
- ACR target 迁入 dormant/optional；默认构建 link map 和运行 module list 证明无 ACR。

## G8：文档和质量机器门

### DOC-002 / DOC-003

- L0 固定：`REVIEW.md`、`docs/review/SCIENCE_OVERVIEW.md`、`PIPELINE_OVERVIEW.md`、`ARCHITECTURE_OVERVIEW.md`、`RELEASE_STATUS.md`、`CHANGE_REVIEW.md`。
- 每模块 README 使用模板，必须链接合同/真实 header/source/test，不抄完整公式。
- owner 只看 L0；底层 Agent 维护 L1/L2/L3。

### DOC-004 / DOC-005

- Clang AST 输出 public symbols；与 API index 做参数、cv/noexcept、visibility、ownership annotation 对比。
- 验证每个 production module 存在 registry、CMake target、README、SCI/ALG/DATA/API/TEST。
- 静态 IR graph 与 runtime trace 比较 node/edge/ports/module version/artifact producer-consumer。
- negative fixture 必须能抓到故意改错函数名、端口、合同引用和 trace 缺节点。

### QA-001..005

- 移除项目源码 blanket warning suppression；第三方 warning 隔离在 SYSTEM/target 范围。
- 静态禁令扫描必须基于生产 reachability，不只 grep；allowlist 写 owner/reason/expiry。
- Sanitizer 运行最小但覆盖所有所有权/并发路径；不能用宽泛 suppression 清屏。
- 生成重复 scheduler/I/O/WCS/weight/配置解析报告；每一重复实现给 KEEP/MIGRATE/DELETE。
- 依赖锁定、license、编译器、flags、SBOM、build id 可复现。

## G9：Linux 控制节点

- 所有命令使用 timeout；2c2g 机器限制并发，避免 OOM，但 heavy synthetic 必须真实使用两个可用核。
- GCC Release 与 Clang Debug 分开 build dir；不提交 build。
- 合成测试按模块/影响链执行；不要拿 32R 到 Linux 反复跑。
- 从 Fatduck 只拷贝 manifest 指定的少量帧和 masters；重算 SHA；失败/离线不阻塞其它 Linux Task。
- cancel/resume、资源回压和低内存情形必须测试。

## G10：Windows 计算节点

- 每次 SSH/远程命令显式 connect/command/idle timeout，日志实时回传；在线前先做只读探测。
- MSVC clean build 后执行同一 test registry；Windows 专属脚本不得成为算法真相源。
- benchmark 生成 Fatduck profile；AVX provider 选择按 kernel，不按 marketing 名称。
- 小真实先验证，再冻结 candidate；32R 只跑最终 candidate 一次成功全链。
- 32R 清单严格 11+11+10，输入 hash、每帧贡献、输出 Artifact identity、资源曲线可查。
- 固定 HiPS viewer 坐标/FOV/stretch；保存机器配置和小型截图/指标，不打包整个 HiPS。

## G11：发布

### REL-001

- 发布布局仅有一个用户入口；CPU provider 是内部组件；baseline 始终随包。
- Linux/Windows 包含 VERSION、default pipeline IR、schemas、license、README、checksums；不含 build/testdata/history。
- 在干净临时目录完成 install/run smoke。

### REL-002

- 重新抽取 API/module/pipeline/version，生成 final traceability；扫描旧入口、旧版本、Windows-only 主流程、Phase3 假状态。
- `RELEASE_STATUS` 对任何未通过项写 NOT VERIFIED/FAIL，不得用“基本完成”。

### REL-003

- 只用 `scripts/package_audit.py` 白名单打包；运行 `validate_audit.py`；解包后再验 SHA。
- 源码快照只包含当前相关源码和文档，不含 `.git`；大产物写 manifest+hash+路径+生成命令。

### REL-004

- 提供固定视图：三块 overlap、最弱背景、亮星区、卫星线区、support 边缘、Phase3 FITS。
- Agent 只能给 `READY_FOR_OWNER_REVIEW`；负责人明确批准后才写发布决定。
