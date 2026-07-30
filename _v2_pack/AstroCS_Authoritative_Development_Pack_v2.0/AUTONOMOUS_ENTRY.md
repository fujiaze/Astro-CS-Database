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

- Gate A 数据与校准整理；
- Gate B Stage1代表帧；
- Gate C HISS正式契约与压缩；
- Gate D 多帧Stage1一致性；
- Gate E 全局加性梯度和合成注入；
- Gate F 银心三片最小闭环；
- Gate G 银心32帧正式叠加；
- Gate H 资源感知编排器与压力验证；
- Gate I 格式冻结、710帧回归和发布准备。

## 5. 汇报条件

Agent 持续执行，不因普通测试失败停止。仅在以下情况向用户汇报：

- 缺少无法从仓库、TestData说明或配置解析的关键输入；
- 需要用户选择会改变 README 冻结原则的方案；
- 发现数据损坏、许可风险或可能造成不可逆数据丢失；
- 全部 Gate 完成并已生成最终审计 ZIP。

硬阻塞必须使用 `templates/BLOCKED_REPORT.md`，列出已尝试方案、证据、最小用户决策。

## 6. 最终交付

完成后生成一个 ZIP，至少包含：

- 更新后的根 README；
- 源码提交范围与 Git 状态；
- Gate A–I 合并报告；
- HISS/HCSD 格式契约；
- T1–T4目录和校准映射；
- 代表 HISS、银心三帧与32帧结果清单；
- 梯度关闭/开启与调试层结果；
- 编排器资源曲线；
- 真实浏览器截图/录屏索引与性能数据；
- 710帧最终回归摘要（仅Gate I）；
- 未解决风险和用户手动验收所需文件。
