# AVX2/FMA provider

本页说明provider族AVX2实现，依据[后端架构](BACKEND.md)与[ISA隔离](ISA_VARIANTS.md)。实际位置为`lib/infrastructure/benchmark/cpu/avx2/`，查询入口是acsd_provider_query_v1，不是backend族的get_api。

## 注册与调用

当前注册calibration-pixel-transform与hips-bulk-transform两个kernel，与baseline用相同kernel_id/科学身份。其他kernel由host查表后选baseline，provider表外调用返回unsupported；provider自身不偷偷执行另一模型。

query握手核对ABI、必要host服务和能力，acsd_cpu_avx2_cap_gate使用capability层要求AVX/AVX2/FMA及OS状态。门面TU按baseline编译，计算TU avx2_kernels.cpp才带高级ISA旗标；旧“整provider TU带-mavx2”的叙述已过时。能力失败时返回不支持，尚不能据此保证加载前全平台任何路径绝无非法指令。

## 数值合同

calibration逐元素计算`(a-b-k c)d`；hips-bulk按定义的四邻点系数组合。FMA改变中间舍入，编译器、相消、非有限值与有效域需实测；没有跨线程归约不等于任意编译配置下结果逐位相同。

既有baseline比较量为`|a-b|/max(1,|b|)≤2e-4`，这是带绝对量级地板的混合尺度指标，不是所有非零b的纯相对误差。baseline与科学reference各在同阈值内，并不能推出variant与reference也必在同一个阈值内；疑点列[Q26](../../../../实验/documentation-refactor/docs/SCIENTIFIC_QUESTIONS.md)。本轮保留原比较参数，不推导新界或修改kernel。

## 内存、预算与验证

参数POD与baseline公开头复用，输入输出别名、容量、dtype、对齐与所有权按该接口核对。host授预算并控制在飞工作，串行回落必须保持数学定义。DSO句柄覆盖函数指针所有调用，再按协议shutdown/unload。

验证真实/模拟能力拒绝、ABI、表外kernel、门面无高级指令、计算面实际发射、非首元素失败、极值/相消、预算1/N和独立reference。老20%/28%等增益只对应原机器/批次，不当本轮性能结论；原文和测试意图在[来源档案](../../../../实验/documentation-refactor/docs/CPU_SOURCE.txt)。
