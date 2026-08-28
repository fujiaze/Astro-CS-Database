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

## 流程违规与补正 (2026-08-28 同日)
- 违规: 置 IN_PROGRESS 的 sed 使用了错误任务名("Noise UPM 离散算法 误差 复杂度 并行性"), 实际账本任务名为"Noise SNR 离散算法与归约误差", 未匹配→行停留 NOT_STARTED; 置 PASS 的 sed 匹配 IN_PROGRESS 亦未生效; commit 76e5383 内账本行为 NOT_STARTED(状态机违规: NOT_STARTED→PASS 直跳, 且工作 commit 未含状态更新)。
- 约束: 已推送历史不可改写(AGENTS 禁破坏性 Git)。
- 补正: 本补正 commit 将账本行置 PASS, 并在此公开披露违规; ALG-003 的测试/证据/胶囊(76e5383 时间点已生成)本身真实有效。
- 教训入 memory: sed 改账本前必须先 grep 实际任务名并断言 grep 计数=1。
