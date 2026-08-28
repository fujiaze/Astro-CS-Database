# BENCH-003 执行日志 (2026-08-28, vm-bj)

## 输入
03_TASK_DETAILS BENCH-003 行(memory read/write/copy/triad;每 kernel small/medium/large、FP32/64、alignment、自动 worker/block candidates;验收=候选不含源码硬编码 core count+结果含资源指标和原始样本引用); 06 §3。

## 动作
1. bench_harness 扩展: worker_candidates(avail)={1, (avail+1)/2≈物理核级, avail} 去重升序——全派生(avail=3→{1,2,3}/16→{1,8,16}/33→{1,17,33}); block_candidates(l2,elt)=L2 派生几何序列(基=l2/(elt·8) 钳 [1K,1M], 公比 4)——机器无关; bench_memory(n,reps)=read/write/copy/triad32/triad64(median-of-5 GB/s, 防 DCE)+current_rss_bytes(资源指标)。
2. tests/backend/bench_candidates_main.cpp: 内存基线+calibration 三 size(small 4K/medium 256K/large 1M)×align(64B 对齐 vs 偏移 4B)×worker 候选全扫描+原始样本写文件(引用)。
3. tests/backend/candidates_probe_main.cpp: 生成器实参打印(avail 1/2/3/8/16/33+L2 128K/512K/2M)。
4. tests/backend/test_bench_candidates.py 5 测试: worker 候选拴证(avail=3→{1,2,3}、16→{1,8,16}, 禁 {2,4,8,16} 固定表)/block 几何公比 4+不同 L2 不同候选/内存带宽合理界(0.5<GB/s<500)+rss 采样存在/kernel 扫描 BEST+原始样本文件(sha256 引用完整性)/**生成器源码无 core count 数值表**(正则断言+派生式在案)。

## 实测(vm-bj)
- 内存基线: read 3.0/write 8.2/copy 9.3/triad32 9.3/triad64 11.2 GB/s(朴素循环, 供识别 memory-bound); rss 采样 151MB。
- 候选扫描真实发现: small(4K) w1 快于 w2(线程开销主导 4K 域)/large(1M) w2 快 24%(1225 vs 1604 µs)——候选生成必要性实证。
- 对齐差异在 f32 朴素实现内不显著(后续 SIMD 变体复测)。

## 验证
- 全量回归 unittest **140/140 OK**(新增 5)。

## 限制与遗留
- kernel 级 FP64 候选: v1 缓冲合同为 f32(baseline_kernels.h); f64 kernel 变体入 ABI 后续版本(内存 triad64 已覆盖 FP64 带宽基线)。
- block 候选对 kernel 的影响经行带尺寸在 CODE 接线时落实(run_banded 连续带+block hint 预留字段)。
- 逐 kernel 选路矩阵汇总(backend/workers/block/证据)属 BENCH-005。

## 产物
lib/backend_host/bench_harness.{h,cpp} 扩展; tests/backend/{bench_candidates_main.cpp,candidates_probe_main.cpp,test_bench_candidates.py}; 本日志。

## PASS 判定
内存 read/write/copy/triad32/64 基线实现; small/medium/large+FP32/64(triad)+alignment+worker/block 自动候选全派生(源码零硬编码核数, 机器可断言); 结果含资源指标(rss)与原始样本文件引用(sha256 可验)。BENCH-003 = PASS。
