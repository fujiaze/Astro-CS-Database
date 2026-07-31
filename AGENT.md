# AGENT.md — AI 代理操作指南

**最后更新**: 2026-07-31
**维护规则**: AI 代理每次会话开始时强制读取本文件

---

## 1. 项目根目录结构

```
Astro CS Normalization Database/
├── lib/                    # 项目源码 (C++/Python/Go) — 唯一修改代码处
├── 工程控制/                # 权威工程规范、任务清单、证据
├── tools/                  # astro_toolkit.py, gen_audit_pack.py
├── testdata/                # 测试数据 + index.json 索引
├── docs/                    # 本地参考文档 [待删 — Wiki 确定后删除]
├── memory.md                # 项目记忆 (分模块索引)
├── AGENT.md                 # 本文件
├── README.md
├── .gitignore
└── (外部数据目录，见 §3)
```

### 关键原则
- **`lib/` 是项目源码**：所有代码修改仅在此目录
- **`工程控制/` 是工程控制包**：权威工程规范、任务清单、证据
- **根目录只保留临时交付包和必要文件**：不堆积旧交付

---

## 2. 权威文档位置 — GitHub Wiki

**项目唯一权威文档维护在 GitHub Wiki 仓库**，避免本地文档不一致。

- **主仓库**: https://github.com/fujiaze/Astro-CS-Database
- **Wiki 仓库**: https://github.com/fujiaze/Astro-CS-Database.wiki.git

### Wiki 操作
```powershell
git clone https://github.com/fujiaze/Astro-CS-Database.wiki.git AstroCS.wiki  # 首次
# 修改后推送
cd AstroCS.wiki
git pull --rebase  # 推送前必须 rebase
git add . && git commit -m "docs(wiki): <说明>" && git push
```

### Wiki 安全规则
- 不删除未知的现有 Wiki 页面
- 更新 `_Sidebar.md` 前检查原内容
- 不得自行把 `WAITING_FOR_USER_REVIEW` 改为 `USER_APPROVED`
- 已冻结页面变更必须在 commit message 中说明改了哪项用户决策

### 本地文档 vs Wiki
- `docs/` 待删 — Wiki 全部确定后删除，**之前不得引用 `docs/` 作为权威**
- `工程控制/docs/` 是工程控制包自带规范，合并后以 Wiki 为准
- 任何规范变更必须同步到 Wiki

---

## 3. 外部数据目录（本地保留，不入仓库）

**以下目录在 `.gitignore` 中排除，仅供本地使用，AI 不得删除：**

| 目录 | 大小 | 用途 | 红线 |
|------|------|------|------|
| `GaiaDR3/` | 41.9 GB | Gaia DR3 星表本体，plate solving 与测光参考星来源 | **AI 历史上多次误删，禁止删除** |
| `GaiaDR3SP/` | 64.7 GB | Gaia DR3 光谱数据库，测光定标用 | 禁止删除 |
| `siril-1.4.3/` | 55.2 MB | Siril 天文图像处理软件源码，开发时参考其实现 | 仅参考，不入仓库 |
| `output/` | 578.7 MB | 运行时输出目录（截图、临时结果等） | 可定期清理 |

### testdata 目录
- 7 个数据集覆盖 T2/T3/T4 三个望远镜（T1 数据未入库，保留占位）
- 详细索引见 `testdata/index.json`
- 包含 `T2/T3/T4 calibration files` 三个空目录（待补充 master bias/dark/flat）
- `testdata/results/` 下为已校准亮场（01/04_calibrated.fits）

---

## 4. 工程控制包合并规则

新的工程控制包合并到 `工程控制/`：

| 源目录 | 目标 | 合并策略 |
|--------|------|----------|
| `agent/` | `工程控制/agent/` | 覆盖旧文件 |
| `checklists/` | `工程控制/checklists/` | 新增不覆盖 |
| `contracts/` | `工程控制/contracts/` | 新增不覆盖 |
| `control/` | `工程控制/control/` | 更新 CURRENT_TASK.md, DECISION_REGISTER.md 等 |
| `docs/` | `工程控制/docs/` | 新增不覆盖 |
| `evidence/` | `工程控制/evidence/` | **永不覆盖已有证据，只新增** |
| `tasks/` | `工程控制/tasks/` | 新增不覆盖 |
| `templates/` | `工程控制/templates/` | 新增不覆盖 |

