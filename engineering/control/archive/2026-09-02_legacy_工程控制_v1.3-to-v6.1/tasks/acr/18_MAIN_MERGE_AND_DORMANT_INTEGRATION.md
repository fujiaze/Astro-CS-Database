# 合并到 main 与备用集成规范

## 1. 会不会有问题

分支合并不是把两套程序机械叠加。Git 根据共同祖先合并文本改动：

- 两个分支修改不同文件，通常自动合并；
- 修改同一文件但不同区域，通常也能自动合并；
- 修改同一文件同一区域，产生 conflict，需要人工决定；
- 即使没有文本 conflict，也可能出现编译、链接、ABI、依赖或运行行为冲突。

本支线采用“只新增隔离底层、极少改公共构建文件、完全不改算法”的方式，能显著降低冲突，但不能保证零风险。

## 2. 最可能的冲突点

### 顶层 CMake

main 可能同时调整 target、选项、安装或依赖逻辑。缓解：ACR 只增加一个 `add_subdirectory` 和独立 options，业务 target 不直接链接 GPU backend。

### 依赖管理

main 可能已经使用不同版本的 oneTBB、fmt/spdlog、JSON 或测试框架。缓解：优先复用已有版本；所有依赖使用 namespaced CMake target；禁止在一个进程链接互不兼容的重复 ABI。

### 平台构建

开发机器有 CUDA，但普通 main 用户没有。缓解：CPU-only 默认可构建；GPU backend 明确 optional；配置时找不到 SDK 不应让整个项目失败，除非用户显式要求该 backend。

### 全局初始化

静态对象可能在程序启动时枚举 GPU、创建线程或输出警告。缓解：全部 lazy initialization；只有 `acr::*` API 或独立工具被调用时才初始化。

### 公共符号和宏

第三方头可能污染宏、编译选项或 C++ standard。缓解：target-local compile options；不使用全局 `add_definitions`/`include_directories`；公共头不包含巨大 backend 头。

### 安装和打包

新增 DLL/so 或插件可能未进入安装包。缓解：CPU core 和 backend plugin 分开列入 manifest；合并后执行干净安装测试。

## 3. 为什么合并后可以安全备用

满足以下设计时，ACR 合并到 main 但不被算法调用，等价于增加了一套未使用的库和测试工具：

- 现有可执行 target 不强制链接所有 GPU 插件；
- ACR core 不在静态初始化中启动线程或探测设备；
- 普通启动不创建 runtime singleton；
- 未标定警告只在 ACR 被实际调用时出现；
- 所有配置使用独立命名空间；
- 没有改变 PipelineFrame 和算法接口；
- CPU-only CI 覆盖无 GPU 环境。

此时合并的主要风险是“构建和依赖”，不是“算法结果变化”。

## 4. 推荐合并流程

1. feature 分支所有 Phase 完成；
2. `git fetch origin`；
3. 在 feature 中合并最新 `origin/main`；
4. 解决冲突并完整测试；
5. 生成 merge preview：`git diff origin/main...HEAD`；
6. path guard 确认算法目录零修改；
7. 推送 feature；
8. 切到 main 并 `git pull --ff-only`；
9. `git merge --no-ff feature/astrocompute-runtime`；
10. 合并后重新配置全新 build 目录；
11. 运行 CPU-only、现有主线测试、ACR classic tests 和普通启动无副作用测试；
12. 推送 main；
13. 输出 Merge Report。

## 5. 为什么建议 --no-ff

该底层支线包含多阶段依赖选择、API、后端和实验历史。`--no-ff` 会在 main 上保留一个明确 merge commit，便于：

- 看出 ACR 是一个独立支线；
- 回滚整个 merge；
- 审计阶段提交；
- 后续集成分支定位基础版本。

如果仓库强制 squash PR，则遵守仓库政策，但必须保留外部 Evidence 和 commit mapping。

## 6. 回滚

若合并后发现问题，优先：

```bash
git revert -m 1 <merge_commit>
```

不要重写已经推送的 main 历史。由于本支线不改算法，回滚应主要移除 ACR 新目录和构建入口。

## 7. 合并后冻结

合并完成后：

- ACR foundation 进入维护/备用状态；
- feature 分支不继续添加算法改造；
- 仅允许通过独立 bugfix 分支修复底层严重问题；
- 用户完成其他算法逻辑验证后，再从最新 main 创建集成分支；
- 每个算法集成单独验收和合并，避免一次大重构。

## 8. 合并是否会影响当前算法

只要 path guard、无副作用初始化、CPU-only 构建和主线回归全部通过，当前算法不会因为 ACR 代码存在于 main 而改变执行结果。真正开始影响算法，要等未来某个算法调用点明确改为 ACR API 后才会发生。

## 9. 单一实现线

合并前不得存在第二套ACR目录、版本分支或平行控制包。所有纠正均应体现在唯一feature分支及其最终merge commit中。
