# EVIDENCE_INDEX: P00-006

## 证据目录
`engineering/evidence/P00-006/`

## 证据清单

| 文件 | SHA-256 | 说明 |
|---|---|---|
| audit_reconciliation_P0P1.json | 3ea5b47f8f0e00b9cd72643e6ab375c99735cf02e98585a7e201ff91904f820c | P0P1 (50 项) 子 Agent 复核数据 |
| audit_reconciliation_P0P1.md | — | P0P1 人类可读报告 |
| audit_reconciliation_P2.json | 438726af9b18b35c396b36cdacac7e1e640c6ac8b53f580df570c3c721187af7 | P2 (54 项) 子 Agent 复核数据 |
| audit_reconciliation_P2.md | — | P2 人类可读报告 |
| audit_reconciliation_P3.json | 00c3dc9c61c9792bfd039905b35ba2d11915d7d7ea75fe48e7508d7963e3c68c | P3 (59 项) 子 Agent 复核数据 |
| audit_reconciliation_P3.md | — | P3 人类可读报告 |
| merge_audit.py | e120f142fdddbaa0ba37cbb78aea836fabe1825a74443767fd35159602af014d | 合并脚本 |
| audit_reconciliation.json | 5f8cb9eccb38020f36e25343f89145ad78975cc7138ba1be0322708c477bd768 | 统一 163 项机器可读复核报告 |
| audit_reconciliation.md | 0ca84891906b5080d5881b03aee5738ed4d5dd298f33de28bccce212c1d634ec | 统一人类可读报告 |
| TASK_REPORT.md | — | 任务执行报告 |
| TEST_REPORT.md | — | 可重复性测试报告 |
| EVIDENCE_INDEX.md | — | 本文件 |
| REVIEW_REPORT.md | — | 独立复核报告 |

## 关键事实证据

### F-001: 163 项全部覆盖
- 证据: audit_reconciliation.json `total_items` = 163
- 来源: 2026-07-18-code-audit-report.md (19 Critical + 31 High + 54 Medium + 59 Low)

### F-002: 112 项 OPEN（P01+ 修复输入）
- 证据: audit_reconciliation.json `summary.OPEN` = 112
- P0P1 中 44 项 OPEN（Critical+High 优先修复）

### F-003: 50 项 CLOSED（已解决）
- 证据: audit_reconciliation.json `summary.CLOSED` = 50
- P3 过半已修复（32/59），代码风格类问题大量解决

### F-004: 1 项 REJECTED
- B8-M-6 fact2 系数：healpix_stack 使用 astrometry.net 风格实现不需要 fact2

### F-005: 9 模块全覆盖
- B1 astro_image_io / B2 calibration / B3 plate_solve / B4 dynamic_psf / B5 photometric_calib / B6 snr_estimator / B7 healpix_drizzle / B8 healpix_stack / B9 orchestrator

## 命令日志
- `python merge_audit.py` — 退出码 0，163 项合并
