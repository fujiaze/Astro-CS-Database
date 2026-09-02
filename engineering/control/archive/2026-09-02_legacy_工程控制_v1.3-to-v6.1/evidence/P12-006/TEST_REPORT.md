# P12-006 — TEST_REPORT

| 字段 | 值 |
| --- | --- |
| 任务 ID | P12-006 |
| 测试日期 | 2026-07-29 |
| 测试方式 | 16 帧代表帧 HISS 文件独立 inspect (aio_healpix_io API) + SHA256 哈希验证 |
| 测试结论 | PASS |

## 1. 测试范围

按 `tasks/P12-006.md` 必测项目要求，覆盖以下范围：

| 测试维度 | 项目 | 通过条件 |
| --- | --- | --- |
| 入口条件 | P12-005 已完成、orchestrator.exe 可用 | 已在 control 文件确认 |
| 修改前事实/失败基线 | P12-005 修复后 16/16 Gate PASS, has_snr=1 | HISS 文件已存在于 P12-004/raw_logs/ |
| Contract/unit/component 测试 | aio_healpix_io Python API HISS 读取 | 16/16 读取成功 |
| 真实数据测试 | 16 帧代表帧 HISS 独立 inspect | 16/16 PASS |
| 旧功能回归 | P12-005 修复有效性 (has_snr, n_points) | 16/16 持续有效 |
| 原始日志/超时/退出码 | generate_formal_hiss.py 脚本日志 | exit_code=0, 0.11s |

## 2. HISS 文件 inspect 测试

### 2.1 总体通过率

| 类别 | 总帧数 | PASS | 通过率 |
| --- | --- | --- | --- |
| Broadband (LUM/RED/GREEN/BLUE) | 10 | 10 | 100% |
| Narrowband (HA/OIII) | 6 | 6 | 100% |
| **总计** | **16** | **16** | **100%** |

### 2.2 HISS inspect Gate 判定标准

每帧 HISS 文件需满足以下全部条件才判为 PASS：
- `has_snr == 1` (HISS 文件包含 SNR 模型)
- `n_points > 0` (SNR 控制点数 > 0)
- `inspect_ok == True` (aio_healpix_io API 读取成功)
- `inspect_error == ""` (无读取错误)

### 2.3 按设备/滤光片分类结果

#### T4 (Galaxy_Center, nside=512)

| 滤镜 | n_pix | has_snr | snr_format | n_points | inspect_ok | 结果 |
| --- | --- | --- | --- | --- | --- | --- |
| RED | 3928 | 1 | 1 | 1984 | True | PASS |
| GREEN | 3931 | 1 | 1 | 1966 | True | PASS |
| BLUE | 3943 | 1 | 1 | 1922 | True | PASS |
| HA | 3933 | 1 | 1 | 1945 | True | PASS |
| OIII | 3941 | 1 | 1 | 1849 | True | PASS |

#### T2 (LDN43/NGC1727, nside=2048)

| 滤镜 | 目标 | n_pix | has_snr | snr_format | n_points | inspect_ok | 结果 |
| --- | --- | --- | --- | --- | --- | --- | --- |
| RED | LDN43 | 1573 | 1 | 1 | 1930 | True | PASS |
| GREEN | LDN43 | 1573 | 1 | 1 | 1953 | True | PASS |
| BLUE | LDN43 | 1573 | 1 | 1 | 1908 | True | PASS |
| HA | LDN43 | 1572 | 1 | 1 | 499 | True | PASS |
| OIII | NGC1727 | 1564 | 1 | 1 | 1940 | True | PASS |

#### T3 (NGC55, nside=2048)

| 滤镜 | n_pix | has_snr | snr_format | n_points | inspect_ok | 结果 |
| --- | --- | --- | --- | --- | --- | --- |
| RED | 1536 | 1 | 1 | 557 | True | PASS |
| GREEN | 1535 | 1 | 1 | 546 | True | PASS |
| BLUE | 1533 | 1 | 1 | 494 | True | PASS |
| HA | 1535 | 1 | 1 | 234 | True | PASS |
| OIII | 1535 | 1 | 1 | 235 | True | PASS |
| LUM | 1536 | 1 | 1 | 875 | True | PASS |

## 3. SHA256 哈希验证

每份 HISS 文件独立计算 SHA256 哈希，全部哈希值唯一且与文件内容对应：

