# 07｜CI 机器合同

## 唯一检查注册表

所有 CI 检查定义在仓库 `ci/checks.json`，字段必须与本包 `ci/checks.schema.json` 一致。Workflow 只能调用 `python3 ci/run.py --profile ...`，不得在 YAML 中复制业务命令。

每项检查明确：ID、profile、命令、输入、输出、超时、是否修改工作区、是否 heavy、适用平台、不可豁免级别。未登记脚本不能成为发布门禁。

## 结果

每次 CI 生成：

```text
artifacts/ci/<sha>/<run-id>/
├── CI_RESULT.json
├── checks/<check-id>.json
├── logs/<check-id>.log.zst
├── resource/<check-id>.csv
├── junit/*.xml
├── coverage/summary.json
└── SOURCE_MANIFEST.json
```

`CI_RESULT.json` 由检查结果计算，禁止手填 PASS。失败日志可截断展示，但原始压缩日志保留 14 天。

## 工作区纯净性

- mutates_workspace=false 的检查执行前后比较 `git status --porcelain=v1 --untracked-files=all`；有新增或修改即失败。
- 生成器只允许写入显式 output 目录；需要更新仓库生成文件时单列写任务，不在 CI 中偷偷提交。
- CI checkout 必须是触发 SHA，不得自动切换到更新 main。

## 已有失败基线

首次接入允许生成 `ci/known_failures.json`，但只接纳已经在现有工作区接管起点复现的非关键工程项，并必须含 owner、reproducer、source_sha、expiry、理由。

以下永不允许豁免：SCI/ALG Oracle、ABI、生产路由、ACR dormant、heavy 单线程/低利用率、泄漏、崩溃、数据损坏、版本一致性、追踪断裂、安全凭据。

CI 比较当前失败集合与基线：新增、计数增加、过期、输出签名变化均失败。修复后必须删除对应基线项，不能重新增加。

## Coverage 与复杂度

- 第一次 deep CI 只测量并记录基线，不用虚构阈值。
- 第二个原子任务冻结每模块最低 coverage 和复杂度上限；以后阈值只能提高/收紧。
- 新增或修改行必须有相关测试；科学核心修改必须有合成 Oracle。

## 资源与性能

- heavy 检查只能经统一监控包装器运行。
- CPU 利用率按 cgroup/affinity 的有效逻辑核归一化，不按主机总核数误判。
- 低利用率判定必须排除明确标记的 IO 等待区间；重计算区间不允许长期单线程。
- benchmark 输出路径选择、样本量、热身、重复数、中位数、波动和数值误差；禁止只凭一次最快结果选 ISA。
