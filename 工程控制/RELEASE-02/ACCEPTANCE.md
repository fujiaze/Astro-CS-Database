# 工程控制 / RELEASE-02 验收记录（ACCEPTANCE）

> PASS 仅由前台独立验证后写入；证据为前台独立复跑结果，不复用 SubAgent 自述。不用 waiver 掩盖红灯。

## 1. 总表

| 任务 | 状态 | 机器门/证据 | 前台结论 |
|---|---|---|---|
| DOC-101 | **PASS** | 核验器 `工程控制/RELEASE-02/verify_doc_pack.py`（R1_missing=0 / R1_hash_mismatch=0 / R2_residue=0 / R3_unreachable=0 / R4_stale_version=0，verdict=PASS，授权差异 4 条）；`ENG-CONSTRAINTS` PASS；`CHK-REGISTRY-DOC-SYNC` PASS（2/2）；旧内容残留扫描 0 命中 | 36 篇替换核验通过；U-1/U-2 随新包闭合 |
| FIX-SCI | **PARTIAL** | 4 件变更 claim（DRZ-001/WCS-001/S2-PHOT-001/S2-P3-001）；`science_contract_lint` 4 文件 15 节 **PASS**（前台复跑）；`CHK-DANGLING` **PASS**；`CHK-SCI-REF` 唯一红点为**并行分片 FIX-A** 的 `C4_symbol_binding P2SMP-CELLSIDE`（前台已复现并确认非本任务，已发消息要求 FIX-A 收尾前修正）；DOI 经 ADS 核验真实（`2002PASP..114..144F`、`2017ApJ...836..187Z`） | 两项 P0 订正落地且 §5/§7 已自洽；**遗留 DISP-DRZ-009**（drizzle 实现仍 legacy 归一，默认 pixfrac=1 逐位不变）→ 门「文档-实现一致」未完全闭合 |
| FIX-A 天光面链 |  | 合成恢复误差；接缝行差倍数；内存实测 | P0-08/09/10 归宿 |
| FIX-B 方差链 |  | ivar 产品；cosmetic 修复；frame_snr 入头；noise_snr 构建 | P0-02/03/04/05/06/07 归宿 |
| FIX-C 集成链 |  | 方差最小性；variance_available；词表；sparse layer | P0-11/12/13 归宿 |
| FIX-D CLI/export |  | 三模板合同测试；预检三级颜色 | P0-14 归宿 |
| FIX-E 性能 |  | 多核/内存/缓存指标（合成与真实） | 性能违约群归宿 |
| TST-101 |  | 负例覆盖率；零测试模块；fixture 解锁；ctest |  |
| BLD-101 |  | cmake/ninja 0 警告；ctest 全绿；全部机器门 | P0-01/16 归宿 |
| E2E-102 |  | L3/L4 rc；科学路径核查；1/N 一致；WCS 闭合 |  |
| VIS-102 |  | L4 §5.2 逐项；行差 ≤2×；裁剪放大 |  |
| PERF-102 |  | G-RES-01 六判据真实数据面零违约 |  |
| DEL-102 |  | 两个 FITS 结构/数值/视觉/单位；限制清单 | 待负责人检查 |

## 2. P0 归宿台账（逐条 CLOSED / 变更 claim / 上呈）

| P0 | 结论 | 证据（提交/报告/测试） |
|---|---|---|
| P0-01 模块门 |  |  |
| P0-02 ivar 产品 |  |  |
| P0-03 frame_snr |  |  |
| P0-04 校准方差 |  |  |
| P0-05 cosmetic 空转 |  |  |
| P0-06 cosmetic 方差 |  |  |
| P0-07 noise_snr 构建 |  |  |
| P0-08 采样/掩膜 |  |  |
| P0-09 天光面 |  |  |
| P0-10 g_k 接线 |  |  |
| P0-11 集成方差 |  |  |
| P0-12 sparse layer |  |  |
| P0-13 weight_mode |  |  |
| P0-14 export 合同 |  |  |
| P0-15 版本号 | 不在本包（FIN 阶段处理） | 负责人裁决：全部验收通过后再改 |
| P0-16 文档↔仓库冲突 | **CLOSED** | 新文档包 `ENGINEERING_SPEC.md` §7 根白名单补 `ACCEPTANCE_SPEC.md`（修 U-1）；`docs/ci/01_CHECKS.md` §2 补登记 `CHK-E2E-REPRO`、`CHK-EXIT-CONSISTENCY`（修 U-2）。前台独立复跑：`ENG-CONSTRAINTS` verdict=PASS、`CHK-REGISTRY-DOC-SYNC` verdict=PASS 2/2 |
| P0-17/18（二轮增量） |  |  |

## 3. 自决闭环留痕（§3a 授权事项）

