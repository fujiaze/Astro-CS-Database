# E2E 实跑记录：normalize → mosaic → export（science-v2-impl）

> 本轮执行单元的实验记录。所有数字来自本机实际运行，命令、输入、退出码、
> 产物路径全部可复跑。未运行的项明确标注。

## 1. 环境与基线

```text
HEAD            942c978e875bf7673ebc30a89fc0b59738092e26 (audit/acsd-science-docs-p5)
g++ 14.2.0 / cmake 3.31.6 / ninja / C++17, 16 核, 23G RAM
构建 A (OFF):  build/linux-sci/acsd   -DACSD_DEV_MODE=OFF
构建 B (ON):   build/linux-dev/acsd   -DACSD_DEV_MODE=ON
astropy 7.0.1 / numpy 2.2.4 / matplotlib 3.11.2
```

## 2. 输入数据索引（S00）

`code/build_input_index.py` → `run/S00/input_index.json`（909 个亮帧/HST 文件 +
27 个母版，逐文件 SHA-256）。
`code/wcs_census.py` → `run/S00/wcs_census.json`（只读 FITS 头的几何普查）。

| 数据集 | 帧数 | 几何可解 | 缺几何 | 抽样滤镜 | 抽样曝光 | px 尺度 (arcsec) |
|---|---|---|---|---|---|---|
| M42 T2T3 | 196 | 166 | 30 | Red/Blue/Green/Hα | 300/600 | 见 p1_snr/p1_products |
| LDN43 T2 | 42 | 42 | 0 | Lum/Red/Green/Blue | 600/1200 | 0.9668 |
| NGC1727 T2 | 64 | 64 | 0 | Red/Green/Blue/OIII | 600/1800 | 0.9669 |
| NGC247 T2 | 68 | 68 | 0 | Lum/R/G/B | 600 | 0.9670 |
| NGC55 T3 | 79 | 79 | 0 | Red/Green/Blue/Hα | 600/1200 | 0.9586 |
| NGC83 T3 | 72 | 72 | 0 | Red/Green/Blue/Lum | 600 | 0.9588 |
| GalaxyCenter T4 | 157 | 156 | 1 | Red | 180 | 6.3096 |
| Victory T4 | 228 | 228 | 0 | Lum | 180 | 6.3069 |
| HST M16 | 3 | 3 | 0 | F502N/F657N/F673N | 16000 | 0.04 |

**待查（AGENTS §13.1：缺几何的帧单独登记，不得猜测补齐）**：M42 T2T3 的 30 帧
（T3 面板 M2 等）缺 `CTYPE*/CRPIX/CD*`，只有 sexagesimal `OBJCTRA/OBJCTDEC`。
这些帧未进入本轮任何分析。另 1 帧 Galaxy_Center T4 同样缺几何。

HST 三帧是 **AstroDrizzle 合成品**，只当"信号模板"用：其像素噪声不是独立
随机输入（不可当作未重采样的独立观测）。本轮 E2E 未使用 HST（见 §7 未运行项）。

## 3. Normalize（阶段一）

输入：M42 T2/M1 的 8 帧 Hα（600s/300s 混合，T2 masters）。
命令与配置：`run/science-v2-impl/e2e/p1_m42_t2_m1_ha.json`
（`master_scale` 与 masters 的 XISF normalized 声明一致 = 65535，这是母版
ADU 换算，**不是**绝对测光标定；`dark_scale_factor` 由真实 FITS EXPTIME 推导）。

```bash
./build/linux-sci/acsd normalize -y --json run/science-v2-impl/e2e/p1_m42_t2_m1_ha.json
# exit=0 (11m39s)
```

产物（`out/p1_m42_t2_m1_ha/`）：8 个逐帧 HiPS 产品目录，每个含
`signal/ support/ variance/ ivar/`；工作区 `p1_work/` 含 `p1_snr.json`、
`p1_sources.json`、`p1_psf.json`、`p1_phot.json`、`p1_flux.json`。

**SNR 事实（D03 的关键）**：HiPS 头真的写出且可读
`ACSD_FRAME_SNR` 与 `ACSD_REFERENCE_FLUX`；与 `p1_snr.json` 的
`snr_reference.snr_f / flux_adu` 数值一致（抽查一帧：HiPS `ACSD_FRAME_SNR`
14.042478467183946 == p1_snr `snr_f` 14.042478467183946）。
"参考 SNR" 的条件口径如实登记：`snr_caliber = upper_bound_no_gain`
（增益未知 ⇒ σ_F 只剩天空项，SNR 是**上界**，不是绝对 SNR）。
**未写入**规整 sparse_snr_layer（`ACSD_SPARSE_SNR_LAYER` 无生产者）——
因此阶段二 `snr_path_effective` 不可能取 `sparse_reconstruct`。

