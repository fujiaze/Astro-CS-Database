# 任务：ACSD-T30 生产侧合规、永不调度的 session 侧漏设回落 0：随 ARCH-DEBT-01 下线或统一冻结装配

> 波次 `W1` ｜ 杠杆分档 `P2` ｜ 整改域 配置+代码 ｜ 基线 HEAD `c8f64e9a`
> 本件是 D8 候选聚合骨架：只做筛选、去重、聚合与可执行性检查，不含新发现。行锚与数值一律回指来源件，不在本件重述为实测。

## 1 对象与现状 → 应为（按被修对象聚合）

| 对象（文件:行 或 配置键） | 内容锚 | 现状 | 应为 | 来源位点（成稿×提及行数） | 第②层小节与判定 | 证据入库状态 |
|---|---|---|---|---|---|---|

| `lib/phase2_session/p2_session.cpp` | — | 两处生产入口的 `tolerance_relative` 取值不同 | 定义层 `upm.h:78-80`（语义）＋ `:113`（默认 0）；赋值层是两个装配点；消费层 `upm.cpp:1036-1040` 唯一 ⇒ **不是"配置两侧默认不同"，是装配点一处显式设、另一处漏设回落默认**。正本 `PHASE2_UPM.md:98` 标题行即"容差随观测尺度归一，**禁绝对容差**"、`:114-115` 明写"生产必须 `tolerance_relative=1 | AUD-401-架构对齐.md×3、AUD-101-DA02-算法推导.md×1 | AUD203·V4[PASS/P2] | 入库 |
| `lib/infrastructure/cli/commands.cpp` | — | 两处生产入口的 `tolerance_relative` 取值不同 | 定义层 `upm.h:78-80`（语义）＋ `:113`（默认 0）；赋值层是两个装配点；消费层 `upm.cpp:1036-1040` 唯一 ⇒ **不是"配置两侧默认不同"，是装配点一处显式设、另一处漏设回落默认**。正本 `PHASE2_UPM.md:98` 标题行即"容差随观测尺度归一，**禁绝对容差**"、`:114-115` 明写"生产必须 `tolerance_relative=1 | AUD-401-架构对齐.md×2、AUD-101-DB-09.md×1、AUD-101-DB-12.md×1、AUD-402-判读-A3.md×1 | AUD203·V4[PASS/P2] 架构接线·W3[PASS] | 入库 |
| `lib/infrastructure/scheduler/src/module_adapters.cpp` | — | （见 §2 依据） | （同对象另有 9 条 D3 主张） | AUD-101-DB-03.md×8、AUD-401-架构对齐.md×7、AUD-101-DB-19.md×6、AUD-202-SNR核验.md×6、AUD-101-DB-20.md×4、AUD-403-注释与README.md×3、D9-工单对账.md×3、AUD-101-D1残余.md×2、AUD-201-测光核验.md×2、AUD-101-DB-08.md×1、AUD-101-DB-11.md×1、AUD-101-DB-12.md×1、AUD-101-DB-18.md×1、AUD-402-判读-A3.md×1 | AUD202-补·V5[PASS] AUD203·V3[PASS/P2] AUD203·V4[PASS/P2] 架构接线·W1[PASS] 架构接线·W3[PASS] 测光默认阶数·三[PASS] | 入库 |
| `docs/algorithms/PHASE2_UPM_IMPL.md` | — | 文档仍写 `s > 1e-12 → raw/sums×reliability` 且"归一化门 `s>1e-12`"；符号表仍列 `p2_upm_normalized_weights` 为有效导出符号 | 代码已改为尺度无关 `s > 0 ∧ isfinite(s)`（正确）；`p2_upm_normalized_weights` 已删除（`upm.cpp:1999` RETIRED，全仓零消费者）⇒ **文档滞后于代码**（同对象另有 2 条 D3 主张） | AUD-101-DA02-算法推导.md×2、AUD-101-DB-03.md×2、AUD-203-天光无缝核验.md×1 | AUD203·V4[PASS/P2] | 入库 |
| `docs/science/PHASE2_UPM.md` | — | §1 把 `ivar=1/variance` 命名为"Phase2 逐像素科学权重"；§11 同一句既写"适用域=空背景随机分量"又写"直接入加权"（同句自相矛盾）；§4.6 称 HiPS 里"只存"帧级 SNR 与稀疏绝对 SNR | 三处都是**指称越界**，不是数学分歧：`ivar` 对天光建模与 UPM 控制点拟合是正确权重（那一组样本按 §5b 排异分层只取源掩膜外，其总方差即 `σ_bg²`，见 `PHASE2_UPM.md:21,:76`）；对阶段二叠加则须由 §5c 加权方差面给权。`07_noise_snr.md:201` 的"只存"与已定案的 variance/ivar 子产品（`DATA_SEMANTICS`（同对象另有 7 条 D3 主张） | AUD-101-DA01-根规范与科学.md×14、AUD-101-DA02-算法推导.md×10、AUD-101-DB-04.md×9、AUD-203-天光无缝核验.md×8、AUD-101-DB-20.md×5、AUD-101-DB-09.md×3、AUD-301-文献池P1.md×3、AUD-101-DB-18.md×2、AUD-101-D1残余.md×1、AUD-301-文献复算-旧判批.md×1、论文3-回执.md×1 | AUD202·V3[VOID] AUD203·V1[PASS/P1] AUD203·V2[PASS/P1] AUD203·V3[PASS/P2] AUD203·V4[PASS/P2] | 入库 |
| `eng/packaging/config/defaults.json` | — | 常数出处登记为配置键；`authority_status: "sourced"` | 六键在 CI 基线里被自登为 `dead_config_key:photometry.*`，值实为代码字面量；同一文档 `:218` 指代码、`:220` 指死键 ⇒ 互相指认。**逐键历史判定：4 键为锚漂移（曾成立、随文档重排失效），`tukey_c` 为登记即错（8/8 个历史版本从未成立）**；4/6 回链行号指错（＋4/＋7/＋147/＋147）；`sourced` 的"有无权威出处"这（同对象另有 4 条 D3 主张） | AUD-101-DB01.md×3、AUD-101-DB-03.md×2、AUD-101-DB-05.md×2、AUD-101-DB-06-07.md×2、AUD-202-SNR核验.md×2、AUD-101-D1残余.md×1、AUD-101-DA01-根规范与科学.md×1、AUD-101-DB-10-补.md×1、AUD-101-DB-10.md×1、AUD-101-DB-18.md×1、AUD-201-测光核验.md×1、AUD-402-判读-A1.md×1、AUD-402-判读-A2.md×1、AUD-402-判读-A3.md×1、AUD-402-常数台账.说明.md×1 | AUD201·V4[PASS] | 入库 |

