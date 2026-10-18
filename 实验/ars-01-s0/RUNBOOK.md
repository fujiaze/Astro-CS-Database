# ARS-01 / S0：原始资源测量复跑说明

本目录专用于 Issue #23 的旧 Runtime 原样基线，禁止在本任务中修改科学算法、调度、MemoryCredit 或 ABI；结果只在 `实验/ars-01-s0/` 之外的运行输出目录生成，不要把原始 FITS、Gaia XPSD 或完整巨量中间文件上传 Git。命令为正式独立 `acsd normalize/mosaic/export`，不是旧的 `phase1/phase2/phase3`。

## 0. 软件与硬件身份（所有工作负载相同）

在**拥有完整原始真实数据、Gaia、当前 main 的已构建二进制**的执行机仓库根运行：

```bash
git fetch origin main
git rev-parse HEAD
git rev-parse origin/main
# 确保本地 HEAD=origin/main；已有实验工作不要擅自 reset/覆盖
cmake --build build/linux-sci-p5 --target acsd -j
sha256sum build/linux-sci-p5/acsd
build/linux-sci-p5/acsd --version --json
cmake -LA -N build/linux-sci-p5 | grep -E 'CMAKE_BUILD_TYPE|CMAKE_CXX_COMPILER|ACSD_' > /tmp/ars01-cmake-cache.txt
```

命令只展示可能沿用的历史构建路径；若实际构建目录不同，用已经记录构建参数和 HEAD 的当前有效目录替换。不准把版本不同的旧 binary 冒充 main 新构建。保存 CMakeCache、build stamp、compile_commands（若存在），并记录内核、CPU ISA、NUMA/cgroup、容器、物理内存和目录文件系统/设备。工具输出 `session.json` 会记录 OS/CPU/内存与源码、binary 身份；编译器和完整 flags 若无 probe 显式 UNINSTRUMENTED，可通过实际编译命令/构建缓存补证，不得猜测。

Linux 测量脚本以 /proc 采样 *ACSD 主 PID*，每 0.25s 一次。其 `read_bytes/write_bytes` 是进程 Linux block I/O 计数，不等于逻辑读取量；不能声称是整机真实磁盘吞吐。Linux /proc 无法读取或 Windows 使用时这组字段为 UNINSTRUMENTED。ACSD 自有 `resource_timeseries.csv`、`resource_summary.json`、`alloc_samples.csv`、`alloc_report.json`、node_trace/graph 若生产生成，也必须原样归档，并核对 planned workers 与租约实测语义。

## W1：真实 M42/Gaia XP 六帧 Normalize

现成成功历史输入为 `实验/issue12-simpson-domain/evidence/50-cfg6.json`，身份清单 `实验/issue92-wcs-gaia-match/evidence/00-identity.json`；使用 **T3/M1 6 帧 Red + T3 bias/dark/flat + GaiaDR3SP**。运行时仅映射旧 `/workspace/Astro CS Database` 到新的真实目录，隔离 `output_dir` / `work_dir`，不修改 Baader R、Ideal QE curve、Gaia phot.fit、master 配置、WCS 与 pixfrac。

```bash
python3 实验/ars-01-s0/run_baseline.py \
  --repo-root "$PWD" --binary "$PWD/build/linux-sci-p5/acsd" \
  --workload W1 --output-root "$PWD/run/ars01/w1" \
  --source-root "$PWD" \
  --gaia-root "$PWD/gaia/GaiaDR3SP" \
  --repeats 3 --interval 0.25
```

每轮重新检查九个真实输入的 SHA-256/字节数；Gaia 20 XPSD 仅有已知文件名/字节大小校验，**不是全量 67.8 GB 内容哈希**，需要更高强度同一 GaIa 证据时另生成其内容摘要。无原始文件必须 **NOT_RUN**。CLI `exit=0`、run manifest complete、`p1_phot.n_frames_applied=6`、逐帧 WCS/photometric provenance、frame_science 与实际 signal HiPS 均需检查才可认定科学工作负载有效。历史 #92 的 wall=566.5s、RSS=12.66 GB **绝不能填作此轮值**。

