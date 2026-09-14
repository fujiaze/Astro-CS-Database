# 包取证摘要：AstroCS_RELEASE_RESCUE_CONTROL_V3_20260912

身份由脚本归一（去复原前缀与 .zip 后缀）。共 2 个实例。

## 【worktree】工程控制/AstroCS_RELEASE_RESCUE_CONTROL_V3_20260912
- 文件 24 | 32.4 KB
- git 事件: {'first': None, 'last': None, 'dels': None, 'adds_n': 0, 'mod_n': None, 'touch_last': None}
- 规格文件: 00_READ_FIRST.md, 01_SCIENCE_AUTHORITY_BASELINE.md, 02_E2E_ACCEPTANCE.md
- tasks/ 共 18: AUD-CI-LIVE.md, AUD-COORD.md, AUD-E2E.md, AUD-P1.md, AUD-P2P3.md, CI-LINUX-EXACT.md, DOC-CLOSE.md, FINAL-AUDIT.md, FIX-CI.md, FIX-E2E.md, FIX-SCIENCE.md, LINUX-FREEZE.md, REAL-MOSAIC-E2E.md, REAL-PLATESOLVE-ALL.md, RESCUE-RULING.md, RESCUE-SNAPSHOT.md, REVIEW-SCIENCE.md, WIN-PORT-VERIFY.md
- START_PROMPT 开头: 读取本工作包入口并执行：全部工作派给SubAgent；审计并行、源码串行；优先恢复E2E；外部等待必须超时。 ⏎ 
- 00_READ_FIRST 标题: # AstroCS 科学重审与全流程恢复工作包 V3 ; ## 唯一目标 ; ## 执行原则 ; ## 前台启动

## 【zip】工程控制/_control_packs/AstroCS_RELEASE_RESCUE_CONTROL_V3_20260912.zip
- 文件 24 | 32.4 KB
- sha256: 14ee6592f0e1e6b556bdaeb156ba33e1898d2998cf3e97ab7d09dcb401f6fdf5
- git 事件: {'first': None, 'last': None, 'dels': None, 'adds_n': 0, 'mod_n': None, 'touch_last': None}
- zip 顶层: AstroCS_RELEASE_RESCUE_CONTROL_V3_20260912
- zip 内 marker: 00_READ_FIRST.md, START_PROMPT.txt
- zip 内规格文件: 00_READ_FIRST.md, 01_SCIENCE_AUTHORITY_BASELINE.md, 02_E2E_ACCEPTANCE.md
- 00_READ_FIRST 开头:

# AstroCS 科学重审与全流程恢复工作包 V3

本包不依赖 CPRun、CP 插件、任务图解析器或其他专用调度框架。  
适用仓库：`fujiaze/Astro-CS-Database` 的 `main`  
输入状态提示：`f5f944a512017642193f687356e6a8b83c20997e`，但执行时必须以当前 `origin/main` 为准。

## 唯一目标

在不回退既有重构成果的前提下，依据外部权威标准和独立 Oracle 重审科学定义与代码，尽快恢复：

1. 当前源码可构建；
2. Phase1、Phase2、Phase3 分别通过真实 CLI 运行，并由测试编排依次交换持久化产品；
3. Linux CI 与全量真实数据终验成功，并冻结 Linux 科学基线；
4. Windows 只在冻结算法上处理编译、ABI、安装树和跨平台数值对拍，随后 Fatduck 验证；
5. 文档、合同、实现、测试与证据在最终 SHA 上一致。

## 执行原则

- 先收敛为一个前台和一份活动控制包；旧会话、旧控制包不得继续派工。
- 审计可以并行；任何 tracked 源码写入都走 `repo-write` 单通道。
- 前台只派发、审证、集成、提交；Worker 不执行 git 写操作。
- 不新建分支/worktree/仓库；不 reset/stash/clean/rebase；不覆盖未提交内容。
- 900 余提交时的可运行版本只用于 git archaeology 和行为线索，不 checkout、运行、整树复制或盲目回退。
- 第一可运行闭环优先于目录美化、接口扩展、覆盖率扩建、Phase3 新投影、ACR、GUI。
- 不靠 waiver、skip、扩大 known-failures、放宽容差或把 partial 写成 complete 获得绿灯。
- 所有外部进程、网络、CI、真实数据和硬件等待必须设置明确超时；重计算同步记录 CPU、内存和磁盘 I/O。
- Phase 独立产品语义保持不变；测试编排器可以依次调用三个 CLI，但产品中不得新增隐式三阶段连跑命令。
- Linux 是本轮科学实现与算法冻结平台。Linux 基线冻结后，Windows 阶段不得修改科学公式、默认参数、容差、数据语义或测试真值；确需修改时必须撤销冻结并回到 Linux 重跑。

## 前台启动

1. 读取本文件、`FRONT_DESK_RUNBOOK.md`、`TASK_LIST.md` 和 01–02 两份基线。
2. 直接把 `tasks/RESCUE-SNAPSHOT.md` 全文交给一个 SubAgent。
3. 验收快照后，同时派发五个审计任务；后续严格按 `TASK_LIST.md` 推进。
4. 前台不亲自实施源码任务；只负

