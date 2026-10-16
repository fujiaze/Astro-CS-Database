# CPU 后端架构

CPU后端提供同一数学kernel的可选实现，通过能力和测量选择。[ISA变体](ISA_VARIANTS.md)维护编译隔离，[能力探测](CAPABILITY_PROBE.md)解释硬件/OS状态，[安全加载](../../api/abi/SECURE_LOADER.md)解释模块加载。科学定义与单位仍由science拥有。

## 两族接口

| 族 | 文件/入口 | 消费者与清单 |
|---|---|---|
| backend | acsd_cpu_*，acsd_backend_get_api_v1 | backend_host/backend_loader.cpp与cpu_routing；backends.manifest.json |
| provider | acsd_cpuprov_*，acsd_provider_query_v1 | module_loader及provider查询；providers.manifest.json |

二者有不同ABI、能力位与消费者，不能混装或按相同名字互换。生产命令构建内adapter与独立DSO也分别核对。host通过kernel_id查询能力，不能把全进程切成一个含糊“AVX2模式”。

## 加载与执行

基础程序和握手门面按baseline编译，只有计算TU带高级ISA旗标。执行前检查实际CPU特征、OS寄存器保存状态、所需子集、合法库路径、文件哈希、ABI和自检；硬件支持不等于OS可安全执行，安全可执行也不等于当前机器最快。线程配额属于资源预算，和ISA能力分开。

跨外部边界使用版本化C ABI、POD/span、host回调及明确分配器。struct_size/abi_version不符拒绝，不猜布局；异常在内部转换，backend不私建长期池。kernel描述记录数学身份、算法/实现版本、dtype、别名、对齐、取消与并发。

## 选择、失败与证据

启动前可选变体不可用或无收益，记录原因并选合法baseline；计算中失败按阶段错误处理，不静默换后端继续混合结果。画像与机器/provider不符时失效并报告，后续真实消费者还要拒绝陈旧值。baseline自身失败不能继续伪成功。

每kernel比较独立科学参考及baseline，记录误差、负值/近零/极值和实际适用域，再比较吞吐。共享源只减少重复实现，不是独立oracle，也不证明FMA或编译器差异始终在固定容差内。安装声明、可加载、自检和科学执行分别报告；旧测试路径与历史读数在[来源档案](../../../../实验/documentation-refactor/docs/CPU_SOURCE.txt)。
