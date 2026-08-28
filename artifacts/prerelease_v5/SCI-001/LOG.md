# SCI-001 执行日志 (2026-08-28, vm-bj)

## 输入
03_TASK_DETAILS SCI-001 行(专属问题+统一写法+science_contract_lint); docs/science/CALIBRATION.md(T100 冻结版 109 行); DATA_SEMANTICS/NOISE_MODEL/GLOSSARY。

## 动作
1. 现状盘点: CALIBRATION.md 已有 目的/符号/单位/有效域/连续定义/假设/不变量/极端/精度/不可接受/Oracle/ALG/追溯;缺 Frames/专属问题回答/Primary literature/Acceptance。
2. 文献核验(web_search 实证): Newberry 1991 PASP 103 122 DOI 10.1086/132801(IOP 页确认); Janesick 2001 SPIE PM83 ISBN 0-8194-3698-4 Ch.2 photon transfer(经二次引用核对); HST ACS Data Handbook §4.4(URL 即节定位)。**不引用未核验的具体公式号**,§5 连续定义声明为 Project-defined derivation。
3. CALIBRATION.md 扩展: §3a 坐标 frame(逐像素无变换,frame_id 沿用); §9a 专属问题逐项回答(pedestal 不加/gain 不建模/read noise 归 NOISE_MODEL/负值保留/FP32 饱和语义/bad_mask 极性/方差不传播); §14 Primary literature(3 条含定位与限制声明); §15 Acceptance(Oracle+四不变量+glossary+lint+SYN-001 转换映射)。修正 §13 引用路径 lib/calibration/cpp/ → src/(实测)。
4. 新建 tools/science_contract_lint.py: S1 15 节齐备/S2 claim ID `SCI-<DOM>-NNN` 唯一/S3 文内 lib|docs 路径存在且行号不越界。
5. 测试 tests/sciencelint/ 4 用例 mutation: 缺 Acceptance/非法 claim ID/锚点文件缺失/真实合同对照。

## 验证
- SCIENCE_CONTRACT_LINT_PASS files=1 sections=15。
- mutation 4/4 按预期 FAIL; 全量回归 unittest **19/19 OK**; GLOSSARY_PASS(18/18)。
- 发现并修复: §13 cosmetic_corrector.cpp 路径错误(cpp/→src/)。

## 产物
docs/science/CALIBRATION.md(扩展 4 节+路径修正); tools/science_contract_lint.py; tests/sciencelint/; 本日志。

## PASS 判定
每个校准量单位唯一(§3+GLOSSARY); 专属问题逐项有锚点回答; 解析不变量→SYN-001 转换已登记(§15); 文献定位经核验。SCI-001 = PASS。
