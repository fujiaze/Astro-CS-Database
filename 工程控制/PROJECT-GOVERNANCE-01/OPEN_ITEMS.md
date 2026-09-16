# OPEN_ITEMS —— 待办登记（前台维护，2026-09-16）

> 来源：R-1..R-7 研究线 + 六条科学订正线 + CI-003 + ROOT-008 的「未做项 / 转办 / 残余发现」汇总。
> 每条含：**是什么 / 归属 / 为什么要等**。已派单的不在此列。

## A. 已批准、待派单

| # | 事项 | 归属 | 依据 |
|---|---|---|---|
| A1 | **像素中心契约口径边界**：`psf` 块（dpsf 原始 index-is-center 坐标）直喂 PHOTOMETRIC/SNR，而 WCS 为 FITS 1-based ⇒ 该域存在独立的中心约定问题 | 新任务（PSF/口径域） | SCI-FIX-PSF 残余发现；SCI-WCS-001 §5a 口径边界 / DISP-WCS-006 家族 |
| A2 | **真实数据台账锚重测**：`ALG-WCS-001 §11.4 Galaxy_Center rms_arcsec=0.1431″`、`memory.md 0.897px` 均为**修复前**产品；坐标契约修正后必须重测 | 新任务（真实数据域） | SCI-FIX-PSF §6 重锚清单 |
| A3 | **17 条既有红 + `test_impact_map` 4 条既有红** 收口（含 R-6 建议的 `CHK-IMPACT-MAP`） | 新任务（门禁收口） | CI-003 自证摘要 §4；CI-003 已给逐条归因 |
| A4 | **`CHK-CONTRACT-TEST` 的 C7 解析缺陷**：旧 checker 把复合 `test_path` 当单路径（p1cal/p1noise/p1phot 早已同型报错，净 7→7） | CI 域 | SCI-FIX-PSF §5 |
| A5 | **`check_standards_registry.py --fault-inject drop-wcs003f1-pointer` 空转**：只有单向规则，缺「条款表 ID 必须出现在 DEVIATION 字段」反向规则 | CI 域（ENGINEERING_SPEC §8 双向一致） | SCI-FIX-PSF 发现；CI-003 已登记 |
| A6 | **同常数残留清理**（`0.6745` / `1.4826` / `0.7316728`）：photometry 9 文件、8 个算法域、12 份文档、ACR、`docs/ARCHITECTURE.md:124`、`tools/docs_machine_consistency.py`（其源码路径已被迁移改坏） | 分域派单 | SCI-FIX-NOISE §9.3 清单 |
| A7 | **同款宪章残留引用**：`lib/algorithms/drizzle/README.md:100`（与 DATA_SEMANTICS :248 同源）+ `docs/{contracts,science,algorithms}/v6/**` 旧世代档案引注 | 文档域 | SCI-FIX-PROJ 收尾报告 |
| A8 | **掩膜 `rmax=60px` 整帧退化**：256² 帧仅 50 颗星即 `rc=1`（Phase2 权重全失）；动它要改 `SCI §6:67` 冻结条款 | 需负责人批准后才派 | R-5 附带发现（已登记） |
| A9 | **INT-001 剩余范围**：3 个 `*_session` 目录删除、`lib/infrastructure/cli/**` 接线三步（`add_subdirectory` / include 4 目录 / `astrocs_cli_subcommands` 链接）、`cli/CMakeLists.txt` 最终处置 | INT-001 | ROOT-008 交接清单 |
| A10 | **版本递增建议**：SCI-FIX-PSF 使 `star_measurements` PSF 行 x/y +0.5px（变为科学正确值）⇒ 建议 `0.11.0-alpha.2 → alpha.3` | **负责人决定**（发布权） | SCI-FIX-PSF §6 |

## B. 等待/依赖

| # | 事项 | 状态 |
|---|---|---|
| B1 | 全量 `ninja -C build -k 0` 0 FAILED | 待 GATE-FIX-RES（生成头 include 7 条）、SCI-FIX-AIO（ABI 签名 1 条）、SCI-FIX-WEIGHT（`v15_plan` 1 条）收敛后由前台复跑 |
| B2 | 锚点门 rc=0 | 待上述线收敛（余 C4 = CAL/COS-CMAKE、DRZ-MAXANGLE、P2SMP-CELLSIDE 属各域） |
| B3 | `问题扫描/**` 台账的两处判据订正（M7-A-117 / M7-A-129） | 等与隔壁挖掘线协调归属后由前台落库（内容已核） |
| B4 | `memory.md` 纳入 AGENTS-GOV / ENG-CONSTRAINTS 扫描面 | 已批准（CI-003 实测扩面后仍 rc=0），待其执行 |
