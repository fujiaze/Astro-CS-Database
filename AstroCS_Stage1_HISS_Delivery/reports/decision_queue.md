# AstroCS Stage1 HISS 未决工程选项决策队列

> **重要声明**: 以下所有项目均为 C++ 实验后的**推荐候选**, 未写入冻结规范, 也未设为不可更改的正式默认值。最终决策需用户与主审助手确认后才能冻结。
>
> 实验原始数据见 `reports/experiments/raw_results.csv` 和 `raw_results.json`, 实验环境见 `reports/experiments/environment.md`, 实验汇总见 `reports/experiments/summary.md`。

## DQ-001: signal 子块默认 codec/transform

- **状态**: 实验完成, 待用户冻结
- **实验报告**: `reports/experiments/summary.md` §DQ-001
- **推荐候选**: `byte-shuffle + LZ4` 或 `byte-shuffle + Zstd`
- **理由**: signal (float32) 数据含天文梯度 + 正态噪声, 字节间相关性较低。byte-shuffle 能将 float32 各字节位置重新排列, 显著提升压缩比 (1.30~1.34×)。LZ4 速度优先 (~2400 MB/s), Zstd 压缩比优先 (1.34×)。
- **实验数据**:
  - large_full: byte-shuffle+LZ4 压缩比 1.306, 压缩 2415 MB/s; byte-shuffle+Zstd 压缩比 1.340, 压缩 549 MB/s
  - center_80: byte-shuffle+LZ4 压缩比 1.305; byte-shuffle+Zstd 压缩比 1.322
  - edge_30: byte-shuffle+LZ4 压缩比 1.303; byte-shuffle+Zstd 压缩比 1.307
- **备选**: RAW (无压缩, 速度最快但体积最大)

## DQ-002: support 子块默认 codec

- **状态**: 实验完成, 待用户冻结
- **实验报告**: `reports/experiments/summary.md` §DQ-002
- **推荐候选**: `LZ4` 或 `Zstd` (无 transform)
- **理由**: support (uint8) 数据范围 128-255, 单字节元素, byte-shuffle 无意义。LZ4 速度优先 (~4300 MB/s), Zstd 压缩比优先 (1.14×)。
- **实验数据**:
  - large_full: LZ4 压缩比 0.996 (近无压缩); Zstd 压缩比 1.142
  - center_80: LZ4 压缩比 0.996; Zstd 压缩比 1.141
  - sparse_5: LZ4 压缩比 0.996; Zstd 压缩比 1.116
- **备选**: RAW (无压缩)

## DQ-003: BITMAP 子块默认 codec

- **状态**: 实验完成, 待用户冻结
- **实验报告**: `reports/experiments/summary.md` §DQ-003
- **推荐候选**: `LZ4` 或 `Zstd` (无 transform)
- **理由**: BITMAP 为位压缩数据 (1 bit/像素), 不同占用率下 RLE 效果差异大。高占用率时字节序列变化频繁, RLE 效果差; 低占用率时大量连续 0 字节, RLE 效果好。LZ4/Zstd 通用性好。
- **实验数据**:
  - large_full (100%): LZ4 压缩比 190.5×, Zstd 压缩比 431.2×, RLE 压缩比 124.1×
  - center_80 (80%): LZ4 压缩比 1.0×, Zstd 压缩比 1.34×, RLE 压缩比 0.52× (膨胀)
  - edge_30 (30%): LZ4 压缩比 1.0×, Zstd 压缩比 1.11×, RLE 压缩比 0.51× (膨胀)
  - sparse_5 (5%): LZ4 压缩比 1.49×, Zstd 压缩比 2.54×, RLE 压缩比 0.91×
- **备选**: RLE+LZ4 (仅在极稀疏场景 <5% 时作为可选优化)

## DQ-004: SPARSE_LIST 子块编码与 codec

- **状态**: 实验完成, 待用户冻结
- **实验报告**: `reports/experiments/summary.md` §DQ-004
- **推荐候选**: `delta + varint + LZ4` 或 `delta + varint + Zstd`
- **理由**: SPARSE_LIST 为升序 uint32 索引列表, delta 编码后值域大幅缩小。delta + varint 能将 4 字节索引压缩到平均 1-2 字节, 再配 LZ4/Zstd 效果更佳。
- **实验数据**:
  - large_full: delta+varint+LZ4 压缩比 978.1×, delta+varint+Zstd 压缩比 13107.2×
  - center_80: delta+varint+LZ4 压缩比 8.3×, delta+varint+Zstd 压缩比 20.4×
  - edge_30: delta+varint+LZ4 压缩比 5.2×, delta+varint+Zstd 压缩比 9.3×
  - sparse_5: delta+varint+LZ4 压缩比 3.98×, delta+varint+Zstd 压缩比 5.39×
