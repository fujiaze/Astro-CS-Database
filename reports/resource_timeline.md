# Resource Timeline（V18R2 单帧资源剖析）

代表性负载：NGC1727 T2 H-alpha 4096² 单帧（orchestrator 全链），250ms 采样
（CPU 核利用 / RSS / 线程 / 磁盘 IO / 上下文切换）。

## Before（V17 基线，gaia 剪枝失效）

| 阶段 | 区间 | CPU均值(核/16) | RSS峰值 | IO | 分类 |
| --- | --- | --- | --- | --- | --- |
| CALIBRATE | 0-1s | 0.5 | 344MB | 235MB读 | IO（master 读取） |
| PSF | 1-2s | 9.4 | 427MB | - | CPU（并行，快） |
| PLATESOLVE | 2-17s | 7.3 | **36.5GB** | - | **内存暴涨 + page-fault 空转**（gaia 查询读 16GB mmap） |
| PHOTOMETRIC | 17-22s | 2.2 | **36.6GB** | - | **CPU 低利用 5s**（spectrum 查询同病） |
| DRIZZLE | 22-91s | 12.5 | 37.5GB | 54MB+54MB | CPU 78% 饱和（69s） |
| HIPS_VERIFY+退出 | 91-131.7s | 1.0 | 36.6→20GB | - | **40s 空转**（36GB 释放 + DLL 卸载） |
| 合计 | 131.7s | 7.7 | 37.5GB | 320MB读 | |

## After（V18R2 修复后）

| 阶段 | 区间 | CPU均值(核/16) | RSS峰值 | 分类 |
| --- | --- | --- | --- | --- |
| CALIBRATE | 0-1s | 0.5 | 411MB | IO |
| PSF | 1-2s | 8.7 | 431MB | CPU |
| PLATESOLVE | ~0.2s | - | - | **15s→0.16s**（gaia 极投影平面剪枝） |
| PHOTOMETRIC | ~0.5s | - | - | **17.8s→<1s**（spectrum 同剪枝） |
| DRIZZLE | 3-65s | 13.8 | 1.2GB | **CPU 86% 饱和**（主导，62s） |
| HIPS_VERIFY+退出 | 65-65.7s | 0.6 | 511MB | **0.7s**（36GB 释放消失） |
| 合计 | **65.7s** | - | **1.2GB** | |

## 结论（回答用户问题清单）

- **CPU 用了多少核**：Drizzle 86%（13.8/16），其余阶段低或瞬态；整体不再是"低利用"。
- **哪些阶段 CPU 低但磁盘也不忙**：原 PLATESOLVE/PHOTOMETRIC（8.9s 后 CPU 1-4 核、
  磁盘 0）——真相是 **mmap 页读入（page fault）**：查询读 16.3GB 数据但
  GetProcessIoCounters 不计 mmap 页，表现为"CPU 低 + IO 0 + RSS 暴涨"。
- **等待/锁**：无阻塞等待；瓶颈是 page-fault 串行页读 + Drizzle 负载。
- **反复打开/写 FITS/HiPS**：HiPS writer 跨 tile 复用 scratch（PERF-007/009）；
  CALIBRATE 每帧读 master（235MB，正常）。
- **每像素 heap allocation**：Drizzle 已线程本地复用（PERF-002），零分配；
  gaia 查询解压块缓存有 4GB 上限。
- **O(P×C×H)**：Drizzle 每源像素 ~25 候选 × overlap（S-H）；candidate 枚举
  已 NESTED 位操作 + 零漏证明；无二次方结构。
- **重复 trig/mapping/hierarchy**：边判断 acos→dot（PERF-004）、行级 Vec3 顶点
  缓存、hierarchy NESTED 直通（PERF-009）。
- **catalogue/plate solve 外部等待**：gaia 是本地 mmap 读，非网络；根因是
  极区 RA 环绕 bbox 退化 → 全树遍历 16.3GB（已修）。
- **内存/page fault/working set**：RSS 峰值 37.5GB→1.2GB（-97%），退出延迟
  40s→0.7s。
- **ACR 适用性**：Drizzle 86% CPU 饱和 → **GPU 无收益**（接 GPU 无意义）；
  应保持 CPU 权威路径。
