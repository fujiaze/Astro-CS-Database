# Phase1 最终性能（V18R2）
before 冻结基线：129.7 / 126.1 / 126.65 s（3× 完整 16 帧）
最终 1 次完整 16 帧：wall median 67.35s / p95 68.4s（-46.8%），16/16 rc=0
阶段 median：DRIZZLE 64.1s、PLATESOLVE 0.15s、PHOTOMETRIC 0.03s、PSF 1.14s、CALIBRATE 0.46s
资源：RSS 峰值 1.2GB（-97%）；Drizzle CPU 86%（饱和）；退出 0.7s
证据：evidence/performance/final_full_16frame.json