## W2：同阶合法 HiPS Mosaic

**必须先从同版本 W1 完整单帧输出**确定真实帧 HiPS 根目录。填充外部列表文件 `run/ars01/w2_inputs.json`：内容为含至少两个不同帧的绝对真实目录字符串数组；禁止手填不存在的路径。

```json
[
  "/ABS/PATH/TO/VALID/P1_FRAME_A_HIPS",
  "/ABS/PATH/TO/VALID/P1_FRAME_B_HIPS"
]
```

运行前共同验证 HiPS tile order、BUNIT、响应/滤镜标记、signal tile、同科学估计量及有效 precision/provenance；不同设备/响应可由正式科学合同允许组合，但本 **正常性能基线**优先使用同通带同响应样本以免 #9/#80 问题干扰。跨 order 当前仅 fail-closed，不是正常测量。

```bash
python3 实验/ars-01-s0/run_baseline.py \
  --repo-root "$PWD" --binary "$PWD/build/linux-sci-p5/acsd" \
  --workload W2 --output-root "$PWD/run/ars01/w2" \
  --w2-hips-json "$PWD/run/ars01/w2_inputs.json" \
  --w2-snr-path sparse_reconstruct --repeats 3
```

若合法帧实际不具备稀疏层，按科学合同允许的路径改为 `--w2-snr-path frame_reconstruct` 并在每轮配置里冻结记录，不得用本次默认值强迫无数据的能力。完整 writer、signal HiPS、完整 run manifest `status=complete` 需核查；#19 不同 order、#80 病态模型、RELEASE-05 已失败产品不准作为 W2 正常基线。

## W3：从 W2 已完成 HiPS Export FITS

指定 W2 **实际**已发布的 signal HiPS 目录和同一产品的 `acsd_run_*.json`，不得按猜测拼接目录名或以测试合成属性冒充成功产品。若 manifest 不完整或 BUNIT/Footprint 不可信，不运行。

```bash
python3 实验/ars-01-s0/run_baseline.py \
  --repo-root "$PWD" --binary "$PWD/build/linux-sci-p5/acsd" \
  --workload W3 --output-root "$PWD/run/ars01/w3" \
  --w3-hips "/ABS/PATH/TO/VALID/P2_SIGNAL_HIPS" \
  --w3-complete-manifest "/ABS/PATH/TO/P2/acsd_run_actual.json" \
  --ra 83.8 --dec -5.4 --width 512 --height 512 \
  --scale 0.0005 --repeats 3
```

检查 input HiPS manifest/tiles 与 science BUNIT，输出 FITS 的 PRIMARY、CRPIX/CRVAL/CD/CTYPE/NAXIS/BUNIT、信号数据、NaN/coverage、sha256、`p3_verify.json`，并用 Astropy/WCSLIB 额外作独立读回（此脚本仅简单读取 FITS 主头，不把它冒充完整科学校验）。注意 #87 非法 tile 空洞风险和 #91 工作线程打开失败历史修复；输入损坏应作为负例，**不能作为正常 W3 成功**。

## 状态与最小验收纪律

脚本默认重复 3 次，输出 `run/ars01/wN/run-01..03` 各轮原始 config、input_identity（真实源 sha）、stdout.jsonl、stderr.log、process_samples.jsonl、measurements.json 和 product/；session.json 提供运行摘要。原始次数一并保留，失败不得删。脚本返回非零并创建 session.json 的 NOT_RUN/ PARTIAL 应如实报告。

实际科学产品成功 **不等于**其数学模型已独立验证。归档时逐轮给 exit/manifest status、输入 full hash、输出 complete/哈希、wall/CPU/RSS/I/O、原始 node timing；AIO ticket/queue、worker busy/idle、old/new Buffer overlap 等缺 probe 即写 UNINSTRUMENTED，不以0填空。2026-10-18 本仓 Git connector 可获取源码与历史报告，未能获取原始真实 FITS/Gaia/运行二进制，故本批 W1/W2/W3 状态均 **NOT_RUN**。Issue #23 仍 Open，未来运行由相应执行机补原始证据后才可能关闭；不得由这个 RUNBOOK 自动启动 #24。