- **备选**: delta + LZ4 (不含 varint, 压缩比略低但实现简单)

## DQ-005: FULL/BITMAP/SPARSE_LIST 切换阈值

- **状态**: 实验完成, 待用户冻结
- **实验报告**: `reports/experiments/summary.md` §DQ-005
- **推荐候选**: 占用率 > 80% 用 FULL; 20%-80% 用 BITMAP; < 20% 用 SPARSE_LIST
- **理由**: 基于体积测量的建议区间。FULL 在低占用率时浪费大量空间; BITMAP 体积与占用率成正比; SPARSE_LIST 在极低占用率时更省空间。BITMAP 与 SPARSE_LIST 的交叉点取决于索引列表大小 vs bitmap 大小。
- **实验数据** (327680 原始字节):
  - 100% 占用: FULL 327680 B, BITMAP 335872 B (膨胀), SPARSE_LIST 589824 B (膨胀)
  - 80% 占用: FULL 327680 B, BITMAP 270332 B, SPARSE_LIST 471852 B (膨胀)
  - 30% 占用: FULL 327680 B, BITMAP 106492 B, SPARSE_LIST 176940 B
  - 5% 占用: FULL 327680 B, BITMAP 24572 B, SPARSE_LIST 29484 B
- **备选阈值**: FULL>70% / BITMAP 15-70% / SPARSE<15% (更激进)

## DQ-006: checksum 算法

- **状态**: 实验完成, 待用户冻结
- **实验报告**: `reports/experiments/summary.md` §DQ-006
- **推荐候选**: `CRC32C` 或 `xxHash32`
- **理由**: 两者均为 4 字节校验值。xxHash32 软件实现吞吐更高 (~8200 MB/s); CRC32C 有 SSE4.2 硬件加速指令时可达更高吞吐 (~500 MB/s 软件实现, 硬件加速可达 ~10 GB/s)。
- **实验数据**:
  - large_full (262144 B): CRC32C 0.516 ms, xxHash32 0.030 ms
  - center_80 (209712 B): CRC32C 0.390 ms, xxHash32 0.024 ms
  - edge_30 (78640 B): CRC32C 0.145 ms, xxHash32 0.009 ms
  - sparse_5 (13104 B): CRC32C 0.024 ms, xxHash32 0.002 ms
- **备选**: RAW (无校验, 不推荐, 仅作基线)
- **当前实现**: HISS Writer/Reader 已实现 CRC32C, 见 `lib/astro_image_io/src/hiss_writer.cpp`

## DQ-007: 子块对齐

- **状态**: 实验完成, 待用户冻结
- **实验报告**: `reports/experiments/summary.md` §DQ-007
- **推荐候选**: `64 字节对齐`
- **理由**: 64 字节对齐与 CPU 缓存行对齐, 随机读取效率较好, padding 浪费中等。8 字节对齐 padding 最小但可能跨缓存行; 4KiB 对齐适合 mmap 但 padding 浪费最大。
- **实验数据**:
  - 8B 对齐: padding 浪费最小 (0-7 字节), 可能跨缓存行
  - 64B 对齐: padding 浪费中等 (0-63 字节), 缓存行对齐
  - 4KiB 对齐: padding 浪费最大 (center_80 膨胀 1.01×, sparse_5 膨胀 1.13×), 适合 mmap
- **备选**: 8 字节对齐 (体积优先) 或 4KiB 对齐 (mmap 优先)

## 决策状态汇总

| DQ | 项目 | 推荐候选 | 状态 |
|----|------|---------|------|
| DQ-001 | signal codec | byte-shuffle + LZ4/Zstd | 待冻结 |
| DQ-002 | support codec | LZ4 或 Zstd | 待冻结 |
| DQ-003 | BITMAP codec | LZ4 或 Zstd | 待冻结 |
| DQ-004 | SPARSE_LIST codec | delta + varint + LZ4/Zstd | 待冻结 |
| DQ-005 | 切换阈值 | FULL>80% / BITMAP 20-80% / SPARSE<20% | 待冻结 |
| DQ-006 | checksum | CRC32C 或 xxHash32 | 待冻结 |
| DQ-007 | 子块对齐 | 64 字节 | 待冻结 |

> **再次声明**: 以上推荐均基于 C++ 合成数据实验, 未使用真实天文数据。最终决策需用户确认后才能写入冻结规范, 并更新 Wiki 的 HISS-Container-and-Tiles.md 和 HISS-Metadata.md。