| 编号 | 发现 | 研究证据 | 文档订正 | 代码/门禁变更 | 验证结果 |
|---|---|---|---|---|---|
| SD-01 | DOC-101 核验器 R4 把**授权发布版本串** `0.0.1alpha` 误判为旧世代残留（5 处假阳性）；根因：RELEASE-01 版定义了 `AUTHORIZED_VERSION_TEXT` 却**从未使用**（死代码） | 本包 ACCEPTANCE_SPEC.md:13/158/183、ASTROCS_DESIGN.md:533、ENGINEERING_SPEC.md:113 均为**发布纪律正文**，属授权文本；旧版 `0.1alpha` 不匹配 `0.0.\d+-?alpha` 故当时未暴露 | 无（文档本身正确） | `工程控制/RELEASE-02/verify_doc_pack.py`：R4 增加授权版本串例外；判据抽成可单测 `scan_text()`；新增 `--self-test`（R2/R3/R4 正负例 6 项） | `--self-test` **6/6 通过**（授权版本串 clean、旧版 `0.0.9-alpha`/`V19` hit、`CHK-*` clean、治理 ID hit、可达引用 clean、死引用 hit）；改后核验器 R4=0 且 verdict=PASS |
| SD-03 | RELEASE-01 SCI-001 的 P0-1：`DRIZZLE.md` §5 归一 `S_p=B0/pixfrac²` 与 §7 不变量 `S_p=B0` **互斥** | Fruchter & Hook 2002 PASP 114,144 式(5)（DOI 10.1086/338393，ADS 核验真实）为**一致加权均值**、drop 面积在分子分母相消；DrizzlePac Handbook §2.3.2；drizzlepac/SWarp 只读对照 | §5/§7/§11 订正为面亮度保持 `S_p=Σ_j B_j a_jp/Σ_j a_jp`、`w=a_jp/A_pixel,j`、条件通量守恒 `Σ_p F_p=pixfrac²Σ_j x_j`（claim `FIX-SCI-DRZ-001`） | **未改实现**：`drizzle_engine.cpp:1531` 仍 legacy `weight=overlap_area/drop_area`，登记 **DISP-DRZ-009**（最小修复 `weight=overlap_area*pixfrac²/drop_area`，默认 pixfrac=1 逐位不变） | 前台复算：`w=a/A_pixel` 且常数面亮度 `x_j=B0·A_pixel` ⇒ `S_p=B0`（与 pixfrac 无关）；legacy 给 `B0/pixfrac²`，证实 §5 错、§7 对；§5/§7 改后自洽；`science_contract_lint` PASS |
| SD-04 | RELEASE-01 SCI-001 的 P0-2：`ASTROMETRY.md` §5a 的「1px 平移」口径 | 求解器拟合自变量为 sdet 半整数像素中心 `det_x=i+0.5`（`sdet_api.cpp:546-549`），`u=det_x−w/2=p−CRPIX=q`（Paper I §2.1.1 式(1)）；astropy 7.0.1 复算 + fixture 独立 TAN+SIP 复算 + T4 大尺度闭环（solved median 0.1077 px） | §5a/§14a 与 `STANDARDS_REGISTRY` STD-F1 标签订正：旧「0-based 常量 1px 平移」是**标签错误**，实现与 Paper I **逐式一致、无 1px/0.5px 误差**（claim `FIX-SCI-WCS-001`） | **未改实现**（结论为实现正确）；建议 comment-only 改 `ipv_wcs.h:43,57-60,70-71` 与测试面 origin relabel | 前台确认订正后 §5a 与 STD-F1 表述一致；`science_contract_lint` PASS；风险 R2（半整数探测与 `cx=w/2` 的 0.5 相消无测试锁定）已登记 |
| SD-02 | 新文档包内部**自相矛盾**：同包 `ASTROCS_DESIGN.md` §153 已订正天光口径，但 §235 仍留旧措辞「天光作为独立背景分量扣除，**不受天光影响**」与旧「信号功率/稳健噪声功率」表述 | 同包 `ASTROCS_DESIGN.md:153`、`docs/plugins/algorithms_phase1/07_noise_snr.md` §4.1（本包已订正为「信号项不被加性天光背景虚高；天光散粒噪声计入 σ_n」）；DOC-101 步骤 4 亦点名「旧天光措辞『不受天光影响/不漂移』」为待扫残留 | `ASTROCS_DESIGN.md` §235 订正为「真实 PSF 源信号/稳健噪声，通量型口径 `F_ref/σ_F`；信号经独立局部背景估计与扣除、不被加性天光背景虚高，天光散粒噪声如实计入 σ_n」 | 无 | 核验器 PASS（R1–R4 全 0，该处登记为第 4 条授权差异）；残留扫描 0 命中；正向确认「不被加性天光背景虚高」「天光散粒噪声」在位 |

## 4. 上呈事项（穷尽：仅 §3a 列明类别）

| 事项 | 证据 | 方案与代价 | agent 推荐 |
|---|---|---|---|
|  |  |  |  |

## 5. 提交台账

| commit | 目的 |
|---|---|
| `3de869ae` | DOC-101 文档包替换与核验（36 篇；U-1/U-2 闭合；核验器 R4 判据改进 + self-test；ASTROCS_DESIGN §235 口径订正） |
