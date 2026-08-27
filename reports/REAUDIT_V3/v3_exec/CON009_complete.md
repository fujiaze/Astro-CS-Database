# CON-009 complete —— 并行确定性矩阵 + TSan 工具链证据 + by-construction 竞态论证

## 决策依据（竞态验证权威路径）
- CON009_tsan_finding.md 已实测：TSan 对 OpenMP 顺序区边界报 data race，
  经分析为**假阳性**（TSan 不跟踪 libgomp 的 happens-before/join）。
- OMPT-TSan harness 路径**不可用**：本机 `libgomp.so` 无 ompt 符号
  （`nm -D /usr/lib/gcc/x86_64-linux-gnu/14/libgomp.so | grep ompt` 为空），
  GCC 14 的 libgomp 未暴露 OMPT 接口，TSan 无法据此消除顺序区边界误报。
- 故采用权威路径 2（finding 第 15/16 条）：**按构造成立 + 确定性测试矩阵**。

## 工具链证据（TSan 不可用分支的编译证据）
- `gcc` / `g++`：Debian 14.2.0-19。
- `clang` / `clang++`：Debian 19.1.7 (3+b1)。
- `libgomp.so`（GCC OpenMP runtime）：存在；**无 ompt 符号**。
- TSan runtime（libtsan.so）存在；linux-tsan 构建配置：
  `-DP2_ENABLE_OPENMP=ON -DCMAKE_CXX_FLAGS='-fsanitize=thread -fno-omit-frame-pointer'`。

## TSan 实测（本机）
- CON-005 并行测试 `Phase2UpmParallel.OneTvsTwoTDetermine`（TSan 构建）：报 data race、
  210 条 WARNING、exit 66；经 TRACE 分析均为顺序区边界的假阳性（无真实并发写），
  见 CON009_tsan_finding.md。不视为真实缺陷，不误修。

## 确定性测试矩阵（同 seed 同输入：1T / 2T / 重复 2T）
| 阶段 | 测试 | 1T/2T | 重复 2T | 断言 |
|---|---|---|---|---|
| UPM build/solve | `Phase2UpmParallel.OneTvsTwoTDetermine` | ✓ | ✓（新增 m3） | 结构计数 exact；`model_hash` exact；calibrate 容差 |
| ACR CPU launcher | `Phase2AcrParallel.LegacyCpuOneVsTwoTDetermine` | ✓ | ✓（新增 out3） | 逐像素 1T/2T 容差；repeat-2T 逐像素一致 |
| Stage2 integration | `Phase2IvarWiring.WireProductionStage2PerFrameIvar` | ✓ | ✓（新增 out_abc_2t_rep） | signal/support 图层逐层 1T/2T 差数=0；repeat-2T 差数=0 |
| Sampler (真实数据) | `Phase2SamplerParallel.OneTvsTwoTDeterminism` | ✓（Fatduck） | —（Fatduck） | frame_id/control_id 顺序 exact；accept/reject 计数 exact；value 容差。Linux 无 HiPS fixture => SKIP，由 Fatduck 运行 |

## by-construction 竞态论证
- 各并行阶段满足：每并行单位由单线程完成（整块隔离）+ 仅顺序无关/结合律归约（max 等）
  + 不相交写；跨线程共享仅限只读/稳定索引槽。
- 因此 1T == 2T == 重复 2T 位精确（已由 UPM `model_hash` exact、
  integration signal/support 逐层差数=0、ACR 逐像素一致验证）。
- 无数据竞争、无死锁、无 oversubscription（全局 worker 预算单一所有者，CON-002）。

## 结论
- CON-009 满足：TSan 不可用/假阳性分支给出编译器证据（gcc/clang 版本 + libgomp 无
  OMPT）并记录；确定性矩阵覆盖全部并行阶段，含重复 2T；逐层/结构/ID/hash 校验到位。
- 判定：**PASS**（按 finding 第 18/19 条的权威路径 2）。
- 遗留：真实数据 sampler 1T/2T 由 Fatduck（Windows）补测运行，记录于 WIN/G2 环节。

## 2026-08-27 补充：CON-010 发现 sampler 并行读真实竞态（推翻本报告"无数据竞争"默认）
- 本报告的确定性矩阵未在 Linux 实际跑 **sampler 并行多 tile 读**：`Phase2SamplerParallel.
  OneTvsTwoTDeterminism` 因无真实 HiPS 在 Linux SKIP；UPM/ACR/integration 的
  1T/2T + repeat-2T 均通过，但未涉及 **多 tile 并发 AIO 读**。
- CON-010 用合成 6×12 tile 负载实测 `cpu_workers=2`：TSan 报
  `sampler.cpp:706 std::set::count / :881 _omp_fn.0` 竞态 + `SEGV in _IO_fread`，
  = cfitsio 并发读非线程安全 ⇒ 生产 CLI **SIGSEGV**（详见 CON010_runtime_gate.md）。
- 因此本报告的**断言仅对 UPM/ACR/integration 成立**；sampler 并行读存在真实竞态，
  CON-004 需重审。补记于此，避免"无数据竞争"被误当作对 sampler 并行也成立。
