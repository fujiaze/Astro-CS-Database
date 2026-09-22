# SUMMARY — RELEASE-05 收口报告

> 状态：**READY_FOR_OWNER_REVIEW**。本报告只给建议，**发布决定权在负责人**；FIN 动作（README、版本号、
> 控制包收口）等负责人明确指令后执行。所有数字可回溯到 `run/RELEASE-05/logs/`、`run/RELEASE-05/accept/`、
> `artifacts/evidence/release-05/` 与提交记录。

## 1. 任务结论

| 状态 | 任务 |
|---|---|
| **PASS（17）** | DOC-501、DOC-502、SCI-501、SCI-502、SCI-503、SCI-504、SCI-505、SCI-506、CONTRACT-501、ARCH-501、ARCH-502、ARCH-503、ARCH-504、GATE-501、GATE-502、E2E-501、ACCEPT-501 |
| **BLOCKED（1）** | **ARCH-505**（生产节点改写与 Session/orchestrator 退役）—— 注册表声明端口与代码真实数据流不一致（18 条偏差、13 blocker），按声明图迁移只会得到 facade；按任务书「不等价就保留并登记阻塞原因，不硬退役」保留 Session 与文件约定，上呈 **OQ-9** |
| **NOT_RUN（4）** | CLEAN-501（依赖 ARCH-505）、PERF-501（依赖 ARCH-505+CLEAN-501+GATE-501）、BLD-501（Windows 腿）、VIS-501（银心组进行中；**M42 组被 OQ-10 阻塞**） |

**关键结论（必须先说）**：本包**未完成，不进入 FIN，不申请发布**。未完成的主因是两条**真实阻塞**，
不是执行预算或环境借口：
1. **OQ-9**：块流结构合同与代码真实数据流不一致 —— 这是**顶层合同的结构性冲突**（AGENTS §10 第 1 类），
   必须由负责人裁决（agent 推荐方案 1：改注册表对齐代码）。
2. **OQ-10**：M42 真实数据 Gaia 解算失败，根因已定位（60 颗选星全饱和 ⇒ 密度估计污染 ⇒ 首个星表查询
   返回 0 即放弃），且**生产路径刻意关闭求解日志**使失败不可诊断；候选修复方向已列出，未擅自改科学算法。

## 2. 科学三创新点结论

- **测光星等坐标系（SCI-A）**：**成立，带适用域**。报告 `实验/photometric-magnitude/REPORT_paper.md`
  （八段 + 附录 A 34 条数字索引）。**边界**：适用域为拟合星数 n≥100（`PHOTOMETRY.md` §16.5），
  而生产门槛仍 `kMinFitStars=3` ⇒ **文档定案未落地**（D-13/D-17③，未决）。
- **跨帧绝对 SNR（SCI-B）**：**成立**。报告 `实验/absolute-snr/REPORT_paper.md`（附录 A 40 条）。
  修复了噪声项双计（SCI-501），并有**双向非退化**证据：zA=1.27 判绿 / zB=25.6 判红 / legacy 逐位一致。
  ACCEPT-501 独立复核确认「防双计在生产调用点已生效」。
- **加性天光无接缝（SCI-C）**：**成立，带表示能力边界与条件结论**。报告
  `实验/additive-sky-seamless/REPORT_paper.md`（附录 A 41 条 + C 口径冲突 9 条 + D 诚实边界 28 条）。
  接缝判据**非退化且 fail-closed 不 skip**（ACCEPT-501 独立复核确认）。

三篇均有「真值无效应 ⇒ 度量归零/判红」的负例，无恒真门。

## 3. 架构落地证据

- **块生命周期与执行器**：`BlockFrame`（ARCH-501，ctest `core_block_frame` 33/33，含负例注入实测能红）、
  `StageBlockFlow`（ARCH-505，ctest `block_flow` 28/28：名字级 undeclared-write fail-closed、
  消费者由执行器盖章、SHORT 残留必须为 0、与直接顺序计算**逐位一致**）。
  规格 `eng/contracts/block_flow/stage_block_flow.json`（20 节点/26 块）+ 两道机器门（12/12、10/10 自测）。
- **三调度器（已实现、已自证、尚未接线）**：
  - ARCH-502 normalize 异步工作流：帧内 DAG 流水 + 多帧并发 + 专用预取线程；测试 32/32，
    预取 **1.42×** 加速（0.1025s vs 0.1457s，w=1），N=1/2/4/8 逐位一致；
  - ARCH-503 mosaic 天区窗口：测试 19/19，N=1/2/4/8/16 **逐位一致且与单窗口参考逐位相同**（容差 0），
    读放大 **0.25**，峰值驻留 32 B 与总图规模解耦；
  - ARCH-504 export 子块流式：测试 22/22，与整图参考**逐位相同**，在途峰值 1048576 B 在 1×/4× 规模下相同。
  **诚实边界**：三者目前**只在库与测试中存在，生产路径零引用**（ACCEPT-501 独立复核指出）——
  即"已实现且自证正确"但**未接线**，接线被 OQ-9 阻塞；三命令当前仍走原 Session 路径。
