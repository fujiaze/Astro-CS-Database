# CHK-001 PLAN — 建立生产可达调用图检查器

## 需求 (04_TASK_SPECIFICATIONS.md CHK-001)
基于 `compile_commands.json`、AST/符号/link map 构建生产可达图，至少覆盖 CLI→Runtime→Module→kernel/I/O。输出 JSON/DOT 与机器断言：
- CLI handler 只能调用 public Runtime/Benchmark/Test/Verify API；
- session/科学内核/I/O内部 symbol 从 CLI 不可达；
- 生产只有一个 scheduler owner；
- ACR symbol/target/module 从默认产品不可达；
- legacy wrapper 只能通过 test registry/preset。
fixture 必须能抓到 CLI 新增一次 `hp_drizzle_run_hips` 直连和 dead Runtime。

## 现状证据
- `cli/main.cpp` 直接 include p1/p2/p3_session.h、hp_drizzle_api.h、aio_fits.h、fitsio.h 并直接调用 `p*_session_*`、`hp_drizzle_run_hips`、`spawn_frame_from_fits`、`fits_read_*`（F-001/F-017/F-018 复现基线）。
- V6 `astrocs_core` Runtime（PipelineIR/ModuleRegistry/Scheduler/RunContext）在 lib/ 与 cli/ 生产代码中无调用（F-006 死代码基线）。

## 影响文件
- tools/quality/check_prod_reachability.py（新）
- evidence/v6_1_rework/tasks/CHK-001/{PLAN.md,TASK_RESULT.json,PROD_REACHABILITY.json,PROD_REACHABILITY.dot,logs/*}

## 科学影响
无（检查器；不动科学代码）。

## 风险
- 当前真实二进制 FAIL 是**预期基线**（正是 RT-008 要修复的状态）；本任务的验收是检查器能抓这些违规 + 负例 PASS。

## 验收命令
1. `python3 tools/quality/check_prod_reachability.py --selftest` → SELFTEST_PASS（伪造 CLI 直连 drizzle 被抓）
2. `python3 tools/quality/check_prod_reachability.py --repo . --binary run/temp/build_v61/astrocs --compile-commands ...` → REACH_FAIL 且逐项列出违规（基线证据）+ 输出 PROD_REACHABILITY.json/.dot（127 节点）
3. nm 证明生产二进制无 ACR 符号
