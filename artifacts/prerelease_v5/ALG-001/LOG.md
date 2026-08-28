# ALG-001 执行日志 (2026-08-28, vm-bj)

## 输入
03_TASK_DETAILS ALG-001 行; docs/algorithms/CALIBRATION_ALGORITHMS.md(T200 DERIVED 版 89 行); SCI-CAL-001(V5 重验版); AGENTS.md 硬约束(禁止硬编码线程数/CPU-only)。

## 动作
1. V5 硬约束合规修复: 删除 `num_threads(16)` 硬编码→worker pool 按 affinity 调度,线程数无关确定性; combine=mean 归约顺序冻结为帧序升序串行(FP32)。
2. 补检查点: 5a variance/mask 传播边界(variance 不传播=SCI-CAL §9, NaN→NaN, bad_mask 仅 cosmetic); 5b SIMD 安全条件(无别名/行连续/禁 fast-math 重结合/标量参考恒在)+取消点(帧粒度/row-block 粒度, AC_ERR_CANCEL)。
3. §7 改 CPU-only 后端策略(删 GPU 扩展路径, ISA 变体经 benchmark 注册, 禁全局 -march)。
4. ID 规范化: "> ID: ALG-CAL-001 范围: ALG-CAL-001..004"; 推导来源声明=SCI-CAL §5 离散化(实现锚为一致性非推导依据)。
5. lint 扩展 --kind alg(10 节: 上游SCI/离散公式/伪代码/边界/确定性/SIMD/复杂度/Oracle/容差/关联), mutation 2 用例(硬编码线程词法级/GPU 词法级)入 tests/sciencelint(21/21)。

## 验证
- SCIENCE_CONTRACT_LINT_PASS kind=alg files=1 sections=10。
- 全量回归 unittest **21/21 OK**(新增 2 ALG mutation)。
- 期间发现并修复: SCI-007 文献行的 IVOA 节号(4.2.1/4.4.1/6.3.1)被版本一致性 checker 误判→EXEMPT 补外部标准关键词 "ivoa"(与 siril/wbpp 同类), VERSION_CONSISTENCY_PASS 复验。

## 产物
docs/algorithms/CALIBRATION_ALGORITHMS.md(V5 合规重验); tools/science_contract_lint.py(--kind alg); tools/check_version_consistency.py(EXEMPT+ivoa); tests/sciencelint(+2); 本日志。

## PASS 判定
从 SCI 方程推导(离散公式 F1-F4 一一对应连续定义); 误差模型(FP32 rtol 1e-6/atol 1e-7 预冻结来源=1e-7×floor 放大 10×)/复杂度 O(n)/SIMD 安全+取消点冻结; 并行性=affinity worker pool 无硬编码; AGENTS 硬约束全过。ALG-001 = PASS。
