# SCI-006 执行日志 (2026-08-28, vm-bj)

## 输入
03_TASK_DETAILS SCI-006 行; docs/science/{REJECTION,INTEGRATION}.md(T107/T108 冻结版); CONFIG_SCHEMA.md(WBPP 对齐记录)。

## 动作
1. 差距分析: 两份各缺 4 节 + claim ID 行规范化(INTEGRATION 逗号集合/REJECTION 范围式)。
2. 补四节(两份): 3a frame(像素栈域+frame_id 绑定+无跨像素状态); 9a 专属问题逐项(REJECTION: 7 方法统计假设/表驱动阈值冻结锚/auto 路由以 nominal n 唯一判定=可判定性/small-N percentile/identity 全程保持; INTEGRATION: 权重来源链/归一 wsum/support=max canonical/mask=eligibility+状态码/identity 可追溯); 14 文献(Rosner 1983 ESD 文章级+Hoaglin winsorization 书籍级+WBPP 源码采纳声明+RCR 软件参考——全部显式定位级别); 15 Acceptance+SYN-006 转换映射。
3. 关键合规点: 自动选择规则=表驱动确定性路由(nominal n),满足"可判定,禁止效果好定义";阈值全部有冻结锚点。

## 验证
- SCIENCE_CONTRACT_LINT_PASS files=2 sections=15。
- 全量回归 unittest 19/19 OK。

## 产物
docs/science/{REJECTION,INTEGRATION}.md(各补四节+ID 规范化); 本日志。

## PASS 判定
每种 rejection 统计假设/阈值/small-N/frame identity 逐项有锚点回答; 自动选择规则可判定; integration 权重/归一/mask 冻结; outlier oracle 可生成(SYN-006)。SCI-006 = PASS。
