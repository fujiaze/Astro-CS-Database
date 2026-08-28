# ALG-006 执行日志 (2026-08-28, vm-bj)

## 输入
03_TASK_DETAILS ALG-006 行(Rejection Integration 离散算法, SCI-006); docs/algorithms/{REJECTION_ALGORITHMS,INTEGRATION_ALGORITHMS}.md(T207/T208 DERIVED)。

## 动作
1. REJECTION_ALGORITHMS.md(ALG-REJ-001..008): 删 GPU per-pixel 切分→CPU-only 行带 worker pool(禁硬编码线程数, 7 方法与线程划分无关); 补 5c(排序 value tie-break frame_id 固定序+ESD/RCR 固定序统计+阈值比较 SIMD 安全+取消=行带粒度掩膜帧原子)。
2. INTEGRATION_ALGORITHMS.md(ALG-INT-001..003): 删 GPU vs/wsum 归约→CPU-only; 5c(像素栈内固定候选序 FP64 归约, 并行仅在像素间; support=max 选择非归约; 取消=行带 P2PixelResult 原子)。
3. 两份 ID 规范化+V5 重验戳; 状态机 csv.writer LF 合规。

## 验证
- SCIENCE_CONTRACT_LINT_PASS kind=alg files=2 sections=10。
- 全量回归 unittest 21/21 OK。

## 产物
docs/algorithms/{REJECTION_ALGORITHMS,INTEGRATION_ALGORITHMS}.md; 本日志。

## PASS 判定
7 方法+auto 路由与聚合状态码从 SCI-REJ/SCI-INT §5 推导(表驱动锚点保留); SIMD 安全+取消点+CPU-only 无硬编码; reject set/identity 可解析验证(SYN-006)。ALG-006 = PASS。
