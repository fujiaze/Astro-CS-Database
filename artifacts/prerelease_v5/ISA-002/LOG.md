# ISA-002 执行日志 (2026-08-28, vm-bj)

## 输入
03_TASK_DETAILS ISA-001..004 行(先 profile 证明热点→只为热点做变体→共享合同→逐 kernel Oracle→无收益 NOT_SHIPPED 但须完整测量→主/baseline 零 ISA 污染→错误变体绝不入候选); 05 §1-2; 上游 ISA-001(PASS, 已立 harness/共享表/决策台账); 下游 BENCH-005(逐 kernel 选路)/ABI-002(manifest)。

## 任务界定与命名澄清
ISA-001 行标称 "SSE4.1 backend", 但其 LOG/决策台账实际 SHIP 的是 **AVX2+FMA** 变体(`avx2_backend.so`, vm-bj 支持 AVX2+FMA+AVX512F)。ISA-002 行标称 "AVX backend"。据此本任务建立 **AVX(无 FMA)** 变体做完整测量:
- 变体 `lib/backend_host/avx_backend.cpp` → `avx_backend.so`, TU 局部旗标 `-mavx`(无 -mfma/-mavx2), 共享 baseline_kernels_impl.inc/backend_table.inc 同一源(零复制漂移)。

## 动作
1. 新建 `lib/backend_host/avx_backend.cpp`(#define ASTROCS_BACKEND_ID "avx" + include impl/table, 绝不复制实现)。
2. 编译证明(06 §2):
   - `g++ -mavx -fPIC -shared ...avx_backend.cpp` → avx_backend.so(仅本 TU 局部旗标)。
   - **双向污染证明**: baseline_backend.o 经 tools/check_baseline_opcodes.py 扫描 = 零 VEX/ymm/zmm; avx_backend.so 反汇编真含 VEX(312 条, vmovaps/vaddss/vf... 等)。变体"真变体"确定性成立。
3. 实机测量(vm-bj, 2 vCPU, median-of-5 × 3 轮, best-of baseline 对变体最保守), 详见 ISA-002/MEASUREMENTS.csv:
   - calibration +11.1%、hips +25.4%、driz_accum −16.6%; noise/spmv/integration 未做变体(排序/gather 型低收益, 同 ISA-001 NOT_SHIPPED 判定)。
4. **决定性判读**: AVX 是 AVX2+FMA 严格子集; 已 SHIP 的 avx2(ISA-001)在 calibration/hips 增益 +20.7%/+28.2% 严格高于 AVX +11.1%/+25.4% → AVX 无独立收益, 登记 **NOT_SHIPPED**; 选路保持 calibration/hips→avx2, 不机械堆砌更低档变体(05 §3 "无机械指令集堆砌")。边界: 仅 AVX 无 AVX2 的主机应在该机复测(本机已具 AVX2, 记录为边界, 不在本机伪造 AVX-only 选路)。

## 验证
- 全量回归 unittest **206/206 OK**(新增 5, test_isa_avx.py)。
- 双向 ISA 污染证明、共享单源(变体 include .inc 而非复制 impl)、测量工件、决策台账 AVX 登记, 全部断言通过。
- 完整测量在案(ISA-002/MEASUREMENTS.csv), 满足"无收益登记 NOT_SHIPPED 但须完整测量" → PASS 条件成立。

## 限制与遗留
- AVX512(vm-bj 实具备)属于 ISA-004(Windows 域); 本任务不动 AVX512 变体(留给 Windows/FAT 复验)。ISA-005(BMI2/POPCNT 整数热点)属 Windows 域, 不在本机路径。
- AVX-only 主机(无 AVX2)的选路未在本机实测(硬件不具备); 已记录为边界。若未来要覆盖, 需在 AVX-only 主机复测, 属独立任务。

## 产物
lib/backend_host/avx_backend.cpp; tests/backend/test_isa_avx.py(5 测试); docs/architecture/ISA_VARIANTS.md(+ISA-002 补充决策); artifacts/prerelease_v5/ISA-002/{LOG.md,MEASUREMENTS.csv}; 本日志。

## PASS 判定
热点完整测量(6 kernel×3 轮); 只为有收益者做变体(共享合同单源); 逐 kernel Oracle(ABI-003 已立+变体容差比对); 主/baseline 零污染+变体真 VEX 双向证明; AVX 经实测被已 SHIP avx2 严格主导, 登记 NOT_SHIPPED(完整测量在案, 非空 NOT_SHIPPED); 错误变体绝不入候选(ABI-002 预检)。ISA-002 = PASS。
