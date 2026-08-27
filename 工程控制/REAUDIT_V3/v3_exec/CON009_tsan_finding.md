# CON-009 TSan + OpenMP 竞态检测 —— 实测结论与方法

## 实测（本机 vm-bj Linux, GCC 14, libgomp)
- 建 TSan 构建: `cmake -S lib/phase2 -B build/linux-tsan -DP2_ENABLE_OPENMP=ON -DCMAKE_CXX_FLAGS='-fsanitize=thread -fno-omit-frame-pointer' -DCMAKE_EXE_LINKER_FLAGS='-fsanitize=thread'`。
- 跑 CON-005 并行测试 (Phase2UpmParallel.OneTvsTwoTDetermine): TSan 报 `data race`, 210 条 WARNING, exit 66。

## 关键：报告的竞态是**假阳性**（不是真实缺陷）
- TRACE: 主线程 `build_impl` (upm.cpp:637) vs OpenMP worker `build_impl._omp_fn.0` (upm.cpp:618/625)。
  - 618/625 是 **w[i] 并行区** (读 m->control_by_id / m->C, 并发只读安全)。
  - 637 是 **M 更新并行区** 的 `#pragma omp parallel for` 指令行 (主线程, 无真实内存写)。
- 这两个区在 build_impl 内是**顺序执行**的独立 `#pragma omp parallel for`; 前一区结束(隐式 barrier/join)后主线程才进后一区, **无真实并发**。
- 根因: **TSan 不跟踪 libgomp(OpenMP) 的 happens-before / 加入点**, 把常驻 worker 线程判为持续活动, 因而误报顺序区边界的并发。

## 权威竞态验证的两种路径
1. **OMPT 背书的 TSan**: GCC libgomp 提供 OMPT 接口, 需 TSan 编译为启用 OMPT 支持并有配套 libgomp; TSan 借此理解 OpenMP 屏障/join, 消除顺序区边界误报。需验证本机 GCC/libgomp 的 OMPT 配合度。
2. **按构造成立 + 正确性测试全绿** (本会话已用): 并行化满足 *整块隔离(每并行单位由单线程完成)+ 仅 max 归约(交换/结合律)+ 不相交写* → 1T/2T **位精确**; 用 1T/2T 确定性 + 结构计数 exact + 浮点容差 测试作为竞态/正确性佐证。已: CON-005 1T/2T + 28 UPM 测试全 PASS。

## 结论/TO DO
- 不把 TSan 假阳性当缺陷误修; 不把 TSan 结论当无竞态绝对证明 (无 OMPT 前提下)。
- CON-009 交付物 = OMPT-TSan harness 或上述按构造论证 + determinism 测试矩阵, 二者其一作为权威依据。
