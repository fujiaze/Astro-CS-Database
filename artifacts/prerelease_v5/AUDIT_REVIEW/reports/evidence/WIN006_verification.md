# WIN-006 验证报告 — 真实银心数据代表链路(首个里程碑)

结论: **进行中(IN_PROGRESS)**。phase1(真实校准) 已 PASS; 输入 hash manifest 已生成; 修复 2 处真实 Bug。phase2/3 链需生产 HIPS 构建, 当前 CLI 无该命令(见 §5 遗留)。

> **2026-08-30 更新**: 用户确认 **phase2/3 真实数据链记录为 BLOCKED** 并继续其它任务。blocker = 缺生产 HIPS 构建命令(HIPS 仅测试 fixture 可造); phase1 真实校准里程碑已达成。WIN-006 ledger → **BLOCKED**。

## 1. 判据(03 L150)
> 从本机数据选三板块少量 R 帧+masters; hash manifest; 跑代表链路并自动监控; contribution、science/接缝/资源/内存全 PASS 才可 32R。

## 2. 数据定位(解除此前 BLOCKED)
真实银心数据在 **`F:\Astro dev\Astro CS Normalization Database\testdata\`**(仓库 testdata 仅 index.json):
- `Galaxy_Center_T4/lights/panel1|2|3`(.fts, 4500x3600, 180S; R 帧各 11/11/10)。
- `T4 calibration files`: masterBias + masterDark×3(180/300/600s) + masterFlat×5(RGBHaOiii), 均为 64MB 未压缩 Float32 .xisf。

**选中**: 每 panel 前 2 张 R 帧(6 帧) + masterBias + masterDark(180s) + masterFlat(Red)。

## 3. 修复的 2 处真实 Bug(均在真实数据上触发, 已提交)
1. **CLI 不支持 XISF 母版**: 原 `cli/CMakeLists.txt` 只 `-DAIO_ENABLE_FITS`(无 aio_xisf.cpp), `p1_session::read_image` 用 `aio_read_fits`(纯 FITS)。.xisf 母版报 "Not a FITS file"。
   → 加入 `aio_xisf.cpp` + `AIO_ENABLE_XISF`(aio_api.cpp + aio_xisf.cpp), `read_image` 改用 `aio_read`(自动探测 .xisf/.fts)。
2. **Windows 主线程栈溢出 0xC00000FD**(写校准帧时): `fits_write_file` 的 `char iobuf[1<<20]`(1MB 栈)超出默认 1MB 主栈。
   → 改 `std::vector<char>` 堆分配。

## 4. 已达成(实测)
- **phase1 真实校准 PASS**: `astrocs phase1 run` rc=0, `phase1 complete`, `calibrate ok: 6 frames`。读取 3 个 .xisf 母版(aio_read_xisf ~0.03s)+ 6 张 .fts, 校准并写出 6 个 `calibrated_*.fts`(Write OK), 生成 run manifest `astrocs_run_ca27755c1f85.json`(sha256 `89138718...`), 并发出 resource/backend/artifact 事件。
- **输入 hash manifest**: `run/win006/win006_input_manifest.json` — 6 lights + 3 masters(bias/dark180s/flat-Red), 每项 path/size_bytes/sha256, 全清单 `inputs_sha256 = d0dfd7a1b2743328452772afb66a2ddd9831f7a34ee7fc549557d090f73dc050`。
- **Regression**: Linux 36 tests(cli_protocol/phase1/2_inprocess/p1_api)OK; 全量 Linux 320 tests 后台复核中(see §6)。Windows Release 重建 0 错误; vm-bj Linux 重建 0 错误。

## 5. 限制 / 遗留(phase2/3 链)
- **CLI 无生产 HIPS 构建命令**: phase2 读 `hips_paths`(HIPS 数据集, aio_hips_open RDSIGNAL), phase3 `source.hips_dir` 也读 HIPS。当前 CLI 仅有的 HIPS 构建入口在**测试 fixture** `phase2_fixture_main.cpp`(aio_hips_write_signal_support_tile), 无面向真实数据的生产命令。
- 因此真实数据的 phase2/3 链需要先构建 HIPS(校准帧→signal/support 瓦片), 该步骤在 CLI 中缺失 → WIN-006 的 phase2/3 与 contribution/science/接缝 门限需补充 HIPS 构建通道或按控制包预期另走路径。
- phase1 由 `phase1 run`(带 master)驱动; `run --phases 1` 不映射 master(差别记录)。

## 6. 提交
- `ad6d94c`(XISF 启用)、`f37fb55`(栈溢出修复); ledger WIN-006 → IN_PROGRESS。

## 7. 证据文件(在 Fatduck `F:/Astro dev/.../run/win006/`)
- `phase1_full.log`(phase1 详细日志), `astrocs_run_ca27755c1f85.json`(run manifest), `win006_input_manifest.json`(输入 hash manifest), `phase1_cfg.json`(配置), `calibrated_*_Red.fts`(校准输出)。
