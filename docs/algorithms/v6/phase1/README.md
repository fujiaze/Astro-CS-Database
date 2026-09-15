# docs/algorithms/v6/phase1/ —— ALG-P1-001 Phase1 算法实施规格

本目录是 V6 并行工程包任务 `ALG-P1-001` 的唯一写域，交付 Phase1 四个算法域的实现规格与测试矩阵。

| 文件 | 角色 |
|---|---|
| `ALG_P1_001_PHASE1_ALGORITHM_SPEC.md` | 人读算法实施规格（calibration covariance / PSF information / PSFSW 四分量 / Drizzle） |
| `ALG_P1_001_TEST_MATRIX.md` | 人读测试矩阵（由机器表机械渲染） |
| `alg_p1_001_spec.json` | 机器可读规格（单位表/公式/适用域/fail-closed/门/mutation/SO/风险） |
| `alg_p1_001_test_matrix.json` | 机器可读测试矩阵（语义源） |
| `tools/verify_alg_p1_001.py` | 独立 Oracle + 结构一致性检查 + 负向 mutation 门（可复跑） |
| `EVIDENCE.md` | 证据与命令日志（实测 rc、mutation 结果、基线/域外说明） |

## 状态

- 基线：`125bc0999363be1a42a1f2df3254601e0cc7b8fb`（任务卡）；报告时 HEAD 已被控制器推进（并行兄弟任务集成），基线仍为其祖先。
- 验证：`python3 tools/verify_alg_p1_001.py --selftest` rc=0（38/38）；`--all-mutations` rc=0（39/39 CAUGHT）。
- 属性：目标态算法规格；未 commit/push；未改上位规范或生产源码；SO-01..07 只登记不擅改。

## 复跑

```bash
cd docs/algorithms/v6/phase1
python3 tools/verify_alg_p1_001.py --selftest        # rc=0, 38/38
python3 tools/verify_alg_p1_001.py --all-mutations   # rc=0, 39/39 CAUGHT
python3 tools/verify_alg_p1_001.py --mutation M-D1    # rc=2 (门红)
```

依赖：Python 3 + numpy（独立 Oracle 只用 numpy 与标准库；无生产实现依赖）。
