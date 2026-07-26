# P06-001 Stage2 真实输入兼容检查 - 证据索引

- 任务编号：P06-001
- 执行日期：2026-07-26
- 证据目录：`engineering/evidence/P06-001/`
- 文件总数：53
- 哈希索引文件：`_file_hashes.txt`（SHA-256 + 字节大小）

---

## 1. 顶层文件

| 文件 | 大小(bytes) | SHA-256 | 描述 |
|---|---|---|---|
| `stage2_compat_results.json` | 15045 | 7E457919A2B02DD3C7697393A749C4A38A86AB2806E02D2AA7DD56F424E02F83 | 结构化结果 JSON（含所有检查详情） |
| `_file_hashes.txt` | - | - | 53 个文件的 SHA-256 + 大小索引 |
| `_file_hashes.csv` | 115 | D401EEDC9BC1EA3B8C90C220DDF2002EE43096BCB0AD5CC4348410C938C402E8 | CSV 格式哈希索引（单行） |
| `make_ring_hiss.py` | 1375 | 592AAADE540009B2E5130F7AA706736989E8FF0347D658F141D3899973342D3B | NESTED→RING HISS 改造工具 |
| `TASK_REPORT.md` | - | - | 任务执行报告 |
| `TEST_REPORT.md` | - | - | 测试结果报告 |
| `EVIDENCE_INDEX.md` | - | - | 本文件，证据索引 |
| `REVIEW_REPORT.md` | - | - | 独立复核报告 |

---

## 2. 输入 HISS 文件（input_hiss/）

来源：`engineering/evidence/P05-002/hiss/`（P05-001 真实观测，6 帧）

| 文件 | 大小(bytes) | SHA-256 | nside | filter |
|---|---|---|---|---|
| `P05-001-C001_Galaxy_Center_T4_Red_180s.hiss` | 47706 | C534865F82A750D5B2F7F346EA42DCD37D10126EAC89FF902070ADE96250F064 | 512 | Red |
| `P05-001-C003_NGC1727_T2_Red_600s.hiss` | 19347 | C9FBB48501D664CE4E6F7EBE528AB7142077C45A791E2926556C9F1CFEB2E438 | 2048 | Red |
| `P05-001-C004_NGC247_T2_Lum_600s.hiss` | 19451 | 417417611D445847B9B7BFF05009C9E23D1A1B3AB3D865F9A261CA80C3F58874 | 2048 | Lum |
| `P05-001-C005_NGC55_T3_Red_600s.hiss` | 18978 | DDABBF5D124C11B0CBB8C07DB824AB7F4015E3191F127C06C763B04FF1835FA2 | 2048 | Red |
| `P05-001-C006_NGC83_cluster_T3_Red_600s.hiss` | 19287 | C50C8C55DCD98481F8DCA5274A370778F339F2B1BC0D49FC073125261F204C60 | 2048 | Red |
| `P05-001-C007_Victory_Nebula_T4_Lum_180s.hiss` | 47691 | 9D76449DEFC2F82839F0C5274007FA6847AD9D14AE0D75FFA944CD897D20ED7A | 512 | Lum |

---

## 3. 检查 A：baseline 可重现性（test_A_baseline/）

| 文件 | 大小(bytes) | SHA-256 | 描述 |
|---|---|---|---|
| `logs/stage2_stdout.log` | 554 | A1C53ABEF7228A3F315A8819653E9909B04DB98F7CCE9518036F1038B6291AC5 | stage2 标准输出 |
| `logs/stage2_stderr.log` | 11616 | 3215EB8E3E81651998D208A7774A8BA42E484E566C11BCDEA034397524E00160 | stage2 标准错误（含 SNR² 加权日志） |
| `logs/stage2_stdout.log.exitcode.txt` | 13 | C1E97067C5F479A44A6F57297A0A8F87A59D181C3C529910F8BF059094BC3ABB | exit code=0 |
| `logs/hcsd_inspect.log` | 713 | B7C76C916319B535B31A9A6A05BD6C3D9B2D8FCB8E6405B3C13F532365BB7E30 | HCSD 元数据检查 |
| `output/stage2_baseline_repro.hcsd` | 187455430 | 2A9BD12E0F91BB59ABB170B2765A4806EC5C45FB16045F7C50E131AFA4122C37 | baseline 复现 HCSD（与 P00-003 一致） |

---

## 4. 检查 B：nside/order 兼容性（test_B_nside_mismatch/）

### 4.1 B1 nside 一致（input_b1_nside_consistent/）

