# OPEN_QUESTIONS — RELEASE-05（无法由执行 agent 裁决项）

> 依据：控制包 RELEASE-05 `00_README.md` §6（必须上呈清单）、`PROMPT.md` 第 9 条、
> AGENTS.md §10（何时必须停下来问负责人）。本文件随 REPORT-501 交付。

## Q-1（发布与成品帧判定）是否具备 0.0.1alpha 预览条件

- **状态**：未达发布门（见下），**不申请发布**；发布决定权在负责人。
- **依据**：本轮完成阶段 1 科学闭环与 CONTRACT-501，但 ARCH-501..505 / CLEAN-501 /
  PERF-501 / ACCEPT-501 / BLD-501 / E2E-501 / VIS-501 / REPORT-501 未完成；
  发布门六条（三篇论文报告 / 五一致性独立验收 / 双平台全绿 / 两组成品帧目检 /
  OPEN_QUESTIONS / FIN 授权）中仅第 1、5 条部分满足。
- **建议**：不进入 FIN；由负责人决定是否以"科学闭环 + 合同冻结"为阶段成果继续排期。

## Q-2（顶层合同结构性变更）三阶段架构目标态尚未落地，是否调整排期或范围

- **状态**：ARCH-501 已交付命名块与生命周期基础；ARCH-502/503/504（三阶段调度器）
  与 ARCH-505（20 个生产节点改写、Session/orchestrator 退役）未开始。
- **影响面**：生产路径**当前仍是整阶段 Session 装配**，不是最高设计 §8.1 的三独立
  调度器 + 命名块内存管线；"生产路径无 Session 直接装配"的验收门未达成。
- **建议**：这是本包最大未完成面，请负责人决定（a）继续按原排期推进 A 线，或
  （b）先做 ARCH-502/503/504 中风险最低的 export 子块流式作为试点。

## Q-3（控制包收口）RELEASE-04 是否物理出库

- **状态**：CONTROL_PACK_SPEC §9 要求"收口即清理"，但 RELEASE-04 是负责人交付物，
  AGENTS.md §10 禁止未经询问删除交付物。
- **现处置**：doc-index 的现行控制包已切到 RELEASE-05；RELEASE-04 入**归档白名单**，
  白名单条目必须自带 `SUMMARY.md`（收口证明），缺证明仍判红——门禁保持有牙。
- **建议**：请负责人裁决是否删除 `工程控制/RELEASE-04/`（内容已在 git 历史）。

## Q-4（规范冲突）SCI-502 的 `converged` 枚举定义

- **冲突**：控制包 `tasks/SCI-502.md` 写 `0/1/2/3 = 收敛/未收敛/达 max_iter/stalled`；
  仓内权威 `docs/plugins/algorithms_phase2/11_upm.md` §4.6 与
  `docs/algorithms/PHASE2_UPM_IMPL.md` §496 均定义
  `0=max_iter / 1=converged / 2=stalled / 3=invalid`。
- **现处置**：按 AGENTS.md §1.1「规范在别的文档去那一份读」采用**仓内权威**定义实现。
- **建议**：确认以仓内权威为准，或指示按任务书文字调整（后者需同步改两份正式文档）。

## Q-5（科学文档 vs 代码落地缺口）测光 n≥100 适用域未在生产实现

- **事实**：`docs/science/PHOTOMETRY.md` §16.5 定案"生产适用域 = 匹配星数 n ≥ 100，
  低星数帧显式降级 `degraded_reason`"，但生产硬门槛仍是 `kMinFitStars=3`
  （`frame_photometry_fit.h:70`、`module_adapters.cpp` 的 `P1_PHOT_MIN_FIT_STARS=3`），
  `lib/` 内无 `low_star_count` 与 100 阈值。
- **影响**：规范已定案、机器未落地 ⇒ 当前产品会以 n=3..99 的帧产出"已标定"结果，
  与适用域声明不符。
- **建议**：立 A 线任务实现降级路径（登记 `degraded_reason=low_star_count`），
  或由负责人裁决改为"仅文档声明、不设机器门"。

## Q-6（性能门）L2 判据是否可在本环境达标

- **状态**：PERF-501 未开始；GATE-501 正在把 L2 门改为 fail-closed 并回放 RELEASE-04
  违规数据。
- **风险**：若真判红后实测无法达到平均利用率 ≥0.85 / p50 ≥0.90，按合同**不得**用
  `record_and_justify` 或 waiver 盖红灯 ⇒ 需给证据化上限并登记本文件。
- **建议**：待 GATE-501 与 PERF-501 完成后回填。

## Q-7（双线文件域）新检查器注册归属

- **事实**：CONTRACT-501 新增 `eng/tools/contract_doc_sync.py`（双向对应校验，
  `--self-test` 4/4 红绿双向）；GATE-502 请求新增静态检查（空断言防复发）。
  两者都需要在 `eng/ci/checks.json` 注册，而该文件属 B 线域。
