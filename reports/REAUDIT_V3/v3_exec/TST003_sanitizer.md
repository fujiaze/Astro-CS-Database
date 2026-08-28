# TST-003 ASan/UBSan/TSan 核验 —— 记录

> TST-003: ASan+UBSan 运行核心单元/合成端到端/错误路径; 并行代码运行 TSan 竞态验证。
> PASS 需 0 sanitizer error / 0 data race / 0 leak; 明确第三方豁免需外部批准。
> 状态：**IN_PROGRESS**（ASan/UBSan 地址/UB 与 TSan 竞态均 0 error; 但 **LSan 被 ptrace 环境
> 阻断** + **synthetic_gate 60s 看门狗超时**，需外部批准或修复）。
> 复核：2026-08-27。

## 1 ASan+UBSan（`build/linux-debug-sanitize`, `-fsanitize=address,undefined`）

| 测试 | 结果 | 注 |
|---|---|---|
| phase2_execution_options | **PASSED 6**（exit 0） | 无地址/UB 错误 |
| phase2_routing | **PASSED 4**（exit 0） | 无地址/UB 错误 |
| phase2_async_io | **PASSED 10**（exit 0） | 无地址/UB 错误 |
| phase2_ivar_wiring | **PASSED 1**（exit 0，从仓库根跑） | 无地址/UB 错误 |
| phase2_synthetic_gate | **超时**（内部 60s 看门狗） | ASan 较慢超时；Release 通过 |

- 全部 ASan 日志无 `AddressSanitizer`/`runtime error:`/`LeakSanitizer`/`ERROR:` 行 ⇒ **0 地址/UB 错误**。
- **LSan 环境限制**：`LeakSanitizer does not work under ptrace`（会话 sandbox 在 ptrace 下运行），
  泄漏检测不可用 —— 属**外部环境限制**，需外部批准豁免或移出 ptrace 后重跑。

## 2 TSan（`build/linux-tsan`, `-fsanitize=thread`, 链 `libtsan.so.2`）

| 测试 | 结果 | 注 |
|---|---|---|
| phase2_async_io | **PASSED 10**（exit 0） | 无 ThreadSanitizer 竞态警告 |
| phase2_sampler_parallel | `[RUN] OneTvsTwoTDeterminism`（exit 0） | 无竞态警告；gtest 汇总 0（非标准 gtest 计数） |

- 无 `WARNING: ThreadSanitizer`/`data race` ⇒ **0 数据竞态**（并行/异步测试）。
- 验证 TSan 确实生效：`libtsan.so.2` 链接、`-fsanitize=thread` 编译。

## 3 结论

- ASan+UBSan 地址/UB 检测：**0 error**（核心单元 execution_options/routing/async_io/ivar_wiring）。
- TSan 竞态检测：**0 race**（async_io 10 用例；sampler_parallel 确定性命）。
- **阻塞/待豁免**：
  1. **Leak 检测**：LSan 在 ptrace 下不可用（外部环境限制）—— 待外部批准或移出 ptrace 重跑。
  2. **synthetic_gate**：ASan 下 60s 内部看门狗超时（合成端到端未在 sanitizer 下完成）—— 待
     放宽超时/或分片运行。
- 因此 TST-003 暂判 **IN_PROGRESS**（主体 0 error/0 race，但 0 leak 未达成，需外部批准）。
