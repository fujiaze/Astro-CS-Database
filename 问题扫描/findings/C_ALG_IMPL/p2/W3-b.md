### W3-R2-004（P2）p1_session_run 仍全局改写 OpenMP ICV 且不恢复；登记锚已漂移（:162-165 → 现 :259-263）
- 位置: lib/phase1_session/p1_session.cpp:259-263；lib/calibration/src/ac_api.cpp:128-134；登记面 docs/contracts/PUBLIC_API.md:687（「budget.max_workers 注入 ac_set_num_threads（p1_session.cpp:162-165，迁移整改点）」）。
- 证据:
  >     // 线程预算注入( 迁移整改点): worker 数=预算快照, 禁硬编码
  >     ac_set_num_threads(static_cast<int>(s->host->budget.max_workers));
- 说明: CONCURRENCY_STANDARD.md:5-6 明令禁「库内随意修改全局 OpenMP setting」。该形态已登记为迁移整改点（非新案），故按「登记在册未整改」定 P2；增量事实两条：①登记锚 p1_session.cpp:162-165 与现行调用点 :259-263 漂移（M6b-E-002 行锚系统性失效家族新实例）；②同仓已存在合规实现并被同族文件采用（P7-UTIL-001 ScopedOmpWorkerInjection module_adapters.cpp:240-262；lib/drizzle/src/module_entry.cpp:915-932 save/restore），P7-UTIL-001 注释 :222-238 逐字记录该形态的实测危害（ICV 污染→§10.5 单活跃线程恒失败）——整改成本一行级，登记后仍未迁移属「修复即扩散」的镜像（修复未扩散到登记点）。
- 建议: 复用 ScopedOmpWorkerInjection 或删行改由 host 注入；同步订正 PUBLIC_API.md:687 锚（path::symbol 口径优先）。
- 置信度: 高。related: M5a-G-006、M3-I-002、M6b-E-002、P7-UTIL-001、DISP-COS-008（同符号的 cosmetic 面登记）
### W3-R2-007（P2）astrocs_process.h 承诺「超时杀进程树」，实现=单进程 SIGKILL；无进程组/Job Object、无 SIGTERM 宽限
- 位置: cli/astrocs_process.h:21、:29；cli/process.cpp:113-115（Win）、:188-194（POSIX）；全仓 setsid/setpgid/CREATE_NEW_PROCESS_GROUP/CreateJobObject 检索零命中（_verify/W3.md §三-5）。
- 证据:
  > //   timeout_s: >0 时超时杀进程树并返回 timed_out; <=0 时不限时(禁止用于生产路径)。
  >             ::kill(pid, SIGKILL);
- 说明: 注释三失实：①「进程树」——POSIX kill(子 pid)，孙进程（harness 拉起的工具）成孤儿继续写盘；Windows TerminateProcess 单进程无 Job Object；fork 路径不 setpgid，连 kill(-pgid) 的前置都没铺。②协作取消（cancel_token.h:3 置位→安全点→exit 9；io_adapter AtomicWriter 析构清 tmp）在 SIGKILL 下全跳过——超时是唯一被承诺的兜底路径，恰是中间态（*.tmp.<pid>.<seq>、.partial）必然残留且仓内无按模式清扫机制的路径。③同仓正对照已存在：ci/run.py:691-696「终止检查进程树（POSIX 杀进程组…）」用 os.killpg(getpgid(pid))——CI 侧会树杀，产品 CLI 侧注释吹了没做。
- 影响: 超时/取消收尾残留污染 run 目录（sha/完整性门可拒判成品，故 P2）；孤儿进程与后续 run 抢资源属「需运行期，未判」。
- 建议: POSIX fork 后 setpgid+TERM→宽限→KILL(-pgid)；Windows Job Object（KILL_ON_JOB_CLOSE）；或注释改「单进程」。
- 置信度: 高。related: W3-R2-002（同一取消/超时家族）、ci/run.py killpg 正对照、机制①（承诺≠实现）
### W3-R2-008（P2）executor.cpp 头注宣称「全仓唯一 executor 池、scheduler 不再自建 std::thread 池」，同树 scheduler 每 run 自建 budget_ 池
- 位置: lib/core/src/executor.cpp:19-20、:39；lib/core/src/scheduler.cpp:339-341；另 lib/phase2/src/upm.cpp:522/618/662/779/1487、lib/phase2/src/sampler.cpp:890、lib/phase3_session/p3_session.cpp:327-333。
- 证据:
  > //   - 无私有池：本文件是全仓唯一 executor 池实现；worker 只在本文件创建，
  > //     scheduler 不再自建 std::thread 池（RT-004 消灭 per-run 池）。
  >   std::vector<std::thread> pool;
  >   for (uint32_t i = 0; i < budget_; ++i) pool.emplace_back([this, &wk, i]() { worker(); });