- **Session/orchestrator 退役**：**未退役**（阻塞于 OQ-9）。按任务书保留并登记阻塞原因。
- **数值等价基线**：ARCH-505 提交后重跑全量 ctest **482/482 PASS**（`arch505_frozen_baseline_ctest.log`）；
  调度器重构（aio 收口）后再次全量 **482/482 PASS**（`accept_fix_ctest.log`）。

## 4. 门禁与测试

- **L2 性能门真判红**：GATE-501 归档回放 **9/9 翻红**（`artifacts/evidence/release-05/GATE-501_VERIFICATION.md`）。
- **fail-closed 普查**：`artifacts/evidence/release-05/FAILCLOSED_SURVEY.md`。
- **本轮新增/加强的门**（全部有 `--self-test` 正负例）：
  `CHK-ARCH502-NORMALIZE-WF`、`CHK-SCHED-PROBE-SCHEMA`(6/6)、`CHK-ARCH503-MOSAIC-WIN`、
  `CHK-ARCH504-EXPORT-STREAM`、`CHK-ARCH505-BLOCK-FLOW`、`CHK-BLOCKFLOW-SPEC`(12/12)、
  `CHK-BLOCKFLOW-CONFORMANCE`(10/10)、`CHK-PREFLIGHT-MATRIX`(11 例)、`CHK-E2E-CHAIN`、
  `CHK-E2E-CHAIN-SELFTEST`。注册表 **91 项**，`validate_registry --strict` 0 error，doc_sync 91==91。
- **门禁补盲区（ACCEPT-501 指出后已修）**：空断言门正则漏 `CHECK(1,"msg")`、扫描面缺 `.c` ⇒
  修后立刻抓到 3 处恒真断言并改为**每节点**实际计数断言（实测具备判别力）。
- **全门结果**：ctest **482/482 PASS**；`run_checks --all --profile fast` **120/120 PASS**；
  `AIO-IO-BOUNDARY_PASS`（HARD=0/A44=0/生产残留=0）；`THREAD_BUDGET_CHECK_PASS`；`QA-002_PASS`；
  `DOC_LINE_ANCHORS_PASS`（879 锚）。**Windows 腿未跑**（BLD-501 未执行）。
  `--profile integration` 有 1 条未定位红（D-14，单跑 PASS，非本任务域）。**零 waiver**。

## 5. 性能

**PERF-501 未执行**（依赖链被 OQ-9 阻塞），故**没有**端到端优化前后对照与计时热点表。已有的
调度器层基线：预取 1.42×（ARCH-502）、读放大 0.25（ARCH-503）、在途峰值与总图解耦（ARCH-504）。

## 6. L4 视觉验收

- 数据与配置：`eng/tools/e2e/make_vis_configs.py`；渲染与自检：`eng/tools/e2e/render_vis.py`
  （拉伸 PNG + 分块 + 逐块自检 + 接缝度量，判据 V1–V5 fail-closed：有限像素占比、非零占比、
  动态范围、**接缝比 ≤1.5**、覆盖包围盒占比）。
- **银心 T4 组**：panel1–3 Red 180s **32 帧**，normalize rc=0（3 块 3 产品）；mosaic/export 进行中。
- **M42 组**：**阻塞**（OQ-10）。
- 负责人目检状态：**未提交目检**（银心组链未走完）。

## 7. 开放问题（OPEN_QUESTIONS 摘要）

Q-1 发布与成品帧判定、Q-2 三阶段架构目标态排期、Q-3 RELEASE-04 是否物理出库、Q-4 `converged` 枚举
（已按仓内权威处置）、Q-5 测光 n≥100 未落地、Q-6 L2 判据本环境可达性、Q-7 双线文件域注册归属、
Q-8 Windows 腿退出码 9 平台限制、**OQ-9 块流结构冲突（BLOCKER）**、**OQ-10 M42 解算失败与可诊断性（BLOCKER）**。

## 8. 发布建议

**建议：不具备 0.0.1alpha 预览条件，本轮不申请发布。** 依据：

1. 架构目标态（三调度器 + 命名块）**尚未在生产路径执行**——三个调度器已实现且自证正确，但零引用、
   未接线，接线被 OQ-9 阻塞（顶层合同结构性冲突，需负责人裁决）；
2. L4 视觉验收**未完成**（M42 组阻塞于 OQ-10；银心组链未走完，负责人未目检）；
3. 性能验收（PERF-501）与双平台全门（BLD-501 的 Windows 腿）**未执行**；
4. 三项科学/工程落地缺口未闭合：测光 n≥100 适用域、`sigma_sky_source` fail-closed、
   `--profile integration` 的 1 条未定位红（D-14）。

**已完成且可复核的确定成果**：阶段 1 科学闭环（DOC-501/502、SCI-501..506，三篇论文式报告）、
CONTRACT-501、ARCH-501..504（含 1.42× 预取、0.25 读放大、逐位一致与有界在途内存的证据）、
GATE-501/502、**E2E-501 三命令真实数据全链首次贯通**（含真实 Gaia IPV 解算与 manifest 链独立复算）、
以及 **ACCEPT-501 独立复核**（NO-GO 回执，其 3 条 BLOCKER 已闭环）。
