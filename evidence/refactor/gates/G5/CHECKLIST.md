# G5 Phase2 Gate Checklist

状态: **PASS** (10/10) — HEAD=`b31a378`, 与 origin/main 一致

| # | 条目 | 状态 | 证据 |
|---|------|------|------|
| 1 | sampler 无固定1 worker | PASS | P2-002 `6ef62a4`: 生产无 cpu_workers=1 硬编码; heavy 禁选 1 (SingleThreaded 拒) |
| 2 | coverage/control deterministic | PASS | P2-002 确定性阈值; sampler_parallel_consistency (1T/2T, Fatduck 真实 fixture) |
| 3 | UPM additive/gauge/disconnected graph defined | PASS | P2-003 `fab2651`: 加性校正/gauge 闭合; upm.cpp 断开分量各自 gauge (C=0 参考帧) |
| 4 | seam synthetic 改善且源 flux 保持 | PASS | P2-003+P2-008: 接缝 8→<0.5 ADU; 星保留 >300 ADU |
| 5 | block plan内存有界；无稠密巨缓存 | PASS | P2-004 `957accf`: 峰值重算修复; 极小预算缩块 (不建全局 cache) |
| 6 | rejection各方法及auto reason | PASS | P2-005 `ab52854`: 10 方法 semantic id; AUTO→winsorized 路由; reason 码方向 |
| 7 | integration weight/variance/support单位清楚 | PASS | P2-006 `3c66e64`: signal/support/ivar/variance 明确名无裸 weight; weighted mean 精确 |
| 8 | diagnostics HiPS artifacts完整 | PASS | P2-006+P1-007 `466c6fa`: HiPS 全产品写 verify (signal/support/ivar/variance + Moc + properties) |
| 9 | canonical IR = runtime trace | PASS | P2-007 `ab45a63`: 4 节点(coverage/sample/upm_build/persist) + facade 委托 + trace |
| 10 | 无低利用/全局锁/黑洞/条纹 | PASS | P2-008 `b31a378`: 资源门 Ok/SingleThreaded/低利用判定; 条纹=卫星轨迹经 rejection quality 层 |

## 验证命令 (全部 exit 0)
- `make p2_workers_test && ./tests/unit/p2_workers_test` → P2-002 PASS
- `make p2_upm_synthetic_test && ./tests/unit/p2_upm_synthetic_test` → P2-003 PASS
- `make p2_block_plan_test && ./tests/unit/p2_block_plan_test` → P2-004 PASS
- `make p2_rejection_test && ./tests/unit/p2_rejection_test` → P2-005 PASS
- `make p2_output_semantics_test && ./tests/unit/p2_output_semantics_test` → P2-006 PASS
- `make p2_ir_facade_test && ASTROCS_REPO=$PWD ./tests/unit/p2_ir_facade_test` → P2-007 PASS
- `make p2_seam_gate_test && ./tests/unit/p2_seam_gate_test` → P2-008 PASS

## Gate 判定
G5 PASS (10/10)。进入 G6 (Phase3 正式开发)。
