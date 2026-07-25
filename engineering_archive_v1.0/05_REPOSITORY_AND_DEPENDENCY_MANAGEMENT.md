# 05 仓库与依赖管理规范

## 1. 目标

从任意全新目录，仅凭主仓库、公开/受控依赖清单和数据集清单，即可重建同一版本系统。

## 2. 主仓库策略

当前基线已是 monorepo。后续默认：

- 主分支 `main`；
- 每项任务分支：`task/<TASK_ID>-short-name`；
- 一项任务至少一个清晰提交；
- 合并前必须通过对应 Gate；
- 不再依赖“作者本机某目录刚好存在”的被忽略源码。

## 3. `healpix_drizzle` / `healpix_stack` 处理

P00 必须从以下方案选一个并写 ADR：

### 推荐方案 A：纳入 monorepo

把两个模块固定版本导入 `lib/healpix_db/`，保留历史或至少保留来源 commit。优点是构建、审计和发布最直接。

### 方案 B：Git submodule

主仓库记录 URL 与 commit，bootstrap 脚本执行 `git submodule update --init --recursive`。必须防止 submodule 浮动到分支头。

### 不接受方案

继续 `.gitignore`，要求开发者手工 clone 到固定目录但不记录 commit。

## 4. 第三方依赖清单

必须新增机器可读锁定文件，至少包含：

- MinGW64/GCC 版本；
- PowerShell 7 版本；
- Python 版本；
- Qt6 版本；
- GSL、OpenMP runtime；
- zstd、lz4；
- Python 包及版本；
- Gaia 数据版本与分块清单；
- 任何参考程序版本。

每项记录：名称、版本、来源、许可证、校验和、使用模块、安装方式。

## 5. 构建产物目录

统一输出：

```text
build/
  <toolchain-id>/
    bin/
    lib/
    obj/
    generated/
    test-results/
    manifest.json
```

DLL 名称、依赖关系和导出符号要写入 `manifest.json`。禁止多个模块把 DLL 随意写在源码目录。

## 6. 构建入口

允许保留各模块现有 Makefile，但仓库根必须提供一个入口，例如：

```powershell
pwsh -File .\tools\bootstrap.ps1
pwsh -File .\tools\build.ps1 -Preset dev-mingw64
pwsh -File .\tools\test.ps1 -Tier smoke
```

每个外部命令必须检查退出码；可能阻塞的 Python 子进程必须设置超时。

## 7. 版本策略

- 仓库版本：SemVer；
- 数据格式版本：独立版本，不与应用版本混用；
- C ABI：每个 DLL 提供版本查询；
- 发布 tag：`vMAJOR.MINOR.PATCH`；
- 开发基线 tag：`baseline-YYYYMMDD-N`；
- 重要数据集：单独数据集版本。

## 8. 依赖变更 Gate

新增/升级依赖必须回答：

- 为什么现有实现不能满足；
- 许可证是否兼容；
- 是否增加部署体积；
- 是否破坏离线能力；
- 是否有固定版本和校验和；
- 回退方式；
- 受影响的构建与测试。
