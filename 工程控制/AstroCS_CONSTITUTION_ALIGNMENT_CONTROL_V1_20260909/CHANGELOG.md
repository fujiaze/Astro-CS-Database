# 控制包 CHANGELOG｜ASTROCS-CONSTITUTION-ALIGNMENT-V1

## rev6（2026-09-12，前台接续重编）

**起点**：上一前台会话中断；HEAD 已推进至 `54287b48`（SCI-F2-001 部分交付：新增 §30.2 恒等式
机器门 RED 证据 + 域外最小补丁文本；acceptance=partial）；`SCI-F3-001` 子代理孤立。

**编制方法**：先取实测事实，再落任务——前台亲自回拉当前 HEAD 的 GitHub Checks 全量结果
（`run/ci_repair/round4/`）并逐条本地复现根因，**不依赖上一会话的推测性登记**。实测红灯：
linux-main 14 项非 PASS、windows-main 9 项非 PASS（其中 `UT-CLI`=`KNOWN_FAIL`、
`UT-CPU-AVX512`=`SKIPPED(waivable)`、`KNOWN-FAILURES-BASELINE-CHECK`=design 三者非红）。

**新增 10 项原子任务**（逐条承接实测红灯，映射表见 `07_FRONT_DESK_RULINGS_20260912.md` §R-04）：

| 任务 | 承接红灯 | lane | 写域 |
|---|---|---|---|
| `GOV-AGENTS-001` | AGENTS-GOV（10/10 要素 MISS） | repo-write | `AGENTS.md` |
| `CI-DATA-REG-001` | DATA-ARTIFACTS | repo-write | `docs/contracts/`、`docs/science/` |
| `CI-VER-CHK-001` | UT-VERSION（19 条误判） | repo-write | `tools/check_version_consistency.py`、`tests/version/` |
| `WCS-PATH-001` | CTEST-P1WCS-ASTROPY-CROSS（+CTEST-LINUX-FULL 级联） | repo-write | `tests/unit/p1wcs/` |
| `ARCH-TB-001` | THREAD-BUDGET + UT-ARCH | repo-write | `tools/arch/`、三个 `module_entry.cpp`、`tests/arch/` |
| `CORE-RACE-001` | CTEST-P1001-REAL-NODES（12% 假红） | repo-write | `lib/core/src/module_adapters.cpp`、`tests/unit/` |
| `CON-COMMENT-001` | CON-COMMENTS + CON-FULL-INTEGRATION | repo-write | 3 个 `lib/*/tests/`、`tools/quality/contracts/` |
| `CI-WIN-001` | WIN-BUILD-RELEASE + WIN-PACKAGE-CANDIDATE | repo-write | `lib/gaia_xpsd_client/`、`.github/workflows/` |
| `CI-BACKEND-001` | UT-BACKEND + dumpbin PATH | repo-write | `.github/workflows/`、`ci/` |
| `CI-REPAIR-002` | 常驻线 round2 + 基线只减不增 | repo-write | `run/ci_repair/`、`ci/`、`tools/quality/` |

**修订既有任务 3 项**：

- `SCI-F2-001`：写域 `lib/phase2/;tests/` → `lib/core/src/module_adapters.cpp;lib/phase2/;tests/`
  （真实修复面在 `lib/core`，原写域结构性无法修复；FD-R1-025）；依赖 `CORE-RACE-001`；状态重开 `IN_PROGRESS`。
- `STD-F1-ADJ`：`owner`/`BLOCKED_EXTERNAL` → `repo-write`/`SA-WCS`（前台裁决 R-02 取方案 b）；
  规格整篇重写为"导出边界显式 +1 桥接 + 合同条款 + 九宫格核验"。
- `CI-002`：`depends_on` 由 8 项扩至 18 项，并新增 `requires: G-CI-FIX`。

**lane 拓扑变更（R-01）**：`ci-repair` lane 冻结为历史分类、停止派发新任务；全部 tracked 写归
`repo-write`(cap 1)，依宪章 §14.5。`CI-REPAIR-001`/`CI-001B` 已 PASSED 保留历史 lane 值。

**门禁**：新增 `G-CI-FIX`（10 项）；`G-SCI` 增列 `STD-F1-ADJ`。

**新增文档**：`07_FRONT_DESK_RULINGS_20260912.md`（R-01…R-14 裁决 + B-01…B-04 待裁 + 追认清单）。

**台账**：47 → **57** 任务；PASSED 29（含本轮新增认定的 `DOC-CONV-001`）、IN_PROGRESS 1、NOT_STARTED 27。
validator CRIT 集同步；`validate_control.py --root <包目录>` → `CONTROL_PASS tasks=57`。

## rev5（2026-09-11，V2 重编）

- 台账重编为 47 任务（V1 闭环 22 项 PASSED 并入 + 25 项新任务）：R0 CI 订正
  （CI-REG-002/CI-REPAIR-001/CI-BASELINE-001）→ R1 治理标准（STD-REG-001/STD-F1-ADJ/DOC-CONV-001）
  → R2 科学缺陷（SCI-F2-001/SCI-F3-001/SCI-ANCHOR-001/UT-CLI-MAINT）→ R3 架构
  （RT-001A/B、MOD-001A/B、CLI-001B）→ R4 CI 平台（CI-001B/CI-002）→ R5 真实数据
  （REAL-001/VIS-001）→ R6 Windows（WIN-000/WIN-001）→ R7 收口。
- lanes 新增 ci-repair(1)；max_parallel 4→6；门禁链 G-STD→G-SCI→G-CODE→G-CI-L1→
  G-REAL-L2→G-FAT-L3→G-RELEASE。
