# 包取证摘要：AstroCS_Authoritative_Development_Pack_2026-07-30_v2.0

身份由脚本归一（去复原前缀与 .zip 后缀）。共 1 个实例。

## 【zip_recovered_from_history】history_zip/AstroCS_Authoritative_Development_Pack_2026-07-30_v2.0.zip
- 文件 74 | 94.6 KB
- sha256: 3e10847e11de079ff36512512934e7ddc53f74523c0f0ab2e328cbed5196434a
- git 事件: {'first': None, 'last': None, 'dels': None, 'adds_n': 0, 'mod_n': None, 'touch_last': 'b9d20c5020316b20adc15caaf2f6472f9e9d504e'}
- zip 顶层: AstroCS_Authoritative_Development_Pack_v2.0
- zip 内 marker: AUTONOMOUS_ENTRY.md, START_PROMPT.txt
- 00_READ_FIRST 开头:

# AstroCS 权威开发包自主入口

## 1. 入口地位

本文件是 Agent 的执行入口；本包根目录 `README.md` 是项目唯一权威总文档。任何历史 Spec、Task、报告、源码注释与 README 冲突时，以 README 为准。

## 2. 首次执行

1. 定位 AstroCS 仓库根目录，确认存在 `.git`、`lib/` 或现有项目文件。
2. 不修改用户原始 TestData、Gaia 数据、历史 HISS/HCSD 和审计证据。
3. 将仓库现有 `README.md` 备份到 `docs/archive/README_pre_authoritative_<UTC>.md`。
4. 将本包 `README.md` 安装为仓库根目录 `README.md`。
5. 将本包的 `engineering_authoritative/` 内容复制到仓库根目录同名目录；本包内对应内容即 `agent/ docs/ control/ tasks/ checklists/ contracts/ templates/ tools/ migration/ evidence/`。
6. 运行 `python tools/validate_pack.py --pack-root <本包目录>`；所有外部进程必须有超时。
7. 运行 `python tools/install_and_migrate.py --repo <仓库根目录> --pack <本包目录>`；脚本仅备份、安装和生成迁移报告，不删除用户数据。
8. 阅读顺序：`README.md` → `migration/CURRENT_STATE_AND_SCOPE_MIGRATION.md` → `control/PROJECT_STATE.yaml` → 当前 Gate 和 Task。

## 3. 核心执行原则

- 不继续机械执行旧 v1.2/v1.3 的 50 项任务；先迁移有效成果和未完成范围。
- 已确认成果不得重做：PlateSolve 单次内部检测共享、709/710 A/B、基础测光匹配修复、SNR写入HISS等。
- 旧 281 份 HISS 只作为调试资产，不作为最终全量回归证据。
- 在 Gate A–H 完成前禁止启动 710 帧最终全量回归。
- Stage2 禁止混合不同规范滤镜，禁止乘性梯度，禁止选单一参考帧替代全局共识曲面。
- 浏览器是科学检查工具；CLI模拟循环不得作为真实GUI性能证据。
- 普通任务只保留简短状态、测试日志和提交；每个 Gate 只写一份合并验收报告。
- Python、外部进程、网络与可能阻塞操作必须设置明确超时。

## 4. 执行顺序

按 Gate A → I 执行。仅在依赖无冲突时并行：

- Gate A 数据与校

