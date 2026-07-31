# AGENT.md - AI 代理操作指南

**最后更新**: 2026-07-31
**维护规则**: 本文件是 AI 代理在本项目工作的强制参考，每次会话开始时必须读取

---

## 1. 项目结构

```
Astro CS Normalization Database/
├── lib/                    # 项目源码 (C++/Python/Go)
│   ├── astro_image_io/     # HISS Reader/Writer/Codec/Transform/Stream
│   ├── calibration/        # Gaia 测光校准
│   ├── healpix_db/         # HEALPix 数据库 + Drizzle + Browser
│   ├── orchestrator/       # CLI 编排
│   ├── plate_solve/        # WCS 定标
│   └── ...
├── docs/                   # 项目文档 (非权威，仅参考)
├── 工程控制/                # 工程控制包 (权威工程规范)
├── tools/                  # 工具集 (astro_toolkit.py 等)
├── testdata/               # 测试数据
├── GaiaDR3/                # Gaia DR3 数据库 (外部数据)
├── output/                 # 输出目录
├── memory.md               # 项目记忆 (分模块记录)
└── AGENT.md                # 本文件
```

### 关键原则
- **`lib/` 是项目源码**：所有代码修改在此目录进行
- **`工程控制/` 是工程控制包**：权威工程规范、任务清单、证据
- **根目录只保留临时交付包和必要文件**：不堆积旧交付

---

## 2. 权威文档位置

**项目唯一权威文档维护在 GitHub Wiki 仓库**，避免本地文档不一致。

- **主仓库**: https://github.com/fujiaze/Astro-CS-Database
- **Wiki 仓库**: https://github.com/fujiaze/Astro-CS-Database.wiki.git

### Wiki 操作流程
```powershell
# 1. 克隆 Wiki (首次)
git clone https://github.com/fujiaze/Astro-CS-Database.wiki.git AstroCS.wiki

# 2. 修改 Wiki 页面后推送
cd AstroCS.wiki
git add .
git commit -m "docs(wiki): 更新说明"
git pull --rebase
git push
```

### Wiki 安全规则
- 推送前必须 `git pull --rebase`
- 不删除未知的现有 Wiki 页面
- 更新 `_Sidebar.md` 前检查原内容
- Agent 不得自行把 `WAITING_FOR_USER_REVIEW` 改为 `USER_APPROVED`
- 已冻结页面变更时，commit message 必须说明改变了哪项用户决策

### 本地文档 vs Wiki
- `docs/` 目录仅作为本地参考，不作为权威来源
- 任何规范变更必须同步到 Wiki
- `工程控制/docs/` 是工程控制包自带的规范，合并后以 Wiki 为准

---

## 3. 工程控制包合并规则

**每次新的工程控制包合并到 `工程控制/` 文件夹**。

### 合并流程
1. 解压/复制新的工程控制包内容
2. 逐目录合并到 `工程控制/`：
   - `agent/` → `工程控制/agent/` (覆盖旧文件)
   - `checklists/` → `工程控制/checklists/` (新增不覆盖)
   - `contracts/` → `工程控制/contracts/` (新增不覆盖)
   - `control/` → `工程控制/control/` (更新 CURRENT_TASK.md, DECISION_REGISTER.md 等)
   - `docs/` → `工程控制/docs/` (新增不覆盖)
   - `evidence/` → `工程控制/evidence/` (新增，不修改已有证据)
   - `tasks/` → `工程控制/tasks/` (新增不覆盖)
   - `templates/` → `工程控制/templates/` (新增不覆盖)
3. 合并完成后删除原始包目录
4. 提交：`git add 工程控制/ && git commit -m "chore(工程控制): 合并新工程控制包 v版本号"`

### 冲突处理
- 同名文件：新版本覆盖旧版本，但在 commit message 中说明变更
- 证据目录 (`evidence/`)：永不覆盖已有证据，只新增

---

## 4. 工具使用指南

**使用 `tools/` 目录的工具执行操作，避免打扰用户。**

### astro_toolkit.py (主要工具)
Python 脚本 + JSON 配置驱动的批量操作工具，用于跳过沙箱确认、保证连续运行。

```powershell
# 用法
python tools/astro_toolkit.py <config.json> --log <logfile>

# 打印示例配置
python tools/astro_toolkit.py --example
```

