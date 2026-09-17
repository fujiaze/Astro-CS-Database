# 工程控制 / RELEASE-01 验收记录（ACCEPTANCE）

> PASS 仅由前台独立验证后写入；证据为前台**独立复跑**结果，不复用 SubAgent 自述。

## 1. 总表

| 任务 | 状态 | 机器门结果 | 证据路径 | 前台结论 |
|---|---|---|---|---|
| DOC-001 | **PASS**（含 UNRESOLVED-1/2/3 上呈） | 文档包核验器 PASS；受影响检查 8/10 PASS，2 红为文档包↔仓库冲突 | `工程控制/RELEASE-01/DOC_PACK_MANIFEST.json`、`run/RELEASE-01/logs/docs/` | 36 篇替换到位、治理痕迹清零、引用可达、一致性基线登记 |
| AUD-001 | **PASS**（交付物齐备；差距本身未闭合） | 分片复跑命令见各报告 §4 | `工程控制/RELEASE-01/GAP_AUDIT.md` + `reports/RELEASE-01/audit/AUD-A{1,2,3,4}-*.md` | 23 模块 + 横向全覆盖；P0=16（去重）、P1=87、P2=35、UNRESOLVED=11 主题 |
| SCI-001 | IN_PROGRESS（S1 PASS / S2 运行中） | S1 复跑脚本 `reports/RELEASE-01/science/exp_*.py` | `reports/RELEASE-01/science/SCI-S1-snr-psfsw.md`、`SCI-S2-topics.md` | SNR/PSFSW 专项 7/7 完成；5 项"我方有误"已定位 |
| BLD-001 | **PARTIAL** | cmake rc=0 / ninja rc=0 / 0 警告；ctest 442 = 430 通过 + 12 跳过 + **0 失败**（404.3 s）；**CHK-MODULE-MANIFEST 红** | `run/RELEASE-01/logs/BLD-001/build_ctest.log` | 构建与全量单测绿；"机器门绿"未达（P0-01/P0-16） |
| TST-001 | IN_PROGRESS | — | `run/RELEASE-01/tests/` | 运行中 |
| DOC-002 | **PASS** | 订正后 `verify_doc_pack.py` 复跑 PASS；Mermaid `mermaid.parse` 30/30 | `reports/RELEASE-01/docs/DOC-002-selfcheck.md` | 38 篇自查；6 处非语义订正（前台已逐条复核 + 补 manifest） |
| E2E-001 | IN_PROGRESS（L3 **PASS** / L4 运行中） | L3 两组 3 命令 rc 全 0；`weight_mode=2` 负例 rc=2 | `run/RELEASE-01/e2e/l3/logs/`、`l3/p3_{m42,gc}/output_phase3.fits` | L3 全链打通；L4 R 通道全量 81 帧运行中 |
| VIS-001 | NOT_STARTED | — | — | — |
| PERF-001 | NOT_STARTED | — | — | — |
| DEL-001 | NOT_STARTED | — | — | — |
| FIN-001 | NOT_STARTED（前置未满足） | — | — | — |

## 2. 各任务独立验证记录

### DOC-001 文档包替换核验

- 来源 `AstroCS文档集.zip`，sha256 `fe7a0a4002b5d72c25b073e02c0633c8189578ea62efced82cd64d1ddcf3a483`，36 篇；23 篇逐字节相同 / 12 篇更新 / 1 篇新增；
- 前台独立复跑：`python3 工程控制/RELEASE-01/verify_doc_pack.py` → 权威文档 36 篇、授权差异 **8** 条、相对引用 81 处、R1–R4 全零、`verdict=PASS rc=0`；
- 授权差异（8 条）：3 条 DOC-001 治理痕迹/死引用清理（`ENGINEERING_SPEC.md`、`14_projection.md`、`05_platesolve.md`）+ 5 条 DOC-002 非语义订正（`03_GATES.md`、`02_PIPELINE.md`、`17_aio.md`、`03_star_detection.md`、`UNIFIED_MODEL.md`）；
- 受影响机器门 8/10 PASS：CHK-ROOT-CLEAN / CHK-DANGLING（含 DOC-INDEX --strict）/ CHK-STALE-DOC / AGENTS-GOV / VERSION-NAMESPACES / VERSION-CONSISTENCY / DOC-L0 / CHK-IMPACT-MAP；**2 红**：ENG-CONSTRAINTS、CHK-REGISTRY-DOC-SYNC（→ §3 UNRESOLVED-1/2）。

