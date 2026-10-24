# 科学Process的独立实施与复现标准

本篇只规定文档和验收的完成标准，不新增框架或科学定义。数学归五份foundation，算法实施归所属process，公共原语归NUMERICS。架构完整、算法规格可复现、生产实现通过是三个独立结论。

每篇process的“可独立实施的算法合同”须给下列内容，不能只引用“使用QR/框架负责资源”或要求读旧源码才能决定关键规则：

1. 估计量、适用假设、固定/拟合量、输入单位/index/版本、依赖能力；明确unknown和未建物理项。
2. 每个登记operation的执行分支和有序步骤，有限循环、候选/组/归约键；哪些是搜索heuristic，哪些产生数学证书。
3. 每个算法参数的来源：Request字段、已持久化模型或本篇固定常数。Request无字段时不能从环境/经验猜关键规则；扩展科学策略须先修公开Request。
4. 名义数值算法、严格误差包围、随机根/influence和模型偏差分别怎样生成；明确证书的原语依赖和失效条件。
5. 最小不可拆单元、需要的shape/nnz/位长/参数数、实际workspace上界、CPU work计量、流式/spill路线与屏障；所有整数尺寸运算先checked。
6. 精度/迭代/工作量/资源的停止条件，以及失败后可消费的有效域。预算不足不自行改模型、秩、mask或容差。
7. 既有Code/Assessment/Issue与输出有效性的对应，未知能力、无支持、不可估、未认证、失败/取消分别表达；不得发明不存在的枚举或线协议字段。
8. 至少一个有判别力的独立数学例子，以及所列关键分支负例；给输入、期望和错误实现会怎样失败。资源/取消/重开/DEV_MODE是附加门槛。
9. 上下游数据边、同根/误差/几何revision传播和失效闭包；不靠隐藏全局会话补输入。
10. 陌生开发者复现：只给公开科学/工程规范和API声明，审核者写出伪代码、状态表和固定例子的独立结果，再与实现比较。若必须读src才能决定估计量、背景阶数、阈值、失败或停止，该模块继续记SPECIFICATION_GAP。

本轮各篇固定了首期路线；可替换私有容器和合法等价求解，但不能由编码者改变这些科学规则。新增高阶/鲁棒/非Gaussian等策略必须有显式请求和单独验收，不把扩展讨论当当前必需已实现能力。

## 状态、证据与数值认证

文档中的局部标签如rank_unresolved/conditional是科学原因或Assessment状态；顶层Code以公开枚举为准。例如未知完整covariance的action返回unavailable，非可估通量返回not_identifiable，谱有显著负值返回invalid_covariance，预算未达目标分别precision_exhausted/resource_exhausted；允许的partial仍保原非ok和实际有效域。

“certified”只在[NUMERICS](NUMERICS.md)的实际原语/积分/线性解/根验证依赖已通过且当前输入在已验证域内成立；名义解、converged、rank字段或一次正常例子都不能替代它。条件于固定PSF/权/选择的结论必须携带条件，不能变成物理模型无偏或选择后精确coverage。

具体完成状态唯一导航为[IMPLEMENTATION-STATUS](../../../audit/IMPLEMENTATION-STATUS.md)。本篇和各模块新增case是后续生产验收要求，本轮只修文档，没有运行这些生产case。
