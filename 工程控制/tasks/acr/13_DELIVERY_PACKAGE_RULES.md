# 严格交付包规则

## 1. 四个逻辑交付部分

- Control Package：本权威控制包；
- Complete Source Snapshot：目标commit完整源码，不只diff；
- Evidence Package：完整构建、测试、画像、资源、GPU/Mixed、Sanitizer和失败日志；
- Merge Report：base/feature/merge commit、冲突、回归和dormant状态。

## 2. 单一干净HEAD

Evidence必须从目标实现HEAD的干净worktree一次生成。以下值必须完全相同：

- `git rev-parse HEAD`；
- manifest.result_commit；
- git_log首条commit；
- source snapshot记录的HEAD；
- diff目标HEAD；
- test summary的HEAD。

Evidence生成物不得再提交到实现分支后引用旧HEAD。推荐使用临时干净worktree或外部输出目录。

## 3. 完整命令日志

每个测试至少保存：

- command；
- working_directory；
- environment/toolchain；
- start/end time；
- timeout；
- exit_code；
- stdout/stderr完整原文；
- PASS/FAIL/SKIPPED和原因。

一行“PASSED N tests”只能做索引，不能代替原始日志。

## 4. Path guard

必须附：

- base与result commit；
-允许路径列表；
- 禁止路径列表；
- 实际changed paths；
- 完整命令与退出码；
- `PASS`。任何 `ABORT`、非零退出码或算法路径变更均阻断合并。

## 5. 实际执行证据

必须包含：

- predicted与actual设备分别记录；
- per-device claimed/done/failed；
- backend原始日志；
- coverage；
- 数据传输；
- profile hash before/after；
- 利用率控制动作；
- RAM/VRAM动作。

不能用推荐字符串证明GPU实际执行。

## 6. Manifest与哈希

每个ZIP提供 `package_manifest.json` 和 `SHA256SUMS.txt`，生成后重新解压、CRC与SHA-256校验。

## 7. 命名

未发布项目只使用稳定名称：

```text
AstroCS_ACR_Control_Package.zip
```

后续覆盖更新，不并列V1/V2或日期包。