## 2 依据

「复核-AUD203」V4（判定：确认，定级 P2）：生产侧 = 1（合规），永不调度的 session 侧漏设回落 0（违正本「禁绝对容差」）；影响的是收敛判定本身而非仅日志；不构成活的 fail-open，但存在「覆盖键在一条路上静默失效」的不对称；标准 04 §1（阶段会话能力归并后独立会话目录退役）。

## 3 改法（具体动作，动词开头）

1. 二选一：随 ARCH-DEBT-01 下线 `lib/phase2_session/**` 通道，或把两条装配路径的默认装配统一为同一冻结函数
2. 在 `defaults.json` 与合同侧显式登记「该键的绝对容差禁用」语义，禁止兜底 0
3. 若下线：按标准 04 §4 以统一注释块标注退役原因、替代实现与日期口径，或直接删除

## 4 文件域（本任务允许触碰的路径集合）

```text
lib/phase2_session/p2_session.cpp
lib/infrastructure/cli/commands.cpp
lib/infrastructure/scheduler/src/module_adapters.cpp
docs/algorithms/PHASE2_UPM_IMPL.md
docs/science/PHASE2_UPM.md
eng/packaging/config/defaults.json
```

不改：上述之外的任何 `lib/`、`eng/`、`docs/`、`实验/`、`工程控制/` 路径；不顺手改科学公式、默认容差、SCI/ALG 冻结定义（AGENTS.md §6）。

## 5 与其他任务的关系

- 顺序 / 前置：
  - 前置：ACSD-T43（阶段调度器接线与 LIB 会话归并，取决于负责人裁-2）—— 下线哪条通道取决于阶段调度器身份
- 必须同批 / 文件域互斥（详表见总览 §5）：
  - 与 T46 同批或标先后；`module_adapters.cpp` 同文件簇

## 6 完成判据（可红可绿：注入下列之一它必须红）

- 在不带该键的装配路径上跑一次收敛判定 ⇒ 必须显式降级并写明原因；静默取 0 ⇒ 判红
- 正例：本任务全部改动落地后，上述判据在干净工作树上一律转绿；`python3 eng/ci/run_checks.py` 与相关 ctest 档全绿（重计算按 AGENTS.md §3 套 `mem_guard.py`）。

