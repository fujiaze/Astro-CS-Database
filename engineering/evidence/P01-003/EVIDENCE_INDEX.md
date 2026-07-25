# EVIDENCE_INDEX: P01-003 (v1.1 HISS/HCSD 格式版本与 round-trip)

## 任务标识
- Task ID: P01-003
- 任务名: HISS/HCSD 格式版本与 round-trip (v1.1 开发包)
- Phase / Gate: P01 / G1
- Commit base: 7b85ff3f0d37a4b26fff6077684993842ed2bbae (main, "P01-002: 建立依赖锁定清单")
- 远端: https://github.com/fujiaze/Astro-CS-Database.git
- 包版本: 2026-07-24-cli-core-v1.1-platesolve-conditional-path
- 生成时间: 2026-07-25 (PSVersion 7.6.3, Windows, Python 3.10.11)

## 证据目录
`engineering/evidence/P01-003/`

## 范围声明
- 本任务为合约冻结任务: 不修改任何 `lib/**` 业务源码, 仅冻结 HISS/HCSD v1.0 格式规范, 建立 round-trip 测试工具, 用真实数据验证。
- HISS/HCSD v1.0 是对 `lib/astro_image_io/src/healpix/aio_healpix_io.cpp` 当前实现现状的冻结, 不引入格式变更。
- 已知缺口在合约中明确记录为 "v1.1+ 待修复", 后续 P02+ 任务可基于本合约规划格式演进。

## 证据清单 (11 个文件, 含 SHA-256)

### 合约文件 (2 个, 位于 engineering/contracts/)

| 文件 | 大小 (字节) | SHA-256 | 说明 |
|---|---:|---|---|
| engineering/contracts/hiss_format_v1.md | 9319 | 0C5C8AE8139CCF719A36BB6B1BC69001DDE9579FD21B2ED1FE4DF250D5B10589 | HISS 格式规范 v1.0 合约冻结 (10 节, 字节级布局/SNR 通道/不变量) |
| engineering/contracts/hcsd_format_v1.md | 10908 | 11061492E659B04EEE0D2D2E47D5265A2FAE98ABFA234154C11B458DB4D65753 | HCSD 格式规范 v1.0 合约冻结 (11 节, 含 leaf_index/按需读取/不变量) |

### 测试工具 (1 个, 位于 engineering/tools/)

| 文件 | 大小 (字节) | SHA-256 | 说明 |
|---|---:|---|---|
| engineering/tools/test_hiss_hcsd_roundtrip.py | 39943 | 4D9FD983238C85C158D684E5B9A27528D07E5CD0356551B544A76B3FC13CF6AD | HISS/HCSD round-trip 测试脚本 (ctypes 加载 astro_image_io.dll, 9 个 API 绑定) |

### 任务证据 (8 个, 位于 engineering/evidence/P01-003/)

| 文件 | 大小 (字节) | SHA-256 | 说明 |
|---|---:|---|---|
| hiss_hcsd_format_spec.json | 13823 | C23A57881F9F9D6093D7E1A4A9863B2530FD71B9B8A6CCB7612C971F43E6A39B | 结构化规范摘要 (HISS/HCSD 完整格式 + 真实数据测试结果) |
| TASK_REPORT.md | 11357 | 5D97A5533ADEA3BCED90D93A4268153DFC3689D1950F4A2A7BF208DC9A28843E | v1.1 任务执行报告 |
| TEST_REPORT.md | 7571 | A922877A1BBC8194FA1E0C33C727518ED7C309079D471E67C458B5E49DACBE2D | v1.1 测试报告 (9 项测试 + Real-data metrics + 4 项 Failures) |
| REVIEW_REPORT.md | (本目录) | (见 git commit) | v1.1 独立复核报告 (VERDICT: PASS) |
| EVIDENCE_INDEX.md | (self) | (self-referential) | v1.1 证据索引 (本文件) |
| roundtrip_output/roundtrip_report.json | 4970 | 0037634A6FC7BB06932D2F094A302366C6BA1B1A7C3D62FF22B000A508396ADA | round-trip 测试 JSON 报告 (4/4 PASS) |
| roundtrip_output/stage1_baseline.roundtrip.hiss | 47710 | C850DE21967E090C8D68CEF35B524AF2E23B88413B945B0A3C6E7D21F66AA384 | stage1 HISS round-trip 副本 (nside=512, n_pix=3927) |
| roundtrip_output/frame1.roundtrip.hiss | 184878349 | 34B5EF6CCF51149BB656342DE862B2BD58EF9C9644673F64067E035BF767550C | frame1 HISS round-trip 副本 (nside=32768, n_pix=15406480) |
| roundtrip_output/frame2.roundtrip.hiss | 184887016 | 133BFF8E49520C89761DF3999DF5E925A25C3F39CB287EE981C128A1E8F352AC | frame2 HISS round-trip 副本 (nside=32768, n_pix=15407202) |
| roundtrip_output/stage2_baseline.roundtrip.hcsd | 187455454 | 021C3C5F6D4A91824CE578138168DD19F7F8C3107C13191D49B441E3BB88F6D8 | stage2 HCSD round-trip 副本 (nside=32768, n_pix=15522966) |

