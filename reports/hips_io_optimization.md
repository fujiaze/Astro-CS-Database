# HiPS I/O 优化（V18R2）
- PERF-007：write_signal_support_tile 只分配当前 dtype scratch，跨 tile 复用（原每 tile 4×262144 缓冲）
- PERF-008：hierarchy 按 NESTED 序 sig/sup 缓存直通（免每 pixel nested_local_to_fits_index 反查）
- 单帧 [hips][profile]：transform 0.037s / fits_write 0.088s / hierarchy 0.047s / 总 0.101s（非瓶颈）
- 科学等价：tile 57/57 与 V17 同构（FP32 舍入级）
