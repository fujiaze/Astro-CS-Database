# Git与交付

## 1. 分支

- 只使用`feature/astrocompute-runtime`；
- 不创建日期分支、V号分支或新仓库；
- 不修改Phase1与现有业务算法；
- 提交前运行path guard。

## 2. 建议提交序列

1. `acr: freeze focused mixed execution contracts`
2. `acr: add weighted integration cpu and openmp reference`
3. `acr: add resident cuda weighted integration launcher`
4. `acr: add weighted integration mixed benchmark and tests`
5. `acr: finalize pre-business evidence`

可合并提交，但每个提交必须可构建，不允许用Evidence提交制造第二HEAD。

## 3. Evidence必须在仓库外生成

Evidence记录同一最终源码HEAD：

- `git/head.txt`
- `git/status_porcelain.txt`为空
- `git/log.txt`
- `git/path_guard.txt`为PASS
- CPU/CUDA configure和build日志
- CTest完整日志
- weighted integration quick/standard JSON与终端日志
- OpenMP线程/编译器/CPU/GPU信息
- OperationProfile
- compute-sanitizer日志
- memory/transfer report
- schema验证
- manifest与UTF-8路径SHA256

## 4. 超时

所有PowerShell/Python封装外部构建、CTest、Benchmark或GPU工具时必须设置明确超时，并在超时后终止完整进程树。禁止无限等待。

建议：

- configure/build：每步600秒；
- quick CTest：整体600秒；
- standard Benchmark：900秒；
- full Benchmark：1800秒；
- compute-sanitizer单项600秒。

## 5. 交付包

审核包只包含必要内容：

- 控制包快照及SHA；
- ACR源码快照或相对base的patch/diff；
- weighted integration样例源码；
- Evidence与原始日志；
- manifest和SHA。

禁止打包build目录、依赖缓存、`.git`对象和重复旧Evidence。
