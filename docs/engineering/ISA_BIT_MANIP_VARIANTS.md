# 整数/位操作 ISA 变体评估

> 上游：`docs/ACSD_DESIGN.md` §9（CPU 后端与资源）、`docs/engineering/ISA_VARIANTS.md`（逐 kernel 选路）

## 0 评估口径

- 只评估**整数/位操作**热点；与算法无关的指令集扩展不写空 DLL，按 `NOT_APPLICABLE`
  给出证据。
- capability 必须与热点对应，不做机械指令集堆砌。

## 1 整数/位操作热点审计（12 kernel）

逐 kernel 检查计算主循环（`lib/infrastructure/benchmark/backend_host/baseline_kernels_impl.inc`
的 `OpComputer`）：

| kernel | 计算类型 | 整数/位操作 | popcount/BMI2 适用？ |
|---|---|---|---|
| calibration-pixel-transform | 纯 float 乘减加 | 无 | 否 |
| noise-snr-reductions | median 排序（比较场）+ fabs 计数 | 无（比较计数，非位计数） | 否 |
| psf-batch | 纯 float exp | 无 | 否 |
| drizzle-overlap / accumulate / normalize | 纯 float | 无 | 否 |
| upm-spmv | CSR gather（内存带宽受限） | 仅下标索引（非位操作） | 否 |
| upm-residual / weight-update | 纯 float | 无 | 否 |
| rejection-stats | median + 比较计数 | 无（比较计数） | 否 |
| integration-accumulate | 纯 float | 无 | 否 |
| hips-bulk-transform | 重采样（纯 float 双线性） | 无 | 否 |

- **upm-spmv** 是唯一含整数索引的 kernel，其主循环为 `acc += in0[k] * in3[col]`：
  **gather 型（数据依赖下标）**，受内存带宽/延迟约束，不是位操作热点；
  BMI2（mulx/rorx/blsr）/ POPCNT（popcnt）不加速 gather。

## 2 指令层证据

以 `-mbmi2 -mpopcnt` 编译同一 baseline 源码为对照变体：

- 变体反汇编**不含任何 BMI2/POPCNT 专用指令**（mulx / rorx / blsr / blsmsk / tzcnt /
  lcnt / popcnt / pdep / pext 计数为零）——工具链在该 kernel 集中未发现可加速的位操作，
  对照变体与 baseline 指令层一致；
- 计时差落在运行噪声量级 ⇒ **无真实位操作收益**。
  逐项反汇编计数与计时读数见 `实验/engineering-evidence/prerelease-v5/`。

## 3 结论

- 本 kernel 集**无整数/位操作热点**适用于 BMI2/POPCNT ⇒ 登记 `NOT_APPLICABLE`，
  **不登记位操作变体**（不新增变体源文件、不入 `backends.manifest.json`）。
- 若未来引入整数/位密集型 kernel（如二值化、高位计数），重新按本节口径评估。

## 4 与预检/模块边界的关系

- 未新增变体 ⇒ 无新 manifest 行、无新增预检负担；逐 kernel 选路见
  `docs/engineering/ISA_VARIANTS.md` §2。
