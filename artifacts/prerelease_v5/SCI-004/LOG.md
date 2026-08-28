# SCI-004 执行日志 (2026-08-28, vm-bj)

## 输入
03_TASK_DETAILS SCI-004 行; docs/science/DRIZZLE.md(T105 冻结版 133 行); DATA_SEMANTICS; UNCERTAINTY_AND_COVARIANCE.md(引用存在性由 lint S3 核验)。

## 动作
1. 差距分析: 缺 4 节 + claim ID 行 ",014,015,016" 逗号列表不匹配规范 → 规范化为 "> ID: SCI-DRZ-001 集合: SCI-DRZ-001,014,015,016"。
2. 文献核验: Fruchter & Hook 2002 PASP 114 144(bibcode 2002PASP..114..144F, 经 MultiDrizzle/DrizzlePac Handbook 与多篇独立文献引用交叉核对); 初稿曾猜测 arXiv 号已自行撤下(不确定不写); pixfrac 实践语义引 DrizzlePac Handbook(节级); HEALPix Górski 2005 ApJ 622 759(文章级, 逐式核验留 SCI-P3/ALG-007)。
3. 补四节: 3a frame(ICRS+NESTED nside=2^order+球面立体角等价); 9a 专属问题 6 项(footprint/pixfrac 有效域 (0,1] 显式拒/flux vs brightness/support=D_p 归一/variance 传播+协方差非目标/极区与非法值边界); 14 文献; 15 Acceptance+SYN-004 转换映射(常数/点源/梯度/旋转/亚像素 shift/pixfrac 扫描/tile boundary)。

## 验证
- SCIENCE_CONTRACT_LINT_PASS files=1 sections=15。
- 全量回归 unittest 19/19 OK; VERSION_CONSISTENCY_PASS。

## 产物
docs/science/DRIZZLE.md(补四节+ID 规范化); 本日志。

## PASS 判定
输出单位冻结(ADU/px² 面亮度); flux/support 不变量与方差传播合同齐(§5/§9a); 复杂度与边界明确; SYN-004 可转换。SCI-004 = PASS。
