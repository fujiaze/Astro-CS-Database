# M42-S1 空间预算（实测 + 显式假设）

采样时间：2026-11-11（与 `inputs.json`、`gaia_sha256.json` 同一会话）。所有"实测"数字来自本机
`df`/`du`/文件字节数；所有"预算"数字由下方公式给出，假设已显式标出。

## 1. 实测输入

| 项 | 数字 | 备注 |
|---|---|---|
| M42 原始数据根 | `/workspace/Astro CS Database/testdata/M42_T2T3_mosaic_Flying_dutchman` | 196 FITS + 1 说明 txt |
| M42 FITS 总字节 | 6,578,841,600 B = **6.127 GiB** | 196 文件，全部只读 SHA256 |
| 其中 S1.2 三帧 | 3×33,566,400 = 100,699,200 B = 96.03 MiB | 仅这 3 帧进入 S1.2 |
| 3 个 XISF 主帧 | 201,396,224 B = 192.07 MiB | bias / dark600 / flat-Hα |
| 解码后校准 FITS | 3×67,112,640 = 201,337,920 B = 192.01 MiB | `run/m42-s1/inputs/calibration/` |
| Gaia XPSD | 36 文件，111,771,752,873 B = **104.10 GiB** | 已冻结 SHA256；Gaia 索引用量另计 |
| 冻结清单本身 | `inputs.json` ≈ 333 KiB + `gaia_sha256.json` ≈ 11 KiB | 可忽略 |
| `/workspace` 可用 | 310,477,467,648 B = **289.2 GiB**（786 GiB 中已用 457 GiB，62%） | `/dev/vdb1`，非 NFS |
| 内存 | 17 GiB 总，约 12 GiB available（采样时 4 GiB used） | 16 vCPU |

`/mnt/AstroData`（100.75.49.20，只读 NFS）实测存在 `HST data/`、`real data/` 两个目录；其 README
未列 M42，本文**不**声称 M42 数据在那里。M42 数据来源只有仓库内 `testdata/`。

## 2. HEALPix 像素数（精确）

S1.2 的 Normalize 逐帧产品与 Mosaic 输出均为 HEALPix NESTED；像素数 `Npix = 12·4^order`：

| order | Npix | signal f64 (8 B/px) | full 三数组 signal+lower+upper (24 B/px) |
|---|---|---|---|
| 13 | 805,306,368 | 6.00 GiB | 18.00 GiB |
| 14 | 3,221,225,472 | 24.00 GiB | 72.00 GiB |

## 3. 预算公式与假设

每像素/每帧字节数按 `OBJECTS.md` 的产品内容估算，**这是预算假设，不是实测产品**：

- signal：`SignalEstimate.signal` f64 → **8 B/px**；
- full profile 另含 `numerical_lower/numerical_upper` 各 f64 → **+16 B/px**；
- DQ/support/validity：按 u16 一条 → **+2 B/px/帧**（仅逐帧产品，Mosaic 不发布第二科学通道）；
- 模型 sidecar（`objects/`、`blocks/`、`frame_science.json`/`mosaic_science.json`）：按科学数组的
  **25%** 计（假设；取决于 PSF/response/geometry 闭包实际大小，信号-only profile 时接近 0）；
- 发布时 staging 与正式产品可能短暂并存 → 峰值另加 **一份输出大小**；
- Normalize 工作区（drizzle contributions/NativeObservation/临时 CSR）取 **每帧 signal 的 1.0×**
  作为上限假设（与产品同量级；实测旧 128² 用例 work≈0.5×output，4096² 未测，故不冒充实测）。

### 单帧 Normalize

| order | signal-only（8+2 B/px） | full（8+16+2 B/px）+25% 模型 |
|---|---|---|
| 13 | 7.50 GiB | **24.38 GiB** |
| 14 | 30.00 GiB | **97.50 GiB** |

### S1.2 三帧 + Mosaic + Export（同 order）

| order | 3×Normalize full | Mosaic full（24 B/px +25%） | Export（目标 WCS 二维，按 ~1.1°×1.1°、0.967"/px ≈ 4096²） | 合计上限 |
|---|---|---|---|---|
| 13 | 73.13 GiB | 22.50 GiB | ≈ 0.13 GiB（signal f64 4096²）+ 模型 | **≈ 96 GiB** |
| 14 | 292.50 GiB | 90.00 GiB | 同左 | **≈ 383 GiB** |

加 work（3×18 GiB @order13）与 staging（+1 份输出 ~24 GiB）：

- **order 13 full：峰值约 140 GiB**；可用 289 GiB，**可行**。
- **order 14 full：峰值约 545 GiB**（383 + work 72 + staging 90）；超过 289 GiB 可用空间，**不可行**（除非外部 scratch）。
- order 14 signal-only 三帧 + Mosaic：3×30 + 30 = 120 GiB，加 work/staging 约 170 GiB，可行但收益
  需与像素尺度匹配（原生 0.967"/px；order 14 叶间距 0.21'，仍是下采样）。

原生像元 0.967"。HEALPix 平均叶间距 = `sqrt(4π/(12·4^order))`：order 13 ≈ 25.8"（0.43'），
order 14 ≈ 12.9"（0.21'）。二者都**粗于**原生 0.967" 约 13–27 倍，所以 Normalize 是面积平均/
下采样，不是分辨率提升；order 13 vs 14 是产品 leaf 分辨率选择，不改变原生信息。

## 4. 建议

1. **S1.2 用 order 13 + full profile**：峰值 ~140 GiB，留 ~150 GiB 余量；`run/m42-s1/staging` 与
   `normalize/` 共用一个受控 scratch 配额。
2. **建议 scratch 上限 160 GiB**（含 work + staging + 日志），发布前检查 `df`；不足时先发布
   signal-only 并具名登记缺失能力，不能静默删模型。
3. **不要在本机默认 order 14 full**：预算 ~545 GiB > 289 GiB。若必须 order 14，先改 signal-only 或
   接外部 scratch，并把决定写入运行配置。
4. Gaia 星表 JSON（队友产出）预计远小于 1 GiB（0.8° 锥、几千星），可忽略；Gaia XPSD 原文件
   104 GiB 只读，不复制进 `run/`。
