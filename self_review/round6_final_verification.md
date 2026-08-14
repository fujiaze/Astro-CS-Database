# Round 6 — Clean-Tree Final Verification

## 环境记录

```text
HEAD       = 77879c720e19e237cab494be327b249090c06d44
BRANCH     = main
DATE       = 2026-08-14T13:17:56+08:00
g++        = 16.1.0 (MSYS2 MinGW64 Rev4)
CMake      = 4.3.2  | Ninja = 1.13.2
Python     = 3.12.2 | Git = 2.53.0
dataset    = run/temp/phase1_freeze/t4_crop_v3.hips（真实 Phase1 产物；
             signal Moc.fits SHA256=1B169976F073429041B4BEFA4F4840F3020216B04AC4906C5572CB73BF11E2D8）
```

## 命令与退出码（clean，未依赖旧 build artifact）

| 步骤 | 命令 | 结果 |
| --- | --- | --- |
| clean configure | `cmake -S lib/phase2 -B run/temp/p2_v15_clean_build_r6 -G Ninja` | exit 0 |
| clean build | `cmake --build run/temp/p2_v15_clean_build_r6` | exit 0（全新目录） |
| full gate | `run/temp/p2_v15_clean_build_r6/phase2_synthetic_gate.exe` | **59/59 PASS（41.8s）** |
| config 一致性 | `py -3.12 tools/config_consistency_check.py` | pass=true（30 keys） |
| 真实 Phase2 t4 | clean `astrocs-stage2 stage2_n2overlap.json` | exit 0；underdetermined=61,588,497；rejected=0 |
| 真实 GC（底图） | satellite 20 帧（受控注入）auto run | 之前 3 次 52.83/43.80/43.02s；PASS |
| 卫星线门（clean CLI） | `satellite_gate_metrics.py` | recall=1.0000；bg/star bias=0；PASS |
| 外部 HiPS oracle | browser geometry_truth + hips_backend（2048 查询 mismatch=0） | PASS |
| oracle 脚本（clean CLI） | rejection_oracle_compare / linear_fit / rcr / matrix | 全部 PASS |
| Browser smoke | clean browser_cli `--stf-manual-probe`（GC view） | bright=0.527 dark=0.009 PASS |
| Browser 全测试 | stf_engine / browser_backend / healpix_math / geometry_truth / hips_backend | 全部 PASS |
| performance smoke | satellite/n2 各 3 次（median 43.80/41.60s） | 无 >5% 无解释回退 |

## 约束符合

- 未依赖上一轮 build artifact（全新构建目录）；
- 未依赖未记录环境变量（PATH 构造在每条命令内显式）；
- 未手改 run/temp 输入（satgate 由版本化工具生成，掩码/trail 受控）；
- generated config 全部提交或由工具生成（stage2 配置在 lib/phase2/configs
  与 run/temp 工具产物，均有记录）。

## 结论

```text
failing core tests     = 0（59/59 + 浏览器 5/5 + oracle 全 PASS）
known P0/P1            = 0
unexplained warnings   = 0（first-party 编译无新增警告）
duplicate prod paths   = 0
semantic ambiguity     = 0
ROUND6=PASS
```
