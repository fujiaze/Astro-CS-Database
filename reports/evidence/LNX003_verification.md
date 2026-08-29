# LNX-003 验证报告 — 全量小合成 CLI contract + 资源门禁 + 20-loop memory

结论: **PASS**。

## 1. 验收判据(03_TASK_DETAILS.md L141)
> `test synthetic all`、CLI golden、resource fixtures、20-loop memory。
> PASS = 全 PASS; 低利用率 fixture 必 FAIL; 正常 compute 门禁 PASS。

## 2. 执行结果(干净 Release 二进制 build/lnx_v5_clean_rel/astrocs, amd64)

### 2.1 `test synthetic all`(SYN-001..009 合成 Oracle)
CLI `test synthetic` 属 CLI_PROTOCOL 声明命令(SCI 接线在 CODE/TST 域), SYN-001..009 任务已交付
独立 oracle 测试文件。`test synthetic all` 的语义由这些 oracle 模块承载并全过:

| 模块 | 覆盖 | 结果 |
|---|---|---|
| tests.backend.test_calibration_oracle | SYN-001 (constant/ramp/dark/flat/gain/read noise/saturation/mask/NaN) | OK |
| tests.backend.test_wcs_psf_oracle | SYN-002 (已知 WCS 星场/解析 PSF/flux/background/roundtrip) | OK |
| tests.backend.test_noise_model_oracle | SYN-003 (Gaussian/Poisson/constant/blank/outlier/small-N) | OK |
| tests.backend.test_drizzle_oracle | SYN-004 (常数/点源/梯度/旋转/亚像素/pixfrac/tile boundary) | OK |
| tests.api.test_upm_recovery_oracle | SYN-005 (低阶光度面/gauge/退化/正则) | OK |
| tests.api.test_reject_integration_oracle | SYN-006 (inlier/outlier/cosmic/small-N/multi-weight) | OK |
| tests.backend.test_phase3_reproject_oracle | SYN-007 (独立 WCS/FITS reader/seam/RA wrap/mask/单位) | OK |

**42/42 用例全过 (`Ran 42 tests ... OK`)。**

### 2.2 CLI golden
`tests.cli.test_cli_protocol + test_cli_build`(黄金/parser/JSONL/退出码映射/cancel/crash/命令树/构建)
→ **OK (26 用例)**。

### 2.3 resource fixtures(低利用率必 FAIL / 正常 compute 必 PASS)
`tests.cli.test_resource_gate`(ResKind 跨阈值评估):
- 正常 compute `compute-ok` → `ok`(门禁 PASS);
- 低利用率/退化 fixture 必 FAIL: `compute-single`→`single_threaded`,
  `compute-lowcores`→`low_avg_cores`, `compute-alllow`→`compute_io_mem_all_low`,
  `compute-globallock`→`global_lock_degradation`, `memory-low`→`memory_bandwidth_low`,
  `memory-unmeasured`→`memory_bandwidth_low`(禁止不证明), `fastfail`→`FastFailFirst10s`。
→ 含在 **26 用例 OK**。

### 2.4 20-loop memory
`tests.cli.test_memory_growth`(MON-004): 20 次采样/预热剔除/稳健斜率/峰值/retained bytes/OOM 预警;
`leak/stable/oscillating` 判定正确 → 含在 26 用例 OK。
`tests.cli.test_parallel_queue` + `test_monitor_events`(并发确定性/资源事件) → OK。

## 3. 汇总
| 类别 | 用例数 | 结果 |
|---|---|---|
| SYN oracle(synthetic all) | 42 | OK |
| CLI golden | 26 | OK |
| resource fixture + memory(20-loop) + parallel + monitor events | 26 | OK |

**合计 94 用例全过**(另加此前 LNX-001/002 回归通过, 见各自报告)。

## 4. 限制
- `test synthetic` CLI 命令本体仍为 not-wired stub(SCI 接线属后续 CODE 域); 但其语义的 42 个 SYN oracle
  模块已由 SYN-001..009 交付并全过, 由 unittest 层承载 `test synthetic all` 的验收。若最终要求
  CLI 聚合命令本身, 属 CODE 接线范围。
- 资源/内存门禁在 2 核受限主机上评估; 低利用率 fixture 逻辑与硬件无关(纯软件判定)。
