# 前台自证条目（producer=FD，负责人裁定：保留现状、仅登记为问题）

> 本文件的条目由**前台自己**取证并定稿（全部 M 域已收工，无人承接），编号 `FD-*`，与 `M*`/`L*` 同级引用。
> 取证手段：会话内一次 agent 指令注入收到全文 + 当场只读 `find`/`git ls-files` 清点，未修改任何被扫描文件。

### FD-G-001 影子树内的 agent 指令文件与冻结宪章在「唯一入口」上正面冲突，且对全部机器门永久不可见

- **ID**：FD-G-001
- **类别**：G_GOV_GATE
- **优先级**：**P1**，带「**依它提交代码即升 P0**」条件升级标签（与 M9 §6 同手法）
- **负责人裁定（本轮）**：**不做改名处置**，仅登记为问题；影子文件一律原地保留。
- **位置**：`run/reaudit_v3/run001/A/AGENTS.md::正式运行只有 orchestrator.exe`（207 行、UNTRACKED）；同文本第二份 `run/reaudit_v3/run001/B/AGENTS.md`（207 行）；另有 `run/arch_audit_p1/shadow_base`(78)、`run/arch_audit_p1/shadow`(57)、`run/scif2001/shadow`(57)、`run/rt001/base_shadow`(57)、`run/release-rescue/**` 七份 clean/clean-check*/base_src/mut/src/a4_src(各 78)、`工程控制/AstroCS_V7_MODULAR_REFOUNDATION_CONTROL_20260902_FINAL3/templates/AGENTS.md`(5)
- **问题说明**：全仓 agent 指令文件（AGENTS.md/CLAUDE.md/*.rules/.cursorrules）**18 份，仅根 `AGENTS.md`（78 行）1 份被 git 跟踪**，其余 **17 份 UNTRACKED** ⇒ 对 CI、对文档门、对 `git ls-files` 类闭包判据**同时不可见**。长度有 **207/78/57/5 四种**，影子 207 行版含根版没有的整节内容，构成**另一套规范**而非快照。其原文与本审计绝大多数条目的判定基线直接互斥：
  ①「正式运行只有 `orchestrator.exe <stage1.json>`」+「`.\toolchain.ps1 run <stage1.json>` 运行 orchestrator（**唯一正式入口**）」vs 宪章 §3.1（每平台一个用户可见入口）与 §8.1（CLI 只暴露一个薄的 `astrocs` 入口、不实现科学公式），根 AGENTS.md「单入口」要素同；
  ②「Python 生产层已删除」vs 根构建仍产 `astrocs_python_abi3`、`lib/astrocs_py/` 在 `add_subdirectory` 8 项内、`packaging/astrocs.product.json` 列 `bin/_astrocs.pyd` 为 required 单元；
  ③「项目**唯一权威文档**维护在 GitHub Wiki 仓库」vs §1.1 权威分层（宪章 > docs/science·algorithms·contracts > 其余），且根目录规范把 `AstroCS.wiki/` 列为**用户/资料区、禁改禁删的本地克隆**；
  ④隐含第二套状态与退出码面（DLL 由其相对路径加载、orchestrator 静态链接），与已定稿的 M8a-C-001（`lib/orchestrator/README.md` 固化第二套 `ASTROCS_*` 码表）同型不同主体。
- **影响**：它是**活的指令注入**（本会话即收到该文件全文，抬头为「These instructions apply to work under `run/reaudit_v3/run001/A`」）。任何在该路径下启动的 agent 会取得与根 AGENTS.md **相反**的"唯一正式入口"要素，并按 §8.1 明令禁止的方式实现入口，**且自认合规**；采信 ③ 则 §1.1 分层被文件系统的偶然性覆盖，本审计约 180 条依赖 §1.1 的条目在其视角下全部失去依据。**没有任何门会因两份 AGENTS.md 互斥而变红。**
- **依据条款**：宪章 §1.1（权威分层）、§3.1、§8.1、§12.3-10（状态与声明须同源）、§14.1；根 `AGENTS.md`「目录规范（强制）」与「单入口」要素
- **建议处置**（负责人已选"仅登记"，以下为留档建议）：①**不改名、不删除**（已裁定）；②改由**门**处理：新增机器门校验"被跟踪的 agent 指令文件必须与根 AGENTS.md 十要素 + §1.1 一致"，并对免报区出现的指令文件**只报告不阻断**（`check_comment_hygiene.py` 那类"写 json/md 但恒 return 0"的形态即 C-11 要治的病，新门不得重犯）；③在 `AGENTS.md` 目录规范中明文登记 `run/**` 内 `AGENTS.md` 的**非权威**地位，消除注入歧义（登记属条文修改，须 §1.2 流程）
- **取证口径**：`find`（含免报区）+ `git ls-files --error-unmatch` 逐份判 TRACKED/UNTRACKED + `wc -l` 计数；引文均为注入原文当场读取，非二手转述
- **置信度**：高（18 份清点、4 种长度、UNTRACKED 判定、四段引文全部一手）
- **related**：M8a-C-001（同型不同主体：README 面向人且被跟踪 vs AGENTS 面向 agent 且在免报区，**不并档**）、M6b-G-002（状态声明无单一事实源）、M6a §10（wiki 权威性待裁，本条提供实证）、M4-F-01/M2a-F-1/L23-004（构建孤儿形态，本条为**指令面同族**）、L25-005（豁免按字面目录名切分 = 换目录即逃检，与本条同根因）、C-10/C-07、F00-11
- **状态**：登记为问题，**不处置**（负责人 2026 裁定）；R 层不得因"未修复"重复计为残余风险之外的新条目
