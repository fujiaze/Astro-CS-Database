# AstroCS v1.2 自治执行入口

你是本仓库的长期工程 Agent。立即在当前仓库执行，不要只给计划，不要要求用户手工解压、运行脚本、选择任务或逐项确认。

## 一、安装与恢复

1. 将当前目录视为候选仓库根目录，先核对 `.git`、工作树、当前分支、远端、现有 `engineering/` 与用户未提交修改。
2. 查找最新且结构有效的 `AstroCS_Next_Phase_Development_Pack*.zip`，解压到临时目录；找到其中本入口后，将整包保留为仓库根目录下 `engineering_v1.2/`。
3. 若 `engineering_v1.2/` 已存在，进入恢复模式：不得覆盖 `control/PROJECT_STATE.yaml`、`control/CURRENT_TASK.md`、任务状态、既有 evidence 或用户修改。只补充缺失的只读文档。
4. 运行 `python engineering_v1.2/tools/validate_pack.py engineering_v1.2`。该工具不调用外部进程。
5. 阅读顺序：
   - `README.md`
   - `docs/00_PHASE_GOAL_AND_BOUNDARIES.md`
   - `docs/01_V11_AUDIT_CORRECTION.md`
   - `agent/MASTER_AGENT_INSTRUCTIONS.md`
   - `agent/PARALLELIZATION_AND_DEPENDENCY_RULES.md`
   - `control/BASELINE_FACTS.md`
   - `control/PROJECT_STATE.yaml`
   - `control/CURRENT_TASK.md`
   - 当前任务及其引用文档。

## 二、状态原则

- 原 `engineering/` 的 G0–G8 和证据保留，禁止伪造或删除；本阶段新增 G9–G16。
- 若审计发现旧结论表述过度，只在本包 `control/DECISION_REGISTER.md` 和新证据中纠正，不改写原始证据。
- 当前第一个任务是 P09-001。完成一个任务并通过隔离复核后，自动选择下一个依赖已满足的任务。
- 不得因为一个任务结束而停下；只有全部完成或真实硬阻塞才汇报。

## 三、执行循环

每项任务必须依次完成：入口核验 → 原始基线/失败复现 → 最小修改 → 分层测试 → 真实数据验证 → 证据归档 → 独立复核 → 状态更新 → 自动进入下一任务。

所有可能阻塞的 Python 外部进程、网络访问、长时间构建和硬件等待必须设置明确超时；超时后保留部分日志并失败退出。

## 四、科学与工程红线

- 不得用旧路径 A/B 一致替代“标准 WCS 可回投真实星点”的闭环验证。
- 不得在 Photometric 内部偷偷翻转 Y 来掩盖错误的 WCS 生产端；最终修复必须统一服务于 Photometric、SNR、Drizzle 和浏览器。
- 不得把 `F_syn` 有效等同于测光成功；必须证明空间匹配、唯一配对、稳健拟合与残差统计均有效。
- 不得把同一 HISS 的副本或同一面板的近重复帧当成大尺度马赛克验证。
- 不得把 `has_snr=false` 的真实数据等权叠加称为 SNR² 加权验证。
- 不得通过降低分辨率、转 uint8、关闭 STF 或减少显示区域来宣称浏览器性能通过。
- 不得让浏览器 I/O、解压、LOD 构建在 GUI/渲染线程同步执行。
- 不得修改 HCSD 格式，除非 P16-006 的 ADR 证明无格式改造无法满足性能门；格式改造必须向后兼容。

## 五、用户可见最终结果

完成后必须交付：

1. T1–T4 设备、滤镜、校准帧和 Light→Master 映射；
2. WCS 闭环与 PlateSolve 全量无回归证据；
3. 修正后的测光、SNR 与真实 HISS；
4. 银心三片 32 帧的无梯度/有梯度 HCSD、指标和接缝对比；
5. 优化后的球面浏览器可执行文件或构建产物；
6. 同一视角、同一 STF 的全景与接缝截图；
7. 浏览器性能基线与优化后对比；
8. 完整测试入口、复核报告和交接说明。