- 说明: RT-004 注记宣称的形态被现行代码推翻：Scheduler::run 每 run 造 budget_ 线程（不经 ThreadBudget 记账，run 尾 join 回收），与共享池同进程并存（同 run 线程上界≈2×budget；计算线程靠租约钳制、合规性属 ARCH-THREAD-001:9「控制面恒 1 线程」灰区）。第一轮 M5a-G-005 报机器门对 std::thread 不可见（门侧），本条是注释对同树事实的正面否认（文侧），互补不重复；§12.3-8「文档并发模型与真实线程入口一致」在源码注释层的失守。
- 建议: 注释订正为「调度面 per-run 池（join 生命周期、不入租约）+ 计算面唯一共享池」并把 scheduler 池登记进 ARCH-THREAD-001 形态表。
- 置信度: 高。related: M5a-G-005、ARCH-THREAD-001、W3-R2-004（同文件族登记/实现漂移）
### W3-R2-009（P2）p1_atomic_publish 兜底「删目标再 rename」且注释自证不违反原子性；同仓另一处注释明令禁止同型——两文一题互相矛盾
- 位置: lib/core/src/module_adapters.cpp:1183-1205；矛盾对照 lib/astro_image_io/src/hiss_stream_writer.cpp:172-174；另注其合同锚 00_COMMON_CONTRACTS §4.6 全仓不存在（find 零命中，_verify/W3.md §三-7）。
- 证据:
  >     // 兜底（Windows 部分实现 rename 不覆盖已存在目标）: 先删目标再 rename。
  >     // 该窗口内目标短暂缺失, 但任一时刻观察到的都是"旧完整文件"或"新完整文件"，
  >     // 不存在半写状态（原子发布的核心不变式）。
  > 不能先删除旧文件再 rename (避免竞态: 删除后 rename 前若进程崩溃, 文件丢失)
