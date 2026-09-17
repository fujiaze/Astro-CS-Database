# 工程控制 / RELEASE-01 验收记录（ACCEPTANCE）

> PASS 仅由前台独立验证后写入；证据为前台**独立复跑**结果，不复用 SubAgent 自述。

## 1. 总表

| 任务 | 状态 | 机器门结果 | 证据路径 | 前台结论 |
|---|---|---|---|---|
| DOC-001 | **PASS**（含 3 条 UNRESOLVED 上呈） | 文档包核验器 PASS；受影响检查 8/10 PASS，2 FAIL（均为文档包↔仓库冲突，已登记 UNRESOLVED） | `run/RELEASE-01/logs/docs/`、`工程控制/RELEASE-01/DOC_PACK_MANIFEST.json` | 36 篇权威文档替换到位、治理痕迹清零、引用可达、一致性基线登记完成 |
| AUD-001 | NOT_STARTED | — | — | — |
| SCI-001 | NOT_STARTED | — | — | — |
| BLD-001 | NOT_STARTED | — | — | — |
| TST-001 | NOT_STARTED | — | — | — |
| DOC-002 | NOT_STARTED | — | — | — |
| E2E-001 | NOT_STARTED | — | — | — |
| VIS-001 | NOT_STARTED | — | — | — |
| PERF-001 | NOT_STARTED | — | — | — |
| DEL-001 | NOT_STARTED | — | — | — |
| FIN-001 | NOT_STARTED | — | — | — |

## 2. 各任务独立验证记录

### DOC-001 文档包替换核验

**替换（负责人显式授权的一次例外）**

- 来源：负责人上传 `AstroCS文档集.zip`，sha256 `fe7a0a4002b5d72c25b073e02c0633c8189578ea62efced82cd64d1ddcf3a483`（与上传件实测一致），36 篇；
- 结果：23 篇与仓库既有内容逐字节相同；12 篇为授权更新；1 篇新增（`docs/research/SNR_WEIGHT_RESEARCH_PACK.md`）；
- 全部按包内目录结构落位；哈希清单见 `DOC_PACK_MANIFEST.json`。

**残留清理（DOC-001 步骤 3，仅非语义订正）**

| 文件 | 清理内容 | 保留语义 |
|---|---|---|
| `ENGINEERING_SPEC.md` §7 | 删治理 ID `CFG-001` / `GAP-033` | config/ 为程序根全局配置（ASTROCS_DESIGN §3.3 点名） |
| `docs/plugins/algorithms_phase3/14_projection.md` | 删任务 ID `GAP-011` | STG/MOL/CEA/ZEA 待实现 |
| `docs/plugins/algorithms_phase1/05_platesolve.md` | 死引用 `docs/algorithms/platesolve/*` → 实际文件 `docs/algorithms/PLATESOLVE.md` | 解算算法推导出处 |

**独立复跑（前台）**

```bash
python3 工程控制/RELEASE-01/verify_doc_pack.py --json-out run/RELEASE-01/logs/docs/DOC-001_verify.json
# 权威文档: 36 篇  授权差异: 3 条  相对引用检查: 81 处
# R1_missing 0  R1_hash_mismatch 0  R2_residue 0  R3_unreachable 0  R4_stale_version 0
# verdict=PASS  rc=0
```

| 判据 | 结果 |
|---|---|
| R1 36 篇在位且哈希 == 授权后哈希 | PASS（0 缺失 / 0 不符） |
| R2 治理痕迹（治理/任务 ID、头部元信息块）零残留 | PASS（0） |
| R3 文档内相对引用可达（81 处） | PASS（0 不可达，1 处死引用已订正） |
| R4 旧版本号残留 | PASS（0） |
| 文档数量与结构 | 36 篇与包内结构一致 |

**受影响机器门（替换后独立复跑，`ci/run_checks.py`）**

| 检查 | rc | 说明 |
|---|---|---|
| CHK-ROOT-CLEAN | 0 | PASS |
| CHK-DANGLING（含 DOC-INDEX --strict） | 0 | PASS |
| CHK-STALE-DOC | 0 | PASS |
| AGENTS-GOV | 0 | PASS |
| VERSION-NAMESPACES | 0 | PASS |
| VERSION-CONSISTENCY | 0 | PASS |
| DOC-L0 | 0 | PASS |
| CHK-IMPACT-MAP | 0 | PASS |
| ENG-CONSTRAINTS | **1** | 见 §3 UNRESOLVED-1 |
| CHK-REGISTRY-DOC-SYNC | **1** | 见 §3 UNRESOLVED-2 |

