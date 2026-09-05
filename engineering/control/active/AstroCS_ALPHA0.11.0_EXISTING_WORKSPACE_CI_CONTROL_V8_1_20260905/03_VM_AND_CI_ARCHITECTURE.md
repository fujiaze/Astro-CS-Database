# 03｜现有工作区、GitHub CI 与 Fatduck 架构

## Linux 服务器：原地工作

不规定绝对目录，不创建第二份仓库。运行时定义：

```bash
ASTROCS_ROOT="$(git rev-parse --show-toplevel)"
cd "$ASTROCS_ROOT"
```

项目代码、活动文档、CI 配置和任务状态均以该现有根目录为基础。控制包放入项目既有控制目录；若尚无控制目录，仅在仓库内建立一个最小、明确的目录并写入索引。不得移动现有源码、控制文件或历史资料。

Linux 服务器职责：Agent 开发、静态检查、文档/合同追踪、轻量编译、短合成测试和 CI 配置。它不是公共仓库 self-hosted runner，也不承担默认真实数据全量重算。

## GitHub 托管 CI

| Workflow | Runner | 触发 | 工作 |
|---|---|---|---|
| `linux-ci.yml` | `ubuntu-24.04` | main push | GCC/Clang、合同/文档、单元、合成 Oracle、分层 sanitizer/coverage |
| `windows-ci.yml` | `windows-2022` | main push | MSVC v143、DLL/CLI、合成 Oracle、候选程序打包 |
| `fatduck-realdata.yml` | GitHub prepare + Fatduck | Windows CI 成功、定时 | 选择最新可信候选，Fatduck 本地真实数据终验 |

GitHub-hosted runner 的临时 checkout 仅属于 CI 沙箱，不是开发工作副本且不得 push。

## Fatduck 受限 runner

Fatduck 保持本机固定目录和固定 harness；具体路径由安装时配置，不写死到跨平台项目代码。它：

- 不 checkout 源码、不编译、不运行仓库脚本；
- 只下载并校验 CI 生成的 exe/DLL；
- 用只读本地 testdata 运行 Phase1/2/3 独立测试；
- 完整 FITS/HiPS/日志留本机；
- 只上传白名单 JPG、JSON/CSV 指标和脱敏日志；
- 使用低权限账户且没有 Git push 凭据。

## 自动闭环

1. 前台从现有工作区机器台账选择 READY 任务。
2. 固定子 Agent完成任务；tracked 文件写入串行。
3. 前台验收 diff 和测试，原子 commit/push main。
4. GitHub Ubuntu/Windows 在同一 SHA 自动验证。
5. Windows CI 产出带 SHA256、工具链和 provenance 的候选程序。
6. Fatduck 在线时取最新可信候选运行本地真实数据；离线只保留/刷新待执行状态，不阻塞其他工作。
7. Owner 查看 JPG 或本地结果并作最终发布裁定。
