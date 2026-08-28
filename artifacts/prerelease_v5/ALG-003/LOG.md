# ALG-003 执行日志 (2026-08-28, vm-bj)

## 输入
03_TASK_DETAILS ALG-003 行(Noise SNR 离散算法与归约误差, SCI-003); docs/algorithms/NOISE_ESTIMATION.md(T204 DERIVED); ALG-001/002 V5 合规模式。

## 动作
1. V5 合规修复: 删 GPU patch 切分→CPU-only(patch 级 worker pool 按 affinity, 禁硬编码线程数); 平面 LS 3 参数=全控制点固定序归约(FP64 禁重结合)。
2. 补 5c SIMD 安全与取消点: patch 内 MAD/median 固定输入序选择; fill 逐像素 SIMD 安全(连续无别名); g_model_floor 指针 key 注册表单线程资源锁自由; 取消=行带粒度, 半成品模型作废(_free 语义), fill 行带不回滚整帧重做。
3. ID 规范化: "> ID: ALG-NOISE-001 范围: ALG-NOISE-001..003"+V5 重验戳。

## 验证
- SCIENCE_CONTRACT_LINT_PASS kind=alg files=1 sections=10。
- 全量回归 unittest 21/21 OK。

## 产物
docs/algorithms/NOISE_ESTIMATION.md; 本日志。

## PASS 判定
离散公式从 SCI-NOISE §5 推导(既有 12 节含数据布局/误差预算保留); 归约误差=固定序+禁重结合; 复杂度 O(pixels)/LS O(64); 并行性 patch 级无硬编码; SIMD 安全+取消点冻结。ALG-003 = PASS。
