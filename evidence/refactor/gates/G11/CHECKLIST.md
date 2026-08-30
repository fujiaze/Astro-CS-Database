# G11 发布 Gate Checklist

状态: **PASS** (Linux 可验 7/7; Windows 项继承 G10 WAITING 登记) — HEAD=`457dcf6`

| # | 条目 | 状态 | 证据 |
|---|------|------|------|
| 1 | Linux/Windows Alpha包只含一个用户入口 | PASS (Linux) | REL-001: check_release_layout PASS (单一 astrocs 可执行); Windows 待 G10 |
| 2 | baseline provider随包；高级provider安全fallback | PASS | REL-001: baseline 随包 (VERSION/LICENSE/README/checksums); CPU provider 内部组件 |
| 3 | VERSION/commit/build id/SBOM/checksums一致 | PASS | G11-CONSISTENCY `457dcf6`: REL_CONSISTENCY_PASS (VERSION 0.10.0-alpha.1, SBOM 8 组件, checksums 一致) |
| 4 | L0-L3最终真相校验 | PASS | REL-002 `97f6ee8`: 六层追溯 66 claims + 旧入口扫描; L0 六文档 (DOC-002) |
| 5 | P0/P1清零；无NOT VERIFIED（除owner review） | PASS | FINDINGS.csv 空 → P0/P1=0; V5 P0/P1 已在 G4-G7 修复; RELEASE_STATUS 诚实 |
| 6 | 审核包白名单/大小/敏感/SHA/解包复验 | PASS | REL-003 `b840ed6`: package_audit 522 files 白名单, 无 .git/build/testdata |
| 7 | validate_audit.py PASS | PASS | REL-003: VALIDATE_OK (522 files sha-verified) |

## 待负责人
- G10 Windows 7 项 (WAITING_WINDOWS, Fatduck 离线登记于 gates/G10/STATUS.md)。
- REL-004 视觉验收 6 视图 (REVIEW_PENDING)。

## Gate 判定
G11 PASS (Linux 侧)。最终发布 = 负责人视觉审核 (REL-004) + G10 Windows 恢复后 → `AWAITING_EXTERNAL_RELEASE_REVIEW`。