- 说明: ①注释推理有洞：删除后 rename 前目标「缺失」≠「旧完整文件」，消费者读到 ENOENT——把「不半写」偷换成「不缺失」；与 hiss_stream_writer 冻结禁令直接矛盾。②实际危害有界：现代 MSVC filesystem::rename 带 REPLACE_EXISTING，兜底仅 rename 真失败时进；Windows 目标被占用时 remove 同失败→:1197 报错删 tmp（旧文件尚在）；「remove 成功而 rename 再失败」需第三方竞态重建同名，概率低→P2。③W3-R2-001 是同型的每次必进删除路径的强实例（本条是仅失败时进的弱实例+文互矛盾）。④hiss_stream_writer:172 引用不存在的 00_COMMON_CONTRACTS.md——M6b-E-004「不存在文档当合同锚」新实例。
- 建议: 三处（aio_upm/p1_atomic_publish/hiss）收敛到单一发布原语（io_adapter AtomicWriter 已最完整：唯一 tmp 名+失败清理+filesystem::rename）；矛盾注释按裁决改写；悬空锚指现行合同。
- 置信度: 高。related: W3-R2-001、M6b-E-002/E-004、§11
### W3-R2-010（P2）aio_frame_save_cache：tmp 以 _wfopen(UTF-16) 打开、改名却用 MoveFileExA(ACP)——同函数开/提编码口径相反
- 位置: lib/astro_image_io/src/aio_pipeline.cpp:78-95（open_utf8_file: CP_UTF8→_wfopen）、:877-878、:911（MoveFileExA）；POSIX 分支 :917 正常。
- 证据:
  >     int wpath_len = MultiByteToWideChar(CP_UTF8, 0, path, -1, nullptr, 0);
  >         FILE* fp = open_utf8_file(tmp_path.c_str(), "wb");
  >         if (!MoveFileExA(tmp_path.c_str(), path, MOVEFILE_REPLACE_EXISTING)) {
- 说明: 非 ASCII 输出路径下 MoveFileExA 把同一批 UTF-8 字节按 ACP 解释：映射失败→rename 必败（返回 5、tmp 被删、缓存永不落盘的静默降级）；映射偏了→生成乱码名文件而正式路径永不存在。同文件一宽一窄自证。正对照 hiss_stream_writer.cpp:183-186（widen+MoveFileExW）。现可达面=tests（pipeline_frame_contract_test.cpp:56、dataflow_fuzz.cpp:101）+AIO 公开 C API 外部宿主（复算 _verify/W3.md §三-5E）；真实 Windows+非 ASCII 生效属「需运行期，未判」，编码口径矛盾静态已判。
- 建议: MoveFileExW+widen（或 std::filesystem::rename），与 open 侧统一。
- 置信度: 高。related: 簇6（Windows-only，本条第四实例）、M9.md §4 平台盲区、W4-R2-08（同区 tmp 面不同机制）
### W3-R2-011（P2）CON-008 BoundedAsyncQueue 缺 §10.6 五判据中的「超时」；全仓零生产消费者
- 位置: lib/phase2/include/astro/phase2/async_io.h:44-67（push/pop 均无限 wait，无 wait_for/deadline）；宪章 §10.6:381。
- 证据:
  >     // 生产者入队。队列满时阻塞；close/cancel 后返回 false。
  >         not_full_.wait(lock, [this] {
  > 异步队列必须有容量、背压、取消、超时和错误传播；禁止无界队列。
- 说明: 容量/背压/取消/错误传播四项在案，「超时」缺位：消费者僵死时生产者永久阻塞，只能靠外部线程代为 cancel()，调用方无自醒手段；头注宣称对齐 ASYNC_IO_CONTRACT.md 亦不映射五判据。全仓 grep：BoundedAsyncQueue 仅 lib/phase2/tests 消费（_verify/W3.md §三-8），生产链零接线——带缺公开合同头一旦接线即带病上线。
- 建议: push/pop 加 wait_for 重载与超时返回值；头注逐项映射 §10.6；接线前补超时负例。
- 置信度: 高。related: 簇8（登记有合同、实现半具身）、ASYNC_IO_CONTRACT.md
### W3-R2-012（P2）节点 plan 自报 max_workers 只进重节点闸门、不进租约请求：writer 自报 1「不占整份预算」实际按整份申请
- 位置: include/astrocs/core/module.h:54-57（语义声明：供 NodeSpec min/max 使用）；lib/core/src/scheduler.cpp:231-233（min/max 唯一消费点=heavy 布尔）；lib/core/src/module_adapters.cpp:5100-5101、:5113-5116。
- 证据:
  >   // P10-UTIL2-005: 节点自报的 worker 需求（供 Scheduler 的 NodeSpec min/max
  >   // workers 使用，取代 runtime 对所有节点填死的 (1, budget) 占位）。
  >     p.max_workers = (spec_.op == P1NodeOp::Writer) ? 1u : 0u;
  >     const uint32_t host_workers =
  >         ctx.budget() ? ctx.budget()->budget() : workers_;
  >     ThreadLease lease = ctx.acquire_lease(host_workers);
- 说明: 「填死的 (1,budget) 占位」只换了 NodeSpec 字段来源；租约申请端仍全员 host_workers=budget()——独占时刻（concurrent=1→share=budget→hint 不缩）自报 1 的 io 类 writer 拿整份租约持有至 execute 返回。§10.4「按 work unit 申请租约」的申请量语义与自报值脱钩=「被算被记不参与执行」新成员；当前实际危害被 heavy 门与 share 均分兜住→P2。
- 建议: acquire 请求量以 NodeSpec.max_workers 封顶（0=未声明才按预算回退），或把注释与合同语义改口「自报值仅用于闸门」。
- 置信度: 高。related: 簇8（INDEX §三唯一总述，禁止另立）、V21-N-15/N-17（estimated_memory_bytes 恒 0 同属该面的兄弟条）、P10-UTIL2-005
