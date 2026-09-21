# 任务：PERF-401 cfitsio 全局锁与 Phase2 并行性能

## 目标
消除 cfitsio 全局锁导致的 Phase2 并行区间均值仅 0.71 核瓶颈，使重计算区间 CPU 利用率接近满载，满足 L2 性能验收；落实最高设计 §9 的内存极简与编排连续性。

## 权威依据
- ASTROCS_DESIGN.md §9（CPU 后端与资源）
- ACCEPTANCE_SPEC.md §4（L2）
- GAP_AUDIT G3-14

## 前置
- FIX-401（原子发布）、CLEAN-403（aio 收口）完成。

## 改动范围
- 允许改：aio FITS 读路径（cfitsio 句柄线程模型/每线程句柄/只读句柄池）、scheduler 并行区间编排、phase2 读 tile 调度、缓存（Gaia/星表两级缓存与查询合并）
- 禁止改：科学公式与数值（归约顺序保持，1/N worker 逐位一致合同不变）

## 步骤
1. 分段计时定位：Phase2 各区间墙钟、CPU 利用、IOWait、锁等待，确认 cfitsio 锁占比；
2. 方案选型（实测对比，不拍脑袋）：每线程独立 fitsfile 只读句柄、tile 读取预取流水线（异步 I/O 单线程 + 有界队列）、或必要的句柄池；读路径无共享可变状态；
3. 编排复核：locality-aware 调度，同一块连续节点同 worker 走完，块间流水；消除"A 做一半切 B 再回 A"；Gaia 同组查询合并、外部请求计数为 1；
4. 内存核对：工作集随分块不随总帧数；稠密权重/天光面现场求值无整轮驻留；缓存有容量/身份/失效/线程模型四要素；
5. 1/4/16 worker 加速比、worker_balance、RSS 曲线归档为性能基线。

## 验收门
- [ ] Phase2 重计算区间（>10s）无连续 ≥10s 低利用窗且队列有工作；均值利用率达标（给修复前后对照）；
- [ ] 1/N worker 产品逐位一致（合同容差内）；
- [ ] RSS 不随总帧数线性增长（成倍帧数对照实验）；
- [ ] Gaia 外部请求同组计数为 1；
- [ ] L2 门通过，基线入 artifacts/acceptance/l2_performance/。

## 禁止
- 不以增大分块/堆内存换利用率；
- 不在科学 kernel 里用 async/future 制造嵌套并行。