- **建议**：由 GATE-501 统一登记，或由负责人指定归属。

## Q-8（Windows 腿）CTRL_CLOSE 退出码 9 平台限制

- **状态**：BLD-501 未开始；G3-4 的 Windows 退出码 9 限制复核未做。
- **建议**：随 BLD-501 处置；如平台确实不可达，按"平台限制显式登记"处理，不 waiver。

---

## OQ-9（BLOCKER，ARCH-505）：注册表声明的节点端口与代码真实数据流不一致

**性质**：顶层合同的结构性不一致（AGENTS §10 第 1 条）。**需负责人裁决，agent 不擅自改注册表。**

**事实**：`lib/infrastructure/pipeline/module_ports.registry.json` 是冻结绑定表，ARCH-505 的块流规格
由它派生。但逐条核对 `lib/infrastructure/scheduler/src/module_adapters.cpp` 的真实读写后，
发现 **12 处**声明与实现不符（机器登记册：`eng/contracts/block_flow/conformance_deviations.json`，
机器门：`eng/tools/quality/check_block_flow_conformance.py`，每条证据带 文件:行 + 必须出现的 token）。

**blocker 级（7 条）**：
- **BFD-A1** writer/write_hips 被声明产出 `fits`，实际**不写任何 FITS**（只写 p1_final.json / p1_products.json）；
  真正的 P1 HiPS FITS 由 **drizzle** 产出（module_adapters.cpp:5052），而 drizzle 只被声明输出 `stacked`。
- **BFD-A2** photometry 声明的 `psf` 输入端口**从未被打开**；PSF 参数实从 p1_sources.json 的 psf_params 读（:3375）。
- **BFD-A3** noise-snr 声明的唯一输入 `fluxes` 与实际完全不符：不读 p1_flux.json，实读
  p1_sources.json（缺失即 DATA fail，:3960）、p1_phot.json（:4151）、cleaned_<base>。
- **BFD-A5** coverage 声明的 `calibrated`/`DATA-P2-CAL` 无对应产物；真实输入是 config `hips_paths`（:5589）。
- **BFD-A7** p3.writer 少声明 1 个输出（p3_writer.json，verify 依赖）与 2 个输入（p3_wcs.json、run_context.json）。
- **BFD-C1** 代码额外注册 `astrocs.phase2.resample`（:697）与 `astrocs.phase3.resample`（:716），
  不在注册表的 20 个 module 内；注册表自述「未知 module_id 即拒绝」，后者代码注释自认是占位残留。
- **BFD-E1** 生产路径节点间实为 output_dir **文件约定**，唯一真实命名块是 drizzle 内 PipelineFrame 的
  `data`/`variance`（帧内、不落盘、不跨节点，:4740）。`docs/contracts/PIPELINE_BLOCK_CONTRACT.md` §51-52
  声称「WCS/PSF/SNR/星表匹配存活到导出」在代码中**无对应路径**：PSF（p1_psf.json）零消费者；
  SNR（p1_snr.json）仅被 P1 drizzle 的 sink 读一次；P3 的 WCS 由 config 的 center/scale 重建（:9701），
  不读 P1 的 WCS。

**为什么这阻塞 ARCH-505**：ARCH-505 要求把 20 个生产节点改为「从命名块读写」。块名来自注册表端口。
若按现注册表迁移，会把节点接到**不存在的边**上（如让 photometry 等一个永不产生的 psf 块、
让 noise-snr 读一个它根本不读的 fluxes 块）——那是 facade，违反硬禁令。
按任务自己的规则「不等价就保留并登记阻塞原因，不硬退役」，**Session 与文件约定暂不退役**。

**已落地且已验证的部分（不受阻塞影响）**：
- 块流规格 `eng/contracts/block_flow/stage_block_flow.json`（由注册表派生，20 节点 / 26 块声明）；
- 块流执行器 `lib/include/astrocs/core/block_flow.h` + `lib/infrastructure/scheduler/src/block_flow.cpp`：
  节点只能读写声明的块（名字级 fail-closed）、生命周期归执行器掌管、单元结束无 SHORT 残留、
  与直接顺序计算**逐位一致**（ctest `block_flow` 28/28）；
- 规格机器门 `eng/tools/quality/check_block_flow_spec.py`（12/12 自测，含手改漂移检测）；
- 一致性登记册机器门（10/10 自测，token 腐烂即判红）；
- **顺带修掉一个真实缺陷**：ARCH-501 的 `BlockDagValidator` 把「空生产者」无条件判 UNKNOWN_NODE，
  与同一函数「外部输入必须声明 optional」条款自相矛盾 ⇒ **阶段外部输入根本无法表达**，
  块流规格里 frames/hips/calibrated 一律被判非法图。已按条款语义统一并补回归（ctest `core_block_frame` 33/33）。

