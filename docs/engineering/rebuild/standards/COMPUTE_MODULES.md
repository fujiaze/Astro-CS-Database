# 可复用计算模块与强类型组合标准

本篇拥有计算积木的公共行为标准，基建骨架只在[SYSTEM](../SYSTEM.md)，具体科学算法仍在各process。所有下述Task/Pipe/Block/异步服务名称为目标角色，不是当前头文件已提供的实现。

## 计算积木的分层

| 模块族 | 提供什么 | 复用者与边界 |
|---|---|---|
| arrays / pipes | 分块读写、slice/view、dtype/索引/单位、封存、只读lease与有界通道 | 全模块；科学数据不经裸指针跨异步边界 |
| elementwise / reduction | 校准/标度所需强类型逐元素算术、DQ传播、固定key map/reduce | observation、photometry、coadd；signed和unknown语义不丢 |
| operators | diagonal/CSR/compose/apply/真transpose、依赖块查询 | geometry、PSF transfer、sky、covariance、Export；一份核心实现 |
| numerics | 区间/精确原语、积分、QR/SVD/稀疏解、PSD/QP、数值证书与停止 | 科学process；规则由NUMERICS唯一拥有，不用另一个GPU求解改变目标 |
| coordinates / geometry | 规范WCS、真实footprint、曲边交叠、支持Boolean与版本反馈 | drizzle、astrometry、Export；不把严格几何简化为四角采样 |
| PSF / spectrum | 唯一像元积分/transfer、响应基积分及实际误差 | fit/measure/information/sky；不各写模板积分 |
| uncertainty | 共享根/influence图、C action、泛函方差与绝对因子界 | 全链；不凭分块或复制制造新噪声根 |
| services | 异步read/append、spill/cache、事件/诊断、取消与事务提交 | 通过pool统一资源；不属于科学算法副本 |

这里是库能力分组，不要求每组再有一个可变manager或独立线程池。普通科学库函数可同步调用小型原语；有可等待I/O/并行块时返回Task或由统一组合器调度，不在算法中调用阻塞future.get等候同一池。

## 一个计算模块的标准

模块公共声明给出强类型Input/Output、domain/codomain单位/index/shape、前置能力、数值与随机误差合同、资源上界函数、输出所有权、重入/取消/异步完成语义。实施时可把公开声明拆为共享基础头与模块公共头，现有contracts.hpp作为聚合入口；每个类型/签名只定义一次，不要求永远把所有算法塞进一个巨型头。当前实际声明仍以已存在的contracts.hpp为准，本轮不改代码。

资源需求是依赖实际shape/nnz/位长的保守上界，至少分input新加载、output、scratch、队列/halo/元数据、spill与I/O在途；共享既有lease不重复收费。实际需要扩大时先增量准入，失败按安全边界返回，不能先分配后补账。不可控第三方workspace需实测余量并纳计划。

组合器包括then（上一步成功后消费typed输出）、when_all（并行独立依赖及错误汇合）、map_blocks（按固定key拆分）、reduce（固定科学顺序归约）和bounded_pipe（异步块流）；无需任意字符串表达式/反射图编辑器。非ok partial不可被then默认当完整成功，只有显式处理Assessment的分支才消费有效域。失败在所有在途任务安全结束后回收，保持第一故障及局部失败范围。

then/when_all只安排有界continuation；await后释放CPU token，回ready再准入，不在I/O完成线程运行重计算。finalize/seal/cleanup也走该完成依赖，不能在worker上get等待同一pool。有序pipe的发布信用按SYSTEM的next_required_key保留，单slot也不能被高key先占住。异步参数只能强持Ref/Block/lease或按值元数据，裸span仅限同步借用。

算法调用模块函数时共享本invocation的预算/lease/取消，不嵌套创建Host或重置work。可直接复用库函数而不重复顶层validate/plan；对外注册适配器负责边界科学校验。优化不得把单位、context、版本、共同噪声根或误差证书在函数间剥离。

## 用组合表达一个科学步骤

以下为目标结构示意，不是现有可编译代码或新公共签名：

```text
read_async(当前输入块)
  → then(校准/标度模块，产同单位与区间/DQ的只读块)
  → then(同一CSR/action模块，产signal及对应模型链)
  → bounded_pipe(显式items/bytes信用)
  → append_async(私有builder)
  → when_all(全部必要写入完成)
  → validate_and_seal(元素/误差/闭包)
```

独立source/窗口可用map_blocks并行；全局photometry/sky系统必须先汇合装配和求解，再开放后续阶段，不能因为管道方便就拆掉科学相关性。选权后coadd.integrate直接消费Selection/WeightSolution，不重新执行这些模块。共享数值函数只一份，CPU后端当前实现，未来GPU可替换支持的批次执行，模块上层数学组合不改。

## 可复制与接入完成条件

模板包必须有本地README/科学文档链接、公开typed接口与依赖列表、一个descriptor/adapter、CPU实现、合法小例与负例、必要许可证/来源。接入后registry/schema/能力目录自动生成，Host无算法switch，默认phase/usage观测自动出现。独立使用时能在最小公共基建/已列依赖下执行相同例子；依赖未提供应构建失败，不能偷偷查全局产品目录或原项目session。

高并行验收同时比较串行、单模块/组合、低H与spill、I/O延迟/失败/取消、满pipe、不同任务完成顺序。性能报告给CPU利用、有效吞吐、I/O等待、队列水位、峰值驻留、复制/传输字节和端到端完成比例；IO overlap本身不是科学正确或性能成功证明。
