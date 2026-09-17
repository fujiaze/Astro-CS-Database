# PROJECT-GOVERNANCE-02 · 待办清单（入口快照）

> 快照时点：HEAD=`9902c788`（已推送，未推送 0）。本文件只登记**未完成项**；完成项在各自线自证摘要与提交消息中留证。

## A. 提交面（后台职责，先做）

| 项 | 内容 |
|---|---|
| A1 | 工作区 **10 个未提交文件**：`ci/checks.json`（注册表 45 项）· `ci/polarity_probe.py` + `ci/polarity_evidence.json`（新门极性记录）· `ci/id_migration_map.json` · `ci/tests/test_deep_profiles.py`/`test_windows_ci.py`/`test_workflow_lock.py`（重算式判据）· `docs/ci/01_CHECKS.md`/`03_GATES.md` · `tools/quality/check_ctest_registration.py`（foreach 展开） |
| A2 | 本批治理附录（生成物只许改生成器 · 空扫描面假绿 · 判据复算>冻结数字 · 判据作者也是受试者）+ 本包文件 |

## B. 端到端（**未跑通**，本包核心）

| 项 | 现状 |
|---|---|
| B1 逐命令冒烟 | `normalize`/`mosaic`/`export` **未逐一在干净环境跑通**（已验的是标定链与天测闭合片段） |
| B2 真实数据全链 | T2/T3/T4 的 `normalize→mosaic→export` 全链**未跑**；已验：T2/T4 标定链、T3/T2/T4 天测闭合（0.5644/0.4202/0.1077 px） |
| B3 失败路径 | 各命令的 rc≠0 + 零产物 + 逐字诊断**未系统取证** |

## C. 台账（未清零）

| 台账 | 余量 |
|---|---|
| SCAN-CLEAR | **606 OPEN**（已路由/转办未逐条关闭） |
| LEDGER-P1 | **141 OPEN** |
| LEDGER-CI | **144**（37 转 W4-A3 含 patch · 93 域外 · 15 待办） |
| LEDGER-DOC | **49 + MOD-002 大面** |

## D. 验收收口

| 项 | 现状 |
|---|---|
| D1 claim 编号 | **未归一**：`SCIENCE_CORRECTNESS.md` 两行同为 `SC-010`，另有 SC-008/SC-009 撞车；无新旧对照 |
| D2 `ACCEPTANCE_FINAL.md` | **不含实测值**（grep 实测命中 0） |
| D3 `TASK_LIST_STATUS.md` | 未更新 |

## E. 科学与契约余项

| 项 | 内容 |
|---|---|
| E1 D08 | 产品侧标记载荷卡（911 份 calibrated 中 0 份带 ASTROCS* 标记/HISTORY）+ §18.5 标记义务 |
| E2 D11 | ipv 分支改 SIP-aware 参考解 + 分别写 `forward_cross_ref`（现对 SIP 腿零鉴别力：12.26 px vs 信号 10.86 px） |
| E3 AIO 元数据提案 | 4 问待裁；实施须与 DET-001 `canonical_sha256` 哈希面同批验证 |
| E4 ABI | `drizzle`(`astrocs_p1_drizzle`) 与 `gaia_xpsd_client`(`astrocs_catalog_gaia`) 两个 SHARED 面结构体补 `struct_size`/`abi_version` + 库侧 `-9` + 布局锁 + 突变自检 |

## F. 门与基建余项

| 项 | 内容 |
|---|---|
| F1 负载敏感 7 条 | 仅交付协议规范 + patch 骨架；**未实施**（改测量协议不改阈值） |
| F2 E2E 新门 CI 注册 | 未落（W4-A3 域） |
| F3 `CHK-PKG-CONSISTENCY` | polarity 记录已补（有真跑负例 rc=2），后续义务：若退出码语义变更需同步复验 |
| F4 `inc/demo.h` | 疑似未登记路径，未核查（可能撞 ROOT-CLEAN 类门） |
| F5 域外 93 条 | 需逐条确认接手方（D10 TRANSFERRED 必须具名） |

## G. 迁移余项

| 项 | 内容 |
|---|---|
| G1 W4-A9 批次 4 | `hips_properties* → lib/algorithms/coverage/` + resample TU 收口（开工包已备，待令） |
| G2 未跟踪残留 | 17 项（多为邻居 root-scan 报告，不属本包提交面） |
