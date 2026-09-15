# tests/integration/v6_p1 — P1-INTEGRATE-001 集成测试

> 写域：`tests/integration/v6_p1/`。独立可构建（不依赖根/公共 CMake 注册，
> C-004.4）。

## 用例

| ctest | 命令 | 断言 |
|---|---|---|
| `v6_p1_positive` | `v6_p1_integrate_test positive <work>` | 写盘 → 重开 → BUNIT/单位/provenance/权重面语义逐条一致（64 checks） |
| `v6_p1_group` | `... group <work>` | 仅由**磁盘重开**的两帧四分量产组内归一权重，median=1、全正 |
| `v6_p1_negative` | `... negative <work>` | 24 条违反冻结注入**全部必红**（含未篡改正控制） |
| `v6_p1_halfproduct` | `... halfproduct <work>` | 发布中途失败 → 无可见半成品 / 无 staging 残留 |
| `v6_p1_schema_oracle` | `v6_p1_reopen_oracle.py` | 独立 Oracle：内嵌记录过生产 schema、独立重算 SHA-256 与逐 HDU BUNIT、独立复算组内归一、Oracle 自检变异必红（68 checks，6/6 mutation caught） |

## 负向门（24；逐条违反冻结即红）

FITS 位翻转、`weight.kind=ivar`、`group_normalized=false`、`median_target≠1`、
`signal_sb=ADU`、缺 `flux_conservation_factor`、缺 `provenance.output_hash`、
`W_info.units` 篡改、`valid=false` 却有 `weight_value`、`variance_from=weight`、
对角归约声明精确、组内归一未延后、manifest/output 哈希不符、concentration
`ADU/px`、第三套词表键、correlation representation 伪造、分量分位倒序、分量数≠4、
`k_corr=1`、software_sha 非 40hex、层 BUNIT 不符，以及 **P33 帧级系数/`psf_snr_power`
重新引入**。

## 运行

```bash
cmake -S tests/integration/v6_p1 -B build/v6_p1_int -DCMAKE_BUILD_TYPE=Release
cmake --build build/v6_p1_int -j 8
ctest --test-dir build/v6_p1_int --output-on-failure
```
