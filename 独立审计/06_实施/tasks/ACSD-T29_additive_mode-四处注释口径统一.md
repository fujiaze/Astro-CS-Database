# 任务：ACSD-T29 注释与代码默认打架：四处措辞统一到正本记法，不动码

> 波次 `W1` ｜ 杠杆分档 `P2` ｜ 整改域 文档+注释 ｜ 基线 HEAD `c8f64e9a`
> 本件是 D8 候选聚合骨架：只做筛选、去重、聚合与可执行性检查，不含新发现。行锚与数值一律回指来源件，不在本件重述为实测。

## 1 对象与现状 → 应为（按被修对象聚合）

| 对象（文件:行 或 配置键） | 内容锚 | 现状 | 应为 | 来源位点（成稿×提及行数） | 第②层小节与判定 | 证据入库状态 |
|---|---|---|---|---|---|---|

| `lib/infrastructure/scheduler/src/module_adapters.cpp` | — | （见 §2 依据） | （同对象另有 9 条 D3 主张） | AUD-101-DB-03.md×8、AUD-401-架构对齐.md×7、AUD-101-DB-19.md×6、AUD-202-SNR核验.md×6、AUD-101-DB-20.md×4、AUD-403-注释与README.md×3、D9-工单对账.md×3、AUD-101-D1残余.md×2、AUD-201-测光核验.md×2、AUD-101-DB-08.md×1、AUD-101-DB-11.md×1、AUD-101-DB-12.md×1、AUD-101-DB-18.md×1、AUD-402-判读-A3.md×1 | AUD202-补·V5[PASS] AUD203·V3[PASS/P2] AUD203·V4[PASS/P2] 架构接线·W1[PASS] 架构接线·W3[PASS] 测光默认阶数·三[PASS] | 入库 |
| `docs/modules/phase2_upm.md` | — | 三处注释/文档口径：①`:9820`"生产默认 `additive_mode = c`（本文件 `:5503` 起），δ 从不施加"；②`:9834`"§9.67 定案 1：默认 delta"；③`:10300`"`c = raw − C_k`（全减，含 B_ref ⇒ 背景被剪掉；仅对照）"；另 `docs/modules/phase2 | ①**假**（三处皆假：默认是 delta；δ 在默认路径确实施加——`sub_delta` 为真、`:10541-10549` 逐像素减 δ；且它自称的锚点 `:5503` 处是 `mark_frame_fail` lambda ⇒ **锚点也已漂移**）；②**真**；③前半句真、"全减含 B_ref"**假**；第四处同样为假 | AUD-101-DB-03.md×5、AUD-101-DB02.md×1 | AUD203·V3[PASS/P2] | 入库 |
| `docs/science/PHASE2_UPM.md` | — | §1 把 `ivar=1/variance` 命名为"Phase2 逐像素科学权重"；§11 同一句既写"适用域=空背景随机分量"又写"直接入加权"（同句自相矛盾）；§4.6 称 HiPS 里"只存"帧级 SNR 与稀疏绝对 SNR | 三处都是**指称越界**，不是数学分歧：`ivar` 对天光建模与 UPM 控制点拟合是正确权重（那一组样本按 §5b 排异分层只取源掩膜外，其总方差即 `σ_bg²`，见 `PHASE2_UPM.md:21,:76`）；对阶段二叠加则须由 §5c 加权方差面给权。`07_noise_snr.md:201` 的"只存"与已定案的 variance/ivar 子产品（`DATA_SEMANTICS`（同对象另有 7 条 D3 主张） | AUD-101-DA01-根规范与科学.md×14、AUD-101-DA02-算法推导.md×10、AUD-101-DB-04.md×9、AUD-203-天光无缝核验.md×8、AUD-101-DB-20.md×5、AUD-101-DB-09.md×3、AUD-301-文献池P1.md×3、AUD-101-DB-18.md×2、AUD-101-D1残余.md×1、AUD-301-文献复算-旧判批.md×1、论文3-回执.md×1 | AUD202·V3[VOID] AUD203·V1[PASS/P1] AUD203·V2[PASS/P1] AUD203·V3[PASS/P2] AUD203·V4[PASS/P2] | 入库 |
| `docs/plugins/algorithms_phase2/11_upm.md` | — | 三处注释/文档口径：①`:9820`"生产默认 `additive_mode = c`（本文件 `:5503` 起），δ 从不施加"；②`:9834`"§9.67 定案 1：默认 delta"；③`:10300`"`c = raw − C_k`（全减，含 B_ref ⇒ 背景被剪掉；仅对照）"；另 `docs/modules/phase2 | ①**假**（三处皆假：默认是 delta；δ 在默认路径确实施加——`sub_delta` 为真、`:10541-10549` 逐像素减 δ；且它自称的锚点 `:5503` 处是 `mark_frame_fail` lambda ⇒ **锚点也已漂移**）；②**真**；③前半句真、"全减含 B_ref"**假**；第四处同样为假（同对象另有 1 条 D3 主张） | AUD-101-DB-20.md×6、AUD-101-DB-04.md×2、AUD-101-DB-03.md×1、AUD-101-DB-19.md×1、D9-工单对账.md×1 | — | 入库 |
| `eng/packaging/config/config_registry.json` | — | （见 §2 依据） | （见 §3 改法对应步骤） | AUD-101-DB-03.md×2、AUD-101-DB-06-07.md×1、AUD-401-架构对齐.md×1、AUD-402-判读-A3.md×1 | AUD203·V3[PASS/P2] 合同层·W3[PASS] | 入库 |

