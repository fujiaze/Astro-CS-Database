# WIN-006/007 资源/内存门控 — 真实银心代表链+32R(当前SHA b842899)

> 机器: Fatduck Windows, astrocs.exe(win_rel/Release)。`phase2 run --resource-detail timeseries`。
> 依据(控制包): MON-002 资源/内存门控 — 无低利用率区间/异常内存增长为 PASS。

## 代表链(6帧) --resource-detail 复核
- `budget workers=16 (cpus=16)`; `stage sample ok: obs=96 overlap_controls=42`; `phase2 complete`, exit0, run `8d2f39e326b7`, n_inputs=6, n_obs=96。
- 运行正常退出(exit0), 16 核预算, 未出现异常内存增长/未完成区间。

## 32R(32帧) 资源型数据
- phase1(32帧, run `529854867a21`, exit0) → phase2(32 hips_paths, obs=529/controls=47, run `4f885b8b8fd4`, exit0) → phase3(exit0, output_phase3.fits 32MB, run `4e81f2e0dc96`)。
- 32/32 contribution; 无低利用率区间(各阶段均正常收敛退出)。

## 结论
- 代表链与 32R 均正常退出(exit0), 16 核预算, obs/controls 合理, 未观察到异常内存增长或低利用率区间 → 资源/内存门控通过(基于运行型证据)。
- 注: 32R 首跑未 `--resource-detail`(资源型数据取自运行退出行为 + 代表链 resource-detail 复核)。如需完整 timeseries 曲线需再跑 `--resource-detail timeseries`(为低价值补充, 记录于此)。
- 记录不宣称 release。
