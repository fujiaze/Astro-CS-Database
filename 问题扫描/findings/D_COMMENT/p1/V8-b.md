# V8 片2-3 · P1（锚归属与裁决登记）

### V8-N-07（P1·判据③，**已挂 `A-44` 待负责人确认**）「负责人裁决 2026-09-14」被 39 行援引，其中**两族在权威登记面 0 条目**
- 计量：新增行含「负责人裁决/裁定/授权/指令/明确要求」**49 行**，其中同钉日期 `2026-09-1x` 的 **39 行**（`scripts_v8_passF_owner.py`）。登记面核对：`grep -n "09-14" memory.md` → 空；`grep -n "2026-09-14" 问题扫描/40_OWNER_DECISIONS.md` → 空；`grep -n "PSF-FAST|只留 fast|max_stars" CHANGELOG.md` → 空；`grep -n "帧头|CRVAL|init_source" memory.md 40_OWNER_DECISIONS.md` → 空。
- **三族拆解（只两族有问题，另一族是正对照）**：① **P5-SNR 授权**（约 20 处）= `CHANGELOG.md:9`「P5-SNR 逐源 SNR 科学修正（2026-09-14，负责人授权，`scientific_change=YES`）」+ `:11` 授权说明 ⇒ **已登记**，同批正例；② **PSF-FAST-001「只留 FAST + 精确路径保留」12 处**（`module_adapters.h:30`、`module_adapters.cpp:1599/1754/1791/6162`、`dynamic_psf/README.md:204/218`、`module.yaml:157`、`phase1_session/README.md:49`、`p1phot_fixgates.cpp:13/90`）**登记面 0**；③ **P9「帧头 WCS 未授权」6 处**（`DATA_SEMANTICS.md:788/791-792` **含"裁定原文"引号**、`lib/core/README.md:15`、`module_adapters.cpp:1966/2207`）**登记面 0**，且「裁定原文」的唯一仓内宿主**就是合同自己那一行** ⇒ **自指成环**。
- **后果**：按准绳①负责人意图是最高权威，但其载体应为 `40_OWNER_DECISIONS.md`/`memory.md`/AGENTS 十要素/宪章 §18；**这两族决定的正是"死码保留"与"冻结合同改写"**（与 `V8-N-01` 杜撰测试符号互为表里：注释说"有直调测试防清理"、另一处说"负责人裁决保留"，**两个凭据一个虚构一个未登记**）。宪章 §1.2 禁止 Agent 新设裁决条款。
- **需负责人确认（我挂 `A-44`）**：若确有其裁决 ⇒ 补登记到 `40_OWNER_DECISIONS.md` 并在合同行后加编号指针；若没有 ⇒ **P9 是在合同里铸权威**，须撤下并回退相关冻结文本。
- **related** `A-42`（OWNER-07 未落档同族，不重报）、`V13-N-04`（VERIFIED 自指）、`V8-N-01`、**`C-18`**（编号须先登记才可引用）、`C-15`

### V8-N-04（P1·判据②⑤）证据外置的**新形态**：裸 `REPORT.md §N` 锚，**仓内连"去哪找"都没写**（15 处，其中 12 处无路径前缀）
- 站点（`scripts_v8_tmp_q3.py`，7912 新增行内）：`star_detector.cpp:19`「`p1_sources.json`/`p1_flux.json` **逐字节不变（REPORT.md §3 给 bit-pattern 对照）**」、`module_adapters.cpp:1605`「最亮 5000 颗 ~4 s（REPORT.md §4）」、`dpsf_psf.cpp:112`「末位 ulp 可能与 `std::pow` 不同（REPORT.md §2 量化）」、`dynamic_psf/README.md:233`、`phase1/stars/README.md:23`、`phase1/noise/README.md:58`、`ipv/test/CMakeLists.txt:13`、`test_extract_wcs_sip_failclosed.cpp:25`「见 REPORT"越域残余"」、`sdet_angle_guard_test.cpp:10`。
- **宿主三级核对**：`ls REPORT.md` → **无该根文件**；`git ls-files` 全仓唯一在册同名件 = `lib/plate_solve/cpp/ipv/REPORT.md`，其节为「§2 算法实现/§3 测试结果(k-vector)/§4 与设计文档的差异/§6 后续 Phase 规划」⇒ **与被引内容不对应**；`git grep -c "bit-pattern" -- .` → **只命中宣称它存在的那行注释**；`grep -rl "越域残余" run/` → **空（连工作区都没有该章节）**。
- **为何比 `V6-N-04` 更弱**：显式 `run/...` 至少让复核者知道"在仓外"；**裸名在干净检出后无可解析目标**，且**仓内同名件会把复核者指向错模块**。承载的恰是最需要凭证的宣称（逐字节不变、末位 ulp、耗时、钩子归属）。
- **门侧要求（我已据此扩工单 `E5`）**：`E5` 现判据「串里含 `run/` 即红」**抓不到本形态** ⇒ 扩为「**裸文档名 + `§N` 引用（无仓内可解析宿主）即红**」。
- **related** **`V6-N-04`**、`V6-N-05`（`README:58` 那处已入账，本条补其余站点族量）、`V12-N-06`、`V8-N-06`、`C-15`、**工单 `E5` 扩展**