## 7 禁止

- 不得在「不动默认容差」的约束下私自把 0 改成别的值来让断言变绿

---

## 来源位点全量（54 份分片成稿 + 12 份复核件）

| 对象 | 成稿位点（文件:抽取行号，全量） | 第②层小节判定原文（截断） |
|---|---|---|
| `lib/phase2_session/p2_session.cpp` | AUD-101-DA02-算法推导.md:1365;AUD-401-架构对齐.md:182;AUD-401-架构对齐.md:408;AUD-401-架构对齐.md:57 | AUD203·V4：确认 |
| `lib/infrastructure/cli/commands.cpp` | AUD-101-DB-09.md:89;AUD-101-DB-12.md:164;AUD-401-架构对齐.md:140;AUD-401-架构对齐.md:148;AUD-402-判读-A3.md:70 | AUD203·V4：确认 ‖ 架构接线·W3：确认（三条事实全复现）＋ 补充（我另找到 2 条成稿未记的实质事实，其中 1 条把风险等级顶高、1 条把成稿押注的理由压低） |
| `lib/infrastructure/scheduler/src/module_adapters.cpp` | AUD-101-D1残余.md:158;AUD-101-D1残余.md:295;AUD-101-DB-03.md:147;AUD-101-DB-03.md:158;AUD-101-DB-03.md:502;AUD-101-DB-03.md:61;AUD-101-DB-03.md:619;AUD-101-DB-03.md:636;AUD-101-DB-03.md:735;AUD-101-DB-03.md:88;AUD-101-DB-08.md:152;AUD-101-DB-11.md:493… | AUD202-补·V5：确认（②③）**；① 的量纲部分**确认**，但其"数值偏 1.03–3.72 倍"的归因须**降级/订正**（偏差主要由缺 `ΣP²` 贡 ‖ AUD203·V3：确认 ‖ AUD203·V4：确认 ‖ 架构接线·W1：确认**（成稿 AUD401-005 的事实面与计数全部独立复现成功；我的定级理由与改法方向与成稿不同，见"与成稿差异"） ‖ 架构接线·W3：确认（三条事实全复现）＋ 补充（我另找到 2 条成稿未记的实质事实，其中 1 条把风险等级顶高、1 条把成稿押注的理由压低） ‖ 测光默认阶数·三：确认（事实链全部成立）＋ 定性与定级须修正 |
| `docs/algorithms/PHASE2_UPM_IMPL.md` | AUD-101-DA02-算法推导.md:162;AUD-101-DA02-算法推导.md:456;AUD-101-DB-03.md:125;AUD-101-DB-03.md:99;AUD-203-天光无缝核验.md:7 | AUD203·V4：确认 |
| `docs/science/PHASE2_UPM.md` | AUD-101-D1残余.md:111;AUD-101-DA01-根规范与科学.md:108;AUD-101-DA01-根规范与科学.md:193;AUD-101-DA01-根规范与科学.md:360;AUD-101-DA01-根规范与科学.md:421;AUD-101-DA01-根规范与科学.md:425;AUD-101-DA01-根规范与科学.md:436;AUD-101-DA01-根规范与科学.md:626;AUD-101-DA01-根规范与科学.md:63;AUD-101-DA01-根规范与科学.md:664;AUD-101-DA01-根规范与科学.md:666;AUD-101-DA01-根规范与科学.md:695… | AUD202·V3：推翻（"两篇相互排斥、须负责人裁决"这个定性不成立） ‖ AUD203·V1：确认 ‖ AUD203·V2：确认 ‖ AUD203·V3：确认 ‖ AUD203·V4：确认 |
| `eng/packaging/config/defaults.json` | AUD-101-D1残余.md:48;AUD-101-DA01-根规范与科学.md:527;AUD-101-DB-03.md:486;AUD-101-DB-03.md:520;AUD-101-DB-05.md:238;AUD-101-DB-05.md:239;AUD-101-DB-06-07.md:158;AUD-101-DB-06-07.md:42;AUD-101-DB-10-补.md:110;AUD-101-DB-10.md:48;AUD-101-DB-18.md:182;AUD-101-DB01.md:420… | AUD201·V4：确认**（四条子claim 全部独立复算成立；"造假 vs 漂移"的裁决见下，成稿未做这一层而我做了） |

