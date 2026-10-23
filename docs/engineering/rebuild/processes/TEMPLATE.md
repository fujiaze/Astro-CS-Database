# Process文档模板

新process复制本结构填写，删掉占位说明后才进入登记。一个process只拥有一组内聚科学操作；已有process文档可作为完整示例。

## 注册与边界

给process_id、模块/本地descriptor路径、operation白名单、typed输入输出、单位/index/shape/必需能力、依赖模型版本、科学依据、CPU实现与未来后端能力。每个operation绑定唯一Request/Response类型和adapter；不同Request不能共用未检查的void指针。算法参数和执行/诊断配置分开。

## 算法与失败合同

给确定步骤/数学对象、默认及显式策略、精度/停止、partial范围、unknown/无支持、错误code/reason与重试条件。列真正消费者及其失效/重算依赖，不复述公共Result字段。科学推导引用foundation，算法实施细节本篇唯一拥有。

## 执行、寿命与观测

给work-unit key、输入/halo/输出块范围、归约次序、组件屏障、是否可重入、资源上界函数及最小可行单元、spill/多遍读取路径、取消安全点和输出强引用。说明可并行部分和不得拆开的相关量。登记局部probe的id/边界/单位/坐标/载荷；默认执行span由框架提供。GPU仅声明可批次边界、dtype/layout/归约与误差要求，当前不提供GPU实现。

## 独立验收与维护

列精确小例、科学负例、schema/单位/index/资源/取消/多线程反例和可完成性能基线；标证据身份及尚未执行范围。说明改变本模块时需更新哪些消费者；不能用已编译、目录齐全或旧版本结果代替生产实跑。


## 计算组合与可复制包

列本process直接使用的公共计算模块及typed函数依赖、异步read/append的就绪/完成边界、pipe容量与最小可推进frontier。写清哪些步骤并行、哪些必须汇合/屏障；模块不调用future.get阻塞同一pool。提供最小CPU依赖包/独立小例，自动生成登记/schema/默认观测的接入步骤；GPU只声明未来能力，不要求本期实现。



## 详细接口/资源检查

实现FOUNDATION_API的Operation五hooks：validate、bounds、plan(显式PlanOptions)、run、finalize；先登记TypeId/schema/codec。plan包含region/跨region累加边、cleanup额度和物理claim/commit模型。给F生存期模拟及最坏workspace界、E窗口、PlanOptions缩块/spill反例；有全局barrier还须E=0多cohort可推进。不能只填函数名和“资源由框架管理”即声称可交付。