### 支持的操作类型
| type | 说明 |
|------|------|
| `git_status` | 查看 git 状态 |
| `git_add` | 暂存文件 |
| `git_commit` | 提交 (使用 -F 文件参数避免长 message 扫描超时) |
| `git_push` | 推送 |
| `git_log` | 查看日志 |
| `run_orchestrator` | 运行 orchestrator (自动注入 build/artifacts 和 mingw64/bin 到 PATH) |
| `sha256` | 计算文件哈希 |
| `mkdir` | 创建目录 |
| `write_file` | 写文件 |
| `copy_file` | 复制文件 |
| `delete_file` | 删除文件 |
| `list_dir` | 列目录 |

### 典型链
```json
[
    {"type": "git_add", "files": ["lib/astro_image_io/src/hiss_common.cpp"]},
    {"type": "git_commit", "message_file": "COMMIT_MSG.txt"},
    {"type": "git_push"},
    {"type": "git_log", "count": 5}
]
```

### vq-commit.ps1 (备用提交工具)
```powershell
vq-commit.ps1 -MessageFile <msg.txt> -Repo "仓库路径" -Files "file1,file2"
```

### 使用原则
- **子 Agent 执行多步操作时**：优先写 JSON 配置然后一次 RunCommand 调用 astro_toolkit
- **commit 走 `git commit -F <message_file>`**：避免长 message 触发扫描超时
- **每步有 timeout_sec 保护**：避免无限等待

---

## 5. 开发工作流规则

### 提交规则
- **精细化 commit**：完成最小任务后必须 commit 留痕
- **完成一阶段子任务 commit 后进行一次 push**
- **commit message 用 conventional commits 格式**：`feat/fix/docs/chore/refactor`

### PowerShell 7 环境
- **强制使用 PowerShell 7 运行环境**
- **禁止在 WSL 进行远程推送** (WSL 无代理)

### 分模块开发
- 每个模块独立建立文件夹与模块开发 memory
- 在根 memory.md 中索引
- 程序中加入详细日志输出
- 每个模块建立日志目录，便于分析

### 任务执行流程
1. 任何任务开始前调用 `iterative-discussion` 技能走确认流程
2. 分析任务依赖关系，先统一公共约定
3. 分配给 Subcoding Agent 执行，最大化安全并行
4. 子任务完成后统一集成与验收

### 用户授权规则
- 用户的自主执行授权仅一次对话有效
- 不得长期默认授权
- 需要向用户二次确认
- 在遇到任何不确定的情况时，强制使用提问工具向用户提问

---

## 6. 关键约束 (从 project_memory.md 继承)

### HISS/Stage1 冻结约束
- signal = 累计通量 (不除面积)
- support = 面积比 [0, 255] uint8
- Tile 叶像素数 = 4^d (d = min(9, log2(NSIDE/16)))
- 球面重叠用 Girard 定理，禁止平面近似
- pixfrac 范围 (0, 1]，拒绝 <=0 和 >1
- 自动 NSIDE 上限 2^22
- Writer 流式写入临时池，不在内存保存全部 Tile
- 原子替换用 MoveFileExW(MOVEFILE_REPLACE_EXISTING)

### PlateSolve 约束
- 向量匹配用 gnomonic 投影
- Y 轴反转 (图像 Y 向下，天空 Dec 向上)
- 候选半径 0.5×FOV 对角线
- RANSAC 内点验证同时用欧氏距离和向量叉积
- Umeyama SVD 替代第二次 RANSAC
- 尺度因子 s 限制在初值 ±10% 内

### 性能约束
- C++ 版本性能不低于 Python 版本
- Gaia 客户端 C++ 内存缓存 (60s TTL)
- 星检测串行执行，解析完成后并行

---

## 7. 常用路径

| 用途 | 路径 |
|------|------|
| GitHub CLI | `C:\Users\fujia\AppData\Local\Temp\gh-cli-install\bin\gh.exe` |
| Commit 脚本 | `C:\Users\fujia\bin\vq-commit.ps1` |
| MSYS2 MinGW64 | `C:\msys64\mingw64\bin` |
| 项目根目录 | `f:\Astro dev\Astro CS Normalization Database` |
| 记忆目录 | `c:\Users\fujia\.trae-cn\memory\projects\-f-Astro-dev-Astro-CS-Normalization-Database` |
