# ACSD V2 Agent 工作规则

当前任务是 **把 main 原有生产科学算法迁到 V2 的模块化、真正内存流水线工程，并完成科学性与性能收敛**。不是算法研发，不是用新数据结构把旧算法重写到功能缩水。

先阅读**[完整科学文档](docs/science/README.md)**中对应领域的主要推导及main算法分册，特别是观测量/坐标/PSF/光度/Drizzle、Sky、传统排异与积分的数学语义；不得把科学正本缩写成一个几千字的概要。再进入[工程架构入口](docs/engineering/rebuild/README.md)及[API](docs/engineering/rebuild/API.md)、[对象](docs/engineering/rebuild/OBJECTS.md)、[执行资源](docs/engineering/rebuild/EXECUTION.md)；从 [TASKS](docs/engineering/rebuild/TASKS.md) 中读取当前步骤，逐个核查 [MODULES](docs/engineering/rebuild/MODULES.md) 的 main 生产调用与V2实际代码，再按 [PIPELINE](docs/engineering/rebuild/PIPELINE.md) 实施。若文件与代码冲突，先用真实调用和科学公式查明事实，再修改代码和文档，不能反过来使代码迎合错误规范。

## 施工准则

- **科学文档完整迁移**：以main完整公式/推导/单位/适用域和已有V2成熟基础为正本；每迁一个生产科学kernel，必核对它的对应科学分册并修订错误/过时接口和文献引用。保留必要数学细节，不随意删除科学章节，不把研究性模型作为必须新开发的功能。采用实际代码与数学推理审查，而不是自动门禁。
- **科学代码先复用，工程胶水重构**：main 的校准、IPV、PSF、测光、autoNSIDE/Drizzle、Coverage/Sky、传统Reject/Integrate、Export的真正生产核与已有效优化优先抽取。V2保留一套模块/缓存/线程池/Storage，不引入第二条生产计算路径。
- **真正的连续数据流**：科学阶段在一个进程内按块交接有寿命的只读 Buffer/ArrayView，生产者等待下游容量；小型全局拟合按数学需求设屏障，屏障后仍恢复块流。不同CLI独立运行必然需要持久产品；但同一流程内部不得为了传递中间数据反复整幅落盘和读回。受控 spill/断点恢复不是常规数据边。
- **不研发**：传统排异与加权平均按 main 的实际可达算法；不再开发BLUE/QP/新的SNR优化或乘性flat校正器。已有科学错误按真实物理含义修复，缺真实标定/响应则诚实标unknown，不虚构功能。
- **性能不显著退化**：保留旧分块/stripe归约/几何LRU/touched-only Tile、Sky稀疏解、P3共享LRU和流式FITS。以相同输入/算法/NSIDE/机器实际画像判断，不以粗化order或不运行步骤换速度。
- **审查依赖推理**：逐调用点核实同一对象/单位/像素映射/权重与线程所有权；把实际原图和输出FITS、星点、sky边界和内存热图作为证据。必要时用少量独立数值算式定位问题，但**不建立自动门禁、夹具数量指标、人工假绿假红**；编译/测试成功只能证明各自范围。
- **自主完成**：遇编译/运行/模型/内存错误即定位原因、修复、复查受影响路径、继续下一模块；不给用户反复要新提示词。仅真正缺原始校准、物理不可辨识、权限或机器资源等外部问题才说明阻断。可让子Agent核对不重叠的旧核/性能/科学细节，主Agent承担唯一merge。
- 原始 `main`、`testdata/`、Gaia库只读；只在 `rebuild/acsd-v2` 提交；无PR/强推。不要把科学大FITS加入Git；临时缓存可按生产者身份重用、失效后安全回收。不保存成批的历史审核文档和重复产物。

本轮工程完成的陈述必须来自实际工作流与产物：**数据是否真的经过流水线、算法是否是main原有算法、真实星点和天空是否合理、资源/性能是否可解释**。Issue/提交/文档都不是科学完成的代替物。