## 4. ON/OFF 科学等价（R0D 验收项 3）

同一输入、同一配置、仅编译开关不同。2 帧子集（快速复跑）：

```bash
# OFF
./build/linux-sci/acsd normalize -y --json run/science-v2-impl/e2e/p1_ab_off.json
# ON
./build/linux-dev/acsd  normalize -y --json run/science-v2-impl/e2e/p1_ab_on.json
python3 实验/science-v2-impl/code/compare_onoff.py out/ab_off out/ab_on
```

| 项 | 结果 |
|---|---|
| 公共文件 | 1518 |
| **逐字节相同** | **1496** |
| 不同 | 22 |
| 其中科学 tile | **0** |
| 其中 properties | 8（仅 `hips_creation_date` 一行） |
| 其中诊断/运行摘要 | 14（`alloc_report.json`、`alloc_samples.csv`、`graph/*`、run manifest） |

**结论：所有 signal/support/variance/ivar 的 FITS tile 逐位相同。** 差异全部是
时间戳与 wall-clock 采样类运行观测，不是科学数值。

ON 侧多出的唯一文件是 `acsd_run_<run_id>.json`（run_id 不同）；开发诊断产物
全部在 `<work_dir>/_dev_diagnostics/<run_id>/` 下，`on_only_outside_dev_root`
为空 ⇒ **诊断工作区与正式产品树隔离成立**。

### 4.1 一次无效的早期读数（诚实留档）

首次 8 帧 ON 实跑出现 `aio_hips_finalize 失败: checksum verify failed`，
OFF 无此错。**追查结论：非构建差异，是我自己的流程失误**——同一命令被并发
启动两次（同一 output_dir），第二次的 `rm -rf` 与第一次的写盘互相踩。
串行重跑（`bash-4910`：OFF 先、ON 后，各自独立目录）后两者均 exit=0 且
科学产物逐位相同。这条留档说明"ON/OFF 分叉"的判定必须用**串行 + 独立目录**
的对照实验，不能用并发残留当证据。

## 5. Mosaic（阶段二）与 D02

```bash
./build/linux-dev/acsd mosaic -y --json run/science-v2-impl/e2e/mosaic.json   # exit=0
```

### 5.1 权重路径实测（D04/D05 真相）

`out/mosaic_d02/p2_final.json` 与 `p2_integrated.json`：

```json
"weight_basis": "per_sample_ivar",
"corrected_variance_used": false,
"snr_chain_used": false,
"snr_path_effective": "not_used",
"snr_path_reason": "not_used_per_sample_ivar_available",
"weight_source": "none"
```

即：**生产实际走逐样本 ivar**；`ACSD_FRAME_SNR`/`ACSD_REFERENCE_FLUX` 虽已
写进 HiPS 头（§3 已证），但因每帧 ivar 可用，SNR 链**未被触发**；规整
sparse_snr_layer 无生产者 ⇒ `sparse_reconstruct` 不可达。这与 DELTA D05 的
静态结论一致，本轮以真实运行读数确认。

### 5.2 D02：最终产品只有一个 signal

改动：`p2_op_write` 的 `AioHipsProductSet` flags 由
`SIGNAL|SUPPORT (+VARIANCE|IVAR)` 改为 `SIGNAL`；variance tile 写调用移除；
`declare_hips_surface_brightness_units` 的 `uncertainty_available` 形参拆为
`variance_products_present`（= "variance/ivar 是否真的在磁盘上"，Phase1 传
true 保留其辅助层，Phase2 传 false）；`p2_final.json` 的 `products` 与
"D02 一致性" READBACK 检查随之更新。

```text
out/mosaic_d02/  →  signal/            （唯一科学图像子产品）
```

**不破坏信号归一**：`aio_hips_write_signal_support_tile` 里
`signal = flux_sum / covered_area`，`covered_area` 是 view 的**输入面**
（`cov_out` 继续填充），不是 support 子产品。

**逐位对照**（改动前后各跑一次，纯 signal 树对比，`compare_onoff.py`）：

