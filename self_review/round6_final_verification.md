# Round 6 — Clean-Tree Final Verification（V16）

## 环境

```text
HEAD     = 37bebb9+fd145ee+（clean build 时点）
BRANCH   = main
工具链   = g++ 16.1.0 / CMake 4.3.2 / Ninja 1.13.2 / py 3.12.2 / git 2.53.0
clean    = run/temp/p2_v16_clean_build（全新 configure+build，exit 0）
```

## 结果

| 步骤 | 命令 | 结果 |
| --- | --- | --- |
| clean configure/build | cmake -S lib/phase2 -B run/temp/p2_v16_clean_build | exit 0 |
| full gate | clean phase2_synthetic_gate.exe | **65/65 PASS（48.5s）** |
| config 一致性 | py -3.12 tools/config_consistency_check.py | pass=true |
| oracle（clean CLI） | rejection_oracle_compare.py | ORACLE_RESULT=PASS |
| 真实 E2E（clean CLI） | astrocs-stage2 real16/stage2_clean.json | exit 0，25.1s；log 显示 wbpp_current group nominal=16→linear_fit 单次 |
| 真实 4 组门 | truth/clean/trail/trail_none | 全部 rc=0（23-25s） |
| 卫星门 V2 | satellite_gate_real_metrics.py（clean CLI 核心） | recall=1.0000；bias≈0 |
| 外部 HiPS | browser hips_backend real16 mosaic | 2048 查询 mismatch=0 PASS |
| 浏览器 | test_stf_engine / manual probe / geometry_truth | PASS |

## 约束

- 未依赖旧 build artifact（全新目录）；
- 未手改 run/temp 输入（真实 raw 帧 + Siril 转换主文件 + 版本化注入工具）；
- 真实 E2E 数据哈希见 reports/full_e2e.md；
- 无未记录环境变量（PATH/CLI 每命令显式）。

## 结论

```text
failing core tests = 0（65/65 + 浏览器 + oracle）
known P0/P1         = 0
duplicate prod path = 0
semantic ambiguity  = 0
真实 raw→Phase1→Phase2 16-exposure E2E = PASS
ROUND6=PASS
```
