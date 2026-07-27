# 当前任务：P07-001 性能与峰值内存基线

读取 `tasks/P07-001.md` 并执行。记录新路径性能，确认去重收益和无异常回退。

## 上一任务完成情况

- P06-003 HCSD 输出与独立读取: DONE (VERDICT: PASS)
  - 证据: evidence/P06-003/
  - 7/7 验证 PASS (17 个子测试全部 PASS)
  - 子叶索引 leaf_index 结构正确 (T1 78/49152 非空与 P00-003 baseline 一致, leaf_ipix 一致, sum(data_length)=n_pix)
  - metadata 必填字段齐全 (nside/nested/n_pix/has_snr + caller 元数据 filter/n_frames/sigma_clip/stack_stats)
  - 输入追溯有效 (n_frames=2=输入 HISS 数, n_pix=15522966=stage2 日志, mean_pixel_count=1.9850 一致)
  - inspect --hcsd 独立读取成功 (DLL 全加载 9/9, JSONL result+completed 输出)
  - 字节级结构符合契约 (Magic/JSON头/leaf_index/sorted_ipix 升序/文件大小 全部验证)
  - 按子叶读取 aio_hcsd_read_leaf 正确 (T1 79/79, T6 6/6 逐子叶 ipix+pixel 与全量读取完全一致)
  - HCSD 字节级可重现 (T1 SHA-256 = P00-003 baseline SHA-256)
  - 残留: 无 format_version (§9.1); 无校验和 (§9.2); meta 无 input_hiss_files (§4.3 不强制); DLL 路径需用 lib/orchestrator/cpp/ (非本任务引入)

## P07-001 依赖

- P05-002 (DONE, Stage1 真实数据端到端)
- P06-003 (DONE, HCSD 输出与独立读取)

## 执行步骤

1. 按 `docs/10_STAGE2_REAL_DATA_VALIDATION_SPEC.md` 执行
2. 固定硬件/线程/配置后比较
3. 记录峰值内存、耗时、泄漏与取消后状态
4. 性能异常必须定位，不能只提高门限
5. 独立复核以 VERDICT: PASS 结束

完成独立复核后, 更新状态并进入依赖满足的下一任务。
