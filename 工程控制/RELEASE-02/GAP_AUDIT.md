# 工程控制 / RELEASE-02 差距审计（GAP_AUDIT）

## 1. 输入基线

RELEASE-02 不重做全量审计，以 RELEASE-01 审计结果为修复输入：

- P0 清单（16 条 + 二轮增量 P0-17/18）：仓库 `工程控制/RELEASE-01/GAP_AUDIT.md` §1、§7.5；
- P1=87、P2=35 明细：`reports/RELEASE-01/audit/AUD-A1-normalize.md`（54 条）、`AUD-A2-mosaic.md`（25 条）、`AUD-A3-export.md`（27 条）、`AUD-A4-infrastructure.md`（41 条）；
- 科学侧：`reports/RELEASE-01/science/SCI-S1-snr-psfsw.md`、`SCI-S2-topics.md`；
- 测试侧：`reports/RELEASE-01/tests/TST-001-report.md`；
- 性能侧：`reports/RELEASE-01/perf/PERF-001-timing.md`；
- 视觉侧：`reports/RELEASE-01/vis/VIS-001-report.md`。

修复任务映射见 `TASK_LIST.md` 与 tasks/FIX-*.md。

## 2. 修复中新增差距（增量登记）

> 修复过程中新发现的问题在此登记，格式同 RELEASE-01：编号、位置（文件:行）、违背条款、级别、归属任务、归宿。

| 编号 | 位置 | 问题 | 级别 | 归属 | 归宿/证据 |
|---|---|---|---|---|---|
|  |  |  |  |  |  |

## 3. 自决研究记录（§3a）

> 科学方法、门禁判据的自决研究在此汇总，详细证据入 reports/RELEASE-02/。

| 主题 | 子代理 | 查阅的文献/开源 | 结论 | 落地位置 |
|---|---|---|---|---|
|  |  |  |  |  |

## 4. 闭合核验

全部修复完成后，前台对照 RELEASE-01 GAP_AUDIT §1 与本文件 §2 逐条核验归宿，结果写入 `ACCEPTANCE.md` §2。

## 8. RELEASE-02 新增发现（增量节）

### 8.1 P0-19 `hips_frame=icrs` 违反 IVOA HiPS 标准（产品互操作缺陷，前台独立定案）

**问题**：AstroCS 写出的 HiPS `properties` 中 `hips_frame = icrs`；IVOA HiPS 标准与全部生产 HiPS 均用 `equatorial`。

**三方独立证据（前台复核，2026-09-18）**：
1. **IVOA 官方规范源码**（`github.com/ivoa-std/HiPS`，`HiPS.tex`）关键字表逐字：
   `hips\_frame & R & Coordinate frame reference – Format: word "equatorial" (ICRS), "galactic", "ecliptic", body frame...`；
   同文示例 properties 亦为 `hips_frame = equatorial`。⇒ **`icrs` 非法**。
2. **生产 HiPS 实样**（CDS/Aladin，HiPS 参考发布方）：`alasky.cds.unistra.fr/DSS/DSSColor/properties` 与
   `alasky.cds.unistra.fr/2MASS/Color/properties` 均为 `hips_frame = equatorial`。
3. **本仓历史自述**：`lib/infrastructure/scheduler/src/module_adapters.cpp:5284` 注释「坐标系 = ICRS（LEDGER-P1 残留②：
   原写非标准值 equatorial，与 properties…）」，`:3730` 亦提两种写法并存 ⇒ 系**前批 agent（M1a-B-005/LEDGER-P1）基于误读的回退**。

**受影响点**：
| 位置 | 现状 | 应为 |
|---|---|---|
| `lib/infrastructure/aio/src/hips/aio_hips_writer.cpp:1122,1371` | 写 `icrs` | 写 `equatorial` |
| `docs/science/PHASE3_HIPS_TO_FITS.md:63` | 称值域 `{icrs,…}`、把标准值 `equatorial` 当「非标准值废止」 | 值域以 `equatorial` 为准，`icrs` 仅作兼容别名 |
| `lib/infrastructure/aio/tests/p1hips/p1hips_tests_units.cpp:288-289` | 断言 `icrs` 为标准、`equatorial` 非标准 | 断言方向反转 |
| `lib/infrastructure/scheduler/src/module_adapters.cpp:5284,3730` | 注释称 `equatorial` 非标准 | 订正注释 |

