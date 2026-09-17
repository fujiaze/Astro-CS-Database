# E2E-001 补充验证：1/N worker 数值一致性（前台独立复跑）

> 依据：tasks/E2E-001.md 验收门「1/N worker 数值一致」；ACCEPTANCE_SPEC.md L1/L3 确定性要求；
> ENGINEERING_SPEC「并行不得改变科学结果」。

## 1. 方法

- 取 **build/cpu_profile.json**（benchmark 生成，kernels.*.workers ∈ {8,16}）为 N-worker 参考面；
- 派生 **workers=1** 变体（仅改 `kernels.*.workers`，host/build/memory_bandwidth 逐字不动；profile_id 重算），
  经 `validate_profile_v2_for_machine` 接受（rc=0，无 "profile invalid" 报错）；
- 同一科学配置（`p1_gc_panel1_red`，Galaxy Center T4 panel1，Red 180s × 4 帧，T4 校准母版，dark K=1）
  仅更换 `output_dir`，分别以 1 worker 与 8/16 workers 运行 `normalize`；
- 比对基准：两次运行的 `astrocs_run_*.json` 中**逐产物 sha256 与 canonical_sha256**
  （`astrocs.canonical-product-hash/v1`，即排除易变元数据后的规范产品哈希）。

## 2. 结果

| 指标 | 结果 |
|---|---|
| 运行返回码 | 1 worker **rc=0**；N worker rc=0 |
| 产物条目数 | 17 / 17 |
| **sha256 逐字节相同** | **15 / 17** |
| 不同的 2 项 | `p1_final.json`（含绝对路径 `hips_root`，属运行报告非科学产品）、`properties`（HiPS 属性文件） |
| 其中 `properties` 的 **canonical_sha256** | **相同**（`35ef0ae4bd515935…`）→ 规范产品哈希一致，差异仅在易变元数据 |
| 结构量（`p1_final.json`） | nside=65536、n_tiles=n_tiles_written=n_support_tiles=275、tile_nside=512、products=[signal,support]、n_variance_tiles=n_ivar_tiles=0、uncertainty_available=false —— **逐项一致** |
| run status / summary | 均 `complete` / `phase1 ok` |

**结论：1/N worker 数值一致性 PASS** —— 除「运行报告内的绝对路径」与「易变元数据」外，全部产品逐字节相同；
唯一一个哈希不同的科学产品文件（`properties`）其**规范产品哈希相同**，证明并行度未改变科学结果。

## 3. 复现

```bash
python3 run/RELEASE-01/e2e/consistency/setup.py        # 派生 cpu_profile_w1.json + 同科学配置
./run/RELEASE-01/e2e/consistency/run_w1.sh             # 1 worker 运行（含磁盘余量前置检查）
python3 run/RELEASE-01/e2e/consistency/compare2.py     # 与 N worker 参考清单逐产物比对
```

参考清单来源：`run/RELEASE-01/e2e/evidence/l3__p1_gc_panel1_red/astrocs_run_*.json`（N-worker 运行原清单，随证据保留）。
