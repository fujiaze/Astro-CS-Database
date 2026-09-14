# 证据底座（脚本固化，可重跑）

所有子目录内容都由 `../_tools/` 下的编号脚本从 git 与当前工作树机械生成，**不含人工判断**。
删除后可用下列命令重建（全程只读仓库，仅写入本目录）：

```text
cd "<仓库根>/设计大纲/_tools"
python3 01_build_commit_evidence.py            # index.jsonl + cards/ + 透视表（约 20 分钟）
python3 03_build_slices.py --size 55 --overlap 10   # compact/ + slices/
python3 02b_build_pack_inventory_v2.py         # packs 清点 + 历史整树复原 + 历史 zip 复原（约 5 分钟）
python3 05_pack_dispatch_table.py              # 包身份谱系底稿
python3 06_pack_groups.py                      # 派工分组
python3 07_pack_commit_link.py                 # 包台账号 × 提交消息编号 对接
python3 09_structural_events.py                # 结构事件表（阶段判据）
python3 10_module_provenance.py                # 模块起止溯源表
python3 04_check_slice_coverage.py             # 分片报告覆盖率核对
python3 08_check_reports.py                    # 前台自检（覆盖率+指针+包侧齐备）
```

审计脚本：`_ref_audit.py`（报告内 seq/sha 对账）、`_path_audit3.py`（报告内路径是否在磁盘或任一历史提交中存在）、`_discipline_check.py`（是否夹带代码块）。

## 目录
| 子目录 | 内容 | 是否入库 |
|---|---|---|
| `commits/index.jsonl` | 1988 条提交全量结构化记录（含文件级清单） | 是 |
| `commits/cards/` | 1988 张全卡（消息原文+变更面+签名+台账行+标题增删+内容摘录） | 否（18MB，可重跑） |
| `commits/compact/` | 1988 张紧凑卡 | 否（可重跑） |
| `commits/slices/` | 37 分片 manifest + BRIEF | 是 |
| `commits/structural_events.csv`、`module_provenance.csv`、`cross_*.csv`、`commits_table.csv` | 透视与溯源表 | 是 |
| `packs/pack_inventory.csv`、`pack_instances.json`、`digest/`、`pack_lineage_table.md`、`dispatch_groups.*`、`pack_commit_link.*`、`recovered.json` | 包清点与摘要 | 是 |
| `packs/history/` | 32 个仅存历史包的整树复原（每目录含 _PROVENANCE.json 双指针） | 否（40MB，可重跑） |
| `packs/history_zip/` | 6 个历史 zip 从 git blob 复原 | 否（22MB，可重跑） |

注意：`.gitignore` 在本目录内做了上述"否"项的排除；要全量入库请删除该文件。