**定性**：这不是「HiPS 数据模型的破坏性变更」，而是**恢复设计 §5.1 所要求的 IVOA HiPS 符合性**（当前输出无人认得的非法值）。
按控制包 §3a 第 1 条（证据充分后直接订正文档与代码）处理；登记为 **P0-19**，归 DEL-102 前的产品面必闭项。

**发现来源**：SCI-AUDIT 独立审计员（`11216326`）独立定案，前台用上述三方证据复核确认。
### 8.2 更正：L4 卫星线未被排异的**主因是验收输入被降采样**（非产品缺陷）

**前台先前判断有误，此处更正。** 我此前据 RELEASE-01 L4 证据推断「74% 像素只有 n=2 ⇒ 排异无法工作」为产品缺陷；
经负责人质询并复核原始数据后确认：**该 n=2 是 L4 E2E 输入被降采样造成的假象**。

**原始数据事实（`testdata/M42_T2T3_mosaic_Flying_dutchman`，前台实测）**：
- 该数据集是 **6 片马赛克**（M1..M6 为 6 个**不同指向**的板块），T2/T3 为两台设备型号相同的望远镜，**各自覆盖全部 6 片**。
- **R 通道每片帧数（T2+T3 合并）**：M1=8、M2=8、M3=8、M4=9、M5=8、M6=8 ⇒ **合计 49 帧**（负责人预期 48，实测 M4 多 1）。
- 每片单台望远镜 R 帧数 2~6，两台合并后每片 ≈8。

**缺陷定位**：RELEASE-01 的 L4 E2E 只用了 **12 个 Phase1 产品**（`p1_m42_t2_m1_red` .. `p1_m42_t3_m6_red`，
即每个「望远镜×板块」仅 1 帧），占 R 通道 49 帧的 **24.5%**；`gen_stage23.py:6` 只是 glob 现有 `p1_m42_*_red`，
没有覆盖全部 R 帧。故每个输出像素 n=2（两片重叠处 n=4/8），**排异在该 n 下确实无法工作**——
但这是**验收输入不具代表性**，不是产品在马赛克正常输入下失效。

**仍成立的真实缺陷（与上条独立）**：
1. AUTO 只在**整组总帧数**上解析一次（`module_adapters.cpp:4412-4418`），非逐像素 —— 违反 DESIGN §4.5 第 2 条；
2. `mosaic.config.algorithm_rejection_method` 在 `lib/` **零 C++ 消费者**，方法硬编码 AUTO ⇒ 用户显式指定被静默忽略 —— 违反 §4.5 第 5 条。

**负责人指示（2026-09-18）**：
- **只用 R 通道跑全流程**（有代表性），验收通过后再做全数据集测试；
- **T2 与 T3 设备型号相同，各自校准后的数据应汇总为一个数据集**测试；
- 即 L4 应以 **49 帧 R（8/片）** 为输入重建，而非 12 帧。

**待办**：以 49 帧 R 重建 L4 输入 → 全流程（normalize→mosaic→export）→ 验证逐像素 n≈8 时排异生效、
卫星线被剔除、接缝消失；随后再排全数据集测试。
### 8.3 P0-21（严重）：`normalize` 静默丢弃多帧输入，只处理 1 帧

**前台实测证据（2026-09-18）**：
- L4 的 12 个 `p1_m42_*_red` 配置共声明 **49** 个 `input_lights`（与磁盘 R 帧数 49 **完全一致**，即配置本身覆盖了全部 R 帧）；
- 但每个配置的日志显示：**读取了全部 light，却只 drizzle 1 帧、只写 1 个 HiPS 产品、无任何 WARN/ERROR**。
  逐配置实测（`drizzles`/`hips_writes` 恒为 1）：

  | 配置 | 读取帧 | drizzle | HiPS 写 |
  |---|---|---|---|
  | t2_m1 | 2 | **1** | 1 |
  | t3_m1 | 6 | **1** | 1 |
  | t3_m3/t3_m4/t3_m6 | 6 | **1** | 1 |
  | …（12 个全部如此） | | **1** | 1 |

