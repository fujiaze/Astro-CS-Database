# G10 Windows Gate — 离线登记

状态: **WAITING_WINDOWS** — Fatduck 离线 (2026-08-31 探测: DNS 解析失败)

| # | 条目 | 状态 | 说明 |
|---|------|------|------|
| 1 | Fatduck identity/data/toolchain | WAITING | 离线: ssh fujia@fatduck 无法解析主机名 |
| 2 | main commit与Linux一致 | WAITING | 恢复后取 0940feb 构建 |
| 3 | MSVC clean Debug/Release | WAITING | 需 Fatduck |
| 4 | CPU benchmark/profile/provider | WAITING | 需 Fatduck |
| 5 | Windows synthetic/resource | WAITING | 需 Fatduck |
| 6 | 三块代表帧 | WAITING | 需 Fatduck 真实数据 |
| 7 | final candidate冻结 | WAITING | 依赖以上 |

## 处理策略 (AGENTS.md)
Fatduck 离线不中止目标: 恢复后重试, 不放弃/降级/伪造。G10 不阻塞 Linux 任务;
REL/G11 的 Linux 侧可验项继续。
