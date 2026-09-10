# 01｜基线审核结论

审核基线：`789c5b6cec5e7b25f6c13e41c55a0a2e8e905cd9`；结论：**NOT_READY_FOR_RELEASE**。

## P0

| ID | finding | 主要证据 | 必须处置 |
|---|---|---|---|
| GOV-P0-001 | 宪章正文仍 DRAFT，与用户“已冻结”及现行 ACTIVE_NORMATIVE 工程约束冲突 | Constitution:4,651-660；Constraints:1-5；Linux/阈值/worktree 双值见 Constraints:34,47,71,82 | Owner 冻结、四项裁决、supersession 与机器索引 |
| WCS-P0-001 | WCS Oracle 把已经是度的误差再次乘 kRadToDeg，绿测不能证明量纲正确 | tests/unit/p1wcs/p1wcs_tests_oracle.cpp:125-131,249-255；p1wcs_oracle.hpp:31-47 | 独立修 Oracle，再修生产；禁止同提交 |
| WCS-P0-002 | SIP AP/BP 约 42px，却以 <50px 作为 PASS，违反冻结 <1e-4px | p1wcs_tests_oracle.cpp:195-232；docs/science/ASTROMETRY.md:75,108-109,131-134 | 第三方/独立 Oracle、生产修复、恢复冻结门 |
| ARCH-P0-001 | 不同 DAG 节点重复调用完整 Phase Session | module_adapters.cpp:697-806；runtime_client.cpp:92-167 | 三 Phase 分别真实节点化、call_count/副作用验证 |
| PROD-P0-001 | 不完整 Phase 仍写 status=complete | p1_session.cpp:258-440 仅 I/O+calibration+cosmetic；p2_session.cpp:119-247 仅 coverage/sample/UPM | availability/complete fail-closed，链完整后再开放 |

## P1

1. Phase2 只写 signal/support，不写 variance/ivar/rejection，且非原子、properties 无完整 provenance（DATA_SEMANTICS.md:918-991）；Phase3 不确定度目标与旧 SCI 合同存在冲突，须 Owner 科学裁决。
2. PSF batch C ABI 未统一校验 width/height/乘法溢出，测试允许异常终止（dpsf_psf.cpp:513-657；p1psf_tests_core.cpp:643-665）。
3. AIO 唯一边界未成立：astrocs_io 仍是骨架，Phase/HiPS 各自直接写；HiPS abort 留半成品（CMakeLists.txt:110-168；lib/hips/src/module_entry.cpp:928-942）。
4. Scheduler、Phase2、Phase3 多层自建线程池；唯一 executor 未接主构建（sampler.cpp:882-890；upm.cpp 多处；p3_session.cpp:247）。
5. 科学 DLL 已部分构建但 install/product manifest/Runtime loader/verify 未闭环；packaging/astrocs.product.json 仍 alpha.1、SKELETON、null hash。
6. Windows WIN-BUILD/TEST/PACKAGE 均 waivable；候选缺失可 warning 跳过（ci/checks.json:2118-2246；ci-windows.yml:64-79）。
7. 监控 CLI 采集后不调用 evaluate，缺证据或低利用率不会使运行失败（run_monitored.py:445-518）。
8. Fatduck 活跃说明使用旧 pwsh/default key/编译职责，与当前 bash shim、专钥、候选验证节点规则冲突（FATDUCK_ACCESS.md:5-25,31,40-43）。

## P2/P3

- p1hips 临时目录缺 RAII 清理；测试曾填满 /tmp。
- content digest 校验只验 64hex 声明，不重算 storage 内容。
- HiPS writer 的 heavy/io 分类和 worker 计划互相矛盾。
- CLI 缺 phaseN validate/plan/inspect，仍静态链接科学实现。
- Phase3 当前仅 TAN+nearest/bilinear，投影清单、流式输出、uncertainty 待冻结与实现。
- 活跃 L0、module README、product manifest、旧控制包 checkpoint 存在状态漂移。
- 当前 SHA 没有 Linux 全 testdata/Gaia、M42/银心、固定参数预览初审和 Fatduck 同候选证据。

## 审核原则

历史 PASS、旧 SHA、Worker 完成消息都不是当前候选 PASS。每个 finding 必须映射到本包任务、当前 SHA 机器证据与独立复核；P0/P1 不得 waiver。