- 判据：`[drizzle_engine] 完成: 16777216 源像素` = **恰好一帧 4096²**（4096²=16,777,216），多帧叠加应为整数倍；
  且每个日志中 `drizzle_engine] 完成` 与 `HiPS 直写完成` **各仅出现 1 次**。

**后果**：L4 马赛克实际只由 **12 帧**（每配置 1 帧）构成，而非 49 帧；37 帧被静默丢弃。
这直接导致每个输出像素 n=2（每片 2 个配置）⇒ **排异无从判断**、SNR 远低于真实产品。
**用户观察到的「卫星线未被去除」由此得到完整解释**（并纠正前台 8.2 的中间结论：8.2 说「49 帧都已使用（预叠加）」是**错的**）。

**定性**：违反 `ENGINEERING_SPEC.md:122` fail-closed —— 「检查器在输入缺失、路径不存在、依赖不可用时判红」，
而此处**输入被静默忽略**却报成功，属假绿。属发布阻断项。

**设计意图（负责人 2026-09-18 明确澄清，无歧义）**：**一组输入数据 → 一组 HiPS 输出；每一帧输入都对应一个 HiPS 输出。**
（前台先前把此处写成「设计侧歧义」是**错的**，已更正。）故本项是**实打实的缺陷**，不是口径问题：
输入 49 帧却只产出 12 个产品（每配置 1 个），**静默丢弃 37 帧**。

**修复方向**：
1. `normalize` 必须**每帧独立处理并各自产出一个 HiPS 产品**（一组进、一组出），结构化 JSON 列出全部产品路径且计数与输入帧数一致；
2. **禁止静默丢弃**：任一帧未处理/失败必须显式报错（fail-closed）；
3. 已按负责人指示补充进最高设计：`ASTROCS_DESIGN.md` §3.4「输出基数」与 §3.3 数据块条目（一组进一组出、不得只取一帧）；
4. L4 重建：R 通道 49 帧、T2+T3 汇总为一个数据集 ⇒ 应得 **49 个 HiPS 产品**，马赛克逐像素 n≈8。

**根因（前台 2026-09-18 定位，`lib/infrastructure/scheduler/src/module_adapters.cpp`）**：
Phase1 各操作器对多帧的处理**内部不一致**——部分循环全部帧，**三个决定性操作器只取第 0 帧**：

| 操作器 | 函数起点 | 多帧处理 | 证据行 |
|---|---|---|---|
| `p1_op_calibrate` | :1532 | ✗ **只取首帧** | `:1634` `p1_read_image(lights.front())` |
| `p1_op_cosmetic` | :1843 | ✓ 循环 | `:1865` `for (const auto& l : doc["input_lights"])` |
| `p1_op_star_psf_impl` | :1931 | ✓ 循环 | `:1945` 同上 |
| `p1_op_wcs` | :2424 | ✗ **只取首帧** | `:2463`、`:2621` `input_lights[0]` |
| `p1_op_noise` | :2980 | ✓ 循环 | `:3031` 同上 |
| **`p1_op_drizzle`** | :3202 | ✗ **只取首帧** | `:3343` `p1_calibrated_path(doc, doc["input_lights"][0]...)` |

其中 **`p1_op_drizzle` 是产出 HiPS 产品的操作器**，只 drizzle `input_lights[0]` ⇒ 每配置恒 1 个产品，
与日志实测（`drizzle_engine] 完成` 恒 1 次、16,777,216 源像素 = 恰一帧）**完全吻合**。
即：不是"叠加失败"，而是**除首帧外的帧从未进入 drizzle**；且无任何报错。

**测试缺口**：`ctest` 460 全绿**未能发现**此缺陷 ⇒ 缺少「多 light normalize」用例，须补正例（多帧正确叠加/正确处理）
与负例（不支持的组合必须报错，不得静默通过）。