```text
common=188  identical=187  differing=1  (properties, 仅 hips_creation_date)
```

⇒ **signal 数值逐位不变**，删除 support/variance/ivar 不影响 signal。

### 5.3 D01/D07：跨滤镜合并资格与最终响应声明

- `lib/algorithms/coverage/src/coverage.cpp`：删除 `filter mismatch` 拒绝；
  保留 `obs_filter` 键存在性、坐标系一致性、NESTED、tile_width、hips_version
  校验。每帧真实通带身份仍登记进 `P2HipsInputInfo.filter_passband`。
- `p2_op_write` 的 `obs_filter` 不再取 `frames[0]`：全组一致时透传真实身份；
  不一致时写 `mixed:<n>_passbands` 并在 stderr 明示；`p2_final.json` 增写
  `obs_filter` + `response_distribution`（参与帧通带分布）。

本轮 E2E 两帧同带（`{"Baader 7nm H-alpha": 2}`），走"透传"分支，与旧行为逐字
相同。**跨滤镜合成的实跑验证未做**（见 §7）。

## 6. Export（阶段三）

```bash
./build/linux-dev/acsd export -y --json run/science-v2-impl/e2e/export.json   # exit=0
```

中心取自输入帧真实 `CRVAL`（83.2835420429, -6.3744273786，非文件名猜测）。
产物：`out/export/output_phase3.fits`，1024×1024，`BITPIX=-32`，
`BUNIT=ADU/sr`，单 HDU，WCS `CRPIX=(512.5,512.5)`、`CD1_1=-0.0006`。
只读 signal 单通道，未要求 variance/ivar/snr（D15 验收）。

## 7. 数字测量与可视化

`code/make_previews.py` → `run/A/preview/`（`measurements.json` + 6 张图）。

| 量 | 值 |
|---|---|
| 像素 | 1,048,576 |
| NaN | 0 |
| 负值 | 72（占有限像素 6.9e-5；天空扣除后的合法负值） |
| median | 2.6565e12 ADU/sr |
| p1 / p99 | 1.975e12 / 3.613e12 |
| 最亮源 | 2.8235e15 ADU/sr @ (x=387, y=247) |
| robust σ | 3.595e11 ADU/sr |
| 5σ 等效 | 1.798e12 ADU/sr |

**尺度自洽性核查（重要，留档）**：绝对数值很大（~1e12 ADU/sr）一度被怀疑是
drizzle 单位错误。逐环节核对后确认**不是缺陷**：
calibrated 中位数 58.46 ADU/px，帧像素 0.9407″ ⇒ 像素球面立体角
2.0799e-11 sr，`B = 58.46/2.0799e-11 = 2.81e12 ADU/sr`，
与 HiPS signal 中位数 2.756e12 相符（1.02 差 = median 与实际覆盖加权均值之差）。
⇒ drizzle 的 `S_p = Σ a_jp B_j / Σ a_jp` 面亮度归一正确，`BUNIT=ADU/sr` 成立。
原始 masters 是 XISF normalized 面，`master_scale=65535` 只是 ADU 换算，
**不是**绝对测光标定 ⇒ 上述 ADU/sr 数值是仪器域，不能作绝对光度解释
（与 D09/D17 的"未科学标定不得声称绝对光度"一致）。

预览图（`run/A/preview/`）：

- `pano_asinh.png` 全景拉伸（asinh，**未** clamp 负值、未改 NaN 语义）
- `bright_star_cutout.png` 最亮源 96px 半径 cutout
- `gradient_map.png` 梯度幅值（线状结构 = 候选接缝；**是图，不是接缝指标**）
- `lsb_asinh_a{1,10,100}.png` 低表面亮度三档 asinh 对照

## 8. 开发诊断实跑（R0D 交付）

`ACSD_DEV_SELECTOR=<selector.json> ACSD_PROBE_LOG=<path>` + `build/linux-dev`：

- 性能 JSONL 正常产出（`out/*/perf_events.jsonl`，6.5 KB/2 帧）。
- 快照样例：`out/ab_snap.p1-work/_dev_diagnostics/<run_id>/snapshots/
  normalize.noise.reference_snr/<frame>/0.json`（189 KB）+ 同名 `.meta.json`。
  meta 含 probe_id / stage / producer / semantic / unit / coord_space /
  dtype / shape / representation / roi / non_scientific_product。
