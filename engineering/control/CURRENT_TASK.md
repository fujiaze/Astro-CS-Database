# 当前任务：P06-002 球面梯度与稳健叠加证据

读取 `tasks/P06-002.md` 并执行。验证球面梯度校正与稳健叠加 (sigma-clip/Winsorized) 在真实数据上的效果, 保存梯度校正前后对比、拒绝统计和输出索引证据。

## 上一任务完成情况

- P06-001 Stage2 真实输入兼容检查: DONE (VERDICT: PASS)
  - 证据: evidence/P06-001/
  - 8 项兼容性检查全部 PASS (A baseline 可重现 / B1 nside 一致 / B2 nside 不一致 / B3 order 不一致 / C filter 混合 / D 重复帧 / E 损坏文件 / F 空目录)
  - SNR² 权重代码路径已证明触发 (日志含 "SNR² 加权"), 因 has_snr=0 退化为等权 (G-002 既存缺口)
  - baseline 字节级可重现 (HCSD SHA-256 与 P00-003 完全一致)
  - 53 个证据文件, 全部 SHA-256 索引
  - 残留: G-002 既存缺口 (has_snr=0 → 等权退化, 待 P03-004 修复后回归); gaia_client_create_ex 失败 (梯度校正回退, 待 gaia_data_dir 配置修复)

## P06-002 依赖

- P06-001 (DONE)

## 执行步骤

1. 验证球面梯度校正效果 (前后对比)
2. 验证稳健叠加 (sigma-clip/Winsorized) 拒绝统计
3. 保存梯度、拒绝和输出索引证据
4. 独立复核以 VERDICT: PASS 结束

完成独立复核后, 更新状态并进入依赖满足的下一任务。