# 命名块与阶段内对象生命周期

本合同依据最高设计的阶段内内存管线。Normalize 字段与端口由 [NORMALIZE_DATA.md](NORMALIZE_DATA.md) 定义；跨阶段产品由 [PHASE_PRODUCT_EXCHANGE.md](../data/PHASE_PRODUCT_EXCHANGE.md) 定义。

## 载体与现状

目标：同阶段节点通过 Runtime 管理的 typed metadata、模型句柄和命名数组块交换数据，跨阶段通过磁盘产品交换。output_dir 文件约定是当前生产实现的载体，须逐边迁移为 typed payload 或显式科学 cache，不能定义成永久唯一载体。当前 BlockFrame 数值块主要在节点内部使用，RunContext artifact descriptor 也不等于已具备跨节点 typed 载荷；扩展范围见 Normalize 接口规格。

开发快照仅旁路观察。正式 spill/cache 由内存预算、恢复需求决定，ON/OFF 同样可用。cache 引用按 run/frame/object/revision/input hashes 校验，禁止以“文件存在”替代生产者完成和身份核验。

## 字段

| 字段 | 语义 |
|---|---|
| name/schema_id | 阶段内对象身份与载荷类型 |
| shape/dtype/unit | 数值数组尺寸、位深和真实单位；模型 metadata 有自己的字段合同 |
| run_id/frame_id/revision | 当前帧和上游版本绑定 |
| producer/consumers | 唯一生产者与实际消费者 |
| optional/status/reason | 可缺性及明确能力状态；未知不等于零 |
| lifecycle | node、frame 或 run 范围 |
| provenance/input_revisions | 来源与准确上游对象 |

数值数组使用现有 BlockMeta；对象身份与复合 metadata 由 typed header 承载，不要求全部塞到单个 unit 字符串或浮点数组。大面默认 FP32、稀疏与元数据 FP64。

## ARS 目标：typed Block 与版本转移

现在 `BlockFrame::create` 内部为数值块分配 bytes，`consume` 按声明消费者回收，但并未提供跨异步请求的 read lease/AIO pin、attempt_id 或 immutable revision。新数据链为 `reserve_bundle → begin_output(private) → fill → validate → publish(immutable) → borrow/read → last lease/ticket release → memory credit release`。旧块在新块计算完成、所有消费者和 AIO 借用结束前始终 live；复合 WCS/PSF/photometric/noise 模型应有 typed model handle、单位/几何和来源，不用 Block 名称猜测。Frame 压力回滚必须先 quiesce CPU/AIO 再批量销毁该 attempt 的私有 Block，不能直接清空整个 Run 或 Mosaic 共享对象。对象 Key、接口签名、shape 溢出与移交释放域见 [ARS 可编码设计 §3–5](../architecture/PROCESS_RUNTIME_DETAILED_DESIGN.md)。

## 生命周期与并发

生产者构建新对象后一次发布，之后只读。Runtime 给声明消费者分发借用/lease，最后一个完成即回收；异常/取消同样归还数组、AIO 和模型句柄。模块不保留未登记的大对象副本，不用全局 map 绕过预算。

图必须有唯一生产者、真实消费边和合法生命周期。descriptor、IR、port registry 与实际取数需一起更新；注册表是当前代码的事实登记，不能推翻目标科学图。节点序可取任意合法拓扑序，独立节点不因数组声明顺序获得额外数学依赖。typed metadata/availability 可以存在但某数值载荷不可用，不能要求“有对象就意味着全部科学模型合格”。

## 检验与机器登记的边界

核对重复生产、跨帧错配、版本陈旧、漏声明读写、方向错误、缺模型、取消泄漏和实际峰值内存。未声明的必要输入返回数据错误；允许缺失的模型返回 status/reason。检查用于发现真实错误，不创设人工审批流程。

当前 module_ports.registry.json、pipeline_block.schema.json 及 port/IR 检查脚本仍记录文件载体和既有词表。执行者随每条生产边的改动同步迁移，旧脚本 PASS 只能说明其登记域，不能证明 typed payload 已流过生产代码。不得把仅匹配 token 的检查当作科学数值证据。
