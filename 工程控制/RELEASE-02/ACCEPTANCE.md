# 工程控制 / RELEASE-02 验收记录（ACCEPTANCE）

> PASS 仅由前台独立验证后写入；证据为前台独立复跑结果，不复用 SubAgent 自述。不用 waiver 掩盖红灯。

## 1. 总表

| 任务 | 状态 | 机器门/证据 | 前台结论 |
|---|---|---|---|
| DOC-101 | **PASS** | 核验器 `工程控制/RELEASE-02/verify_doc_pack.py`（R1_missing=0 / R1_hash_mismatch=0 / R2_residue=0 / R3_unreachable=0 / R4_stale_version=0，verdict=PASS，授权差异 4 条）；`ENG-CONSTRAINTS` PASS；`CHK-REGISTRY-DOC-SYNC` PASS（2/2）；旧内容残留扫描 0 命中 | 36 篇替换核验通过；U-1/U-2 随新包闭合 |
| FIX-SCI |  | 变更 claim；science_contract_lint、CHK-SCI-REF |  |
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
| SD-02 | 新文档包内部**自相矛盾**：同包 `ASTROCS_DESIGN.md` §153 已订正天光口径，但 §235 仍留旧措辞「天光作为独立背景分量扣除，**不受天光影响**」与旧「信号功率/稳健噪声功率」表述 | 同包 `ASTROCS_DESIGN.md:153`、`docs/plugins/algorithms_phase1/07_noise_snr.md` §4.1（本包已订正为「信号项不被加性天光背景虚高；天光散粒噪声计入 σ_n」）；DOC-101 步骤 4 亦点名「旧天光措辞『不受天光影响/不漂移』」为待扫残留 | `ASTROCS_DESIGN.md` §235 订正为「真实 PSF 源信号/稳健噪声，通量型口径 `F_ref/σ_F`；信号经独立局部背景估计与扣除、不被加性天光背景虚高，天光散粒噪声如实计入 σ_n」 | 无 | 核验器 PASS（R1–R4 全 0，该处登记为第 4 条授权差异）；残留扫描 0 命中；正向确认「不被加性天光背景虚高」「天光散粒噪声」在位 |

## 4. 上呈事项（穷尽：仅 §3a 列明类别）

| 事项 | 证据 | 方案与代价 | agent 推荐 |
|---|---|---|---|
|  |  |  |  |

## 5. 提交台账

| commit | 目的 |
|---|---|
|  |  |
