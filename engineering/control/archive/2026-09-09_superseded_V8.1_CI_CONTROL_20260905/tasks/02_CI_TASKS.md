# CI 任务逐项清单

## V8-CI-001｜真实检查资产盘点

扫描 CTest、Python tests、`tools/quality`、contracts、monitoring、sanitizer 和 graph scripts。逐项登记真实命令、必需参数、输入、输出、timeout、平台、是否写工作区、是否 heavy。禁止简单循环执行全部脚本。

输出：`ci/checks.json`、`ci/impact_map.json`、`ci/INVENTORY_REPORT.md`。未知参数、假 PASS 或生成副作用必须修正或明确 prerequisite。

## V8-CI-002｜统一执行器

实现 `ci/run.py`：按 profile/check/focus 选择检查，执行 timeout，捕获 exit code，验证输出，生成 per-check JSON 和总结果。CI YAML 只调用它，不复制业务命令。

单元测试必须覆盖：成功、非零退出、timeout、信号终止、缺输出、schema 错、脏工作区、重复 ID、未登记命令、伪造 PASS。

## V8-CI-003｜资源探测与统一监控

读取 affinity/cpuset/cgroup quota、逻辑/物理核和可用内存，动态计算并发；不得硬编码核数。heavy wrapper 记录真实进程/线程 CPU、RSS/PSS、IO、threads、progress。用 busy-loop、sleep、memory-growth 负向样例证明低利用率和泄漏会失败。

## V8-CI-004｜本地 fast

建立 changed path → checks 映射。必含：版本、schema、合同索引、SCI→TEST 追踪、ACR dormant、生产可达性、serial-heavy、陈旧版本注释、相关单测。目标 60 秒；超时报告最慢项，不静默删除。

## V8-CI-005｜GitHub Linux

- `ubuntu-24.04`。
- main push：Release GCC configure/build、轻量单测、全合成 Oracle、文档/合同/调用图检查。
- manual/schedule deep：Clang、ASan+UBSan、目标 TSan、coverage、complexity。
- heavy 子项必须使用监控；不下载 Fatduck 数据，不比较历史版本。

## V8-CI-006｜GitHub Windows

- `windows-2022`、VS 2022 v143、Win10 minimum target、ACR OFF。
- configure/build/test/install/package。
- 验证所有 DLL 名称、导出 ABI、加载、注册、CLI 入口和合成 Oracle。
- candidate 内含 `BUILD_PROVENANCE.json`、`SOURCE_MANIFEST.json`、`SHA256SUMS`；不含源码、测试数据、build cache。

## V8-CI-007｜Workflow 与 action 锁

安装 Linux/Windows workflow。触发仅 main push、manual、可选 schedule；所有 action 锁完整 commit SHA；workflow permissions 最小；YAML 不含算法命令。action lock 由在线查询官方 tag 生成并机器复验，模板占位符未替换不得启用。

## V8-CI-008｜Fatduck workflow

三个 job：

1. GitHub-hosted `select-candidate`：选择最新 main、Linux/Windows 均 PASS 的候选，输出 SHA/run/artifact digest。
2. `fatduck-validate`：只下载、校验、调用固定本地 harness、校验 publish 白名单、上传公开目录。
3. GitHub-hosted `notify-owner`：下载公开结果，更新单一 Owner Review Issue；只有此 job 有 `issues: write`。

触发仅为 Windows CI 成功和每 6 小时 schedule，不开放手动触发。concurrency 只保留最新 pending，不取消 running。Fatduck job 中禁止 checkout、安装、任意 repo script、任意网络上传。

## V8-CI-009｜负向测试

至少注入并确认拒绝：假 PASS JSON、非零退出、timeout、dirty workspace、未登记脚本、PR/fork 触发 Fatduck、tag action、hardcoded core、heavy 无 monitor、版本漂移、上传 FITS/headers/绝对路径、Fatduck checkout、Issue 写 token 落到 Fatduck。

## V8-CI-011｜GitHub 仓库设置

使用 GitHub API/CLI 配置并现场读取确认：Actions 启用；默认 token 只读；允许的 action 策略与 `actions.lock.json` 一致；artifact retention 14 天；Issues 可用于单一 Owner Review；Fatduck runner 为 repository-level 且只有自定义 label；公开 fork/PR workflow 不得匹配 Fatduck。凭据只从既有环境读取，不写日志或文件。缺管理权限时只报告一组准确设置项，不阻塞源码/CI实现。

## V8-CI-010｜双平台实测

V8-CI-001..009 各自已经按任务原子 commit/push。V8-CI-010 不再制造汇总提交，只记录最新 main 完整 SHA并等待该 SHA 的 Linux/Windows workflows。核验 check 数量、artifact digest、source manifest、resource summary 和 candidate 内容。失败创建归属明确的修复提交，不 amend。

## V8-CI-012｜首次 deep 基线

对 V8-CI-010 的同一 SHA 手动触发一次 `linux-deep`，实际执行 Clang、ASan+UBSan、目标 TSan、coverage、complexity 和完整合成矩阵。首次 coverage/complexity 只形成机器测量基线；sanitizer/科学失败必须修复。保存最慢检查和资源摘要，确认单个 job 小于 GitHub 托管 runner 时限。

## V8-CIQA-001｜独立反审

只读 Agent 重新运行负向样例，并审查：workflow 权限/触发/action 锁、结果可伪造性、workspace 副作用、known-failure、Fatduck 数据泄漏、低利用率检测。P0/P1 自动创建修复任务，修复后复审，不等 Owner。
