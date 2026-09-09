# 01｜冻结约束

## 产品与平台

- 当前线：`0.11.0-alpha`；本轮目标：`0.11.0-alpha.2`。
- 唯一 CPU 架构：amd64/x86-64。
- Windows 10+ 是主要客户端平台；正式 Windows 构建固定 GitHub `windows-2022`、Visual Studio 2022、MSVC v143，并记录实际版本。
- 现有 Linux 服务器是 Agent 工作区、静态检查和轻量验证节点；GitHub 托管 Ubuntu/Windows runner 承担自动 CI。
- GUI 不在本轮。HiPS Browser 独立于 CLI/计算 DLL；未来 Windows GUI 通过 CLI 或稳定接口调用。

## 现有工作区

- 服务器已经存在权威开发工作区，必须原地使用；根目录只能由 `git rev-parse --show-toplevel` 发现。
- 只允许在当前 `main` 开发。不得创建分支、worktree、额外开发克隆，不得移动、替换或重新初始化仓库。
- 不要求删除历史分支或旧目录；只禁止本轮新增。发现历史遗留只记录，不执行破坏性清理。
- 任何已有 tracked/untracked 修改先记录来源和任务归属；不得自动 stash、clean、reset 或覆盖。
- 控制、证据、缓存和归档位置沿用仓库当前规范。若确实缺失，只能在现有仓库内部补最小必要目录。

## 架构与科学

- Phase1/2/3 独立运行，通过显式制品连接；任一阶段可单独执行、恢复和验收。
- CLI 是唯一命令入口；模块为独立库/DLL，通过稳定 C ABI/注册表接入调度器。
- 数据管道、IO、调度器、日志、资源监控是基础设施，算法模块不得复制这些能力。
- Phase3 是 HiPS 球面到平面 FITS 的多投影模块；新增公式必须先有 SCI/ALG 合同和参考依据。
- 科学定义 → 算法 → 数据合同 → API/函数 → 源码符号 → 单元/合成测试必须可机器追踪。

## 性能

- 当前发布只启用纯 CPU provider；ACR/GPU 保留为发布后优化线，构建默认关闭且运行不可达。
- generic、AVX2、AVX-512 等实现不得按型号硬编码；所有候选先过同一 Oracle，再由 benchmark 实测选择。
- 没有有效 benchmark 配置时使用通用 amd64 路径；配置必须绑定 CPU 特征、软件版本和校验哈希。
- heavy 任务必须观测真实工作线程 ID、每线程 CPU、工作单元数、负载分布、进度、内存和 IO。声明 workers/parallel 不是证据。
- 单线程、长期低利用率、内存持续无界增长或无进度均失败，不得加入豁免名单。

## 工程与文档

- 固定子 Agent 绑定模块；只读可并行，tracked 文件写入串行。
- 每个任务：冻结当前状态 → 最小修改 → 任务验收 → 全局快检 → 原子 commit → push main → 同 SHA CI。
- 使用最小充分错误处理；禁止为未复现的假设场景堆叠 fallback、吞错、宽泛 catch 或无界重试。
- L0 面向 Owner；L1–L3 面向 Agent/机器。源码注释只保留单位、合同、前后置条件、生命周期、并发和数值原因。
- `VERSION`、CMake、CLI `--version`、制品元数据和活动文档版本必须机器一致；历史归档不改写。

## 不得豁免

科学 Oracle 失败、ABI 破坏、ACR 进入生产、heavy 单线程/低利用率、资源泄漏、伪 PASS、版本漂移、追踪断裂和 Fatduck 原始数据外传均不得白名单化。