- `debug_manifest.json` 含 `non_scientific_product: true`、probe 注册表、
  selection、perf_sink。

### 8.1 实测发现并修掉的两个自身缺陷（留档）

1. **guard 作用域 Bug**：phase1 的 `DevDiagSessionGuard` 原先写在
   `run_with_resource_gate` **之前**的块作用域里 ⇒ 会话还没跑 guard 就析构，
   `debug_manifest.json` 写在快照之前，`entries` 恒空（实测
   `written=0` 而文件已在盘上）。已把 guard 提到函数作用域覆盖会话运行。
   修后 manifest 的 counts/entries 与实际落盘一致。
2. **`declare_hips_surface_brightness_units` 形参语义混淆**：D02 后
   Phase2 只剩 signal，但函数按 `uncertainty_available` 去找 variance/ivar 的
   properties ⇒ `properties 不可读` fail-closed。形参改名为
   `variance_products_present`（"是否在磁盘上"），Phase1 传 true、
   Phase2 传 false。这是**形参语义错误**，不是科学公式错误；修后 Phase2 exit=0。

## 9. 未运行 / 未闭合（不得声称已完成）

| 项 | 状态 |
|---|---|
| R3 精度桥梁 T=V_B/V_F 的生产实现 | **未改**。`weight_chain.h` 仍是 `w=(SNR/F_ref)²·g²`（点源通量恒等式）。本轮只做了静态定位 + 规范摘录，未动权重公式 |
| R3 反例（两帧同 S_ref、不同 PSF/像元面积/相关 K，w_B 不同）| **未构造/未跑** |
| R2 稀疏控制点真正闭环 | **未闭合**。HiPS 头帧级 SNR 已证可用；规整 sparse layer 无生产者 |
| R5 连通分量规范/gauge 实测 | **未跑**（生产 `snr_path_effective=not_used`，本轮 mosaic 未触发星空联合模型路径的独立读回）|
| R4 曲边 oracle / 混合分辨率 target_order | **未跑**。`target_order=min(...)` 逻辑未改 |
| 跨滤镜合成实跑 | **未跑**（D01 代码已改，无跨滤镜输入验证）|
| HST 结构前向模拟 | **未跑** |
| E1–E8 实验矩阵 | **均未跑**（解析真值/模拟探测器/几何 oracle 等）|
| 三阶段 OFF/ON 全链对照 | 只做到 normalize 的逐位对照；mosaic/export 各只跑 ON 一侧 |
| ACSD_PROBES 旧环境变量旁路 | 已确认 CMake 侧统一；`ACSD_PROBE_LOG` 在 ON 构建仍是 perf sink 的路径源（属"开发运行选择器"范畴，OFF 构建不编译 probe.cpp，已由符号/字符串扫描证明）|

## 10. 复跑命令清单

```bash
# OFF / ON 构建
cmake -S . -B build/linux-sci -G Ninja -DCMAKE_BUILD_TYPE=Release -DACSD_DEV_MODE=OFF
cmake -S . -B build/linux-dev -G Ninja -DCMAKE_BUILD_TYPE=Release -DACSD_DEV_MODE=ON

# 数据索引
python3 实验/science-v2-impl/code/build_input_index.py
python3 实验/science-v2-impl/code/wcs_census.py

# E2E（串行！各自独立 output_dir）
./build/linux-sci/acsd normalize -y --json run/science-v2-impl/e2e/p1_ab_off.json
./build/linux-dev/acsd  normalize -y --json run/science-v2-impl/e2e/p1_ab_on.json
python3 实验/science-v2-impl/code/compare_onoff.py \
        run/science-v2-impl/e2e/out/ab_off run/science-v2-impl/e2e/out/ab_on

# 带快照与性能的 ON 运行
ACSD_DEV_SELECTOR=$PWD/run/science-v2-impl/e2e/dev_selector.json \
ACSD_PROBE_LOG=$PWD/run/science-v2-impl/e2e/out/ab_snap/perf_events.jsonl \
  ./build/linux-dev/acsd normalize -y --json run/science-v2-impl/e2e/p1_ab_on.json

# mosaic / export（D02 后）
./build/linux-dev/acsd mosaic -y --json run/science-v2-impl/e2e/mosaic.json
./build/linux-dev/acsd export -y --json run/science-v2-impl/e2e/export.json
python3 实验/science-v2-impl/code/make_previews.py
```
