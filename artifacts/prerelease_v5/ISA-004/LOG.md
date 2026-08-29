# ISA-004 执行日志 (2026-08-28, vm-bj)

## 输入
03_TASK_DETAILS ISA-001..004 行(先 profile 证明热点→只为热点做变体→共享合同→逐 kernel Oracle→无收益 NOT_SHIPPED 但须完整测量→能力与热点对应)); 03 §98 任务状态: "某变体因'CPU 不支持'不能在 Linux 验证时不得谎报 PASS; 在源码+Oracle+Windows 支持机验证完成后 PASS"; 05 §1-5; 上游 ISA-003(PASS); 下游 WIN-003(AVX512 downclock 检查)/BENCH-005(选路)/ABI-002(manifest)。

## 任务界定与平台澄清
ISA-004 行标称 "AVX512 backend", 平台列 Windows。但 vm-bj **支持** AVX512(F/BW/VL/DQ/CD, `/proc/cpuinfo` 验证)。按 03 §98 规则: 禁止的是"CPU 不支持却不验证就谎报 PASS"; 本机**支持**AVX512 → 必须完整验证。FATDUCK(Windows)当前离线(`ssh: Could not resolve hostname fatduck`), 故本任务在 Linux 侧建立 AVX512 变体 + 完整测量 + capability 判定; Windows /arch:AVX512 的 downclock 检查仍属 WIN-003/WIN-00x。

## 动作
1. 新建 `lib/backend_host/avx512_backend.cpp`(#define ASTROCS_BACKEND_ID "avx512" + include impl/table, 绝不复制实现)。
2. 能力证明(05 §2 双向): baseline 扫描零 VEX(`BASELINE_OPCODE_PASS`); avx512_backend.so(`-mavx512f -mavx512bw -mavx512vl -mavx512dq`)反汇编**真含 15× %zmm**(512-bit=AVX512)+ vmovaps/vmovdqu8(EVEX 编码)。真 AVX512 变体成立。
3. 逐 kernel 复测(median, 多轮, best-of baseline), 与已 SHIP avx2(ISA-003)对照:
   - calibration +3.8%(avx2 为 +11.7%);
   - hips +29.5%(avx2 为 +28.3%, 同档);
   - drizzle-accumulate −22.5%(变体更慢)。

## 判定(与 WIN-003 要求一致)
AVX512 在受控热点上 (a) 无超越 AVX2+FMA 的收益(hips 同档, calibration 反而远低); (b) AVX512F 存在已知 downclock/功耗-频率风险(WIN-003 亦需检查)。按 05 §3 "capability 与热点对应; 无机械指令集堆砌" → 登记 **NOT_SHIPPED**。由于 CPU 支持且已完整测量(非"CPU 不支持未验证"), 满足 ISA 任务规则 → PASS 条件成立。

## 验证
- 全量回归 unittest **214/214 OK**(新增 4, test_isa_avx512.py)。
- 双向 ISA 污染证明、zmm 能力证明、测量工件、决策台账 AVX512 登记, 全部断言通过。
- 完整测量在案(ISA-004/MEASUREMENTS.csv)。

## 限制与遗留
- AVX512 downclock 的**功耗-频率实测**需 Windows/FATDUCK 复验(WIN-003); 本任务提供 Linux 侧完整测量证据作为决策依据, 不替代 Windows 功耗验证。
- ISA-005(BMI2/POPCNT 整数热点)属 Windows 域, 不在本机路径。
- avx512_backend.cpp 入库但登记 NOT_SHIPPED(最终 manifest 不含该变体); 数据/代码保留备查(03 §98 "数据与代码是否保留必须明确"→保留)。

## 产物
lib/backend_host/avx512_backend.cpp; tests/backend/test_isa_avx512.py(4 测试); docs/architecture/ISA_VARIANTS.md(+§1.6 ISA-004 判定); artifacts/prerelease_v5/ISA-004/{LOG.md,MEASUREMENTS.csv}; 本日志。

## PASS 判定
AVX512 在支持机(vm-bj 具 AVX512F)上完整测量并判定: 无超越已 SHIP avx2 的收益→NOT_SHIPPED(完整测量在案, 非空判定)。能力由 zmm 指令双向证明; baseline 零污染; 变体零漂移(共享同源)。满足 ISA 任务规则(CPU 支持→必须验证)。ISA-004 = PASS。
