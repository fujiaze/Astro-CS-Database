### W3-R2-005（P1）provider 租约释放不配对：未 acquire 必 release、acquire(1) 失败被忽略、host_release 无钳制 ⇒ 预算计数器可为负
- 位置: lib/backend_host/host_services.cpp:69-84；providers/cpu/baseline/src/baseline_provider.cpp:418-433、:455-457；providers/cpu/avx2/src/avx2_provider.cpp:244-257、:276-277；providers/cpu/avx512/src/avx512_provider.cpp:243-256、:275-276。
- 证据:
  > int host_acquire(void* ud, uint32_t n) {
  >     // CAS 循环: Σ(active)+n ≤ max_workers 才成功(ARCH-004 §1 Σ≤budget)
  > void host_release(void* ud, uint32_t n) {
  >     static_cast<HostState*>(ud)->active_workers.fetch_sub(static_cast<long>(n));
  >             if (workers == 1)
  >                 (void)host->executor->acquire(host->executor->user_data, 1);
- 说明: ①!(cap>1 && N>1) 时（单像素 kernel 或 cap==1 降级宿主）全部 acquire 被跳过，release 守卫只看指针非空与 workers>0（初值 1 恒真）→ 每次调用净 -1；②:428-429 acquire(1) 返回值丢弃，未授予也在 :457 release(1)；③host_release 无授予校验无 0 钳制 → active_workers 负漂后 host_acquire 放行超额并发，Σactive≤budget（宪章 §10.4「防止…超额订阅」；THREAD_BUDGET_ARCH.md:8）被永久破坏。测试正对照缺失: oracle stub_release=空操作、stub_acquire 不按占用累计拒绝（tests/cpu/baseline/provider_kernel_oracle_main.cpp:43-52）→ 采集面结构性不可见（机制⑪实例）。
- 可达性: run_kernel 在 cli/、lib/ 生产链零调用（复算 _verify/W3.md §三-5E），现可达面=tests/cpu oracle；故定 P1 不抬 P0。
- 建议: release 量=实际 granted；host_release 下界钳制+违约报错；oracle stub 换累积式语义与 host_services 同判据。
- 置信度: 高。related: M5b_L12_L17（THREAD-BUDGET 门对 providers 三处 workers=1 盲区，同文件族）、M5a-G-005、机制⑪
### W3-R2-006（P1）主线程在采样线程存活期内无同步调用 ProcessMonitor::summary() ⇒ 数据竞争（UB）
- 位置: cli/monitor.h:244-320（全类零锁零 atomic：grep -cE 'std::(atomic|mutex)|lock_guard|unique_lock' cli/monitor.h = 0）；cli/commands.cpp:843-853、:858-859。
- 证据:
  >     const int rrc = astrocs::cli::run_pipeline({phase.back() - '0'}, cfg_text, budget,
  >                                                &fail_reason, &first10s_cancel);
  > #if defined(__GLIBC__)
  >     malloc_trim(0);
  > #endif
  >         const auto s0 = mon.summary();
- 说明: 采样线程 commands.cpp:782 while (sampling.load()) 内每 0.5s mon.tick() → monitor.h:249 samples_.push_back（无锁容器突变）；主线程 run_pipeline 返回后 :853 即调 mon.summary()（monitor.h:301/:307 迭代 samples_、读 n_/峰值成员），sampling=false 迟至 :858、join 在 :859。:844→:853 的主线程耗时（malloc_trim 在多 GB 堆上 10²ms 量级）与相邻 tick 交叠即 data race。B13-R13-3 判例（executor.cpp:117-125）给同类读写加了 obs_mu_，监控面漏网。:864/:899/:1046 的后续 summary() 在 join 后，安全。
- 影响: 每 phase run 收尾 progress/resource 观测事件可撕裂（不改科学值，TSan 形态必报——本轮无 TSan 实跑，B-09 边界，静态可判）。
- 建议: 先 sampling=false+join 再取 summary（或加互斥快照）；:853 与 :864 合并。
- 置信度: 高。related: B13-R13-3（同型修复判例）、M5a-G-001/002 观测族、W3-R2-003（同属观测证据链）

# W3（第二轮 · 并发与资源生命周期）· P2 -b 片（7 条）｜HEAD a3a343a4
> 轴档案见 ../../../_verify/W3.md。W3-R2-004 原判 P1，经登记面复核降 P2（PUBLIC_API.md:687 已登记为迁移整改点，非无据违规）。


