# 终产物与复现

## 1 交付包

| 项 | 路径 | sha256 |
|---|---|---|
| 交付包（整个单元，含 FITS、读数、代码、文档） | `实验/P5-signal-extraction/deliver/P5-signal-extraction.tar.gz` | 交付回报中给出（包内本文件恒为前一代，哈希随包内容变） |

## 2 终产物 FITS（只交叠好的成品，不交中间数据）

切平面（TAN）投影，最近邻填充、不插值、不重采样平滑；未覆盖处为 NaN。除路径 A 外均为 0.2014″/px
（叶像素的一半尺度），路径 A 为叶像素原生 0.1007″/px。

| 文件 | 内容 | 尺度 | 有效像素 | sha256 |
|---|---|---|---|---|
| `deliver/fits/p5x_A.fits` | **路径 A 差模对齐**（推荐成品） | 0.1007″/px | 5 408 152 | `e455ed3189e4c0ef06278c071ab65b670149fcfb01e702e0d7b99bdb0f33c04b` |
| `deliver/fits/p5x_B.fits` | 路径 B 接缝驱动 LSQ（边界专用，对照用） | 0.2014″/px | 1 352 073 | `2a6dc121c2f60347ed33a21b20f9012b6ab7831d7939edf3d261b9d55bb642bb` |
| `deliver/fits/p5x_prod.fits` | 生产产物（基准对照） | 0.2014″/px | 1 352 073 | `bbc5c235e6661fd30f617be95721a29139e849cbb64e44b70772aef79bf8e812` |
| `deliver/fits/p5x_raw.fits` | 未校正堆栈（对照，接缝 16.7%） | 0.2014″/px | 1 352 073 | `3b2b564d9eb1b1d94f78e1c4a404f07da28f5b8db3e18faa71bfcf5d9e71fe1b` |
| `deliver/fits/p5x_D_frame_const.fits` | 路径 D 逐帧常数权重（对照，最差权重口径） | 0.2014″/px | 1 352 073 | `82cca867a52079f60eadd3166d6ad1a639bf2252ff3a743fc2c8fbcedba776ef` |

读数（非成品，随包交付供复核）：`results/p5x_readouts.json`
sha256 `7e1a161c5eb63d59790d3ed3246409936e00a39345266ed1a8f4dec976a63cce`。

每条路径的像素级修正场、逐帧场与中间堆栈都可由代码现场重算，不入交付包。

## 3 复现

```bash
cd "/workspace/Astro CS Database"
bash 实验/P5-signal-extraction/code/run_all.sh          # 全部读数 + FITS，约 90 秒
```

只读输入：

- `run/LAMBDA-ROLLBACK/base/mosaic/`：`p2_sky_plane.bin`（联合模型）、`p2_corrected_f*.bin`
  与其方差（逐帧已校正瓦片）、`p2_samples.json`（天光样点）、`p2_integrated.json`（并集瓦片与积分数组）
- `run/STARMASK/out_det1/`、`run/STARMASK/out_maskoff/`：星掩膜开/关两个产物，用于掩膜归因

无随机源、无环境变量输入；「可复现」的判定面是 `results/p5x_readouts.json` 的确定性数值字段，
墙钟字段 `elapsed_s` 是运行时事实、不参与判定。
