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