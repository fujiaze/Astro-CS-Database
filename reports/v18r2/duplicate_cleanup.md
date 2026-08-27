# Duplicate Cleanup（V18R2）
| 重复 | 处理 |
| --- | --- |
| SHA-256 ×4（common/orchestrator/ACR×2） | 归一化 common/crypto（编译 + 配置 SHA 一致验证） |
| lib/data_pipeline vs astro_image_io | 删除 data_pipeline（canonical = astro_image_io） |
| legacy healpix_stack 构建引用 | V17 已归档；V18R2 确认 drizzle Makefile 独立 |
| omp_set_num_threads 全局 | parallel num_threads 子句 |
