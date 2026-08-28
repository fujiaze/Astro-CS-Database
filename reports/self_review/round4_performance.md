# Round 4 — Performance（V18R2 资源驱动）

## before（冻结基线，V17）

```text
3× 完整 16 帧：129.7 / 126.1 / 126.65 s（中位 126.65）
```

## 单帧资源剖析（frame00，250ms 采样）

| 指标 | before | after | 变化 |
| --- | --- | --- | --- |
| wall | 131.7s | 65.7s | -50% |
| RSS 峰值 | 37.5GB | 1.2GB | -97% |
| PLATESOLVE | 15s（36GB mmap 读） | 0.16s | -99% |
| PHOTOMETRIC | 17.8s（36GB） | <1s | -97% |
| 进程退出 | 40s（36GB 释放） | 0.7s | -98% |
| Drizzle | 69s（78% CPU） | 62s（86% CPU） | -10% |

## 最终完整 16 帧（V18R2，唯一一次）

```text
wall median 67.35s / p95 68.4s，16/16 rc=0
（vs before 126.65s → -46.8%）

阶段 median：DRIZZLE 64.1s（主导，86% CPU）、PLATESOLVE 0.15s、
PHOTOMETRIC 0.03s、PSF 1.14s、CALIBRATE 0.46s
```

## 硬件利用率结论

- Drizzle 86% CPU（接近饱和；ACR/GPU 无收益——接 GPU 无意义）；
- 空转/等待根因全部消除（mmap 页读、退出延迟）；
- 分配：Drizzle 每像素零堆分配；gaia 解压缓存 4GB 上限；
- 复杂度：候选查询每像素 ~25（常界）；hierarchy 直通。

```text
ROUND4=PASS
```