合并完成后删除原始包目录，提交：`chore(工程控制): 合并新工程控制包 v版本号`

---

## 5. 工具与提交规范

### astro_toolkit.py（首选，跳过沙箱确认）
```powershell
python tools/astro_toolkit.py <config.json> --log <logfile>
python tools/astro_toolkit.py --example  # 示例配置
```
支持操作：`git_status/git_add/git_commit/git_push/git_log/run_orchestrator/sha256/mkdir/write_file/copy_file/delete_file/list_dir`。`run_orchestrator` 自动注入 build/artifacts 和 mingw64/bin 到 PATH。

### vq-commit.ps1（备用）
```powershell
vq-commit.ps1 -MessageFile <msg.txt> -Repo "仓库路径" -Files "file1,file2"
```

### 提交规则
- **精细化 commit**：完成最小任务后必须 commit 留痕
- **完成一阶段子任务后 push 一次**
- **commit message 用 conventional commits**：`feat/fix/docs/chore/refactor`
- **commit 走 `git commit -F <message_file>`**：避免长 message 触发扫描超时
- **子 Agent 多步操作时**：写一个 JSON 配置一次 RunCommand 调用 astro_toolkit

---

## 6. 强制流程规则

### 任务执行流程
1. **任何任务开始前必须调用 `iterative-discussion` 技能走确认流程**（用户明确给文档的除外）
2. 分析任务依赖关系，先统一公共约定
3. 分配给 Subcoding Agent 执行，最大化安全并行
4. 子任务完成后统一集成与验收

### 环境约束
- **强制使用 PowerShell 7 运行环境**（WSL 无代理，禁止 WSL 远程推送）
- **GitHub CLI**：`C:\Users\fujia\AppData\Local\Temp\gh-cli-install\bin\gh.exe`
- **MSYS2 MinGW64**：`C:\msys64\mingw64\bin`
- **Commit 脚本**：`C:\Users\fujia\bin\vq-commit.ps1`
- **记忆目录**：`c:\Users\fujia\.trae-cn\memory\projects\-f-Astro-dev-Astro-CS-Normalization-Database`

### 用户授权规则
- 用户的自主执行授权**仅一次对话有效**，不得长期默认授权
- 需要向用户二次确认
- 在遇到任何不确定的情况时，**强制使用 AskUserQuestion 提问工具向用户提问**

### 模块化与日志
- 每个模块独立建立文件夹与模块开发 memory（已在根 `memory.md` 索引）
- 程序中加入详细日志输出，每个模块建立日志目录
- **禁止在单个文件堆叠上千行代码**：职责单一、模块化

---

## 7. 关键科学约束（指针）

**详细冻结约束、算法参数、性能要求见 `c:\Users\fujia\.trae-cn\memory\projects\-f-Astro-dev-Astro-CS-Normalization-Database\project_memory.md` 与 GitHub Wiki。**

以下是核心红线（违反即返工）：

- **HISS/Stage1**：signal=累计通量（不除面积）/ support=面积比[0,255] uint8 / Tile叶像素数=4^d / 球面重叠用 Girard 定理 / pixfrac∈(0,1] / 自动 NSIDE 上限 2^22
- **PlateSolve**：向量匹配用 gnomonic 投影 / Y轴反转 / 候选半径 0.5×FOV / RANSAC+Umeyama SVD / 尺度因子 s 限制±10%
- **性能**：C++ 版本性能不低于 Python / Gaia 客户端 60s TTL 内存缓存 / 星检测串行
- **测试**：契约不满足必须真正失败，禁止 `ASSERT_TRUE(true)` 软通过

---

## 8. 当前状态（2026-07-31）

- **Stage1 修复 v1 已交付，ChatGPT 审查拒绝验收**（17 项阻断性问题）
- 审查包：`AstroCS_Stage1_Fix_Review_2026-07-31.zip`（根目录，本地保留不入仓库）
- 17 步优先修复顺序已明确，等 ChatGPT 审查完毕后再讨论修复方案
- Wiki 已基本完成，部分页面待用户冻结决策项后同步更新
