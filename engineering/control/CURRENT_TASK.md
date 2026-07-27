# 当前任务：P06-003 HCSD 输出与独立读取

读取 `tasks/P06-003.md` 并执行。验证子叶索引、metadata、输入追溯和浏览器/独立读取。

## 上一任务完成情况

- P06-002 球面梯度与稳健叠加证据: DONE (VERDICT: PASS)
  - 证据: evidence/P06-002/
  - 7 项验证全部 PASS (T1 baseline 可重现 / T2-T4 sigma-clip 三档 / T5 梯度校正启用 / T6 确定性 / T7 SNR² 权重真实生效)
  - SNR² 权重真实生效 definitive proof (合成 HISS has_snr=1, 输出像素=18.0=SNR² 加权均值, 非等权 15.0)
  - 梯度校正管线完整运行 (GaiaDR3SP 启用, GaiaClient 创建成功, 43383 颗 Gaia 星, 5 阶段完整运行, HCSD meta 标注 success=true)
  - 确定性保证 (T6 两次运行 SHA-256 完全一致)
  - baseline 字节级可重现 (T1 SHA-256 与 P00-003 完全一致)
  - 残留: G-002 真实数据 has_snr=0 仍退化为等权 (待 P03-004 修复后回归); HCSD has_snr 字段不传播 (不影响堆叠数学); 梯度校正 fit_rms=0.0 (C003 副本差异为 0, 待未来用不同帧测试非零差异)

## P06-003 依赖

- P06-002 (DONE)
- P04-003 (DONE, capabilities 与 inspect 命令)

## 执行步骤

1. 按 `docs/10_STAGE2_REAL_DATA_VALIDATION_SPEC.md` 执行
2. 验证子叶索引 (nside 子叶位移/非空子叶数)
3. 验证 metadata (HCSD meta_json 字段完整性)
4. 验证输入追溯 (HISS → HCSD 帧溯源)
5. 验证浏览器/独立读取 (inspect 命令 + 第三方工具)
6. 独立复核以 VERDICT: PASS 结束

完成独立复核后, 更新状态并进入依赖满足的下一任务。
