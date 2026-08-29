# ISA-003 执行日志 (2026-08-28, vm-bj)

## 输入
03_TASK_DETAILS ISA-001..004 行(先 profile 证明热点→只为热点做变体→共享合同→逐 kernel Oracle→无收益 NOT_SHIPPED 但须完整测量→主/baseline 零 ISA 污染→能力与热点对应, 不机械堆砌); 05 §1-5; 上游 ISA-002(PASS); 下游 BENCH-005(选路)/ABI-002(manifest)。

## 任务界定
ISA-003 行标称 "AVX2 FMA backend"。ISA-001 已实际构建并 SHIP `avx2_backend.so`(AVX2+FMA, `-mavx2 -mfma`)。故本任务**不重复实现**同一后端, 而是**独立复测 + 能力登记**, 确认 AVX2+FMA 的 SHIP 决策成立(ISA-001 已立 harness/共享表/决策台账)。

## 动作
1. 能力证明(05 §2 双向): 
   - baseline_backend.o 经 tools/check_baseline_opcodes.py = `BASELINE_OPCODE_PASS`(零 VEX/ymm/zmm)。
   - avx2_backend.so(`-mavx2 -mfma`)反汇编 **真含 vfnmadd231ss**(FMA, AVX2+FMA 定义特征)+ **34× %ymm 寄存器**(AVX2 256-bit)。"真 AVX2+FMA 变体"成立。
2. 独立复测(vm-bj, 2 vCPU): median 聚合多轮, best-of baseline 对变体最保守。详见 ISA-003/MEASUREMENTS.csv:
   - calibration +11.7%、hips +28.3% → **SHIP(avx2)**;
   - drizzle-accumulate −14.0% → NOT_SHIPPED(变体更慢)。
3. 决策台账(ISA_VARIANTS.md +§1.5): 登记 AVX2+FMA capability SHIP; 与 ISA-002"AVX NOT_SHIPPED"判读一致(AVX2+FMA 是受控热点最优 SHIP 档; AVX 被其严格主导)。ISA-004(AVX512)/ISA-005(BMI2/POPCNT)属 Windows 域, 本机不评估。

## 验证
- 全量回归 unittest **210/210 OK**(新增 4, test_isa_avx2_fma.py)。
- 双向 ISA 污染证明、FMA/ymm 能力证明、测量工件、决策台账 AVX2+FMA 登记, 全部断言通过。
- 完整测量在案(ISA-003/MEASUREMENTS.csv)。

## 限制与遗留
- AVX512(vm-bj 实具备)属 ISA-004(Windows 域); BMI2/POPCNT 属 ISA-005(Windows 域), 均不在本机路径, 留给 Windows/FAT 复验。
- avx2_backend.so 的连续生产集成(选路/预检/加载)由 BENCH-005/ABI-002 在 run 管线中验证; 本任务聚焦能力复验与登记。

## 产物
tests/backend/test_isa_avx2_fma.py(4 测试); docs/architecture/ISA_VARIANTS.md(+§1.5 ISA-003 capability 登记); artifacts/prerelease_v5/ISA-003/{LOG.md,MEASUREMENTS.csv}; 本日志。avx2_backend.cpp 未改动(ISA-001 已实现+SHIP)。

## PASS 判定
热点(calibration/hips)独立复测确认 AVX2+FMA 增益 +11.7%/+28.3% SHIP 成立; 能力由 FMA(vfnmadd)+ymm 指令双向证明; baseline 零污染; 完整测量在案; 无变体漂移(复用 ISA-001 已 SHIP 后端)。ISA-003 = PASS。
