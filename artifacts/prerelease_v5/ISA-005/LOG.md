# ISA-005 执行日志 (2026-08-28, vm-bj)

## 输入
03_TASK_DETAILS ISA-005 行「只评估整数/位操作热点; VNNI 等与算法无关则写 NOT_APPLICABLE 证据, 不写空 DLL | capability 与热点对应; 无机械指令集堆砌」; 05 §1-3; 上游 ISA-004(PASS)。vm-bj 支持 BMI1/BMI2/POPCNT(`/proc/cpuinfo` 验证)。

## 审计结论(12 ABI-003 kernel)
逐 kernel 检查计算主循环(baseline_kernels_impl.inc OpComputer):
- **纯 float 无位操作**: calibration / psf-batch / drizzle-overlap / drizzle-accumulate / drizzle-normalize / upm-residual / upm-weight-update / integration-accumulate / hips-bulk-transform。
- **无 popcount/BMI2 适用**: noise-reductions / rejection-stats(median 排序+比较计数, 非位计数)。
- **upm-spmv**(唯一含整数索引): 主循环 `acc += in0[k] * in3[col]` 是 **gather 型**(数据依赖下标), 受内存带宽/延迟约束, 非位操作热点。BMI2(mulx/rorx/blsr)/POPCNT 不加速 gather。

## 实证(机器证据, 非只读断言)
以 `-mbmi2 -mpopcnt` 编译同一 baseline 源码为变体 DSO, 反汇编(03 §91 "写证据"):
- 变体 DSO **含 BMI2/POPCNT 专用指令数 = 0**(无 mulx/rorx/blsr/blsmsk/tzcnt/lzcnt/popcnt/pdep/pext)。
- 即工具链在 kernel 集中**未发现可加速的位操作**——变体与 baseline 指令层面一致。
- 计时差(calibration/driz 数十 ns)纯为共享 2-vCPU VM run-to-run 噪声; hips 差 0.4% 即噪声量级。**无真实位操作收益**。

## 判定(03 §91)
kernel 集**无整数/位操作热点**适用于 BMI2/POPCNT → 登记 **NOT_APPLICABLE**, **不写空 DLL**(未把 bmi2_backend 作为正式 SHPI 变体入库; 仅瞬时测量工件于 /tmp)。

## 验证
- 全量回归 unittest **218/218 OK**(新增 4, test_isa_bit_manip.py)。
- test_01 审计无位操作热点; test_02 -mbmi2 -mpopcnt 变体指令计数=0(工具链证明); test_03 无空 DLL 入库+证据表 instruction_count=0+NOT_APPLICABLE; test_04 决策台账 NOT_APPLICABLE/不写空 DLL/upm-spmv。

## 限制与遗留
- 若未来引入整数/位密集型 kernel(如 binarization/高位计数), 需重新评估(本轮无)。
- Windows(/arch: 同 +BMI2/POPCNT)未复测; 因指令层已证明无语义差异(工具链无可加速位操作), Windows 侧结论一致, 但注册由 WIN-003/WIN-00x 域处理。

## 产物
tests/backend/test_isa_bit_manip.py(4 测试); docs/architecture/ISA_BIT_MANIP_VARIANTS.md(NOT_APPLICABLE 证据台账); artifacts/prerelease_v5/ISA-005/{LOG.md,MEASUREMENTS.csv}(含 instruction_count=0 证据列); 本日志。未新增正式变体文件(遵守"不写空 DLL")。

## PASS 判定
整数/位操作热点经 12-kernel 源码审计 + 机器实证(-mbmi2 -mpopcnt 编译零位操作指令)证明不存在 → NOT_APPLICABLE(完整证据在案, 非空判定)。无机械指令集堆砌(未创建空 DLL)。ISA-005 = PASS。