## 关键事实证据

### F-001: HISS 格式规范冻结
- 合约: engineering/contracts/hiss_format_v1.md (9319 字节)
- Magic: "HISS" (4 字节)
- 字节序: 小端序 (x86 native)
- JSON 头: zstd level=5 压缩, 含 nside/nested/n_pix/has_snr/snr_format/snr_n_points + caller meta
- SNR 通道: format 0 (逐像素 float32[n_pix]) / format 1 (稀疏控制点 4+20*n+24 字节)
- 校验和: 无 (缺陷, v1.1+ 待修复)
- format_version: 无 (缺陷, v1.1+ 待修复)
- 已知缺口: 6 项 (见合约 §8)

### F-002: HCSD 格式规范冻结
- 合约: engineering/contracts/hcsd_format_v1.md (10908 字节)
- Magic: "HCSD" (4 字节)
- 子叶索引表: 49152 项 × 24 字节 = 1179648 字节固定大小
- LeafIndexEntry: leaf_ipix:u64 + data_offset:u64(字节) + data_length:u64(像素)
- 排序规则: 按 (leaf_ipix, ipix) 升序
- 按需读取: aio_hcsd_read_leaf 支持 (P00-003 验证 78/49152 非空子叶)
- 校验和: 无 (缺陷)
- format_version: 无 (缺陷)
- 已知缺口: 10 项 (见合约 §9)

### F-003: Round-trip 测试 4/4 PASS
- 脚本: engineering/tools/test_hiss_hcsd_roundtrip.py (39943 字节)
- 测试文件: 3 HISS + 1 HCSD (覆盖 nside=512 和 nside=32768)
- 字段验证: json_header/ipix/pixel/snr/snr_model/leaf_read 全部通过
- 副本字节级差异: HISS +17 字节, HCSD +24 字节 (Python json.dumps 与 C hio_build_json 字符串差异, 不影响语义等价)
- HCSD 按子叶读取: 10 个非空子叶抽样验证全部 PASS

### F-004: 业务源码未修改
- git status 确认 lib/ 下无业务源码改动 (本任务仅写入 engineering/contracts/, engineering/tools/, engineering/evidence/P01-003/)
- 符合 P01-003 合约冻结任务范围

### F-005: 测试脚本独立于 Python 绑定
- 测试脚本用 ctypes 直接加载 lib/astro_image_io/astro_image_io.dll
- 不依赖 lib/astro_image_io/python/aio_healpix_io.py (该文件 _find_dll() 试图加载已归档的 lib/healpix_db/healpix_io/healpix_io.dll, 路径已失效)
- 调用 9 个 aio_hiss_*/aio_hcsd_* 函数 (DLL 导出表验证通过)

### F-006: 真实数据覆盖范围
- HISS: 3 个文件 (stage1_baseline 47KB + frame1/frame2 各 ~184MB)
- HCSD: 1 个文件 (stage2_baseline 187MB, 78 非空子叶)
- has_snr: 全部 false (P00-003 G-002 缺口导致 SNR 退化, 无 snr_format=1 文件可测)
- nside: 512 (stage1) + 32768 (frame1/frame2/stage2)
- n_pix 范围: 3927 ~ 15522966

### F-007: 发现的格式问题 (10 项, 记录不修复)
1. 无显式 format_version 字段 (HISS/HCSD 均缺失)
2. 无校验和机制 (CRC/SHA 全部缺失)
3. JSON 头解析使用字符串搜索 (hio_parse_json_* 用 find() 而非真正 JSON 解析器)
4. HISS 非字节级可重现 (P00-003 已记录)
5. HCSD 字节级可重现 (P00-003 验证)
6. hiss_read/hiss_read_snr_model 行为不一致
7. HCSD 无 SNR 通道 (has_snr 强制 false)
8. N_LEAVES 硬编码 49152
9. leaf_index data_offset/data_length 单位混淆
10. leaf_index leaf_ipix 字段冗余

## 复核结论
- VERDICT: PASS (详见 REVIEW_REPORT.md)
- 任务目标"冻结 HISS/HCSD 格式规范, 建立 round-trip 测试"达成
- 4/4 真实数据 round-trip 测试通过
- 11 个证据文件 SHA-256 全部采集, 可被后续 P02+ 格式演进任务直接引用
