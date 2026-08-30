# AstroCS V5 审核包(当前现状) — 汇总与诚实 verdict

- 生成时间: 2026-08-30; 当前 main: `b8ce36691e0b6ef31a9a31814f711f09b48a2263`; 版本 `0.9.0-alpha.1`。
- 审核包: `artifacts/prerelease_v5/AUDIT_REVIEW/`(whitelist, 216K, package_final `PACKAGE_SELECTION_PASS`)。
- 复现: `python3 tools/assemble_audit.py`。
- 校验: `python3 工程控制/.../scripts/validate_final_package.py artifacts/prerelease_v5/AUDIT_REVIEW` → **FINAL_PACKAGE_FAIL**(如实)。

## 数据(02_TASK_LEDGER.csv, 98 任务)
- **PASS 87**; BLOCKED 2(PAR-002, WIN-006); NOT_STARTED 8(WIN-007/8/9, REV-003, REL-001..004); REVIEW_PENDING 1(REV-002)。
- COMMITS.csv 89 条(push_status 规范化为 PASS), 2 条非 PASS: WIN-006(BLOCKED)、REV-002(REVIEW_PENDING)。

## 校验器列出(未达 `AWAITING_EXTERNAL_RELEASE_REVIEW` 的原因)
1. non-PASS tasks: PAR-002, REV-002, WIN-006, WIN-007, WIN-008, WIN-009, REV-003, REL-001..004。
2. invalid commit row: PAR-002(BLOCKED)、REV-002(REVIEW_PENDING)。
3. missing PASS Linux/Windows alpha artifacts(RELEASE_ARTIFACTS 为空; 无任何平台 alpha 包)。
4. invalid final verdict(verdict=RELEASE_NOT_READY_BLOCKED, 非 AWAITING_EXTERNAL_RELEASE_REVIEW)。
5. missing windows_32r_run_id(32R 未跑)。

## 核心阻塞(审核包如实汇报)
- **WIN-006 BLOCKED**: phase1 真实银心(T4)校准 PASS(6 R 帧 + .xisf 母版), 修 2 处真实 Bug(XISF 支持缺; 写校准帧 Windows 栈溢出 0xC00000FD), 输入 hash manifest `d0dfd7a1...`。但 phase2/3 真实数据链需 HIPS 数据集, 供应链 CLI **无生产 HIPS 构建命令**(仅测试 fixture 可造 `aio_hips_write_signal_support_tile`); 已确认 `cli/main.cpp` dispatch 无 hips/drizzle 产线命令。
- **PAR-002 BLOCKED**: 见 FINDINGS/blocker 记录。

## 结论
当前候选**未达发布门槛**。合法 `AWAITING_EXTERNAL_RELEASE_REVIEW` 不可生成。交外部审阅决策: 补齐 HIPS 产线 / 改走合成验证 / 分层放行。
