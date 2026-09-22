# RELEASE-05 审核包索引（REPORT-501 §1–§7）

> 依据：控制包 `工程控制/RELEASE-05/tasks/REPORT-501.md`（八项审核内容）；
> 本文件是**索引**，每一项给出可直接打开的证据路径与一句话结论。数字全部可回溯到文件。
> 状态：**READY_FOR_OWNER_REVIEW**（未进 FIN；FIN 动作等负责人明确指令）。

---

## 1. 三篇论文式实验报告（SCI-503/504/505）

| 创新点 | 报告 | 结论（一句话） |
|---|---|---|
| SCI-A 测光星等坐标系 | `实验/photometric-magnitude/REPORT_paper.md` | 成立，**适用域 n≥100 拟合星**（文档 §16.5）；生产门槛仍 kMinFitStars=3（D-13/D-17③，未落地） |
| SCI-B 跨帧绝对 SNR | `实验/absolute-snr/REPORT_paper.md` | 成立；噪声项双计已修（SCI-501）并有双向非退化证据（zA=1.27 绿 / zB=25.6 红） |
| SCI-C 加性天光无接缝 | `实验/additive-sky-seamless/REPORT_paper.md` | 成立，附**表示能力边界**与条件结论；接缝门非退化且 fail-closed 不 skip（ACCEPT-501 独立复核确认） |

每篇均为八段结构 + 附录 A 数字索引（34/40/41 条），另 SCI-505 附 C（口径冲突 9 条）与 D（诚实边界 28 条）。

## 2. ACCEPT-501 独立验收报告

- 报告：`run/RELEASE-05/accept/ACCEPT-501_report.md`（334 行；五一致性矩阵 + 14 条问题 + 证据 JSON + 可复跑脚本）
- 执行者：独立子代理（**非** ARCH/GATE/SCI 实现者），只读全仓，零 git 写权限。
- 结论：**NO-GO**。最严重 3 条 BLOCKER：
  1. ARCH-504 `export_stream` 真实数据竞争（`inflight_bytes_/inflight_count_` 递减未持锁 ⇒ 无符号下溢，≈21% 概率红）→ **已修**，修后连跑 40/40 PASS；
  2. fast 档非全绿（deep 构建树陈旧 + 3 个自测步骤 missing_output）→ **已修**，见 §7；
  3. 块流一致性登记册漏登记 ≥8 处 → **已补 6 条**（BFD-A11..A16，逐行复核 file:line+token）。
- 其余指出项处置：空断言门盲区**已修**（正则补 `CHECK(1,"msg")`、扫描面补 `.c`；修后立刻抓到 3 处恒真断言并改为**每节点** destroy 实际计数断言，实测具备判别力）；E2E docstring 两条判据**已实现**（FITS 结构自洽 + 目录纪律）；未修项见 §7 与 ACCEPTANCE D-17。
- 正面独立复算（不是自证）：三命令边界无隐式串联；E2E `verify_chain` 非退化（control 绿 + 8/8 负例红，含真实产品字节篡改）；四个指定门注入违规**实测判红**；UPM `converged` 枚举文档/代码/C ABI **三处一致**；SNR 防双计在生产调用点已生效。

## 3. OPEN_QUESTIONS.md

`工程控制/RELEASE-05/OPEN_QUESTIONS.md` —— Q-1..Q-8 + **OQ-9** + **OQ-10**。两个 BLOCKER：

- **OQ-9**（块流结构冲突）：注册表声明端口与代码真实数据流不一致（**18 条偏差、13 blocker**）。按声明图迁移节点 = facade，故按任务书「不等价就保留并登记阻塞原因，不硬退役」。agent 推荐方案 1（改注册表对齐代码）。
- **OQ-10**（M42 解算失败 + 可诊断性）：根因已定位——M42 视场最亮 60 源**全部饱和** ⇒ 密度估计污染 ⇒ 极限星等迭代首个 Gaia 查询 `m_lim=20.435` 返回 0 即放弃；且错误串把它误报成「parity/尺度闸门拒绝」，生产路径还**刻意清零求解日志**（`module_adapters.cpp:2946`）。四条候选修复方向已列出，未擅自改算法。

## 4. 性能报告