### AUD-001 设计-实现差异审计

- 4 个分片 SubAgent（normalize 8 / mosaic 5 / export 3 + CLI / infrastructure 7）**只找不改**，全部未改受版本控制文件；
- 覆盖：23 个模块每模块 ≥1 条结论（含 8 条"无差距"）+ 5 条横向条款；
- 差距：**P0 去重后 16 条**、P1 87 条、P2 35 条；UNRESOLVED 归并为 11 个主题；
- 前台已核：分片报告的 P0 证据行均可复现（抽查 `CHK-MODULE-MANIFEST`、`sky_plane` 零命中、`p1_final.json n_variance_tiles=0`、`--version` 输出、`session_commands.h:150` 文案）。

### SCI-001 科学自审（S1 完成部分）

- 研究包 7/7 任务完成；8 个开源实现源码级核验（Siril/SWarp/DSS/SExtractor/SEP/photutils/properimage/SCAMP），photutils 3.0.0 数值对拍通过；
- 结论"我方有误"5 项：PSFSNR 公式（应为 `c3·(Σf)²/(c4σ_n²)`，非 `√(Σf²)`）、`psf_snr_power` 实为 DEFERRED、研究包混引 ZOGY 与 How-to-COAAD-I、DeepSkyStacker 许可证/URL、SEP 许可证；
- 文档订正 6 处（`PSF_SIGNAL_WEIGHT.md` §3/§4、`NOISE_MODEL.md` §14a.1、`SCIENTIFIC_REFERENCES.md`）；新增文献 9 条 + 参考代码库 8 项；
- 前台已核：SCI-S2 对 `docs/science|algorithms` 的 45 文件改动为 1027 增 / **2 删**，删除项仅 `psf_snr_power` 行改为 DEFERRED（有代码证据），其余为新增"参考文献与参考代码库"节；**未改公式**（新出现的公式行为官方式[16]/[18]/[20] 的**引用披露**）。

### BLD-001 逐模块并行构建与冒烟

- 前台独立复跑（非 SubAgent 自述）：`cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release` rc=0 → `ninja -C build` rc=0（0 warning / 0 error）→ `ctest --test-dir build --output-on-failure` rc=0，**442 用例：430 通过 / 12 跳过 / 0 失败**，耗时 404.30 s；
- 12 个跳过用例清单已归档（`phase2_synthetic_gate` ×3、`phase2_sampler_parallel` ×1 等），TST-001 正在判定是否属 SKIP 充数；
- **未达项**：验收门"机器门绿"不成立——`CHK-MODULE-MANIFEST` 红（负责人已裁决的临时红，P0-01）+ `CHK-REGISTRY-DOC-SYNC` 红（P0-16）；逐模块独立构建/冒烟的**逐模块证据表**待 TST-001 汇总后补。

### DOC-002 其他文档自查

- 38 篇（根 5 + docs/ci 5 + docs/plugins 24 + docs/design 4）；
- 6 处非语义订正（前台已逐条 diff 复核，行数不变）：`03_GATES.md` §11.3→§11.4 与 `§2.3`→`§2.1`、`02_PIPELINE.md` §11.3→§11.4、`17_aio.md` 模块路径、`03_star_detection.md` 去脆性行锚、`UNIFIED_MODEL.md` 表格错列；
- Mermaid：作用域 30 图，结构检查 + `mermaid.parse` 30/30 PASS（浏览器级渲染因 Chromium 下载失败未完成，环境限制）；
- 移交 AUD-001 的 7 项代码不一致 + `config_registry.json` 23 条行锚失配，均已并入 GAP_AUDIT §3/§5。

