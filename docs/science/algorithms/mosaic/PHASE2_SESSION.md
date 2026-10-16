# Mosaic 局部会话的离散装配

本页依据[最高设计](../../../ACSD_DESIGN.md)、[背景科学](../../sky/UPM.md)和[Mosaic管线](../../../detail/mosaic/pipeline.md)，解释局部装配算法。所有函数、参数、默认值、manifest字段、返回码、资源与异常由[session详细设计](../../../detail/mosaic/modules.md#session)完整规定；本页不另立科学模型或调度器。

## 1. 装配对象与生产调用

`lib/phase2_session/p2_session.cpp` 是局部 coverage/sample/UPM/persist facade，静态目标 `acsd_phase2_session`；它没有把背景施加、排异、集成或signal写出接入本会话，正常结束标partial。`module_adapters.cpp::P2Api` 保留对该五函数的委托；聚合 `acsd.phase2.resample` 工厂使用 SessionModule。生产七个节点已注册独立 P2NodeModule operation，不能沿用旧“七节点均委托全session”的描述。

| 局部阶段 | 输入 → 输出 | 实际域调用 |
|---|---|---|
| coverage | 冻结HiPS路径 → N份元数据、K个union tile、target_order | p2_coverage_build 两遍 |
| sample | coverage、相同路径、默认采样配置 → J份控制观测、G个几何节点与统计 | p2_sample_controls 两遍 |
| upm_build | 观测、具名配置 → opaque UPM模型和可选info | p2_upm_build/info |
| persist（条件） | 模型、显式保存路径 → 单UPM文件和artifact登记 | p2_upm_save |

生产背景实际用 sky-plane，不是由此表证明库 M+C 模型就是当前扣除模型。局部模型和七节点产品分别报告。

## 2. 顺序算法与失败前缀

```text
create(host): 握手顶层ABI，保存host，初始化诊断/manifest
validate(config): 检JSON/object/必需键与部分类型，不读产品
run(config):
  parse；建立局部路径及指针视图
  若取消：追加coverage cancelled并返回
  coverage running
  probe coverage；仅当rc非0且K=0时终止
  分配inputs与max(K,1) cells；fill coverage；失败终止
  安装coverage POD清理guard；追加coverage ok与计数
  若取消：追加sample cancelled并返回
  sample running；默认配置注入host workers
  probe sample；仅当rc非0且J=0时终止
  分配max(J,1)观测、max(G,1)节点；fill；失败终止
  观测缩到实际J；追加sample ok与计数
  若取消：追加upm_build cancelled并返回
  build本会话UPM配置；构建model；失败走域码映射
  info若成功则附计数/hash，失败仍记build ok
  若persist_upm且存在upm_save_path：
    若取消：close model；追加persist cancelled并返回
    save；失败close并返回IO；成功登记path/artifact
  close model；追加顶层计数、status=partial与能力表
  返回ACS_OK
inspect(): 序列化当前manifest，host alloc；调用者host free
destroy(): 释放SessionState（run局部对象此时不再持有）
```

这是实际实现顺序，不表示其中所有异常路径都已满足工程合同。probe非0有计数被忽略、未复用validate、模型非RAII、rc0空模型返回码歧义、取消/重复run状态都在detail明确，不能将正常路径计数泛化到所有失败前缀。

coverage、sample均重做两遍，输入产品必须同一快照；计数相等还不证明内容身份相同。两遍容量设计及各自检查能力分别由[coverage](PHASE2_COVERAGE.md)和[sampler](PHASE2_SAMPLER.md)说明。

## 3. 科学量与数值边界

本层不定义新的估计量。控制值继承实际产品单位，背景控制方差与点源参考信息量分开。Huber/规范/平滑/支撑的物理含义见UPM；本会话只透传有限配置子集，其sigma_floor与绝对tolerance不是普适无量纲误差标准。

同一handle串行，域段顺序执行，worker上限注入不等于本层提供租借实现。取消只在实际到达的段入口轮询，不提供段内实时取消。persist的模型文件、最终HiPS与完成清单属于不同产物，不能据保存返回0宣布整个mosaic已发布。

## 4. 独立核验与参考实现

装配验证用可编程host/logger/cancel/allocator，最小确定性HiPS输入、域调用计数和失败注入；分别核验正常路径、每个失败前缀、探测/回填、所有权及取消后inspect。配置/计数字段用精确比较，域浮点输出用其独立真值与适用容差。静态符号存在、构建、实际执行和科学验证分开，本轮未执行。

旧402行中的有效拓扑、五API、全部配置常数、manifest、错误/取消、并行、调用计数与验证负例均归正式detail；旧任务归属、迁移冻结和当时缺测试登记保存在[既有来源档案](../../../../实验/documentation-refactor/docs/MOSAIC_REDUCERS_SOURCE.txt)。档案不承担接口定义。

参考实现是上述本仓p2_session及module_adapters真实工厂；其许可证以仓库对应文件为准。Kahn, “Topological sorting of large networks”, CACM5(1962),558–562，[DOI](https://doi.org/10.1145/368996.369025)用于一般DAG顺序背景，本会话本身按固定顺序直调，不执行该拓扑排序算法。[W3C PROV-DM](https://www.w3.org/TR/prov-dm/)用于来源模型阅读，不证明当前manifest已实现该标准；SHA-256和FITS校验和由产品/I/O专题定义，不能仅凭本会话引用宣布已校验。
