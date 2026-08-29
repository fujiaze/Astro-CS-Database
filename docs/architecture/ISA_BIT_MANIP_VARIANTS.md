# 整数/位操作 ISA 变体评估 (ISA-005) — NOT_APPLICABLE

> ID: ARCH-ISA-005  状态: 结论 NOT_APPLICABLE (V5 ISA-005, 2026-08-28, vm-bj)  上游: ISA-004/05 §1-3  下游: WIN-003/manifest
> 依据(03 §91): "只评估整数/位操作热点；VNNI 等与算法无关则写 NOT_APPLICABLE 证据，不写空 DLL；capability 与热点对应；无机械指令集堆砌"。

## 1 整数/位操作热点审计(12 ABI-003 kernel)

逐 kernel 检查计算主循环(baseline_kernels_impl.inc OpComputer, 见 grep 证据):

| kernel | 计算类型 | 整数/位操作 | popcount/BMI2 适用? |
|---|---|---|---|
| calibration-pixel-transform | 纯 float 乘减加 | 无 | 否 |
| noise-snr-reductions | median 排序(比较场)+ fabs 计数 | 无(比较计数, 非位计数) | 否 |
| psf-batch | 纯 float exp | 无 | 否 |
| drizzle-overlap/accumulate/normalize | 纯 float | 无 | 否 |
| upm-spmv | CSR gather(内存带宽受限) | 仅下标索引(非位操作) | 否 |
| upm-residual/weight-update | 纯 float | 无 | 否 |
| rejection-stats | median + 比较计数 | 无(比较计数) | 否 |
| integration-accumulate | 纯 float | 无 | 否 |
| hips-bulk-transform | 重采样(纯 float 双线性) | 无 | 否 |

- **upm-spmv** 是唯一含整数索引的 kernel, 但其主循环是 `acc += in0[k] * in3[col]` —— **gather 型(数据依赖下标)**, 受内存带宽/延迟约束, 非位操作热点。BMI2(mulx/rorx/blsr)/POPCNT(popcnt)不加速 gather。

## 2 实证(机器证据, 非只读断言)

以 `-mbmi2 -mpopcnt` 编译同一 baseline 源码为变体 DSO, 反汇编(03 §91 要求"写证据"):

- **变体 DSO 反汇编含 BMI2/POPCNT 专用指令数 = 0**(无 mulx/rorx/blsr/blsmsk/tzcnt/lzcnt/popcnt/pdep/pext)。
- 即工具链在 kernel 集中**未发现任何可加速的位操作**——变体与 baseline 指令层面一致。
- 计时差(calibration/driz 数十 ns)纯为共享 2-vCPU VM 的 run-to-run 噪声; hips 差 0.4% 即噪声量级。**无真实位操作收益**。

## 3 结论

- 本 kernel 集**无整数/位操作热点**适用于 BMI2/POPCNT。按 03 §91 → 登记 **NOT_APPLICABLE**, **不写空 DLL**(不创建位操作变体文件入库)。
- 测噪验证: 变体 DSO 仅作为瞬时测量工件(/tmp), **不 SHPI/不入 manifest**。
- 若未来引入整数/位密集型 kernel(如 binarization/高位计数), 需重新评估(本轮无)。

## 4 完整性
- 测量工件: artifacts/prerelease_v5/ISA-005/MEASUREMENTS.csv(含 bmi2_popcnt_instruction_count=0 证据列)。
- 与 preflight/ABI-002 关系: 未新增变体 → 无新 manifest 行, 无预检负担。