### E2E-001 全流程真实数据端到端（L3 小批量，前台独立复跑）

| 组 | 数据 | normalize | mosaic | export | 产物 |
|---|---|---|---|---|---|
| M42 | T2 M1 Red 300s ×2 + T3 M1 Red 300s ×6（600s dark，K=0.5） | rc=0 | rc=0（wm1） | rc=0 | `l3/p3_m42/output_phase3.fits` 2048²，RA---TAN/DEC--TAN，CRVAL 83.21382/−6.306814，BUNIT ADU |
| Galaxy Center | T4 panel1 Red 180s ×4 + panel2 Red 180s ×4（180s dark） | rc=0 | rc=0（wm1） | rc=0 | `l3/p3_gc/output_phase3.fits` 2048²，RA---TAN/DEC--TAN，CRVAL −87.148208/−15.664133，BUNIT ADU |

- **负例面**：`weight_mode=2` 两组均 rc=2，报 `weight_mode=2 requires per-frame ivar products; 2/2 frames missing ivar … set legacy_allow_weight_fallback=true` → 与 P0-02 一致，fail-closed 语义正确；
- 科学抽检：WCS 解算 Gaia 闭合 RMS 0.1028″（M42 T3M1，inliers 37/64）、0.3242″（GC panel1，inliers 41/63）；往返 ~1e-10 px；`p1_final.json` 显示 `n_tiles=n_support_tiles=112/275`、`n_variance_tiles=n_ivar_tiles=0`；
- 数值抽检：两个成品帧全有限、零 NaN、零零值；中位数 8.44e12（M42）/ 4.64e11（GC）ADU —— **单位口径待裁决**（BUNIT=ADU 与 HiPS 面元归一不一致，P1/P0 候选，见 GAP_AUDIT §5 B-6 同族）。

## 3. 红灯与豁免

- 本控制包**不使用 waiver 掩盖红灯**。

### UNRESOLVED-1（P0）：文档包 `ENGINEERING_SPEC.md` §7 删去 `ACCEPTANCE_SPEC.md`
### UNRESOLVED-2（P0）：文档包 `docs/ci/01_CHECKS.md` §2 删去 `CHK-EXIT-CONSISTENCY`/`CHK-E2E-REPRO`
### UNRESOLVED-3（P1）：版本口径四冲突（`0.1alpha` / `0.11.0-alpha.2` / `0.0.1alpha` / "Alpha 前无版本"）

> 三者细节与证据见本文件上一版（commit `41b41e2d` 版 §3）与 `GAP_AUDIT.md` §5 U-F/U-G。
> **新增关键证据（前台查证）**：`CHK-EXIT-CONSISTENCY` 与 `CHK-E2E-REPRO` 是**前批 agent 自己引入**的注册项——`git log -S'CHK-EXIT-CONSISTENCY' -- ci/checks.json` 仅命中 `5eb1433f`，`-- docs/ci/01_CHECKS.md` 命中 `bd300e85`（新增）与 `41b41e2d`（本次删除）；两个检查器文件亦由 `5eb1433f` 新增。即：负责人文档包从未登记这两项。**建议**：按 `docs/ci/01_CHECKS.md §2.1` 退役流程从 `ci/checks.json` 移除注册（保留检查器可复跑性），而非回写文档包。

### 范围冲突（须负责人裁定，决定结论走向）

新文档包引入实现中不存在的语义（稀疏天光面、locality-aware 编排与流式内存、Gaia 两级缓存与查询合并、GLS/Q-W/psfsw 三目标、variance/ivar 链），而 `00_README.md §2` 明确本轮"不引入新功能"。二者互斥，须裁定：**(a)** 这些属发布阻断项 → 当前实现不可发布；**(b)** 属下一版本路线图 → L2/L3 中依赖它们的验收条目不能按新文档判绿，需在发布说明中如实声明。详见 `GAP_AUDIT.md` §4。
