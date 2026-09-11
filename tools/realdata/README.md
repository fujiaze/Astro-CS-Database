# tools/realdata — 真实数据审计与匹配工具（REAL-000）

真实数据验收（`工程控制/AstroCS_CONSTITUTION_ALIGNMENT_CONTROL_V1_20260909/04_OWNER_DECISIONS_20260910.md`）
前置工具：数据盘点、对账、索引升级校验与确定性校准匹配计划。本目录工具
**只读 testdata/ 与 GaiaDR3*/GaiaDR3SP/**，产物一律写 `run/realdata/`。

## match_plan.py

```bash
# 1) 盘点 + 对账 + 逐文件 sha256 清单（30G 约 2 分钟）
python3 tools/realdata/match_plan.py inventory --testdata testdata \
    --index testdata/index.json --out run/realdata --hash

# 2) 生成确定性匹配计划 + phase_config 模板 + 人读摘要
python3 tools/realdata/match_plan.py plan --testdata testdata \
    --index testdata/index.json --out run/realdata
```

产物（`run/realdata/`，gitignore）：

| 文件 | 内容 |
|---|---|
| `file_manifest.csv` | testdata 全部文件逐文件 sha256（行数=文件数=944） |
| `inventory_grouping.json` | 数据集×望远镜×面板×曝光×滤镜 分组计数 + 与 index 对账 |
| `gaia_registry.json` | GaiaDR3/GaiaDR3SP 注册摘要（DATA-GAIA-001：db_type/file_count/路径） |
| `match_plan.json` | 逐帧 `{bias, dark(file,K,strategy), flat}` 或 `UNMATCHED(reason_code)` |
| `match_summary.md` | 人读摘要 + 缺口对照表 + OWNER_CONFIRM findings |
| `phase_configs/*.json` | 各数据集 phase_config 模板（masters/滤镜/曝光/gaia_data_dir/dark_optimization=true） |
| `sha256_spotsample.txt` | 10 条抽验记录（供 `sha256sum` 独立复核） |
| `logs/` | 全部命令日志（timeout 执行） |

## 冻结匹配规则（修改须走 REAL-000 变更流程）

1. **匹配键顺序**：望远镜目录 → sensor 尺寸/binning → 曝光 → 滤镜；
2. **滤镜归一**：casefold + 去分隔符（`OIII == Oiii`）；**不改数据文件名**；
3. **dark 策略**：精确曝光优先（容差 ≤0.01s，K=1.0）；缺失时线性缩放
   `K = t_light/t_dark`（`docs/science/CALIBRATION.md:57,90`），估计器
   `OPTIMAL` fallback `EXPOSURE_RATIO`（`docs/algorithms/CALIBRATION_ALGORITHMS.md:158,292`），
   `phase_config.dark_optimization=true` 显式开启并逐帧记录 K；
   K 域 `(0, 10]`（K_OUT_OF_RANGE 语义），越界 → `NO_MASTER_DARK_BEYOND_POLICY`；
4. **flat 策略**：滤镜归一精确匹配；缺失 → `NO_<FILTER>_FLAT`（如
   `NO_LUM_FLAT`），不得用其他滤镜 flat 顶替；
5. **T1**：显式空集用例（0 帧 0 母版）→ `T1: empty(PASS)`，不是 skip。

## 测试

```bash
python3 -m pytest tests/realdata/ -q     # 匹配器正例/策略/四类负例 + index v1.2 校验
python3 -m pytest tests/monitoring/ -q   # 索引消费方回归
```

相关测试：`tests/realdata/test_match_plan.py`（匹配语义）、
`tests/realdata/test_index_v12.py`（testdata/index.json v1.2 schema 与磁盘对账）。
