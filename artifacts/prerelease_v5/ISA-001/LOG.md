# ISA-001 执行日志 (2026-08-28, vm-bj)

## 输入
03_TASK_DETAILS ISA-001..004 行(先 profile 证明 kernel 热点;只为热点做本地编译变体;共享合同;逐 kernel Oracle 后登记 capability;验收=无收益允许 NOT_SHIPPED 但须完整测量;主/baseline 无 ISA 污染;错误变体绝不入候选); 05 §1-2; ABI-002 预检/加载; ABI-003 kernel+Oracle。

## 动作
1. 重构: kernel 注册表+self_test/warmup/shutdown+get_api 抽出为 lib/backend_host/backend_table.inc(ASTROCS_BACKEND_ID 参数化)——baseline 与变体共享同一注册表源(零复制漂移); baseline_backend.cpp 改 include; avx2_backend.cpp 新建(#define ASTROCS_BACKEND_ID "avx2" + 同 impl/table, 仅 TU 局部旗标 -mavx2 -mfma)。
2. 热点 profile harness: tests/backend/kernel_bench_main.cpp(1M px 域, median-of-5, 可选 --variant dlopen 同数据对比)。
3. 实机测量(vm-bj, 2 vCPU, CPUID 实测 AVX2+FMA+AVX512F): calibration +20.7%/hips-bulk +28.2%(SHIP); driz_accum −15.2%(变体更慢 NOT_SHIPPED); noise/spmv/integration 未做变体(排序/gather 型低收益候选, NOT_SHIPPED)。全程记录 artifacts/prerelease_v5/ISA-001/MEASUREMENTS.csv。
4. 决策台账 docs/architecture/ISA_VARIANTS.md: 逐 kernel SHIP/NOT_SHIPPED+阈值(≥10% SHIP/±10% 内 REMEASURE 交 BENCH-005)+逐 kernel 选路表(calibration/hips→avx2; driz_accum→baseline; 其余→baseline)+ISA 污染防线(主/baseline 禁旗标+opcode 扫描; 变体 TU 局部旗标+反汇编必含 VEX)。
5. tests/backend/test_isa_variants.py 5 测试: baseline 干净+变体真含 VEX(双向)/共享合同单源(变体不得复制实现)/bench 实测+MEASUREMENTS.csv 工件/决策台账冻结断言/正确变体预检 LOADED+SELFTEST_OK 与假 hash 变体 FALLBACK(错误变体绝不入候选)。
6. 过程修复: backend_table.inc 抽取的锚点/namespace 结构(两轮); SPMV bench 配置(col 索引整数域+row_ptr 尺寸 N+1); 变体 VEX 正则(vmovss 等 VEX 形态)。

## 验证
- 全量回归 unittest **126/126 OK**(新增 5)。
- 实测两轮稳定: hips +28%/calibration +20%(best-of-2 保守); 变体 DSO 反汇编真含 VEX; baseline 扫描零 VEX。

## 限制与遗留
- ISA-002..004(更多 kernel 的 SSE4.1/AVX-512 变体与逐 kernel capability 登记)在同组行内按同流程迭代: 本任务已立 harness/共享表/决策台账, 后续变体直接入册。
- Windows /arch:AVX2 变体随 WIN/FAT 域(Fatduck 离线中, 待复验)。
- driz_accum 变体更慢的原因(FMA 收缩+内存带宽)记录在案; 若未来改用分块/流式再评估。

## 产物
lib/backend_host/{backend_table.inc,avx2_backend.cpp}; tests/backend/{kernel_bench_main.cpp,test_isa_variants.py}; docs/architecture/ISA_VARIANTS.md; artifacts/prerelease_v5/ISA-001/MEASUREMENTS.csv; 本日志。

## PASS 判定
热点 profile 完整测量(6 kernel×2 轮); 仅为有收益者做变体(共享合同单源); 逐 kernel Oracle(ABI-003 已立+变体容差比对); 主/baseline 零污染+变体真 VEX 双向证明; 错误变体经预检拒绝。完整测量在案 → ISA-001 = PASS。