**请负责人裁决（三选一，附 agent 推荐）**：
1. **改注册表对齐代码**（推荐）：按 A1–A10 修正端口/生产者/消费者，注册表继续作为唯一事实源；
   代价：注册表版本号递增，涉及 P1/P2/P3 端口合同的破坏性变更，需同步 docs/contracts。
2. **改代码对齐注册表**：让 photometry 真读 p1_psf.json、noise-snr 真读 p1_flux.json、
   writer 真写 FITS 等；代价：改科学装配与产物链，风险高于方案 1，且与 SCI-502/SCI-506 的已验证口径冲突需重验。
3. **保留双轨**：注册表作为「目标合同」，另建「现状合同」并长期登记差异；
   代价：长期双份事实源，ARCH-505 的迁移只能对目标合同做，短期无法完成退役。

**agent 推荐方案 1**，并在同一变更里同步 `docs/contracts/PIPELINE_BLOCK_CONTRACT.md` 的块生命周期清单
（删掉代码中不存在的「WCS/PSF/SNR 存活到导出」表述，改为「跨阶段唯一载体 = HiPS 产品树」）。


---

## OQ-10（BLOCKER，VIS-501 / 可诊断性）：M42（T2/T3）真实帧 Gaia IPV 解算失败，且生产路径**刻意关闭求解日志**

**现象（可复现）**
```
build/astrocs normalize --json run/RELEASE-05/e2e/configs/p1_m42_t2_red.json -y
→ rc=2, node wcs failed:
  ipv_solve_from_memory_with_callback_d failed:
  ipv_select_from_memory_with_callback_f64 ??????, ????????????
  (ipv 真实求解器链: 求解失败或解被 parity/尺度合理性闸门拒绝; frame .../calibrated_M42_M1_T2_...-300S-Red.fts)
```
- 同一命令、同一代码、同一 `gaia_data_dir`（`gaia/GaiaDR3`）跑**银心 T4** 三帧**全部成功**
  （nside=65536、42 内点、rms 0.32"），故链路本身可用，失败与数据集相关；
- M42 T2 帧的星点检测正常（`p1_sources.json` n_detected = 86717/92839/…，与 T4 同量级），
  排除"星太少"；
- 帧头指向存在且合理（`RA 05 33 08.00 / DEC -06 22 32.0`，`CRVAL1/2 = 83.2834/-6.3743`，
  `FOCALLEN 1917.6 mm`、`XPIXSZ 9.0 µm` ⇒ 约 0.968"/px）。

**为什么查不下去（本 OQ 的实质）**
`lib/infrastructure/scheduler/src/module_adapters.cpp:2946` 在调用真实求解器前**清零**日志目录：
```cpp
std::memset(ip.log_dir, 0, sizeof(ip.log_dir));  // 节点面禁写求解日志
```
IPVSolver 内部的 `Logger`（`lib/algorithms/platesolve/cpp/ipv/include/ipv_log.h`）本可逐阶段记录
"星表查询/三角形匹配/RANSAC/parity 与尺度闸门/extract_wcs_sip" 的每一步，但生产路径把它关掉了，
于是失败只剩一句**不可读的乱码**（错误串是 GBK 字节被按 UTF-8 打印成 `??????`）。
结果：真实数据的解算失败**无法定位根因**，只能二选一——要么盲改算法，要么放弃该数据集。
这同时违反 AGENTS §6「不以'环境问题/工具问题'掩盖失败，必须给出可复现证据」的可诊断性要求。

**请负责人裁决（三选一，附 agent 推荐）**
1. **加可诊断开关（推荐）**：把 `log_dir` 改为由配置/环境变量显式开启（默认仍关闭，不改变生产默认行为），
   失败时把求解日志落到 `output_dir` 或 `run/`；同时把 IPV 的 GBK 错误串按 UTF-8 归一（或至少转义），
   让失败信息可读。代价：改 `module_adapters.cpp` 的节点面策略 + 一处编码归一，需登记新配置键。
2. **保持关闭，M42 组登记为数据侧受限**：VIS-501 只出银心组成品帧，M42 组标 BLOCKED；
   代价：VIS-501 只完成一半，且下次遇到解算失败仍然查不动。
3. **先做一次性离线诊断**（不改生产）：单独写一个探针程序直接调 IPV 求解器并开启日志，
   定位 M42 失败根因后再决定是否需要方案 1。代价：多一个一次性工具，但不触碰生产策略。

**agent 推荐方案 3 立即做、方案 1 随后做**：先拿到根因（可能只是帧头指向偏差或 parity 约定），
再决定是数据问题还是代码问题；无论结论如何，"生产失败无日志且错误串乱码"都应修。

**影响面**：VIS-501（M42 组成品帧）、E2E-501 的 M42 数据集冒烟（已改为银心 T4 跑通全链）、
以及任何依赖真实 T2/T3 数据的验收。

