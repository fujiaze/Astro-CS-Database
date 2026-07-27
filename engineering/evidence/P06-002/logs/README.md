# P06-002 logs/ 目录索引
# 本目录汇总 P06-002 任务的 stage2 运行日志和 inspect 输出
# 详细日志在各测试子目录的 logs/ 下

## 主日志（汇总）
- stage2_run.jsonl: T1 baseline 运行 stdout (JSON 结果)
- stage2_run.err.log: T1 baseline 运行 stderr (DEBUG 日志, 含 SNR² 加权/sigma-clip 关键词)
- inspect_hcsd.json: T1 baseline HCSD inspect 输出

## 各测试子目录日志
- T1_baseline/logs/: baseline 复现 (P00-003 输入, DEBUG)
- T2_sigma_clip_debug/logs/: sigma-clip 严格模式 (合成离群值, DEBUG)
- T5_gradient/logs/: 梯度校正启用 (GaiaDR3SP, DEBUG)
- T6_determinism/logs/: 确定性测试 (两次运行)
- T7_snr_weight_debug/logs/: SNR² 权重证明 (合成 SNR HISS, DEBUG)
- runs/T2_outlier_strict/logs/: sigma-clip 严格模式 (早期运行)
- runs/T3_outlier_default/logs/: sigma-clip 默认模式 (早期运行)
- runs/T4_outlier_loose/logs/: sigma-clip 宽松模式 (早期运行)
- test_B_overlap_duplicate/logs/: 重叠图证据 (C003 副本)
- test_D_snr_weight/logs/: SNR² 权重早期运行
