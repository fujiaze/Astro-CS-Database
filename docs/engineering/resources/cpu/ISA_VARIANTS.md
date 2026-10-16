# ISA 变体与逐内核选路

变体服务同一科学kernel，在受控域保持数值语义后按实测性能选择。逐kernel路由和两族入口见[CPU后端](BACKEND.md)，实际构建见[构建图](../../build/BUILD_GRAPH.md)。

## 两族与注册

backend族acsd_cpu_*使用acsd_backend_get_api_v1及backends.manifest.json；provider族acsd_cpuprov_*使用acsd_provider_query_v1及providers.manifest.json。各有ABI、能力位与消费者，构建可生成的DSO不等于当前发行推荐或运行实际选择。

目前AVX2 provider只登记calibration-pixel-transform和hips-bulk-transform；其他kernel按host选择合法baseline。AVX、AVX-512与整数/位扩展是否有收益取决于机器、编译器、数据与内核，不能从指令集包含关系推导吞吐严格支配。旧SHIP/NOT_SHIPPED及10%经验带是历史批次决策，不是普适性能定理。

## 编译隔离

两族均拆门面TU和计算TU：门面承载query/get_api、自检入口和表，按baseline编译；计算面通过唯一跨TU桥调用，局部设置ISA旗标。backend桥为acsd_variant_kernel_dispatch_v1，provider桥为acsd_cpuprov_kernel_range_v1。

根CMake给OBJECT计算目标配置GCC/Clang的-mavx2/-mfma或具体AVX-512子集，MSVC用对应/arch及浮点收缩策略。不能把旗标挂到整个主CLI、baseline或门面；开启LTO/链接优化后还需产物级检查，源码隔离不是最终指令隔离证明。

## 能力声明与产物核对

manifest记录实际文件hash、ABI、required_features、compiler、局部flags及kernel能力。编译许可面、实际发射面和运行required集合分别核对，包含隐式FMA等依赖。GCC/MSVC与两族掩码来自各自位表，不能复制旧928/992数字当通用安全判定。

反汇编检查计算面具有所声明档位的实际使用，query/get_api/能力门面能在baseline机器安全到达。动态导出白名单与Windows导出桥差异分别登记；多一个可解析符号和ABI语义变化不是同一问题。

## 测量与数值

固定输入、编译、预算、机器与统计协议比较，热身/冷读、重复中位数/散度及baseline取值策略如实保存。共享impl减少复制，不替代独立科学oracle。FMA/相消/尾部长度、别名和不同worker下的数值与身份都需验证，容差按[AVX2说明](AVX2_PROVIDER.md)和被估计量解释。

整数索引存在不代表BMI/POPCNT可加速，原12kernel的位操作扫描只支持其输入与实现集合。未来热点改变后重新测量，不能把“本次无收益”冻结成永久不提供能力。

## 原始证据与当前限制

旧文详细的ISA-001/002/003读数、R-60过程、目标迁移及cpuprov_avx512发射FMA却漏声明的记录完整保存于[来源档案](../../../../实验/documentation-refactor/docs/CPU_SOURCE.txt)。其中既有风险须按当前CMake、清单生成器与真实二进制重核，不能因重写正文就宣布已修复；本轮未编译、反汇编或运行kernel。
