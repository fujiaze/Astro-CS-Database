# ALG-005 执行日志 (2026-08-28, vm-bj)

## 输入
03_TASK_DETAILS ALG-005 行(UPM 稀疏求解与接缝算法, SCI-005); docs/algorithms/{UPM_SOLVER,PHASE2_SAMPLER}.md(T206 DERIVED)。

## 动作
1. UPM_SOLVER.md(ALG-UPM-001..003): 删 GPU 块级求值→CPU-only(IRLS 串行=确定性 reference, 块级 C[frame×control] 可 affinity worker pool, 禁硬编码线程数); 补 5c(残差/Huber 逐观测 SIMD 安全+法方程观测索引固定序归约禁重结合+取消点=IRLS 迭代间, 整模型原子性: 不写 Model/persist/hash); ID 规范化+V5 重验戳。
2. PHASE2_SAMPLER.md: claim ID 格式修复 ALG-P2-SAMPLE-* → ALG-P2SAMPLE-001..N(连字符违规); 并行模型节 V5 重验(默认串行=确定性 reference+hotfix 历史说明, 实验并行按 affinity 调度无硬编码)。该文档为接口/设计混合结构(输入/输出/Pre/Post), 不套 ALG 10 节 lint 模板, 以内容修复+LOG 声明处理。
3. 状态机: csv.writer+lineterminator="\n" 置 IN_PROGRESS→PASS(LF 行尾, 无 CRLF 回归)。

## 验证
- SCIENCE_CONTRACT_LINT_PASS kind=alg files=1 sections=10。
- 全量回归 unittest 21/21 OK。

## 产物
docs/algorithms/UPM_SOLVER.md; docs/algorithms/PHASE2_SAMPLER.md; 本日志。

## PASS 判定
稀疏求解(Huber IRLS+稀疏 json persist+dense cache 1e-12 等价)与接缝算法(gauge=min frame_id+harmonic continuation)从 SCI-UPM §5 推导; SIMD 安全+取消点+CPU-only 无硬编码冻结; 接缝指标映射 SYN-005(预冻结)。ALG-005 = PASS。