## 2 依据

「复核-AUD203」V3（判定：确认，定级 P2；行为正确 ⇒ 纯注释/符号口径缺陷）：三处注释里 `:9820`（默认 c、δ 从不施加）与 `:10300`（c = 全减含 B_ref）为假、`:9834` 为真，另有成稿漏的第四处 `docs/modules/phase2_upm.md:75-76`；《天光无缝核验报告》DEV-04（`:9820` 自称的锚点 `:5503` 处实为 `mark_frame_fail` lambda ⇒ 锚点也已漂移；`upm.cpp:976-977` 把分量参考帧帧行恒置 0 ⇒ mode c 对参考帧是恒等变换；默认实为 delta）。

## 3 改法（具体动作，动词开头）

1. 按 `docs/modules/phase2_upm.md:70-72` 的强制消歧规则，把四处统一到正本 `C_f = 帧分量（仅偏差）`／`b_k = 全量`
2. 在 `docs/modules/phase2_upm.md:75-76` 写「实现面默认已为 delta」，删除「属待改」的错误断言
3. 订正 `:9820` 的失效锚 `:5503`
4. `additive_mode` 键补登记（`config_registry.json:1855` 现记 finding=unregistered）

## 4 文件域（本任务允许触碰的路径集合）

```text
lib/infrastructure/scheduler/src/module_adapters.cpp
docs/modules/phase2_upm.md
docs/science/PHASE2_UPM.md
docs/plugins/algorithms_phase2/11_upm.md
eng/packaging/config/config_registry.json
```

不改：上述之外的任何 `lib/`、`eng/`、`docs/`、`实验/`、`工程控制/` 路径；不顺手改科学公式、默认容差、SCI/ALG 冻结定义（AGENTS.md §6）。

## 5 与其他任务的关系

- 顺序 / 前置：
  - 无前置
- 必须同批 / 文件域互斥（详表见总览 §5）：
  - `module_adapters.cpp` 同文件簇（见 T20 行）⇒ 同批或串行；`config_registry.json` 与 T05/T36 共改

## 6 完成判据（可红可绿：注入下列之一它必须红）

- 注入一条「`additive_mode = c` 即全减背景」的注释 ⇒ 与正本消歧规则比对判红
- 注释锚 `:5503` 未订正即留 ⇒ ACSD-T06 的锚门判红
- 正例：本任务全部改动落地后，上述判据在干净工作树上一律转绿；`python3 eng/ci/run_checks.py` 与相关 ctest 档全绿（重计算按 AGENTS.md §3 套 `mem_guard.py`）。

## 7 禁止

