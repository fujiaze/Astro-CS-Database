# P12-006 — EVIDENCE_INDEX

## 1. 任务元数据

| 字段 | 值 |
| --- | --- |
| 任务 ID | P12-006 |
| 阶段 | P12 |
| Gate | G12 |
| 完成日期 | 2026-07-29 |
| Verdict | PASS |
| 依赖 | P12-005 (DONE) |

## 2. 证据 4 件套

| 文件 | 路径 | 说明 |
| --- | --- | --- |
| TASK_REPORT | `工程控制/evidence/P12-006/TASK_REPORT.md` | HISS 生成 + inspect 验证 |
| TEST_REPORT | `工程控制/evidence/P12-006/TEST_REPORT.md` | Gate 通过情况 + SHA256 哈希 + 元数据完整性 |
| EVIDENCE_INDEX | `工程控制/evidence/P12-006/EVIDENCE_INDEX.md` | 本文件 — SHA256 文件清单 |
| REVIEW_REPORT | `工程控制/evidence/P12-006/REVIEW_REPORT.md` | 独立复核 (VERDICT: PASS) |

## 3. 脚本与报告 SHA256

| 文件 | SHA256 | 大小 (bytes) |
| --- | --- | --- |
| `工程控制/evidence/P12-006/scripts/generate_formal_hiss.py` | B60E3FD15B79554C65492BF5598D23115832C0F959507600F99634C28D421FC9 | 15949 |
| `工程控制/evidence/P12-006/reports/hiss_inventory.csv` | 28BE2A9EA50F7E59EF6DCAAC90C2E51AD9400625B67DB3F4BE21DF3E9DB5ACA1 | 11708 |
| `工程控制/evidence/P12-006/reports/hiss_generation_summary.json` | 4EE89FE243405F56B174AF7FD6D90A928D8151761A78945BF7377E95F9473A39 | 20884 |
| `工程控制/evidence/P12-006/raw_logs/generate_formal_hiss.log` | 722A46978C2038AE6FD1B1C2F1265208CF8862DDCC5BF2EB04190BCD9F58A4D3 | 6076 |

## 4. 正式 HISS 文件 SHA256 (16 份)

### 4.1 T4 (5 帧, nside=512)

| 帧 | SHA256 | 大小 (bytes) | n_points |
| --- | --- | --- | --- |
| T4_RED_Galaxy_Center.hiss | 0BB36E9D4CA4D8A357A049BE402EFA1C617E6841612CF2BCC3DD84CFBB62FD4B | 87433 | 1984 |
| T4_GREEN_Galaxy_Center.hiss | B1B459E844D012B72BE3932BC338E1F509ACFC73C50646609811553D9C129157 | 87110 | 1966 |
| T4_BLUE_Galaxy_Center.hiss | B2F125085EEB6948E61B87CE664469BA8CDC6455C33B08B8CEC5C2ADAE33A2D9 | 86378 | 1922 |
| T4_HA_Galaxy_Center.hiss | 7C1DCB4B0C138D58311F8BEE871268C990E124549C82609B4CBE9895F344B569 | 86719 | 1945 |
| T4_OIII_Galaxy_Center.hiss | 08533455C9FD456E56DCA829DE9D4E92E9E8DC5E53E72D2A08CFFD609555CE9E | 84891 | 1849 |

### 4.2 T2 (5 帧, nside=2048)

| 帧 | SHA256 | 大小 (bytes) | n_points |
| --- | --- | --- | --- |
| T2_RED_LDN43.hiss | D301B6B1CC6DB1DA89F4A5652C1409231A63F701561380C8AC00BD56821A4ACD | 58076 | 1930 |
| T2_GREEN_LDN43.hiss | BFF8141DB2F11D83CE044C349CB2E837F0AA1A29A60A01F3A313F2991C5861B3 | 58537 | 1953 |
| T2_BLUE_LDN43.hiss | C7A6B7F31B4D53B7C79745C947EE03C318051FC078134681EEACCAFDC93460DE | 57638 | 1908 |
| T2_HA_LDN43.hiss | 34808CB3ED048D82DFAD8AF759A6D80451520DE9EC7D09FDF0CEF60F4D1A4EAA | 29447 | 499 |
| T2_OIII_NGC1727.hiss | B546425A563BFEEFA2AAD7EB6C45F7E4EEE58DF360700B7BD01E28F2CE9A3D6C | 58176 | 1940 |

### 4.3 T3 (6 帧, nside=2048)

| 帧 | SHA256 | 大小 (bytes) | n_points |
| --- | --- | --- | --- |
| T3_RED_NGC55.hiss | FFB1D0E9A8743F96EE9A525B3E7D51D84F5FBECCB1ACDDD3EA8F6073AE7B04FE | 30166 | 557 |
| T3_GREEN_NGC55.hiss | ADB4FCB3BA192A733922B0BC4E1CC47CE2670B8C210E812AAC1504BF46AC229E | 29934 | 546 |
| T3_BLUE_NGC55.hiss | 1CD812225220327A689B68049B2881A0E0FA529BAD656EA29D11F2ED5E541AFD | 28871 | 494 |
| T3_HA_NGC55.hiss | FB629B17A866A32AC44A0ABCBA7D1E309E5043EFCF23AA3700F652A0AFBD5D8A | 23701 | 234 |
| T3_OIII_NGC55.hiss | 7E91194C68948745AF1791C625EAEFEA71482BDBD0495E27C19723BB79BE0364 | 23714 | 235 |
| T3_LUM_NGC55.hiss | C82738CA36B4A8A17B14190D7E5B4FD40E305881205CC849715B496151A43346 | 36525 | 875 |

## 5. 源 HISS 文件位置

P12-005 通过 orchestrator stage1 生成的 HISS 文件原存储位置：

```
工程控制/evidence/P12-004/raw_logs/<frame>/<frame>.hiss
```

P12-006 将上述 16 份 HISS 文件复制到正式位置：

```
工程控制/evidence/P12-006/hiss/<frame>.hiss
```

源 HISS 文件 SHA256 与正式位置 HISS 文件 SHA256 一致（使用 `shutil.copy2` 保持文件内容不变）。

## 6. stage1.log 清单

P12-005 stage1 运行日志保留在 `工程控制/evidence/P12-004/raw_logs/<frame>/stage1.log`，记录了：
- 命令行参数
- 退出码 (全部 0)
- 完整 stdout/stderr
- PhotometricDiag KV 块
- SNR 模型构建日志
- HISS 写入日志 (含 has_snr=1, n_points)

## 7. 哈希计算方法

- **工具**: Python `hashlib.sha256()` (generate_formal_hiss.py 内置)
- **算法**: SHA-256 (大写十六进制)
- **块大小**: 1MB (1 << 20 bytes)
- **计算日期**: 2026-07-29
- **验证**: 全部 16 份 HISS 文件 + 4 份脚本/报告文件 = 20 份文件哈希已记录

## 8. 证据完整性声明

- 所有 HISS 文件由 orchestrator.exe (P12-005 修复后) 通过 stage1 完整流水线生成
- 所有 HISS 文件 SHA256 哈希由 generate_formal_hiss.py 在 2026-07-29 计算
- HISS inspect 使用 lib/astro_image_io/python/aio_healpix_io.py 独立 API 读取，非 orchestrator 内部接口
- hiss_inventory.csv 和 hiss_generation_summary.json 由脚本自动生成，未被人工编辑
- 所有 HISS 文件 has_snr=1, snr_format=1, n_points>0