**PERF-501 未执行**（依赖 ARCH-505 + CLEAN-501 + GATE-501；ARCH-505 阻塞于 OQ-9）。
已具备的基线材料：
- 调度器层探针数据（ARCH-502/503/504 测试内，落 `run/RELEASE-05/evidence/` 与 ctest 现场）：
  - ARCH-502 预取开启 0.1025s vs 关闭 0.1457s = **1.42×**（w=1）；w=4 饱和 1.00×（已记录，不设断言）；
  - ARCH-503 读放大 **0.25**（393216 B 路由 vs 1572864 B 朴素）、峰值驻留 32 B 与总图规模解耦；
  - ARCH-504 在途峰值 **1048576 B** 在 1× 与 4× 规模下相同（有界性证据）。
- L2 真门证据：GATE-501 `artifacts/evidence/release-05/GATE-501_VERIFICATION.md`（归档回放 9/9 翻红）。
- 计时热点：**未做**（PERF-501 未执行）。

## 5. 视觉验收（VIS-501）

- 数据：银心 T4 panel1–3 Red 180s **32 帧**（K=1）、M42 T2/T3 Red 300s（600s dark，K=0.5），只做 R 通道。
- 工具：`eng/tools/e2e/make_vis_configs.py`（配置）、`eng/tools/e2e/render_vis.py`（拉伸 PNG + 分块 + 逐块自检 + 接缝度量，判据 V1–V5 fail-closed）。
- 状态：银心组全链运行中；**M42 组被 OQ-10 阻塞**。
- 证据路径（产出后）：`run/RELEASE-05/vis/out/<group>/{full.png, tiles/, vis_report.json}`、日志 `run/RELEASE-05/vis/logs/`。

## 6. 双线变更清单

A 线（架构/清理）：`b3a1dd27` ARCH-501 → `53c9c0f0`+ `9480f26e` ARCH-502 → `c107e596` ARCH-503 → `6811803b` ARCH-504 → `557ddd68` ARCH-505（部分，上呈）→ `60689b99` aio 收口。
B 线（门禁/测试/文档）：`25c8227d` GATE-501 → `77e3097b`/`090390b9`/`abcc0529`/`091e0755`/`6f217cef` GATE-502 → `02bf4704` E2E-501。
科学/文档：`1efb5c9e` SCI-503、`dbce6add` SCI-504、`2150c162`+`ea121e86` SCI-505、`1d9609ae`/`3f7feb14` DOC-502 订正。
逐条目的与权威依据见各提交消息（`git log`）。

## 7. 机器门证据

| 门 | 结果 | 证据 |
|---|---|---|
| 全量 ctest | 见 `run/RELEASE-05/logs/accept_fix_ctest.log` | 482 个测试 |
| fast 档 | 见 `run/RELEASE-05/logs/accept_fix_fast.log` | 120 步 |
| deep 构建树 | `run/RELEASE-05/logs/accept_fix_deep.log` | 供 linux-deep 档 ctest 目标使用 |
| aio I/O 边界 | `AIO-IO-BOUNDARY_PASS`（HARD=0、A44=0、PRODUCTION-RESIDUAL=0、白名单一一对应；自测 14 负例） | `eng/ci/check_aio_io_boundary.py` |
| 线程预算 | `THREAD_BUDGET_CHECK_PASS`（未登记=0、硬编码=0、登记键 25/命中 37） | `eng/tools/arch/check_thread_budget.py` |
| 串行硬编码 | `QA-002_PASS` | `eng/tools/check_serial_hardcode.py` |
| 文档行号锚 | `DOC_LINE_ANCHORS_PASS`（39 docs / 879 anchors / OK 870 / EXEMPT 9） | `docs/algorithms/anchors/check_doc_line_anchors.py` |
| 块流规格 / 一致性 | 12/12 与 10/10 自测；一致性 18 条偏差、13 blocker 全上呈 | `eng/tools/quality/check_block_flow_{spec,conformance}.py` |
| E2E 链 | `E2E501_CHAIN_PASS`（manifest 链独立复算 + FITS 结构自洽 + 目录纪律） | `run/RELEASE-05/evidence/e2e501_chain.json` |
| 空断言 | `findings=0`（扫描面已补 `.c`） | `eng/tools/quality/check_test_discriminative.py` |
| Windows 腿 | **未跑**（BLD-501 未执行） | — |
| waiver | **零 waiver** | `eng/ci/checks.json` 中 `waivable: false` |

## 8. SUMMARY.md 收口报告

`工程控制/RELEASE-05/SUMMARY.md`（含结论页四问）。