- 不得动码（第②层判定：行为面正确且可审计，默认 delta、缺产物时显式降级、manifest 全落）；不得以改注释顺带改默认值

## 8 登记与边界

唯一真实风险是误用：把 `additive_mode=c` 当论文里的「全减背景退化臂」来跑，得到的其实是另一套仍保背景的、对齐到参考帧的加性扣除。

---

## 来源位点全量（54 份分片成稿 + 12 份复核件）

| 对象 | 成稿位点（文件:抽取行号，全量） | 第②层小节判定原文（截断） |
|---|---|---|
| `lib/infrastructure/scheduler/src/module_adapters.cpp` | AUD-101-D1残余.md:158;AUD-101-D1残余.md:295;AUD-101-DB-03.md:147;AUD-101-DB-03.md:158;AUD-101-DB-03.md:502;AUD-101-DB-03.md:61;AUD-101-DB-03.md:619;AUD-101-DB-03.md:636;AUD-101-DB-03.md:735;AUD-101-DB-03.md:88;AUD-101-DB-08.md:152;AUD-101-DB-11.md:493… | AUD202-补·V5：确认（②③）**；① 的量纲部分**确认**，但其"数值偏 1.03–3.72 倍"的归因须**降级/订正**（偏差主要由缺 `ΣP²` 贡 ‖ AUD203·V3：确认 ‖ AUD203·V4：确认 ‖ 架构接线·W1：确认**（成稿 AUD401-005 的事实面与计数全部独立复现成功；我的定级理由与改法方向与成稿不同，见"与成稿差异"） ‖ 架构接线·W3：确认（三条事实全复现）＋ 补充（我另找到 2 条成稿未记的实质事实，其中 1 条把风险等级顶高、1 条把成稿押注的理由压低） ‖ 测光默认阶数·三：确认（事实链全部成立）＋ 定性与定级须修正 |
| `docs/modules/phase2_upm.md` | AUD-101-DB-03.md:102;AUD-101-DB-03.md:128;AUD-101-DB-03.md:135;AUD-101-DB-03.md:551;AUD-101-DB-03.md:770;AUD-101-DB02.md:97 | AUD203·V3：确认 |
| `docs/science/PHASE2_UPM.md` | AUD-101-D1残余.md:111;AUD-101-DA01-根规范与科学.md:108;AUD-101-DA01-根规范与科学.md:193;AUD-101-DA01-根规范与科学.md:360;AUD-101-DA01-根规范与科学.md:421;AUD-101-DA01-根规范与科学.md:425;AUD-101-DA01-根规范与科学.md:436;AUD-101-DA01-根规范与科学.md:626;AUD-101-DA01-根规范与科学.md:63;AUD-101-DA01-根规范与科学.md:664;AUD-101-DA01-根规范与科学.md:666;AUD-101-DA01-根规范与科学.md:695… | AUD202·V3：推翻（"两篇相互排斥、须负责人裁决"这个定性不成立） ‖ AUD203·V1：确认 ‖ AUD203·V2：确认 ‖ AUD203·V3：确认 ‖ AUD203·V4：确认 |
| `docs/plugins/algorithms_phase2/11_upm.md` | AUD-101-DB-03.md:130;AUD-101-DB-04.md:42;AUD-101-DB-04.md:46;AUD-101-DB-19.md:1398;AUD-101-DB-20.md:203;AUD-101-DB-20.md:204;AUD-101-DB-20.md:337;AUD-101-DB-20.md:389;AUD-101-DB-20.md:67;AUD-101-DB-20.md:68;D9-工单对账.md:116 | — |
| `eng/packaging/config/config_registry.json` | AUD-101-DB-03.md:416;AUD-101-DB-03.md:671;AUD-101-DB-06-07.md:56;AUD-401-架构对齐.md:256;AUD-402-判读-A3.md:45 | AUD203·V3：确认 ‖ 合同层·W3：确认（成稿列出的四组差异逐条复算全部成立）＋ 定级上调（性质不是"抄了旧快照"，是"越位成第二套登记面"，且其复制对象自身也已漂移；按实现逐 |