| 文件 | 大小(bytes) | SHA-256 | 描述 |
|---|---|---|---|
| `input/frame_C003.hiss` | 19347 | C9FBB48501D664CE4E6F7EBE528AB7142077C45A791E2926556C9F1CFEB2E438 | C003 输入 |
| `input/frame_C005.hiss` | 18978 | DDABBF5D124C11B0CBB8C07DB824AB7F4015E3191F127C06C763B04FF1835FA2 | C005 输入 |
| `logs/b1_stdout.log` | 601 | 63DD1290ECE8449AB9D65F57852949AC910F819FB5EDBF03643099B17BBEFCFE | stdout |
| `logs/b1_stderr.log` | 12025 | 34398EDEB2A99C82722974C215A00079393D950A03ACB56FB1B307D64F6BEBD9 | stderr |
| `logs/b1_stdout.log.exitcode.txt` | 13 | C1E97067C5F479A44A6F57297A0A8F87A59D181C3C529910F8BF059094BC3ABB | exit=0 |
| `logs/b1_hcsd_inspect.log` | 706 | 66FA02DE1E3F237682AFA3996915672BF6F32C740B56F53A548D8ECF255952EF | HCSD 检查 |
| `output/b1_nside_consistent.hcsd` | 1217055 | B3EE2CC335DF871D47961CC25D72D86910703368B95E052C62A2462391B44A86 | B1 输出 HCSD |

### 4.2 B2 nside 不一致（input_b2_nside_mismatch/）

| 文件 | 大小(bytes) | SHA-256 | 描述 |
|---|---|---|---|
| `input/frame_C001.hiss` | 47706 | C534865F82A750D5B2F7F346EA42DCD37D10126EAC89FF902070ADE96250F064 | C001 nside=512 |
| `input/frame_C003.hiss` | 19347 | C9FBB48501D664CE4E6F7EBE528AB7142077C45A791E2926556C9F1CFEB2E438 | C003 nside=2048 |
| `logs/b2_stdout.log` | 409 | 3BC8F8EA35B0ECFD2AB1582D371AC1BCE93F1E556F05A79AE782D4E3D26D2854 | stdout |
| `logs/b2_stderr.log` | 9101 | 50A687370CBAF6D4D8D4AF40DA6C3573587B558E2AF580F07EEF0BC86C62F167 | stderr（含 nside 不一致日志） |
| `logs/b2_stdout.log.exitcode.txt` | 13 | CF24CEBE066D6FC14449D3AFCFCBC2E9DA129F2A2229521490CC982D7EA32FB8 | exit=1 |

### 4.3 B3 order 不一致（input_b3_order_mismatch/）

| 文件 | 大小(bytes) | SHA-256 | 描述 |
|---|---|---|---|
| `input/frame_C003_NESTED.hiss` | 19347 | C9FBB48501D664CE4E6F7EBE528AB7142077C45A791E2926556C9F1CFEB2E438 | C003 NESTED |
| `input/frame_C003_RING.hiss` | 19347 | F118D43D0B30EEA774C4F7EC75DAAD557918D38200630BD2BDF21F8DF08CB27F | C003 改造为 RING |
| `logs/b3_stdout.log` | 409 | 568A82522C51B3121959CC4E985DA08BC249F90488BA992E65525C4E4DDE32DE | stdout |
| `logs/b3_stderr.log` | 9125 | E6021AC5C9FA04B803D3C41D09D6C22C99E5AC4E881037D584008591930B6156 | stderr（含 nested 不一致日志） |
| `logs/b3_stdout.log.exitcode.txt` | 13 | CF24CEBE066D6FC14449D3AFCFCBC2E9DA129F2A2229521490CC982D7EA32FB8 | exit=1 |

---

## 5. 检查 C：filter 混合（test_C_filter_mixed/）

| 文件 | 大小(bytes) | SHA-256 | 描述 |
|---|---|---|---|
| `input/frame_C003_Red.hiss` | 19347 | C9FBB48501D664CE4E6F7EBE528AB7142077C45A791E2926556C9F1CFEB2E438 | C003 Red |
| `input/frame_C004_Lum.hiss` | 19451 | 417417611D445847B9B7BFF05009C9E23D1A1B3AB3D865F9A261CA80C3F58874 | C004 Lum |
| `logs/stdout.log` | 570 | 63333DE58F997B59F960A630450280F3C48D2894841DD668125BAD87651DBB83 | stdout |
| `logs/stderr.log` | 11778 | E599FF1A1512669AAB358490A2CE9E5CDB8443801F4CC82F28665668C0DA271E | stderr |
| `logs/stdout.log.exitcode.txt` | 13 | C1E97067C5F479A44A6F57297A0A8F87A59D181C3C529910F8BF059094BC3ABB | exit=0 |
| `logs/hcsd_inspect.log` | 31 | DF41E236E3EA5BDAE4A7F090C8D79ED47521E9CD75DE4E7B1F4DF4D7A39F4DAD | HCSD 检查（filter=Red 取首帧） |
| `output/filter_mixed.hcsd` | 1217523 | 7B0F01BA320F09EEC7D2A1C2FB64E5333EEF71B615408C9077B7658CAA51EA48 | C 输出 HCSD |

---

## 6. 检查 D：重复帧（test_D_duplicate/）