| 帧 | 大小 (bytes) | SHA256 |
| --- | --- | --- |
| T4_RED_Galaxy_Center | 87433 | 0BB36E9D4CA4D8A357A049BE402EFA1C617E6841612CF2BCC3DD84CFBB62FD4B |
| T4_GREEN_Galaxy_Center | 87110 | B1B459E844D012B72BE3932BC338E1F509ACFC73C50646609811553D9C129157 |
| T4_BLUE_Galaxy_Center | 86378 | B2F125085EEB6948E61B87CE664469BA8CDC6455C33B08B8CEC5C2ADAE33A2D9 |
| T4_HA_Galaxy_Center | 86719 | 7C1DCB4B0C138D58311F8BEE871268C990E124549C82609B4CBE9895F344B569 |
| T4_OIII_Galaxy_Center | 84891 | 08533455C9FD456E56DCA829DE9D4E92E9E8DC5E53E72D2A08CFFD609555CE9E |
| T2_RED_LDN43 | 58076 | D301B6B1CC6DB1DA89F4A5652C1409231A63F701561380C8AC00BD56821A4ACD |
| T2_GREEN_LDN43 | 58537 | BFF8141DB2F11D83CE044C349CB2E837F0AA1A29A60A01F3A313F2991C5861B3 |
| T2_BLUE_LDN43 | 57638 | C7A6B7F31B4D53B7C79745C947EE03C318051FC078134681EEACCAFDC93460DE |
| T2_HA_LDN43 | 29447 | 34808CB3ED048D82DFAD8AF759A6D80451520DE9EC7D09FDF0CEF60F4D1A4EAA |
| T2_OIII_NGC1727 | 58176 | B546425A563BFEEFA2AAD7EB6C45F7E4EEE58DF360700B7BD01E28F2CE9A3D6C |
| T3_RED_NGC55 | 30166 | FFB1D0E9A8743F96EE9A525B3E7D51D84F5FBECCB1ACDDD3EA8F6073AE7B04FE |
| T3_GREEN_NGC55 | 29934 | ADB4FCB3BA192A733922B0BC4E1CC47CE2670B8C210E812AAC1504BF46AC229E |
| T3_BLUE_NGC55 | 28871 | 1CD812225220327A689B68049B2881A0E0FA529BAD656EA29D11F2ED5E541AFD |
| T3_HA_NGC55 | 23701 | FB629B17A866A32AC44A0ABCBA7D1E309E5043EFCF23AA3700F652A0AFBD5D8A |
| T3_OIII_NGC55 | 23714 | 7E91194C68948745AF1791C625EAEFEA71482BDBD0495E27C19723BB79BE0364 |
| T3_LUM_NGC55 | 36525 | C82738CA36B4A8A17B14190D7E5B4FD40E305881205CC849715B496151A43346 |

## 4. HISS 元数据完整性测试

### 4.1 meta JSON 字段覆盖率

所有 16 份 HISS 文件的 meta JSON 均包含以下 14 个字段：

```
drizzle, exposure_s, filter, fits_meta, has_snr, n_pix, nested,
nside, obs_time, pixfrac, snr_format, snr_n_points, source, wcs
```

字段覆盖率: 14/14 = 100% (16/16 帧)

### 4.2 HISS 文件格式验证

- **Magic**: 全部 16 份 HISS 文件 magic = "HISS" (4 bytes)
- **zstd 压缩**: JSON header 使用 zstd 压缩 (level 5)
- **snr_format=1**: 全部 16 份使用稀疏控制点格式 (非旧版逐像素格式)
- **nside**: T4=512, T2/T3=2048 (与设备 FOV 匹配)
- **nested**: 全部使用嵌套序

## 5. 旧功能回归测试

| 功能 | 测试 | 结果 |
| --- | --- | --- |
| P12-005 initDiag 修复 | has_snr=1 (n_points>0) | ✓ 16/16 持续有效 |
| P12-005 scale_factor 修复 | HISS 文件成功生成 | ✓ 16/16 (无 INVALID_SCALE) |
| P12-005 滤光片修复 | HA/OIII HISS 生成成功 | ✓ 6/6 (T4 HA/OIII + T2 HA/OIII + T3 HA/OIII) |
| P12-005 中文路径修复 | T2 HISS 生成成功 | ✓ 5/5 (T2 全部帧) |
| P11-006 WCS/SIP | HISS wcs 字段存在 | ✓ 16/16 |

## 6. 原始日志/超时/退出码

| 项 | 值 |
| --- | --- |
| 脚本路径 | `工程控制/evidence/P12-006/scripts/generate_formal_hiss.py` |
| 脚本退出码 | 0 (成功) |
| 总耗时 | 0.11 秒 (复制 + inspect + 哈希 + 清单生成) |
| 日志路径 | `工程控制/evidence/P12-006/raw_logs/generate_formal_hiss.log` |
| 源 HISS 路径 | `工程控制/evidence/P12-004/raw_logs/<frame>/<frame>.hiss` |
| 正式 HISS 路径 | `工程控制/evidence/P12-006/hiss/<frame>.hiss` |

## 7. 测试结论

- 16/16 帧 HISS 文件独立 inspect 通过 (Broadband 10/10, Narrowband 6/6)
- 全部帧 `has_snr=1`, `snr_format=1`, `n_points > 0` (最小 234)
- 全部帧 SHA256 哈希唯一且已记录
- 全部帧 meta JSON 字段完整 (14/14 字段)
- P12-005 修复全部持续有效，无回归
- 无 fallback/skip/数据范围缩减

**最终判定: PASS**
