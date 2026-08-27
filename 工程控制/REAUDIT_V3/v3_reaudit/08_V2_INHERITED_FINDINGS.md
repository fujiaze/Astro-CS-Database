# V2 已确认问题基线

以下问题不得因“V3 未复现”自动关闭。必须由对应 Task 给出修复 commit、测试和外部复核。状态初始均为 FAIL。

| V2 ID | 严重度 | 固定问题 | V3 Task |
|---|---|---|---|
| P0-01 | P0 | Phase2 OpenMP/Dispatcher 生产接线虚假，运行单线程 | CON-001..010 |
| P0-02 | P0 | API checker 的 signature mismatch 分支为空操作 | CHK-002 |
| P0-03 | P0 | API_CONTRACTS 大量字段为全表模板占位 | API-001..005 |
| P1-01 | P1 | UPM Huber、复杂度、OpenMP 文档与实现矛盾 | SCI-004, ALG-003 |
| P1-02 | P1 | Drizzle 单位、面亮度与常量场定义矛盾 | SCI-003, ALG-002 |
| P1-03 | P1 | 63/67 traceability 行缺 ALG leg | CHK-001, REL-001 |
| P1-04 | P1 | AIO 缺 `-fPIC`，Phase2 构建依赖不显式 | BLD-001, BLD-002 |
| P1-05 | P1 | Linux ivar wiring test 硬编码 `.exe` 而失败 | BLD-003, TST-002 |
| P1-06 | P1 | 全测试清单绝大多数未构建/未运行 | TST-001..003 |
| P1-07 | P1 | Orchestrator 无 Linux 动态加载路径 | BLD-003 |
| P2-01 | P2 | Stage2 热循环临时分配设计脆弱；O2 可能优化掉但 Debug 会分配 | CON-005, CON-006 |
| P2-02 | P2 | 99 处 Windows 绝对路径/`.exe` 等可移植性问题 | BLD-003 |
| P2-03/P2-08 | P2 | Drizzle Makefile 硬编码 Windows `--stack` | BLD-003 |
| P2-04 | P2 | ACR 无条件 `windows.h` | BLD-003 |
| P2-05 | P2 | Phase2 测试硬编码 Windows executable | BLD-003, TST-002 |
| P2-06 | P2 | 文档引用不存在 commit | DOC-001 |
| P2-07 | P2 | 文档引用不存在文件/路径 | DOC-001, CHK-005 |
| P2-09 | P2 | forbidden-pattern checker 漏报 hardcoded 16 worker cap | CHK-004 |
| P2-10 | P2 | BASS SHA256 清单含路径/哈希错误 | ID-003, CHK-005 |
| P2-11 | P2 | SNR shared build 混入 `-static` | BLD-004 |
| P2-12 | P2 | Gaia client shared build 缺 `-fPIC` | BLD-002 |
| P2-13 | P2 | PlateSolve Makefile 使用 cmd.exe 与 kernel32 | BLD-003, BLD-004 |
| P2-14 | P2 | Dynamic PSF shared build 缺 `-fPIC` | BLD-002 |
| P2-15 | P2 | 36 个合同 API 无生产实现引用 | API-001..005 |
| P2-16 | P2 | 21 组非平凡重复函数体 | REL-001；仅在不改变科学语义时修复 |
| P2-17 | P2 | 5 个确认失效的文档引用 | DOC-001 |
| P3-01 | P3 | `AstroCS.wiki` 为无 `.gitmodules` 的坏 gitlink | REL-001，需外部决定保留或删除 |
| P3-02 | P3 | 未校准真实帧被 PHOTSCAL gate 拒绝 | 非缺陷；保留为正确科学 gate 测试 |
| P3-03 | P3 | MAD sigma 常数精度不一致 | SCI-002, ORA-001 |

## 额外 V3 审核问题

- V2 返回包的 `00_READ_FIRST.md` 截断。
- findings 数量在 27/29/30 间不一致。
- 状态词包含 `DONE(partial)` 等非法表达。
- Agent 已知单线程后仍运行 A/C 全量。
- B、C-common 和关键 seam 指标缺失。
- 大包先生成、后人工裁剪；没有白名单打包器。

这些是过程门禁问题，不通过修改最终摘要关闭；必须由本控制包的 Checkpoint、状态校验和打包脚本防止再次发生。
