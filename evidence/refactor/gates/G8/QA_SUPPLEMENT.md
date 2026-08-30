# G8 补充: QA-001..005 质量机器门

状态: **PASS** (5/5) — HEAD=`60493ea`

| # | 任务 | 结果 | 证据 |
|---|------|------|------|
| 1 | QA-001 清零生产编译警告抑制 | PASS `41943d4` | P2/P3 去 -w; 生产零警告; -w 仅第三方豁免 |
| 2 | QA-002 串行与硬编码静态禁令 | PASS `f8001e2` | GLOB→显式清单 (cfitsio 60/P2/P3); ACR 剔出 (nm=0); 无 workers=1/2048 硬编码 |
| 3 | QA-003 Sanitizer 验证 | PASS `6a7bdb4` | ASan 构建成功 (G3 受限解除); 7 组合成无内存错误; LSan ptrace 受限登记 |
| 4 | QA-004 重复实现与所有权 | PASS `1ee0795` | DUPLICATION_REVIEW: WCS/weight/config KEEP, scheduler DELETE(LEG); 新重复=0 |
| 5 | QA-005 可复现构建与 SBOM | PASS `60493ea` | DEPENDENCIES.md 锁定; build id 可追溯; 重构建复现一致 |

## 验证命令 (全 PASS)
- python3 tools/check_warning_suppression.py / check_serial_hardcode.py / check_duplication.py / check_reproducible_build.py
- ASan 7 组合成 (LD_PRELOAD libasan, detect_leaks=0)

## 判定
G8 原有 9/9 + QA 5/5 全过。G8 整体 PASS。
