# 开发实施规格补全与定向复核

日期2026-10-23。范围承接[process框架记录](process-framework.md)：负责人要求文档能直接分配开发任务，不能只描述角色和架构意图。本轮仍只修改Markdown；不修改源码、公开头、schema、构建和科学测试数据。内部开发只维护当前合同，无旧产品/API兼容要求，GPU只设计扩展。

## 可直接交给开发者的内容

- [FOUNDATION_API](../engineering/rebuild/standards/FOUNDATION_API.md)：定稿目标公共头、字段和C++20签名；Task/错误汇合、强所有权、容量票据、异步Storage、有界pipe、五个typed hook、codec/registry、统一Host/wire入口、region/epoch、取消清理及诊断宏。
- [SCHEDULER](../engineering/rebuild/standards/SCHEDULER.md)：首期CPU调度的固定配置、账本原子转换、单cohort进展保底、R/F/E生存期模拟、明确重规划入口、分支/join信用、低key发布保留、多command加权轮转、I/O/CPU分段和故障排空。
- [COMPUTE_MODULES](../engineering/rebuild/standards/COMPUTE_MODULES.md)：22族计算函数、现有操作到强类型请求的生成规则、块函数声明与组合器语义、每族资源上界必需项。
- [CALIBRATION_WALKTHROUGH](../engineering/rebuild/standards/CALIBRATION_WALKTHROUGH.md)：首个模块的文件责任、唯一像元公式/共享噪声Jacobian、60n+S工作集、完整run/finalize/登记代码及固定科学/低内存/故障输入。
- [DEVELOPMENT](../engineering/rebuild/standards/DEVELOPMENT.md)：M0—M6交付顺序和门槛，22个process各自的工作key、最小不可拆单位、R/F及全局屏障、依赖与独立case。每个process文档已链接自己的实施合同。

这些是目标实施合同，未把新接口写入实际公共头。后续实施时，公开头成为可编译声明的唯一正本，规范摘录与manifest由检查保持一致；模块不能自行新建另一套Task、资源池或注册通道。科学公式仍由五份foundation和所属process拥有。

## 修订后复核结论

两位独立只读审核者分别复核资源/执行合同与诊断/GPU边界，未参与规格编写。已关闭的必要问题包括：

1. 显式bounds和PlanOptions允许Host重新调用plan，不按比例猜资源；pipe绑定具体region/edge。
2. stream贡献进入已计R的累加器即退F，barrier等全部EOF才借进展额度；E=0也不要求唯一cohort等尚未启动的下一cohort。
3. claimed票据与物理allocated分开；失败分配不增物理峰值，已准入票据在arena关闭后仍可commit，最后Lease才真正释放计费。
4. science取消/工作预算耗尽后，预留内建cleanup仍能排空/abort/unlink；不开放任意清理名义的新科学任务，清理失败保原主code。
5. registry可登记schema/codec，统一wire入口由同一typed binding生成；transport成功与科学结果code分开，外部块及临时产物有明确寿命。
6. 校准validate只核元数据，冷输入逐值检查在异步读后的计费CPU段；母版共享bias的导数与像元公式一致。
7. 诊断入口借用context，选择前不复制长字符串；宏内构造异常隔离，快照含dtype/shape/range/index/unit且不保科学Lease。

定向复核范围内无未决必要P1/P2。这不是生产实现正确性的认证，也不替代未来逐模块科学、故障、并发与性能实跑。

## 本轮实际检查与边界

既有设计检查通过：六个根工程文件、302项schema定义、77种operation、示例/链接与4295条历史删除记录。独立文档核对通过：22个process、57个typed operation绑定唯一，38项模块/服务probe（33项科学、5项服务）所有权一致，当前公开科学free函数均有归属；另有Host通用phase/usage观测。

从FOUNDATION_API、COMPUTE_MODULES和校准实施例提取全部C++代码块，按顺序拼接，提供仅用于语法检查的then/settle模板体后，以C++20及Wall/Wextra检查通过；这验证声明、lambda返回和typed登记能相互匹配，没有运行Task/pool/Storage或科学算法。

诊断宏另作独立fixture：OFF参数含未声明表达式仍可编译；ON使用已有记录和延迟payload签名可编译。实际运行的隔离fixture以测试替身sink检查未选快照不构造payload、不复制长context；选中payload或record构造抛异常只计丢样，科学调用不受异常传播。该fixture未验证生产sink的文件、配额或调度实现。

S-FLOOR至S-LIFE、C-CAL及DEVELOPMENT各门槛仍是后续实施验收任务；本轮不报告运行吞吐、线程扩展率、科学精度或GPU性能。提交以最新远端树追加且只含Markdown，保留现有实现及分支历史。