| 文件 | 大小(bytes) | SHA-256 | 描述 |
|---|---|---|---|
| `input/frame_C003_copy1.hiss` | 19347 | C9FBB48501D664CE4E6F7EBE528AB7142077C45A791E2926556C9F1CFEB2E438 | C003 副本1 |
| `input/frame_C003_copy2.hiss` | 19347 | C9FBB48501D664CE4E6F7EBE528AB7142077C45A791E2926556C9F1CFEB2E438 | C003 副本2（字节级一致） |
| `logs/stdout.log` | 561 | CA1E6B4106B8958312A3B8F3CDA2DD2F8739561225753B2A8C51248E6E3EF7C7 | stdout |
| `logs/stderr.log` | 11715 | 04EBCB45B8E63F3F3BD46C80A619AA2735B5FE9FB54CD4394F44D648954A6062 | stderr（含 mean_count=2.0） |
| `logs/stdout.log.exitcode.txt` | 13 | C1E97067C5F479A44A6F57297A0A8F87A59D181C3C529910F8BF059094BC3ABB | exit=0 |
| `logs/hcsd_inspect.log` | 691 | 9BCB82E390271C9D565571FFE0B4000422E410E7475F3FF977B80882F086EC3A | HCSD 检查 |
| `output/duplicate.hcsd` | 1198623 | BD761931EEB7C0121179844975ADE763885A662E563CEB62D6318EAA43E8F041 | D 输出 HCSD |

---

## 7. 检查 E：损坏文件（test_E_corrupted/）

| 文件 | 大小(bytes) | SHA-256 | 描述 |
|---|---|---|---|
| `input/frame_C003_good.hiss` | 19347 | C9FBB48501D664CE4E6F7EBE528AB7142077C45A791E2926556C9F1CFEB2E438 | C003 完整 |
| `input/frame_C005_truncated.hiss` | 100 | 2C4EAADE91460D05E49329823451EA781F90B441147592AF54CAE10A7421C8C5 | C005 截断至 100 字节 |
| `logs/stdout.log` | 386 | 91D8DDCA5F3BD3AEF051E28218581D9DF543CAF2FFADCF9D7103F204EEFC96DB | stdout |
| `logs/stderr.log` | 8854 | CF7FBA1504B344281B68C191FAF85180918D76B3B2079DC9AA7BD1451C27116A | stderr（含 HPS_ERR_HIO=-5） |
| `logs/stdout.log.exitcode.txt` | 13 | CF24CEBE066D6FC14449D3AFCFCBC2E9DA129F2A2229521490CC982D7EA32FB8 | exit=1 |

---

## 8. 检查 F：空目录（test_F_empty_dir/）

| 文件 | 大小(bytes) | SHA-256 | 描述 |
|---|---|---|---|
| `logs/stdout.log` | 392 | 743D29A34919E9D35ED67D964226E68AFE0BD5ACA4D3048EA74A1AB2AF5ACDDA | stdout |
| `logs/stderr.log` | 2957 | 20776D3CD4CF75C549B6FA6532732D3B1D469CF6CED6DAB50227F4ACC764EB92 | stderr（含"目录下无 .hiss 文件"） |
| `logs/stdout.log.exitcode.txt` | 13 | CF24CEBE066D6FC14449D3AFCFCBC2E9DA129F2A2229521490CC982D7EA32FB8 | exit=1 |

---

## 9. 证据完整性

- 所有文件 SHA-256 已记录在 `_file_hashes.txt`，可用于完整性校验。
- 所有成功路径 HCSD SHA-256 已记录，可用于可重现性对比。
- baseline 检查（A）证明 P00-003 的字节级可重现性（SHA 完全一致）。
- 所有失败路径无部分输出残留（原子性清理验证）。

---

## 10. 证据目录结构

```
engineering/evidence/P06-001/
├── TASK_REPORT.md
├── TEST_REPORT.md
├── EVIDENCE_INDEX.md
├── REVIEW_REPORT.md
├── stage2_compat_results.json
├── _file_hashes.txt
├── _file_hashes.csv
├── make_ring_hiss.py
├── input_hiss/                          # 6 个真实观测 HISS
├── test_A_baseline/                     # baseline 可重现性
│   ├── logs/
│   └── output/stage2_baseline_repro.hcsd
├── test_B_nside_mismatch/               # nside/order 兼容性
│   ├── input_b1_nside_consistent/
│   ├── input_b2_nside_mismatch/
│   ├── input_b3_order_mismatch/
│   ├── logs/
│   └── output/b1_nside_consistent.hcsd
├── test_C_filter_mixed/                 # filter 混合
│   ├── input/
│   ├── logs/
│   └── output/filter_mixed.hcsd
├── test_D_duplicate/                    # 重复帧
│   ├── input/
│   ├── logs/
│   └── output/duplicate.hcsd
├── test_E_corrupted/                    # 损坏文件
│   ├── input/
│   └── logs/
└── test_F_empty_dir/                    # 空目录
    └── logs/
```
