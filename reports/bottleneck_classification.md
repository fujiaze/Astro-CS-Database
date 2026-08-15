# Bottleneck Classification（V18R2）
| 阶段 | before 分类 | after 分类 | 修复 |
| --- | --- | --- | --- |
| PLATESOLVE | memory/page-fault（16.3GB mmap 读） | CPU（0.15s） | 极投影平面剪枝 |
| PHOTOMETRIC | memory/page-fault（26GB） | CPU（0.03s） | spectrum 同剪枝 |
| DRIZZLE | CPU 78% | CPU 86%（饱和，62s） | profiler 门控/scratch/顶点缓存/dot 预判 |
| 退出 | serial/memory（40s 释放） | 0.7s | RSS 1.2GB |
| ACR 适用性 | - | 无收益（Drizzle 已饱和） | 保持 CPU 权威 |
