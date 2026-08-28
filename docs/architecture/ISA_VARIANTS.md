# ISA 变体决策台账 (ISA-001..004 冻结 — V5)

> ID: ARCH-ISA-001  状态: FROZEN (V5 ISA-001, 2026-08-28)  上游: ABI-003/05 §1-2  下游: BENCH-005(逐 kernel 选路)/ABI-002(manifest)
> 原则(05 §1): 先 profile 证明热点→只为热点做变体→共享合同禁漂移→逐 kernel Oracle→无收益 NOT_SHIPPED 但须完整测量(见 artifacts/prerelease_v5/ISA-001/MEASUREMENTS.csv)。

## 1 测量环境与结论(vm-bj, 2 vCPU, AVX2+FMA+AVX512F 实测)

| kernel | baseline ns | avx2 变体 ns | 提升 | 决策 |
|---|---|---|---|---|
| calibration-pixel-transform(1M px) | 1 576 742 | 1 250 899 | **+20.7%** | **SHIP(avx2)** |
| hips-bulk-transform(1M px) | 16 922 530 | 12 145 714 | **+28.2%** | **SHIP(avx2)** |
| drizzle-accumulate(1M×3) | 2 587 976 | 2 981 571 | −15.2% | NOT_SHIPPED(变体更慢) |
| noise-snr-reductions(1M×3) | 35 079 711 | — | — | NOT_SHIPPED(排序型, ISA 低收益候选) |
| upm-spmv(512K nnz) | 1 980 485 | — | — | NOT_SHIPPED(gather 型, ISA 低收益候选) |
| integration-accumulate(1M×3) | 3 876 266 | — | — | NOT_SHIPPED(同上) |

- 方法: tests/backend/kernel_bench_main.cpp, median-of-5 计时×2 轮, baseline 取最优(对变体最保守); 变体=avx2_backend.cpp(-mavx2 -mfma, 共享 baseline_kernels_impl.inc 同一源, **零复制漂移**)。
- SHIP 阈值: ≥+10% 且方向稳定; REMEASURE 带宽±10% 内交 BENCH-005。

## 2 变体注册(05 §5 capability)

- `lib/backend_host/avx2_backend.cpp` → `avx2_backend.so`(DSO, manifest: required=avx2+fma, sha256 实测入 backends.manifest.json); 预检(ABI-002)保证: 不支持 ISA 的主机绝不加载/执行。
- 逐 kernel 选路(BENCH-005): calibration/hips→avx2 变体; drizzle-accumulate→**保持 baseline**(变体更慢); 其余→baseline。错误变体绝不入候选(hash/ABI/ISA 预检+逐 kernel Oracle)。
- variant Oracle: 与 baseline 同公式同序(共享源)→输出允许 FMA 舍入差(容差 2e-4 相对, 与 Python 参考比对); 值语义不变。

## 3 ISA 污染防线(05 §2)

- 主 CLI/baseline TU: 无 -march/-mavx 旗标(测试断言), opcode scanner 禁 VEX/ymm/zmm(test_abi_kernels::test_04)。
- 变体 TU: 整 TU 局部旗标(-mavx2 -mfma)→反汇编必须含 VEX(test_isa_variants::test_02, 变体"真变体"证明)。
- Windows 变体(/arch:AVX2)随 WIN/FAT 域同流程登记。

## 4 与 ABI-003 oracle 的关系

- 12 kernel 的 Oracle(独立参考+确定性)在 ABI-003 已立; 变体复用同一 Oracle(共享公式), 变体特有仅舍入差→容差比对; 确定性(budget 1 vs 4 逐位)对变体同样成立(同 impl 外层)。