**未做（明确不做，非隐瞒）**：仓库其余 ~310 篇非权威文档（docs/contracts、docs/standards、docs/modules、docs/owner、docs/archive…）的树级风格清洗**不在本任务文件域**——DOC-001 文件域是与负责人上传文档包对齐的 `docs/` 与根 `*.md`；树级残留已量化登记为 DOC-002 工作项，清单见 `DOC_RESIDUE_INVENTORY.md`。DOC-001 验收门第 3 条的"无治理 ID 残留"按"授权文档集零残留 + 树级残留量化登记"执行。

## 3. 红灯与豁免

- 本控制包**不使用 waiver 掩盖红灯**；未过项逐条登记如下，均需负责人裁决后关闭。

### UNRESOLVED-1（P0，ENG-CONSTRAINTS 红）：文档包 §7 删去了 `ACCEPTANCE_SPEC.md`

- 现象：`ENGINEERING_SPEC.md` §7"仓库根固定条目"在文档包中删除了 `ACCEPTANCE_SPEC.md`（新旧差异中 §7 仅此一处改动）；仓库根实际存在该文件，`ci/root_manifest.json` 亦登记之 → `manifest_not_wider_than_spec7` 判红。
- 证据：`run/RELEASE-01/logs/docs/ENG-CONSTRAINTS.log`；`python3 tools/doccheck/check_engineering_constraints.py` 直跑输出 `violations=[{check: manifest_not_wider_than_spec7, detail: ci/root_manifest.json 白名单比 §7 更宽: ACCEPTANCE_SPEC.md}]`。
- 判定依据：文档包自身把 `ACCEPTANCE_SPEC.md` 列为权威链 ⑤（`ASTROCS_DESIGN.md` §0）、`AGENTS.md` §1.5 要求必读，且该文件仍在仓库根 → 倾向**文档包误删**。
- 需负责人裁决：① 恢复 §7 中的 `ACCEPTANCE_SPEC.md`（推荐）；或 ② 确认其不再是根固定条目，由前台把 `ci/root_manifest.json` 转入 `gov001_pending_owner_root_entries`。

### UNRESOLVED-2（P0，CHK-REGISTRY-DOC-SYNC 红）：文档包 §2 删去两个已注册检查行

- 现象：`docs/ci/01_CHECKS.md` §2 表删除了 `CHK-EXIT-CONSISTENCY`、`CHK-E2E-REPRO` 两行，但 `ci/checks.json` 仍注册二者 → `registered_not_documented: ['CHK-E2E-REPRO','CHK-EXIT-CONSISTENCY']`。
- 证据：`run/RELEASE-01/logs/docs/CHK-REGISTRY-DOC-SYNC.log`；`python3 ci/check_registry_doc_sync.py` 直跑输出同上。
- 判定依据：两者都是当前**可复跑且已修绿**的检查（CHK-EXIT-CONSISTENCY 扫 tools/ci 退出码自洽；CHK-E2E-REPRO 为真实数据端到端天测闭合复现门），且新文档 §2.1"已退役检查器"清单并未列入它们 → 倾向**文档包漏列**，而非有意退役。
- 需负责人裁决：① 在 §2 恢复两行（推荐）；或 ② 明确退役二者（前台按退役流程从 `ci/checks.json` 移除注册、保留检查器可复跑性）；或 ③ 补入 §2.1 退役清单。

### UNRESOLVED-3（P1，版本口径冲突）：发布版本号是 `0.1alpha` 还是 `0.0.1alpha`

- 文档包 `ACCEPTANCE_SPEC.md` §6 / `ASTROCS_DESIGN.md` §12 写 `0.1alpha`；
- 控制包 `FIN-001.md` 与 `GAP_AUDIT.md` 模板写 `0.0.1alpha`，负责人本次指示亦为 `0.0.1alpha`；
- 且 `docs/ci/01_CHECKS.md` 的 `VERSION-CONSISTENCY` 行仍要求 `--expected 0.11.0-alpha.2`（当前构建实测 `astrocs --version` = `0.11.0-alpha.2+ge88a0d89…`），与"Alpha 前代码与产物中不含任何版本信息"（ASTROCS_DESIGN §12、ENGINEERING_SPEC §7）并存冲突。
- 影响：FIN-001 的 `--version` 写入值 + 版本纪律门。需负责人在 FIN-001 前给出唯一口径。

### 其余登记（不阻塞 DOC-001，进入 AUD-001/API-DOCS）

- `export --help` 模板文案"当前唯一实现 TAN"与插件文档/实现（`lib/algorithms/projection/p3_proj_v6.cpp:270,349` 冻结 8、实现 4）不一致 → GAP 候选（见 `GAP_AUDIT.md`）。
