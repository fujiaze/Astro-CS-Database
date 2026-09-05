# 10｜公开仓库与 Fatduck 安全边界

GitHub-hosted Linux/Windows 可运行仓库源码；Fatduck 是持久化本机且有受限数据，必须使用不同策略。

## Fatduck 硬规则

- repository-level runner，唯一自定义 label：`fatduck-realdata`；禁止默认 label 匹配普通 job。
- 专用低权限 Windows 用户和 ACL；不可访问个人目录、浏览器、SSH key、Git 凭据。
- workflow 仅由成功的 main Windows CI 或 schedule 触发；不提供公开手动触发，禁止 PR、fork、issue 事件。
- Fatduck job 不使用 `actions/checkout`，不运行仓库源码/脚本，不安装软件。
- 只下载 GitHub Windows build 生成的候选二进制；先校验 provenance 与 SHA256。
- 只调用 `D:\AstroCSRunner\harness\run-validation.cmd`；harness 哈希与版本由本地和仓库双重锁定。
- `GITHUB_TOKEN` 最小只读；上传 artifact 使用平台令牌，通知/Issue 写入在后续 GitHub-hosted job 完成。
- testdata 目录只读；公开输出目录与完整输出目录分离。

## Workflow 保护

- 三个 workflow 中的第三方 action 均锁完整 40 位 commit SHA；tag 仅写注释。
- `.github/workflows/**`、`ci/publish_policy.json`、Fatduck harness lock 的任何变化触发专门 CI 与独立只读复审。
- workflow 权限默认 `contents: read`；只有 GitHub-hosted notify job 单独获得 `issues: write`。
- 自托管 job 的 YAML 只包含下载、哈希校验、固定 harness 调用、白名单校验、上传公开目录；机器检查拒绝其他 `run/uses`。
- runner 自动更新；若版本过旧导致不接单，状态检查输出更新提示，不绕过。

## 队列

self-hosted job 离线最多排队 24 小时。schedule + concurrency 保留最新候选、替换旧 pending；运行中的验证不被新候选中止。完成后若 main 已前进，由下一轮验证最新 SHA。