- 启动快照制取代复杂冻结：STARTUP_SNAPSHOT.json 记录启动状态即可。
- V1 台账归档 TASK_LEDGER_V1_ARCHIVE.csv。

## rev4（2026-09-11，负责人 V2 设计指令）

- 新增 `06_V2_PACK_DESIGN_20260911.md`：第二阶段控制包编制总纲——三层 CI 拓扑
  （GitHub 双虚拟机编译+合成门 / 本机 Linux 真实数据 / Fatduck Windows 真实数据）、
  主线+CI 修复双线并行（多 lane，max_parallel 4→6，写域互斥分组）、轮报义务
  （每轮拉回上一轮 CI 结果）、宪章符合性任务模板、V2 任务谱系 46 任务草案。
  V1 剩余 19 任务并入 V2 谱系执行。

## rev3.3（2026-09-11，负责人 CI 指令增补）

- 登记册新增 STD-F7/F8/F9/F10：**全量 C++ ctest 仅在 linux-deep（7 项全 waivable），
  常规 CI 不强制跑本轮新增测试矩阵**——下一轮 CI-REG-002 修复（显式注册新测试、
  关键检查不可豁免、known-failures 基线机器化、第三方依赖登记）；本轮不改只记录。

## rev3.2（2026-09-11，负责人验收指令）

- 新增 `05_FINDINGS_REGISTER_20260911.md`：PM 验收 16 项闭环任务——P0=0；
  P1×3（STD-F1 CRPIX 1px 口径待负责人裁决、STD-F2 integrate 拒绝样本剔除、
  STD-F3 AIO NREJ/NUSED+provenance 通道）；P2×3（IVOA 键补强、监督任务欠账、
  **STD-REG-001 国际标准冻结注册表**）。下一轮控制包一并解决。

## rev3.1（2026-09-10，负责人三点指令增补）

1. **全量解析成功率硬门**（REAL-001 层A）：parse/校准/solve 三级各自 ≥99%
   （UNAVAILABLE 不计分母，失败≤9 帧且逐帧 reason_code，任一级<99%=FAIL）。
2. **解析正确性视觉抽检**（REAL-001 产出 + VIS-001 判定）：每数据集确定性抽 3 帧，
   全帧叠加图（检测星点 vs 逆向投影点）+ **九宫格 9×100×100px**——布局为中心 1 格 +
   四角 4 格 + 四边中点 4 格（覆盖全帧畸变梯度，非最亮星布局）；VIS-001 用视觉能力逐格
   判定重合度与偏移模式（平移/旋转/边缘发散/parity），偏移>3/27 或 parity 翻转=FINDING。
3. **Phase3 接入全流程**（REAL-001 层B + VIS-001）：两套数据集 9 个 Phase2 马赛克全部
   经 Phase3 导出平面 FITS（TAN 起步，四投影就绪后加验）+ **拉伸后预览**（负责人明确：
   不拉伸看不见）；VIS-001 增加星点形态/天区形态/parity/coverage 视觉核验节。
4. Windows 全流程范围不变（抽样复验）；「Windows 全流程 + Gaia 传输到 Fatduck」列为负责人待决项。

## rev3（2026-09-10）

负责人真实数据验收指令落地（`04_OWNER_DECISIONS_20260910.md`）：

- 新增 `REAL-000`：真实数据审计、`testdata/index.json` v1.1→v1.2（补 M42 数据集、
  multi-telescope mosaic schema、pixel_size_um 待素材文件确认）、确定性匹配工具与测试、
  逐文件 sha256 清单、缺口量化（T2/T3 300s dark 缩放 53+94 帧、T2 Lum 平场缺失 15 帧、
  T1 显式空集）。
- 扩写 `REAL-001`：三层验收——A 全量校准+解算（553 帧）、B M42+银心完整 Phase1（353 帧
  单帧 HiPS）与 Phase2（4+5 滤镜马赛克，schema/matrix 校验，GaiaDR3+GaiaDR3SP 双分支）、
  C 机器证据与资源门；断点续跑；timeout 提至 259200s。
- 扩写 `VIS-001`：固定拉伸参数预览（马赛克全图+面板接触表）、初审清单逐项勾选。
- 增补 `WIN-001`：同候选真实数据复验子集（manifest 绑定、不 checkout/编译）。
- `TASK_LEDGER.csv` 扩至 30 任务并同步状态快照（PASSED×9、PASSED_FINDING_OPEN×1、
  IN_FLIGHT×1，以 ACTIVITY_STATE.md 为真相源）。
- `control-pack.json` 与台账重新对齐（REAL-001 依赖 CI-002+REAL-000；VIS-001 转 repo-write）。
- `validators/validate_control.py` CRIT 集扩至 30。

## rev2（2026-09-10，执行侧）

- 接入负责人三项裁决与 P1 架构抽验：`WCS-003`（AP/BP 扩展+迭代反演，选择 B）、
  `BASE-UTIL-001`（utilization 归因，选择 A）、`P1-HIPS-DIGEST-001`（多 HDU digest，选择 A）、
  `ARCH-AUDIT-P1`（独立架构抽验）；G-SCI 增加 WCS-003，RT-001 增加 BASE-UTIL-001/WCS-003，
  AUD-001 增加 ARCH-AUDIT-P1/P1-HIPS-DIGEST-001。

## rev1（2026-09-09）

- 初版 25 任务、6 门禁、cprun/v4 图、evidence schema、双校验器。
