# AstroCS V5 预发布审核包(当前现状)

- 版本: `0.9.0-alpha.1`, 当前 main 提交: `ec7a8cdc15661da4d225d75a435b40e4daca71fd` (`ec7a8cdc1566`)
- 状态: **非发布就绪**。`verdict=RELEASE_NOT_READY_BLOCKED`(合法 `AWAITING_EXTERNAL_RELEASE_REVIEW` 未达成)。

## 已收敛
- **87/98** 任务 PASS(含 02 ledger 各 ALG/SCI/ARCH/API/CLI/ISA/BENCH/MON/P3/TRACE/VER/DOC 与 WIN-001..005)。
- WIN-001..005 全 PASS(Windows 单一 CLI 构建/协议/analyze/ASan/取消链路)。
- **WIN-006 里程碑**: 真实银心(T4)数据 phase1 校准 PASS(6 R 帧 + .xisf 母版), 期间修复 2 处真实 Bug(missing 需 XISF 支持; 写校准帧 Windows 栈溢出 0xC00000FD)。输入 hash manifest 已生成(`win006_input_manifest.json`, inputs_sha256=`d0dfd7a1b2743328452772afb66a2ddd9831f7a34ee7fc549557d090f73dc050`)。

## 阻塞项(审核包如实汇报)
- **WIN-006 BLOCKED**: phase2/3 真实数据链需 HIPS 数据集, 但供应链 CLI 无**生产 HIPS 构建命令**(HIPS 仅测试 fixture `phase2_fixture_main` aio_hips_write_signal_support_tile 可造)。已确认:`cli/main.cpp` dispatch 无 hips/drizzle 产线命令。→ 需评审是否补齐 HIPS 产线或改走合成验证。
- **PAR-002 BLOCKED**: 见 FINDINGS/blocker 记录。

## 剩余(未开始)
- WIN-007(32R)/WIN-008(HiPS 接缝)/WIN-009(Windows 发布包)**未开始**(前两者依赖真实 HIPS 链, 后者为当前用户指示'跳过后续'后暂缓)。
- REV-002 REVIEW_PENDING(已提交归档/API 异步审阅胶囊); REV-003(WIN-009 胶囊), REL-001..004(发布审阅)未开始。
- C2..C9 连续检查点未全部达成; 无 alpha 发布物(RELEASE_ARTIFACTS 为空), 无 32R 资源门禁记录(RESOURCE_RESULTS 为空)。

## 结论
当前候选**未达发布门槛**(09 §5 / 10 §5)。LEGITIMATE `AWAITING_EXTERNAL_RELEASE_REVIEW` 不可生成; 本审核包如实记录进展与阻塞, 交外部审阅决策下一步(HIPS 产线 / 合成验证 / 分层放行)。