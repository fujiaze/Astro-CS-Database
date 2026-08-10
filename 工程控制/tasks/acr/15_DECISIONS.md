# 冻结决策

1. ACR是独立底层运行时，本分支不改任何现有算法。
2. 继续使用唯一`feature/astrocompute-runtime`，不创建版本分支或新仓库。
3. 控制包不使用发布版本号，修订直接更新这一份。
4. 路由依据是通用多维硬件画像，不是业务kernel固定比例。
5. 用户不配置CPU/GPU任务份额，公共API和配置禁止share参数。
6. Benchmark覆盖CPU ISA、FP32/FP64、内存、传输、归约、卷积、稀疏、原子、分支和启动开销。
7. 画像保存尺寸曲线和驻留条件，不生成单一总分。
8. 算法未来只提供TaskClass/TaskTraits和工作域。
9. 运行时使用成本模型和共享工作池动态派发CPU、单GPU和多GPU。
10. 设备完成后继续领取工作，尾部收缩；这不是在线学习。
11. 正式运行不修改hardware-profile。
12. 默认资源占用目标CPU/GPU约95%；所有CPU线程可参与。
13. 未标定时纯CPU多线程并发出非阻断警告。
14. 默认IEEE FP32，特殊任务声明FP64或FP64 accumulator。
15. 最大程度复用alpaka/oneTBB/hwloc/cpu_features/Google Benchmark及成熟FFT/BLAS原语；公共API不暴露第三方类型。
16. STREAM/BabelStream/PolyBench/HPCG/Roofline作为经典测试和模型设计参考，不用整套总分直接路由。
17. 至少一个真实GPU后端必须通过，CPU-only不依赖GPU SDK。
18. ACR lazy initialization，合并main后普通AstroCS无副作用。
19. Evidence必须从一个干净HEAD一次生成。
20. 全部验收后`--no-ff`合并到main备用，后续算法集成另开分支。
