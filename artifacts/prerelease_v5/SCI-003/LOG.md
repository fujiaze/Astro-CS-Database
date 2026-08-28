# SCI-003 执行日志 (2026-08-28, vm-bj)

## 输入
03_TASK_DETAILS SCI-003 行; docs/science/NOISE_MODEL.md(T104 冻结版 118 行); DATA_SEMANTICS §4a; SCI-001/002 lint 机制。

## 动作
1. 差距分析: 缺 4 节 + claim ID 行格式(S2): 头行为范围式 "SCI-NOISE-001..015" 不匹配 lint 合同 ID 规范 → 规范化为 "> ID: SCI-NOISE-001 范围: SCI-NOISE-001..015 (legacy SNR-001..015)"。
2. 文献核验: Newberry 1991(复用 SCI-001 已核验条目, 文章级); MAD→σ=1/Φ⁻¹(3/4) 恒等式(教科书级, Project-defined); Tukey 内点集复用 SCI-PHOT 链。均声明定位级别。
3. 补四节: 3a frame(像素域 patch/平面场, 无 WCS); 9a 专属问题逐项(signal/noise/blank sky/SNR 消费侧定义/variance-ivar 零负 NaN 条件/Poisson+read noise 仅诊断/权重归一与适用域); 14 文献; 15 Acceptance+SYN-003 转换映射(Gaussian/Poisson/常量/blank sky/outlier/small-N + ivar=0 边界)。

## 验证
- SCIENCE_CONTRACT_LINT_PASS files=1 sections=15。
- 全量回归 unittest 19/19 OK。

## 产物
docs/science/NOISE_MODEL.md(补四节+ID 规范化); 本日志。

## PASS 判定
所有 weight 字段(pixel_weight=ivar)可指出数学定义(GLOSSARY+§5); 零/负/NaN 条件明确(§8/§9a); 权重归一与适用域冻结; 可生成 outlier oracle(§15 SYN-003 映射)。SCI-003 = PASS。
